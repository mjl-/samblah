/* $Id$ */

#include "samblah.h"


enum completion_type {
	CMP_NONE,       /* no completion */
	CMP_LOCAL,      /* local file completion */
	CMP_REMOTE      /* remote file completion */
};


static void     cmdmatch(const char *, int *, char ***);
static void     filematch(char *, int *, char ***, enum completion_type);
static enum completion_type     cmptype(const char *);


/*
 * NOTE: token is unused because we currently do the following
 * (because we cannot get readline to determine the token using the
 * quoting mechanism we want to use):
 * based on the currenty position of the cursor in the line we
 * determine to which token that position belongs and compare them
 * with `start' and `end', when it matches, readline did what we
 * wanted and we can complete the token, otherwise readline couldn't
 * get it right and we cannot tell readline which start/end it should
 * use, thus we cannot complete since readline would replace a wrong
 * part of the current line.
 */
/* ARGSUSED */
/*
 * Determines matches for token.  Alternative for
 * rl_attempted_completion_function, performs the command and file
 * completion.
 */
char **
complete(const char *token, int start, int end)
{
	char **tokenv = NULL;
	int tokenc = 0;
	char **matchv = NULL;
	int matchc = 0;
	char **newv;
	int tokeni;
	char *p1, *p2;
	int i;

	/* no fallback completion by readline */
	rl_attempted_completion_over = 1;

	/* parse command line so far, escaping special characters */
	if (tokenize_partial_escape(rl_line_buffer,
	    &tokenc, &tokenv, &tokeni, start, end) != NULL)
		return NULL;

	/* readline did not correctly detect token */
	if (tokeni == -1) {
		/*
		 * TODO get readline to always get token right
		fprintf(stderr, "\ntoken mismatch: '%s'\n", token);
		 */
		freelist(tokenc, tokenv);
		return NULL;
	}

	/* whether token is a command or a file */
	if (tokeni == 0)
		cmdmatch(tokenv[0], &matchc, &matchv);
	else
		filematch(tokenv[tokeni], &matchc,
		    &matchv, cmptype(tokenv[0]));

	/* no need for the tokens as we parsed the line */
	freelist(tokenc, tokenv);

	if (matchv == NULL || matchc == 0)
		return NULL;    /* no matches */

	/* sort so determining common leading string is easy */
	qsort(matchv, (size_t)matchc, sizeof (char *), qstrcmp);

	/* need space for leading element and trailing NULL */
	newv = realloc(matchv, sizeof (char *) * (matchc + 2));
	if (newv == NULL) {
		freelist(matchc, matchv);
		return NULL;
	}
	matchv = newv;

	/* shift elements away from begin, including trailing NULL */
	for (i = matchc; i >= 0; --i)
		matchv[i + 1] = matchv[i];
	++matchc;

	/* common leading string is that of first and last element */
	p1 = matchv[1];
	p2 = matchv[matchc - 1];
	while (*p1 != '\0' && *p2 != '\0' && *p1 == *p2)
		++p1, ++p2;

	/* add command leading string as first element */
	matchv[0] = malloc((size_t)(p1 - matchv[1] + 1));
	if (matchv[0] == NULL) {
		freelist(matchc, matchv);
		return NULL;
	}
	strncpy(matchv[0], matchv[1], (size_t)(p1 - matchv[1]));
	matchv[0][p1 - matchv[1]] = '\0';

	/* do not add space after a directory so we can go on completing */
	if (*matchv[0] != '\0' && *(strchr(matchv[0], '\0') - 1) == '/')
		rl_completion_append_character = '\0';
	else
		rl_completion_append_character = ' ';

	/* quote (escape characters too) part that will be inserted */
	if ((matchv[0] = quote_escape(matchv[0])) == NULL) {
		freelist(matchc, matchv);
		return NULL;
	}

	return matchv;
}


/*
 * Displays matches when completing (rl_completion_display_matches_hook),
 * uses readline display function, but strips of common leading
 * directory part.
 */
