/* $Id$ */

#include "samblah.h"

/*
 * Used by cmdls_list, primarily to easily sort files based on name and status
 * information.
 */
struct namest {
	char *name;             /* name of file/directory */
	struct stat st;         /* file information */
};


static void     cmdls_listdir(char *, int, int);
static int      isdir(const char *);
static int      listdir(char *, int *, char ***);
static int      statlist(char *, char **, int *, struct namest **, int *);
static int      filecmp(const void *, const void *);
static int      namecmp(const void *, const void *);
static void     printlist(int, struct namest *, int);


/* A ls(1)-like command.  lopt is for -l, ropt for -r. */
void
cmdls_list(int argc, char **argv, int lopt, int ropt)
{
	int namestc;
	struct namest *namestv;
	int filec;
	int i;

	if (argc == 1 && isdir(*argv)) {
		cmdls_listdir(*argv, lopt, ropt);
		return;
	}

	if (!statlist(NULL, argv, &namestc, &namestv, &filec))
		return;         /* statlist printed message on error */

	/* put files first, than print them */
	qsort(namestv, (size_t)namestc, sizeof (struct namest), filecmp);
	printlist(filec, namestv, lopt);

	/* list the directories */
	for (i = filec; !int_signal && i != namestc; ++i) {
		printf("%s%s:\n", (i == 0) ? "" : "\n", namestv[i].name);
		cmdls_listdir(namestv[i].name, lopt, ropt);
	}

	free(namestv);
}


/* Lists contents of directory dir.  lopt is for -l, ropt for -r. */
static void
cmdls_listdir(char *dir, int lopt, int ropt)
{
	int namec;
	char **namev;
	int namestc;
	struct namest *namestv;
	int filec;
	char buf[SMB_PATH_MAXLEN + 1];
	int bufleft;
	char *bufoff;
	char *rdir;
	int i;

	if ((bufleft = SMB_PATH_MAXLEN - (strlen(dir) + 1)) > 0) {
		strcpy(buf, dir);
		strcat(buf, "/");
		bufoff = buf + (SMB_PATH_MAXLEN - bufleft);
	} else {
		errno = ENAMETOOLONG;
		cmdwarn("in %s", dir);
		return;
	}

	/* read directory, namev is NULL on error or no entries */
	if (!listdir(dir, &namec, &namev))
		return;         /* listdir printed message on error */

	/*
	 * determine whether entries (entries namev in directory dir) are
	 * directories or files and how many of them are files
	 */
	if (!statlist(dir, namev, &namestc, &namestv, &filec)) {
		freelist(namec, namev);
		return;         /* statlist printed message on error */
	}

	/* sort all entries by name and print all of them */
	qsort(namestv, (size_t)namestc, sizeof (struct namest), namecmp);
	printlist(namestc, namestv, lopt);

	if (!ropt) {
		/* not listing recursively, nothing more to do */
		freelist(namec, namev);
		free(namestv);
		return;
	}

	/*
	 * list directories recursively, so sort namestv so that
	 * files are in front.  since we already determined how
	 * many files there were (namely filec), we can easily walk
	 * through the directories, listing each of them
	 */
	qsort(namestv, (size_t)namestc, sizeof (struct namest), filecmp);

	/* list the directories, we start where the directories begin */
	for (i = filec; !int_signal && i != namestc; ++i) {
		rdir = namestv[i].name;
		if (strlen(rdir) > bufleft) {
			errno = ENAMETOOLONG;
			cmdwarn("in %s", dir);
			continue;
		}

		strcpy(bufoff, rdir);
		rdir = buf;

		printf("\n%s:\n", rdir);
		cmdls_listdir(rdir, lopt, ropt);
	}

	freelist(namec, namev);
	free(namestv);
}


/* Returns whether path is a directory. */
static int
isdir(const char *path)
{
	struct stat st;

	if (int_signal)
		return 0;

	return smb_stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}


/*
 * Retrieves list of files in directory dir, entries will be put
 * in namev, number of entries in namec.  Returns whether the operation
 * was successful.  On error, namev will not contain allocated memory.
 */
static int
listdir(char *dir, int *namec, char ***namev)
{
	const struct smb_dirent *dent;
	char **newv;
	int dh = -1;

	*namec = 0;
	*namev = NULL;

	if (int_signal || (dh = smb_opendir(dir)) < 0)
		goto listdirerror;

	while (!int_signal && (dent = smb_readdir(dh)) != NULL) {
		if (streql(dent->name, ".") || streql(dent->name, ".."))
			continue;

		newv = realloc(*namev, sizeof (char *) * (*namec + 2));
		if (newv == NULL)
			goto listdirerror;
		*namev = newv;

		/* save copy of entry name */
		if (((*namev)[*namec] = strdup(dent->name)) == NULL)
			goto listdirerror;
		++*namec;
	}

	if (int_signal || (dh = smb_closedir(dh)) != 0)
		goto listdirerror;

	if (*namev != NULL)
		(*namev)[*namec] = NULL;

	return (*namec == 0) ? 0 : 1;

listdirerror:
	if (!int_signal) {
		cmdwarn("%s", dir);
		if (dh >= 0)
			(void)smb_closedir(dh);
	}

	while (*namec > 0)
		free((*namev)[--*namec]);
	free(*namev);

	*namev = NULL;
	*namec = 0;

	return 0;
}


