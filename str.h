/* $Id$ */

typedef struct Str Str;

struct Str {
	char   *begin;		/* NULL or the start of the string */
	char   *nul;		/* points to the NUL terminating the begin (undefined when begin is NULL) */
	int	size;		/* begin is a pointer to size allocated characters */
};

int	str_length(Str *);
char   *str_charptr(Str *s);
char   *str_charptr_freerest(Str *s);
void	str_putchar(Str *s, char c);
void	str_putcharptr(Str *s, const char *s2);
void	str_putstr(Str *s, Str *s2);
void	str_free(Str *s);
char	str_getchar(Str *s, int *idx);
Str    *str_new(const char *s2);
