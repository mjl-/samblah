/* $Id$ */

#include "samblah.h"

/* variables and their default values */
static int      onexist = VAR_ASK;
static int      showprogress = 1;
static char     pager[VAR_STRING_MAXLEN + 1] = DEFAULT_PAGER;

const char **
listvariables(void)
{
	static const char *variables[] = { "onexist", "pager", "showprogress", NULL };

	return variables;
}

/*
 * Sets the value of variable `name' to the given value.
 * On error, an error string is returned.  Otherwise, NULL is returned.
 */
const char *
setvariable(const char *name, const char *valuestr)
{
	if (streql(name, "onexist")) {
		if (streql(valuestr, "ask"))
			onexist = VAR_ASK;
		else if (streql(valuestr, "resume"))
			onexist = VAR_RESUME;
		else if (streql(valuestr, "overwrite"))
			onexist = VAR_OVERWRITE;
		else if (streql(valuestr, "skip"))
			onexist = VAR_SKIP;
		else
			return "invalid value, must be ask, resume, "
			    "overwrite or skip";
		return NULL;
	} else if (streql(name, "pager")) {
		if (strlen(valuestr) + 1 > sizeof pager)
			return "value too long";
		strcpy(pager, valuestr);
		return NULL;
	} else if (streql(name, "showprogress")) {
		if (streql(valuestr, "yes"))
			showprogress = 1;
		else if (streql(valuestr, "no"))
			showprogress = 0;
		else
			return "invalid value, must be yes or no";
		return NULL;
	} else {
		return "unknown variable";
	}
}

/*
 * Same as getvariable_{boolean,onexist,string}, but variable and value as
 * string representation.  When variable does not exist, NULL is returned.
 */
char *
getvariable(const char *name)
{
	if (streql(name, "onexist")) {
		switch (onexist) {
		case VAR_ASK:	        return "ask";
		case VAR_RESUME:        return "resume";
		case VAR_OVERWRITE:     return "overwrite";
		case VAR_SKIP:          return "skip";
		default:		return NULL;	/* should not happen */
		}
	} else if (streql(name, "pager")) {
		return pager;
	} else if (streql(name, "showprogress")) {
		return showprogress ? "yes" : "no";
	} else {
		return NULL;
	}
}

int
getvariable_bool(const char *name)
{
	assert(streql(name, "showprogress"));
	return showprogress;
}

int
getvariable_onexist(const char *name)
{
	assert(streql(name, "onexist"));
	return onexist;
}

char *
getvariable_string(const char *name)
{
	assert(streql(name, "pager"));
	return pager;
}
