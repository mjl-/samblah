/* $Id$ */

#include "samblah.h"


static void             usage(void);
static void             parse_config(const char *file, int use_file);
static const char      *config_set_cmd(int, char * const *);
static const char      *config_alias_cmd(int, char * const *);


/*
 * Reads environment, parses the command line arguments and reads config file.
 * On error, a message is printed to stderr and exit called.
 */
void
do_init(int argc, char **argv)
{
	int ch;
	int Popt = 0;
	char *carg, *user, *pass, *host, *share, *path;
	char samblahrc[FALLBACK_PATH_MAXLEN + 1];
	int use_samblahrc = 0;
	char *homedir;
	char passinput[SMB_PASS_MAXLEN + 1];
	struct alias *al;
	const char *errmsg;

	if (getenv("PAGER") != NULL) {
		errmsg = setvariable("pager", getenv("PAGER"));
		if (errmsg != NULL)
			errx(1, "setting variable pager: %s", errmsg);
	}

	carg = user = pass = host = share = path = NULL;
	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "c:hp:Pu:")) != -1)
		switch(ch) {
		case 'c':
			carg = eoptarg;         /* config file */
			break;
		case 'p':
			pass = eoptarg;
			break;
		case 'P':
			Popt = 1;               /* ask for password */
			break;
		case 'u':
			user = eoptarg;
			break;
		default:
			usage();
			exit(1);
		}

	/* accepting no more than three arguments */
	if (argc - eoptind > 3) {
		warnx("too many arguments");
		usage();
		exit(1);
	}

	/* with an alias as argument, -u, -p and -P may not be used */
	if ((argc - eoptind == 1) && (Popt || user != NULL || pass != NULL)) {
		warnx("illegal combination of options");
		usage();
		exit(1);
	}

	if (Popt) {
		if (!askpass(passinput))
			errx(1, "could not read password");
		pass = passinput;
	}

	/*
	 * try reading config files in following order: on command
	 * line with -c, from $SAMBLAHRC, from $HOME/.samblahrc
	 */
	if (carg != NULL) {
		if (xsnprintf(samblahrc, sizeof samblahrc,
		    "%s", carg) >= sizeof samblahrc) {
			errno = ENAMETOOLONG;
			err(1, "%s", carg);
		}
		use_samblahrc = 1;
	} else if (getenv("SAMBLAHRC") != NULL) {
		if (xsnprintf(samblahrc, sizeof samblahrc, "%s",
		    getenv("SAMBLAHRC")) >= sizeof samblahrc) {
			errno = ENAMETOOLONG;
			err(1, "%s", getenv("SAMBLAHRC"));
		}
		use_samblahrc = 1;
	} else {
		if ((homedir = getenv("HOME")) != NULL) {
			/* construct config path */
			if (xsnprintf(samblahrc, sizeof samblahrc, "%s%s",
			    homedir, "/.samblahrc") >= sizeof samblahrc) {
				errno = ENAMETOOLONG;
				err(1, "$HOME/.samblahrc");
			}
			use_samblahrc = 0;
		} else
			use_samblahrc = -1;     /* do not read config at all */
	}

	argc -= eoptind;
	argv += eoptind;

	/* parse file, on error a message printed exit called */
	if (use_samblahrc >= 0)
		parse_config(samblahrc, use_samblahrc);

	if (argc == 0) {
		return; /* we do not have to connect */
	} else if (argc == 1) {
		/* argument is an alias */
		al = getalias(argv[0]);
		if (al == NULL)
			errx(1, "no such alias");

		host = al->host;
		share = al->share;
		user = al->user;
		pass = al->pass;
		path = al->path;
	} else {
		/* arguments are host, share and possibly path */
		host = argv[0];
		share = argv[1];
		path = argv[2]; /* when argv[1] is last, argv[2] is NULL */
	}

	/* try to open location passed on command line */
	errmsg = smb_connect(host, share, user, pass, path);
	if (errmsg != NULL)
		errx(1, "%s", errmsg);
}


static void
usage(void)
{
	fprintf(stderr,
"usage: samblah [-c file] [alias]\n"
"       samblah [-c file] [-u user] [-P | -p pass] host share [path]\n");
}


static void
parse_config(const char *file, int use_file)
{
	FILE *in;
	char buf[SAMBLAHRC_LINE_MAXLEN + 2];    /* `+ 2' for \n and \0 */
	char *cp;
	int linenum;
	char **tokenv;
	int tokenc;
	const char *errmsg;

	if ((in = fopen(file, "r")) == NULL) {
		if (use_file)
			err(1, "opening %s", file);
		return;
	}

	tokenv = NULL;
	tokenc = 0;
	/* read, parse and execute lines */
	for (linenum = 1; fgets(buf, sizeof buf, in) != NULL; ++linenum) {

		/*
		 * check if line is valid.  with NUL in input, this may
		 * falsely print an error or stop processing after NUL.
		 */
		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = '\0';
		else if (feof(in))
			;       /* does not have a '\n' */
		else {
			(void)fclose(in);
			errx(1, "%s:%u: line too long", file, linenum);
		}

		/* empty line or comment */
		if (*(buf + strspn(buf, " \t")) == '\0' ||
		    *(buf + strspn(buf, " \t")) == '#') {
			tokenv = NULL;
			tokenc = 0;
			continue;
		}

		errmsg = tokenize(buf, &tokenc, &tokenv);
		if (errmsg != NULL) {
			(void)fclose(in);
			errx(1, "tokenizing line: %s", errmsg);
		}

		errmsg = "unknown command";
		if (tokenc >= 1 && streql(tokenv[0], "set"))
			errmsg = config_set_cmd(tokenc, tokenv);
		else if (tokenc >= 1 && streql(tokenv[0], "alias"))
			errmsg = config_alias_cmd(tokenc, tokenv);

		freelist(tokenc, tokenv);
		tokenv = NULL;
		tokenc = 0;

		if (errmsg != NULL) {
			(void)fclose(in);
			errx(1, "%s:%u: %s", file, linenum, errmsg);
		}
	}

	if (ferror(in)) {
		warn("reading %s", file);
		(void)fclose(in);
		exit(1);
	}

	if (fclose(in) != 0)
		err(1, "closing %s", file);
}


static const char *
config_set_cmd(int argc, char * const *argv)
{
	if (argc != 3)
		return "wrong number of arguments";

	return setvariable(argv[1], argv[2]);
}


static const char *
config_alias_cmd(int argc, char * const *argv)
{
	const char *user, *pass;
	const char *alias, *host, *share, *path;
	int ch;

	if (argc < 4)
		return "wrong number of arguments";

	alias = argv[1];

	user = pass = NULL;
	eoptind = 2;    /* skip keyword "alias" and alias itself */
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, ":p:u:")) != -1)
		switch (ch) {
		case 'p':
			pass = eoptarg;
			break;
		case 'u':
			user = eoptarg;
			break;
		case ':':
		default:
			return "illegal option or missing argument";
		}
	argc -= eoptind;
	argv += eoptind;

	/* still need host, share and possibly path */
	if (!(argc == 2 || argc == 3))
		return "wrong number of arguments";

	host = argv[0];
	share = argv[1];
	path = argv[2];         /* when argv[1] is last arg, argv[2] is null */

	return setalias(alias, user, pass, host, share, path);
}
