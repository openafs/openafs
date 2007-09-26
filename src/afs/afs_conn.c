/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#ifdef AFS_RXK5
/* BEWARE: this code uses "u".  Must include heimdal krb5.h (u field name)
 * before libuafs afs/sysincludes.h (libuafs makes u a function.)
 */
#ifdef USING_K5SSL
#include <k5ssl.h>
#else
#include <krb5.h>
#endif
#endif

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#if !defined(AFS_LINUX20_ENV)
#include <net/if.h>
#endif
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN60_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#if     defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif

#ifdef AFS_RXK5
#include <rx/rxk5.h>
#include <afs/rxk5_tkt.h>
#endif

/* Exported variables */
afs_rwlock_t afs_xconn;		/* allocation lock for new things */
afs_rwlock_t afs_xinterface;	/* for multiple client address */
afs_int32 cryptall = 0;		/* encrypt all communications */


unsigned int VNOSERVERS = 0;
struct conn *
afs_Conn(register struct VenusFid *afid, register struct vrequest *areq,
	 afs_int32 locktype)
{
    u_short fsport = AFS_FSPORT;
    struct volume *tv;
    struct conn *tconn = NULL;
    struct srvAddr *lowp = NULL;
    struct unixuser *tu;
    int notbusy;
    int i;
    struct srvAddr *sa1p;

    AFS_STATCNT(afs_Conn);
    tv = afs_GetVolume(afid, areq, READ_LOCK);
    if (!tv) {
	if (areq) {
	    afs_FinalizeReq(areq);
	    areq->volumeError = 1;
	}
	return NULL;
    }

    if (tv->serverHost[0] && tv->serverHost[0]->cell) {
	fsport = tv->serverHost[0]->cell->fsport;
    } else {
	VNOSERVERS++;
    }

    /* First is always lowest rank, if it's up */
    if ((tv->status[0] == not_busy) && tv->serverHost[0]
	&& !(tv->serverHost[0]->addr->sa_flags & SRVR_ISDOWN))
	lowp = tv->serverHost[0]->addr;

    /* Otherwise we look at all of them. There are seven levels of
     * not_busy. This means we will check a volume seven times before it
     * is marked offline. Ideally, we only need two levels, but this
     * serves a second purpose of waiting some number of seconds before
     * the client decides the volume is offline (ie: a clone could finish
     * in this time).
     */
    for (notbusy = not_busy; (!lowp && (notbusy <= end_not_busy)); notbusy++) {
	for (i = 0; i < MAXHOSTS && tv->serverHost[i]; i++) {
	    if (tv->status[i] != notbusy) {
		if (tv->status[i] == rd_busy || tv->status[i] == rdwr_busy) {
		    if (!areq->busyCount)
			areq->busyCount++;
		} else if (tv->status[i] == offline) {
		    if (!areq->volumeError)
			areq->volumeError = VOLMISSING;
		}
		continue;
	    }
	    for (sa1p = tv->serverHost[i]->addr; sa1p; sa1p = sa1p->next_sa) {
		if (sa1p->sa_flags & SRVR_ISDOWN)
		    continue;
		if (!lowp || (lowp->sa_iprank > sa1p->sa_iprank))
		    lowp = sa1p;
	    }
	}
    }
    afs_PutVolume(tv, READ_LOCK);

    if (lowp) {
	tu = afs_GetUser(areq->uid, afid->Cell, SHARED_LOCK);
	tconn = afs_ConnBySA(lowp, fsport, afid->Cell, tu, 0 /*!force */ ,
			     1 /*create */ , locktype);

	afs_PutUser(tu, SHARED_LOCK);
    }

    return tconn;
}				/*afs_Conn */


