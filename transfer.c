/* $Id$ */

#include "samblah.h"


#define TRANSFER_BUFSIZE       32768    /* size of buffer for `get' */
#define PROGRESSLINE_MAXLEN     1024    /* length of line, used for buffer */


/* For specifying if a file (descriptor) is remote or local. */
enum side       { SIDE_REMOTE, SIDE_LOCAL };

/* Used by copybyfd to print progress. */
static const char      *file;
static off_t    start_offset;
static off_t    end_offset;
static size_t   transferred;
static size_t   previoustransferred;
static int      average;
static struct timeval   previoustime;
static char     progress[PROGRESSLINE_MAXLEN + 1];
static int      progresslen;


static void     alarm_handler(int);
static int      mkpath(const char *, mode_t, int (*)(const char *, mode_t));
static void     transfer(enum side, const char *, const char *, int *, int);
static void     transferfile(enum side, const char *, const char *, int *);
static int      copybyfd(int, int, enum side, off_t, off_t, const char *);
static int      askonexist(const char *, struct stat, struct stat, int *, int *);
static int      open_wrap(const char *, int, mode_t);
static void     makeprogress(const char *, double, off_t);
static void     makesize(char [10], off_t);
static void     printprogress(void);
static void     printcompleted(double);
static unsigned long    timediff(struct timeval, struct timeval);


/*
 * Retrieves rpath and writes it to lpath, recursive when ropt is
 * true.  lexist tells what to do when the local path already exists.
 * Note that the user will be prompted for input when lexist is `ask'.
 */
void
transfer_get(const char *rpath, const char *lpath, int *exist, int ropt)
{
	/* use the generic transfer for retrieving */
	transfer(SIDE_REMOTE, rpath, lpath, exist, ropt);
}


/*
 * Uploads lpath to the remote rpath, possibly recursive.  rexists
 * tells what to do when the local path already exists.  Note that
 * the user will be prompted for input when rexist is `ask'.
 */
void
transfer_put(const char *lpath, const char *rpath, int *exist, int ropt)
{
	/* use the generic transfer for uploading */
	transfer(SIDE_LOCAL, lpath, rpath, exist, ropt);
}


/*
 * Like transfer_get, but retrieve rpath and write it to fd.  rpath
 * cannot be transferred recursively.  The user will not be prompted.
 * On success non-zero is returned, otherwise zero is returned and
 * errno set.
 */
int
transfer_get_fd(const char *rpath, int fd)
{
	int sfd;                /* remote file descriptor */
	struct stat st;         /* remote file information */
	int save_errno;		

	/* open remote file */
	sfd = smb_open(rpath, O_RDONLY, (mode_t)0);
	if (sfd < 0) {
		cmdwarn("opening %s", rpath);
		return 0;
	}

	/* get size of remote file */
	if (smb_stat(rpath, &st) != 0) {
		save_errno = errno;

		cmdwarn("%s", rpath);
		(void)smb_close(sfd);

		errno = save_errno;
		return 0;
	}

	/* do the copying, copybyfd closes the file handles */
	if (!copybyfd(sfd, fd, SIDE_REMOTE, (off_t)0, st.st_size, rpath)) {
		/* on SIGINT, do not say anything, just stop */
		if (!int_signal)
			cmdwarn("retrieving %s", rpath);
		return 0;
	}

	return 1;
}


/*
 * Transfers spath (source path) which resides as sside (remote or
 * local) to dpath (destination path) which resides at the opposite
 * side of dside, when ropt is true spath is retrieved recursively.
 * dexist what to do when dpath exists, note that this must be a
 * pointer so the value can be save when the user selects `overwrite
 * all', `resume all' or `skip all'.
 */
