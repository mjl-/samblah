/* $Id$ */

#include "samblah.h"


/* For universal (local or remote) directory access. */
struct dir {
	union {
		int dh;
		DIR *dp;
	} handle;
	union {
		const struct smb_dirent *sdent;
		const struct dirent *dent;
	} dirent;
};


static int      tokenglob(int, int *, char ***, enum file_location);
static int      globbable(const char *);
static void     tokshift(char **, int);
static void     tokinsert(char **, char **, int);
static int      uni_opendir(const char *, struct dir *, enum file_location);
static int      uni_readdir(struct dir *, enum file_location);
static int      uni_closedir(struct dir *, enum file_location);
static int      uni_exists(const char *, enum file_location);
static int      uni_isdir(const char *, enum file_location);


/*
 * Replaces all tokens in tokenv except the first (which is the command) by
 * matches, the trailing NULL is kept.  tokenc is the number of tokens.
 * side indicates if the files are local or remote.  On success GLB_OK is
 * returned, on error GLB_DIRERR, GLB_NOMEM or GLB_INTR is returned and
 * errno set.  Memory is not freed on error, the caller must always free
 * the memory.
 */
int
smbglob(int *tokenc, char ***tokenv, int side)
{
	int i;
	int ret;
	enum file_location loc = side ? LOC_REMOTE : LOC_LOCAL;

	/*
	 * tokenglob returns by how many elements the token at index i was
	 * replaced.  the number of new elements is added to i so that we
	 * do not glob the newly inserted matches.
	 */
	for (i = 1; i < *tokenc; ++i)
		if ((ret = tokenglob(i, tokenc, tokenv, loc)) <= 0)
			return ret;
		else
			i += ret - 1;
	return GLB_OK;
}


/*
 * Performs globbing for one token which is at pos (index) in tokenv
 * which has tokenc elements.  loc denotes whether the file is local
 * or remote.  On success inserts elements in tokenv, reallocating
 * if necessary and returns number of elements inserted (1 with
 * no matches).  On error GLB_NOMEM, GLB_DIRERR or GLB_INTR is
 * returned and errno is set.
 */
static int
tokenglob(int pos, int *tokenc, char ***tokenv, enum file_location loc)
{
	int ret;
	char *tokencpy;
	char **ntokenv = NULL;
	int ntokenc = 0;
	char **newv;

	if (globbable((*tokenv)[pos])) {
		/* make copy of token so tokenmatch can write in it */
		tokencpy = strdup((*tokenv)[pos]);
		if (tokencpy == NULL)
			return GLB_NOMEM;

		ret = tokenmatch("", tokencpy, &ntokenc, &ntokenv, loc);
		free(tokencpy);

		if (ret != GLB_OK) {
			freelist(ntokenc, ntokenv);
			return ret;     /* errno unchanged since tokenmatch */
		}
	} else {
		ntokenc = 0;
	}

	if (ntokenc == 0) {
		/* no matches, insert the token literally */
		unescape((*tokenv)[pos]);
		return 1;
	}

	/* make room for the matches */
	newv = realloc(*tokenv, sizeof (char *) * (*tokenc + ntokenc));
	if (newv == NULL) {
		freelist(ntokenc, ntokenv);
		return GLB_NOMEM;       /* errno not changed by freelist */
	}
	*tokenv = newv;

	qsort(ntokenv, (size_t)ntokenc, sizeof ntokenv[0], qstrcmp);

	/* discard *tokenv + pos and place matches there instead */
	free((*tokenv)[pos]);
	tokshift(*tokenv + pos + 1, ntokenc - 1);
	tokinsert(*tokenv + pos, ntokenv, ntokenc);
	*tokenc += ntokenc - 1;

	free(ntokenv);

	return ntokenc;
}


/*
 * Generates matches for token which is in path.  Matches are placed
 * in ntokenvp matches, the number of matches in ntokencp.  loc denotes
 * if token is a local or a remote file.  When not called recursively
 * from within tokenmatch, path must be "" (path is only used for
 * recursion).  On success GLB_OK is returned, on error GLB_NOMEM, GLB_DIRERR
 * or GLB_INTR is returned and errno set.  Caller should free ntokenvp,
 * even on error.
 * Note that on success but without having generated matches, ntokenvp
 * need not be freed.
 */
int
tokenmatch(char *path, char *token, int *ntokencp, char ***ntokenvp,
    enum file_location loc)
{
	int ret;
	char *ntoken;
	char **newv;
	char *npath;
	struct dir d;
	const char *dname;
	int dnamelen;
	int pathlen;
	int save_errno;

	/* because of recursion, this is a good place to check for interrupt */
	if (int_signal) {
		errno = EINTR;
		return GLB_INTR;
	}

