/* $Id$ */

#include "samblah.h"

typedef struct Dir Dir;

struct Dir {
	union {
		int dh;
		DIR *dp;
	} handle;
	union {
		const Smbdirent *sdent;
		const struct dirent *dent;
	} dirent;
};


static int      tokenglob(int, List *, int);
static int      globbable(const char *);
static int	tokenmatch2(char *, const char *, List *, int);
static int      uni_opendir(const char *, Dir *, int);
static int      uni_readdir(Dir *, int);
static int      uni_closedir(Dir *, int);
static int      uni_exists(const char *, int);
static int      uni_isdir(const char *, int);


/*
 * Replaces all elements in tokens except the first (which is the command) by
 * matches.  Remoteglobbing indicates if the files are local or remote.  On
 * success GLB_OK is returned, on error GLB_DIRERR or GLB_INTR is returned and
 * errno set.  Memory is not freed on error, the caller must always free the
 * memory.
 */
int
smbglob(List *tokens, int remoteglobbing)
{
	int	i;
	int	ret;

	/*
	 * tokenglob expands the element in tokens at position i.  On success
	 * it returns the index of the next element to be expanded (which may
	 * point to the terminating NULL element), on failure, it returns one
	 * of the GLB_* errors.
	 */
	i = 1;
	while (i < list_count(tokens)) {
		ret = tokenglob(i, tokens, remoteglobbing);
		if (ret <= 0)
			return ret;
		i = ret;
	}

	return GLB_OK;
}


/*
 * Performs globbing for one token which is at pos (index) in tokens.
 * remoteglobbing indicates whether the file is remote or local.  On success
 * inserts elements in tokens and returns the next position to be globbed.  On
 * error GLB_DIRERR or GLB_INTR is returned and errno is set.
 */
static int
tokenglob(int pos, List *tokens, int remoteglobbing)
{
	int	ret;
	List   *newtokens = NULL;
	char   *tokencpy;

	if (globbable((char *)list_elem(tokens, pos))) {
		/* make copy of token so tokenmatch can write in it */
		tokencpy = xstrdup((char *)list_elem(tokens, pos));

		newtokens = list_new();
		ret = tokenmatch(tokencpy, newtokens, remoteglobbing);
		free(tokencpy);

		if (ret != GLB_OK) {
			list_free(newtokens); newtokens = NULL;
			return ret;     /* errno unchanged since tokenmatch */
		}

		/*
		 * now newtokens is non-NULL which means its list_elem(tokens, pos)
		 * should be replaced by the elements in newtokens
		 */
	}

	if (newtokens == NULL || list_count(newtokens) == 0) {
		/* no matches, insert the token literally */
		unescape((char *)list_elem(tokens, pos));
		if (newtokens != NULL)
			list_free(newtokens);
		return pos + 1;
	}

	list_sort(newtokens, qstrcmp);

	/*
	 * replace the one token with the matches, makes copies of arguments so
	 * list_free() below is correct
	 */
	list_replace(tokens, pos, newtokens);
	ret = pos + list_count(newtokens) - 1;
	list_free(newtokens);

	return ret;
}


/*
 * Generates matches for token which is in path.  Matches are placed in tokens.
 * remoteglobbing denotes if token is remote or local.  When not called
 * recursively from within tokenmatch, path must be "" (path is only used for
 * recursion).  On success GLB_OK is returned, on error GLB_DIRERR or GLB_INTR
 * is returned and errno set.  Caller should free tokens, even on error.
 *
 * Note that on success but without having generated matches, tokens need not
 * be freed.
 */
int
tokenmatch(const char *token, List *tokens, int remoteglobbing)
{
	return tokenmatch2("", token, tokens, remoteglobbing);
}

