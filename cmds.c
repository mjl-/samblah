/* $Id$ */

#include "samblah.h"


/*
 * IMPORTANT: When changing a command make sure the help/description info in
 * the struct command below is changed accordingly.
 */


static void     cmd_cd(int, char **);
static void     cmd_chmod(int, char **);
static void     cmd_close(int, char **);
static void     cmd_get(int, char **);
static void     cmd_help(int, char **);
static void     cmd_lcd(int, char **);
static void     cmd_lpwd(int, char **);
static void     cmd_ls(int, char **);
static void     cmd_lshosts(int, char **);
static void     cmd_lsshares(int, char **);
static void     cmd_lsworkgroups(int, char **);
static void     cmd_lumask(int, char **);
static void     cmd_mkdir(int, char **);
static void     cmd_mv(int, char **);
static void     cmd_open(int, char **);
static void     cmd_page(int, char **);
static void     cmd_put(int, char **);
static void     cmd_pwd(int, char **);
static void     cmd_quit(int, char **);
static void     cmd_rm(int, char **);
static void     cmd_rmdir(int, char **);
static void     cmd_set(int, char **);
static void     cmd_umask(int, char **);
static void     cmd_version(int, char **);

static void     usage(void);
static void     usage_command(const char *);
static void     options(const char *);


/*
 * Note that two last fields in struct command can contain 11 entries
 * and a NULL.  If longer fields are needed, change the size of the
 * fields in cmds.h.
 */
struct command cmdv[] = {
    { "cd", cmd_cd, CMD_MUSTCONN,
      "change current remote directory",
      { "cd directory", NULL },
      { NULL } },
    { "chmod", cmd_chmod, CMD_MUSTCONN,
      "change mode of remote file or directory",
      { "chmod mode file ...", NULL },
      { NULL } },
    { "close", cmd_close, CMD_MUSTCONN,
      "close current connection",
      { "close", NULL },
      { NULL } },
    { "get", cmd_get, CMD_MUSTCONN,
      "retrieve remote files",
      { "get [-rcfs] file ...", "get [-rcfs] -o file1 file2", NULL },
      { "-c       resume (continue) local file if it exists",
        "-f       force overwrite of local file if it exists",
        "-o file  give local file specified name",
        "-r       retrieve recursively",
        "-s       skip if local file exists",
        NULL } },
    { "help", cmd_help, CMD_MAYCONN,
      "print help information of command",
      { "help [command]", NULL },
      { NULL } },
    { "lcd", cmd_lcd, CMD_MAYCONN,
      "change current local directory",
      { "lcd directory", NULL },
      { NULL } },
    { "lpwd", cmd_lpwd, CMD_MAYCONN,
      "print current local directory",
      { "lpwd", NULL },
      { NULL } },
    { "ls", cmd_ls, CMD_MUSTCONN,
      "list remote files",
      { "ls [-lr] file ...", NULL },
      { "-l print extra information",
        "-r recursively list directories",
        NULL } },
    { "lshosts", cmd_lshosts, CMD_MAYCONN,
      "list hosts in workgroup",
      { "lshosts workgroup", NULL },
      { "-l       print extra information",
        NULL } },
    { "lsshares", cmd_lsshares, CMD_MAYCONN,
      "list shares on host",
      { "lsshares [-a] [-l] [-u user] [-P | -p pass] host", NULL },
      { "-a       print hidden shares",
        "-l       print extra information",
        "-p pass  use given password",
        "-P       prompt for password",
        "-u user  use given username",
        NULL } },
    { "lsworkgroups", cmd_lsworkgroups, CMD_MAYCONN,
      "list workgroups on network",
      { "lsworkgroups", NULL },
      { NULL } },
    { "lumask", cmd_lumask, CMD_MAYCONN,
      "change local umask",
      { "lumask mode", NULL },
      { NULL } },
    { "mkdir", cmd_mkdir, CMD_MUSTCONN,
      "create directories on remote host",
      { "mkdir directory ...", NULL },
      { NULL } },
    { "mv", cmd_mv, CMD_MUSTCONN,
      "rename remote file or move remote files to directory",
      { "mv file1 file2", "mv file ... directory", NULL },
      { NULL } },
    { "open", cmd_open, CMD_MUSTNOTCONN,
      "open connection to host or alias",
      { "open [-u user] [-P | -p pass] host share [path]",
        "open alias",
        NULL },
      { "-p pass  login using given password",
        "-P       prompt for password",
        "-u user  login using given username",
        NULL } },
    { "page", cmd_page, CMD_MUSTCONN,
      "show remote file in pager",
      { "page file", NULL },
      { NULL } },
    { "put", cmd_put, CMD_MUSTCONN,
      "write local files and directories to remote host",
      { "put [-cfrs] file ...", "put [-cfrs] -o file1 file2", NULL },
      { "-c       resume (continue) remote file if it exists",
        "-f       force overwriting remote file if it exists",
        "-o file  give remote file specified name",
        "-r       upload recursively",
        "-s       skip if remote file exists",
        NULL } },
    { "pwd", cmd_pwd, CMD_MUSTCONN,
      "print current remote working directory",
      { "pwd", NULL },
      { NULL } },
    { "quit", cmd_quit, CMD_MAYCONN,
      "quit program",
      { "quit", NULL },
      { NULL } },
    { "rm", cmd_rm, CMD_MUSTCONN,
      "remove remote files",
      { "rm [-r] file ...", NULL },
      { "-r recursively remove remote directories",
        NULL } },
    { "rmdir", cmd_rmdir, CMD_MUSTCONN,
      "remove remote directories",
      { "rmdir directory ...", NULL },
      { NULL } },
    { "set", cmd_set, CMD_MAYCONN,
      "show/modify values of variables",
      { "set [variable [value]]", NULL },
      { NULL } },
    { "umask", cmd_umask, CMD_MAYCONN,
      "change remote umask",
      { "umask mode", NULL },
      { NULL } },
    { "version", cmd_version, CMD_MAYCONN,
      "show version information",
      { "version", NULL },
      { NULL } }
};
int cmdc = sizeof cmdv / sizeof (struct command);


