/* $Id$ */

#include "samblah.h"

static List    *aliases = NULL;

/*
 * Create a new alias.  User, pass and path may be NULL.
 * On success NULL is returned, otherwise an error message is returned.
 */
const char *
setalias(const char *aliasname, const char *user, const char *pass,
    const char *host, const char *share, const char *path)
{
	Alias  *alias;

	if (aliases == NULL)
		aliases = list_new();

	if (strlen(aliasname) > ALIAS_MAXLEN || !validconn(user, pass, host, share, path))
		return "invalid alias";

	if (getalias(aliasname) != NULL)
		return "alias already exists";

	/* empty strings for user and pass will be dealt with in smbwrap */
	alias = xmalloc(sizeof (Alias));
	strcpy(alias->alias, aliasname);
	strcpy(alias->user, (user != NULL) ? user : "");
	strcpy(alias->pass, (pass != NULL) ? pass : "");
	strcpy(alias->host, host);
	strcpy(alias->share, share);
	strcpy(alias->path, (path != NULL) ? path : "/");
	list_add(aliases, alias);

	return NULL;
}


/*
 * Finds an alias by name, returns NULL when no such alias exists.
 */
const Alias *
getalias(const char *name)
{
	int	i;
	Alias  *alias;

	if (aliases == NULL)
		return NULL;

	for (i = 0; i < list_count(aliases); ++i) {
		alias = (Alias *)list_elem(aliases, i);
		if (streql(name, alias->alias))
			return alias;
	}
	return NULL;
}