void
displaymatches(char **matchv, int matchc, int maxlen)
{
	char **newmatchv;
	char *p1, *p2;
	int i;
	int leadlen;
	char *lastslash;

	/* common leading string is that of first and last element */
	lastslash = NULL;
	p1 = matchv[1];
	p2 = matchv[matchc];
	while (*p1 != '\0' && *p2 != '\0' && *p1 == *p2) {
		if (*p1 == '/')
			lastslash = p1;
		++p1, ++p2;
	}

	/* when no leading directory part, just print matches */
	if (lastslash == NULL) {
		rl_display_match_list(matchv, matchc, maxlen);
		rl_on_new_line();
		return;
	}

	/* leading common directory part, and new maximum length */
	leadlen = lastslash - matchv[1] + 1;
	maxlen -= leadlen;

	/* new list with matches and leading substring */
	newmatchv = malloc(sizeof (char *) * (matchc + 1));
	if (newmatchv == NULL)
		return;
	newmatchv[0] = malloc((size_t)(p1 - (matchv[1] + leadlen) + 1));
	if (newmatchv[0] == NULL) {
		free(newmatchv);
		return;
	}
	strncpy(newmatchv[0], matchv[1] + leadlen,
	    (size_t)(p1 - (matchv[1] + leadlen)));
	newmatchv[0][p1 - (matchv[1] + leadlen)] = '\0';

	/* fill the list list */
	for (i = 1; i <= matchc; ++i)
		newmatchv[i] = matchv[i] + leadlen;
	
	/* let readline display the adjusted matches */
	rl_display_match_list(newmatchv, matchc, maxlen);

	free(newmatchv[0]);
	free(newmatchv);

	/* make readline print the prompt and what is on the line */
	rl_on_new_line();
}


/*
 * Generates matching commands for token.  Matches are put in argmatchv
 * (NULL terminated) and the number of elements in argmatchc.  On
 * error argmatchv will be NULL and argmatchc zero.
 */
static void
cmdmatch(const char *token, int *argmatchc, char ***argmatchv)
{
	size_t tokenlen;
	const char *cmd;
	char **matchv = NULL;
	int matchc = 0;
	char **newv;
	int i;

	/* no matches by default */
	*argmatchv = NULL;
	*argmatchv = 0;
	tokenlen = strlen(token);

	/* walk through commands and check for matches */
	for (i = 0; i < cmdc; ++i) {
		if (strncmp(token, cmd = cmdv[i].name, tokenlen) != 0)
			continue;

		/* space for new element and trailing NULL */
		newv = realloc(matchv, sizeof (char *) * (matchc + 2));
		if (newv == NULL) {
			freelist(matchc, matchv);
			return;
		}
		matchv = newv;

		/* make copy of command so readline can free it */
		if ((matchv[matchc++] = strdup(cmd)) == NULL) {
			freelist(matchc, matchv);
			return;
		}
	}

	/* when at least one match, add leading and trailing NULL */
	if (matchv != NULL)
		matchv[matchc] = NULL;

	*argmatchc = matchc;
	*argmatchv = matchv;
}


/*
 * Same as cmdmatch but token is the file to match.  It works like
 * cmdmatch.  Type denotes which completion to do.
 */
static void
filematch(char *token, int *argmatchc, char ***argmatchv,
    enum completion_type type)
{
	char **matchv = NULL;
	int matchc = 0;
	char **newv;
	char *ntoken;

	/* no matches by default */
	*argmatchv = NULL;
	*argmatchc = 0;

	if (type == CMP_NONE)
		return;

	/* add `*' at end of token so globbing can match things */
	ntoken = malloc(strlen(token) + 2);
	if (ntoken == NULL)
		return;
	strcpy(ntoken, token);
	strcat(ntoken, "*");

	/* do globbing on the token */
	if (tokenmatch("", ntoken, &matchc, &matchv,
	    (type == CMP_REMOTE) ? LOC_REMOTE : LOC_LOCAL) != GLB_OK) {
		free(ntoken);
		freelist(matchc, matchv);
		return;
	}
	free(ntoken);

	/* need space for trailing NULL */
	newv = realloc(matchv, sizeof (char *) * (matchc + 1));
	if (newv == NULL) {
		freelist(matchc, matchv);
		return;
	}
	matchv = newv;
	matchv[matchc] = NULL;

	*argmatchc = matchc;
	*argmatchv = matchv;
}


/* Determines which completion to do based on cmd. */
static enum completion_type
cmptype(const char *cmd)
{
	int i;

	for (i = 0; i < cmdc; ++i) {
		if (!streql(cmd, cmdv[i].name))
			continue;

		if (streql(cmd, "put"))
			return CMP_LOCAL;
		else if (cmdv[i].conn == CMD_MUSTCONN && !connected)
			return CMP_NONE;
		else if (cmdv[i].conn == CMD_MUSTCONN && connected)
			return CMP_REMOTE;
		return CMP_LOCAL;
	}
	return CMP_LOCAL;       /* unrecognized command */
}
