/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libsmbclient.h>

#include "smbwrap.h"

#define streql(s1, s2)  (strcmp(s1, s2) == 0)


/* Non-zero if connected to a share, not connected by default. */
int connected = 0;


/*
 * These variables contain information about the current connection
 * as initiated using smb_connect.
 * smb_user may be the empty string, in which case authentication
 * is done as `guest' which seems to be what some Windows versions
 * accept.
 */
static char smb_user[SMB_USER_MAXLEN + 1];
static char smb_pass[SMB_PASS_MAXLEN + 1];
static char smb_host[SMB_HOST_MAXLEN + 1];
static char smb_share[SMB_SHARE_MAXLEN + 1];
static char smb_path[SMB_PATH_MAXLEN + 1];

/* Username and password to use when doing a listing. */
static int  doing_listing;
static char list_user[SMB_USER_MAXLEN + 1];
static char list_pass[SMB_PASS_MAXLEN + 1];


static int      listuri(const char *, int *, struct smb_dirent **);
static void     smbc_dirent2smb_dirent(const struct smbc_dirent *from, struct smb_dirent *to);
static void     auth_callback(const char *, const char *, char *, int, char *, int, char *, int);
static void     makeuri(char *);
static void     makecwduri(char *);
static void     makeuri_generic(char *, int);
static int      evaluri(char *, const char *);
static int      evalpath(char *, const char *);
static char    *strrslash(char *, char *);
static int      validhost(const char *);
static int      validshare(const char *);
static int      validuser(const char *);
static int      validpass(const char *);
static int      validworkgroup(const char *);
static int      validpath(const char *);


/*
 * Initializes the samba library.  On success returns non-zero,
 * otherwise returns zero.
 */
int
smb_init(void)
{
	doing_listing = 0;

	/* initialize libsmbclient, second argument is debug level */
	if (smbc_init(auth_callback, 0) == 0)
		return 1;
	return 0;
}


/*
 * Connects to host and share, change to path.  user, pass and path
 * may be NULL.  Must only be called when `connected' is false.
 * On success NULL is returned, otherwise a string describing the
 * error is returned.
 */
char *
smb_connect(const char *host, const char *share, const char *user,
    const char *pass, const char *path)
{
	static char errbuf[SMB_ERRMSG_MAXLEN];
	const char *errmsg;

	/* test if data is valid */
	if (!validconn(user, pass, host, share, path)) {
		errno = EINVAL;
	} else {
		/* fill buffers */
		strcpy(smb_host, host);
		strcpy(smb_share, share);
		strcpy(smb_user, (user != NULL) ? user : "");
		strcpy(smb_pass, (pass != NULL) ? pass : "");
		strcpy(smb_path, "/");

		if (smb_chdir((path == NULL) ? "." : path) == 0) {
			connected = 1;
			return NULL;
		}
	}

	/* handle error */
	switch (errno) {
	case EINVAL:    errmsg = "Invalid destination parameters"; break;
	case ENOENT:    errmsg = "No such host or share"; break;
	case ENODEV:    errmsg = "No such host"; break;
	default:        errmsg = strerror(errno);
	}

	/* XXX use makeuri and friends for this */
	assert(snprintf(errbuf, sizeof errbuf, "smb://%s/%s%s%s: %s", host,
	    share, (path == NULL) ? "" : ((*path == '/') ? "" : "/"),
	    (path == NULL) ? "" : path, errmsg) != -1);

	return errbuf;
}


/*
 * Disconnects.  Always succeeds since libsmbclient has no idea of
 * opening and closing connections, that is all handled internally.
 */
int
smb_disconnect(void)
{
	connected = 0;
	return 0;
}


/*
 * Like chdir(2).  Dot-dot, leading slashes and double slashes are
 * handled appropriately.
 * Possible errno values: any of smbc_opendir or ENAMETOOLONG.
 * - 
 */