static int
tokenmatch2(char *path, const char *token, List *tokens, int remoteglobbing)
{
	int ret;
	char *ntoken;
	char *npath;
	Dir d;
	const char *dname;
	int dnamelen;
	int pathlen;
	int save_errno;
	char   *newtoken;

	/* because of recursion, this is a good place to check for interrupt */
	if (int_signal) {
		errno = EINTR;
		return GLB_INTR;
	}

	/* token == NULL is the sign to stop the recursion and return */
	if (token == NULL) {
		/* + 2 because token could be a directory and get an extra / */
		newtoken = xmalloc(strlen(path) + 2);
		strcpy(newtoken, path);

		if (uni_isdir(path, remoteglobbing))
			strcat(newtoken, "/");
		
		list_add(tokens, newtoken);
		return GLB_OK;
	}

	/* for matching something in / */
	if (*token == '/')
		return tokenmatch2("/", token + strspn(token, "/"), tokens, remoteglobbing);

	/* component to match is token, token for next call is ntoken */
	ntoken = strchr(token, '/');
	if (ntoken != NULL) {
		*ntoken = '\0';
		if (*++ntoken == '/')
			ntoken += strspn(ntoken, "/");
	}

	/*
	 * Note: ntoken could be NULL which means the next recursive call will
	 * definitely add the token and return
	 */

	/* path + slash when path is non-empty */
	pathlen = (*path == '\0') ? 0 : strlen(path) + 1;

	/* when non-globbable, check if file exists, otherwise read path */
	if (!globbable(token)) {
		/* create new path */
		npath = xmalloc(pathlen + strlen(token) + 1);
		*npath = '\0';
		if (*path != '\0') {
			strcat(npath, path);
			if (!streql(path, "/"))
				strcat(npath, "/");
		}
		strcat(npath, token);

		/* npath not existing is a dead end but not an error */
		if (!uni_exists(npath, remoteglobbing))
			ret = GLB_OK;
		else
			ret = tokenmatch2(npath, ntoken, tokens, remoteglobbing);
		free(npath);
	} else {
		if (uni_opendir((*path == '\0') ? "." : path, &d, remoteglobbing) < 0)
			return GLB_OK;

		while (!int_signal && uni_readdir(&d, remoteglobbing) != -1) {
			dname = remoteglobbing ? d.dirent.sdent->name : d.dirent.dent->d_name;
			dnamelen = strlen(dname);

			if (fnmatch(token, dname, FNM_PERIOD) == FNM_NOMATCH)
				continue;

			npath = xmalloc((size_t)(pathlen + dnamelen + 1));
			*npath = '\0';
			if (*path != '\0') {
				strcat(npath, path);
				if (!streql(path, "/"))
					strcat(npath, "/");
			}
			strcat(npath, dname);

			ret = tokenmatch2(npath, ntoken, tokens, remoteglobbing);
			free(npath);
			if (ret != GLB_OK) {
				save_errno = errno;
				(void)uni_closedir(&d, remoteglobbing);
				errno = save_errno;
				return ret;
			}
		}

		if (int_signal) {
			(void)uni_closedir(&d, remoteglobbing);
			errno = EINTR;
			return GLB_INTR;
		}

		if (uni_closedir(&d, remoteglobbing) != 0)
			ret = GLB_DIRERR;
		ret = GLB_OK;
	}

	return ret;
}


/* Returns whether argument contains globbable characters */
static int
globbable(const char *pattern)
{
	const char *cp = NULL;

	while ((cp = strpbrk((cp == NULL) ? pattern : cp + 1, "*?[]")) != NULL)
		if (cp == pattern || *(cp - 1) != '\\')
			return 1;
	return 0;
}


/* Set of functions for `unified' (local or remote) directory access. */

static int
uni_opendir(const char *path, Dir *dp, int remoteglobbing)
{
	if (remoteglobbing)
		return ((dp->handle.dh = smb_opendir(path)) < 0) ? -1 : 1;
	return ((dp->handle.dp = opendir(path)) == NULL) ? -1 : 1;
}


static int
uni_readdir(Dir *dp, int remoteglobbing)
{
	if (remoteglobbing)
		return ((dp->dirent.sdent = smb_readdir(dp->handle.dh)) == NULL) ? -1 : 1;
	return ((dp->dirent.dent = readdir(dp->handle.dp)) == NULL) ? -1 : 1;
}


static int
uni_closedir(Dir *dp, int remoteglobbing)
{
	if (remoteglobbing)
		return (smb_closedir(dp->handle.dh) == 0) ? 0 : -1;
	return (closedir(dp->handle.dp) == 0) ? 0 : -1;
}


static int
uni_exists(const char *path, int remoteglobbing)
{
	struct stat st;
	int (*statptr)(const char *, struct stat *);

	statptr = remoteglobbing ? smb_stat : stat;
	return ((*statptr)(path, &st) == 0) ? 1 : 0;
}


static int
uni_isdir(const char *path, int remoteglobbing)
{
	struct stat st;
	int (*statptr)(const char *, struct stat *);

	statptr = remoteglobbing ? smb_stat : stat;
	return ((*statptr)(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}
