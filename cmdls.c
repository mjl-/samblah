/* $Id$ */

#include "samblah.h"

typedef struct Dentinfo Dentinfo;

struct Dentinfo {
	char   *name;		/* name of file/directory */
	struct	stat st;	/* file information */
};


static void     listdir(const char *, int, int);
static int	filecount(List *);
static List    *statdir(const char *);
static List    *statlist(int, char **);
static void	dentinfo_free(void *);
static int      filecmp(const void *, const void *);
static int      namecmp(const void *, const void *);
static void     printls(int, List *, int);


/*
 * Helper function for the internal ls command.  Argv contains the files to be
 * listed, starting at index 0.  There are argc elements.  Lopt indicates if -l
 * was specified on the command line, thus if size/last modification time/etc
 * should be printed.  Ropt is for -r, recursive listing.
 *
 * Files (as opposed to directories) specified on the command line can simply
 * be printed.  Directories require some special handling.  If exactly one
 * directory (and no files) was given on the command line, print the contents
 * of that directory without a leading "directory:\n".  If multiple arguments
 * were given, print the files first, then for each directory, print
 * "directory:\n" and then the contents of the directory.
 */
void
cmdls_list(int argc, char **argv, int lopt, int ropt)
{
	int	i;
	List   *entries;
	struct stat st;
	Dentinfo   *entry;

	if (argc == 1 && (smb_stat(argv[0], &st) == 0 && S_ISDIR(st.st_mode))) {
		/* exactly one argument specified which is a directory */
		listdir(argv[0], lopt, ropt);
		return;
	}

	/*
	 * one single file or multiple arguments specified.  retrieve
	 * information on all elements, sort them (files first, then
	 * directories and then on name), and print the files only.
	 */
	entries = statlist(argc, argv);
	list_sort(entries, filecmp);
	printls(filecount(entries), entries, lopt);

	/* print the directories */
	for (i = filecount(entries); !int_signal && i < list_count(entries); ++i) {
		entry = (Dentinfo *)list_elem(entries, i);

		/* no leading newline if nothing has been printed yet */
		printf("%s%s:\n", (i == 0) ? "" : "\n", entry->name);
		listdir(entry->name, lopt, ropt);
	}
	list_free_func(entries, dentinfo_free);
}

/*
 * Print contents of directory.  Lopt and ropt indicate if -l or -r was
 * specified on the command line.
 *
 * First all contents of the directory are listed, both files and directories.
 * Then, if ropt was specified, recurse in the directories.
 */
static void
listdir(const char *directory, int lopt, int ropt)
{
	List   *entries;
	Str    *newdir;
	int	i;
	Dentinfo *entry;

	/*
	 * retrieve information about all entries in the directory, sort them
	 * on name, then print all elements
	 */
	entries = statdir(directory);
	if (entries == NULL)
		return;
	list_sort(entries, namecmp);
	printls(list_count(entries), entries, lopt);

	if (!ropt) {
		/* no recursion, we are done */
		list_free_func(entries, dentinfo_free);
		return;
	}

	/* sort again, now files first, then directories */
	list_sort(entries, filecmp);

	/* skip the files, only list all directories */
	for (i = filecount(entries); !int_signal && i < list_count(entries); ++i) {
		entry = (Dentinfo *)list_elem(entries, i);

		newdir = str_new(directory);
		str_putcharptr(newdir, "/");
		str_putcharptr(newdir, entry->name);

		printf("\n%s:\n", str_charptr(newdir));
		listdir(str_charptr(newdir), lopt, ropt);
		str_free(newdir); newdir = NULL;
	}
	list_free_func(entries, dentinfo_free);
}


/*
 * Determine the number of files (as opposed to directories) in the list.
 */
static int
filecount(List *entries)
{
	int	i, count;
	Dentinfo *entry;

	count = 0;
	for (i = 0; i < list_count(entries); ++i) {
		entry = (Dentinfo *)list_elem(entries, i);
		if (!S_ISDIR(entry->st.st_mode))
			++count;
	}
	return count;
}


/*
 * Retrieve contents of directory.  On error, NULL is returned.  On success, a
 * list of Dentinfo's is returned (which should be freed by the caller).
 */
