/* $Id$ */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "egetopt.h"
#include "str.h"
#include "list.h"
#include "smbwrap.h"


#define VERSION                  "0.0"
#define DEFAULT_PAGER           "less"

enum {
	SAMBLAHRC_LINE_MAXLEN   = 1024,   /* max length of line in samblahrc */
	VAR_STRING_MAXLEN       =  512,   /* max length of a string variable */
	FALLBACK_PATH_MAXLEN    = 1024,   /* to use when pathconf fails */
	RESUME_ROLLBACK         = 8192    /* bytes to retransfer of file */
};

#define streql(s1, s2)  (strcmp(s1, s2) == 0)
#define nelem(p)	(sizeof p / sizeof p[0])


/* miscellaneous functions, misc.c */
void    printcolumns(List *);
int     qstrcmp(const void *, const void *);
int     xsnprintf(char *, size_t, const char *, ...);
const char     *makedatestr(time_t);

void   *xmalloc(size_t);
void   *xrealloc(void *, size_t);
char   *xstrdup(const char *);

void    err(int, const char *, ...);
void    errx(int, const char *, ...);
void    warn(const char *, ...);
void    warnx(const char *, ...);
void    vwarn(const char *, va_list);
void    vwarnx(const char *, va_list);

void    cmdwarn(const char *, ...);
void    cmdwarnx(const char *, ...);


/* variables, vars.c */
enum    { VAR_ASK, VAR_RESUME, VAR_OVERWRITE, VAR_SKIP };

const char    **listvariables(void);
const char     *setvariable(const char *, const char *);
char   *getvariable(const char *);
int	getvariable_bool(const char *);
int	getvariable_onexist(const char *);
char   *getvariable_string(const char *);


/* initialization, init.c */
void do_init(int, char **);


/* ls command, interactive, cmdls.c */
void cmdls_list(int, char **, int, int);


/* commands, cmds.c */
typedef struct Cmd Cmd;

enum connection_prereq {
	CMD_MAYCONN,            /* may be connected */
	CMD_MUSTCONN,           /* must be connected */
	CMD_MUSTNOTCONN         /* must not be connected */
};

/*
 * Note that usage and options can contain 11 lines (and a NULL), make sure
 * the struct command in cmds.c fits.
 */
struct Cmd {
	const char *name;               /* command as typed on prompt */
	void (*func)(int, char **);    /* cmd_ function */
	enum connection_prereq conn;    /* if connection is needed */
	const char *descr;              /* description of command */
	const char *usage[12];          /* usage information */
	const char *options[12];        /* option information */
};

extern Cmd	commands[];
extern int	commandcount;


/* generating and displaying matches, complete.c  */
char  **complete(const char *, int, int);
void    displaymatches(char **, int, int);


/* functions for interface to user, interface.c */
void    do_interface(void);
int     term_width(void);
int     askpass(char [SMB_PASS_MAXLEN + 1]);

extern int done;
extern const char *cmdname;
extern volatile sig_atomic_t int_signal;


/* command/.samblahrc line parse functions, parsecl.c */
const char     *tokenize(const char *, List *);
const char     *tokenize_escape(const char *, List *);
const char     *tokenize_partial_escape(const char *, List *, int *, int, int);
void            unescape(char *);
char           *quote(char *);
int             isquoted(char *, int);


/* Error/success codes, return values of smbglob. */
#define GLB_OK           0      /* no problems */
#define GLB_DIRERR      -1      /* error opening/reading/closing directory */
#define GLB_INTR        -2      /* interrupted */

int     smbglob(List *, int);
int     tokenmatch(const char *, List *, int);


/* transfer data from/to smb files or file descriptors, transfer.c */
void    transfer_get(const char *, const char *, int *, int);
int     transfer_get_fd(const char *, int);
void    transfer_put(const char *, const char *, int *, int);


/* help functions doing much of the actual work for the internal commands, smbhlp.c */
void    smbhlp_list_hosts(const char *, int);
void    smbhlp_list_shares(const char *, const char *, const char *, int, int);
void    smbhlp_list_workgroups(void);
void    smbhlp_move(int, char **);
void    smbhlp_remove(const char *, int);