static void
transfer(enum side sside, const char *spath, const char *dpath,
    int *dexist, int ropt)
{
	int len;                /* for length of spath */
	int dh = -1;            /* for remote directory handle */
	DIR *dp = NULL;         /* for local directory stream */

	struct stat st;                         /* for information of spath */
	const struct smb_dirent *rdent = NULL;  /* for recursion on remote */
	const struct dirent *ldent = NULL;      /* for recursion on local */

	int (*sstat)(const char *, struct stat *);      /* for source-stat */
	int (*dmkdir)(const char *, mode_t);    /* for mkdir of destination */

	sstat = (sside == SIDE_REMOTE) ? smb_stat : stat;
	dmkdir = (sside == SIDE_REMOTE) ? mkdir : smb_mkdir;

	/* have to find out if argument is file or directory */
	if (sstat(spath, &st) != 0) {
		cmdwarn("%s", spath);
		return;
	}

	/* if file, transfer immediately */
	if (!S_ISDIR(st.st_mode)) {
		transferfile(sside, spath, dpath, dexist);
		return;
	}

	/* transfer files in spath to dpath */

	/* make entire path, not an error if it already exists */
	if (mkpath(dpath, (mode_t)(S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH),
	    dmkdir) != 0 && errno != EEXIST) {
		/* when interrupted, stop silently */
		if (errno != EINTR)
			cmdwarn("creating %s", dpath);
		return;
	}

	/* retrieve contents of directory */
	if ((sside == SIDE_REMOTE && (dh = smb_opendir(spath)) < 0) ||
	    (sside == SIDE_LOCAL && ((dp = opendir(spath)) == NULL))) {
		cmdwarn("opening %s", spath);
		return;
	}

	len = strlen(spath);

	/* walk through contents of directory */
	while (!int_signal &&
	    ((sside == SIDE_REMOTE && (rdent = smb_readdir(dh)) != NULL) ||
	    (sside == SIDE_LOCAL && ((ldent = readdir(dp)) != NULL)))) {
		char *nspath;                   /* for new source path */
		char *ndpath;                   /* for new destination path */
		const char *sdent_name;         /* for directory entry */
		long spathmax, dpathmax;        /* for max lengths of paths */
		int ndpathlen, nspathlen;       /* for lengths of new paths */

		sdent_name = (sside == SIDE_REMOTE)
		    ? rdent->name : ldent->d_name;
		spathmax = (sside == SIDE_REMOTE)
		    ? SMB_PATH_MAXLEN : pathconf(spath, _PC_PATH_MAX);
		dpathmax = (sside == SIDE_LOCAL)
		    ? SMB_PATH_MAXLEN : pathconf(dpath, _PC_PATH_MAX);

		/* use default PATH_MAXLEN when pathconf returns infinite */
		if (sside == SIDE_LOCAL && spathmax == -1)
			spathmax = FALLBACK_PATH_MAXLEN;
		if (sside == SIDE_REMOTE && dpathmax == -1)
			dpathmax = FALLBACK_PATH_MAXLEN;

		/* make sure we do not transfer dot or dot-dot */
		if (streql(sdent_name, ".") || streql(sdent_name, ".."))
			continue;

		/* skip file if it is too long */
		nspathlen = len + 1 + strlen(sdent_name);
		if (nspathlen > spathmax) {
			errno = ENAMETOOLONG;
			cmdwarn("in %s", spath);
			continue;
		}

		/* skip file if it is too long */
		ndpathlen = strlen(dpath) + 1 + strlen(sdent_name);
		if (ndpathlen > dpathmax) {
			errno = ENAMETOOLONG;
			cmdwarn("in %s", dpath);
			continue;
		}

		nspath = malloc((size_t)nspathlen + 1);
		if (nspath == NULL) {
			cmdwarn("source path buffer");
			continue;
		}

		ndpath = malloc((size_t)ndpathlen + 1);
		if (ndpath == NULL) {
			cmdwarn("destination path buffer");
			free(nspath);
			continue;
		}

		/* construct new paths */
		strcpy(nspath, spath);
		if (spath[strlen(spath) - 1] != '/')
			strcat(nspath, "/");
		strcat(nspath, sdent_name);

		strcpy(ndpath, dpath);
		if (dpath[strlen(dpath) - 1] != '/')
			strcat(ndpath, "/");
		strcat(ndpath, sdent_name);

		/* transfer the new file/directory recursively */
		transfer(sside, nspath, ndpath, dexist, ropt);

		free(ndpath);
		free(nspath);
	}

	if (int_signal) {
		if (sside == SIDE_REMOTE)
			(void)smb_closedir(dh);
		else
			(void)close(dh);
		return;
	}

	/* cleanup */
	if ((sside == SIDE_REMOTE && smb_closedir(dh) != 0) ||
	    (sside == SIDE_LOCAL && closedir(dp) != 0))
		cmdwarn("closing %s", spath);
}


/*
 * Transfers spath which resides at sside to dpath (which resides
 * at the opposite side of sside.  dexist tells what to do when dpath
 * exits.
 */
