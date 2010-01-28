/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif


#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/stds.h"
#include "afs/pthread_glock.h"
#include "rx/rxkad.h"
#include "rx/rx.h"
#include "afs/cellconfig.h"
#include "afs/keys.h"
#include "afs/auth.h"
#include "afs/pthread_glock.h"
#include "des.h"
#include "des_prototypes.h"
#else /* defined(UKERNEL) */
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
#include <string.h>
#include <stdio.h>
#include <des.h>
#include <des_prototypes.h>
#include <rx/rxkad.h>
#include <rx/rx.h>
#include "cellconfig.h"
#include "keys.h"
#include "auth.h"
#endif /* defined(UKERNEL) */

/* return a null security object if nothing else can be done */
static afs_int32
QuickAuth(struct rx_securityClass **astr, afs_int32 *aindex)
{
    register struct rx_securityClass *tc;
    tc = rxnull_NewClientSecurityObject();
    *astr = tc;
    *aindex = 0;
    return 0;
}

#if !defined(UKERNEL)
/* Return an appropriate security class and index */
afs_int32
afsconf_ServerAuth(void *arock,
		   struct rx_securityClass **astr, 
		   afs_int32 *aindex)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) arock;
    register struct rx_securityClass *tclass;

    LOCK_GLOBAL_MUTEX;
    tclass = (struct rx_securityClass *)
	rxkad_NewServerSecurityObject(0, adir, afsconf_GetKey, NULL);
    if (tclass) {
	*astr = tclass;
	*aindex = 2;		/* kerberos security index */
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    } else {
	UNLOCK_GLOBAL_MUTEX;
	return 2;
    }
}
#endif /* !defined(UKERNEL) */

static afs_int32
GenericAuth(struct afsconf_dir *adir, 
	    struct rx_securityClass **astr, 
	    afs_int32 *aindex, 
	    rxkad_level enclevel)
{
    char tbuffer[256];
    struct ktc_encryptionKey key, session;
    struct rx_securityClass *tclass;
    afs_int32 kvno;
    afs_int32 ticketLen;
    register afs_int32 code;

    /* first, find the right key and kvno to use */
    code = afsconf_GetLatestKey(adir, &kvno, &key);
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* next create random session key, using key for seed to good random */
    des_init_random_number_generator(ktc_to_cblock(&key));
    code = des_random_key(ktc_to_cblock(&session));
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* now create the actual ticket */
    ticketLen = sizeof(tbuffer);
    memset(tbuffer, '\0', sizeof(tbuffer));
    code =
	tkt_MakeTicket(tbuffer, &ticketLen, &key, AUTH_SUPERUSER, "", "", 0,
		       0xffffffff, &session, 0, "afs", "");
    /* parms were buffer, ticketlen, key to seal ticket with, principal
     * name, instance and cell, start time, end time, session key to seal
     * in ticket, inet host, server name and server instance */
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* Next, we have ticket, kvno and session key, authenticate the connection.
     * We use a magic # instead of a constant because of basic compilation
     * order when compiling the system from scratch (rx/rxkad.h isn't installed
     * yet). */
    tclass = (struct rx_securityClass *)
	rxkad_NewClientSecurityObject(enclevel, &session, kvno, ticketLen,
				      tbuffer);
    *astr = tclass;
    *aindex = 2;		/* kerberos security index */
    return 0;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index
 */
afs_int32
afsconf_ClientAuth(void *arock, struct rx_securityClass ** astr,
		   afs_int32 * aindex)
{
    struct afsconf_dir * adir = (struct afsconf_dir *) arock;
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX;
    rc = GenericAuth(adir, astr, aindex, rxkad_clear);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index.  This one, unlike the above,
 * tells rxkad to encrypt the data, too.
 */
afs_int32
afsconf_ClientAuthSecure(void *arock, 
			 struct rx_securityClass **astr, 
			 afs_int32 *aindex)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) arock;
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX;
    rc = GenericAuth(adir, astr, aindex, rxkad_crypt);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}

/*!
 * Build a set of security classes suitable for a server accepting
 * incoming connections
 */
#if !defined(UKERNEL)
void
afsconf_BuildServerSecurityObjects(struct afsconf_dir *dir,
				   afs_uint32 flags,
			           struct rx_securityClass ***classes,
			           afs_int32 *numClasses)
{
    if (flags & AFSCONF_SEC_OBJS_RXKAD_CRYPT)
	*numClasses = 4;
    else
	*numClasses = 3;

    *classes = calloc(*numClasses, sizeof(**classes));

    (*classes)[0] = rxnull_NewServerSecurityObject();
    (*classes)[1] = NULL;
    (*classes)[2] = rxkad_NewServerSecurityObject(0, dir,
						  afsconf_GetKey, NULL);
    if (flags & AFSCONF_SEC_OBJS_RXKAD_CRYPT)
	(*classes)[3] = rxkad_NewServerSecurityObject(rxkad_crypt, dir,
						      afsconf_GetKey, NULL);
}
#endif