/*
 * Calls smb_stat on the filenames in namev, prefixed with path
 * when it is not NULL.  Results are stored in argnamestc and argnamestv.
 * The number of files (as opposed to directories) in namev will be
 * stored in argfilec.  Returns whether the operation was successful.
 * Note that the pointers in namev will end up in argnamestv.  On
 * error, argnamestv will not contain allocated memory, but namev
 * will NOT be touched.
 */
static int
statlist(char *path, char **namev, int *argnamestc, struct namest **argnamestv,
    int *argfilec)
{
	struct stat st;
	struct namest *namestv = NULL;
	int namestc = 0;
	struct namest *newv;
	int filec = 0;
	char buf[SMB_PATH_MAXLEN + 1];
	int bufleft = 0;
	char *bufoff = NULL;

	if (path != NULL) {
		if ((bufleft = SMB_PATH_MAXLEN - (strlen(path) + 1)) > 0) {
			strcpy(buf, path);
			strcat(buf, "/");
			bufoff = buf + (SMB_PATH_MAXLEN - bufleft);
		} else {
			errno = ENAMETOOLONG;
			cmdwarn("in %s", path);
			return 0;
		}
	}

	while (*namev != NULL) {
		/* finish creating path to stat */
		if (path != NULL && (bufleft < strlen(*namev))) {
			errno = ENAMETOOLONG;
			cmdwarn("in %s", path);
			++namev;
			continue;
		}
		if (path != NULL)
			strcpy(bufoff, *namev);

		/* silently return when interrupted */
		if (int_signal) {
			free(namestv); namestv = NULL;
			
			*argnamestc = 0;
			*argnamestv = NULL;
			*argfilec = 0;

			return 0;
		}

		if (smb_stat((path == NULL) ? *namev : buf, &st) != 0) {
			cmdwarn("%s", *namev++);
			continue;
		}

		newv = realloc(namestv,
		    sizeof (struct namest) * (namestc + 2));
		if (newv == NULL) {
			cmdwarn("directory entries buffer");
			free(namestv); namestv = NULL;
			
			*argnamestc = 0;
			*argnamestv = NULL;
			*argfilec = 0;

			return 0;
		}
		namestv = newv;

		namestv[namestc].name = *namev++;
		memcpy(&namestv[namestc].st, &st, sizeof (struct stat));
		if (!S_ISDIR(namestv[namestc].st.st_mode))
			++filec;
		++namestc;
	}

	*argfilec = filec;
	*argnamestc = namestc;
	*argnamestv = namestv;
	return 1;
}


/*
 * Compare two `files' (struct namest), for qsort.  Files are
 * `smaller' than directories.  When both are files or both are
 * directories, strings are compared using strcmp.
 */
static int
filecmp(const void *arg1, const void *arg2)
{
	const struct namest *nst1 = (const struct namest *)arg1;
	const struct namest *nst2 = (const struct namest *)arg2;

	if (!S_ISDIR(nst1->st.st_mode) && S_ISDIR(nst2->st.st_mode))
		return -1;
	if (S_ISDIR(nst1->st.st_mode) && !S_ISDIR(nst2->st.st_mode))
		return 1;
	return strcmp(nst1->name, nst2->name);
}


static int
namecmp(const void *arg1, const void *arg2)
{
	const struct namest *nst1 = (const struct namest *)arg1;
	const struct namest *nst2 = (const struct namest *)arg2;

	return strcmp(nst1->name, nst2->name);
}


/*
 * Prints the nsc number of elements in nsv.  lopt is for -l for
 * whether or not to print extra information.
 */
static void
printlist(int nsc, struct namest *nsv, int lopt)
{
	struct collist col;

	if (lopt) {
		for (; nsc-- != 0; ++nsv)
			printf("%s %10lld %s%s\n",
			    makedatestr(nsv->st.st_mtime),
			    (long long)nsv->st.st_size, nsv->name,
			    (S_ISDIR(nsv->st.st_mode) ? "/" : ""));
		return;
	}

	col_initlist(&col);
	while (nsc--)
		if (!col_addlistelem(&col, (nsv++)->name)) {
			cmdwarn("adding entry to buffer");
			col_freelist(&col);
			return;
		}
	col_printlist(&col);
	col_freelist(&col);
}
