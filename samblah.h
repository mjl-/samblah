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
#include "smbwrap.h"


#define VERSION                  "0.0"
#define SAMBLAHRC_LINE_MAXLEN     1024    /* max len of line in samblahrc */
#define ALIAS_MAXLEN                64    /* max len of an alias */
#define VAR_STRING_MAXLEN          512	  /* max len of a string variable */
#define DEFAULT_PAGER           "less"
#define FALLBACK_PATH_MAXLEN      1024    /* to use when pathconf fails */
#define RESUME_ROLLBACK           8192    /* bytes to retransfer of file */

#define streql(s1, s2)  (strcmp(s1, s2) == 0)


/* miscellaneous functions, misc.c */
void    freelist(int, char **);
char   *strdup(const char *);
int     qstrcmp(const void *, const void *);
int     xsnprintf(char *, size_t, const char *, ...);
const char     *makedatestr(time_t);

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

extern const char *variablenamev[];
extern int variablenamec;

const char     *setvariable(const char *, const char *);
char           *getvariable(const char *);
int             getvariable_bool(const char *);
int             getvariable_onexist(const char *);
char           *getvariable_string(const char *);


/* aliases, alias.c */
struct alias {
	char alias[ALIAS_MAXLEN + 1];
	char user[SMB_USER_MAXLEN + 1];
	char pass[SMB_PASS_MAXLEN + 1];
	char host[SMB_HOST_MAXLEN + 1];
	char share[SMB_SHARE_MAXLEN + 1];
	char path[SMB_PATH_MAXLEN + 1];
};

const char     *setalias(const char *, const char *, const char *, const char *, const char *, const char *);
struct alias   *getalias(const char *);


/* initialization, init.c */
void do_init(int, char **);


/* ls command, interactive, cmdls.c */
void cmdls_list(int , char **, int, int);


/* commands, cmds.c */
enum connection_prereq {
	CMD_MAYCONN,            /* may be connected */
	CMD_MUSTCONN,           /* must be connected */
	CMD_MUSTNOTCONN         /* must not be connected */
};

/*
 * Note that usage and options can contain 11 lines (and a NULL), make sure
 * the struct command in cmds.c fits.
 */
struct command {
	const char *name;               /* command as typed on prompt */
	void (*func)(int, char **);    /* cmd_ function */
	enum connection_prereq conn;    /* if connection is needed */
	const char *descr;              /* description of command */
	const char *usage[12];          /* usage information */
	const char *options[12];        /* option information */
};

extern struct command cmdv[];
extern int cmdc;


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
const char     *tokenize(const char *, int *, char ***);
const char     *tokenize_escape(const char *, int *, char ***);
const char     *tokenize_partial_escape(const char *, int *, char ***, int *, int, int);
void            unescape(char *);
char           *quote_escape(char *);
int             isquoted(char *, int);


/* print columnized output, pcols.c */
struct collist {
	int width;              /* length of largest element in elems */
	char **elemv;           /* element vector */
	int elemc;             /* element count */
};

void    col_initlist(struct collist *);
int     col_addlistelem(struct collist *, const char *);
void    col_printlist(struct collist *);
void    col_freelist(struct collist *);


enum file_location {
	LOC_LOCAL,              /* local file */
	LOC_REMOTE              /* remote file */
};

/* Error/success codes, return values of smbglob. */
#define GLB_OK           0      /* no problems */
#define GLB_NOMEM       -1      /* out of memory */
#define GLB_DIRERR      -2      /* error opening/reading/closing directory */
#define GLB_INTR        -3      /* interrupted */

int     smbglob(int *, char ***, int);
int     tokenmatch(char *, char *, int *, char ***, enum file_location);


void    transfer_get(const char *, const char *, int *, int);
int     transfer_get_fd(const char *, int);
void    transfer_put(const char *, const char *, int *, int);


void    smbhlp_list_hosts(const char *, int);
void    smbhlp_list_shares(const char *, const char *, const char *, int, int);
void    smbhlp_list_workgroups(void);
void    smbhlp_move(int, char **);
void    smbhlp_remove(const char *, int);