static void
usage_command(const char *cmd)
{
	int i, j;

	/* find command in list */
	for (i = 0; i < cmdc; ++i) {
		if (!streql(cmd, cmdv[i].name))
			continue;

		/* print the usage lines, with a leading "usage:" once */
		for (j = 0; cmdv[i].usage[j] != NULL; ++j)
			printf("%6s %s\n",
			    (j == 0) ? "usage:" : "", cmdv[i].usage[j]);
		return;
	}
}


static void
usage(void)
{
	usage_command(cmdname);
}


static void
options(const char *cmd)
{
	int i, j;

	/* find command in list */
	for (i = 0; i < cmdc; ++i) {
		if (!streql(cmd, cmdv[i].name))
			continue;

		/* print the usage lines, with a leading "options:" once */
		for (j = 0; cmdv[i].options[j] != NULL; ++j)
			printf("%8s %s\n",
			    (j == 0) ? "options:" : "", cmdv[i].options[j]);
		return;
	}
}


/* Begin of the commands. */

static void
cmd_cd(int argc, char **argv)
{
	if (argc != 2) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	if (smb_chdir(argv[1]) != 0)
		cmdwarn("%s", argv[1]);
}


/* ARGSUSED */
static void
cmd_chmod(int argc, char **argv)
{
	cmdwarnx("not implemented, see manual page for details");
}