static void
transferfile(enum side sside, const char *spath, const char *dpath,
    int *dexist)
{
	/* for calling tranferfile after having asked on destination exist */
	int tmpexist;
	int sfd, dfd;   /* source and destination file handle */
	int flags;      /* flags for opening of file */
	off_t offset;   /* where to start in file when resuming */
	struct stat dst, sst;

	int (*dopen)(const char *, int, mode_t);
	int (*sopen)(const char *, int, mode_t);
	int (*dclose)(int);
	int (*sclose)(int);
	int (*dstat)(const char *, struct stat *);
	int (*sstat)(const char *, struct stat *);
	off_t (*dlseek)(int, off_t, int);
	off_t (*slseek)(int, off_t, int);
	int (*sfstat)(int, struct stat *);

	dopen = (sside == SIDE_REMOTE) ? open_wrap : smb_open;
	sopen = (sside == SIDE_REMOTE) ? smb_open : open_wrap;
	dclose = (sside == SIDE_REMOTE) ? close : smb_close;
	sclose = (sside == SIDE_REMOTE) ? smb_close : close;
	dstat = (sside == SIDE_REMOTE) ? stat : smb_stat;
	sstat = (sside == SIDE_REMOTE) ? smb_stat : stat;
	dlseek = (sside == SIDE_REMOTE) ? lseek : smb_lseek;
	slseek = (sside == SIDE_REMOTE) ? smb_lseek : lseek;
	sfstat = (sside == SIDE_REMOTE) ? smb_fstat : fstat;

	offset = 0;             /* start at begin by default */
	dst.st_size = 0;        /* `current position' is begin by default */

	/* default is create file and write only */
	flags = O_WRONLY | O_CREAT;

	if (*dexist == VAR_OVERWRITE)
		flags |= O_TRUNC;       /* just overwrite */
	else
		/* for checking if file exists (ask or resume) */
		flags |= O_EXCL;

	/* XXX this goes too deep and gets too ugly */

	/* open/create destination file */
	if ((dfd = dopen(dpath, flags,
	    (mode_t)(S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))) < 0) {

		/* file exists, check what to do */
		if (errno == EEXIST) {

			if (*dexist == VAR_SKIP)
				return;

			/* get attributes of destination and source */
			if (dstat(dpath, &dst) != 0) {
				cmdwarn("%s", dpath);
				return;
			}

			if (sstat(spath, &sst) != 0) {
				cmdwarn("%s", spath);
				return;
			}

			/* if we should be resuming, try to open destination */
			if (*dexist == VAR_RESUME) {
				/* cannot resume beyond file */
				if (sst.st_size <= dst.st_size) {
					cmdwarnx("resuming %s: already as "
					    "large as or larger than source",
					    spath);
					return;
				}

				dfd = dopen(dpath, O_WRONLY, (mode_t)0);
				if (dfd < 0) {
					cmdwarn("opening %s", dpath);
					return;
				}

				/* determine offset from start */
				if (dst.st_size > RESUME_ROLLBACK)
					offset = dst.st_size - RESUME_ROLLBACK;
				else
					offset = 0;

				/* seek to right location */
				if (offset > 0) {
					if (dlseek(dfd, offset, SEEK_SET) ==
					    -1) {
						cmdwarn("seeking %s", dpath);
						(void)close(dfd);
						return;
					}
				}

				/* variable dfd now is valid `destination' */
			} else {
				/* ask what to do */
				if (!askonexist(dpath, sst, dst,
				    &tmpexist,dexist)) {
					if (!int_signal)
						cmdwarnx("could not read "
						    "answer");
					return;
				}

				if (tmpexist == VAR_SKIP)
					return;

				if (tmpexist == VAR_RESUME &&
				    sst.st_size <= dst.st_size) {
					cmdwarnx("resuming %s: already as "
					    "large as or larger than source",
					    spath);
					return;
				}

				/* tmpexist is VAR_RESUME or VAR_OVERWRITE */
				transferfile(sside, spath, dpath, &tmpexist);
				return;
			}
		}
		/* other error, clean up */
		else {
			cmdwarn("opening %s", dpath);
			return;
		}
	}

	/* open source file */
	sfd = sopen(spath, O_RDONLY, (mode_t)0);
	if (sfd < 0) {
		cmdwarn("opening %s", spath);
		(void)dclose(dfd);
		return;
	}

	/* seek if necessary */
	if (offset > 0) {
		if (slseek(sfd, offset, SEEK_SET) == (off_t)-1) {
			cmdwarn("seeking %s", spath);
			(void)dclose(dfd);
			(void)sclose(sfd);
			return;
		}
	}

	/* retrieve info about file */
	if (sfstat(sfd, &sst) != 0) {
		cmdwarn("%s", spath);
		(void)dclose(dfd);
		(void)sclose(sfd);
		return;
	}

	/* copy the fd's, copybyfd closes file handles */
	if (!copybyfd(sfd, dfd, sside, offset, sst.st_size, spath))
		/* on SIGINT, do not say anything, just stop */
		if (!int_signal)
			cmdwarn("transferring %s", spath);

	return;
}


