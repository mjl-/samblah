/* $Id$ */

#include "samblah.h"

static void     printlist(List *, int[], int);


/*
 * Prints a list of hosts on the network, or in workgroup when it
 * is not NULL.
 */
void
smbhlp_list_hosts(const char *workgroup, int lopt)
{
	List   *list;
	int	types[] = { SMB_SERVER, -1 };
	const char     *errmsg;

	list = list_new();
	errmsg = smb_hosts(workgroup, list);
	if (errmsg != NULL) {
		if (workgroup == NULL)
			cmdwarnx("listing all hosts: %s", errmsg);
		else
			cmdwarnx("listing hosts in %s: %s", workgroup, errmsg);
		return;
	}

	printlist(list, types, lopt);
	list_free(list);
}


/*
 * Prints a list of shares on host.  User and pass may be NULL.
 * lopt is for -l, when set extra information (e.g. comments of the
 * shares) will be printed.
 */
void
smbhlp_list_shares(const char *user, const char *pass, const char *host,
    int aopt, int lopt)
{
	int	i;
	int	types[] = { SMB_FILE_SHARE, SMB_PRINTER_SHARE,
			    SMB_COMMS_SHARE, SMB_IPC_SHARE, -1 };
	List   *list;
	List   *newlist;
	Smbdirent  *share;
	const char *errmsg;

	list = list_new();
	errmsg = smb_shares(user, pass, host, list);
	if (errmsg != NULL) {
		cmdwarnx("listing shares on %s: %s", host, errmsg);
		return;
	}

	if (!aopt) {
		/*
		 * when not printing `hidden' shares, truncate accepted share
		 * types.  also remove all shares with a name ending with `$'
		 */
		types[1] = -1;

		newlist = list_new();
		for (i = 0; i < list_count(list); ++i) {
			share = (Smbdirent *)list_elem(list, i);
			if (strlen(share->name) > 0 && share->name[strlen(share->name) - 1] != '$')
				list_add(newlist, share);
			else
				free(share);
		}
		/* don't free shares, only free List container */
		list_free_func(list, NULL);
		list = newlist; newlist = NULL;
	}

	printlist(list, types, lopt);
	list_free(list);
}


/* Prints a list of all workgroups on the network. */
void
smbhlp_list_workgroups(void)
{
	int	types[] = { SMB_WORKGROUP, -1 };
	List   *list;
	const char *errmsg;

	list = list_new();
	errmsg = smb_workgroups(list);
	if (errmsg != NULL) {
		cmdwarnx("listing workgroups: %s", errmsg);
		return;
	}

	printlist(list, types, 0);
	list_free(list);
}


/*
 * Changes names of two remote files or moves at least one file to
 * a directory.  argnum must be >= 2, args must contain argnum elements
 * plus a trailing NULL.  If argnum > 2 or (argnum is 2 and second
 * element in args is a directory), then all elements except the last
 * will be moved to the path denoted by the last element.  Otherwise
 * the first element will be renamed to the second element.
 */