/* ARGSUSED */
static void
cmd_close(int argc, char **argv)
{
	if (argc != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	/* always succeeds */
	(void)smb_disconnect();
}


static void
cmd_get(int argc, char **argv)
{
	int ch;
	int copt = 0, fopt = 0, ropt = 0, sopt = 0;
	char *oarg = NULL;
	int exist;
	struct stat st;

	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "cfo:rs")) != -1)
		switch (ch) {
		case 'c':
			copt = 1;
			break;
		case 'f':
			fopt = 1;
			break;
		case 'o':
			oarg = eoptarg;
			break;
		case 'r':
			ropt = 1;
			break;
		case 's':
			sopt = 1;
			break;
		default:
			usage();
			return;
		}

	if (argc  - eoptind < 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	if ((copt && fopt) || (copt && sopt) || (fopt && sopt) ||
	    (oarg != NULL && argc - eoptind != 1) ||
	    (ropt && (oarg != NULL))) {
		cmdwarnx("illegal combination of options");
		usage();
		return;
	}

	argc -= eoptind;
	argv += eoptind;

	exist = getvariable_onexist("onexist");
	if (copt) exist = VAR_RESUME;
	if (fopt) exist = VAR_OVERWRITE;
	if (sopt) exist = VAR_SKIP;
	/*
	 * Note that a pointer to exist is passed to transfer_get below so
	 * that `{overwrite,resume,skip} all' is kept for all arguments in argv
	 */

	/* if output file has been specified, get the only argument to it */
	if (oarg != NULL) {
		transfer_get(argv[0], oarg, &exist, 0);
		return;
	}

	/* retrieve each argument */
	for (;!int_signal && *argv != NULL; ++argv) {
		/* get attributes to check if it is a file */
		if (smb_stat(*argv, &st) != 0) {
			cmdwarn("%s", *argv);
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {
			/* files do not need directory part locally */
			if ((oarg = strrchr(*argv, '/')) == NULL)
				oarg = *argv;
			else
				++oarg;
		} else if (!ropt) {
			cmdwarnx("cannot retrieve directory non-recursively");
			continue;
		} else if (**argv == '/' || streql(*argv, "..") ||
		    strncmp(*argv, "../", 3) == 0) {
			/* retrieve to `.' locally */
			oarg = ".";
		} else {
			/* retrieve to same directory locally */
			oarg = *argv;
		}
		transfer_get(*argv, oarg, &exist, ropt);
	}
}


static void
cmd_help(int argc, char **argv)
{
	int i;
	struct collist cl;

	switch (argc) {
	case 1:
		/* initialize list */
		col_initlist(&cl);

		/* add all elements to list, then print and free */
		for (i = 0; i < cmdc; ++i)
			if (!col_addlistelem(&cl, cmdv[i].name)) {
				cmdwarn("adding to buffer");
				col_freelist(&cl);
				return;
			}
		col_printlist(&cl);
		col_freelist(&cl);
		break;
	case 2:
		/* find command to give help about */
		for (i = 0; i < cmdc; ++i) {
			if (!streql(argv[1], cmdv[i].name))
				continue;

			printf("description: %s\n", cmdv[i].descr);
			usage_command(argv[1]);
			options(argv[1]);
			break;
		}
		if (i == cmdc)
			cmdwarnx("no such command");
		break;
	default:
		cmdwarnx("wrong number of arguments");
		usage();
	}
}


static void
cmd_lcd(int argc, char **argv)
{
	if (argc != 2) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	if (chdir(argv[1]) != 0)
		cmdwarn("%s", argv[1]);
}


/* ARGSUSED */
static void
cmd_lpwd(int argc, char **argv)
{
	char *lcwd;
	long lcwdsize;

	if (argc != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	/* determine maximum possible size for path */
	if ((lcwdsize = pathconf(".", _PC_PATH_MAX)) == -1)
		lcwdsize = FALLBACK_PATH_MAXLEN;

	/* allocate memory, should be enough to hold longest possible path */
	lcwd = malloc((size_t)lcwdsize);
	if (lcwd == NULL) {
		cmdwarn("cwd buffer");
		return;
	}

	if (getcwd(lcwd, (size_t)lcwdsize) != NULL)
		printf("%s\n", lcwd);
	else
		cmdwarn("working directory");
	free(lcwd);
}


static void
cmd_ls(int argc, char **argv)
{
	int ch;
	int lopt = 0, ropt = 0;
	char *dotargv[] = { ".", NULL };

	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "lr")) != -1)
		switch (ch) {
		case 'l':
			lopt = 1;
			break;
		case 'r':
			ropt = 1;
			break;
		default:
			usage();
			return;
		}

	argc -= eoptind;
	argv += eoptind;

	/* list current directory by default */
	if (argc == 0) {
		argc = 1;
		argv = dotargv;
	}
	cmdls_list(argc, argv, lopt, ropt);
}


