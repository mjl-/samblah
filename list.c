/* $Id$ */

/*
 * List.c provides functions for managing lists (e.g. of filenames, tokens),
 * mostly, these are strings.
 *
 * All elements in a list are assumed to be allocated using malloc(3), some
 * functions free(3) elements (e.g. list_free(), list_replace()).
 */

#include "samblah.h"

/*
 * Check if list can hold count more elements.
 * If not, reallocate so that it can.
 */
static void
expandfor(List *list, int count)
{
	int	max;

	/*
	 * allocate at least twice the memory currently allocated.  when more
	 * memory is needed, allocate exactly the amount necessary.
	 */
	if (list->count + count + 1 > list->size) {
#define MAX(a, b)	((a) > (b) ? (a) : (b))
		max = MAX(list->size * 2, list->count + count + 1);
#undef MAX
		list->elems = xrealloc(list->elems, sizeof list->elems[0] * max);
		list->size = max;
	}
}


/*
 * Allocates and returns a new List.  The returned element is NULL-terminated
 * and has zero elements.
 */
List *
list_new(void)
{
	List   *list;

	list = xmalloc(sizeof (List));
	list->elems = xmalloc(sizeof (void *));
	list->elems[0] = NULL;
	list->count = 0;
	list->size = 1;

	return list;
}


/*
 * Frees:
 * - all elements held in list->elems using free(3)
 * - list->elems itself
 * - list itself
 */
void
list_free(List *list)
{
	list_free_func(list, free);
}


/*
 * Frees:
 * - all elements held in list->elems, using the supplied function
 * - list->elems itself
 * - list itself
 */
void
list_free_func(List *list, void (*freefunc)(void *))
{
	int	i;

	if (freefunc != NULL)
		for (i = 0; i < list->count; ++i)
			freefunc(list->elems[i]);
	free(list->elems);
	free(list);
}


/*
 * Returns the number of list (excluding the terminating NULL).
 */
int
list_count(List *list)
{
	return list->count;
}


/*
 * Returns the elements-vector held in list.  The caller must not modify the
 * returned vector.
 */
void **
list_elems(List *list)
{
	return list->elems;
}


/*
 * Returns the token at index.  Index must be a valid index (i.e. less than
 * list->count).
 */
void *
list_elem(List *list, int index)
{
	assert(index < list->count);
	return list->elems[index];
}


/*
 * Adds the given element to the end of the list.  A terminating NULL is
 * kept.
 */
void
list_add(List *list, void *newtoken)
{
	assert(newtoken != NULL);
	expandfor(list, 1);

	list->elems[list->count++] = newtoken;
	list->elems[list->count] = NULL;
}


/*
 * Puts the given element at the beginning of list, the current elements are
 * shifted.
 */
void
list_prepend(List *list, void *newtoken)
{
	int	i;

	assert(newtoken != NULL);
	expandfor(list, 1);

	/* shift elements one place, including trailing NULL */
	for (i = list->count; i >= 0; --i)
		list->elems[i + 1] = list->elems[i];
	list->elems[0] = newtoken;
	++list->count;
}


/*
 * Puts newtoken at index.  The old element is not freed, just replaced.
 */
void
list_put(List *list, int index, void *newtoken)
{
	assert(index < list->count);
	assert(newtoken != NULL);

	list->elems[index] = newtoken;
}


/*
 * Replace the string at index by the strings in newlist.
 * The string at index is freed.  The strings in newlist are duplicated
 * using xstrdup before being put in list.  Nothing of newlist is freed.
 */
void
list_replace(List *list, int index, List *newlist)
{
	int	i;
	int	extracount;

	assert(index < list->count);
	expandfor(list, list_count(newlist));

	free(list->elems[index]);

	/*
	 * move the elements past index (including the trailing NULL) to make
	 * room for the list_count(newlist) - 1 new elements
	 */
	extracount = list_count(newlist) - 1;
	for (i = list->count; i >= index + 1; --i)
		list->elems[i + extracount] = list->elems[i];

	for (i = 0; i < list_count(newlist); ++i) {
		list->elems[index] = xstrdup(list_elem(newlist, i));
		++index;
	}
	list->count += list_count(newlist) - 1;
}


/*
 * Returns list->elems but before doing so, frees list itself.
 */
void **
list_elems_freerest(List *list)
{
	void  **elems;

	elems = list->elems;
	free(list);

	return elems;
}


/*
 * Sorts the elements in the list using qsort and the giving compare function.
 */
void
list_sort(List *list, int (*compare)(const void *, const void *))
{
	qsort(list->elems, list->count, sizeof list->elems[0], compare);
}