/*
 * Copies from from to to.  fromside is the side of the source.
 * cur is the current offset in from at which the copying starts.
 * size is the total size of the file to be copied.  frompath is the
 * name that goes with from (the first argument).  On failure 0 is
 * returned and errno is set, otherwise anything but 0 may be returned.
 */
static int
copybyfd(int from, int to, enum side fromside, off_t cur, off_t size,
    const char *frompath)
{
	char buf[TRANSFER_BUFSIZE];     /* transfer buffer */
	ssize_t count;                  /* number of bytes read */
	ssize_t written;                /* number of bytes written */
	size_t countleft;               /* number of read bytes to write */
	struct timeval begintime, endtime;
	int showprogress;
	int save_errno;
	double completedaverage;
	ssize_t (*readfrom)(int, void *, size_t);
	ssize_t (*writeto)(int, const void *, size_t);
	int (*closefrom)(int);
	int (*closeto)(int);
	struct sigaction alarmact;

	/* safe default values */
	count = -1;
	written = -1;

	/* determine which functions to use */
	if (fromside == SIDE_REMOTE) {
		readfrom = smb_read;
		writeto = write;
		closefrom = smb_close;
		closeto = close;
	} else {
		readfrom = read;
		writeto = smb_write;
		closefrom = close;
		closeto = smb_close;
	}

	/* determine if we should print progress */
	showprogress = getvariable_bool("showprogress");

	start_offset = cur;
	end_offset = size;
	file = frompath;
	previoustransferred  = transferred = 0;
	previoustime.tv_sec = 0;
	previoustime.tv_usec = 0;
	average = 0;

	alarmact.sa_handler = alarm_handler;
	alarmact.sa_flags = SA_RESTART;
	sigemptyset(&alarmact.sa_mask);

	/* set the time at which the transfer started */
	(void)gettimeofday(&begintime, NULL);

	/* print initial line */
	if (showprogress) {
		if (sigaction(SIGALRM, &alarmact, NULL) != 0) {
			/* could set signal, simply do not print progress */
			showprogress = 0;
		} else {
			printprogress();
			(void)alarm(1);
		}
	}

	/* keep reading and writing till finished or error */
	while (!int_signal) {
		count = (*readfrom)(from, buf, sizeof buf);
		if (count == -1) {
			save_errno = errno;

			(void)(*closefrom)(from);
			(void)(*closeto)(to);

			(void)alarm(0);
			alarmact.sa_handler = SIG_DFL;
			(void)sigaction(SIGALRM, &alarmact, NULL);

			errno = save_errno;
			return 0;
		}
		if (count == 0)
			break;

		countleft = (size_t)count;
		while (!int_signal && countleft != 0) {
			written = (*writeto)(to,
			    buf + ((size_t)count - countleft), countleft);
			if (written == -1) {
				save_errno = errno;

				(void)(*closefrom)(from);
				(void)(*closeto)(to);

				(void)alarm(0);
				alarmact.sa_handler = SIG_DFL;
				(void)sigaction(SIGALRM, &alarmact, NULL);

				errno = save_errno;
				return 0;
			}
			if (written == 0)
				break;

			countleft -= written;
		}

		transferred += count - countleft;

		if (written == 0)
			break;
	}

	(void)alarm(0);
	alarmact.sa_handler = SIG_DFL;
	(void)sigaction(SIGALRM, &alarmact, NULL);

	/* when interrupted, cleanup and set errno */
	if (int_signal) {
		(void)(*closefrom)(from);
		(void)(*closeto)(to);

		errno = EINTR;
		return 0;
	}

	/* binary OR since from and to must always be closed */
	if ((count == -1) | ((*closefrom)(from) != 0) | ((*closeto)(to) != 0))
		return 0;       /* error while reading or closing */

	(void)gettimeofday(&endtime, NULL);

	completedaverage = 0.0;
	if (timediff(endtime, begintime) != 0)
		completedaverage = (double)1e6 * (double)transferred /
		    (double)timediff(endtime, begintime);
	printcompleted(completedaverage);

	return 1;
}


