/* $Id$ */

#include "samblah.h"


const char *variablenamev[] = {
	"onexist",
	"pager",
	"showprogress"
};
int variablenamec = sizeof variablenamev / sizeof (char *);


enum    { VARTYPE_BOOLEAN, VARTYPE_ONEXIST, VARTYPE_STRING };
struct variable {
	const char     *name;
	int             type;
	void           *value;
};


/* Variables and their default values. */
static int      onexist = VAR_ASK;
static int      showprogress = 1;
static char     pager[VAR_STRING_MAXLEN + 1] = DEFAULT_PAGER;

/* All information about the variables. */
static struct variable variablev[] = {
    { "onexist", VARTYPE_ONEXIST, &onexist },
    { "pager", VARTYPE_STRING, pager },
    { "showprogress", VARTYPE_BOOLEAN, &showprogress }
};
static int variablec = sizeof variablev / sizeof (struct variable);


static struct variable *findvariable(const char *);


/* Sets name to value, value remains unchanged on error.  */
const char *
setvariable(const char *name, const char *valuestr)
{
	struct variable *p;
	void *value;

	p = findvariable(name);
	if (p == NULL)
		return "no such variable";
	value = p->value;

	/* types are handled differently */
	switch (p->type) {
	case VARTYPE_BOOLEAN:
		if (streql(valuestr, "yes"))
			*((int *)value) = 1;
		else if (streql(valuestr, "no"))
			*((int *)value) = 0;
		else
			return "invalid value, must be yes or no";
		return NULL;
	case VARTYPE_ONEXIST:
		if (streql(valuestr, "ask"))
			*((int *)value) = VAR_ASK;
		else if (streql(valuestr, "resume"))
			*((int *)value) = VAR_RESUME;
		else if (streql(valuestr, "overwrite"))
			*((int *)value) = VAR_OVERWRITE;
		else if (streql(valuestr, "skip"))
			*((int *)value) = VAR_SKIP;
		else
			return "invalid value, must be ask, resume,"
			    "overwrite or skip";
		return NULL;
	case VARTYPE_STRING:
		if (strlen(valuestr) > VAR_STRING_MAXLEN)
			return "value too long";
		strcpy((char *)value, valuestr);
		return NULL;
	}
	warnx("unknown variable type in setvariable");
	abort();
	/* NOTREACHED */
}


/*
 * Same as getvariable_{boolean,onexist,string}, but variable and value as
 * string representation.  When variable does not exist, NULL is returned.
 */
char *
getvariable(const char *name)
{
	struct variable *p;
	void *value;
	int value_int;

	/* find variable info */
	p = findvariable(name);
	if (p == NULL)
		return NULL;
	value = p->value;

	/* types are handled differently */
	switch (p->type) {
	case VARTYPE_BOOLEAN:           return *(int *)value ? "yes" : "no";
	case VARTYPE_STRING:            return (char *)p->value;
	case VARTYPE_ONEXIST:
		value_int = *(int *)value;

		assert(value_int == VAR_ASK || value_int == VAR_RESUME ||
		    value_int == VAR_OVERWRITE || value_int == VAR_SKIP);

		switch (value_int) {
		case VAR_ASK:	        return "ask";
		case VAR_RESUME:        return "resume";
		case VAR_OVERWRITE:     return "overwrite";
		case VAR_SKIP:          return "skip";
		default:
			warnx("unknown value for `onexist' in getvariable");
			abort();
		}
	}
	warnx("unknown variable type in getvariable");
	abort();
	/* NOTREACHED */
}


int
getvariable_bool(const char *name)
{
	struct variable *p;

	p = findvariable(name);
	assert(p != NULL && p->type == VARTYPE_BOOLEAN);

	return *(int *)p->value;
}


int
getvariable_onexist(const char *name)
{
	struct variable *p;

	p = findvariable(name);
	assert(p != NULL && p->type == VARTYPE_ONEXIST);

	return *(int *)p->value;
}


char *
getvariable_string(const char *name)
{
	struct variable *p;

	p = findvariable(name);
	assert(p != NULL && p->type == VARTYPE_STRING);

	return (char *)p->value;
}


static struct variable *
findvariable(const char *name)
{
	int i;

	/* find matching variable info and return pointer to it */
	for (i = 0; i < variablec; ++i)
		if (streql(name, variablev[i].name))
			return &variablev[i];
	return NULL;
}
