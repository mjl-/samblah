/* $Id$ */

#include "samblah.h"


/* Initializes argument.  Call pcol_freelist after using to clean up. */
void
col_initlist(struct collist *cl)
{
	cl->width = 0;
	cl->elemv = NULL;
	cl->elemc = 0;
}


/*
 * Adds elem to list.  On error false is returned and errno set, true is
 * returned on success.  col_freelist must eventually be called in both cases.
 */
int
col_addlistelem(struct collist *cl, const char *str)
{
	char **newv;

	if (strlen(str) > cl->width)
		cl->width = strlen(str);

	/* reallocate for adding element */
	newv = realloc(cl->elemv, sizeof (char *) * (cl->elemc + 1));
	if (newv == NULL) {
		col_freelist(cl);
		return 0;       /* errno is not changed by col_freelist */
	}
	cl->elemv = newv;

	/* make copy of string */
	cl->elemv[cl->elemc] = strdup(str);
	if (cl->elemv[cl->elemc] == NULL)
		return 0;       /* errno is not changed by col_freelist */
	++cl->elemc;

	return 1;
}


/* Prints contents of list to screen in columns. */
void
col_printlist(struct collist *cl)
{
	int termwidth;
	int colfit, rowcnt, colcnt;
	int i, j;

	/* do not print empty list */
	if (cl->elemc == 0 || cl->width <= 0)
		return;

	termwidth = term_width();

	colfit = div(termwidth, cl->width + 2).quot;
	if (colfit <= 1) {
		/* one column, get it over with */
		for (i = 0; i < cl->elemc; ++i)
			printf("%s\n", cl->elemv[i]);
		return;
	}

	rowcnt = (cl->elemc + colfit - (((cl->elemc % colfit) == 0)
	    ? colfit : (cl->elemc % colfit))) / colfit;

	/* there are situations where we could do without last columns */
	colcnt = (cl->elemc + rowcnt - ((cl->elemc % rowcnt) == 0
	    ? rowcnt : (cl->elemc % rowcnt))) / rowcnt;

	for (i = 0; i < rowcnt; ++i) {
		for (j = 0; j < colcnt && j * rowcnt + i < cl->elemc; ++j)
			printf("%-*s", cl->width + 2,
			    cl->elemv[j * rowcnt + i]);
		printf("\n");
	}
}


/* Frees memory allocated in list. */
void
col_freelist(struct collist *cl)
{
	while (cl->elemc--)
		free(cl->elemv[cl->elemc]);
	free(cl->elemv);
	col_initlist(cl);
}
