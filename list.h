/* $Id$ */

typedef struct List List;

struct List {
	int	count;		/* number of tokens present, excluding the always present NULL-terminator */
	int	size;		/* memory for size tokens, including the always present NULL-terminator */
	void  **elems;		/* elements, always ends with a NULL. i.e. elems[count] == NULL */
};

List   *list_new(void);
void	list_free(List *);
void	list_free_func(List *, void (*)(void *));
int	list_count(List *);
void  **list_elems(List *);
void   *list_elem(List *, int);
void	list_add(List *, void *);
void	list_prepend(List *, void *);
void	list_put(List *, int, void *);
void	list_replace(List *, int, List *);
void  **list_elems_freerest(List *);
void	list_sort(List *, int (*)(const void *, const void *));
