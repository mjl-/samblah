/* $Id$ */

#include <sys/types.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "egetopt.h"


char   *eoptarg;
int     eopterr = 1;    /* print warning by default */
int     eoptind = 1;    /* arguments start at index 1 in argv */
int     eoptopt;
int     eoptreset = 0;


int
egetopt(int argc, char * const *argv, const char *optstr)
{
	static ssize_t pos = -1;    /* position in current argument */
	char *arg;

	if (eoptreset) {
		eoptreset = 0;
		pos = -1;
	}

	if (eoptind >= argc)
		return -1;

	/* should not happen, would mean user changed argv[eoptind] */
	if (pos >= (ssize_t)strlen(argv[eoptind])) {
		++eoptind;
		pos = -1;
	}

	/* new token */
	if (pos == -1) {
		if (argv[eoptind][0] != '-' || argv[eoptind][1] == '\0')
			return -1;

		if (strcmp(argv[eoptind], "--") == 0) {
			++eoptind;
			return -1;
		}

		/* token has options, use first */
		pos = 1;
	}

	arg = argv[eoptind] + pos;
	eoptopt = *arg;

	/* illegal option (by standard or by optstr) */
	if (!isalnum(*arg) || strchr(optstr, *arg) == NULL) {
		if (eopterr && *optstr != ':')
			fprintf(stderr, "%s: illegal option -- %c\n",
			    argv[0], *arg);
		return '?';
	}

	if (*(strchr(optstr, *arg) + 1) == ':') {
		/* needs an argument */

		/* argument appended to option */
		if (*(arg + 1) != '\0') {
			eoptarg = arg + 1;
			++eoptind;
			pos = -1;
			return *arg;
		}

		/* missing argument */
		if (argv[eoptind + 1] == NULL) {
			if (eopterr && *optstr != ':')
				fprintf(stderr,
				    "%s: option requires an argument -- %c\n",
				    argv[0], *arg);
			++eoptind;
			pos = -1;
			return (*optstr == ':') ? ':' : '?';
		}

		/* argument is next token */
		eoptarg = argv[eoptind + 1];
		eoptind += 2;
		pos = -1;
		return *arg;
	} else {
		/* does not need an argument */

		if (*(arg + 1) == '\0') {
			pos = -1;
			++eoptind;
		} else
			++pos;
		return *arg;
	}
}
