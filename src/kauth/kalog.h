/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 */

typedef struct {
    time_t last_use;
    afs_int32 host;
} kalog_elt;

#define KALOG_DB_MODE 		0600

/* types of operations that we log */
#define LOG_GETTICKET 		0
#define LOG_CHPASSWD		1
#define	LOG_CRUSER		2
#define	LOG_AUTHENTICATE	3
#define	LOG_DELUSER		4
#define	LOG_SETFIELDS		5
#define	LOG_UNLOCK              6
#define	LOG_AUTHFAILED	        7
#define	LOG_TGTREQUEST		8

#ifdef AUTH_DBM_LOG
#ifdef AFS_LINUX20_ENV
#include <gdbm.h>
#define dbm_store	gdbm_store
#define dbm_firstkey	gdbm_firstkey
#define dbm_fetch	gdbm_fetch
#define dbm_close	gdbm_close
#define dbm_open(F, L, M)	gdbm_open(F, 512, L, M, 0)
#define afs_dbm_nextkey(d, k)	gdbm_nextkey(d, k)
#define DBM GDBM_FILE
#define DBM_REPLACE GDBM_REPLACE

#else /* AFS_LINUX20_ENV */
#include <ndbm.h>
#define afs_dbm_nextkey(d, k) dbm_nextkey(d)
#endif
#endif /* AUTH_DBM_LOG */

void ka_log(char *principal, char *instance, char *sprincipal, char *sinstance,
	    char *realm, int hostaddr, int type);
#ifdef AUTH_DBM_LOG
void kalog_log(char *principal, char *instance, char *sprincipal,
	       char *sinstance, char *realm, int hostaddr, int type);
#define KALOG(a,b,c,d,e,f,g) kalog_log(a,b,c,d,e,f,g)
#else
#define KALOG(a,b,c,d,e,f,g) ka_log(a,b,c,d,e,f,g)
#endif