int
smb_chdir(const char *path)
{
	int dh;
	int save_errno;

	/* check if buffer is big enough */
	if (strlen(path) > SMB_PATH_MAXLEN) {
		errno = ENAMETOOLONG;
		return -1;
	}

	/* TODO a smb_stat with a S_ISDIR() should be enough */
	dh = smb_opendir(path);
	if (dh < 0)
		return -1;

	save_errno = errno;
	(void)smb_close(dh);

	/* this will succeed since path will fit */
	assert(evalpath(smb_path, path));

	errno = save_errno;
	return 0;
}


/*
 * Returns the string representation of the remote working directory.
 * It is a smburi with host, share and user.  Always succeeds.
 */
const char *
smb_getcwd(void)
{
	static char buf[SMB_URI_MAXLEN + 1];

	makecwduri(buf);

	return buf;
}


/*
 * Like mkdir(2).
 * Possible errno value: any of smbc_mkdir or ENAMETOOLONG.
 */
int
smb_mkdir(const char *path, mode_t mode)
{
	char uribuf[SMB_URI_MAXLEN + 1];

	if (!evaluri(uribuf, path))
		return -1;

	return smbc_mkdir(uribuf, mode);
}


/*
 * Like rmdir(2).
 * Possible errno values: any of smbc_rmdir or ENAMETOOLONG.
 */
int
smb_rmdir(const char *path)
{
	char uribuf[SMB_URI_MAXLEN + 1];

	if (!evaluri(uribuf, path))
		return -1;
	return smbc_rmdir(uribuf);
}


/*
 * Like open(2).  Note that mode is currently not used.
 * Possible errno values:  any of smbc_open or ENAMETOOLONG.
 */
int
smb_open(const char *path, int flags, mode_t mode)
{
	char uribuf[SMB_URI_MAXLEN + 1];

	if (!evaluri(uribuf, path))
		return -1;

	return smbc_open(uribuf, flags, mode);
}


/*
 * Like read(2).
 * Possible errno values: any of smbc_read.
 */
int
smb_read(int fh, void *buf, size_t bufsize)
{
	return smbc_read(fh, buf, bufsize);
}


/*
 * Like write(2).
 * Possible errno values: any of smbc_write.
 */
int
smb_write(int fh, const void *buf, size_t bufsize)
{

	/* TODO check why smbc_write's buffer to write is not const */
	return smbc_write(fh, (void *)buf, bufsize);
}


/*
 * Like lseek(2).
 * Possible errno values: any of smbc_lseek.
 */
off_t
smb_lseek(int fh, off_t offset, int base)
{
	return smbc_lseek(fh, offset, base);
}


/*
 * Like close(2).
 * Possible errno values: any of smbc_close.
 */
int
smb_close(int fh)
{
	return smbc_close(fh);
}


/*
 * Like stat(2).
 * Possible errno values: any of smbc_stat or ENAMETOOLONG.
 */
int
smb_stat(const char *path, struct stat *st)
{
	char uribuf[SMB_URI_MAXLEN + 1];

	if (!evaluri(uribuf, path))
		return -1;

	return smbc_stat(uribuf, st);
}


/*
 * Like fstat(2).
 * Possible errno values: any of smbc_fstat.
 */
int
smb_fstat(int fh, struct stat *st)
{
	return smbc_fstat(fh, st);
}


/*
 * Like rename(2).
 * Possible errno values: any of smbc_rename or ENAMETOOLONG.
 */
int
smb_rename(const char *frompath, const char *topath)
{
	char fromuribuf[SMB_URI_MAXLEN + 1];
	char touribuf[SMB_URI_MAXLEN + 1];

	if (!evaluri(fromuribuf, frompath)|| !evaluri(touribuf, topath))
		return -1;

	return smbc_rename(fromuribuf, touribuf);
}


/*
 * Like unlink(2).
 * Possible errno values: any of smbc_unlink or ENAMETOOLONG.
 */