static void
cmd_lshosts(int argc, char **argv)
{
	int ch;
	int lopt = 0;

	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "l")) != -1)
		switch (ch) {
		case 'l':
			lopt = 1;
			break;
		default:
			usage();
			return;
		}

	argc -= eoptind;
	argv += eoptind;

	if (argc != 0 && argc != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	/* mind that when argc is 1 then argv[0] is NULL */
	smbhlp_list_hosts(argv[0], lopt);
}


static void
cmd_lsshares(int argc, char **argv)
{
	int ch;
	char *user = NULL;
	char *pass = NULL;
	char passinput[SMB_PASS_MAXLEN + 1];
	int aopt = 0, lopt = 0, Popt = 0;

	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "alp:Pu:")) != -1)
		switch (ch) {
		case 'a':
			aopt = 1;
			break;
		case 'l':
			lopt = 1;
			break;
		case 'p':
			pass = eoptarg;
			break;
		case 'P':
			Popt = 1;
			break;
		case 'u':
			user = eoptarg;
			break;
		default:
			usage();
			return;
		}

	if (argc - eoptind != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	if (pass != NULL && Popt) {
		cmdwarnx("-p and -P cannot be used together");
		usage();
		return;
	}

	if (Popt) {
		if (!askpass(passinput)) {
			cmdwarnx("could not read password");
			return;
		} else
			pass = passinput;
	}

	argc -= eoptind;
	argv += eoptind;

	smbhlp_list_shares(user, pass, argv[0], aopt, lopt);
}