struct conn *
afs_ConnBySA(struct srvAddr *sap, unsigned short aport, afs_int32 acell,
	     struct unixuser *tu, int force_if_down, afs_int32 create,
	     afs_int32 locktype)
{
    struct conn *tc = 0;
    struct rx_securityClass *csec;	/*Security class object */
    int isec;			/*Security index */
    int service;

    if (!sap || ((sap->sa_flags & SRVR_ISDOWN) && !force_if_down)) {
	/* sa is known down, and we don't want to force it.  */
	return NULL;
    }

    ObtainSharedLock(&afs_xconn, 15);
    for (tc = sap->conns; tc; tc = tc->next) {
	if (tc->user == tu && tc->port == aport) {
	    break;
	}
    }

    if (!tc && !create) {
	ReleaseSharedLock(&afs_xconn);
	return NULL;
    }

    if (!tc) {
	/* No such connection structure exists.  Create one and splice it in.
	 * Make sure the server record has been marked as used (for the purposes
	 * of calculating up & down times, it's now considered to be an
	 * ``active'' server).  Also make sure the server's lastUpdateEvalTime
	 * gets set, marking the time of its ``birth''.
	 */
	UpgradeSToWLock(&afs_xconn, 37);
	tc = (struct conn *)afs_osi_Alloc(sizeof(struct conn));
	memset((char *)tc, 0, sizeof(struct conn));

	tc->user = tu;
	tc->port = aport;
	tc->srvr = sap;
	tc->refCount = 0;	/* bumped below */
	tc->forceConnectFS = 1;
	tc->id = (struct rx_connection *)0;
	tc->next = sap->conns;
	sap->conns = tc;
	afs_ActivateServer(sap);

	ConvertWToSLock(&afs_xconn);
    }
    tc->refCount++;

    if (tu->states & UTokensBad) {
	/* we may still have an authenticated RPC connection here,
	 * we'll have to create a new, unauthenticated, connection.
	 * Perhaps a better way to do this would be to set
	 * conn->forceConnectFS on all conns when the token first goes
	 * bad, but that's somewhat trickier, due to locking
	 * constraints (though not impossible).
	 */
	if (tc->id && (rx_SecurityClassOf(tc->id) != 0)) {
	    tc->forceConnectFS = 1;	/* force recreation of connection */
	}
	tu->vid = UNDEFVID;	/* forcibly disconnect the authentication info */
    }

    if (tc->forceConnectFS) {
	UpgradeSToWLock(&afs_xconn, 38);
	csec = (struct rx_securityClass *)0;
	if (tc->id) {
	    AFS_GUNLOCK();
	    rx_DestroyConnection(tc->id);
	    AFS_GLOCK();
	}
	/*
	 * Stupid hack to determine if using vldb service or file system
	 * service.
	 */
	if (aport == sap->server->cell->vlport)
	    service = 52;
	else
	    service = 1;
	isec = 0;
	if (tu->vid != UNDEFVID) {
	    int level;

	    isec = 2;
	    if (cryptall) {
#if 0
		/* this is a myth.  See note in viced/viced.c */
		if (service == 1) isec = 3;
#endif
		level = rxkad_crypt;
	    } else {
		level = rxkad_clear;
	    }
#ifdef AFS_RXK5
	    /* rxk5_clear, rxk5_auth, and rxk5_crypt have the same values as
	     rxkad_clear, rxkad_auth, and rxkad_crypt */
	    if(tu->rxk5creds) {
	      rxk5_creds *rxk5creds = (rxk5_creds*) tu->rxk5creds;
	      isec = 5;
	      if(level == rxkad_clear) 
		level = rxkad_auth;
	      csec = rxk5_NewClientSecurityObject(level, rxk5creds->k5creds, 0);
	    } else {
#endif
	    /* kerberos tickets on channel 2 */
	    csec = rxkad_NewClientSecurityObject(level,
                                                 (struct ktc_encryptionKey *)tu->ct.HandShakeKey,
						 /* kvno */
						 tu->ct.AuthHandle, tu->stLen,
						 tu->stp);
#ifdef AFS_RXK5
	}
#endif
	}
	if (isec == 0)
	    csec = rxnull_NewClientSecurityObject();
	AFS_GUNLOCK();
	tc->id = rx_NewConnection(sap->sa_ip, aport, service, csec, isec);
	AFS_GLOCK();
	if (service == 52) {
	    rx_SetConnHardDeadTime(tc->id, afs_rx_harddead);
	}

	tc->forceConnectFS = 0;	/* apparently we're appropriately connected now */
	if (csec)
	    rxs_Release(csec);
	ConvertWToSLock(&afs_xconn);
    }

    ReleaseSharedLock(&afs_xconn);
    return tc;
}

/*
 * afs_ConnByHost
 *
 * forceConnectFS is set whenever we must recompute the connection. UTokensBad
 * is true only if we know that the tokens are bad.  We thus clear this flag
 * when we get a new set of tokens..
 * Having force... true and UTokensBad true simultaneously means that the tokens
 * went bad and we're supposed to create a new, unauthenticated, connection.
 */
struct conn *
afs_ConnByHost(struct server *aserver, unsigned short aport, afs_int32 acell,
	       struct vrequest *areq, int aforce, afs_int32 locktype)
{
    struct unixuser *tu;
    struct conn *tc = 0;
    struct srvAddr *sa = 0;

    AFS_STATCNT(afs_ConnByHost);
/* 
  1.  look for an existing connection
  2.  create a connection at an address believed to be up
      (if aforce is true, create a connection at the first address)
*/

    tu = afs_GetUser(areq->uid, acell, SHARED_LOCK);

    for (sa = aserver->addr; sa; sa = sa->next_sa) {
	tc = afs_ConnBySA(sa, aport, acell, tu, aforce,
			  0 /*don't create one */ ,
			  locktype);
	if (tc)
	    break;
    }

    if (!tc) {
	for (sa = aserver->addr; sa; sa = sa->next_sa) {
	    tc = afs_ConnBySA(sa, aport, acell, tu, aforce,
			      1 /*create one */ ,
			      locktype);
	    if (tc)
		break;
	}
    }

    afs_PutUser(tu, SHARED_LOCK);
    return tc;

}				/*afs_ConnByHost */


struct conn *
afs_ConnByMHosts(struct server *ahosts[], unsigned short aport,
		 afs_int32 acell, register struct vrequest *areq,
		 afs_int32 locktype)
{
    register afs_int32 i;
    register struct conn *tconn;
    register struct server *ts;

    /* try to find any connection from the set */
    AFS_STATCNT(afs_ConnByMHosts);
    for (i = 0; i < MAXCELLHOSTS; i++) {
	if ((ts = ahosts[i]) == NULL)
	    break;
	tconn = afs_ConnByHost(ts, aport, acell, areq, 0, locktype);
	if (tconn) {
	    return tconn;
	}
    }
    return NULL;

}				/*afs_ConnByMHosts */


void
afs_PutConn(register struct conn *ac, afs_int32 locktype)
{
    AFS_STATCNT(afs_PutConn);
    ac->refCount--;
}				/*afs_PutConn */


/* for multi homed clients, an RPC may timeout because of a
client network interface going down. We need to reopen new 
connections in this case
*/
void
ForceNewConnections(struct srvAddr *sap)
{
    struct conn *tc = 0;

    if (!sap)
	return;			/* defensive check */

    ObtainWriteLock(&afs_xconn, 413);
    for (tc = sap->conns; tc; tc = tc->next)
	tc->forceConnectFS = 1;
    ReleaseWriteLock(&afs_xconn);
}
