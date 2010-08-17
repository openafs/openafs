/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * User space client specific interface glue
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include <net/if.h>
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "afs_usrops.h"
#include "afs/auth.h"
#include "afs/cellconfig.h"
#include "afs/vice.h"
#include "afs/kautils.h"
#include "afs/nsafs.h"

#define NSAFS_DFLT_RCVTHREADS	2	/* Dflt number recevice threads */
#define NSAFS_BUFFER_SIZE	4096	/* Send/Receive buffer size */
#define NSAFS_MAX_PATH		1024	/* Maximum path length */
#define NSAFS_USERNAME_MAX	64	/* Maximum username length */
#define NSAFS_PASSWORD_MAX	64	/* Maximum password length */
#define NSAFS_LOGIN_HASH_SIZE	1024	/* MUST be power of two */
#define TEN_MINUTES		600	/* 10 minutes = 600 seconds */

#define NSAFS_DIR_ALLOW		"GET,HEAD,MOVE,INDEX,RMDIR"
#define NSAFS_LINK_ALLOW	"GET,HEAD,MOVE,DELETE"
#define NSAFS_FILE_ALLOW	"GET,HEAD,MOVE,PUT,DELETE"

#ifndef MAX
#define MAX(A,B)		((A)>(B)?(A):(B))
#endif /* !MAX */
#ifndef MIN
#define MIN(A,B)		((A)<(B)?(A):(B))
#endif /* !MAX */

/*
 * Used by KA module to get local cell info
 */
struct afsconf_dir *KA_conf;

/*
 * Initialization parameters. The plugin is initialized in
 * the Netscape parent process, but we don't initialize AFS
 * until we are in the child process.
 */
CRITICAL nsafs_init_lock;
int nsafs_init_done;
char *mountDirParam;
char *cellNameParam;
char *confDirParam;
char *logFileParam;
char *cacheBaseDirParam;
int cacheBlocksParam;
int cacheFilesParam;
int cacheStatEntriesParam;
int dCacheSizeParam;
int vCacheSizeParam;
int chunkSizeParam;
int debugParam;
int nDaemonsParam;
long maxExpirationParam;

/*
 * Structure user to store entries in AFS login cache
 */
struct nsafs_login {
    afs_uint32 expiration;
    long viceid;
    long group0;
    long group1;
    char username[NSAFS_USERNAME_MAX];
    char cellname[NSAFS_USERNAME_MAX];
    char cksum[SHA_HASH_BYTES];
    struct nsafs_login *next;
    struct nsafs_login *prev;
};

/*
 * Value used to initialize SHA checksums on username/password pairs
 */
afs_uint32 nsafs_login_pad[SHA_HASH_INTS] = {
    0x0D544971, 0x2281AC5B, 0x58B51218, 0x4085E08D, 0xB68C484B
};

/*
 * Cache of AFS logins
 */
CRITICAL nsafs_login_lock;
struct {
    struct nsafs_login *head;
    struct nsafs_login *tail;
} nsafs_login_cache[NSAFS_LOGIN_HASH_SIZE];

/*
 * Mapping from characters to 64 bit values
 */
static int base64_to_value[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * Decode a base64 encoded buffer in place
 */
void
nsafs_decode64(char *buf)
{
    int i, j;
    int len;
    int val1;
    int val2;
    int val3;
    int val4;

    /*
     * Allow trailing blanks
     */
    for (len = strlen(buf); buf[len - 1] == ' ' && len > 0; len--);

    /*
     * Valid encodings are multiples of four characters
     */
    if (len & 0x3) {
	buf[0] = '\0';
	return;
    }

    for (i = 0, j = 0; i < len; i += 4, j += 3) {
	val1 = base64_to_value[buf[i]];
	val2 = base64_to_value[buf[i + 1]];
	val3 = base64_to_value[buf[i + 2]];
	val4 = base64_to_value[buf[i + 3]];
	buf[j] = ((val1 << 2) & 0xfc) | ((val2 >> 4) & 0x3);
	buf[j + 1] = ((val2 << 4) & 0xf0) | ((val3 >> 2) & 0xf);
	buf[j + 2] = ((val3 << 6) & 0xc0) | (val4 & 0x3f);
    }
    buf[j] = '\0';
}


/*
 * Interface for pioctls - used for unlogging
 */
#include "afs/venus.h"
int
do_pioctl(char *in_buffer, int in_size, char *out_buffer, int out_size,
	  int opcode, char *path, int followSymLinks)
{
    struct ViceIoctl iob;
    iob.in = in_buffer;
    iob.in_size = in_size;
    iob.out = out_buffer;
    iob.out_size = out_size;

#ifdef AFS_USR_SUN5_ENV
    return syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, _VICEIOCTL(opcode),
		   &iob, followSymLinks);
#else /* AFS_USR_SUN5_ENV */
    return lpioctl(path, _VICEIOCTL(opcode), &iob, followSymLinks);
#endif /* AFS_USR_SUN5_ENV */
}

/*
 * unlog - invalidate any existing AFS tokens with the kernel cache
 * manager. In case the server is started up with tokens
 */
int
unlog()
{
    return do_pioctl(NULL, 0, NULL, 0, VIOCUNPAG, NULL, 0);
}

/*
 * Initialize the AFS client and the login cache
 */
void
nsafs_init_once()
{
    int i;
    crit_enter(nsafs_init_lock);
    if (nsafs_init_done == 0) {
	i = unlog();
	if (i) {
/*	printf("unlog from AFS failed: errno:%d\n", errno); */
	}
	uafs_Init("nsafs-init", mountDirParam, confDirParam,
		  cacheBaseDirParam, cacheBlocksParam, cacheFilesParam,
		  cacheStatEntriesParam, dCacheSizeParam, vCacheSizeParam,
		  chunkSizeParam, 0, debugParam, nDaemonsParam, -1,
		  logFileParam);
	nsafs_login_lock = crit_init();
	for (i = 0; i < NSAFS_LOGIN_HASH_SIZE; i++) {
	    DLL_INIT_LIST(nsafs_login_cache[i].head,
			  nsafs_login_cache[i].tail);
	}
	KA_conf = afs_cdir;
	nsafs_init_done = 1;
    }
    crit_exit(nsafs_init_lock);
}

/*
 * Hash function for the AFS login cache
 */
int
nsafs_login_hash(char *name, char *cell)
{
    char *p;
    afs_uint32 val;
    for (val = *name, p = name; *p != '\0'; p++) {
	val = (val << 2) ^ val ^ (afs_uint32) (*p);
    }
    for (p = cell; *p != '\0'; p++) {
	val = (val << 2) ^ val ^ (afs_uint32) (*p);
    }
    return val & (NSAFS_LOGIN_HASH_SIZE - 1);
}

/*
 * Compute a SHA checksum on the username, cellname, and password
 */
void
nsafs_login_checksum(char *user, char *cell, char *passwd, char *cksum)
{
    int passwdLen;
    int userLen;
    int cellLen;
    char *shaBuffer;
    shaState state;

    /*
     * Compute SHA(username,SHA(password,pad))
     */
    passwdLen = strlen(passwd);
    userLen = strlen(user);
    cellLen = strlen(cell);
    shaBuffer =
	afs_osi_Alloc(MAX(userLen + cellLen, passwdLen) + SHA_HASH_BYTES);
    strcpy(shaBuffer, passwd);
    memcpy((void *)(shaBuffer + passwdLen), (void *)(&nsafs_login_pad[0]),
	   SHA_HASH_BYTES);
    sha_clear(&state);
    sha_hash(&state, shaBuffer, passwdLen + SHA_HASH_BYTES);
    memcpy(shaBuffer, user, userLen);
    memcpy(shaBuffer + userLen, cell, cellLen);
    sha_bytes(&state, shaBuffer + userLen + cellLen);
    sha_clear(&state);
    sha_hash(&state, shaBuffer, userLen + cellLen + SHA_HASH_BYTES);
    sha_bytes(&state, &cksum[0]);
    memset(shaBuffer, 0, MAX(userLen + cellLen, passwdLen) + SHA_HASH_BYTES);
    afs_osi_Free(shaBuffer,
		 MAX(userLen + cellLen, passwdLen) + SHA_HASH_BYTES);
}

/*
 * Set the AFS identity given from the group0 and group1 strings
 */
void
nsafs_set_id_from_ints(int viceid, int group0, int group1)
{
    int i;
    struct usr_ucred *crp;

    u.u_viceid = viceid;
    crp = u.u_cred;
    set_cr_uid(crp, viceid);
    set_cr_ruid(crp, viceid);
    crp->cr_suid = viceid;
    crp->cr_groups[0] = group0;
    crp->cr_groups[1] = group1;
    crp->cr_groups[2] = getgid();
    crp->cr_ngroups = 1;
    for (i = 3; i < NGROUPS; i++) {
	crp->cr_groups[i] = NOGROUP;
    }
}

