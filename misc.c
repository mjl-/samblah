/* $Id$ */

#include "samblah.h"


void
freelist(int argc, char **argv)
{
	if (argv == NULL)
		return;

	while (argc != 0)
		free(argv[--argc]);
	free(argv);
}


char *
strdup(const char *str)
{
	char *new;

	new = malloc(strlen(str) + 1);
	if (new == NULL)
		return NULL;

	strcpy(new, str);
	return new;
}


int
qstrcmp(const void *s1, const void *s2)
{
	return strcmp(*((char * const *)s1), *((char * const *)s2));
}


/* PRINTFLIKE3 */
int
xsnprintf(char *buf, size_t buflen, const char *fmt, ...)
{
	int ret;
	va_list ap;

	/* LINTED [pointer casts may be troublesome] */
	va_start(ap, fmt);
	assert((ret = vsnprintf(buf, buflen, fmt, ap)) >= 0);
	/* LINTED [expression has null effect] */
	va_end(ap);

	return ret;
}


/*
 * Returns string representation of date.  String returned is a
 * static buffer and must not be freed.
 */
const char *
makedatestr(time_t mtime)
{
	static char buf[13];
	struct tm *tmmtime;

	tmmtime = localtime(&mtime);
	assert(strftime(buf, sizeof buf,
	    (time(NULL) - mtime > 365 * 24 * 3600) ?
	    "%b %d  %Y" : "%b %d %H:%M", tmmtime) != 0);

	return buf;
}



/*
 * Some <err.h>-functions from BSD.
 * The functions print a printf-like format (the `const char *,
 * ...') to stderr, and the err and errx exit(3) with the error code
 * given (first argument).  Functions not ending with `x' also print
 * the message for the current value of errno using strerror().  The
 * `const char *' formats should not end with a newline.  Functions
 * ending with `v' work are like the printf `v'-serie.
 */

void
err(int ret, const char *fmt, ...)
{
	va_list ap;

	/* LINTED [lint: pointer casts may be troublesome] */
	va_start(ap, fmt);
	fprintf(stderr, "samblah: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", strerror(errno));
	/* LINTED [lint: expression has null effect] */
	va_end(ap);

	exit(ret);
}


void
errx(int ret, const char *fmt, ...)
{
	va_list ap;

	/* LINTED [lint: pointer casts may be troublesome] */
	va_start(ap, fmt);
	fprintf(stderr, "samblah: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	/* LINTED [lint: expression has null effect] */
	va_end(ap);

	exit(ret);
}


void
warn(const char *fmt, ...)
{
	va_list ap;

	/* LINTED [lint: pointer casts may be troublesome] */
	va_start(ap, fmt);
	vwarn(fmt, ap);
	/* LINTED [lint: expression has null effect] */
	va_end(ap);

}


void
warnx(const char *fmt, ...)
{
	va_list ap;

	/* LINTED [lint: pointer casts may be troublesome] */
	va_start(ap, fmt);
	vwarnx(fmt, ap);
	/* LINTED [lint: expression has null effect] */
	va_end(ap);
}


void
vwarn(const char *fmt, va_list ap)
{
	fprintf(stderr, "samblah: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", strerror(errno));
}


void
vwarnx(const char *fmt, va_list ap)
{
	fprintf(stderr, "samblah: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}


/*
 * Cmdwarn and cmdwarnx are like warn and warnx from <err.h> but
 * to be used by the internal/interactive commands.  Global variable
 * `cmdname' is used for printing the command name.
 */

/* PRINTFLIKE1 */
void
cmdwarn(const char *fmt, ...)
{
	va_list ap;

	/* LINTED [lint: pointer casts may be troublesome] */
	va_start(ap, fmt);
	printf("%s: ", cmdname);
	vprintf(fmt, ap);
	printf(": %s\n", strerror(errno));
	/* LINTED [lint: expression has null effect] */
	va_end(ap);
}


/* PRINTFLIKE1 */
void
cmdwarnx(const char *fmt, ...)
{
	va_list ap;

	/* LINTED [lint: pointer casts may be troublesome] */
	va_start(ap, fmt);
	printf("%s: ", cmdname);
	vprintf(fmt, ap);
	printf("\n");
	/* LINTED [lint: expression has null effect] */
	va_end(ap);
}