/*
 * This function will be called when the destination file of a
 * transfer already exists.  The user will be asked what to do:
 * overwrite, resume, overwrite all, resume all, skip or skip all.
 * dpath/dst is the destination path/info.  localexist tells what to do
 * when dpath exists, variable exist is used to save the answer from the
 * user in case it was either `overwrite all', `resume all' or `skip all'.
 * On error zero is returned, otherwise non-zero is returned.
 */
static int
askonexist(const char *dpath, struct stat sst, struct stat dst,
    int *localexist, int *exist)
{
	int c;
	char buf[4];            /* for "xx\n\0" */
	int ready = 0;

	printf("\n");
	printf("File exists: %s\n", dpath);
	printf("source: %20lld  %s\n", (long long)sst.st_size,
	    makedatestr(sst.st_mtime));
	printf("target: %20lld  %s\n", (long long)dst.st_size,
	    makedatestr(dst.st_mtime));

	while (!int_signal && !ready) {
		/* print the options */
		printf("[O]verwrite [O!] all  [R]esume [R!] all  "
		    "[S]kip [S!] all  [C]ancel > ");
		fflush(stdout);

		/* read answer, remove line on error */
		if (fgets(buf, sizeof buf, stdin) == NULL) {
			if (feof(stdin))
				clearerr(stdin);
			return 0;
		}

		/* no newline is an error (more than two chars), read line */
		if (strchr(buf, '\n') == NULL) {
			while ((c = getchar()) != EOF && c != '\n')
				;       /* nothing */

			if (feof(stdin))
				clearerr(stdin);

			return 0;
		} else {
			*(strchr(buf, '\n')) = '\0';
		}

		ready = 1;

		if (strcasecmp(buf, "O") == 0)
			*localexist = VAR_OVERWRITE;
		else if (strcasecmp(buf, "O!") == 0)
			*localexist = *exist = VAR_OVERWRITE;
		else if (strcasecmp(buf, "R") == 0)
			*localexist = VAR_RESUME;
		else if (strcasecmp(buf, "R!") == 0)
			*localexist = *exist = VAR_RESUME;
		else if (strcasecmp(buf, "S") == 0)
			*localexist = VAR_SKIP;
		else if (strcasecmp(buf, "S!") == 0)
			*localexist = *exist = VAR_SKIP;
		else if (strcasecmp(buf, "C") == 0)
			/*
			 * this is evil but cancel does have the same semantics
			 * as an interrupt, and without using int_signal, much
			 * more code needs to change
			 */
			int_signal = 1;
		else
			ready = 0;      /* invalid answer, try again */
	}

	/* give error when interrupted */
	if (int_signal)
		return 0;

	return 1;
}


/*
 * open_wrap is the open(2) call with three arguments (third argument
 * of open(2) is ..., since we want te fit open(2) and smb_open in
 * one pointer, we need this wrapper to let the compiler (and ourselves)
 * know it is ok.  Same behaviour of course as the three argument
 * version of open(2).
 */
static int
open_wrap(const char *path, int flags, mode_t mode)
{
	return open(path, flags, mode);
}


/*
 * Creates a path like `mkdir -p', that is, creating every part of the path
 * if it does not exist.  On success 0 is returned, -1 indicates an error with
 * errno set.  Besides the errno's of mkdir(2) it can set errno to ENOMEM or
 * EINTR.
 */
static int
mkpath(const char *path, mode_t mode, int (*lmkdir)(const char *, mode_t))
{
	char *buf;
	char *cp;
	int ret;

	buf = malloc(strlen(path) + 1);
	if (buf == NULL)
		return -1;      /* errno set by malloc */

	strcpy(buf, path);

	/* create path except last directory */
	for (cp = strchr(buf, '/'); !int_signal && cp != NULL;
		cp = strchr(cp + 1, '/')) {

		*cp = '\0';
		if ((ret = lmkdir(buf, mode)) != 0 && errno != EEXIST) {
			free(buf);
			return ret;
		}
		*cp = '/';
	}

	free(buf);

	if (int_signal) {
		errno = EINTR;
		return -1;
	}

	return lmkdir(path, mode);
}