int
smb_unlink(const char *path)
{
	char uribuf[SMB_URI_MAXLEN + 1];

	if (!evaluri(uribuf, path))
		return -1;

	return smbc_unlink(uribuf);
}


/*
 * Like opendir(2), with return value as directory handle/descriptor.
 * Possible errno values: any of smbc_opendir or ENAMETOOLONG.
 */
int
smb_opendir(const char *path)
{
	char uribuf[SMB_URI_MAXLEN + 1];

	if (!evaluri(uribuf, path))
		return -1;

	return smbc_opendir(uribuf);
}


/*
 * Reads next file/directory for dh.  On failure or end of list
 * NULL is returned, otherwise a pointer to a smb_dirent is returned.
 * Note that it is not possible to detect difference between failure
 * and end of directory.
 */
const struct smb_dirent *
smb_readdir(int dh)
{
	static struct smb_dirent dent;
	const struct smbc_dirent *cdent;

	/* TODO check why smbc_readdir has unsigned int for directory handle */
	cdent = smbc_readdir((unsigned int)dh);
	if (cdent == NULL)
		return NULL;
	smbc_dirent2smb_dirent(cdent, &dent);
	return &dent;
}


/*
 * Like telldir(2).
 * Possible errno values: any of smbc_telldir.
 */
off_t
smb_telldir(int dh)
{
	return smbc_telldir(dh);
}


/*
 * Like lseekdir(2).
 * Possible errno values: any of smbc_lseekdir.
 */
int
smb_lseekdir(int dh, off_t offset)
{
	return smbc_lseekdir(dh, offset);
}


/*
 * Like closedir(2).
 * Possible errno values: any of smbc_closedir.
 */
int
smb_closedir(int dh)
{
	return smbc_closedir(dh);
}


const char *
smb_workgroups(int *argc, struct smb_dirent **argv)
{
	int r;

	switch (r = listuri("smb://", argc, argv)) {
	case 0:         return NULL;
	default:        return strerror(r);
	}
}


const char *
smb_hosts(const char *workgroup, int *argc, struct smb_dirent **argv)
{
	char uribuf[SMB_URI_MAXLEN + 1];
	int r;

	if (workgroup != NULL && !validworkgroup(workgroup))
		return "Invalid arguments";

	/* clear username and password */
	strcpy(list_user, "");
	strcpy(list_pass, "");

	/* create uri */
	strcpy(uribuf, "smb://");
	if (workgroup != NULL)
		strcat(uribuf, workgroup);

	switch (r = listuri(uribuf, argc, argv)) {
	case 0:         return NULL;
	case ENODEV:    return "No such workgroup";
	case ENOENT:    return "No such workgroup";
	default:        return strerror(r);
	}
}


const char *
smb_shares(const char *user, const char *pass, const char *host,
    int *argc, struct smb_dirent **argv)
{
	char uribuf[SMB_URI_MAXLEN + 1];
	int r;

	/* check if user, pass and host are valid */
	if ((user == NULL && pass != NULL) ||
	    (user != NULL && !validuser(user)) ||
	    (pass != NULL && !validpass(pass)) || !validhost(host))
		return "Invalid arguments";

	/* create the uri */
	strcpy(uribuf, "smb://");
	if (user != NULL && !streql(smb_user, "")) {
		strcat(uribuf, smb_user);
		if (pass != NULL && !streql(smb_pass, "")) {
			strcat(uribuf, ":");
			strcat(uribuf, smb_pass);
		}
		strcat(uribuf, "@");
	}
	strcat(uribuf, host);

	/* set list_user and list_pass for use in auth_callback */
	strcpy(list_user, (user != NULL) ? user : "");
	strcpy(list_pass, (pass != NULL) ? pass : "");

	switch (r = listuri(uribuf, argc, argv)) {
	case 0:         return NULL;
	case ENODEV:    return "No such host";
	case ENOENT:    return "No such host";
	default:        return strerror(r);
	}
}



