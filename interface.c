/* $Id$ */

#include "samblah.h"

int	done = 0;			/* when true, samblah will quit */
const char *cmdname = "samblah";	/* contains running command */
volatile sig_atomic_t int_signal = 0;	/* set to true on SIGINT */
volatile sig_atomic_t winch_signal = 0;	/* set to true on SIGWINCH */


static int cols = -1;   /* width of screen */
static int lines;       /* unused, needed for setting cols */


static void     do_line(void);
static void     do_command(List *);
static void     int_handler(int);
static void     winch_handler(int);


/*
 * Starts the command reading and executing loop.  When `quit' is executed or
 * on EOF it returns (variable `done' will be true).  On error it prints a
 * message and exits.
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
	 * TODO get readline to determine tokens when ending quote is missing
	 * or when current position is not end of line i.e. find magic
	 * combination to get desired effect
	 */

	/*
	 * for our own completion and displaying, needed for our quoting.
	 * these are defined in complete.c
	 */
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
	char   *line;
	List   *tokens;
	const char *prompt = "samblah> ";
	const char *errmsg;

	/* possibly cleanup from previous command */
	int_signal = 0;

	/* read input, stop on EOF */
	line = readline(prompt);
	if (line == NULL) {
		done = 1;
		return;
	}

	/* nothing to do with an empty line */
	if (strspn(line, " \t") == strlen(line))
		return;

	/*
	 * at least the line is non-empty, it may still have an invalid command
	 * or incorrect quoting
	 */
	add_history(line);

	/* see if it is a shell command, i.e. when it starts with `!' */
	if (line[0] == '!') {
		(void)system(&line[1]);
		free(line);
		return;
	}

	/*
	 * tokenize, characters special to fnmatch(3) are escaped.  these
	 * tokens aren't ready yet to give to the internal commands,
	 * do_command() will call smbglob() which will replace patterns with
	 * matches.
	 */
	tokens = list_new();
	errmsg = tokenize_escape(line, tokens);
	if (errmsg != NULL) {
		cmdwarnx("%s", errmsg);
	} else {
		/* assume line is not empty after all (yes, we already checked it wasn't) */
		assert(list_count(tokens) != 0);

		/* execute tokens */
		do_command(tokens);
	}

	list_free(tokens); tokens = NULL;
	free(line); line = NULL;
}


/* When no command could be found a message is printed. */
static void
do_command(List *tokens)
{
	int	i;
	int	remoteglobbing;
	Cmd    *cmd;
	int	argc;
	char  **argv;

	/* find struct command to execute */
	i = 0;
	for (i = 0; i < commandcount; ++i) {
		cmd = &commands[i];
		if (streql((char *)list_elem(tokens, 0), cmd->name))
			break;
	}
	if (i == commandcount) {
		cmdwarnx("no such command");
		return;
	}

	/* determine if our connected status is correct for this command */
	if (cmd->conn == CMD_MUSTCONN && !connected) {
		cmdwarnx("not connected");
		return;
	}
	if (cmd->conn == CMD_MUSTNOTCONN && connected) {
		cmdwarnx("already connected");
		return;
	}

	/* perform globbing, either on remote or on local files */
	remoteglobbing = cmd->conn == CMD_MUSTCONN && !streql((char *)list_elem(tokens, 0), "put");
	switch (smbglob(tokens, remoteglobbing)) {
	case GLB_DIRERR:
		cmdwarn("handling directories in globbing");
		return;
	case GLB_INTR:
		/* return silently, the user deliberately caused the interrupt */
		return;
	}

	argc = list_count(tokens);
	argv = (char **)list_elems(tokens);

	/* cmdname is used by the warning/error printing functions used by the commands */
	cmdname = cmd->name;
	(*(cmd->func))(argc, argv);
	cmdname = "samblah";

	return;
}


/*
 * Return the current width of the terminal.
 */
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
	int	c;
	char	buf[SMB_PASS_MAXLEN + 2];     /* +2 because an extra newline is read */
	struct termios tp;

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