/* ARGSUSED */
static void
alarm_handler(int sig)
{
	int save_errno;

	save_errno = errno;
	printprogress();
	(void)alarm(1);
	errno = save_errno;
}


/* Determines the difference in microseconds between newest and oldest. */
static unsigned long
timediff(struct timeval newest, struct timeval oldest)
{
	return (unsigned long)((newest.tv_sec - oldest.tv_sec) * 1e6 +
	    newest.tv_usec - oldest.tv_usec);
}


/*
 * Writes the file and a progress bar to variable `progress', the
 * length of the line is written to progresslen.  The last 13
 * bytes are kept empty, to be filled with an ETA or an average speed.
 * Example lines:
 * file:                      |***************           | 9352 KB  122.2 KB/s
 * file:                      |*******                   |   71 KB   01:49 ETA
 */
static void
makeprogress(const char *file, double ratio, off_t size)
{
	int totallen, filelen, barlen, sizelen;
	int printdots;
	int space;
	int starlen;
	char *barp;
	char barbuf[51];
	char sizebuf[10];

	/*
	 * totallen  - length of entire line except for the last 13 bytes
	 * filelen   - length of file name, colon, and two spacesr,
	 *             we want it at least 15
	 * barlen    - length of the entire bar, we want it to between 25 and 50
	 * sizelen   - length of current size, must be either 0 or 9
	 *
	 * when these variables are calculated, label `makestring:' will
	 * create the string.
	 *
	 * a description about magic occurrences of constants:
	 * 13  - space to be reserved at end of line for eta/average
	 * 3 + strlen(file)  - for the ":  " after the file
	 * 15  - desired length for file field
	 * 9   - " xxxxx ?B"
	 * 25  - minimum size of barlen if it is to be printed
	 * 50  - maximum size of barlen
	 */

	/* get current width of terminal and make sure it fits in our buffer */
	progresslen = term_width();
	if (progresslen > sizeof progress - 1)
		progresslen =  sizeof progress - 1;

	/* when even the eta/average does not fit, stop */
	if (progresslen < 13) {
		*progress = '\0';
		return;
	}
	totallen = progresslen - 13;

	barlen = 0;
	sizelen = 9;

	filelen = 15;
	if (totallen < filelen) {
		/* we cannot even print the file, stop */
		*progress = '\0';
		return;
	}

	if (totallen - filelen < sizelen) {
		/* not enough space for minimum file length or size */
		filelen  = (totallen < strlen(file) + 3)
		    ? totallen : strlen(file) + 3;
		sizelen = 0;
		goto makestring;
	}

	if (totallen - filelen - sizelen < 25) {
		/* not enough space for bar */
		filelen = totallen - sizelen;
		if (filelen > strlen(file) + 3)
			filelen = strlen(file) + 3;
		goto makestring;
	}

	/*
	 * we have both a file name and a bar, first try to make the
	 * file as long as possible, then try to maximize the bar
	 * length, then calculate the white space between them
	 */

	barlen = 25;
	filelen = totallen - sizelen - barlen;
	if (filelen >= strlen(file) + 3)
		filelen = strlen(file) + 3;
	barlen = totallen - filelen - sizelen;
	if (barlen > 50)
		barlen = 50;
	goto makestring;

makestring:
	printdots = strlen(file) > filelen - 3;
	if (printdots)
		file += (strlen("...") + strlen(file) + 3) - filelen;
	space = totallen - sizelen - filelen + 2 - barlen;

	assert(sizelen == 0 || sizelen == 9);
	if (sizelen)
		makesize(sizebuf, size);
	else
		*sizebuf = '\0';

	if (barlen >= 25) {
		barlen -= 2;
		starlen = (int)(0.5 + (double)barlen * ratio);

		barp = barbuf;
		*barp++ = '|';
		while (starlen) {
			*barp++ = '*';
			--starlen;
			--barlen;
		}
		while (barlen) {
			*barp++ = ' ';
			--barlen;
		}
		*barp++ = '|';
		*barp = '\0';
	} else {
		*barbuf = '\0';
	}

	(void)xsnprintf(progress, sizeof progress, "%s%s:%*s%s%*s",
	    printdots ? "..." : "", file, space, "", barbuf,
	    sizelen, sizebuf);
}


static void
makesize(char buf[10], off_t size)
{
	char *suffix = " KMGTP";

	while (size >= 99999) {
		++suffix;
		size /= 1024;
	}
	(void)xsnprintf(buf, 10, "%lld %cB", (long long)size, *suffix);
}


