/* $Id$ */

#include "samblah.h"

static void             usage(void);
static void             parse_config(const char *file, int use_file);


/*
 * Reads environment, parses the command line arguments and reads config file.
 * On error, a message is printed to stderr and exit called.
 */
void
do_init(int argc, char **argv)
{
	int	ch;
	int	Popt = 0;
	const char     *carg, *user, *pass, *host, *share, *path;
	char	samblahrc[FALLBACK_PATH_MAXLEN + 1];
	int	use_samblahrc = 0;
	char   *homedir;
	char	passinput[SMB_PASS_MAXLEN + 1];
	const char     *errmsg;

	if (getenv("PAGER") != NULL) {
		errmsg = setvariable("pager", getenv("PAGER"));
		if (errmsg != NULL)
			errx(1, "setting variable pager: %s", errmsg);
	}

	carg = user = pass = host = share = path = NULL;
	eoptind = 1;
	eoptreset = 1;  /* clean egetopt state */
	while ((ch = egetopt(argc, argv, "c:p:Pu:")) != -1)
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
		}

	argc -= eoptind;
	argv += eoptind;

	/* accepting no more than three arguments */
	if (argc > 3) {
		warnx("too many arguments");
		usage();
	}
	if (argc == 1) {
		warnx("missing share");
		usage();
	}

	if (Popt) {
		if (!askpass(passinput))
			errx(1, "could not read password");
		pass = passinput;
	}

	/*
	 * try reading config files in following order:
	 * 1. on command line with -c
	 * 2. from $SAMBLAHRC
	 * 3. from $HOME/.samblahrc
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
		homedir = getenv("HOME");
		if (homedir != NULL) {
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

	/* parse file, on error a message printed exit called */
	if (use_samblahrc >= 0)
		parse_config(samblahrc, use_samblahrc);

	if (argc == 0) {
		return; /* we do not have to connect */
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
"       samblah [-c file] [-u user] [-P | -p pass] [host share [path]]\n");
	exit(1);
}


static void
parse_config(const char *file, int use_file)
{
	FILE   *in;
	char	buf[SAMBLAHRC_LINE_MAXLEN + 2];    /* `+ 2' for \n and \0 */
	char   *cp;
	int	linenum;
	List   *tokens;
	const char *errmsg;

	if ((in = fopen(file, "r")) == NULL) {
		if (use_file)
			err(1, "opening %s", file);
		return;
	}

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
		    *(buf + strspn(buf, " \t")) == '#')
			continue;

		tokens = list_new();
		errmsg = tokenize(buf, tokens);
		if (errmsg != NULL) {
			(void)fclose(in);
			errx(1, "tokenizing line: %s", errmsg);
		}

		if (list_count(tokens) >= 1 &&
		    streql((char *)list_elem(tokens, 0), "set")) {
			if (list_count(tokens) != 3)
				errmsg = "wrong number of arguments";
			else
				errmsg = setvariable(list_elem(tokens, 1), list_elem(tokens, 2));
		} else {
			errmsg = "unknown command";
		}

		list_free(tokens); tokens = NULL;

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
