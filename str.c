/* $Id$ */

/*
 * The str_* functions allow for dynamically allocated strings without the
 * hassle of allocating (since allocating happens in these functions).
 */

#include "samblah.h"


/*
 * Make sure the string can store count more characters, possibly reallocating
 * the buffer in the string.
 */
static void
expandfor(Str *s, int count)
{
	int	max;
	int	len;

	/*
	 * allocate at least twice the memory currently allocated.  when more
	 * memory is needed, allocate exactly the amount necessary.
	 */
	len = str_length(s);
        if (len + count + 1 > s->size) {
		max = len * 2;
		if (len + count + 1 > max)
			max = len + count + 1;

		s->begin = xrealloc(s->begin, max);
		s->size = max;
		s->nul = s->begin+len;
	}
}


int
str_length(Str *s)
{
	if (s->begin == NULL)
		return 0;
	return s->nul - s->begin;
}


char *
str_charptr(Str *s)
{
	return s->begin;
}


char *
str_charptr_freerest(Str *s)
{
	char   *charptr;

	charptr = s->begin;
	free(s);
	return charptr;
}


void
str_putchar(Str *s, char c)
{
	int	len;

	expandfor(s, 1);

	len = str_length(s);
	s->begin[len++] = c;
	s->nul++;
	s->nul[0] = '\0';
}


void
str_putcharptr(Str *s, const char *s2)
{
	while (*s2 != '\0')
		str_putchar(s, *(s2++));
}


void
str_putstr(Str *s, Str *s2)
{
	str_putcharptr(s, str_charptr(s2));
}


void
str_free(Str *s)
{
	free(s->begin);
	free(s);
}


char
str_getchar(Str *s, int *index)
{
	if (*index+1 > str_length(s)) {
		(*index)++;
		return '\0';
	}
	return s->begin[(*index)++];
}


Str *
str_new(const char *s2)
{
	Str    *s;

	s = xmalloc(sizeof (Str));
	s->begin = xmalloc(1);
	s->nul = s->begin;
	s->begin[0] = '\0';
	s->size = 1;

	str_putcharptr(s, s2);

	return s;
}