static List *
statdir(const char *directory)
{
	int	dh;
	List   *entries;
	Str    *file;
	Dentinfo  *entry;
	Smbdirent *dent;
	
	if (int_signal)
		return NULL;

	dh = smb_opendir(directory);
	if (dh < 0)
		return NULL;

	entries = list_new();

	while (!int_signal && (dent = smb_readdir(dh)) != NULL) {
		if (streql(dent->name, ".") || streql(dent->name, ".."))
			continue;

		entry = xmalloc(sizeof (Dentinfo));
		entry->name = xstrdup(dent->name);

		file = str_new(directory);
		str_putcharptr(file, "/");
		str_putcharptr(file, entry->name);

		if (smb_stat(str_charptr(file), &entry->st) != 0) {
			cmdwarn("%s", entry->name);
			dentinfo_free(entry);
			str_free(file); file = NULL;
			continue;
		}
		str_free(file); file = NULL;
		list_add(entries, entry);
	}

	if (int_signal) {
		list_free_func(entries, dentinfo_free);
		(void)smb_closedir(dh);
		return NULL;
	}

	if (smb_closedir(dh) != 0) {
		cmdwarn("%s", directory);
		list_free_func(entries, dentinfo_free);
		return NULL;
	}

	return entries;
}


/*
 * Retrieve information about all files/directories in argv (starting at index
 * 0, with argc elements).  This function always succeeds returning a list of
 * Dentinfo's which must be freed by the caller.  For files in argv for which
 * smb_stat() returns an error (e.g. because they do not exist), an error
 * message is printed and the file is ignored.
 */
static List *
statlist(int argc, char **argv)
{
	int	i;
	List   *entries;
	Dentinfo *entry;

	entries = list_new();
	for (i = 0; !int_signal && i < argc; ++i) {
		entry = xmalloc(sizeof (Dentinfo));
		entry->name = xstrdup(argv[i]);
		if (smb_stat(argv[i], &entry->st) != 0) {
			cmdwarn("%s", argv[i]);
			dentinfo_free(entry);
			continue;
		}
		list_add(entries, entry);
	}

	return entries;
}


static void
dentinfo_free(void *p)
{
	Dentinfo *entry;

	entry = (Dentinfo *)p;
	free(entry->name);
	free(entry);
}


/*
 * Compare two Dentinfo's, for use with qsort.  Files come before
 * directories.  When both elements are files or both are directories, they are
 * compared by name.
 */
static int
filecmp(const void *arg1, const void *arg2)
{
	const Dentinfo *di1 = *(const Dentinfo **)arg1;
	const Dentinfo *di2 = *(const Dentinfo **)arg2;

	if (!S_ISDIR(di1->st.st_mode) && S_ISDIR(di2->st.st_mode))
		return -1;
	if (S_ISDIR(di1->st.st_mode) && !S_ISDIR(di2->st.st_mode))
		return 1;
	return strcmp(di1->name, di2->name);
}


/*
 * Compare two Dentinfo's by name.
 */
static int
namecmp(const void *arg1, const void *arg2)
{
	const Dentinfo *di1 = *(const Dentinfo **)arg1;
	const Dentinfo *di2 = *(const Dentinfo **)arg2;
	
	return strcmp(di1->name, di2->name);
}


/*
 * Prints the first count in files.  lopt indicates if last modification time
 * and filesize should be printed.  If lopt is false, the files are printed in
 * columns.
 */
static void
printls(int count, List *entries, int lopt)
{
	int	i;
	List   *tmp;
	Dentinfo   *entry;
	const char *datestr;
	const char *trailing;

	if (lopt) {
		for (i = 0; i < count; ++i) {
			entry = (Dentinfo *)list_elem(entries, i);
			datestr = makedatestr(entry->st.st_mtime);
			trailing = S_ISDIR(entry->st.st_mode) ? "/" : "";

			printf("%s %10lld %s%s\n", datestr,
				(long long)entry->st.st_size, entry->name, trailing);
		}
	} else {
		tmp = list_new();
		for (i = 0; i < count; ++i) {
			entry = (Dentinfo *)list_elem(entries, i);
			list_add(tmp, entry->name);
		}
		printcolumns(tmp);
		/* don't free the elements, only the List container */
		list_free_func(tmp, NULL); tmp = NULL;
	}
}