	/* token == NULL is the sign to stop the recursion and return */
	if (token == NULL) {
		newv = realloc(*ntokenvp, sizeof (char *) * (*ntokencp + 1));
		if (newv == NULL)
			return GLB_NOMEM;
		*ntokenvp = newv;

		/* + 2 because token could be a directory and get an extra / */
		(*ntokenvp)[*ntokencp] = malloc(strlen(path) + 2);
		if ((*ntokenvp)[*ntokencp] == NULL)
			return GLB_NOMEM;
		strcpy((*ntokenvp)[*ntokencp], path);

		if (uni_isdir(path, loc))
			strcat((*ntokenvp)[*ntokencp], "/");
		++*ntokencp;
		return GLB_OK;
	}

	/* for matching something in / */
	if (*token == '/')
		return tokenmatch("/", token + strspn(token, "/"),
		    ntokencp, ntokenvp, loc);

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
		npath = malloc(pathlen + strlen(token) + 1);
		if (npath == NULL)
			return GLB_NOMEM;
		*npath = '\0';
		if (*path != '\0') {
			strcat(npath, path);
			if (!streql(path, "/"))
				strcat(npath, "/");
		}
		strcat(npath, token);

		/* npath not existing is a dead end but not an error */
		if (!uni_exists(npath, loc))
			ret = GLB_OK;
		else
			ret = tokenmatch(npath, ntoken,
			    ntokencp, ntokenvp, loc);
		free(npath);
	} else {
		if (uni_opendir((*path == '\0') ? "." : path, &d, loc) < 0)
			return GLB_OK;

		while (!int_signal && uni_readdir(&d, loc) != -1) {
			dname = (loc == LOC_REMOTE)
			    ? d.dirent.sdent->name : d.dirent.dent->d_name;
			dnamelen = strlen(dname);

			if (fnmatch(token, dname, FNM_PERIOD) == FNM_NOMATCH)
				continue;

			npath = malloc((size_t)(pathlen + dnamelen + 1));
			if (npath == NULL) {
				save_errno = errno;
				(void)uni_closedir(&d, loc);
				errno = save_errno;
				return GLB_NOMEM;
			}
			*npath = '\0';
			if (*path != '\0') {
				strcat(npath, path);
				if (!streql(path, "/"))
					strcat(npath, "/");
			}
			strcat(npath, dname);

			ret = tokenmatch(npath, ntoken,
			    ntokencp, ntokenvp, loc);
			free(npath);
			if (ret != GLB_OK) {
				save_errno = errno;
				(void)uni_closedir(&d, loc);
				errno = save_errno;
				return ret;
			}
		}

		if (int_signal) {
			(void)uni_closedir(&d, loc);
			errno = EINTR;
			return GLB_INTR;
		}

		if (uni_closedir(&d, loc) != 0)
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


/*
 * Shifts elements in first argument by second argument places until
 * NULL is encountered.
 */
static void
tokshift(char **tokenv, int off)
{
	do
		tokenv[off] = *tokenv;
	while (*tokenv++ != NULL);
}


/*
 * Inserts ntokenc elements from ntokenv in tokenv.  tokenv is
 * assumed to be large enough.
 */
static void
tokinsert(char **tokenv, char **ntokenv, int ntokenc)
{
	while (ntokenc != 0)
		*tokenv++ = *ntokenv++, ntokenc--;
}


/* Set of functions for `unified' (local or remote) directory access. */

static int
uni_opendir(const char *path, struct dir *dp, enum file_location loc)
{
	if (loc == LOC_REMOTE)
		return ((dp->handle.dh = smb_opendir(path)) < 0) ? -1 : 1;
	return ((dp->handle.dp = opendir(path)) == NULL) ? -1 : 1;
}


static int
uni_readdir(struct dir *dp, enum file_location loc)
{
	if (loc == LOC_LOCAL)
		return ((dp->dirent.dent = readdir(dp->handle.dp)) == NULL)
		    ? -1 : 1;
	return ((dp->dirent.sdent = smb_readdir(dp->handle.dh)) == NULL)
	    ? -1 : 1;
}


static int
uni_closedir(struct dir *dp, enum file_location loc)
{
	if (loc == LOC_REMOTE)
		return (smb_closedir(dp->handle.dh) == 0) ? 0 : -1;
	return (closedir(dp->handle.dp) == 0) ? 0 : -1;
}


static int
uni_exists(const char *path, enum file_location loc)
{
	struct stat st;
	int (*statptr)(const char *, struct stat *);

	statptr = (loc == LOC_REMOTE) ? smb_stat : stat;
	return ((*statptr)(path, &st) == 0) ? 1 : 0;
}


static int
uni_isdir(const char *path, enum file_location loc)
{
	struct stat st;
	int (*statptr)(const char *, struct stat *);

	statptr = (loc == LOC_REMOTE) ? smb_stat : stat;
	return ((*statptr)(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}