/*
 * Set the AFS identity given from the viceid, group0 and group1 strings
 */
void
nsafs_set_id_from_strings(char *viceid, char *group0, char *group1)
{
    int i;
    struct usr_ucred *crp;

    if (viceid != NULL && group0 != NULL && group1 != NULL) {
	nsafs_set_id_from_ints(atoi(viceid), atoi(group0), atoi(group1));
    } else {
	u.u_viceid = getuid();
	crp = u.u_cred;
	set_cr_uid(crp, getuid());
	set_cr_ruid(crp, getuid());
	crp->cr_suid = getuid();
	crp->cr_groups[0] = getgid();
	crp->cr_ngroups = 1;
	for (i = 1; i < NGROUPS; i++) {
	    crp->cr_groups[i] = NOGROUP;
	}
    }
}

/*
 * Look up a login ID in the cache. If an entry name is found for the
 * given username, and the SHA checksums match, then set the group0
 * and group1 parameters and return 1, otherwise return 0.
 */
int
nsafs_login_lookup(char *user, char *cell, char *cksum, int *viceid,
		   int *group0, int *group1)
{
    int index;
    long curTime;
    struct nsafs_login *loginP, *tmpP, loginTmp;

    /*
     * Search the hash chain for a matching entry, free
     * expired entries as we search
     */
    index = nsafs_login_hash(user, cell);
    curTime = time(NULL);
    crit_enter(nsafs_login_lock);
    loginP = nsafs_login_cache[index].head;
    while (loginP != NULL) {
	if (loginP->expiration < curTime) {
	    tmpP = loginP;
	    loginP = tmpP->next;
	    nsafs_set_id_from_ints(tmpP->viceid, tmpP->group0, tmpP->group1);
	    uafs_unlog();
	    DLL_DELETE(tmpP, nsafs_login_cache[index].head,
		       nsafs_login_cache[index].tail, next, prev);
	    afs_osi_Free(tmpP, sizeof(struct nsafs_login));
	    continue;
	}
	if (strcmp(loginP->username, user) == 0
	    && strcmp(loginP->cellname, cell) == 0
	    && memcmp((void *)&loginP->cksum[0], (void *)cksum,
		      SHA_HASH_BYTES) == 0) {
	    *viceid = loginP->viceid;
	    *group0 = loginP->group0;
	    *group1 = loginP->group1;
	    crit_exit(nsafs_login_lock);
	    return 1;
	}
	loginP = loginP->next;
    }
    crit_exit(nsafs_login_lock);
    return 0;
}

/*
 * Insert a login ID into the cache. If the user already has an entry,
 * then overwrite the old entry.
 */
int
nsafs_login_store(char *user, char *cell, char *cksum, int viceid, int group0,
		  int group1, afs_uint32 expiration)
{
    int index;
    long curTime;
    struct nsafs_login *loginP, *tmpP, loginTmp;

    /*
     * Search the hash chain for a matching entry, free
     * expired entries as we search
     */
    index = nsafs_login_hash(user, cell);
    curTime = time(NULL);
    crit_enter(nsafs_login_lock);
    loginP = nsafs_login_cache[index].head;
    while (loginP != NULL) {
	if (strcmp(loginP->username, user) == 0
	    && strcmp(loginP->cellname, cell) == 0) {
	    break;
	}
	if (loginP->expiration < curTime) {
	    tmpP = loginP;
	    loginP = tmpP->next;
	    nsafs_set_id_from_ints(tmpP->viceid, tmpP->group0, tmpP->group1);
	    uafs_unlog();
	    DLL_DELETE(tmpP, nsafs_login_cache[index].head,
		       nsafs_login_cache[index].tail, next, prev);
	    afs_osi_Free(tmpP, sizeof(struct nsafs_login));
	    continue;
	}
	loginP = loginP->next;
    }
    if (loginP == NULL) {
	loginP = (struct nsafs_login *)
	    afs_osi_Alloc(sizeof(struct nsafs_login));
	strcpy(&loginP->username[0], user);
	strcpy(&loginP->cellname[0], cell);
    } else {
	DLL_DELETE(loginP, nsafs_login_cache[index].head,
		   nsafs_login_cache[index].tail, next, prev);
	nsafs_set_id_from_ints(loginP->viceid, loginP->group0,
			       loginP->group1);
	uafs_unlog();
    }
    nsafs_set_id_from_ints(viceid, group0, group1);
    memcpy((void *)&loginP->cksum[0], (void *)cksum, SHA_HASH_BYTES);
    loginP->viceid = viceid;
    loginP->group0 = group0;
    loginP->group1 = group1;
    loginP->expiration = expiration;
    DLL_INSERT_TAIL(loginP, nsafs_login_cache[index].head,
		    nsafs_login_cache[index].tail, next, prev);
    crit_exit(nsafs_login_lock);
    return 0;
}

/*
 * Extract a string parameter from the parameter block
 */
int
nsafs_get_string(char **paramP, char *dflt, char *name, pblock * pb,
		 Session * sn, Request * rq)
{
    char *tmpPtr;
    char error[128];

    tmpPtr = pblock_findval(name, pb);
    if (tmpPtr == NULL) {
	if (dflt == NULL) {
	    log_error(LOG_MISCONFIG, "nsafs", sn, rq,
		      "nsafs_init: please supply a %s parameter", name);
	    return REQ_ABORTED;
	} else {
	    tmpPtr = dflt;
	}
    }
    *paramP = afs_osi_Alloc(strlen(tmpPtr) + 1);
    strcpy(*paramP, tmpPtr);
    return REQ_PROCEED;
}

/*
 * Extract a long integer parameter from the parameter block
 */
int
nsafs_get_long(long *paramP, long dflt, char *name, pblock * pb, Session * sn,
	       Request * rq)
{
    char *start, *end;
    long val;

    start = pblock_findval(name, pb);
    if (start == NULL) {
	if (dflt < 0) {
	    log_error(LOG_MISCONFIG, "nsafs", sn, rq,
		      "nsafs_init: please supply a %s parameter", name);
	    return REQ_ABORTED;
	} else {
	    *paramP = dflt;
	    return REQ_PROCEED;
	}
    }
    val = strtol(start, &end, 10);
    if (val <= 0 || end == start || *end != '\0') {
	log_error(LOG_MISCONFIG, "nsafs", sn, rq,
		  "nsafs_init: invalid %s parameter '%s'", name, start);
	return REQ_ABORTED;
    }
    *paramP = val;
    return REQ_PROCEED;
}

/*
 * Extract an integer parameter from the parameter block
 */
int
nsafs_get_int(int *paramP, int dflt, char *name, pblock * pb, Session * sn,
	      Request * rq)
{
    int code;
    long val;

    code = nsafs_get_long(&val, (long)dflt, name, pb, sn, rq);
    if (code == REQ_PROCEED) {
	*paramP = val;
    }
    return code;
}

/*
 * Parse the authorization header for username and password
 */
void
nsafs_parse_authhdr(char *authHdr, char *user, char *cell, char *passwd)
{
    int i;
    char *p;

    user[0] = '\0';
    passwd[0] = '\0';
    cell[0] = '\0';

    /*
     * Skip leading blanks, check for basic authentication
     */
    for (p = authHdr; *p == ' ' && *p != '\0'; p++);
    if (strncasecmp(p, "basic ", 6) != 0) {
	return;
    }
    for (p += 6; *p == ' '; p++);

    /*
     * Username and password are base64 encoded
     */
    nsafs_decode64(p);

    /*
     * Format is user@cell:passwd. The user, cell or passwd may be missing
     */
    for (i = 0; *p != '@' && *p != ':' && *p != '\0'; p++, i++) {
	user[i] = *p;
    }
    user[i] = '\0';
    if (*p == '@') {
	for (i = 0, p++; *p != ':' && *p != '\0'; p++, i++) {
	    cell[i] = *p;
	}
	cell[i] = '\0';
    }
    if (*p == ':') {
	for (i = 0, p++; *p != '\0'; p++, i++) {
	    passwd[i] = *p;
	}
	passwd[i] = '\0';
    }
}

/*
 * Return an appropriate error given a system errno
 */
