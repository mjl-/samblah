/* $Id$ */

#include "samblah.h"


static void     printlist(int, struct smb_dirent *, int[], int);


/*
 * Prints a list of hosts on the network, or in workgroup when it
 * is not NULL.
 */
void
smbhlp_list_hosts(const char *workgroup, int lopt)
{
	int argc;
	struct smb_dirent *argv;
	int types[] = { SMB_SERVER, -1 };
	const char *errmsg;

	errmsg = smb_hosts(workgroup, &argc, &argv);
	if (errmsg != NULL) {
		if (workgroup == NULL)
			cmdwarnx("listing all hosts: %s", errmsg);
		else
			cmdwarnx("listing hosts in %s: %s", workgroup, errmsg);
		return;
	}

	printlist(argc, argv, types, lopt);
	free(argv);
}


/*
 * Prints a list of shares on host.  user and pass may be NULL.
 * lopt is for -l, when set extra information (e.g. comments of the
 * shares) will be printed.
 */
void
smbhlp_list_shares(const char *user, const char *pass, const char *host,
    int aopt, int lopt)
{
	int argc, i;
	struct smb_dirent *argv;
	size_t namelen;
	int types[] = { SMB_FILE_SHARE, SMB_PRINTER_SHARE,
	    SMB_COMMS_SHARE, SMB_IPC_SHARE, -1 };
	const char *errmsg;

	errmsg = smb_shares(user, pass, host, &argc, &argv);
	if (errmsg != NULL) {
		cmdwarnx("listing shares on %s: %s", host, errmsg);
		return;
	}

	if (!aopt) {
		/*
		 * when not printing `hidden' shares, truncate
		 * accepted share types.  also remove all shares with
		 * a name ending with `$'
		 */
		types[1] = -1;

		i = 0;
		while (argc != i) {
			namelen = strlen(argv[i].name);
			if (namelen > 0 && argv[i].name[namelen - 1] == '$')
				memcpy(&argv[i], &argv[--argc], sizeof argv[0]);
			else
				++i;
		}
	}

	printlist(argc, argv, types, lopt);
	free(argv);
}


/* Prints a list of all workgroups on the network. */
void
smbhlp_list_workgroups(void)
{
	int argc;
	struct smb_dirent *argv;
	int types[] = { SMB_WORKGROUP, -1 };
	const char *errmsg;

	errmsg = smb_workgroups(&argc, &argv);
	if (errmsg != NULL) {
		cmdwarnx("listing workgroups: %s", errmsg);
		return;
	}

	printlist(argc, argv, types, 0);
	free(argv);
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
	int i, len;
	struct stat st1, st2;
	char npath[SMB_PATH_MAXLEN + 1];

	/* when more than two arguments, always move to last argument */
	if (argnum > 2) {
		/* last argument must exist */
		if (smb_stat(args[argnum - 1], &st1) != 0) {
			cmdwarn("%s", args[argnum - 1]);
			return;
		}

		/* last argument must be directory */
		if (S_ISDIR(st1.st_mode)) {
			cmdwarnx("last argument must be a directory");
			return;
		}

		len = strlen(args[argnum - 1]);

		for (i = 0; !int_signal && i < argnum - 1; ++i) {
			/* path must fit */
			if (len + 1 + strlen(args[i]) > SMB_PATH_MAXLEN) {
				errno = ENAMETOOLONG;
				cmdwarn("in %s", args[argnum - 1]);
				continue;
			}

			/* construct target, npath */
			strcpy(npath, args[argnum - 1]);
			strcat(npath, "/");
			strcat(npath, args[i]);

			/* actually do the moving */
			if (smb_rename(args[i], npath) != 0)
				cmdwarn("renaming %s ", args[i]);
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
			if (strlen(args[0]) + 1 + strlen(args[1]) >
			    SMB_PATH_MAXLEN) {
				errno = ENAMETOOLONG;
				cmdwarn("in %s", args[0]);
				return;
			}

			/* construct target */
			strcpy(npath, args[1]);
			strcat(npath, "/");
			strcat(npath, args[0]);

			/* move argument 1 to directory (argument 2) */
			if (smb_rename(args[0], npath) != 0)
				cmdwarn("renaming %s", args[0]);

			return;
		}
		/* second argument was not directory but existed */
		else {
			cmdwarnx("cannot move directory to existing file");
			return;
		}
	}

	/* just rename first argument to not-yet-existent second argument */
	if (smb_rename(args[0], args[1]) != 0)
		cmdwarn("renaming %s", args[0]);
}


/* Removes the remote file rpath, recursive when ropt is true. */
void
smbhlp_remove(const char *rpath, int ropt)
{
	struct stat st;
	int dh;
	const struct smb_dirent *dent;
	char npath[SMB_PATH_MAXLEN + 1];

	/* only remove one file */
	if (!ropt)  {
		if (smb_unlink(rpath) != 0)
			cmdwarn("removing %s", rpath);
		return;
	}

	/* for checking if it is a directory */
	if (smb_stat(rpath, &st) != 0) {
		cmdwarn("%s", rpath);
		return;
	}

	/* remove contents if it is a directory */
	if (S_ISDIR(st.st_mode)) {
		if ((dh = smb_opendir(rpath)) < 0) {
			cmdwarn("opening %s", rpath);
			return;
		}

		while (!int_signal && (dent = smb_readdir(dh)) != NULL) {
			/* avoid removing "." and ".." recursively */
			if (streql(dent->name, ".") ||
			    streql(dent->name, ".."))
				continue;

			if (strlen(rpath) + 1 + strlen(dent->name) >
			    SMB_PATH_MAXLEN) {
				errno = ENAMETOOLONG;
				cmdwarn("in %s", rpath);
				continue;
			}

			strcpy(npath, rpath);
			strcat(npath, "/");
			strcat(npath, dent->name);

			smbhlp_remove(npath, ropt);
		}

		/* cleanup when interrupted */
		if (int_signal) {
			(void)smb_closedir(dh);

			return;
		}

		if (smb_closedir(dh) != 0) {
			cmdwarn("closing %s", rpath);
			return;
		}

		if (smb_rmdir(rpath) != 0)
			cmdwarn("removing %s", rpath);
	} else {
		/* remove file */
		if (smb_unlink(rpath) != 0)
			cmdwarn("removing %s", rpath);
	}
}


static void
printlist(int argc, struct smb_dirent *argv, int types[], int lopt)
{
	int i;
	int typei;
	struct collist cl;

	if (!lopt)
		col_initlist(&cl);

	for (i = 0; i < argc; ++i) {
		for (typei = 0; types[typei] != -1; ++typei)
			if (argv[i].type == types[typei])
				break;
		if (types[typei] == -1)
			continue;

		if (lopt) {
			printf("%-28s%s\n", argv[i].name, argv[i].comment);
			continue;
		} else {
			if (!col_addlistelem(&cl, argv[i].name)) {
				cmdwarn("printing element");
				col_freelist(&cl);
				return;
			}
		}
	}

	if (!lopt) {
		col_printlist(&cl);
		col_freelist(&cl);
	}
}
