/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 2.3  90/08/31  16:11:49
 * Move permit_xprt.h.
 * 
 * Revision 2.2  90/08/20  10:05:54
 * Include permit_xprt.h.
 * Use stds.h.
 * 
 * Revision 2.1  90/08/07  18:51:45
 * Start with clean version to sync test and dev trees.
 * */
/* See RCS log for older history. */

#if defined(UKERNEL)
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/stds.h"
#include "../afs/pthread_glock.h"
#include "../des/des.h"
#include "../rx/rxkad.h"
#include "../rx/rx.h"
#include "../afs/cellconfig.h"
#include "../afs/keys.h"
#include "../afs/auth.h"
#include "../afs/pthread_glock.h"
#else /* defined(UKERNEL) */
#include <afs/param.h>
#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <des.h>
#include <rx/rxkad.h>
#include <rx/rx.h>
#include "cellconfig.h"
#include "keys.h"
#include "auth.h"
#endif /* defined(UKERNEL) */


extern afs_int32 afsconf_Authenticate();
extern int afsconf_GetKey();
extern struct rx_securityClass *rxkad_NewServerSecurityObject();
extern struct rx_securityClass *rxkad_NewClientSecurityObject();

/*
 * Note: it is necessary for the #include of permit_xprt.h occur
 * AFTER the above declaration of rxkad_NewClientSecurityObject() --
 * otherwise, depending on the version of permit_xprt.h that gets
 * included, there might be a syntax error.
 */

#if defined(UKERNEL)
#include "../afs/permit_xprt.h"
#else /* defined(UKERNEL) */
#include "../permit_xprt.h"
#endif /* defined(UKERNEL) */


/* return a null security object if nothing else can be done */
static afs_int32 QuickAuth(astr, aindex)
struct rx_securityClass **astr;
afs_int32 *aindex; {
    register struct rx_securityClass *tc;
    tc = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
    *astr = tc;
    *aindex = 0;
    return 0;
}

#if !defined(UKERNEL)
/* Return an appropriate security class and index */
afs_int32 afsconf_ServerAuth(adir, astr, aindex)
register struct afsconf_dir *adir;
struct rx_securityClass **astr;
afs_int32 *aindex; {
    register struct rx_securityClass *tclass;
    
    LOCK_GLOBAL_MUTEX
    tclass = (struct rx_securityClass *)
	rxkad_NewServerSecurityObject(0, adir, afsconf_GetKey, (char *) 0);
    if (tclass) {
	*astr = tclass;
	*aindex = 2;    /* kerberos security index */
	UNLOCK_GLOBAL_MUTEX
	return 0;
    }
    else {
	UNLOCK_GLOBAL_MUTEX
	return 2;
    }
}
#endif /* !defined(UKERNEL) */

static afs_int32 GenericAuth(adir, astr, aindex, enclevel)
struct afsconf_dir *adir;
struct rx_securityClass **astr;
afs_int32 *aindex; 
rxkad_level enclevel; {
    char tbuffer[256];
    struct ktc_encryptionKey key, session;
    struct rx_securityClass *tclass;
    afs_int32 kvno;
    afs_int32 ticketLen;
    struct timeval tv;
    Key_schedule schedule;
    register afs_int32 i, code;
    
    /* first, find the right key and kvno to use */
    code = afsconf_GetLatestKey(adir, &kvno, &key);
    if (code) {
	return QuickAuth(astr, aindex);
    }
    
    /* next create random session key, using key for seed to good random */
    des_init_random_number_generator (&key);
    code = des_random_key (&session);
    if (code) {
	return QuickAuth(astr, aindex);
    }
    
    /* now create the actual ticket */
    ticketLen = sizeof(tbuffer);
    code = tkt_MakeTicket(tbuffer, &ticketLen, &key, AUTH_SUPERUSER, "", "", 0,
			   0xffffffff, &session, 0, "afs", "");
    /* parms were buffer, ticketlen, key to seal ticket with, principal
	name, instance and cell, start time, end time, session key to seal
	in ticket, inet host, server name and server instance */
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* Next, we have ticket, kvno and session key, authenticate the connection.
     * We use a magic # instead of a constant because of basic compilation
     * order when compiling the system from scratch (rx/rxkad.h isn't installed
     * yet). */
    tclass = (struct rx_securityClass *)
	rxkad_NewClientSecurityObject(enclevel, &session, kvno,
				      ticketLen, tbuffer);
    *astr = tclass;
    *aindex = 2;    /* kerberos security index */
    return 0;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index
 */
afs_int32 afsconf_ClientAuth(adir, astr, aindex)
struct afsconf_dir *adir;
struct rx_securityClass **astr;
afs_int32 *aindex; {
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX
    rc = GenericAuth(adir, astr, aindex, rxkad_clear);
    UNLOCK_GLOBAL_MUTEX
    return rc;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index.  This one, unlike the above,
 * tells rxkad to encrypt the data, too.
 */
afs_int32 afsconf_ClientAuthSecure(adir, astr, aindex)
struct afsconf_dir *adir;
struct rx_securityClass **astr;
afs_int32 *aindex; {
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX
    rc = GenericAuth(adir, astr, aindex, rxkad_crypt);
    UNLOCK_GLOBAL_MUTEX
    return rc;
}

