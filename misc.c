/* $Id$ */

#include "samblah.h"

/*
 * Prints contents of list to screen in columns.
 */
void
printcolumns(List *list)
{
	int	maxwidth;
	int	termwidth;
	int	colfit, rowcnt, colcnt;
	int	i, j;
	int	elemcount;
	char   *elem;

	elemcount = list_count(list);

	maxwidth = 0;
	for (i = 0; i < elemcount; ++i) {
		elem = (char *)list_elem(list, i);
		if (strlen(elem) > maxwidth)
			maxwidth = strlen(elem);
	}

	/* do not print empty list */
	if (elemcount == 0 || maxwidth <= 0)
		return;

	termwidth = term_width();

	colfit = div(termwidth, maxwidth + 2).quot;
	if (colfit <= 1) {
		/* one column, get it over with */
		for (i = 0; i < elemcount; ++i)
			printf("%s\n", (char *)list_elem(list, i));
		return;
	}

	rowcnt = (elemcount + colfit - (((elemcount % colfit) == 0)
	    ? colfit : (elemcount % colfit))) / colfit;

	/* there are situations where we could do without last columns */
	colcnt = (elemcount + rowcnt - (((elemcount % rowcnt) == 0)
	    ? rowcnt : (elemcount % rowcnt))) / rowcnt;

	for (i = 0; i < rowcnt; ++i) {
		for (j = 0; j < colcnt && j * rowcnt + i < elemcount; ++j)
			printf("%-*s", maxwidth + 2, (char *)list_elem(list, j * rowcnt + i));
		printf("\n");
	}
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
	int	ret;
	va_list	ap;

	/* LINTED [pointer casts may be troublesome] */
	va_start(ap, fmt);
	assert((ret = vsnprintf(buf, buflen, fmt, ap)) >= 0);
	/* LINTED [expression has null effect] */
	va_end(ap);

	return ret;
}


/*
 * Returns string representation of date.  String returned is a static buffer
 * and must not be freed.
 */
const char *
makedatestr(time_t mtime)
{
	static char buf[13];
	struct tm  *tmmtime;
	char   *fmt;

	tmmtime = localtime(&mtime);
	if (time(NULL)-mtime > 365*24*3600)
		fmt = "%b %d  %Y";
	else
		fmt = "%b %d %H:%M";

	if (strftime(buf, sizeof buf, fmt, tmmtime) == 0)
		return "strftime";

	return buf;
}


void *
xmalloc(size_t size)
{
	void   *p;

	p = malloc(size);
	if (p == NULL)
		err(2, "xmalloc");
	return p;
}

void *
xrealloc(void *p, size_t size)
{
	p = realloc(p, size);
	if (p == NULL)
		err(2, "xrealloc");
	return p;
}

char *
xstrdup(const char *s)
{
	char   *news;

	news = xmalloc(strlen(s) + 1);
	strcpy(news, s);

	return news;
}


/*
 * Some <err.h>-functions from BSD.
 * These functions print a printf-like format to stderr, and the err and errx
 * exit(3) with the error code given (first argument).  Functions not ending
 * with `x' also print the message for the current value of errno using
 * strerror().  The formats need not end with a newline, a newline is always
 * printed.  Functions ending with `v' work are like the printf `v'-series.
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