int
nsafs_error_check(int code, char *text, pblock * pb, Session * sn,
		  Request * rq)
{
    char txtbuf[256];
    char *realmBuf;
    char *path;
    char *tmp;

    if (text != NULL) {
	sprintf(txtbuf, "%s: %s\n", text, strerror(code));
    } else {
	sprintf(txtbuf, "%s\n", strerror(code));
    }

    /*
     * Special case, if we get EACCES inside a public directory
     * and are unauthenticated, change the error to EPERM unless
     * nsafs_nocheck is set. If nsafs_nocheck is set then change
     * EPERM to EACCES
     */
    if (code == EACCES && pblock_findval("nsafs_nocheck", rq->vars) == NULL
	&& (pblock_findval("nsafs_viceid", rq->vars) == NULL
	    || pblock_findval("nsafs_group0", rq->vars) == NULL
	    || pblock_findval("nsafs_group1", rq->vars) == NULL)) {
	code = EPERM;
    } else if (code == EPERM
	       && pblock_findval("nsafs_nocheck", rq->vars) == NULL) {
	char *status = pblock_findval("status", rq->vars);
	if (strcmp(status, "Login Failed"))
	    code = EACCES;
    }

    switch (code) {
    case EPERM:
	/*
	 * We overload EPERM (not super-user) to mean unauthenticated.
	 * We use the first subdirectory beneath the AFS mount point
	 * as the realm.
	 */
	log_error(LOG_SECURITY, "nsafs", sn, rq, txtbuf);
	protocol_status(sn, rq, PROTOCOL_UNAUTHORIZED, NULL);
	path = pblock_findval("path", rq->vars);
	realmBuf = NULL;
	if (path != NULL) {
	    path = uafs_afsPathName(path);
	}
	if (path != NULL && *path != '\0') {
	    realmBuf = strdup(path);
	}
	if (realmBuf == NULL) {
	    /* Don't dump core, just make AFS into one big realm */
	    sprintf(txtbuf, "Basic realm=\"%s\"", afs_mountDir);
	} else {
	    /* extract the first subdirectory in AFS */
	    if ((tmp = strchr(realmBuf, '/')) != NULL)
		*tmp = '\0';
	    sprintf(txtbuf, "Basic realm=\"%s/%s\"", afs_mountDir, realmBuf);
	    free(realmBuf);
	}
	pblock_nvinsert("WWW-authenticate", txtbuf, rq->srvhdrs);
	break;
    case EACCES:
	log_error(LOG_SECURITY, "nsafs", sn, rq, txtbuf);
	protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
	break;
    case ENOENT:
    case ENOTDIR:
	log_error(LOG_INFORM, "nsafs", sn, rq, txtbuf);
	protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
	break;
    default:
	log_error(LOG_FAILURE, "nsafs", sn, rq, txtbuf);
	protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
	break;
    }
    return REQ_ABORTED;
}

/*
 * Check the preconditions on a request. Return REQ_PROCEED
 * if the preconditions are met. Any other return value means
 * that the request has been aborted.
 */
int
nsafs_check_preconditions(struct stat *stp, pblock * pb, Session * sn,
			  Request * rq)
{
    int code;
    time_t mtime;
    struct tm tms, *tmsp;
    char *reqhdr;

    mtime = stp->st_mtime;
    tmsp = system_gmtime(&mtime, &tms);

    request_header("if-modified-since", &reqhdr, sn, rq);
    if (reqhdr != NULL && util_later_than(tmsp, reqhdr)) {
	protocol_status(sn, rq, PROTOCOL_NOT_MODIFIED, NULL);
	return REQ_ABORTED;
    }

    request_header("if-unmodified-since", &reqhdr, sn, rq);
    if (reqhdr != NULL && !util_later_than(tmsp, reqhdr)) {
	protocol_status(sn, rq, PROTOCOL_PRECONDITION_FAIL, NULL);
	return REQ_ABORTED;
    }

    return REQ_PROCEED;
}

/*
 * Set the content-length and last-modified response header
 *
 * We used to call protocol_set_finfo, but it wasn't handling
 * if-unmodified-since headers correctly.
 */
int
nsafs_set_finfo(Session * sn, Request * rq, struct stat *stp)
{
    int code;
    time_t mtime;
    struct tm tms, *tmsp;
    char *reqhdr;
    char dateStr[128];
    char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    mtime = stp->st_mtime;
    tmsp = system_gmtime(&mtime, &tms);
    sprintf(&dateStr[0], "%s, %02d %s %d %02d:%02d:%02d GMT",
	    days[tmsp->tm_wday], tmsp->tm_mday, months[tmsp->tm_mon],
	    tmsp->tm_year + 1900, tmsp->tm_hour, tmsp->tm_min, tmsp->tm_sec);
    pblock_nvinsert("Last-Modified", &dateStr[0], rq->srvhdrs);
    pblock_nninsert("Content-Length", stp->st_size, rq->srvhdrs);

    return REQ_PROCEED;
}

/*
 * Initialize the AFS plugin. We do not initialize the AFS client
 * here because we are still in the parent process. We don't
 * initialize AFS until we get the first service request.
 */
