/* $Id$ */

#include "samblah.h"

enum completion_type {
	CMP_NONE,       /* no completion */
	CMP_LOCAL,      /* local file completion */
	CMP_REMOTE      /* remote file completion */
};


static char    *commonlead(const char *, const char *);
static List    *cmdmatch(const char *);
static List    *filematch(const char *, enum completion_type);
static enum completion_type     cmptype(const char *);


/*
 * Complete() is called by readline to retrieve a list of completion for the
 * given token, which starts at start and ends in end in readlines linebuffer.
 * Our case is a bit complicated because samblah's quoting mechanism confuses
 * readline, which sometimes causes it to get the token wrong (e.g. what it
 * thinks is a token is actually more than a token or only the half of a
 * token).  Therefore this function checks if readline got the start and end
 * right.  If not, no matches are generated.  If it got it right, the matches
 * are generated and returned.
 * The returned matches and the vector in which it is contained is freed by
 * readline.
 */
char **
complete(const char *token, int start, int end)
{
	List   *tokens;
	List   *matches;
	int	tokenindex;
	char   *first, *last;
	char   *newtoken;
	char   *token0;

	tokens = list_new();

	/* no fallback completion by readline */
	rl_attempted_completion_over = 1;

	/* parse command line so far, escaping special characters */
	if (tokenize_partial_escape(rl_line_buffer, tokens, &tokenindex, start, end) != NULL) {
		list_free(tokens); tokens = NULL;
		return NULL;
	}

	/* readline did not correctly detect token */
	if (tokenindex == -1) {
		/*
		 * TODO get readline to always get token right
		fprintf(stderr, "\ntoken mismatch: '%s'\n", token);
		 */
		list_free(tokens); tokens = NULL;
		return NULL;
	}

	token0 = (char *)list_elem(tokens, 0);
	if (tokenindex == 0)
		matches = cmdmatch(token0);
	else
		matches = filematch((char *)list_elem(tokens, tokenindex), cmptype(token0));

	/* no need for the tokens as we parsed the line */
	list_free(tokens); tokens = NULL;

	if (list_count(matches) == 0) {
		list_free(matches); matches = NULL;
		return NULL;
	}

	/*
	 * after sorting, the common leading string of the first and last
	 * elements are the common leading string of all elements
	 */
	list_sort(matches, qstrcmp);

	first = (char *)list_elem(matches, 0);
	last = (char *)list_elem(matches, list_count(matches) - 1);
	newtoken = commonlead(first, last);

	/* do not add space after a directory so we can go on completing */
	if (newtoken[strlen(newtoken) - 1] == '/')
		rl_completion_append_character = '\0';
	else
		rl_completion_append_character = ' ';

	list_prepend(matches, quote(newtoken));

	return (char **)list_elems_freerest(matches);
}


/*
 * Display the matches generated by complete() (see above).
 * matchv	matchv[0] is the common leading substring of all elements in
 *		matchv.  matchv[0] may be quoted, all other elements in matchv
 *		are never quoted.  the first match generated by complete() is
 *		in matchv[1], the last element is in matchv[matchc].
 * matchc	number of elements in matchv.  the elements start at index 1.
 * maxlen	length of the largest element
 *
 * displaymatches() strips the common leading directories from the matches to
 * make the matches to be printed shorter.
 * contents of matchv are unchanged after return.
 */
void
displaymatches(char **matchv, int matchc, int maxlen)
{
	int	i, leadlen;
	char   *lastslash;
	char   *common;

	/*
	 * up to the last slash of matchv[0] is the common leading directory
	 * part
	 */
	common = matchv[0];
	lastslash = strrchr(common, '/');

	/*
	 * determine leadlen, the number of characters to skip to remove the
	 * common leading path
	 */
	leadlen = 0;
	if (lastslash != NULL) {
		/*
		 * determine how many characters to remove to remove the common
		 * leading directory parts.  matchv[0] may be quoted, then its
		 * first character is a quote.  so remove one character less on
		 * the rest of matchv (since the rest is never quoted).  maxlen
		 * also changes.
		 */
		leadlen = lastslash - common + 1;
		if (common[0] == '\'')
			leadlen -= 1;
		maxlen -= leadlen;
	}

	/* display the possibly shorter matches */
	for (i = 1; i <= matchc; ++i)
		matchv[i] += leadlen;
	rl_display_match_list(matchv, matchc, maxlen);
	for (i = 1; i <= matchc; ++i)
		matchv[i] -= leadlen;

	/* make readline print the prompt and current line buffer contents */
	rl_on_new_line();
}


/*
 * Allocate and return the common leading string of first and last.
 */
static char *
commonlead(const char *first, const char *last)
{
	int	i;
	char   *newtoken;

	i = 0;
	while (first[i] != '\0' && last[i] != '\0' && first[i] == last[i])
		++i;

	newtoken = xmalloc(i + 1);
	memmove(newtoken, first, i);
	newtoken[i] = '\0';

	return newtoken;
}

/*
 * Generates commands starting with token and returns them as a List.
 * The returned List is never NULL but may contain zero elements.
 */
static List *
cmdmatch(const char *token)
{
	List   *tokens;
	size_t	tokenlen;
	int	i;
	Cmd    *cmd;

	/* walk through commands and check for matches */
	tokens = list_new();
	tokenlen = strlen(token);
	for (i = 0; i < commandcount; ++i) {
		cmd = &commands[i];
		if (strncmp(token, cmd->name, tokenlen) == 0) {
			/* make copy so that readline can free it */
			list_add(tokens, xstrdup(cmd->name));
		}
	}
	return tokens;
}


/*
 * Generate matches for the file starting with token.  Type denotes if the file
 * is remote or local.
 */
static List *
filematch(const char *token, enum completion_type type)
{
	Str    *pattern;
	List   *tokens;
	int	remoteglobbing;

	tokens = list_new();

	if (type == CMP_NONE)
		return tokens;

	/* add `*' at end of token so there is something to match */
	pattern = str_new(token);
	str_putcharptr(pattern, "*");

	remoteglobbing = (type == CMP_REMOTE);
	if (tokenmatch(str_charptr(pattern), tokens, remoteglobbing) != GLB_OK) {
		list_free(tokens);
		tokens = list_new();
	}
	str_free(pattern); pattern = NULL;

	return tokens;
}


/*
 * Determines which completion to do based on command.
 */
static enum completion_type
cmptype(const char *name)
{
	int	i;
	Cmd    *cmd;

	for (i = 0; i < commandcount; ++i) {
		cmd = &commands[i];

		if (!streql(name, cmd->name))
			continue;

		if (streql(name, "put"))
			return CMP_LOCAL;
		else if (cmd->conn == CMD_MUSTCONN && !connected)
			return CMP_NONE;
		else if (cmd->conn == CMD_MUSTCONN && connected)
			return CMP_REMOTE;
		return CMP_LOCAL;
	}
	return CMP_LOCAL;       /* unrecognized command */
}