/*
 * Retrieves contents the directory denoted by uri.  Results are
 * stored in argv, the count in argc.  On success, true is returned,
 * false otherwise.  The contents of argv must be freed when the call
 * succeeds, on failure all allocated memory is freed automatically.
 */
static int
listuri(const char *uri, int *argc, struct smb_dirent **argv)
{
	int dh;
	const struct smbc_dirent *dent;
	void *tmp;
	int save_errno;

	*argc = 0;
	*argv = NULL;
	doing_listing = 1;

	if ((dh = smbc_opendir(uri)) < 0)
		goto error;

	while ((dent = smbc_readdir(dh)) != NULL) {
		/* skip "." and ".." */
		if (streql(dent->name, ".") || streql(dent->name, ".."))
			continue;

		tmp = realloc(*argv, sizeof (struct smb_dirent) * (*argc + 1));
		if (tmp == NULL) {
			save_errno = errno;
			(void)smbc_closedir(dh);
			errno = save_errno;
			goto error;
		}
		*argv = (struct smb_dirent *)tmp;
		smbc_dirent2smb_dirent(dent, *argv + *argc);
		++*argc;
	}

	if (smbc_closedir(dh) != 0)
		goto error;

	doing_listing = 0;
	return 0;

error:
	free(*argv); *argv = NULL;
	*argc = 0;

	/* TODO remove this when libsmbclient is fixed */
	if (errno == 0)
		errno = ENOSYS;

	doing_listing = 0;
	return errno;

}


static void
smbc_dirent2smb_dirent(const struct smbc_dirent *from, struct smb_dirent *to)
{
	assert(strlen(from->name) < sizeof to->name);
	assert(strlen(from->comment) < sizeof to->comment);

	to->type = from->smbc_type;
	strcpy(to->name, from->name);
	strcpy(to->comment, from->comment);
}


/* ARGSUSED */
static void
auth_callback(const char *host, const char *share, char *wg, int wglen,
    char *user, int userlen, char *pass, int passlen)
{
	char *connuser, *connpass;

	if (doing_listing) {
		connuser = list_user;
		connpass = list_pass;
	} else {
		connuser = smb_user;
		connpass = smb_pass;
	}

	if (userlen > strlen(connuser) + 1 || *connuser != '\0')
		strcpy(user, connuser);
	if (*connuser == '\0' && userlen >= strlen("guest") + 1)
		strcpy(user, "guest");

	if (passlen > strlen(connpass) + 1 || *connpass != '\0')
		strcpy(pass, connpass);
}


static void
makecwduri(char *uribuf)
{
	makeuri_generic(uribuf, 1);
}


static void
makeuri(char *uribuf)
{
	makeuri_generic(uribuf, 0);
}


static void
makeuri_generic(char *uribuf, int cwd)
{
	strcpy(uribuf, "smb://");
	if (!streql(smb_user, "")) {
		strcat(uribuf, smb_user);
		if (!streql(smb_pass, "")) {
			strcat(uribuf, ":");
			strcat(uribuf, (cwd ? "PASSWORD" : smb_pass));
		}
		strcat(uribuf, "@");
	}
	strcat(uribuf, smb_host);
	strcat(uribuf, "/");
	strcat(uribuf, smb_share);
	strcat(uribuf, smb_path);
}


static int
evaluri(char *uribuf, const char *npath)
{
	char save[SMB_PATH_MAXLEN + 1];

	if (strlen(smb_path) + 1 + strlen(npath) > SMB_PATH_MAXLEN) {
		errno = ENAMETOOLONG;
		return 0;
	}

	/* save current path */
	strcpy(save, smb_path);

	/* use smb_path to create the new path */
	if (!evalpath(smb_path, npath))
		return 0;       /* errno set by evalpath */

	/* create the uri in our buffer */
	makeuri(uribuf);

	/* restore our path */
	strcpy(smb_path, save);

	return 1;
}