/* ARGSUSED */
static void
cmd_lsworkgroups(int argc, char **argv)
{
	if (argc != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	smbhlp_list_workgroups();
}


static void
cmd_lumask(int argc, char **argv)
{
	mode_t modeold = 0;
	mode_t modetmp = 0;
	unsigned int inmode;

	switch (argc) {
	case 1:
		/* show current umask */
		modetmp = umask(modeold), umask(modetmp);
		modetmp &= S_IRWXU|S_IRWXG|S_IRWXO;
		printf("%03o\n", modetmp);
		break;
	case 2:
		/* set new umask */
		if (strlen(argv[1]) > 3 || strlen(argv[1]) < 1 ||
		    strlen(argv[1]) != strspn(argv[1], "01234567") ||
		    sscanf(argv[1], "%o", &inmode) != 1) {
			cmdwarnx("invalid umask");
			return;
		}

		umask((mode_t)(inmode&(S_IRWXU|S_IRWXG|S_IRWXO)));
		break;
	default:
		cmdwarnx("wrong number of arguments");
		usage();
	}
}


static void
cmd_mkdir(int argc, char **argv)
{
	if (argc == 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	/* make all directories, mode is not used */
	for (++argv; *argv != NULL && !int_signal; ++argv)
		if (smb_mkdir(*argv, (mode_t)0) != 0)
			cmdwarn("creating %s", *argv);
}


static void
cmd_mv(int argc, char **argv)
{
	if (argc <= 2) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	/* pass arguments minus program name */
	smbhlp_move(argc - 1, argv + 1);
}


static void
cmd_open(int argc, char **argv)
{
	int ch;
	int Popt = 0;
	char passinput[SMB_PASS_MAXLEN + 1];
	char *host, *share, *user = NULL, *pass = NULL, *path = NULL;
	struct alias *al;
	char *errmsg;

	if (argc == 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	if (argc == 2) {
		/* argument is an alias */
		al = getalias(argv[1]);
		if (al == NULL) {
			cmdwarnx("no such alias");
			return;
		}

		host = al->host;
		share = al->share;
		user = al->user;
		pass = al->pass;
		path = al->path;

		errmsg = smb_connect(host, share, user, pass, path);
		if (errmsg != NULL)
			cmdwarnx("%s", errmsg);
		return;
	}

	/* we will have to use egetopt and read arguments */

	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "p:Pu:")) != -1)
		switch (ch) {
		case 'p':
			pass = eoptarg;
			break;
		case 'P':
			Popt = 1;
			break;
		case 'u':
			user = eoptarg;
			break;
		default:
			usage();
			return;
		}

	if (argc - eoptind != 2 && argc - eoptind != 3) {
		cmdwarnx("host and share expected");
		usage();
		return;
	}

	/* cannot have -p and -P */
	if (pass != NULL && Popt) {
		cmdwarnx("-p and -P cannot be used together");
		usage();
		return;
	}

	if (Popt) {
		if (!askpass(passinput)) {
			cmdwarnx("could not read password");
			return;
		} else
			pass = passinput;
	}

	argc -= eoptind;
	argv += eoptind;

	host = argv[0];
	share = argv[1];
	path = argv[2];         /* argv[2] could be NULL */

	/* ready for actual opening of connection */
	errmsg = smb_connect(host, share, user, pass, path);
	if (errmsg != NULL)
		cmdwarnx("%s", errmsg);
}


static void
cmd_page(int argc, char **argv)
{
	const char *pager;
	char *file;
	char *tmpdir;
	size_t filelen;
	char *pagecmd;
	int fd;

	if (argc != 2) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	/* make template, try using $TMPDIR, fall back to /tmp */
	tmpdir = (getenv("TMPDIR") != NULL) ? getenv("TMPDIR") : "/tmp";
	filelen = strlen(tmpdir) + strlen("/samblah-XXXXXX");
	file = malloc(filelen + 1);
	if (file == NULL) {
		cmdwarn("mkstemp template");
		return;
	}
	strcpy(file, tmpdir);
	strcat(file, "/samblah-XXXXXX");

	if ((fd = mkstemp(file)) == -1) {
		cmdwarn("temporary file");
		free(file);
		return;
	}

	if (!transfer_get_fd(argv[1], fd)) {    /* fd will be always closed */
		(void)unlink(file);
		free(file);
		return;         /* error message has already been printed */
	}

	pager = getvariable_string("pager");
	pagecmd = malloc(strlen(pager) + 1 + filelen + 1);
	if (pagecmd == NULL) {
		cmdwarn("page command");
		(void)unlink(file);
		free(file);
		return;
	}
	strcpy(pagecmd, getvariable_string("pager"));
	strcat(pagecmd, " ");
	strcat(pagecmd, file);
	(void)system(pagecmd);

	if (unlink(file) != 0)
		cmdwarn("removing %s", file);
	free(pagecmd);
	free(file);
}