/*
 * Prints a line showing progress of the current transfer.  makeprogress
 * is used to create a line containing the file name, progressbar and
 * current file size.  It then adds an ETA and removes the current
 * line and prints the new progress.
 */
static void
printprogress(void)
{
	struct timeval now;
	char etabuf[14];
	int seconds;
	int large, small;
	double ratio;
	char sizebuf[10];
	size_t filelen;
	int filefits;

	/* handle transferring from devices and such, do not print bars */
	if (start_offset + transferred > end_offset) {
		/* print only filename and current offset */
		makesize(sizebuf, start_offset + transferred);
		filelen = term_width() - strlen(sizebuf) - 3;
		filefits = strlen(file) <= filelen;
		
		(void)xsnprintf(progress, sizeof progress, "\r%s%s:  %9s",
		    filefits ? "" : "...",
		    file + (filefits ? 0 : filelen - strlen(file)), sizebuf);
		(void)write(STDOUT_FILENO, progress, strlen(progress));

		/*
		 * there is no need to set the previous* variables as done
		 * below, we will not be needing them
		 */

		return;
	}

	(void)gettimeofday(&now, NULL);

	if (end_offset == 0)
		ratio = 1.0;
	else
		ratio = (double)(start_offset + transferred) /
		    (double)end_offset;

	makeprogress(file, ratio, start_offset + transferred);

	if (progresslen < 13) {
		/* do not print anything with a very small terminal */
		previoustransferred = transferred;
		previoustime.tv_sec = now.tv_sec;
		previoustime.tv_usec = now.tv_usec;
		return;
	}

	if (previoustime.tv_sec == 0 && previoustime.tv_usec == 0) {
		/* first call of printprogress for transfer */
		strcpy(etabuf, "   --:-- ETA ");
	} else {
		if (average <= 1) {
			/* current average is useless, create a new one */
			average =
			    1e6 * (double)(transferred - previoustransferred) /
			    (double)timediff(now, previoustime);
		} else {
			/* use little of new average, makes the eta stable */
			average = (int)(0.8 * (double)average + 0.2 * 1e6 *
			    (double)(transferred - previoustransferred) /
			    (double)timediff(now, previoustime));
		}

		/* make sure we will not be dividing by zero */
		if (average <= 0)
			average = 1;

		seconds = (int)((end_offset - start_offset - transferred) /
		    average);

		if (seconds >= 3600 * 24) {
			strcpy(etabuf, "    days ETA ");
			large = small = -1;
		} else if (seconds >= 3600) {
			/* transfer will take hours */
			large = seconds / 3600;
			small = (seconds % 3600) / 60;
		} else {
			/* transfer will take minutes */
			large = seconds / 60;
			small = seconds % 60;
		}

		if (large != -1 && small != -1)
			(void)xsnprintf(etabuf, sizeof etabuf,
			    "   %02.2d:%02.2d ETA ", large, small);
	}

	strcat(progress, etabuf);

	(void)write(STDOUT_FILENO, "\r", 1);
	(void)write(STDOUT_FILENO, progress, strlen(progress));

	previoustransferred = transferred;
	previoustime.tv_sec = now.tv_sec;
	previoustime.tv_usec = now.tv_usec;
}


/*
 * Prints a line showing the completion of the transfer.  makeprogress
 * is used to create a line containing the file name, progressbar
 * (which of course is full) and the final file size.  It then adds
 * the average transfer speed, removes the current line and prints
 * the created line.
 */
static void
printcompleted(double average)
{
	char *suffix = " KMGTP";
	char avgbuf[6];

	/*
	 * use start_offset + transferred here because we could have read
	 * from something which has an incorrect size (/dev/zero has 0)
	 */
	makeprogress(file, 1.0, start_offset + transferred);

	if (progresslen < 13)
		return;

	while (average >= 999.9) {
		++suffix;
		average /= 1024.0;
	}
	(void)xsnprintf(avgbuf, sizeof avgbuf, "%03.1f", average);
	(void)xsnprintf(progress + strlen(progress), sizeof progress -
	    strlen(progress), "%7s %cB/s ", avgbuf, *suffix);

	(void)write(STDOUT_FILENO, "\r", 1);
	(void)write(STDOUT_FILENO, progress, strlen(progress));
	(void)write(STDOUT_FILENO, "\n", 1);
}
