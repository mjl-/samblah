/* $Id$ */

enum {
	SMB_USER_MAXLEN       =  127,      /* "guest" must fit */
	SMB_PASS_MAXLEN       =  127,     /* "PASSWORD" must fit */
	SMB_HOST_MAXLEN       =  127,
	SMB_SHARE_MAXLEN      =  127,
	SMB_PATH_MAXLEN       = 1023,
	SMB_WORKGROUP_MAXLEN  =  127,
	SMB_URI_MAXLEN        =  6 + SMB_USER_MAXLEN + 1 + SMB_PASS_MAXLEN + 1 + SMB_HOST_MAXLEN + 1 + SMB_SHARE_MAXLEN + 1 + SMB_PATH_MAXLEN,
	SMB_COMMENT_MAXLEN    = 1023
};

enum {
	SMB_ERRMSG_MAXLEN     =  511
};

enum {
	SMB_WORKGROUP     = 1,
	SMB_SERVER        = 2,
	SMB_FILE_SHARE    = 3,
	SMB_PRINTER_SHARE = 4,
	SMB_COMMS_SHARE   = 5,
	SMB_IPC_SHARE     = 6,
	SMB_DIR           = 7,
	SMB_FILE          = 8,
	SMB_LINK          = 9
};


typedef struct Smbdirent Smbdirent;

struct Smbdirent {
	unsigned int    type;
	char            comment[SMB_COMMENT_MAXLEN];
	char            name[SMB_PATH_MAXLEN];
};


extern int connected;


int     smb_init(void);
char   *smb_connect(const char *, const char *, const char *, const char *, const char *);
int     smb_disconnect(void);
int     smb_chdir(const char *);
const char     *smb_getcwd(void);
int     smb_mkdir(const char *, mode_t);
int     smb_rmdir(const char *);
int     smb_open(const char *, int, mode_t);
ssize_t smb_read(int, void *, size_t);
ssize_t smb_write(int, const void *, size_t);
off_t   smb_lseek(int, off_t, int);
int     smb_close(int);
int     smb_stat(const char *, struct stat *);
int     smb_fstat(int, struct stat *);
int     smb_rename(const char *, const char *);
int     smb_unlink(const char *);
int     smb_opendir(const char *);
Smbdirent      *smb_readdir(int);
off_t   smb_telldir(int);
int     smb_lseekdir(int, off_t);
int     smb_closedir(int);
int     validconn(const char *, const char *, const char *, const char *, const char *);
const char     *smb_workgroups(List *);
const char     *smb_hosts(const char *, List *);
const char     *smb_shares(const char *, const char *, const char *, List *);
