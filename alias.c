/* $Id$ */

#include "samblah.h"


static struct alias    *aliasv = NULL;
static int              aliasc = 0;


/*
 * Sets an alias, creates a new alias if it does not exist.  Only alias,
 * host and share must be non-NULL.
 */
const char *
setalias(const char *alias, const char *user, const char *pass,
    const char *host, const char *share, const char *path)
{
	void *tmp;
	struct alias *aliasp;

	/*
	 * any alias is allowed as long as it isn't too long, validconn
	 * checks if arguments aren't too long and for invalid characters
	 */
	if (strlen(alias) > ALIAS_MAXLEN ||
	    !validconn(user, pass, host, share, path))
		return "invalid alias";

	/* check if alias with this name already exists */
	if (getalias(alias) != NULL)
		return "alias already exists";

	/* reallocate alias vector for new element and trailing NULL */
	tmp = realloc(aliasv, (aliasc + 2) * sizeof (struct alias));
	if (tmp == NULL)
		return "out of memory";
	aliasv = tmp;

	/* empty strings for user and pass will be dealt with in smbwrap */
	aliasp = &aliasv[aliasc++];

	strcpy(aliasp->alias, alias);
	strcpy(aliasp->user, (user != NULL) ? user : "");
	strcpy(aliasp->pass, (pass != NULL) ? pass : "");
	strcpy(aliasp->host, host);
	strcpy(aliasp->share, share);
	strcpy(aliasp->path, (path != NULL) ? path : "/");

	return NULL;
}


/* Finds an alias by name, returns NULL when no such alias exists. */
struct alias *
getalias(const char *name)
{
	int i;

	/* find matching name for alias and return pointer to that alias */
	for (i = 0; i < aliasc; ++i)
		if (streql(name, aliasv[i].alias))
			return &aliasv[i];
	return NULL;
}
