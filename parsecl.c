/* $Id$ */

#include "samblah.h"


static const char      *tokenize_generic(const char *, int *, char ***, int *, int, int);
static void     unquote(char *);
static char    *unquote_escape(char *);


/*
 * Tokenizes and unquotes (not escaping special characters) line,
 * tokens are stored in tokenvp and token count in tokencp.
 */
const char *
tokenize(const char *line, int *tokencp, char ***tokenvp)
{
	const char *errmsg;
	int i;

	/* tokenize, returned tokens are quoted */
	errmsg = tokenize_generic(line, tokencp, tokenvp, NULL, -1, -1);
	if (errmsg != NULL)
		return errmsg;

	/* unquote all tokens, not escaping special characters */
	for (i = 0; i < *tokencp; ++i)
		unquote((*tokenvp)[i]);

	return NULL;
}


/* Same as tokenize, but special characters (for fnmatch(3)) are escaped. */
const char *
tokenize_escape(const char *line, int *tokencp, char ***tokenvp)
{
	/* same as tokenize_partial_escape but without the partial */
	return tokenize_partial_escape(line, tokencp, tokenvp, NULL, -1, -1);
}


/*
 * Same as tokenize_escape, but when quotes are unbalanced the
 * ending quote is added.  When start and end are the start and end
 * of a token, the index in the token vector is written to tokenip.
 */
const char *
tokenize_partial_escape(const char *line, int *tokencp, char ***tokenvp,
    int *tokenip, int start, int end)
{
	int i;
	const char *errmsg;

	/* tokenize, returned tokens are quoted */
	errmsg = tokenize_generic(line, tokencp, tokenvp, tokenip, start, end);
	if (errmsg != NULL)
		return errmsg;

	/* unquote all tokens, escaping special characters */
	for (i = 0; i < *tokencp; ++i)
		if (((*tokenvp)[i] = unquote_escape((*tokenvp)[i])) == NULL) {
			freelist(*tokencp, *tokenvp);
			return "out of memory";
		}

	return NULL;
}



/* Removes escaping from special characters in token. */
void
unescape(char *token)
{
	/*
	 * look for backslash, remove it and skip escaped character
	 * so escaped backslashes are not removed
	 */
	for (; (token = strchr(token, '\\')) != NULL; ++token)
		memmove(token, token + 1, strlen(token + 1) + 1);
}


/* Quotes possibly escaped token, so it can be placed on the command line. */
char *
quote_escape(char *token)
{
	char *ntoken;
	char *cp;

	/* without special characters, we do not need te quote */
	if (strpbrk(token, " \t'*?[]") == NULL)
		return token;

	/* all quotes need an extra escaping one */
	cp = token;
	while ((cp = strchr(cp, '\'')) != NULL) {
		ntoken = realloc(token, strlen(token) + 2);
		if (ntoken == NULL) {
			free(token);
			return NULL;
		}
		/* ntoken != token is possible */
		cp = ntoken + (cp - token);
		token = ntoken;

		memmove(cp + 1, cp, strlen(cp) + 1);
		*cp = '\'';
		cp += 2;
	}

	/* allocate space for extra leading and trailing quote */
	ntoken = realloc(token, strlen(token) + 3);
	if (ntoken == NULL) {
		free(token);
		return NULL;
	}
	token = ntoken;

	/* add quote to begin and end of token */
	memmove(token + 1, token, strlen(token) + 1);
	*token = '\'';
	strcat(token, "'");

	return token;
}


/* Returns whether the character at index in line is quoted. */
int
isquoted(char *line, int index)
{
	int quoted = 0;
	char *bp, *cp;

	/* while there is another `opening' qoute */
	cp = line;
	while ((cp = bp = strchr(cp, '\'')) != NULL) {
		/* starting quote itself is not `quoted' */
		++bp, ++cp;

		/*
		 * not quoted when after last ending quote and before current
		 * beginning quote
		 */
		if (line + index < bp) {
			quoted = 0;
			break;
		}

		/* skip two successive quotes */
		while ((cp = strchr(cp, '\'')) != NULL && *(cp + 1) == '\'')
			cp += 2;

		/* quoted when index is between begin and end of quotement */
		if (cp == NULL || line + index < cp) {
			quoted = 1;
			break;
		}
		++cp;
	}

	/*
	 * TODO fix the quoting so it can be removed, for debugging only
	fprintf(stderr, "index=%d, char=%c %squoted\n", index, line[index],
	    quoted ? "" : "not ");
	 */

	return quoted;
}


/*
 * Same as tokenize_partial_escape but does not escape special characters
 * (for fnmatch(3)).
 */
