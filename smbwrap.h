/* $Id$ */


#define SMB_USER_MAXLEN          127      /* "guest" must fit */
#define SMB_PASS_MAXLEN          127      /* "PASSWORD" must fit */
#define SMB_HOST_MAXLEN          127
#define SMB_SHARE_MAXLEN         127
#define SMB_PATH_MAXLEN         1023
#define SMB_WORKGROUP_MAXLEN     127
#define SMB_URI_MAXLEN           (6 + SMB_USER_MAXLEN + 1 + SMB_PASS_MAXLEN + 1 + SMB_HOST_MAXLEN + 1 + SMB_SHARE_MAXLEN + 1 + SMB_PATH_MAXLEN)
#define SMB_COMMENT_MAXLEN      1023

#define SMB_NAME_MAXLEN         1023

#define SMB_ERRMSG_MAXLEN        511

#define SMB_WORKGROUP       1
#define SMB_SERVER          2
#define SMB_FILE_SHARE      3
#define SMB_PRINTER_SHARE   4
#define SMB_COMMS_SHARE     5
#define SMB_IPC_SHARE       6
#define SMB_DIR             7
#define SMB_FILE            8
#define SMB_LINK            9

struct smb_dirent {
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
int     smb_read(int, void *, size_t);
int     smb_write(int, const void *, size_t);
off_t   smb_lseek(int, off_t, int);
int     smb_close(int);
int     smb_stat(const char *, struct stat *);
int     smb_fstat(int, struct stat *);
int     smb_rename(const char *, const char *);
int     smb_unlink(const char *);
int     smb_opendir(const char *);
const struct smb_dirent       *smb_readdir(int);
off_t   smb_telldir(int);
int     smb_lseekdir(int, off_t);
int     smb_closedir(int);
int     validconn(const char *, const char *, const char *, const char *, const char *);
const char     *smb_workgroups(int *, struct smb_dirent **);
const char     *smb_hosts(const char *, int *, struct smb_dirent **);
const char     *smb_shares(const char *, const char *, const char *, int *, struct smb_dirent **);