void
smbhlp_move(int argnum, char **args)
{
	int	i;
	Str    *targetpath;
	char   *last;
	struct stat	st1, st2;

	/* when more than two arguments, always move to last argument */
	if (argnum > 2) {
		/* last argument must exist */
		last = args[argnum - 1];
		if (smb_stat(last, &st1) != 0) {
			cmdwarn("%s", last);
			return;
		}

		/* last argument must be directory */
		if (S_ISDIR(st1.st_mode)) {
			cmdwarnx("last argument must be a directory");
			return;
		}

		for (i = 0; !int_signal && i < argnum - 1; ++i) {
			/* path must fit */
			if (strlen(last) + 1 + strlen(args[i]) > SMB_PATH_MAXLEN) {
				errno = ENAMETOOLONG;
				cmdwarn("in %s", args[argnum - 1]);
				continue;
			}

			/* construct targetpath */
			targetpath = str_new(args[argnum - 1]);
			str_putcharptr(targetpath, "/");
			str_putcharptr(targetpath, args[i]);

			/* actually do the moving */
			if (smb_rename(args[i], str_charptr(targetpath)) != 0)
				cmdwarn("renaming %s ", args[i]);
			str_free(targetpath); targetpath = NULL;
		}

		/* nothing special to do when interrupted */

		return;
	}

	/* we only have two arguments, the first argument must exist */
	if (smb_stat(args[0], &st2) != 0) {
		cmdwarn("%s", args[0]);
		return;
	}

	/* the second argument could exist (when moving to directory) */
	if (smb_stat(args[1], &st1) == 0) {     /* on success! */

		/* if existing thing is a directory, move to it */
		if (S_ISDIR(st1.st_mode)) {
			/* path must fit */
			if (strlen(args[0]) + 1 + strlen(args[1]) > SMB_PATH_MAXLEN) {
				errno = ENAMETOOLONG;
				cmdwarn("in %s", args[0]);
				return;
			}

			/* construct target */
			targetpath = str_new(args[1]);
			str_putcharptr(targetpath, "/");
			str_putcharptr(targetpath, args[0]);

			/* move argument 1 to directory (argument 2) */
			if (smb_rename(args[0], str_charptr(targetpath)) != 0)
				cmdwarn("renaming %s", args[0]);
			str_free(targetpath); targetpath = NULL;

			return;
		} else {
			/* second argument was not directory but existed */
			cmdwarnx("cannot move directory to existing file");
			return;
		}
	}

	/* just rename first argument to not-yet-existent second argument */
	if (smb_rename(args[0], args[1]) != 0)
		cmdwarn("renaming %s", args[0]);
}


/*
 * Removes the remote file path, recursive when ropt is true.
 */
void
smbhlp_remove(const char *path, int ropt)
{
	int	dh;
	Str    *nextpath;
	struct stat	st;
	Smbdirent      *dent;

	/* only remove one file */
	if (!ropt)  {
		if (smb_unlink(path) != 0)
			cmdwarn("removing %s", path);
		return;
	}

	/* for checking if it is a directory */
	if (smb_stat(path, &st) != 0) {
		cmdwarn("%s", path);
		return;
	}

	/* remove contents if it is a directory */
	if (S_ISDIR(st.st_mode)) {
		dh = smb_opendir(path);
		if (dh < 0) {
			cmdwarn("opening %s", path);
			return;
		}

		while (!int_signal && (dent = smb_readdir(dh)) != NULL) {
			/* avoid removing "." and ".." recursively */
			if (streql(dent->name, ".") || streql(dent->name, ".."))
				continue;

			if (strlen(path) + 1 + strlen(dent->name) > SMB_PATH_MAXLEN) {
				errno = ENAMETOOLONG;
				cmdwarn("in %s", path);
				continue;
			}

			nextpath = str_new(path);
			str_putcharptr(nextpath, "/");
			str_putcharptr(nextpath, dent->name);

			smbhlp_remove(str_charptr(nextpath), ropt);

			str_free(nextpath); nextpath = NULL;
		}

		/* cleanup when interrupted */
		if (int_signal) {
			(void)smb_closedir(dh);
			return;
		}

		if (smb_closedir(dh) != 0) {
			cmdwarn("closing %s", path);
			return;
		}

		if (smb_rmdir(path) != 0)
			cmdwarn("removing %s", path);
	} else {
		/* remove file */
		if (smb_unlink(path) != 0)
			cmdwarn("removing %s", path);
	}
}


static void
printlist(List *list, int types[], int lopt)
{
	int	i;
	int	typeindex;
	List   *strlist;
	Smbdirent      *dirent;

	if (!lopt)
		strlist = list_new();

	for (i = 0; i < list_count(list); ++i) {
		dirent = (Smbdirent *)list_elem(list, i);

		for (typeindex = 0; types[typeindex] != -1; ++typeindex) {
			if (dirent->type == types[typeindex])
				break;
		}
		if (types[typeindex] == -1)
			continue;

		if (lopt)
			printf("%-28s%s\n", dirent->name, dirent->comment);
		else
			list_add(strlist, (char *)dirent->name);
	}

	if (!lopt) {
		printcolumns(strlist);
		/* don't remove elements, only List container */
		list_free_func(strlist, NULL);
	}
}