static const char *
tokenize_generic(const char *constline, int *tokencp, char ***tokenvp,
    int *tokenip, int start, int end)
{
	char *line;
	char *bp, *cp, *tmp;
	char **tokenv = NULL;
	int tokenc = 0;
	char **newv;
	int stop = 0;
	int partial = !(start == -1 && end == -1);
	int quoteadd = 0;

	if (partial) {
		assert(tokenip != NULL);
		*tokenip = -1;
	}

	/* make copy of line to work with */
	line = malloc(strlen(constline) + 2);
	if (line == NULL)
		return "out of memory";
	strcpy(line, constline);

	/* skip leading white space */
	bp = cp = line + strspn(line, " \t");
	while (!stop) {
		/* find next token boundary */
		if ((tmp = strpbrk(cp, " \t'")) == NULL)
			tmp = strchr(cp, '\0');
		cp = tmp;

		/* action based on character at boundary */
		switch (*cp) {
		case '\'':
			/* find ending quote */

			++cp;

			/* skip quoted quotes */
			while ((cp = strchr(cp, '\'')) != NULL &&
			    *(cp + 1) == '\'')
				cp += 2;

			if (cp == NULL) {
				/* missing end quote */

				if (partial) {
					/* add quote and move to end of line */
					strcat(line, "'");
					cp = line + strlen(line);
					quoteadd = 1;
				} else {
					freelist(tokenc, tokenv);
					free(line);
					return "unbalanced quote";
				}
			} else
				++cp;   /* continue after end of quote */
			break;
		case '\0':
		case ' ':
		case '\t':
			/* end of token and possibly of line found */

			/*
			 * space for extra token, possible extra token for
			 * completion and trailing NULL
			 */
			newv = realloc(tokenv, sizeof (char *) * (tokenc + 3));
			if (newv == NULL) {
				freelist(tokenc, tokenv);
				free(line);
				return "out of memory";
			}
			tokenv = newv;

			/* add copy of token */
			tokenv[tokenc] = malloc((size_t)(cp - bp + 1));
			if (tokenv[tokenc] == NULL) {
				freelist(tokenc, tokenv);
				free(line);
				return "out of memory";
			}
			strncpy(tokenv[tokenc], bp, (size_t)(cp - bp));
			tokenv[tokenc][cp - bp] = '\0';
			++tokenc;

			/* check if the start/end denote a token */
			if (partial && bp - line == start &&
			    cp - line - (quoteadd ? 1 : 0) == end)
				*tokenip = tokenc - 1;

			cp = bp = cp + strspn(cp, " \t");

			/*
			 * add empty token when at end of line and being
			 * partial
			 */
			if (partial && cp != line && *cp == '\0' &&
			    start == end && cp - line == start) {
				tokenv[tokenc] = malloc(1);
				if (tokenv[tokenc] == NULL) {
					freelist(tokenc, tokenv);
					free(line);
					return "out of memory";
				}
				*tokenv[*tokenip = tokenc++] = '\0';
			}

			/* terminate list when at end of line */
			if (*cp == '\0') {
				tokenv[tokenc] = NULL;
				stop = 1;
			}
			break;
		}
	}

	free(line);

	*tokenvp = tokenv;
	*tokencp = tokenc;

	return NULL;
}


/* Removes quoting from token, special characters are not escaped. */
static void
unquote(char *token)
{
	char *cp = token;

	/* look for next opening quote */
	while ((cp = strchr(cp, '\'')) != NULL) {
		/* remove starting quote */
		memmove(cp, cp + 1, strlen(cp + 1) + 1);

		/* keep one of two successive quotes */
		while ((cp = strchr(cp, '\'')) != NULL && *(cp + 1) == '\'') {
			memmove(cp, cp + 1, strlen(cp + 1) + 1);
			++cp;
		}

		/* assume token has correct quotes */
		assert(cp != NULL);

		/* remove ending quote */
		memmove(cp, cp + 1, strlen(cp + 1) + 1);
	}
}


/*
 * Same as unquote, but special characters are quoted, value returned
 * is new token, the argument is freed, caller must free result.
 */
static char *
unquote_escape(char *token)
{
	char *tmp;
	char *cp = token;

	/* look for next opening quote */
	while ((cp = strchr(cp, '\'')) != NULL) {
		/* remove starting quote */
		memmove(cp, cp + 1, strlen(cp + 1) + 1);

		for (;;) {
			/* assuming token has at least ending quote */
			assert((cp = strpbrk(cp, "'\\?*[]")) != NULL);

			if (*cp == '\'' && *(cp + 1) == '\'') {
				/* keep one of two successive quotes */
				memmove(cp, cp + 1, strlen(cp + 1) + 1);
				++cp;
			} else if (*cp == '\'') {
				/* ending quote */
				break;
			} else {
				/* escape special character */

				/* reallocate for extra backslash */
				tmp = realloc(token, strlen(token) + 2);
				if (tmp  == NULL) {
					free(token);
					return NULL;
				}
				/* tmp != token is possible */
				cp = tmp + (cp - token);
				token = tmp;

				/* move elements to right, insert backslash */
				memmove(cp + 1, cp, strlen(cp) + 1);
				*cp = '\\';

				cp += 2;
			}
		}

		/* remove ending quote */
		memmove(cp, cp + 1, strlen(cp + 1) + 1);
	}

	return token;
}
