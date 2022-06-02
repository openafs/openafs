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


#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#if !defined(AFS_LINUX_ENV)
#include <net/if.h>
#endif
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX_ENV) && !defined(AFS_DARWIN_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if	defined(AFS_SUN5_ENV)
#include <inet/led.h>
#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#endif

/* Exported variables */
afs_rwlock_t afs_xconn;		/* allocation lock for new things */
afs_rwlock_t afs_xinterface;	/* for multiple client address */
afs_int32 cryptall = 1;		/* encrypt all communications */

/* some connection macros */

/* a constructor */
#define new_conn_vector(xcv) \
do { \
	xcv = (struct sa_conn_vector *) \
	afs_osi_Alloc(sizeof(struct sa_conn_vector)); \
	if (xcv) { \
		memset((char *)xcv, 0, sizeof(struct sa_conn_vector)); \
	} \
} while (0);

/* select a connection to return (if no connection has lower utilization
 * than any other) */
#define conn_vec_select_conn(xcv, bix, conn) \
do { \
    (bix) = ((xcv)->select_index)++ % CVEC_LEN; \
    (conn) = &((xcv)->cvec[bix]); \
} while (0);

#define struct_conn(s) ((struct afs_conn *)(s))

#define REPORT_CONNECTIONS_ISSUED 0 /* enable to see utilization */

/**
 * Find a connection with call slots available, allocating one
 * if nothing is available and we find an allocated slot
 * @param xcv  A connection vector
 * @param create  If set, a new connection may be created
 */
static struct afs_conn *
find_preferred_connection(struct sa_conn_vector *xcv, int create)
{
    afs_int32 cix, bix;
    struct afs_conn *tc = NULL;

    bix = -1;
    for(cix = 0; cix < CVEC_LEN; ++cix) {
        tc = &(xcv->cvec[cix]);
        if (!tc->id) {
            if (create) {
                tc->parent = xcv;
                tc->forceConnectFS = 1;
                tc->activated = 1;
                bix = cix;
                break;
            } /* create */
        } else {
            if (tc->refCount < (RX_MAXCALLS-1)) {
                bix = cix;
                goto f_conn;
            } else if (cix == (CVEC_LEN-1))
                conn_vec_select_conn(xcv, bix, tc);
        } /* tc->id */
    } /* for cix < CVEC_LEN */

    if (bix < 0) {
        tc = NULL;
        goto out;
    }

f_conn:
    tc->refCount++;
    xcv->refCount++;

#if REPORT_CONNECTIONS_ISSUED
    afs_warn("Issuing conn %d refCount=%d parent refCount=%d\n", bix,
             tc->refCount, xcv->refCount);
#endif

out:
    return (tc);

}        /* find_preferred_connection */


/**
 * Release all connections for unix user xu at server xs
 * @param xu
 * @param xs
 */
static void
release_conns_user_server(struct unixuser *xu, struct server *xs)
{
    int cix, glocked;
    struct srvAddr *sa;
    struct afs_conn *tc;
    struct sa_conn_vector *tcv, **lcv, *tcvn;
    for (sa = (xs)->addr; sa; sa = sa->next_sa) {
        lcv = &sa->conns;
        for (tcv = *lcv; tcv; lcv = &tcv->next, tcv = *lcv) {
            if (tcv->user == (xu) && tcv->refCount == 0) {
                *lcv = tcv->next;
                /* our old friend, the GLOCK */
                glocked = ISAFS_GLOCK();
                if (glocked)
                    AFS_GUNLOCK();
                for(cix = 0; cix < CVEC_LEN; ++cix) {
                    tc = &(tcv->cvec[cix]);
                    if (tc->activated) {
                        rx_SetConnSecondsUntilNatPing(tc->id, 0);
                        rx_DestroyConnection(tc->id);
			/* find another eligible connection */
			if (sa->natping == tc) {
			    int cin;
			    struct afs_conn *tcn;
			    sa->natping = NULL;
			    for (tcvn = sa->conns; tcvn; tcvn = tcvn->next) {
				if (sa->natping != NULL)
				    break;
				if (tcvn == tcv)
				    continue;
				for(cin = 0; cin < CVEC_LEN; ++cin) {
				    tcn = &(tcvn->cvec[cin]);
				    if (tcn->activated) {
					rx_SetConnSecondsUntilNatPing(tcn->id, 20);
					sa->natping = tcn;
					break;
				    }
				}
			    }
			}
                    }
                }
                if (glocked)
                    AFS_GLOCK();
                afs_osi_Free(tcv, sizeof(struct sa_conn_vector));
                break;    /* at most one instance per server */
            } /*Found unreferenced connection for user */
        }
    } /*For each connection on the server */

}        /* release_conns_user_server */


static void
release_conns_vector(struct sa_conn_vector *tcv)
{
    int cix, glocked;
    struct afs_conn *tc;
    struct sa_conn_vector *next;

    while (tcv != NULL) {
	next = tcv->next;

        /* you know it, you love it, the GLOCK */
        glocked = ISAFS_GLOCK();
        if (glocked)
            AFS_GUNLOCK(); \
        for(cix = 0; cix < CVEC_LEN; ++cix) {
            tc = &(tcv->cvec[cix]);
            if (tc->activated) {
                rx_SetConnSecondsUntilNatPing(tc->id, 0);
                rx_DestroyConnection(tc->id);
		if (tcv->srvr->natping == tc)
		    tcv->srvr->natping = NULL;
            }
        }
        if (glocked)
            AFS_GLOCK();
        afs_osi_Free(tcv, sizeof(struct sa_conn_vector));
	tcv = next;
    }

}        /* release_conns_vector */


unsigned int VNOSERVERS = 0;

/**
 * Pick a security object to use for a connection to a given server,
 * by a given user
 *
 * @param[in] conn
 *	The AFS connection for which the security object is required
 * @param[out] secLevel
 *	The security level of the returned object
 *
 * @return
 *	An rx security object. This function is guaranteed to return
 *	an object, although that object may be rxnull (with a secLevel
 *	of 0)
 */
static struct rx_securityClass *
afs_pickSecurityObject(struct afs_conn *conn, int *secLevel)
{
    struct rx_securityClass *secObj = NULL;
    union tokenUnion *token;

    /* Do we have tokens ? */
    if (conn->parent->user->states & UHasTokens) {
	token = afs_FindToken(conn->parent->user->tokens, RX_SECIDX_KAD);
	if (token) {
	    *secLevel = RX_SECIDX_KAD;
	    /* kerberos tickets on channel 2 */
	    secObj = rxkad_NewClientSecurityObject(
			 cryptall ? rxkad_crypt : rxkad_clear,
                         (struct ktc_encryptionKey *)
			       token->rxkad.clearToken.HandShakeKey,
		         token->rxkad.clearToken.AuthHandle,
		         token->rxkad.ticketLen, token->rxkad.ticket);
	    /* We're going to use this token, so populate the viced */
	    conn->parent->user->viceId = token->rxkad.clearToken.ViceId;
	}
     }
     if (secObj == NULL) {
	*secLevel = 0;
	secObj = rxnull_NewClientSecurityObject();
     }

     return secObj;
}


/**
 * Try setting up a connection to the server containing the specified fid.
 * Gets the volume, checks if it's up and does the connection by server address.
 *
 * @param afid
 * @param areq Request filled in by the caller.
 * @param locktype Type of lock that will be used.
 *
 * @return The conn struct, or NULL.
 */
struct afs_conn *
afs_Conn(struct VenusFid *afid, struct vrequest *areq,
	 afs_int32 locktype, struct rx_connection **rxconn)
{
    u_short fsport = AFS_FSPORT;
    struct volume *tv;
    struct afs_conn *tconn = NULL;
    struct srvAddr *lowp = NULL;
    struct unixuser *tu;
    int notbusy;
    int i;
    struct srvAddr *sa1p;
    afs_int32 replicated = -1; /* a single RO will increment to 0 */

    *rxconn = NULL;

    AFS_STATCNT(afs_Conn);
    /* Get fid's volume. */
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
	&& tv->serverHost[0]->addr
	&& !(tv->serverHost[0]->addr->sa_flags & SRVR_ISDOWN) &&
	!(((areq->idleError > 0) || (areq->tokenError > 0))
	  && (areq->skipserver[0] == 1)))
	lowp = tv->serverHost[0]->addr;

    /* Otherwise we look at all of them. There are seven levels of
     * not_busy. This means we will check a volume seven times before it
     * is marked offline. Ideally, we only need two levels, but this
     * serves a second purpose of waiting some number of seconds before
     * the client decides the volume is offline (ie: a clone could finish
     * in this time).
     */
    for (notbusy = not_busy; (!lowp && (notbusy <= end_not_busy)); notbusy++) {
	for (i = 0; i < AFS_MAXHOSTS && tv->serverHost[i]; i++) {
	    if (tv->states & VRO)
		replicated++;
	    if (((areq->tokenError > 0)||(areq->idleError > 0))
		&& (areq->skipserver[i] == 1))
		continue;
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
    if ((replicated == -1) && (tv->states & VRO)) {
	for (i = 0; i < AFS_MAXHOSTS && tv->serverHost[i]; i++) {
	    if (tv->states & VRO)
		replicated++;
	}
    } else
	replicated = 0;

    afs_PutVolume(tv, READ_LOCK);

    if (lowp) {
	tu = afs_GetUser(areq->uid, afid->Cell, SHARED_LOCK);
	tconn = afs_ConnBySA(lowp, fsport, afid->Cell, tu, 0 /*!force */ ,
			     1 /*create */ , locktype, replicated, rxconn);

	afs_PutUser(tu, SHARED_LOCK);
    }

    return tconn;
}				/*afs_Conn */


/**
 * Connects to a server by it's server address.
 *
 * @param sap Server address.
 * @param aport Server port.
 * @param acell
 * @param tu Connect as this user.
 * @param force_if_down
 * @param create
 * @param replicated
 * @param locktype Specifies type of lock to be used for this function.
 *
 * @return The new connection.
 */
struct afs_conn *
afs_ConnBySA(struct srvAddr *sap, unsigned short aport, afs_int32 acell,
	     struct unixuser *tu, int force_if_down, afs_int32 create,
	     afs_int32 locktype, afs_int32 replicated,
	     struct rx_connection **rxconn)
{
    int glocked, foundvec;
    struct afs_conn *tc = NULL;
    struct sa_conn_vector *tcv = NULL;
    struct rx_securityClass *csec; /*Security class object */
    int isec; /*Security index */
    int service;
    int isrep = (replicated > 0)?CONN_REPLICATED:0;

    *rxconn = NULL;

    if (!sap || ((sap->sa_flags & SRVR_ISDOWN) && !force_if_down)) {
	/* sa is known down, and we don't want to force it.  */
	return NULL;
    }

    /* find cached connection */
    ObtainSharedLock(&afs_xconn, 15);
    foundvec = 0;
    for (tcv = sap->conns; tcv; tcv = tcv->next) {
        if (tcv->user == tu && tcv->port == aport &&
	    (isrep == (tcv->flags & CONN_REPLICATED))) {
            /* return most eligible conn */
            if (!foundvec)
                foundvec = 1;
            UpgradeSToWLock(&afs_xconn, 37);
            tc = find_preferred_connection(tcv, create);
            ConvertWToSLock(&afs_xconn);
            break;
        }
    }

    if (!tc && !create) {
        /* Not found and can't create a new one. */
        ReleaseSharedLock(&afs_xconn);
        return NULL;
    }

    if (AFS_IS_DISCONNECTED && !AFS_IN_SYNC) {
        afs_warnuser("afs_ConnBySA: disconnected\n");
        ReleaseSharedLock(&afs_xconn);
        return NULL;
    }

    if (!foundvec && create) {
	/* No such connection vector exists.  Create one and splice it in.
	 * Make sure the server record has been marked as used (for the purposes
	 * of calculating up & down times, it's now considered to be an
	 * ``active'' server).  Also make sure the server's lastUpdateEvalTime
	 * gets set, marking the time of its ``birth''.
	 */
	UpgradeSToWLock(&afs_xconn, 37);
        new_conn_vector(tcv);

        tcv->user = tu;
        tcv->port = aport;
        tcv->srvr = sap;
        tcv->next = sap->conns;
	if (isrep)
	    tcv->flags |= CONN_REPLICATED;
        sap->conns = tcv;

        /* all struct afs_conn ptrs come from here */
        tc = find_preferred_connection(tcv, create);

	afs_ActivateServer(sap);

	ConvertWToSLock(&afs_xconn);
    } /* end of if (!tcv) */

    if (!tc) {
        /* Not found and no alternatives. */
        ReleaseSharedLock(&afs_xconn);
        return NULL;
    }

    if (tc->refCount > 10000) {
	static int warned;
	if (!warned) {
	    warned = 1;
	    afs_warn("afs: Very high afs_conn refCount detected (conn %p, count %d)\n",
	             tc, (int)tc->refCount);
	    afs_warn("afs: Trying to continue, but this may indicate an issue\n");
	    afs_warn("afs: that may eventually crash the machine. Please file\n");
	    afs_warn("afs: a bug report.\n");
	}
    }

    if (tu->states & UTokensBad) {
	/* we may still have an authenticated RPC connection here,
	 * we'll have to create a new, unauthenticated, connection.
	 * Perhaps a better way to do this would be to set
	 * conn->forceConnectFS on all conns when the token first goes
	 * bad, but that's somewhat trickier, due to locking
	 * constraints (though not impossible).
	 */
	if (tc->id && (rx_SecurityClassOf(tc->id) != RX_SECIDX_NULL)) {
	    tc->forceConnectFS = 1;	/* force recreation of connection */
	}
	tu->states &= ~UHasTokens;      /* remove the authentication info */
    }

    glocked = ISAFS_GLOCK();
    if (tc->forceConnectFS) {
	UpgradeSToWLock(&afs_xconn, 38);
	if (tc->id) {
	    if (sap->natping == tc)
		sap->natping = NULL;
	    if (glocked)
                AFS_GUNLOCK();
            rx_SetConnSecondsUntilNatPing(tc->id, 0);
            rx_DestroyConnection(tc->id);
	    if (glocked)
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

	csec = afs_pickSecurityObject(tc, &isec);

	if (glocked)
            AFS_GUNLOCK();
	tc->id = rx_NewConnection(sap->sa_ip, aport, service, csec, isec);
	if (glocked)
            AFS_GLOCK();
	if (service == 52) {
	    rx_SetConnHardDeadTime(tc->id, afs_rx_harddead);
	}

        /* Setting idle dead time to non-zero activates idle-dead
	 * RX_CALL_TIMEOUT errors. */
	if (isrep)
	    rx_SetConnIdleDeadTime(tc->id, afs_rx_idledead_rep);
	else
	    rx_SetConnIdleDeadTime(tc->id, afs_rx_idledead);

	/*
	 * Only do this for one connection
	 */
	if ((service != 52) && (sap->natping == NULL)) {
	    sap->natping = tc;
	    rx_SetConnSecondsUntilNatPing(tc->id, 20);
	}

	tc->forceConnectFS = 0;	/* apparently we're appropriately connected now */
	if (csec)
	    rxs_Release(csec);
	ConvertWToSLock(&afs_xconn);
    } /* end of if (tc->forceConnectFS)*/

    *rxconn = tc->id;
    rx_GetConnection(*rxconn);

    ReleaseSharedLock(&afs_xconn);
    return tc;
}

/**
 * forceConnectFS is set whenever we must recompute the connection. UTokensBad
 * is true only if we know that the tokens are bad.  We thus clear this flag
 * when we get a new set of tokens..
 * Having force... true and UTokensBad true simultaneously means that the tokens
 * went bad and we're supposed to create a new, unauthenticated, connection.
 *
 * @param aserver Server to connect to.
 * @param aport Connection port.
 * @param acell The cell where all of this happens.
 * @param areq The request.
 * @param aforce Force connection?
 * @param locktype Type of lock to be used.
 * @param replicated
 *
 * @return The established connection.
 */
struct afs_conn *
afs_ConnByHost(struct server *aserver, unsigned short aport, afs_int32 acell,
	       struct vrequest *areq, int aforce, afs_int32 locktype,
	       afs_int32 replicated, struct rx_connection **rxconn)
{
    struct unixuser *tu;
    struct afs_conn *tc = NULL;
    struct srvAddr *sa = NULL;

    *rxconn = NULL;

    AFS_STATCNT(afs_ConnByHost);

    if (AFS_IS_DISCONNECTED && !AFS_IN_SYNC) {
        afs_warnuser("afs_ConnByHost: disconnected\n");
        return NULL;
    }

/*
  1.  look for an existing connection
  2.  create a connection at an address believed to be up
      (if aforce is true, create a connection at the first address)
*/

    tu = afs_GetUser(areq->uid, acell, SHARED_LOCK);

    for (sa = aserver->addr; sa; sa = sa->next_sa) {
	tc = afs_ConnBySA(sa, aport, acell, tu, aforce,
			  0 /*don't create one */ ,
			  locktype, replicated, rxconn);
	if (tc)
	    break;
    }

    if (!tc) {
	for (sa = aserver->addr; sa; sa = sa->next_sa) {
	    tc = afs_ConnBySA(sa, aport, acell, tu, aforce,
			      1 /*create one */ ,
			      locktype, replicated, rxconn);
	    if (tc)
		break;
	}
    }

    afs_PutUser(tu, SHARED_LOCK);
    return tc;

}				/*afs_ConnByHost */


/**
 * Connect by multiple hosts.
 * Try to connect to one of the hosts from the ahosts array.
 *
 * @param ahosts Multiple hosts to connect to.
 * @param aport Connection port.
 * @param acell The cell where all of this happens.
 * @param areq The request.
 * @param locktype Type of lock to be used.
 * @param replicated
 *
 * @return The established connection or NULL.
 */
struct afs_conn *
afs_ConnByMHosts(struct server *ahosts[], unsigned short aport,
		 afs_int32 acell, struct vrequest *areq,
		 afs_int32 locktype, afs_int32 replicated,
		 struct rx_connection **rxconn)
{
    afs_int32 i;
    struct afs_conn *tconn;
    struct server *ts;

    *rxconn = NULL;

    /* try to find any connection from the set */
    AFS_STATCNT(afs_ConnByMHosts);
    for (i = 0; i < AFS_MAXCELLHOSTS; i++) {
	if ((ts = ahosts[i]) == NULL)
	    break;
	tconn = afs_ConnByHost(ts, aport, acell, areq, 0, locktype,
			       replicated, rxconn);
	if (tconn) {
	    return tconn;
	}
    }
    return NULL;

}				/*afs_ConnByMHosts */


/**
 * Decrement reference count to this connection.
 * @param ac
 * @param locktype
 */
void
afs_PutConn(struct afs_conn *ac, struct rx_connection *rxconn,
            afs_int32 locktype)
{
    AFS_STATCNT(afs_PutConn);
    ac->refCount--;
    if (ac->refCount < 0) {
	osi_Panic("afs_PutConn: refcount imbalance 0x%lx %d",
	          (unsigned long)(uintptrsz)ac, (int)ac->refCount);
    }
    ac->parent->refCount--;
    rx_PutConnection(rxconn);
}				/*afs_PutConn */


/**
 * Free up a connection vector, allowing, eg, code in afs_user.c
 * to ignore how connections are stored/pooled
 * @param tcv
 */
void
afs_ReleaseConns(struct sa_conn_vector *tcv) {
    release_conns_vector(tcv);
}


/**
 * Free connection vector(s) for a user
 * @param au
 */
void
afs_ReleaseConnsUser(struct unixuser *au) {

    int i;
    struct server *ts;

    for (i = 0; i < NSERVERS; i++) {
        for (ts = afs_servers[i]; ts; ts = ts->next) {
            release_conns_user_server(au, ts);
        }	/*For each server on chain */
    } /*For each chain */
}


/**
 * For multi homed clients, a RPC may timeout because of a
 * client network interface going down. We need to reopen new
 * connections in this case.
 *
 * @param sap Server address.
 */
void
ForceNewConnections(struct srvAddr *sap)
{
    int cix;
    struct afs_conn *tc = NULL;
    struct sa_conn_vector *tcv = NULL;

    if (!sap)
	return; /* defensive check */

    ObtainWriteLock(&afs_xconn, 413);
    for (tcv = sap->conns; tcv; tcv = tcv->next) {
        for(cix = 0; cix < CVEC_LEN; ++cix) {
            tc = &(tcv->cvec[cix]);
            if (tc->activated)
                tc->forceConnectFS = 1;
        }
    }
    ReleaseWriteLock(&afs_xconn);
}