static void
cmd_put(int argc, char **argv)
{
	int ch;
	int copt = 0, fopt = 0, ropt = 0, sopt = 0;
	char *oarg = NULL;
	int exist;
	struct stat st;

	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "cfo:rs")) != -1)
		switch (ch) {
		case 'c':
			copt = 1;
			break;
		case 'f':
			fopt = 1;
			break;
		case 'o':
			oarg = eoptarg;
			break;
		case 'r':
			ropt = 1;
			break;
		case 's':
			sopt = 1;
			break;
		default:
			usage();
			return;
		}

	if (argc - eoptind < 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	if ((copt && fopt) || (copt && sopt) || (fopt && sopt) ||
	    (oarg != NULL && argc - eoptind != 1) ||
	    (ropt && (oarg != NULL))) {
		cmdwarnx("illegal combination of options");
		usage();
		return;
	}

	argc -= eoptind;
	argv += eoptind;

	exist = getvariable_onexist("onexist");
	if (copt) exist = VAR_RESUME;
	if (fopt) exist = VAR_OVERWRITE;
	if (sopt) exist = VAR_SKIP;
	/*
	 * Note that a pointer to exist is passed to transfer_get below so
	 * that `{overwrite,resume,skip} all' is kept for all arguments in argv
	 */

	/* if output file has been specified, `put' the only argument to it */
	if (oarg != NULL) {
		transfer_put(argv[0], oarg, &exist, 0);
		return;
	}

	/* `put' each argument */
	for (;*argv != NULL && !int_signal; ++argv) {
		/* get the attributes for check for file or directory */
		if (stat(*argv, &st) != 0) {
			cmdwarn("%s", *argv);
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {
			/* files do not need directory part */
			if ((oarg = strrchr(*argv, '/')) == NULL)
				oarg = *argv;
			else
				++oarg;
		} else if (!ropt) {
			cmdwarnx("cannot put directory non-recursively");
			continue;
		} else if (**argv == '/' || streql(*argv, "..") ||
		    strncmp(*argv, "../", 3) == 0) {
			/* retrieve to `.' locally */
			oarg = ".";
		} else  {
			/* retrieve to same directory locally */
			oarg = *argv;
		}
		transfer_put(*argv, oarg, &exist, ropt);
	}
}


/* ARGSUSED */
static void
cmd_pwd(int argc, char **argv)
{
	if (argc != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	printf("%s\n", smb_getcwd());
}


/* ARGSUSED */
static void
cmd_quit(int argc, char **argv)
{
	if (argc != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	if (connected)
		/* always succeeds */
		(void)smb_disconnect();

	/* tell interface we are done with the command reading */
	done = 1;
}


static void
cmd_rm(int argc, char **argv)
{
	int ch;
	int ropt = 0;
	
	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "r")) != -1)
		switch (ch) {
		case 'r':
			ropt = 1;
			break;
		default:
			usage();
			return;
		}

	if (argc - eoptind < 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	argc -= eoptind;
	argv += eoptind;

	for (;*argv != NULL && !int_signal; ++argv)
		smbhlp_remove(*argv, ropt);
}


static void
cmd_rmdir(int argc, char **argv)
{
	if (argc == 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	for (++argv; *argv != NULL && !int_signal; ++argv)
		if (smb_rmdir(*argv) != 0)
			cmdwarn("removing %s", *argv);
}


static void
cmd_set(int argc, char **argv)
{
	int i;
	const char *value;
	const char *errmsg;

	switch (argc) {
	case 1:
		/* variablenamec and variablenamev are from vars.c */
		for (i = 0; i < variablenamec; ++i)
			printf("%s = %s\n", variablenamev[i],
			    getvariable(variablenamev[i]));
		break;
	case 2:
		value = getvariable(argv[1]);
		if (value == NULL) {
			cmdwarnx("no such variable");
			return;
		}
		printf("%s = %s\n", argv[1], value);
		break;
	case 3:
		errmsg = setvariable(argv[1], argv[2]);
		if (errmsg != NULL)
			cmdwarnx("%s", errmsg);
		break;
	default:
		cmdwarnx("wrong number of arguments");
		usage();
	}
}


/* ARGSUSED */
static void
cmd_umask(int argc, char **argv)
{
	cmdwarnx("not implemented, see manual page for details");
}


/* ARGSUSED */
static void
cmd_version(int argc, char **argv)
{
	if (argc != 1) {
		cmdwarnx("wrong number of arguments");
		usage();
		return;
	}

	printf("samblah %s\n", VERSION);
}