NSAPI_PUBLIC int
nsafs_init(pblock * pb, Session * sn, Request * rq)
{
    int code;

    nsafs_init_done = 0;
    nsafs_init_lock = crit_init();

    /*
     * Parse the startup parameters
     */
    code = nsafs_get_string(&mountDirParam, "/afs", "mount", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_string(&cellNameParam, NULL, "cell", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_string(&confDirParam, NULL, "confdir", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_string(&cacheBaseDirParam, NULL, "cachedir", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&cacheBlocksParam, -1, "blocks", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&cacheFilesParam, 0, "files", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&cacheStatEntriesParam, -1, "stat", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&nDaemonsParam, -1, "daemons", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&dCacheSizeParam, 0, "dcache", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&vCacheSizeParam, 0, "volumes", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&chunkSizeParam, 0, "chunksize", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_int(&debugParam, 0, "debug", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code = nsafs_get_string(&logFileParam, NULL, "logfile", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }
    code =
	nsafs_get_long(&maxExpirationParam, LONG_MAX, "exp-max", pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }

    return REQ_PROCEED;
}

/*
 * Extract name strings from a comma separated list
 */
char *
nsafs_NameFromNames(char *last, char *list, int *pos)
{
    int len;
    char *start;
    char *retVal;

    if (last == NULL) {
	*pos = 0;
    } else {
	afs_osi_Free(last, strlen(last) + 1);
    }
    start = &list[*pos];
    if (*start == '\0') {
	return NULL;
    }
    for (len = 0; start[len] != ',' && start[len] != '\0'; len++);
    *pos += len;
    if (list[*pos] == ',') {
	*pos += 1;
    }
    retVal = afs_osi_Alloc(len + 1);
    memcpy(retVal, start, len);
    retVal[len] = '\0';
    return retVal;
}

/*
 * Authcheck function for AFS
 *
 * Check for an Authorization header. If there is one then do the
 * AFS login and place the first two groups in the user's creds
 * into the nsafs-group1 and nsafs-group2 request variables.
 * Send an Unauthorized response if login fails to prompt the user
 * to reenter the username and password.
 */
NSAPI_PUBLIC int
nsafs_basic(pblock * pb, Session * sn, Request * rq)
{
    int i;
    int rc;
    int code;
    int pos;
    char *authHdr;
    char *cellP;
    char user[NSAFS_USERNAME_MAX];
    char cell[NSAFS_USERNAME_MAX];
    char passwd[NSAFS_PASSWORD_MAX];
    char cksum[SHA_HASH_BYTES];
    struct usr_ucred *crp;
    afs_int32 password_expires = -1;
    int viceid;
    int group0;
    int group1;
    afs_uint32 expiration;
    char *reason;
    char txtbuf[128];

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Get the authorization header, if none found then continue,
     * we check whether authorization is required later on.
     */
    request_header("authorization", &authHdr, sn, rq);
    if (authHdr == NULL || strlen(authHdr) == 0) {
	return REQ_NOACTION;
    }

    /*
     * Get the user name and password from the authorization header
     */
    nsafs_parse_authhdr(authHdr, &user[0], &cell[0], &passwd[0]);
    if (user[0] == '\0' || passwd[0] == '\0') {
	memset((void *)&passwd[0], 0, NSAFS_PASSWORD_MAX);
	pblock_nvinsert("status", "Login Failed", rq->vars);
	return nsafs_error_check(EPERM, "Invalid auth header", pb, sn, rq);
    }
    if (cell[0] == '\0') {
	strcpy(cell, afs_LclCellName);
    }

    /*
     * Lookup the username and password in the login cache.
     * If we find a match we reuse the existing identity.
     */
    nsafs_login_checksum(user, cell, passwd, &cksum[0]);
    rc = nsafs_login_lookup(user, cell, cksum, &viceid, &group0, &group1);
    if (rc != 0) {
	pblock_nninsert("nsafs_viceid", viceid, rq->vars);
	pblock_nninsert("nsafs_group0", group0, rq->vars);
	pblock_nninsert("nsafs_group1", group1, rq->vars);
	memset((void *)&passwd[0], 0, NSAFS_PASSWORD_MAX);
	return REQ_PROCEED;
    }

    /*
     * Make sure the user is from one of the cells we are configured for
     */
    cellP = nsafs_NameFromNames(NULL, cellNameParam, &pos);
    while (cellP != NULL) {
	if (strcmp(cellP, cell) == 0) {
	    break;
	}
	cellP = nsafs_NameFromNames(cellP, cellNameParam, &pos);
    }
    if (cellP == NULL) {
	memset((void *)&passwd[0], 0, NSAFS_PASSWORD_MAX);
	pblock_nvinsert("status", "Login Failed", rq->vars);
	return nsafs_error_check(EPERM, "Invalid cell", pb, sn, rq);
    }
    afs_osi_Free(cellP, strlen(cellP) + 1);

    u.u_expiration = 0;
    nsafs_set_id_from_strings(NULL, NULL, NULL);
    code = uafs_klog(user, cell, passwd, &reason);
    memset((void *)&passwd[0], 0, NSAFS_PASSWORD_MAX);
    if (code != 0) {
#if 0
	sprintf(txtbuf, "%s@%s: %s\n", user, cell, reason);
	pblock_nvinsert("status", "Login Failed", rq->vars);
	return nsafs_error_check(EPERM, txtbuf, pb, sn, rq);
#else /* 0 */
	return REQ_PROCEED;
#endif /* 0 */
    }
    expiration = u.u_expiration;
    usr_assert(expiration != 0);
    expiration =
	MIN(expiration, (afs_uint32) (time(NULL) + maxExpirationParam));

    /*
     * Insert the credentials into the login cache
     */
    pblock_nvinsert("auth-type", "basic", rq->vars);
    pblock_nvinsert("auth-user", user, rq->vars);
    pblock_nninsert("nsafs_viceid", u.u_viceid, rq->vars);
    pblock_nninsert("nsafs_group0", u.u_cred->cr_groups[0], rq->vars);
    pblock_nninsert("nsafs_group1", u.u_cred->cr_groups[1], rq->vars);
    nsafs_login_store(user, cell, cksum, u.u_viceid, u.u_cred->cr_groups[0],
		      u.u_cred->cr_groups[1], expiration);

    return REQ_PROCEED;
}

/*
 * Name trans function for AFS
 *
 * Terminates the name translation step for files in AFS.
 * Puts the AFS pathname into path and into ppath request vars.
 */
NSAPI_PUBLIC int
nsafs_mount(pblock * pb, Session * sn, Request * rq)
{
    char *reqUri;
    char *newReqUri;
    int isAfsPath;

    /*
     * Get the URI from the request block
     */
    reqUri = pblock_findval("uri", rq->reqpb);
    if (reqUri == NULL) {
	return REQ_NOACTION;
    }

    if (uafs_afsPathName(reqUri) == NULL) {
	isAfsPath = 0;
    } else {
	isAfsPath = 1;
    }

    /*
     * If we have a new-url parameter then the new-url may be in AFS
     * if and only if the path is in AFS
     */
    request_header("new-url", &newReqUri, sn, rq);
    if (newReqUri != NULL) {
	if (util_uri_is_evil(newReqUri)) {
	    util_uri_parse(newReqUri);
	}
	if (uafs_afsPathName(newReqUri) != NULL) {
	    if (!isAfsPath) {
		/*
		 * We do not support moving files in or out of AFS
		 */
		log_error(LOG_INFORM, "nsafs", sn, rq, "new-URL not in AFS");
		protocol_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
		return REQ_ABORTED;
	    } else {
		pblock_nvinsert("newpath", newReqUri, rq->vars);
	    }
	} else if (isAfsPath) {
	    /*
	     * We do not support moving files in or out of AFS
	     */
	    log_error(LOG_INFORM, "nsafs", sn, rq, "new-URL not in AFS");
	    protocol_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
	    return REQ_ABORTED;
	}
    } else if (!isAfsPath) {
	return REQ_NOACTION;
    }

    /*
     * This is an AFS request
     */
    pblock_nvinsert("path", reqUri, rq->vars);
    pblock_nvinsert("ppath", reqUri, rq->vars);
    return REQ_PROCEED;
}

/*
 * Allow unauthorized users to access a specific directory in AFS
 */
NSAPI_PUBLIC int
nsafs_public(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *public;
    int pubLen;

    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    public = pblock_findval("public", pb);
    if (public == NULL) {
	return REQ_NOACTION;
    }

    /*
     * if the path is in AFS and is in the given directory then allow access
     */
    if (util_uri_is_evil(path)) {
	util_uri_parse(path);
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }
    if (util_uri_is_evil(public)) {
	util_uri_parse(public);
    }
    pubLen = strlen(public);
    if (strncmp(path, public, pubLen) != 0
	|| (path[pubLen] != '/' && path[pubLen] != '\0')) {
	return REQ_NOACTION;
    }
    if (pblock_findval("nsafs_viceid", rq->vars) == NULL
	|| pblock_findval("nsafs_group0", rq->vars) == NULL
	|| pblock_findval("nsafs_group1", rq->vars) == NULL) {
	pblock_nvinsert("nsafs_public", "TRUE", rq->vars);
    }
    return REQ_PROCEED;
}

/*
 * Identify a path that should be an authentication realm.
 */
NSAPI_PUBLIC int
nsafs_realm(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *realm;
    int pathLen;

    /*
     * Ignore matches for the current path once we find a realm
     * line that matches this path.
     */
    realm = pblock_findval("nsafs_realm", rq->vars);
    if (realm != NULL) {
	return REQ_NOACTION;
    }

    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (util_uri_is_evil(path)) {
	util_uri_parse(path);
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    realm = pblock_findval("path", pb);
    if (realm == NULL) {
	return REQ_NOACTION;
    }
    if (util_uri_is_evil(realm)) {
	util_uri_parse(realm);
    }

    pathLen = strlen(realm);
    if (strncmp(path, realm, pathLen) != 0
	|| (path[pathLen] != '/' && path[pathLen] != '\0')) {
	return REQ_NOACTION;
    }
    pblock_nvinsert("nsafs_realm", realm, rq->vars);
    return REQ_PROCEED;
}

/*
 * Check whether any path elements beneath the nolinks directory
 * are symbolic links. Return REQ_PROCEED if no links are found.
 */
int
nsafs_check_for_links(char *path, char *nolinks, pblock * pb, Session * sn,
		      Request * rq)
{
    int rc;
    int code;
    int dirLen;
    char *tmpPath;
    int allocSize;
    int pathLen;
    struct stat st;

    /*
     * Check each component of the path below the nolinks directory.
     */
    dirLen = strlen(nolinks);
    pathLen = strlen(path);
    if (pathLen < dirLen || strncmp(path, nolinks, dirLen) != 0
	|| (path[dirLen] != '/' && path[dirLen] != '\0')) {
	return REQ_PROCEED;
    }
    if (path[dirLen] == '/') {
	dirLen++;
    }

    allocSize = pathLen + 1;
    tmpPath = (char *)afs_osi_Alloc(allocSize);
    strcpy(tmpPath, path);
    while (tmpPath[pathLen] == '/' && pathLen > dirLen) {
	tmpPath[pathLen] = '\0';
	pathLen--;
    }
    while (pathLen > dirLen) {
	rc = uafs_lstat(tmpPath, &st);
	if (rc < 0) {
	    code = errno;
	    afs_osi_Free(tmpPath, allocSize);
	    return nsafs_error_check(code, NULL, pb, sn, rq);
	}
	if ((st.st_mode & S_IFMT) == S_IFLNK) {
	    afs_osi_Free(tmpPath, allocSize);
	    return nsafs_error_check(ENOENT, NULL, pb, sn, rq);
	}
	while (tmpPath[pathLen] != '/' && pathLen > dirLen) {
	    tmpPath[pathLen] = '\0';
	    pathLen--;
	}
	while (tmpPath[pathLen] == '/' && pathLen > dirLen) {
	    tmpPath[pathLen] = '\0';
	    pathLen--;
	}
    }
    afs_osi_Free(tmpPath, allocSize);
    return REQ_PROCEED;
}

/*
 * Deny access to symbolic links in a directory or its descendents.
 */
NSAPI_PUBLIC int
nsafs_nolinks(pblock * pb, Session * sn, Request * rq)
{
    int code;
    char *path;
    char *newPath;
    char *nolinks;
    char *viceid;
    char *group0;
    char *group1;

    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    nolinks = pblock_findval("nolinks", pb);
    if (nolinks == NULL) {
	return REQ_NOACTION;
    }
    newPath = pblock_findval("newpath", rq->vars);

    /*
     * if the path is in AFS and is in the nolinks directory then deny access
     * to any symbolic links below the nolinks directory.
     */
    if (util_uri_is_evil(path)) {
	util_uri_parse(path);
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }
    if (util_uri_is_evil(nolinks)) {
	util_uri_parse(nolinks);
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    code = nsafs_check_for_links(path, nolinks, pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }

    if (newPath != NULL) {
	if (util_uri_is_evil(newPath)) {
	    util_uri_parse(newPath);
	}
	if (uafs_afsPathName(newPath) == NULL) {
	    return REQ_NOACTION;
	}
	code = nsafs_check_for_links(newPath, nolinks, pb, sn, rq);
	if (code != REQ_PROCEED) {
	    return code;
	}
    }

    return REQ_NOACTION;
}

/*
 * Set the MIME type for files in AFS.
 */
NSAPI_PUBLIC int
nsafs_force_type(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *dflt;

    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    dflt = pblock_findval("type", pb);
    if (dflt == NULL) {
	return REQ_NOACTION;
    }

    /*
     * If the path is in AFS then set the nsafs_variable in the
     * request variables. The nsafs_error_check routine only
     * sends an Unauthorized error if this variable is not set.
     */
    if (util_uri_is_evil(path)) {
	util_uri_parse(path);
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    if (pblock_findval("content-type", rq->srvhdrs) == NULL) {
	pblock_nvinsert("content-type", dflt, rq->srvhdrs);
    }

    return REQ_PROCEED;
}

/*
 * Disable the Unauthorized response message so users never get
 * prompted for their name and password.
 */
NSAPI_PUBLIC int
nsafs_nocheck(pblock * pb, Session * sn, Request * rq)
{
    char *path;

    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }

    /*
     * If the path is in AFS then set the nsafs_variable in the
     * request variables. The nsafs_error_check routine only
     * sends an Unauthorized error if this variable is not set.
     */
    if (util_uri_is_evil(path)) {
	util_uri_parse(path);
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    if (pblock_findval("nsafs_nocheck", rq->vars) == NULL) {
	pblock_nvinsert("nsafs_nocheck", "TRUE", rq->vars);
    }

    /*
     * If this is a public directory then proceed, otherwise access
     * is forbidden
     */
    if (pblock_findval("nsafs_public", rq->vars) != NULL) {
	return REQ_PROCEED;
    }

    return nsafs_error_check(EACCES, NULL, pb, sn, rq);
}

/*
 * Require all requests for AFS files that are not explicitly made
 * public to be authenticated.
 */
NSAPI_PUBLIC int
nsafs_check(pblock * pb, Session * sn, Request * rq)
{
    char *path;

    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }

    /*
     * If the path is in AFS then require authentication
     */
    if (util_uri_is_evil(path)) {
	util_uri_parse(path);
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * If this is a public directory then proceed
     */
    if (pblock_findval("nsafs_public", rq->vars) != NULL) {
	return REQ_PROCEED;
    }

    if (pblock_findval("nsafs_viceid", rq->vars) == NULL
	|| pblock_findval("nsafs_group0", rq->vars) == NULL
	|| pblock_findval("nsafs_group1", rq->vars) == NULL) {
	return nsafs_error_check(EPERM, NULL, pb, sn, rq);
    }

    return REQ_PROCEED;
}

/*
 * Find index files for directories
 */
NSAPI_PUBLIC int
nsafs_find_index(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *indexNames;
    char *indexP;
    char *nameP;
    char *viceid;
    char *group0;
    char *group1;
    struct stat st;
    int rc;
    int code;
    int pos;

    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }

    indexNames = pblock_findval("index-names", pb);
    if (indexNames == NULL) {
	return REQ_NOACTION;
    }

    /*
     * Skip pathnames that don't end in a slash
     */
    if (*path == '\0' || path[strlen(path) - 1] != '/') {
	return REQ_NOACTION;
    }

    /*
     * If the path is a directory then check if the directory has
     * an index file.
     */
    if (util_uri_is_evil(path)) {
	util_uri_parse(path);
    }
    if (uafs_afsPathName(path) == NULL) {
	/*
	 * Look for index files in local file system
	 */
	rc = stat(path, &st);
	if (rc < 0) {
	    code = errno;
	    return nsafs_error_check(code, NULL, pb, sn, rq);
	}

	if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    return REQ_NOACTION;
	}

	indexP = nsafs_NameFromNames(NULL, indexNames, &pos);
	while (indexP != NULL) {
	    nameP = afs_osi_Alloc(strlen(path) + strlen(indexP) + 2);
	    sprintf(nameP, "%s/%s", path, indexP);
	    rc = stat(nameP, &st);
	    if (rc == 0 && (st.st_mode & S_IFMT) != S_IFDIR) {
		param_free(pblock_remove("path", rq->vars));
		pblock_nvinsert("path", nameP, rq->vars);
		afs_osi_Free(nameP, strlen(path) + strlen(indexP) + 2);
		afs_osi_Free(indexP, strlen(indexP) + 1);
		return REQ_PROCEED;
	    }
	    afs_osi_Free(nameP, strlen(path) + strlen(indexP) + 2);
	    indexP = nsafs_NameFromNames(indexP, indexNames, &pos);
	}
    } else {
	/*
	 * Check for group numbers in the request vars, otherwise use the
	 * defaults
	 */
	viceid = pblock_findval("nsafs_viceid", rq->vars);
	group0 = pblock_findval("nsafs_group0", rq->vars);
	group1 = pblock_findval("nsafs_group1", rq->vars);
	nsafs_set_id_from_strings(viceid, group0, group1);

	/*
	 * Look for index files in AFS
	 */
	rc = uafs_stat(path, &st);
	if (rc < 0) {
	    code = errno;
	    return nsafs_error_check(code, NULL, pb, sn, rq);
	}

	if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    return REQ_NOACTION;
	}

	indexP = nsafs_NameFromNames(NULL, indexNames, &pos);
	while (indexP != NULL) {
	    nameP = afs_osi_Alloc(strlen(path) + strlen(indexP) + 2);
	    sprintf(nameP, "%s/%s", path, indexP);
	    rc = uafs_stat(nameP, &st);
	    if (rc == 0 && (st.st_mode & S_IFMT) != S_IFDIR) {
		param_free(pblock_remove("path", rq->vars));
		pblock_nvinsert("path", nameP, rq->vars);
		afs_osi_Free(nameP, strlen(path) + strlen(indexP) + 2);
		afs_osi_Free(indexP, strlen(indexP) + 1);
		return REQ_PROCEED;
	    }
	    afs_osi_Free(nameP, strlen(path) + strlen(indexP) + 2);
	    indexP = nsafs_NameFromNames(indexP, indexNames, &pos);
	}
    }
    return REQ_NOACTION;
}

/*
 * Node in binary tree used to sort directory entries
 */
struct nsafs_tree {
    char *name;			/* directory member name */
    char *text;			/* directory index text */
    int allocLen;		/* Size of this allocated block */
    int textLen;		/* Size of the text string */
    int balance;		/* used to balance binary trees */
    struct nsafs_tree *left;
    struct nsafs_tree *right;
};

#ifdef NSAFS_TREE_DEBUG
/*
 * Validate that the given tree is a valid balanced binary tree
 */
int
nsafs_tree_check(struct nsafs_tree *root)
{
    int leftDepth;
    int rightDepth;
    int balance;

    if (root->left == NULL) {
	leftDepth = 0;
    } else {
	assert(strcmp(root->left->name, root->name) < 0);
	leftDepth = nsafs_tree_check(root->left);
    }
    if (root->right == NULL) {
	rightDepth = 0;
    } else {
	assert(strcmp(root->right->name, root->name) >= 0);
	rightDepth = nsafs_tree_check(root->right);
    }
    balance = rightDepth - leftDepth;
    assert(balance == root->balance);
    assert(balance >= -1);
    assert(balance <= 1);
    return (MAX(leftDepth, rightDepth) + 1);
}
#endif /* NSAFS_TREE_DEBUG */

/*
 * Insert a node into the balanced binary tree of directory entries.
 * rootP is the address of the parent's pointer to this node.
 * Returns the change in depth of this tree (0 or 1)
 */
int
nsafs_node_insert(struct nsafs_tree *newNode, struct nsafs_tree **rootP)
{
    struct nsafs_tree *thisNode;
    int delta;

    thisNode = *rootP;
    if (strcmp(newNode->name, thisNode->name) < 0) {
	/*
	 * Insert left
	 */
	if (thisNode->left == NULL) {
	    thisNode->left = newNode;
	    if (thisNode->right == NULL) {
		thisNode->balance = -1;
		return 1;
	    } else {
		thisNode->balance = 0;
		return 0;
	    }
	} else {
	    delta = nsafs_node_insert(newNode, &thisNode->left);
	    if (delta == 0) {
		return 0;
	    }
	    thisNode->balance -= delta;
	    if (thisNode->balance == -2) {
		/*
		 * rotate right
		 */
		if (thisNode->left->balance > 0) {
#ifdef NSAFS_TREE_DEBUG
		    printf("rotate left, line %d\n", __LINE__);
#endif /* NSAFS_TREE_DEBUG */
		    *rootP = thisNode->left->right;
		    if ((*rootP)->balance > 0) {
			thisNode->left->balance = -(*rootP)->balance;
			(*rootP)->balance = 0;
			thisNode->balance = 0;
		    } else {
			thisNode->left->balance = 0;
			thisNode->balance = -(*rootP)->balance;
			(*rootP)->balance = 0;
		    }
		    thisNode->left->right = (*rootP)->left;
		    (*rootP)->left = thisNode->left;
		    thisNode->left = (*rootP)->right;
		    (*rootP)->right = thisNode;
		} else {
#ifdef NSAFS_TREE_DEBUG
		    printf("rotate left, line %d\n", __LINE__);
#endif /* NSAFS_TREE_DEBUG */
		    *rootP = thisNode->left;
		    (*rootP)->balance = 0;
		    thisNode->balance = 0;
		    thisNode->left = (*rootP)->right;
		    (*rootP)->right = thisNode;
		}
		return 0;
	    } else if (thisNode->balance != 0) {
		return 1;
	    } else {
		return 0;
	    }
	}
    } else {
	/*
	 * Insert right
	 */
	if (thisNode->right == NULL) {
	    thisNode->right = newNode;
	    if (thisNode->left == NULL) {
		thisNode->balance = 1;
		return 1;
	    } else {
		thisNode->balance = 0;
		return 0;
	    }
	} else {
	    delta = nsafs_node_insert(newNode, &thisNode->right);
	    if (delta == 0) {
		return 0;
	    }
	    thisNode->balance += delta;
	    if (thisNode->balance == 2) {
		/*
		 * rotate left
		 */
		if (thisNode->right->balance < 0) {
#ifdef NSAFS_TREE_DEBUG
		    printf("rotate right, line %d\n", __LINE__);
#endif /* NSAFS_TREE_DEBUG */
		    *rootP = thisNode->right->left;
		    if ((*rootP)->balance < 0) {
			thisNode->right->balance = -(*rootP)->balance;
			(*rootP)->balance = 0;
			thisNode->balance = 0;
		    } else {
			thisNode->right->balance = 0;
			thisNode->balance = -(*rootP)->balance;
			(*rootP)->balance = 0;
		    }
		    thisNode->right->left = (*rootP)->right;
		    (*rootP)->right = thisNode->right;
		    thisNode->right = (*rootP)->left;
		    (*rootP)->left = thisNode;
		} else {
#ifdef NSAFS_TREE_DEBUG
		    printf("rotate right, line %d\n", __LINE__);
#endif /* NSAFS_TREE_DEBUG */
		    *rootP = thisNode->right;
		    (*rootP)->balance = 0;
		    thisNode->balance = 0;
		    thisNode->right = (*rootP)->left;
		    (*rootP)->left = thisNode;
		}
		return 0;
	    } else if (thisNode->balance != 0) {
		return 1;
	    } else {
		return 0;
	    }
	}
    }
}

/*
 * Allocate storage for a new directory entry, copy in the name and
 * text, and insert the entry into the balanced binary tree.
 */
void
nsafs_tree_insert(char *name, char *text, struct nsafs_tree **rootP)
{
    int nameLen;
    int textLen;
    int allocLen;
    struct nsafs_tree *newNode;

    /*
     * allocate storage, initialize the fields, and copy in the data
     */
    nameLen = strlen(name);
    textLen = strlen(text);
    allocLen = sizeof(struct nsafs_tree) + nameLen + textLen + 2;
    newNode = (struct nsafs_tree *)afs_osi_Alloc(allocLen);
    usr_assert(newNode != NULL);
    newNode->name = (char *)(newNode + 1);
    newNode->text = newNode->name + nameLen + 1;
    newNode->textLen = textLen;
    newNode->allocLen = allocLen;
    newNode->balance = 0;
    newNode->left = NULL;
    newNode->right = NULL;
    strcpy(newNode->name, name);
    strcpy(newNode->text, text);

    /*
     * If this is the first node, insert it here, otherwise call
     * nsafs_node_insert to insert the node into the balanced
     * binary tree.
     */
    if (*rootP == NULL) {
	*rootP = newNode;
    } else {
	nsafs_node_insert(newNode, rootP);
    }
#ifdef NSAFS_TREE_DEBUG
    nsafs_tree_check(*rootP);
#endif /* NSAFS_TREE_DEBUG */
}

/*
 * Transmit the contents of the tree
 */
int
nsafs_tree_send(SYS_NETFD sd, struct nsafs_tree *root, char *outbuf,
		int *buflen, int bufsize)
{
    int code;
    struct nsafs_tree *node;
    char *txtBuf;
    int txtLen;
    int len;

    /*
     * Recurse left, iterate right
     */
    node = root;
    while (node != NULL) {
	if (node->left != NULL) {
	    code = nsafs_tree_send(sd, node->left, outbuf, buflen, bufsize);
	    if (code == IO_ERROR) {
		return IO_ERROR;
	    }
	}
	txtLen = node->textLen;
	txtBuf = node->text;
	while (txtLen > 0) {
	    if (*buflen == bufsize) {
		code = net_write(sd, outbuf, bufsize);
		if (code == IO_ERROR) {
		    return IO_ERROR;
		}
		*buflen = 0;
	    }
	    len = MIN(txtLen, bufsize - *buflen);
	    memcpy((void *)(outbuf + *buflen), (void *)txtBuf, len);
	    *buflen += len;
	    txtBuf += len;
	    txtLen -= len;
	}
	node = node->right;
    }
    return 0;
}

/*
 * Free the binary tree and all data within
 */
void
nsafs_tree_free(struct nsafs_tree *root)
{
    struct nsafs_tree *node, *next;

    /*
     * Iterate left, recurse right
     */
    node = root;
    while (node != NULL) {
	if (node->right != NULL) {
	    nsafs_tree_free(node->right);
	}
	next = node->left;
	afs_osi_Free(node,
		     sizeof(struct nsafs_tree) + strlen(node->name) +
		     strlen(node->text) + 2);
	node = next;
    }
}

/*
 * Send the contents of an AFS directory, Simple directory format
 */
int
nsafs_send_directory(char *path, struct stat *stp, pblock * pb, Session * sn,
		     Request * rq)
{
    char *dirbuf;
    int buflen;
    char *filename;
    usr_DIR *dirp;
    struct usr_dirent *enp;
    int contentLength;
    int rc;
    int code;
    char *htmlHdr =
	"<HTML>\r\n<BODY>\r\n" "<TITLE>Index of %s</TITLE>\r\n"
	"<h1>Index of %s</h1>\r\n" "<PRE><HR>\r\n";
    char *htmlTrl = "</PRE></BODY></HTML>\r\n";
    struct nsafs_tree *root;

    /*
     * Set the content type to "text/html"
     */
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);

    /*
     * Build a binary tree of directory entries, and calculate the
     * length of our response message
     */
    dirp = uafs_opendir(path);
    if (dirp == NULL) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }
    root = NULL;
    contentLength = 0;
    dirbuf = afs_osi_Alloc(NSAFS_BUFFER_SIZE);
    while ((enp = uafs_readdir(dirp)) != NULL) {
	if (strcmp(enp->d_name, ".") == 0) {
	    continue;
	} else if (strcmp(enp->d_name, "..") == 0) {
	    filename = "Parent Directory";
	} else {
	    filename = enp->d_name;
	}
	sprintf(dirbuf, "<A HREF=\"%s%s\" NAME=\"%s\"> %s</A>\r\n", path,
		enp->d_name, filename, filename);
	contentLength += strlen(dirbuf);
	nsafs_tree_insert(enp->d_name, dirbuf, &root);
    }
    if (errno != 0) {
	code = errno;
	uafs_closedir(dirp);
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }
    rc = uafs_closedir(dirp);
    if (rc < 0) {
	code = errno;
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    /*
     * Calculate the length of the HTML headers and trailers,
     * set the content-length field in the reply header, and
     * start the reply message
     */
    sprintf(dirbuf, htmlHdr, path, path);
    contentLength += strlen(dirbuf) + strlen(htmlTrl);
    stp->st_size = contentLength;
    code = nsafs_set_finfo(sn, rq, stp);
    if (code != REQ_PROCEED) {
	nsafs_tree_free(root);
	protocol_status(sn, rq, PROTOCOL_NOT_MODIFIED, NULL);
	return code;
    }
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    code = protocol_start_response(sn, rq);
    if (code != REQ_PROCEED) {
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	return REQ_PROCEED;
    }

    /*
     * Send the HTML header, file data and HTML trailer
     */
    if (net_write(sn->csd, dirbuf, strlen(dirbuf)) == IO_ERROR) {
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	log_error(LOG_INFORM, "nsafs", sn, rq, "send_directory IO_ERROR");
	return REQ_EXIT;
    }
    buflen = 0;
    code = nsafs_tree_send(sn->csd, root, dirbuf, &buflen, NSAFS_BUFFER_SIZE);
    if (code == IO_ERROR) {
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	log_error(LOG_INFORM, "nsafs", sn, rq, "send_directory IO_ERROR");
	return REQ_EXIT;
    }
    nsafs_tree_free(root);
    if (buflen != 0) {
	code = net_write(sn->csd, dirbuf, buflen);
	if (code == IO_ERROR) {
	    afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	    log_error(LOG_INFORM, "nsafs", sn, rq, "send_directory IO_ERROR");
	    return REQ_EXIT;
	}
    }
    afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
    if (net_write(sn->csd, htmlTrl, strlen(htmlTrl)) == IO_ERROR) {
	return REQ_EXIT;
    }

    return REQ_PROCEED;
}

/*
 * Send the contents of an AFS file
 */
int
nsafs_send_file(char *path, struct stat *stp, pblock * pb, Session * sn,
		Request * rq)
{
    char *filebuf;
    int i;
    int rc;
    int fd;
    int code;

    /*
     * Make sure we can open the file before we send the response header
     */
    fd = uafs_open(path, O_RDONLY, 0);
    if (fd < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    /*
     * Add a content-length field to the response header and
     * begin the response message. We have already checked the
     * preconditions, so a return code other than REQ_PROCEED
     * means that this is a HEAD command.
     */
    code = nsafs_set_finfo(sn, rq, stp);
    if (code != REQ_PROCEED) {
	return code;
    }
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    code = protocol_start_response(sn, rq);
    if (code != REQ_PROCEED) {
	return REQ_PROCEED;
    }

    /*
     * Send the file contents
     */
    filebuf = afs_osi_Alloc(NSAFS_BUFFER_SIZE);
    while ((rc = uafs_read(fd, filebuf, NSAFS_BUFFER_SIZE)) > 0) {
	if (net_write(sn->csd, filebuf, rc) == IO_ERROR) {
	    afs_osi_Free(filebuf, NSAFS_BUFFER_SIZE);
	    uafs_close(fd);
	    log_error(LOG_INFORM, "nsafs", sn, rq, "send_file IO_ERROR");
	    return REQ_EXIT;
	}
    }
    if (rc < 0) {
	code = errno;
	uafs_close(fd);
	afs_osi_Free(filebuf, NSAFS_BUFFER_SIZE);
	log_error(LOG_FAILURE, "nsafs", sn, rq, "uafs_read, err=%d", code);
	return REQ_EXIT;
    }
    afs_osi_Free(filebuf, NSAFS_BUFFER_SIZE);
    rc = uafs_close(fd);
    if (rc < 0) {
	code = errno;
	log_error(LOG_FAILURE, "nsafs", sn, rq, "uafs_close, err=%d", code);
    }
    return REQ_PROCEED;
}

/*
 * Service function for AFS files and directories
 */
NSAPI_PUBLIC int
nsafs_send(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *dirPath;
    char *viceid;
    char *group0;
    char *group1;
    char *rangeHdr;
    struct stat st;
    int rc;
    int len;
    int code;

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Only service paths that are in AFS
     */
    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * We do not support content-range headers
     */
    request_header("content-range", &rangeHdr, sn, rq);
    if (rangeHdr != NULL) {
	protocol_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
	log_error(LOG_INFORM, "nsafs", sn, rq,
		  "content-range is not implemented");
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    /*
     * Get the file attributes
     */
    rc = uafs_stat(path, &st);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    /*
     * Check the request preconditions
     */
    code = nsafs_check_preconditions(&st, pb, sn, rq);
    if (code != REQ_PROCEED) {
	return code;
    }

    /*
     * Send the contents of files, and a formatted index of directories
     */
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
	len = strlen(path);
	dirPath = afs_osi_Alloc(len + 2);
	strcpy(dirPath, path);
	if (dirPath[len - 1] != '/') {
	    dirPath[len] = '/';
	    dirPath[len + 1] = '\0';
	}
	if (util_uri_is_evil(dirPath)) {
	    util_uri_parse(dirPath);
	}
	code = nsafs_send_directory(dirPath, &st, pb, sn, rq);
	afs_osi_Free(dirPath, len + 2);
    } else {
	code = nsafs_send_file(path, &st, pb, sn, rq);
    }
    return code;
}

/*
 * Service function to create new AFS files
 */
NSAPI_PUBLIC int
nsafs_put(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *viceid;
    char *group0;
    char *group1;
    char *rangeHdr;
    char *lengthHdr;
    char *filebuf;
    struct stat st;
    int i;
    int rc;
    int fd;
    int eof;
    int bytesRead;
    int bytesToRead;
    int code;
    int contentLength;
    int rspStatus;

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Only service paths that are in AFS
     */
    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * We do not support content-range headers
     */
    request_header("content-range", &rangeHdr, sn, rq);
    if (rangeHdr != NULL) {
	protocol_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
	log_error(LOG_INFORM, "nsafs", sn, rq,
		  "content-range is not implemented");
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    /*
     * Check for a content length header
     */
    request_header("content-length", &lengthHdr, sn, rq);
    if (lengthHdr != NULL) {
	contentLength = atoi(lengthHdr);
    } else {
	contentLength = -1;
    }

    /*
     * Get the file attributes
     */
    rc = uafs_stat(path, &st);
    if (rc < 0) {
	code = errno;
	if (code == ENOENT) {
	    rspStatus = PROTOCOL_CREATED;
	} else {
	    return nsafs_error_check(code, NULL, pb, sn, rq);
	}
    } else if ((st.st_mode & S_IFMT) == S_IFDIR) {
	return nsafs_error_check(EISDIR, NULL, pb, sn, rq);
    } else {
	rspStatus = PROTOCOL_OK;
    }

    fd = uafs_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    /*
     * Get the file contents
     */
    filebuf = afs_osi_Alloc(NSAFS_BUFFER_SIZE);
    eof = 0;
    while (!eof) {
	if (contentLength < 0) {
	    bytesToRead = NSAFS_BUFFER_SIZE;
	} else {
	    bytesToRead = MIN(contentLength, NSAFS_BUFFER_SIZE);
	    if (bytesToRead == 0) {
		eof = 1;
	    }
	}
	for (bytesRead = 0; !eof && bytesRead < bytesToRead; bytesRead++) {
	    rc = netbuf_getc(sn->inbuf);
	    if (rc == IO_EOF) {
		eof = 1;
	    } else if (rc == IO_ERROR) {
		code = errno;
		afs_osi_Free(filebuf, NSAFS_BUFFER_SIZE);
		uafs_close(fd);
		return nsafs_error_check(EIO, NULL, pb, sn, rq);
	    } else {
		filebuf[bytesRead] = rc;
	    }
	}
	if (bytesRead > 0) {
	    if (contentLength > 0) {
		contentLength -= bytesRead;
	    }
	    rc = uafs_write(fd, filebuf, bytesRead);
	    if (rc < 0) {
		code = errno;
		afs_osi_Free(filebuf, NSAFS_BUFFER_SIZE);
		uafs_close(fd);
		return nsafs_error_check(code, NULL, pb, sn, rq);
	    }
	}
    }
    afs_osi_Free(filebuf, NSAFS_BUFFER_SIZE);
    rc = uafs_close(fd);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }
    if (contentLength > 0) {
	log_error(LOG_FAILURE, "nsafs", sn, rq, "received partial contents");
	return REQ_EXIT;
    }

    pblock_nninsert("Content-Length", 0, rq->srvhdrs);
    protocol_status(sn, rq, rspStatus, NULL);
    code = protocol_start_response(sn, rq);
    return code;
}

/*
 * Service function to delete AFS files
 */
NSAPI_PUBLIC int
nsafs_delete(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *viceid;
    char *group0;
    char *group1;
    struct stat st;
    int rc;
    int code;

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Only service paths that are in AFS
     */
    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    /*
     * Get the file attributes
     */
    rc = uafs_lstat(path, &st);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    } else if ((st.st_mode & S_IFMT) == S_IFDIR) {
	log_error(LOG_INFORM, "nsafs", sn, rq, "Cannot DELETE directories");
	protocol_status(sn, rq, PROTOCOL_METHOD_NOT_ALLOWED, NULL);
	pblock_nvinsert("Allow", NSAFS_DIR_ALLOW, rq->srvhdrs);
	return REQ_ABORTED;
    }

    rc = uafs_unlink(path);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    pblock_nninsert("Content-Length", 0, rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    code = protocol_start_response(sn, rq);
    return code;
}

/*
 * Service function to create AFS directories
 */
NSAPI_PUBLIC int
nsafs_mkdir(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *viceid;
    char *group0;
    char *group1;
    struct stat st;
    int rc;
    int code;

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Only service paths that are in AFS
     */
    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    /*
     * Create the directory
     */
    rc = uafs_mkdir(path, 0755);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    pblock_nninsert("Content-Length", 0, rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    code = protocol_start_response(sn, rq);
    return code;
}

/*
 * Service function to delete AFS directories
 */
NSAPI_PUBLIC int
nsafs_rmdir(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *viceid;
    char *group0;
    char *group1;
    struct stat st;
    int rc;
    int code;

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Only service paths that are in AFS
     */
    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    /*
     * Get the file attributes, rmdir only work on directories.
     */
    rc = uafs_lstat(path, &st);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    /*
     * Call unlink to delete symbolic links to directories, and rmdir
     * to to delete directories.
     */
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
	rc = uafs_rmdir(path);
    } else if ((st.st_mode & S_IFMT) == S_IFLNK) {
	log_error(LOG_INFORM, "nsafs", sn, rq, "Cannot RMDIR links");
	protocol_status(sn, rq, PROTOCOL_METHOD_NOT_ALLOWED, NULL);
	pblock_nvinsert("Allow", NSAFS_LINK_ALLOW, rq->srvhdrs);
	return REQ_ABORTED;
    } else if ((st.st_mode & S_IFMT) != S_IFDIR) {
	log_error(LOG_INFORM, "nsafs", sn, rq, "Cannot RMDIR files");
	protocol_status(sn, rq, PROTOCOL_METHOD_NOT_ALLOWED, NULL);
	pblock_nvinsert("Allow", NSAFS_FILE_ALLOW, rq->srvhdrs);
	return REQ_ABORTED;
    }
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    pblock_nninsert("Content-Length", 0, rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    code = protocol_start_response(sn, rq);
    return code;
}

/*
 * Service function to rename AFS files and directories
 */
NSAPI_PUBLIC int
nsafs_move(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *newPath;
    char *viceid;
    char *group0;
    char *group1;
    struct stat st;
    int rc;
    int code;

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Only service paths that are in AFS
     */
    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }
    newPath = pblock_findval("newpath", rq->vars);
    if (newPath == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(newPath) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    /*
     * Rename the object
     */
    rc = uafs_rename(path, newPath);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    pblock_nninsert("Content-Length", 0, rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    code = protocol_start_response(sn, rq);
    return code;
}

/*
 * Send the index of an AFS directory
 */
int
nsafs_index(pblock * pb, Session * sn, Request * rq)
{
    char *path;
    char *viceid;
    char *group0;
    char *group1;
    char *dirbuf;
    char *tmpPath;
    int buflen;
    usr_DIR *dirp;
    struct usr_dirent *enp;
    int contentLength;
    int rc;
    int code;
    struct stat st;
    struct nsafs_tree *root;

    if (nsafs_init_done == 0) {
	nsafs_init_once();
    }

    /*
     * Only service paths that are in AFS
     */
    path = pblock_findval("path", rq->vars);
    if (path == NULL) {
	return REQ_NOACTION;
    }
    if (uafs_afsPathName(path) == NULL) {
	return REQ_NOACTION;
    }

    /*
     * Check for group numbers in the request vars, otherwise use the
     * defaults
     */
    viceid = pblock_findval("nsafs_viceid", rq->vars);
    group0 = pblock_findval("nsafs_group0", rq->vars);
    group1 = pblock_findval("nsafs_group1", rq->vars);
    nsafs_set_id_from_strings(viceid, group0, group1);

    /*
     * Get the file attributes, index does not work on symbolic links.
     */
    rc = uafs_lstat(path, &st);
    if (rc < 0) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }
    if ((st.st_mode & S_IFMT) == S_IFLNK) {
	log_error(LOG_INFORM, "nsafs", sn, rq, "Cannot INDEX links");
	protocol_status(sn, rq, PROTOCOL_METHOD_NOT_ALLOWED, NULL);
	pblock_nvinsert("Allow", NSAFS_LINK_ALLOW, rq->srvhdrs);
	return REQ_ABORTED;
    } else if ((st.st_mode & S_IFMT) != S_IFDIR) {
	log_error(LOG_INFORM, "nsafs", sn, rq, "Cannot INDEX files");
	protocol_status(sn, rq, PROTOCOL_METHOD_NOT_ALLOWED, NULL);
	pblock_nvinsert("Allow", NSAFS_FILE_ALLOW, rq->srvhdrs);
	return REQ_ABORTED;
    }

    /*
     * Set the content type to "text/html"
     */
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);

    /*
     * Build a binary tree of directory entries, and calculate the
     * length of our response message
     */
    dirp = uafs_opendir(path);
    if (dirp == NULL) {
	code = errno;
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }
    dirbuf = afs_osi_Alloc(NSAFS_BUFFER_SIZE);
    tmpPath = afs_osi_Alloc(NSAFS_MAX_PATH);
    root = NULL;
    contentLength = 0;
    while ((enp = uafs_readdir(dirp)) != NULL) {
	sprintf(tmpPath, "%s/%s", path, enp->d_name);
	rc = uafs_lstat(tmpPath, &st);
	if (rc < 0) {
	    continue;
	} else if ((st.st_mode & S_IFMT) == S_IFDIR) {
	    sprintf(dirbuf, "%s directory %u %u\r\n", enp->d_name, st.st_size,
		    st.st_mtime);
	} else if ((st.st_mode & S_IFMT) == S_IFLNK) {
	    sprintf(dirbuf, "%s link %u %u\r\n", enp->d_name, st.st_size,
		    st.st_mtime);
	} else {
	    sprintf(dirbuf, "%s unknown %u %u\r\n", enp->d_name, st.st_size,
		    st.st_mtime);
	}
	contentLength += strlen(dirbuf);
	nsafs_tree_insert(enp->d_name, dirbuf, &root);
    }
    if (errno != 0) {
	code = errno;
	uafs_closedir(dirp);
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	afs_osi_Free(tmpPath, NSAFS_MAX_PATH);
	nsafs_tree_free(root);
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }
    afs_osi_Free(tmpPath, NSAFS_MAX_PATH);
    rc = uafs_closedir(dirp);
    if (rc < 0) {
	code = errno;
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	return nsafs_error_check(code, NULL, pb, sn, rq);
    }

    /*
     * Set the content-length field in the reply header, and
     * start the reply message
     */
    pblock_nninsert("Content-Length", contentLength, rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    code = protocol_start_response(sn, rq);
    if (code != REQ_PROCEED) {
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	return REQ_PROCEED;
    }

    /*
     * Send the index
     */
    buflen = 0;
    code = nsafs_tree_send(sn->csd, root, dirbuf, &buflen, NSAFS_BUFFER_SIZE);
    if (code == IO_ERROR) {
	afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	nsafs_tree_free(root);
	log_error(LOG_INFORM, "nsafs", sn, rq, "send_index IO_ERROR");
	return REQ_EXIT;
    }
    nsafs_tree_free(root);
    if (buflen != 0) {
	code = net_write(sn->csd, dirbuf, buflen);
	if (code == IO_ERROR) {
	    afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);
	    log_error(LOG_INFORM, "nsafs", sn, rq, "send_index IO_ERROR");
	    return REQ_EXIT;
	}
    }
    afs_osi_Free(dirbuf, NSAFS_BUFFER_SIZE);

    return REQ_PROCEED;
}