/*
 * Combine opath and npath and write it to opath.  If it does not
 * fit, zero is returned and path remains unchanged.  On success
 * non-zero is returned.
 */
static int
evalpath(char *opath, const char *npath)
{
	char buf[SMB_PATH_MAXLEN * 2 + 2];
	char *ptr, *slashptr;

	/* copy path to buf in which we will work */
	strcpy(buf, opath);

	if (*npath == '/') {
		strcpy(buf, npath);
	} else {
		strcat(buf, "/");
		strcat(buf, npath);
	}

	while ((ptr = strstr(buf, "//")) != NULL)
		memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);

	while ((ptr = strstr(buf, "/./")) != NULL)
		memmove(ptr, ptr + 2, strlen(ptr + 2) + 1);

	if (streql(buf + strlen(buf) - 2, "/."))
		*(buf + strlen(buf) - 1) = '\0';

	while ((ptr = strstr(buf, "/../")) != NULL) {
		slashptr = strrslash(buf, ptr);
		memmove((slashptr != NULL ? slashptr : buf), ptr + 3,
		    strlen(ptr + 3) + 1);
	}

	if (streql(buf + strlen(buf) - 3, "/..")) {
		slashptr = strrslash(buf, buf + strlen(buf) - 3);
		if (slashptr == NULL)
			strcpy(buf, "/");
		else
			*(slashptr + 1) = '\0';
	}

	if (strlen(buf) != 1 && *(buf + strlen(buf) - 1) == '/')
		*(buf + strlen(buf) - 1) = '\0';

	if (strlen(buf) > SMB_PATH_MAXLEN) {
		errno = ENAMETOOLONG;
		return 0;
	}

	/* copy the new and updated path */
	strcpy(opath, buf);

	return 1;
}


static char *
strrslash(char *begin, char *end)
{
	char *last, *curr;

	last = NULL;
	curr = begin - 1;

	while ((curr = strchr(curr + 1, '/')) < end)
		last = curr;

	return last;
}


int
validconn(const char *user, const char *pass, const char *host,
	const char *share, const char *path)
{
	return (share == NULL || !validshare(share) ||
	    host == NULL || !validhost(host) ||
	    (user == NULL && pass != NULL) ||
	    (user != NULL && !validuser(user)) ||
	    (pass != NULL && !validpass(pass)) ||
	    (path != NULL && !validpath(path))) ? 0 : 1;
}


/*
 * Six functions to test whether a given host, share, username, password,
 * workgroup or path is valid (i.e. does not contain certain characters and
 * is not empty).
 */


static int
validuser(const char *user)
{
	assert(user != NULL);
	return strlen(user) <= SMB_USER_MAXLEN && strpbrk(user, "/:@;") == NULL;
}


static int
validpass(const char *pass)
{
	assert(pass != NULL);
	return strlen(pass) <= SMB_PASS_MAXLEN && strpbrk(pass, "/@:;") == NULL;
}


static int
validhost(const char *host)
{
	assert(host != NULL);
	return *host != '\0' && strlen(host) <= SMB_HOST_MAXLEN &&
	    strpbrk(host, "/:@;") == NULL;
}


static int
validshare(const char *share)
{
	assert(share != NULL);
	return *share != '\0' && strlen(share) <= SMB_SHARE_MAXLEN &&
	    strpbrk(share, "/") == NULL;
}


static int
validworkgroup(const char *wg)
{
	assert(wg != NULL);
	return *wg != '\0' && strlen(wg) <= SMB_WORKGROUP_MAXLEN &&
	    strpbrk(wg, "/:@;") == NULL;
}


static int
validpath(const char *path)
{
	assert(path != NULL);
	return strlen(path) <= SMB_PATH_MAXLEN;
}
