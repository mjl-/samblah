/* $Id$ */

#include "samblah.h"

static const char      *tokenize_generic(const char *, List *, int *, int, int);


/*
 * Tokenizes line, tokens are stored in tokens.  Characters special to
 * fnmatch(3) are not escaped.  On succes NULL is returned, otherwise an error
 * message is returned.
 */
const char *
tokenize(const char *line, List *tokens)
{
	int	i;
	const char *errmsg;

	/* tokenize_generic returns escaped tokens */
	errmsg = tokenize_generic(line, tokens, NULL, -1, -1);
	if (errmsg != NULL)
		return errmsg;

	/* unescape all tokens */
	for (i = 0; i < list_count(tokens); ++i)
		unescape((char *)list_elem(tokens, i));

	return NULL;
}


/*
 * Same as tokenize, but special characters (for fnmatch(3)) are escaped.
 */
const char *
tokenize_escape(const char *line, List *tokens)
{
	return tokenize_generic(line, tokens, NULL, -1, -1);
}


/*
 * Same as tokenize_escape, but when quotes are unbalanced the
 * ending quote is added.  When start and end are the start and end
 * of a token, the index in the token vector is written to tokenip.
 */
const char *
tokenize_partial_escape(const char *line, List *tokens, int *tokenip, int start, int end)
{
	return tokenize_generic(line, tokens, tokenip, start, end);
}


/*
 * Removes escaping from special characters in token.
 */
void
unescape(char *token)
{
	int	from;
	int	to;

	from = 0;
	to = 0;
	while (token[from] != '\0') {
		/* remove backslash from escaped character */
		if (token[from] == '\\')
			++from;
		token[to++] = token[from++];
	}
	token[to] = '\0';
}


/*
 * Quotes token if it contains whitespace or special characters.  It frees the
 * original token and allocates a new one.
 */
char *
quote(char *token)
{
	Str    *dest;
	int	i;
	char   *result;

	/* without special characters, we do not need te quote */
	if (strpbrk(token, " \t'*?[]") == NULL)
		return token;

	dest = str_new("");
	str_putchar(dest, '\'');
	for (i = 0; token[i] != '\0'; ++i) {
		if (token[i] == '\'')
			str_putchar(dest, '\'');
		str_putchar(dest, token[i]);
	}
	str_putchar(dest, '\'');

	result = str_charptr(dest);
	free(dest);
	return result;
}


/*
 * Called by readline to determine of the character at index is quoted or not.
 */
int
isquoted(char *line, int index)
{
	int	i;
	int	inquote;

	inquote = 0;
	i = 0;
	while (line[i] != '\0') {
		if (line[i] == '\'') {
			if (inquote && line[i + 1] == '\'') {
				if (index == i || index == i + 1)
					return 1;
				i += 2;
			} else {
				/* starting or ending quote */
				inquote = !inquote;
				if (index == i)
					return 0;
				++i;
			}
		} else {
			if (index == i)
				return inquote;
			++i;
		}
	}
	return 0;
}


/*
 * Tokenizes line, the result is put in tokens.  When `start' and `end' are not
 * -1, this function checks of they are the start and end indices of a token.
 * Special characters inside quotes are escaped.
 * On succes, NULL is returned, otherwise an error message is returned.
 */
static const char *
tokenize_generic(const char *line, List *tokens, int *tokenip,
			int start, int end)
{
	int	i;
	Str    *token;
	int	inquote, intoken;
	int	partial;
	int	tokenstart, tokenend;;

	partial = (start != -1 && end != -1);
	if (partial)
		*tokenip = -1;

	token = str_new("");
	i = strspn(line, " \t");
	tokenstart = i;
	intoken = 1;
	inquote = 0;
	while (line[i] != '\0') {
		switch (line[i]) {
		case '\'':
			if (!intoken)
				tokenstart = i;
			intoken = 1;

			if (inquote && line[i + 1] == '\'') {
				str_putchar(token, '\'');
				i += 2;
			} else {
				inquote = !inquote;
				++i;
			}
			break;
		case ' ':
		case '\t':
			if (!intoken) {
				/* whitespace between tokens, skip it */
				++i;
				break;
			}

			/* whitespace inside quoted token or token boundary */
			if (inquote) {
				str_putchar(token, line[i]);
				++i;
			} else {
				tokenend = i;
				list_add(tokens, str_charptr(token));
				free(token);
				token = str_new("");
				intoken = 0;
				++i;
			}
			break;
		default:
			if (!intoken)
				tokenstart = i;
			intoken = 1;

			if (inquote && strchr("\\?*[]", line[i]) != NULL)
				str_putchar(token, '\\');
			str_putchar(token, line[i]);
			++i;
		}
	}

	/* it is likely we were processing a token when we encountered the NUL */
	if (intoken) {
		if (inquote && !partial) {
			return "unbalanced quotes";
		} else {
			/* the ending quote may be missing, pretend it is there */
			tokenend = i;
			list_add(tokens, str_charptr(token));
			free(token); token = NULL;
		}
	} else {
		str_free(token); token = NULL;
	}

	/* check if the start/end denote a token */
	if (partial && tokenstart == start && tokenend == end)
		*tokenip = list_count(tokens) - 1;

	/*
	 * add empty token when at end of line and being partial.
	 * when start == 0, the case above would have added an empty token.
	 */
	if (partial && start == end && start != 0) {
		list_add(tokens, xstrdup(""));
		*tokenip = list_count(tokens) - 1;
	}

	return NULL;
}
