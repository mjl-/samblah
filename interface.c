/* $Id$ */

#include "samblah.h"


int done = 0;           /* when true, program should stop */
const char *cmdname = "samblah";        /* contains running command */
volatile sig_atomic_t int_signal = 0;   /* set to true on SIGINT */
volatile sig_atomic_t winch_signal = 0; /* set to true on SIGWINCH */


static int cols = -1;   /* width of screen */
static int lines;       /* unused, needed for setting cols */


static void     do_line(void);
static void     do_command(int *, char ***);
static void     int_handler(int);
static void     winch_handler(int);


/*
 * Starts the command reading and executing loop.  When `quit' is executed or
 * on EOF it returns.  On error it prints a message and exits.
 */
void
do_interface(void)
{
	struct sigaction winchact, intact;

	/* terminal resize signal handler */
	winchact.sa_handler = winch_handler;
	winchact.sa_flags = SA_RESTART;
	sigemptyset(&winchact.sa_mask);
	if (sigaction(SIGWINCH, &winchact, NULL) != 0)
		err(1, "setting SIGWINCH handler");

	/* interrupt signal handler */
	intact.sa_handler = int_handler;
	intact.sa_flags = SA_RESTART;
	sigemptyset(&intact.sa_mask);
	if (sigaction(SIGINT, &intact, NULL) != 0)
		err(1, "setting SIGINT handler");

	/* for conditional parsing in .inputrc and perhaps more */
	rl_readline_name = "samblah";

	/* initialize readline, so we can initialise terminal width */
	(void)rl_initialize();
	rl_get_screen_size(&lines, &cols);

	/*
	 * set readline's signal handlers (also SIGINT), readline's handlers
	 * call ours set above
	 */
	(void)rl_set_signals();

	/*
	 * TODO get readline to determine tokens when ending quote
	 * is missing or when current position is not end of line
	 * i.e. find magic combination to get desired effect
	 */

	/* for our own completion and displaying, needed for our quoting */
	rl_attempted_completion_function = complete;
	rl_completion_display_matches_hook = displaymatches;
	rl_char_is_quoted_p = isquoted;
	rl_basic_word_break_characters = " \t";
	rl_basic_quote_characters = "";
	rl_completer_word_break_characters = " \t";
	rl_completer_quote_characters = "'";

	/* initialize history */
	using_history();

	while (!done)
		do_line();

	clear_history();
}


/* Reads and parses line, then executes command. */
static void
do_line(void)
{
	char **tokenv = NULL;
	int tokenc = 0;
	char *line;
	const char *prompt = "samblah> ";
	const char *errmsg;

	/* cleanup from previous command */
	int_signal = 0;

	/* read input, stop on EOF */
	if ((line = readline(prompt)) == NULL) {
		done = 1;
		return;
	}

	/* nothing to do with an empty line */
	if (strspn(line, " \t") == strlen(line))
		return;

	add_history(line);

	/* see if it is a shell command, ! */
	if (*line == '!') {
		(void)system(line + 1);
		free(line);
		return;
	}

	/* tokenize, escaping special characters */
	errmsg = tokenize_escape(line, &tokenc, &tokenv);
	if (errmsg != NULL) {
		cmdwarnx("%s", errmsg);
	} else {
		/* assume line is not empty after all */
		assert(tokenc != 0 && *tokenv != NULL);

		/* execute tokens */
		do_command(&tokenc, &tokenv);
	}

	freelist(tokenc, tokenv);
	free(line);
}


/* When no command could be found a message is printed. */
static void
do_command(int *tokencp, char ***tokenvp)
{
	int i;

	/* find struct command to execute */
	for (i = 0;; ++i) {
		if (i == cmdc) {
			cmdwarnx("no such command");
			return;
		}
		if (streql(**tokenvp, cmdv[i].name))
			break;
	}

	/* determine if our connected status is correct for this command */
	if (cmdv[i].conn == CMD_MUSTCONN && !connected) {
		cmdwarnx("not connected");
		return;
	}
	if (cmdv[i].conn == CMD_MUSTNOTCONN && connected) {
		cmdwarnx("already connected");
		return;
	}

	/*
	 * perform globbing, smbglob inserts tokens in vector and modifies
	 * count, last argument denotes if local or remote globbing should be
	 * done
	 */
	switch (smbglob(tokencp, tokenvp,
	    cmdv[i].conn == CMD_MUSTCONN && !streql(**tokenvp, "put"))) {
	case GLB_NOMEM:
		cmdwarn("globbing");
		return;
	case GLB_DIRERR:
		cmdwarn("handling directories in globbing");
		return;
	case GLB_INTR:
		return;
	}

	cmdname = cmdv[i].name;
	(*(cmdv[i].func))(*tokencp, *tokenvp);
	cmdname = "samblah";

	return;
}


/* Returns current width of the terminal. */
int
term_width(void)
{
	/* when terminal size has changed, calculate again */
	if (winch_signal) {
		rl_resize_terminal();
		rl_get_screen_size(&lines, &cols);
		winch_signal = 0;
	}
	return cols;
}


/*
 * Asks user for a password and write it to buf.  On success
 * true is returned, otherwise false.
 */
int
askpass(char pass[SMB_PASS_MAXLEN + 1])
{
	int c;
	struct termios tp;
	char buf[SMB_PASS_MAXLEN + 2];     /* needed for extra newline read */

	/* fill tp */
	if (tcgetattr(STDIN_FILENO, &tp) != 0)
		return 0;

	printf("password: ");
	fflush(stdout);

	/* make terminal not echo the password */
	tp.c_lflag &= ~ECHO;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tp) != 0)
		return 0;

	/* read password, make sure terminal echoes again on error */
	if (fgets(buf, sizeof buf, stdin) == NULL) {
		tp.c_lflag |= ECHO;
		(void)tcsetattr(STDIN_FILENO, TCSANOW, &tp);
		printf("\n");

		if (feof(stdin))
			clearerr(stdin);

		return 0;
	}

	/* enable echoing */
	tp.c_lflag |= ECHO;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tp) != 0)
		return 0;

	printf("\n");

	/* when line is too long, remove it and stop, success otherwise */
	if (strchr(buf, '\n') == NULL) {
		/* remove leftovers from stdin */
		while ((c = getchar()) != EOF && c != '\n')
			;       /* nothing */

		if (feof(stdin))
			clearerr(stdin);

		return 0;
	}

	*(strchr(buf, '\n')) = '\0';
	strcpy(pass, buf);

	return 1;
}


/* ARGSUSED */
static void
int_handler(int sig)
{
	int_signal = 1;
}


/* ARGSUSED */
static void
winch_handler(int sig)
{
	winch_signal = 1;
}
