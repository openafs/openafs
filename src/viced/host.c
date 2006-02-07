/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/stds.h>
#include <rx/xdr.h>
#include <afs/assert.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afsint.h>
#include <afs/rxgen_consts.h>
#include <afs/nfs.h>
#include <afs/errors.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#ifdef AFS_ATHENA_STDENV
#include <krb.h>
#endif
#include <afs/acl.h>
#include <afs/ptclient.h>
#include <afs/prs_fs.h>
#include <afs/auth.h>
#include <afs/afsutil.h>
#include <rx/rx.h>
#include <afs/cellconfig.h>
#include <stdlib.h>
#include "viced_prototypes.h"
#include "viced.h"
#include "host.h"


#ifdef AFS_PTHREAD_ENV
pthread_mutex_t host_glock_mutex;
#endif /* AFS_PTHREAD_ENV */

extern int Console;
extern int CurrentConnections;
extern int SystemId;
extern int AnonymousID;
extern prlist AnonCPS;
extern int LogLevel;
extern struct afsconf_dir *confDir;	/* config dir object */
extern int lwps;		/* the max number of server threads */
extern afsUUID FS_HostUUID;

int CEs = 0;			/* active clients */
int CEBlocks = 0;		/* number of blocks of CEs */
struct client *CEFree = 0;	/* first free client */
struct host *hostList = 0;	/* linked list of all hosts */
int hostCount = 0;		/* number of hosts in hostList */
int rxcon_ident_key;
int rxcon_client_key;

#define CESPERBLOCK 73
struct CEBlock {		/* block of CESPERBLOCK file entries */
    struct client entry[CESPERBLOCK];
};

static void h_TossStuff_r(register struct host *host);

/*
 * Make sure the subnet macros have been defined.
 */
#ifndef IN_SUBNETA
#define	IN_SUBNETA(i)		((((afs_int32)(i))&0x80800000)==0x00800000)
#endif

#ifndef IN_CLASSA_SUBNET
#define	IN_CLASSA_SUBNET	0xffff0000
#endif

#ifndef IN_SUBNETB
#define	IN_SUBNETB(i)		((((afs_int32)(i))&0xc0008000)==0x80008000)
#endif

#ifndef IN_CLASSB_SUBNET
#define	IN_CLASSB_SUBNET	0xffffff00
#endif


/* get a new block of CEs and chain it on CEFree */
static void
GetCEBlock()
{
    register struct CEBlock *block;
    register int i;

    block = (struct CEBlock *)malloc(sizeof(struct CEBlock));
    if (!block) {
	ViceLog(0, ("Failed malloc in GetCEBlock\n"));
	ShutDownAndCore(PANIC);
    }

    for (i = 0; i < (CESPERBLOCK - 1); i++) {
	Lock_Init(&block->entry[i].lock);
	block->entry[i].next = &(block->entry[i + 1]);
    }
    block->entry[CESPERBLOCK - 1].next = 0;
    Lock_Init(&block->entry[CESPERBLOCK - 1].lock);
    CEFree = (struct client *)block;
    CEBlocks++;

}				/*GetCEBlock */


/* get the next available CE */
static struct client *
GetCE()
{
    register struct client *entry;

    if (CEFree == 0)
	GetCEBlock();
    if (CEFree == 0) {
	ViceLog(0, ("CEFree NULL in GetCE\n"));
	ShutDownAndCore(PANIC);
    }

    entry = CEFree;
    CEFree = entry->next;
    CEs++;
    memset((char *)entry, 0, CLIENT_TO_ZERO(entry));
    return (entry);

}				/*GetCE */


/* return an entry to the free list */
static void
FreeCE(register struct client *entry)
{
    entry->next = CEFree;
    CEFree = entry;
    CEs--;

}				/*FreeCE */

/*
 * The HTs and HTBlocks variables were formerly static, but they are
 * now referenced elsewhere in the FileServer.
 */
int HTs = 0;			/* active file entries */
int HTBlocks = 0;		/* number of blocks of HTs */
static struct host *HTFree = 0;	/* first free file entry */

/*
 * Hash tables of host pointers. We need two tables, one
 * to map IP addresses onto host pointers, and another
 * to map host UUIDs onto host pointers.
 */
static struct h_hashChain *hostHashTable[h_HASHENTRIES];
static struct h_hashChain *hostUuidHashTable[h_HASHENTRIES];
#define h_HashIndex(hostip) ((hostip) & (h_HASHENTRIES-1))
#define h_UuidHashIndex(uuidp) (((int)(afs_uuid_hash(uuidp))) & (h_HASHENTRIES-1))

struct HTBlock {		/* block of HTSPERBLOCK file entries */
    struct host entry[h_HTSPERBLOCK];
};


/* get a new block of HTs and chain it on HTFree */
static void
GetHTBlock()
{
    register struct HTBlock *block;
    register int i;
    static int index = 0;

    block = (struct HTBlock *)malloc(sizeof(struct HTBlock));
    if (!block) {
	ViceLog(0, ("Failed malloc in GetHTBlock\n"));
	ShutDownAndCore(PANIC);
    }
#ifdef AFS_PTHREAD_ENV
    for (i = 0; i < (h_HTSPERBLOCK); i++)
	assert(pthread_cond_init(&block->entry[i].cond, NULL) == 0);
#endif /* AFS_PTHREAD_ENV */
    for (i = 0; i < (h_HTSPERBLOCK); i++)
	Lock_Init(&block->entry[i].lock);
    for (i = 0; i < (h_HTSPERBLOCK - 1); i++)
	block->entry[i].next = &(block->entry[i + 1]);
    for (i = 0; i < (h_HTSPERBLOCK); i++)
	block->entry[i].index = index++;
    block->entry[h_HTSPERBLOCK - 1].next = 0;
    HTFree = (struct host *)block;
    hosttableptrs[HTBlocks++] = block->entry;

}				/*GetHTBlock */


/* get the next available HT */
static struct host *
GetHT()
{
    register struct host *entry;

    if (HTFree == 0)
	GetHTBlock();
    assert(HTFree != 0);
    entry = HTFree;
    HTFree = entry->next;
    HTs++;
    memset((char *)entry, 0, HOST_TO_ZERO(entry));
    return (entry);

}				/*GetHT */


/* return an entry to the free list */
static void
FreeHT(register struct host *entry)
{
    entry->next = HTFree;
    HTFree = entry;
    HTs--;

}				/*FreeHT */


static short consolePort = 0;

int
h_Release(register struct host *host)
{
    H_LOCK;
    h_Release_r(host);
    H_UNLOCK;
    return 0;
}

/**
 * If this thread does not have a hold on this host AND
 * if other threads also dont have any holds on this host AND
 * If either the HOSTDELETED or CLIENTDELETED flags are set
 * then toss the host
 */
int
h_Release_r(register struct host *host)
{

    if (!((host)->holds[h_holdSlot()] & ~h_holdbit())) {
	if (!h_OtherHolds_r(host)) {
	    /* must avoid masking this until after h_OtherHolds_r runs
	     * but it should be run before h_TossStuff_r */
	    (host)->holds[h_holdSlot()] &= ~h_holdbit();
	    if ((host->hostFlags & HOSTDELETED)
		|| (host->hostFlags & CLIENTDELETED)) {
		h_TossStuff_r(host);
	    }
	} else
	    (host)->holds[h_holdSlot()] &= ~h_holdbit();
    } else
	(host)->holds[h_holdSlot()] &= ~h_holdbit();

    return 0;
}

int
h_OtherHolds_r(register struct host *host)
{
    register int i, bit, slot;
    bit = h_holdbit();
    slot = h_holdSlot();
    for (i = 0; i < h_maxSlots; i++) {
	if (host->holds[i] != ((i == slot) ? bit : 0)) {
	    return 1;
	}
    }
    return 0;
}

int
h_Lock_r(register struct host *host)
{
    H_UNLOCK;
    h_Lock(host);
    H_LOCK;
    return 0;
}

/**
  * Non-blocking lock
  * returns 1 if already locked
  * else returns locks and returns 0
  */

int
h_NBLock_r(register struct host *host)
{
    struct Lock *hostLock = &host->lock;
    int locked = 0;

    H_UNLOCK;
    LOCK_LOCK(hostLock);
    if (!(hostLock->excl_locked) && !(hostLock->readers_reading))
	hostLock->excl_locked = WRITE_LOCK;
    else
	locked = 1;

    LOCK_UNLOCK(hostLock);
    H_LOCK;
    if (locked)
	return 1;
    else
	return 0;
}


#if FS_STATS_DETAILED
/*------------------------------------------------------------------------
 * PRIVATE h_AddrInSameNetwork
 *
 * Description:
 *	Given a target IP address and a candidate IP address (both
 *	in host byte order), return a non-zero value (1) if the
 *	candidate address is in a different network from the target
 *	address.
 *
 * Arguments:
 *	a_targetAddr	   : Target address.
 *	a_candAddr	   : Candidate address.
 *
 * Returns:
 *	1 if the candidate address is in the same net as the target,
 *	0 otherwise.
 *
 * Environment:
 *	The target and candidate addresses are both in host byte
 *	order, NOT network byte order, when passed in.  We return
 *	our value as a character, since that's the type of field in
 *	the host structure, where this info will be stored.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static char
h_AddrInSameNetwork(afs_uint32 a_targetAddr, afs_uint32 a_candAddr)
{				/*h_AddrInSameNetwork */

    afs_uint32 targetNet;
    afs_uint32 candNet;

    /*
     * Pull out the network and subnetwork numbers from the target
     * and candidate addresses.  We can short-circuit this whole
     * affair if the target and candidate addresses are not of the
     * same class.
     */
    if (IN_CLASSA(a_targetAddr)) {
	if (!(IN_CLASSA(a_candAddr))) {
	    return (0);
	}
	targetNet = a_targetAddr & IN_CLASSA_NET;
	candNet = a_candAddr & IN_CLASSA_NET;
    } else if (IN_CLASSB(a_targetAddr)) {
	if (!(IN_CLASSB(a_candAddr))) {
	    return (0);
	}
	targetNet = a_targetAddr & IN_CLASSB_NET;
	candNet = a_candAddr & IN_CLASSB_NET;
    } /*Class B target */
    else if (IN_CLASSC(a_targetAddr)) {
	if (!(IN_CLASSC(a_candAddr))) {
	    return (0);
	}
	targetNet = a_targetAddr & IN_CLASSC_NET;
	candNet = a_candAddr & IN_CLASSC_NET;
    } /*Class C target */
    else {
	targetNet = a_targetAddr;
	candNet = a_candAddr;
    }				/*Class D address */

    /*
     * Now, simply compare the extracted net values for the two addresses
     * (which at this point are known to be of the same class)
     */
    if (targetNet == candNet)
	return (1);
    else
	return (0);

}				/*h_AddrInSameNetwork */
#endif /* FS_STATS_DETAILED */


/* Assumptions: called with held host */
void
h_gethostcps_r(register struct host *host, register afs_int32 now)
{
    register int code;
    int slept = 0;

    /* wait if somebody else is already doing the getCPS call */
    while (host->hostFlags & HCPS_INPROGRESS) {
	slept = 1;		/* I did sleep */
	host->hostFlags |= HCPS_WAITING;	/* I am sleeping now */
#ifdef AFS_PTHREAD_ENV
	pthread_cond_wait(&host->cond, &host_glock_mutex);
#else /* AFS_PTHREAD_ENV */
	if ((code = LWP_WaitProcess(&(host->hostFlags))) != LWP_SUCCESS)
	    ViceLog(0, ("LWP_WaitProcess returned %d\n", code));
#endif /* AFS_PTHREAD_ENV */
    }


    host->hostFlags |= HCPS_INPROGRESS;	/* mark as CPSCall in progress */
    if (host->hcps.prlist_val)
	free(host->hcps.prlist_val);	/* this is for hostaclRefresh */
    host->hcps.prlist_val = NULL;
    host->hcps.prlist_len = 0;
    slept ? (host->cpsCall = FT_ApproxTime()) : (host->cpsCall = now);

    H_UNLOCK;
    code = pr_GetHostCPS(htonl(host->host), &host->hcps);
    H_LOCK;
    if (code) {
	/*
	 * Although ubik_Call (called by pr_GetHostCPS) traverses thru all protection servers
	 * and reevaluates things if no sync server or quorum is found we could still end up
	 * with one of these errors. In such case we would like to reevaluate the rpc call to
	 * find if there's cps for this guy. We treat other errors (except network failures
	 * ones - i.e. code < 0) as an indication that there is no CPS for this host. Ideally
	 * we could like to deal this problem the other way around (i.e. if code == NOCPS 
	 * ignore else retry next time) but the problem is that there're other errors (i.e.
	 * EPERM) for which we don't want to retry and we don't know the whole code list!
	 */
	if (code < 0 || code == UNOQUORUM || code == UNOTSYNC) {
	    /* 
	     * We would have preferred to use a while loop and try again since ops in protected
	     * acls for this host will fail now but they'll be reevaluated on any subsequent
	     * call. The attempt to wait for a quorum/sync site or network error won't work
	     * since this problems really should only occurs during a complete fileserver 
	     * restart. Since the fileserver will start before the ptservers (and thus before
	     * quorums are complete) clients will be utilizing all the fileserver's lwps!!
	     */
	    host->hcpsfailed = 1;
	    ViceLog(0,
		    ("Warning:  GetHostCPS failed (%d) for %x; will retry\n",
		     code, host->host));
	} else {
	    host->hcpsfailed = 0;
	    ViceLog(1,
		    ("gethost:  GetHostCPS failed (%d) for %x; ignored\n",
		     code, host->host));
	}
	if (host->hcps.prlist_val)
	    free(host->hcps.prlist_val);
	host->hcps.prlist_val = NULL;
	host->hcps.prlist_len = 0;	/* Make sure it's zero */
    } else
	host->hcpsfailed = 0;

    host->hostFlags &= ~HCPS_INPROGRESS;
    /* signal all who are waiting */
    if (host->hostFlags & HCPS_WAITING) {	/* somebody is waiting */
	host->hostFlags &= ~HCPS_WAITING;
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&host->cond) == 0);
#else /* AFS_PTHREAD_ENV */
	if ((code = LWP_NoYieldSignal(&(host->hostFlags))) != LWP_SUCCESS)
	    ViceLog(0, ("LWP_NoYieldSignal returns %d\n", code));
#endif /* AFS_PTHREAD_ENV */
    }
}

/* args in net byte order */
void
h_flushhostcps(register afs_uint32 hostaddr, register afs_uint32 hport)
{
    register struct host *host;
    int held = 0;

    H_LOCK;
    host = h_Lookup_r(hostaddr, hport, &held);
    if (host) {
	host->hcpsfailed = 1;
	if (!held)
	    h_Release_r(host);
    }
    H_UNLOCK;
    return;
}


/*
 * Allocate a host.  It will be identified by the peer (ip,port) info in the
 * rx connection provided.  The host is returned held and locked
 */
#define	DEF_ROPCONS 2115

struct host *
h_Alloc_r(register struct rx_connection *r_con)
{
    struct servent *serverentry;
    register index = h_HashIndex(rxr_HostOf(r_con));
    register struct host *host;
    static struct rx_securityClass *sc = 0;
    afs_int32 now;
    struct h_hashChain *h_hashChain;
#if FS_STATS_DETAILED
    afs_uint32 newHostAddr_HBO;	/*New host IP addr, in host byte order */
#endif /* FS_STATS_DETAILED */

    host = GetHT();

    h_hashChain = (struct h_hashChain *)malloc(sizeof(struct h_hashChain));
    if (!h_hashChain) {
	ViceLog(0, ("Failed malloc in h_Alloc_r\n"));
	assert(0);
    }
    h_hashChain->hostPtr = host;
    h_hashChain->addr = rxr_HostOf(r_con);
    h_hashChain->next = hostHashTable[index];
    hostHashTable[index] = h_hashChain;

    host->host = rxr_HostOf(r_con);
    host->port = rxr_PortOf(r_con);
    if (consolePort == 0) {	/* find the portal number for console */
#if	defined(AFS_OSF_ENV)
	serverentry = getservbyname("ropcons", "");
#else
	serverentry = getservbyname("ropcons", 0);
#endif
	if (serverentry)
	    consolePort = serverentry->s_port;
	else
	    consolePort = htons(DEF_ROPCONS);	/* Use a default */
    }
    if (host->port == consolePort)
	host->Console = 1;
    /* Make a callback channel even for the console, on the off chance that it
     * makes a request that causes a break call back.  It shouldn't. */
    {
	if (!sc)
	    sc = rxnull_NewClientSecurityObject();
	host->callback_rxcon =
	    rx_NewConnection(host->host, host->port, 1, sc, 0);
	rx_SetConnDeadTime(host->callback_rxcon, 50);
	rx_SetConnHardDeadTime(host->callback_rxcon, AFS_HARDDEADTIME);
    }
    now = host->LastCall = host->cpsCall = host->ActiveCall = FT_ApproxTime();
    host->hostFlags = 0;
    host->hcps.prlist_val = NULL;
    host->hcps.prlist_len = 0;
    host->interface = 0;
#ifdef undef
    host->hcpsfailed = 0;	/* save cycles */
    h_gethostcps(host);		/* do this under host hold/lock */
#endif
    host->FirstClient = 0;
    h_Hold_r(host);
    h_Lock_r(host);
    h_InsertList_r(host);	/* update global host List */
#if FS_STATS_DETAILED
    /*
     * Compare the new host's IP address (in host byte order) with ours
     * (the File Server's), remembering if they are in the same network.
     */
    newHostAddr_HBO = (afs_uint32) ntohl(host->host);
    host->InSameNetwork =
	h_AddrInSameNetwork(FS_HostAddr_HBO, newHostAddr_HBO);
#endif /* FS_STATS_DETAILED */
    return host;

}				/*h_Alloc_r */


/* Lookup a host given an IP address and UDP port number. */
/* hostaddr and hport are in network order */
/* Note: host should be released by caller if 0 == *heldp and non-null */
/* hostaddr and hport are in network order */
struct host *
h_Lookup_r(afs_uint32 haddr, afs_uint32 hport, int *heldp)
{
    register afs_int32 now;
    register struct host *host = 0;
    register struct h_hashChain *chain;
    register index = h_HashIndex(haddr);
    extern int hostaclRefresh;

  restart:
    for (chain = hostHashTable[index]; chain; chain = chain->next) {
	host = chain->hostPtr;
	assert(host);
	if (!(host->hostFlags & HOSTDELETED) && chain->addr == haddr
	    && chain->port == hport) {
	    *heldp = h_Held_r(host);
	    if (!*heldp)
		h_Hold_r(host);
	    h_Lock_r(host);
	    if (host->hostFlags & HOSTDELETED) {
		h_Unlock_r(host);
		if (!*heldp)
		    h_Release_r(host);
		goto restart;
	    }
	    h_Unlock_r(host);
	    now = FT_ApproxTime();	/* always evaluate "now" */
	    if (host->hcpsfailed || (host->cpsCall + hostaclRefresh < now)) {
		/*
		 * Every hostaclRefresh period (def 2 hrs) get the new
		 * membership list for the host.  Note this could be the
		 * first time that the host is added to a group.  Also
		 * here we also retry on previous legitimate hcps failures.
		 *
		 * If we get here we still have a host hold.
		 */
		h_gethostcps_r(host, now);
	    }
	    break;
	}
	host = NULL;
    }
    return host;

}				/*h_Lookup */

/* Lookup a host given its UUID. */
struct host *
h_LookupUuid_r(afsUUID * uuidp)
{
    register struct host *host = 0;
    register struct h_hashChain *chain;
    register index = h_UuidHashIndex(uuidp);

    for (chain = hostUuidHashTable[index]; chain; chain = chain->next) {
	host = chain->hostPtr;
	assert(host);
	if (!(host->hostFlags & HOSTDELETED) && host->interface
	    && afs_uuid_equal(&host->interface->uuid, uuidp)) {
	    break;
	}
	host = NULL;
    }
    return host;

}				/*h_Lookup */


/*
 * h_Hold_r: Establish a hold by the current LWP on this host--the host
 * or its clients will not be physically deleted until all holds have
 * been released.
 * NOTE: h_Hold_r is a macro defined in host.h.
 */

/* h_TossStuff_r:  Toss anything in the host structure (the host or
 * clients marked for deletion.  Called from h_Release_r ONLY.
 * To be called, there must be no holds, and either host->deleted
 * or host->clientDeleted must be set.
 */
static void
h_TossStuff_r(register struct host *host)
{
    register struct client **cp, *client;
    int i;

    /* if somebody still has this host held */
    for (i = 0; (i < h_maxSlots) && (!(host)->holds[i]); i++);
    if (i != h_maxSlots)
	return;

    /* if somebody still has this host locked */
    if (h_NBLock_r(host) != 0) {
	char hoststr[16];
	ViceLog(0,
		("Warning:  h_TossStuff_r failed; Host %s:%d was locked.\n",
		 afs_inet_ntoa_r(host->host, hoststr), ntohs(host->port)));
	return;
    } else {
	h_Unlock_r(host);
    }

    /* ASSUMPTION: rxi_FreeConnection() does not yield */
    for (cp = &host->FirstClient; (client = *cp);) {
	if ((host->hostFlags & HOSTDELETED) || client->deleted) {
	    if (client->refCount) {
		char hoststr[16];
		ViceLog(0,
			("Warning: Host %s:%d client %x refcount %d while deleting.\n",
			 afs_inet_ntoa_r(host->host, hoststr),
			 ntohs(host->port), client, client->refCount));
	    }
	    if ((client->ViceId != ANONYMOUSID) && client->CPS.prlist_val) {
		free(client->CPS.prlist_val);
		client->CPS.prlist_val = NULL;
	    }
	    client->CPS.prlist_len = 0;
	    if (client->tcon) {
		rx_SetSpecific(client->tcon, rxcon_client_key, (void *)0);
	    }
	    CurrentConnections--;
	    *cp = client->next;
	    FreeCE(client);
	} else
	    cp = &client->next;
    }

    /* We've just cleaned out all the deleted clients; clear the flag */
    host->hostFlags &= ~CLIENTDELETED;

    if (host->hostFlags & HOSTDELETED) {
	register struct h_hashChain **hp, *th;
	register struct rx_connection *rxconn;
	afsUUID *uuidp;
	struct AddrPort hostAddrPort;
	int i;

	if (host->Console & 1)
	    Console--;
	if ((rxconn = host->callback_rxcon)) {
	    host->callback_rxcon = (struct rx_connection *)0;
	    /*
	     * If rx_DestroyConnection calls h_FreeConnection we will
	     * deadlock on the host_glock_mutex. Work around the problem
	     * by unhooking the client from the connection before
	     * destroying the connection.
	     */
	    client = rx_GetSpecific(rxconn, rxcon_client_key);
	    if (client && client->tcon == rxconn)
		client->tcon = NULL;
	    rx_SetSpecific(rxconn, rxcon_client_key, (void *)0);
	    rx_DestroyConnection(rxconn);
	}
	if (host->hcps.prlist_val)
	    free(host->hcps.prlist_val);
	host->hcps.prlist_val = NULL;
	host->hcps.prlist_len = 0;
	DeleteAllCallBacks_r(host, 1);
	host->hostFlags &= ~RESETDONE;	/* just to be safe */

	/* if alternate addresses do not exist */
	if (!(host->interface)) {
	    for (hp = &hostHashTable[h_HashIndex(host->host)]; (th = *hp);
		 hp = &th->next) {
		assert(th->hostPtr);
		if (th->hostPtr == host) {
		    *hp = th->next;
		    h_DeleteList_r(host);
		    FreeHT(host);
		    free(th);
		    break;
		}
	    }
	} else {
	    /* delete all hash entries for the UUID */
	    uuidp = &host->interface->uuid;
	    for (hp = &hostUuidHashTable[h_UuidHashIndex(uuidp)]; (th = *hp);
		 hp = &th->next) {
		assert(th->hostPtr);
		if (th->hostPtr == host) {
		    *hp = th->next;
		    free(th);
		    break;
		}
	    }
	    /* delete all hash entries for alternate addresses */
	    assert(host->interface->numberOfInterfaces > 0);
	    for (i = 0; i < host->interface->numberOfInterfaces; i++) {
		hostAddrPort = host->interface->interface[i];

		for (hp = &hostHashTable[h_HashIndex(hostAddrPort.addr)]; (th = *hp);
		     hp = &th->next) {
		    assert(th->hostPtr);
		    if (th->hostPtr == host) {
			*hp = th->next;
			free(th);
			break;
		    }
		}
	    }
	    free(host->interface);
	    host->interface = NULL;
	    h_DeleteList_r(host);	/* remove host from global host List */
	    FreeHT(host);
	}			/* if alternate address exists */
    }
}				/*h_TossStuff_r */


/* Called by rx when a server connection disappears */
int
h_FreeConnection(struct rx_connection *tcon)
{
    register struct client *client;

    client = (struct client *)rx_GetSpecific(tcon, rxcon_client_key);
    if (client) {
	H_LOCK;
	if (client->tcon == tcon)
	    client->tcon = (struct rx_connection *)0;
	H_UNLOCK;
    }
    return 0;
}				/*h_FreeConnection */


/* h_Enumerate: Calls (*proc)(host, held, param) for at least each host in the
 * system at the start of the enumeration (perhaps more).  Hosts may be deleted
 * (have delete flag set); ditto for clients.  (*proc) is always called with
 * host h_held().  The hold state of the host with respect to this lwp is passed
 * to (*proc) as the param held.  The proc should return 0 if the host should be
 * released, 1 if it should be held after enumeration.
 */
void
h_Enumerate(int (*proc) (), char *param)
{
    register struct host *host, **list;
    register int *held;
    register int i, count;

    H_LOCK;
    if (hostCount == 0) {
	H_UNLOCK;
	return;
    }
    list = (struct host **)malloc(hostCount * sizeof(struct host *));
    if (!list) {
	ViceLog(0, ("Failed malloc in h_Enumerate\n"));
	assert(0);
    }
    held = (int *)malloc(hostCount * sizeof(int));
    if (!held) {
	ViceLog(0, ("Failed malloc in h_Enumerate\n"));
	assert(0);
    }
    for (count = 0, host = hostList; host; host = host->next, count++) {
	list[count] = host;
	if (!(held[count] = h_Held_r(host)))
	    h_Hold_r(host);
    }
    assert(count == hostCount);
    H_UNLOCK;
    for (i = 0; i < count; i++) {
	held[i] = (*proc) (list[i], held[i], param);
	if (!held[i])
	    h_Release(list[i]);	/* this might free up the host */
    }
    free((void *)list);
    free((void *)held);
}				/*h_Enumerate */

/* h_Enumerate_r (revised):
 * Calls (*proc)(host, held, param) for each host in hostList, starting
 * at enumstart
 * Hosts may be deleted (have delete flag set); ditto for clients.
 * (*proc) is always called with
 * host h_held() and the global host lock (H_LOCK) locked.The hold state of the
 * host with respect to this lwp is passed to (*proc) as the param held.
 * The proc should return 0 if the host should be released, 1 if it should
 * be held after enumeration.
 */
void
h_Enumerate_r(int (*proc) (), struct host *enumstart, char *param)
{
    register struct host *host, *next;
    register int held, nheld;

    if (hostCount == 0) {
	return;
    }
    if (enumstart && !(held = h_Held_r(enumstart)))
	h_Hold_r(enumstart); 
    for (host = enumstart; host; host = next, held = nheld) {
	held = (*proc) (host, held, param);
	next = host->next;
	if (next && !(nheld = h_Held_r(next)))
	    h_Hold_r(next);
	if (!held)
	    h_Release_r(host); /* this might free up the host */
    }
}				/*h_Enumerate_r */

/* inserts a new HashChain structure corresponding to this UUID */
void
hashInsertUuid_r(struct afsUUID *uuid, struct host *host)
{
    int index;
    struct h_hashChain *chain;

    /* hash into proper bucket */
    index = h_UuidHashIndex(uuid);

    /* insert into beginning of list for this bucket */
    chain = (struct h_hashChain *)malloc(sizeof(struct h_hashChain));
    if (!chain) {
	ViceLog(0, ("Failed malloc in hashInsertUuid_r\n"));
	assert(0);
    }
    assert(chain);
    chain->hostPtr = host;
    chain->next = hostUuidHashTable[index];
    hostUuidHashTable[index] = chain;
}


/* inserts a new HashChain structure corresponding to this address */
void
hashInsert_r(afs_uint32 addr, afs_uint16 port, struct host *host)
{
    int index;
    struct h_hashChain *chain;

    /* hash into proper bucket */
    index = h_HashIndex(addr);

    /* insert into beginning of list for this bucket */
    chain = (struct h_hashChain *)malloc(sizeof(struct h_hashChain));
    if (!chain) {
	ViceLog(0, ("Failed malloc in hashInsert_r\n"));
	assert(0);
    }
    chain->hostPtr = host;
    chain->next = hostHashTable[index];
    chain->addr = addr;
    chain->port = port;
    hostHashTable[index] = chain;

}

/*
 * This is called with host locked and held. At this point, the
 * hostHashTable should not be having entries for the alternate
 * interfaces. This function has to insert these entries in the
 * hostHashTable.
 *
 * All addresses are in network byte order.
 */
int
addInterfaceAddr_r(struct host *host, afs_uint32 addr, afs_uint16 port)
{
    int i;
    int number;
    int found;
    struct Interface *interface;
    char hoststr[16], hoststr2[16];

    assert(host);
    assert(host->interface);

    ViceLog(125, ("addInterfaceAddr : host %s:d addr %s:%d\n", 
		   afs_inet_ntoa_r(host->host, hoststr), ntohs(host->port), 
		   afs_inet_ntoa_r(addr, hoststr2), ntohs(port)));

    /*
     * Make sure this address is on the list of known addresses
     * for this host.
     */
    number = host->interface->numberOfInterfaces;
    for (i = 0, found = 0; i < number && !found; i++) {
	if (host->interface->interface[i].addr == addr &&
	    host->interface->interface[i].port == port)
	    found = 1;
    }
    if (!found) {
	interface = (struct Interface *)
	    malloc(sizeof(struct Interface) + (sizeof(struct AddrPort) * number));
	if (!interface) {
	    ViceLog(0, ("Failed malloc in addInterfaceAddr_r\n"));
	    assert(0);
	}
	interface->numberOfInterfaces = number + 1;
	interface->uuid = host->interface->uuid;
	for (i = 0; i < number; i++)
	    interface->interface[i] = host->interface->interface[i];
	interface->interface[number].addr = addr;
	interface->interface[number].port = port;
	free(host->interface);
	host->interface = interface;
    }

    /*
     * Create a hash table entry for this address
     */
    hashInsert_r(addr, port, host);

    return 0;
}


/*
 * This is called with host locked and held. At this point, the
 * hostHashTable should not be having entries for the alternate
 * interfaces. This function has to insert these entries in the
 * hostHashTable.
 *
 * All addresses are in network byte order.
 */
int
removeInterfaceAddr_r(struct host *host, afs_uint32 addr, afs_uint16 port)
{
    int i;
    int number;
    int found;
    struct Interface *interface;
    char hoststr[16], hoststr2[16];

    assert(host);
    assert(host->interface);

    ViceLog(125, ("removeInterfaceAddr : host %s:d addr %s:%d\n", 
		   afs_inet_ntoa_r(host->host, hoststr), ntohs(host->port), 
		   afs_inet_ntoa_r(addr, hoststr2), ntohs(port)));

    /*
     * Make sure this address is on the list of known addresses
     * for this host.
     */
    interface = host->interface;
    number = host->interface->numberOfInterfaces;
    for (i = 0, found = 0; i < number; i++) {
	if (interface->interface[i].addr == addr &&
	    interface->interface[i].port == port) {
	    found = 1;
	    break;
	}
    }
    if (found) {
	number--;
	for (; i < number; i++) {
	    interface->interface[i].addr = interface->interface[i+1].addr;
	    interface->interface[i].port = interface->interface[i+1].port;
	}
	interface->numberOfInterfaces = number;
    }

    /*
     * Remove the hash table entry for this address
     */
    hashDelete_r(addr, port, host);

    return 0;
}


/* Host is returned held */
struct host *
h_GetHost_r(struct rx_connection *tcon)
{
    struct host *host;
    struct host *oldHost;
    int code;
    int held, oheld;
    struct interfaceAddr interf;
    int interfValid = 0;
    struct Identity *identP = NULL;
    afs_int32 haddr;
    afs_int16 hport;
    char hoststr[16], hoststr2[16];
    Capabilities caps;
    struct rx_connection *cb_conn = NULL;

    caps.Capabilities_val = NULL;

    haddr = rxr_HostOf(tcon);
    hport = rxr_PortOf(tcon);
  retry:
    if (caps.Capabilities_val)
	free(caps.Capabilities_val);
    caps.Capabilities_val = NULL;
    caps.Capabilities_len = 0;

    code = 0;
    host = h_Lookup_r(haddr, hport, &held);
    identP = (struct Identity *)rx_GetSpecific(tcon, rxcon_ident_key);
    if (host && !identP && !(host->Console & 1)) {
	/* This is a new connection, and we already have a host
	 * structure for this address. Verify that the identity
	 * of the caller matches the identity in the host structure.
	 */
	h_Lock_r(host);
	if (!(host->hostFlags & ALTADDR)) {
	    /* Another thread is doing initialization */
	    h_Unlock_r(host);
	    if (!held)
		h_Release_r(host);
	    ViceLog(125,
		    ("Host %s:%d starting h_Lookup again\n",
		     afs_inet_ntoa_r(host->host, hoststr),
		     ntohs(host->port)));
	    goto retry;
	}
	host->hostFlags &= ~ALTADDR;
	cb_conn = host->callback_rxcon;
	rx_GetConnection(cb_conn);
	H_UNLOCK;
	code =
	    RXAFSCB_TellMeAboutYourself(cb_conn, &interf, &caps);
	if (code == RXGEN_OPCODE)
	    code = RXAFSCB_WhoAreYou(cb_conn, &interf);
	rx_PutConnection(cb_conn);
	cb_conn=NULL;
	H_LOCK;
	if (code == RXGEN_OPCODE) {
	    identP = (struct Identity *)malloc(sizeof(struct Identity));
	    if (!identP) {
		ViceLog(0, ("Failed malloc in h_GetHost_r\n"));
		assert(0);
	    }
	    identP->valid = 0;
	    rx_SetSpecific(tcon, rxcon_ident_key, identP);
	    /* The host on this connection was unable to respond to 
	     * the WhoAreYou. We will treat this as a new connection
	     * from the existing host. The worst that can happen is
	     * that we maintain some extra callback state information */
	    if (host->interface) {
		ViceLog(0,
			("Host %s:%d used to support WhoAreYou, deleting.\n",
			 afs_inet_ntoa_r(host->host, hoststr),
			 ntohs(host->port)));
		host->hostFlags |= HOSTDELETED;
		h_Unlock_r(host);
		if (!held)
		    h_Release_r(host);
		host = NULL;
		goto retry;
	    }
	} else if (code == 0) {
	    interfValid = 1;
	    identP = (struct Identity *)malloc(sizeof(struct Identity));
	    if (!identP) {
		ViceLog(0, ("Failed malloc in h_GetHost_r\n"));
		assert(0);
	    }
	    identP->valid = 1;
	    identP->uuid = interf.uuid;
	    rx_SetSpecific(tcon, rxcon_ident_key, identP);
	    /* Check whether the UUID on this connection matches
	     * the UUID in the host structure. If they don't match
	     * then this is not the same host as before. */
	    if (!host->interface
		|| !afs_uuid_equal(&interf.uuid, &host->interface->uuid)) {
		ViceLog(25,
			("Host %s:%d has changed its identity, deleting.\n",
			 afs_inet_ntoa_r(host->host, hoststr), host->port));
		host->hostFlags |= HOSTDELETED;
		h_Unlock_r(host);
		if (!held)
		    h_Release_r(host);
		host = NULL;
		goto retry;
	    }
	} else {
	    afs_inet_ntoa_r(host->host, hoststr);
	    ViceLog(0,
		    ("CB: WhoAreYou failed for %s:%d, error %d\n", hoststr,
		     ntohs(host->port), code));
	    host->hostFlags |= VENUSDOWN;
	}
	if (caps.Capabilities_val
	    && (caps.Capabilities_val[0] & CLIENT_CAPABILITY_ERRORTRANS))
	    host->hostFlags |= HERRORTRANS;
	else
	    host->hostFlags &= ~(HERRORTRANS);
	host->hostFlags |= ALTADDR;
	h_Unlock_r(host);
    } else if (host) {
	if (!(host->hostFlags & ALTADDR)) {
	    /* another thread is doing the initialisation */
	    ViceLog(125,
		    ("Host %s:%d waiting for host-init to complete\n",
		     afs_inet_ntoa_r(host->host, hoststr),
		     ntohs(host->port)));
	    h_Lock_r(host);
	    h_Unlock_r(host);
	    if (!held)
		h_Release_r(host);
	    ViceLog(125,
		    ("Host %s:%d starting h_Lookup again\n",
		     afs_inet_ntoa_r(host->host, hoststr),
		     ntohs(host->port)));
	    goto retry;
	}
	/* We need to check whether the identity in the host structure
	 * matches the identity on the connection. If they don't match
	 * then treat this a new host. */
	if (!(host->Console & 1)
	    && ((!identP->valid && host->interface)
		|| (identP->valid && !host->interface)
		|| (identP->valid
		    && !afs_uuid_equal(&identP->uuid,
				       &host->interface->uuid)))) {
	    char uuid1[128], uuid2[128];
	    if (identP->valid)
		afsUUID_to_string(&identP->uuid, uuid1, 127);
	    if (host->interface)
		afsUUID_to_string(&host->interface->uuid, uuid2, 127);
	    ViceLog(0,
		    ("CB: new identity for host %s:%d, deleting(%x %x %s %s)\n",
		     afs_inet_ntoa_r(host->host, hoststr), ntohs(host->port),
		     identP->valid, host->interface,
		     identP->valid ? uuid1 : "",
		     host->interface ? uuid2 : ""));

	    /* The host in the cache is not the host for this connection */
	    host->hostFlags |= HOSTDELETED;
	    h_Unlock_r(host);
	    if (!held)
		h_Release_r(host);
	    goto retry;
	}
    } else {
	host = h_Alloc_r(tcon);	/* returned held and locked */
	h_gethostcps_r(host, FT_ApproxTime());
	if (!(host->Console & 1)) {
	    int pident = 0;
	    cb_conn = host->callback_rxcon;
	    rx_GetConnection(cb_conn);
	    H_UNLOCK;
	    code =
		RXAFSCB_TellMeAboutYourself(cb_conn, &interf, &caps);
	    if (code == RXGEN_OPCODE)
		code = RXAFSCB_WhoAreYou(cb_conn, &interf);
	    rx_PutConnection(cb_conn);
	    cb_conn=NULL;
	    H_LOCK;
	    if (code == RXGEN_OPCODE) {
		if (!identP)
		    identP =
			(struct Identity *)malloc(sizeof(struct Identity));
		else
		    pident = 1;

		if (!identP) {
		    ViceLog(0, ("Failed malloc in h_GetHost_r\n"));
		    assert(0);
		}
		identP->valid = 0;
		if (!pident)
		    rx_SetSpecific(tcon, rxcon_ident_key, identP);
		ViceLog(25,
			("Host %s:%d does not support WhoAreYou.\n",
			 afs_inet_ntoa_r(host->host, hoststr),
			 ntohs(host->port)));
		code = 0;
	    } else if (code == 0) {
		if (!identP)
		    identP =
			(struct Identity *)malloc(sizeof(struct Identity));
		else
		    pident = 1;

		if (!identP) {
		    ViceLog(0, ("Failed malloc in h_GetHost_r\n"));
		    assert(0);
		}
		identP->valid = 1;
		interfValid = 1;
		identP->uuid = interf.uuid;
		if (!pident)
		    rx_SetSpecific(tcon, rxcon_ident_key, identP);
		ViceLog(25,
			("WhoAreYou success on %s:%d\n",
			 afs_inet_ntoa_r(host->host, hoststr),
			 ntohs(host->port)));
	    }
	    if (code == 0 && !identP->valid) {
	        cb_conn = host->callback_rxcon;
	        rx_GetConnection(cb_conn);
		H_UNLOCK;
		code = RXAFSCB_InitCallBackState(cb_conn);
	        rx_PutConnection(cb_conn);
	        cb_conn=NULL;
		H_LOCK;
	    } else if (code == 0) {
		oldHost = h_LookupUuid_r(&identP->uuid);
                if (oldHost) {
                    int probefail = 0;

		    if (!(oheld = h_Held_r(oldHost)))
			h_Hold_r(oldHost);
		    h_Lock_r(oldHost);

                    if (oldHost->interface) {
			afsUUID uuid = oldHost->interface->uuid;
                        cb_conn = host->callback_rxcon;
                        rx_GetConnection(cb_conn);
			H_UNLOCK;
			code = RXAFSCB_ProbeUuid(cb_conn, &uuid);
                        rx_PutConnection(cb_conn);
                        cb_conn=NULL;
			H_LOCK;
			if (code && MultiProbeAlternateAddress_r(oldHost)) {
                            probefail = 1;
                        }
                    } else {
                        probefail = 1;
                    }

                    if (probefail) {
                        /* The old host is either does not have a Uuid,
                         * is not responding to Probes, 
                         * or does not have a matching Uuid. 
                         * Delete it! */
                        oldHost->hostFlags |= HOSTDELETED;
                        h_Unlock_r(oldHost);
			/* Let the holder be last release */
			if (!oheld) {
			    h_Release_r(oldHost);
			}
			oldHost = NULL;
                    }
                }
		if (oldHost) {
		    /* This is a new address for an existing host. Update
		     * the list of interfaces for the existing host and
		     * delete the host structure we just allocated. */
		    if (oldHost->host != haddr || oldHost->port != hport) {
		    ViceLog(25,
			    ("CB: new addr %s:%d for old host %s:%d\n",
			     afs_inet_ntoa_r(host->host, hoststr),
				ntohs(host->port), 
				afs_inet_ntoa_r(oldHost->host, hoststr2),
			     ntohs(oldHost->port)));
			if (oldHost->host == haddr) {
			    /* We have just been contacted by a client behind a NAT */
			    removeInterfaceAddr_r(oldHost, oldHost->host, oldHost->port);
			} else {
			    int i, found;
			    struct Interface *interface = oldHost->interface;
			    int number = oldHost->interface->numberOfInterfaces;
			    for (i = 0, found = 0; i < number; i++) {
				if (interface->interface[i].addr == haddr &&
				    interface->interface[i].port != hport) {
				    found = 1;
				    break;
				}
			    }
			    if (found) {
				/* We have just been contacted by a client that has been
				 * seen from behind a NAT and at least one other address.
				 */
				removeInterfaceAddr_r(oldHost, haddr, interface->interface[i].port);
			    }
			}
		    addInterfaceAddr_r(oldHost, haddr, hport);
			oldHost->host = haddr;
			oldHost->port = hport;
		    }
		    host->hostFlags |= HOSTDELETED;
		    h_Unlock_r(host);
		    if (!held)
			h_Release_r(host);
		    host = oldHost;
		} else {
		    /* This really is a new host */
		    hashInsertUuid_r(&identP->uuid, host);
		    cb_conn = host->callback_rxcon;
		    rx_GetConnection(cb_conn);		
		    H_UNLOCK;
		    code =
			RXAFSCB_InitCallBackState3(cb_conn,
						   &FS_HostUUID);
		    rx_PutConnection(cb_conn);
		    cb_conn=NULL;
		    H_LOCK;
		    if (code == 0) {
			ViceLog(25,
				("InitCallBackState3 success on %s:%d\n",
				 afs_inet_ntoa_r(host->host, hoststr),
				 ntohs(host->port)));
			assert(interfValid == 1);
			initInterfaceAddr_r(host, &interf);
		    }
		}
	    }
	    if (code) {
		afs_inet_ntoa_r(host->host, hoststr);
		ViceLog(0,
			("CB: RCallBackConnectBack failed for %s:%d\n",
			 hoststr, ntohs(host->port)));
		host->hostFlags |= VENUSDOWN;
	    } else {
		ViceLog(125,
			("CB: RCallBackConnectBack succeeded for %s:%d\n",
			 hoststr, ntohs(host->port)));
		host->hostFlags |= RESETDONE;
	    }
	}
	if (caps.Capabilities_val
	    && (caps.Capabilities_val[0] & CLIENT_CAPABILITY_ERRORTRANS))
	    host->hostFlags |= HERRORTRANS;
	else
	    host->hostFlags &= ~(HERRORTRANS);
	host->hostFlags |= ALTADDR;	/* host structure initialization complete */
	h_Unlock_r(host);
    }
    if (caps.Capabilities_val)
	free(caps.Capabilities_val);
    caps.Capabilities_val = NULL;
    caps.Capabilities_len = 0;
    return host;

}				/*h_GetHost_r */


static char localcellname[PR_MAXNAMELEN + 1];
char local_realms[AFS_NUM_LREALMS][AFS_REALM_SZ];
int  num_lrealms = -1;

/* not reentrant */
void
h_InitHostPackage()
{
    afsconf_GetLocalCell(confDir, localcellname, PR_MAXNAMELEN);
    if (num_lrealms == -1) {
	int i;
	for (i=0; i<AFS_NUM_LREALMS; i++) {
	    if (afs_krb_get_lrealm(local_realms[i], i) != 0 /*KSUCCESS*/)
		break;
	}

	if (i=0) {
	    ViceLog(0,
		    ("afs_krb_get_lrealm failed, using %s.\n",
		     localcellname));
	    strncpy(local_realms[0], localcellname, AFS_REALM_SZ);
	    num_lrealms = i =1;
	} else {
	    num_lrealms = i;
	}

	/* initialize the rest of the local realms to nullstring for debugging */
	for (; i<AFS_NUM_LREALMS; i++)
	    local_realms[i][0] = '\0';
    }
    rxcon_ident_key = rx_KeyCreate((rx_destructor_t) free);
    rxcon_client_key = rx_KeyCreate((rx_destructor_t) 0);
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_init(&host_glock_mutex, NULL) == 0);
#endif /* AFS_PTHREAD_ENV */
}

static int
MapName_r(char *aname, char *acell, afs_int32 * aval)
{
    namelist lnames;
    idlist lids;
    afs_int32 code;
    afs_int32 anamelen, cnamelen;
    int foreign = 0;
    char *tname;

    anamelen = strlen(aname);
    if (anamelen >= PR_MAXNAMELEN)
	return -1;		/* bad name -- caller interprets this as anonymous, but retries later */

    lnames.namelist_len = 1;
    lnames.namelist_val = (prname *) aname;	/* don't malloc in the common case */
    lids.idlist_len = 0;
    lids.idlist_val = NULL;

    cnamelen = strlen(acell);
    if (cnamelen) {
	if (afs_is_foreign_ticket_name(aname, "", acell, localcellname)) {
	    ViceLog(2,
		    ("MapName: cell is foreign.  cell=%s, localcell=%s, localrealms={%s,%s,%s,%s}\n",
		    acell, localcellname, local_realms[0],local_realms[1],local_realms[2],local_realms[3]));
	    if ((anamelen + cnamelen + 1) >= PR_MAXNAMELEN) {
		ViceLog(2,
			("MapName: Name too long, using AnonymousID for %s@%s\n",
			 aname, acell));
		*aval = AnonymousID;
		return 0;
	    }
	    foreign = 1;	/* attempt cross-cell authentication */
	    tname = (char *)malloc(PR_MAXNAMELEN);
	    if (!tname) {
		ViceLog(0, ("Failed malloc in MapName_r\n"));
		assert(0);
	    }
	    strcpy(tname, aname);
	    tname[anamelen] = '@';
	    strcpy(tname + anamelen + 1, acell);
	    lnames.namelist_val = (prname *) tname;
	}
    }

    H_UNLOCK;
    code = pr_NameToId(&lnames, &lids);
    H_LOCK;
    if (code == 0) {
	if (lids.idlist_val) {
	    *aval = lids.idlist_val[0];
	    if (*aval == AnonymousID) {
		ViceLog(2,
			("MapName: NameToId on %s returns anonymousID\n",
			 lnames.namelist_val));
	    }
	    free(lids.idlist_val);	/* return parms are not malloced in stub if server proc aborts */
	} else {
	    ViceLog(0,
		    ("MapName: NameToId on '%s' is unknown\n",
		     lnames.namelist_val));
	    code = -1;
	}
    }

    if (foreign) {
	free(lnames.namelist_val);	/* We allocated this above, so we must free it now. */
    }
    return code;
}

/*MapName*/


/* NOTE: this returns the client with a Write lock */
struct client *
h_ID2Client(afs_int32 vid)
{
    register struct client *client;
    register struct host *host;

    H_LOCK;
    for (host = hostList; host; host = host->next) {
	if (host->hostFlags & HOSTDELETED)
	    continue;
	for (client = host->FirstClient; client; client = client->next) {
	    if (!client->deleted && client->ViceId == vid) {
		client->refCount++;
		H_UNLOCK;
		ObtainWriteLock(&client->lock);
		H_LOCK;
		client->refCount--;
		H_UNLOCK;
		return client;
	    }
	}
    }

    H_UNLOCK;
    return NULL;
}

/*
 * Called by the server main loop.  Returns a h_Held client, which must be
 * released later the main loop.  Allocates a client if the matching one
 * isn't around. The client is returned with its reference count incremented
 * by one. The caller must call h_ReleaseClient_r when finished with
 * the client.
 */
struct client *
h_FindClient_r(struct rx_connection *tcon)
{
    register struct client *client;
    register struct host *host;
    struct client *oldClient;
    afs_int32 viceid;
    afs_int32 expTime;
    afs_int32 code;
    int authClass;
#if (64-MAXKTCNAMELEN)
    ticket name length != 64
#endif
    char tname[64];
    char tinst[64];
    char uname[PR_MAXNAMELEN];
    char tcell[MAXKTCREALMLEN];
    int fail = 0;

    client = (struct client *)rx_GetSpecific(tcon, rxcon_client_key);
    if (client && !client->deleted) {
	client->refCount++;
	h_Hold_r(client->host);
	if (client->prfail != 2) {	/* Could add shared lock on client here */
	    /* note that we don't have to lock entry in this path to
	     * ensure CPS is initialized, since we don't call rx_SetSpecific
	     * until initialization is done, and we only get here if
	     * rx_GetSpecific located the client structure.
	     */
	    return client;
	}
	H_UNLOCK;
	ObtainWriteLock(&client->lock);	/* released at end */
	H_LOCK;
    } else if (client) {
	client->refCount++;
    }

    authClass = rx_SecurityClassOf((struct rx_connection *)tcon);
    ViceLog(5,
	    ("FindClient: authenticating connection: authClass=%d\n",
	     authClass));
    if (authClass == 1) {
	/* A bcrypt tickets, no longer supported */
	ViceLog(1, ("FindClient: bcrypt ticket, using AnonymousID\n"));
	viceid = AnonymousID;
	expTime = 0x7fffffff;
    } else if (authClass == 2) {
	afs_int32 kvno;

	/* kerberos ticket */
	code = rxkad_GetServerInfo(tcon, /*level */ 0, &expTime,
				   tname, tinst, tcell, &kvno);
	if (code) {
	    ViceLog(1, ("Failed to get rxkad ticket info\n"));
	    viceid = AnonymousID;
	    expTime = 0x7fffffff;
	} else {
	    int ilen = strlen(tinst);
	    ViceLog(5,
		    ("FindClient: rxkad conn: name=%s,inst=%s,cell=%s,exp=%d,kvno=%d\n",
		     tname, tinst, tcell, expTime, kvno));
	    strncpy(uname, tname, sizeof(uname));
	    if (ilen) {
		if (strlen(uname) + 1 + ilen >= sizeof(uname))
		    goto bad_name;
		strcat(uname, ".");
		strcat(uname, tinst);
	    }
	    /* translate the name to a vice id */
	    code = MapName_r(uname, tcell, &viceid);
	    if (code) {
	      bad_name:
		ViceLog(1,
			("failed to map name=%s, cell=%s -> code=%d\n", uname,
			 tcell, code));
		fail = 1;
		viceid = AnonymousID;
		expTime = 0x7fffffff;
	    }
	}
    } else {
	viceid = AnonymousID;	/* unknown security class */
	expTime = 0x7fffffff;
    }

    if (!client) {
	host = h_GetHost_r(tcon);	/* Returns it h_Held */

    retryfirstclient:
	/* First try to find the client structure */
	for (client = host->FirstClient; client; client = client->next) {
	    if (!client->deleted && (client->sid == rxr_CidOf(tcon))
		&& (client->VenusEpoch == rxr_GetEpoch(tcon))) {
		if (client->tcon && (client->tcon != tcon)) {
		    ViceLog(0,
			    ("*** Vid=%d, sid=%x, tcon=%x, Tcon=%x ***\n",
			     client->ViceId, client->sid, client->tcon,
			     tcon));
		    oldClient =
			(struct client *)rx_GetSpecific(client->tcon,
							rxcon_client_key);
		    if (oldClient) {
			if (oldClient == client)
			    rx_SetSpecific(client->tcon, rxcon_client_key,
					   NULL);
			else
			    ViceLog(0,
				    ("Client-conn mismatch: CL1=%x, CN=%x, CL2=%x\n",
				     client, client->tcon, oldClient));
		    }
		    client->tcon = (struct rx_connection *)0;
		}
		client->refCount++;
		H_UNLOCK;
		ObtainWriteLock(&client->lock);
		H_LOCK;
		break;
	    }
	}

	/* Still no client structure - get one */
	if (!client) {
	    h_Lock_r(host);
	    /* Retry to find the client structure */
	    for (client = host->FirstClient; client; client = client->next) {
		if (!client->deleted && (client->sid == rxr_CidOf(tcon))
		    && (client->VenusEpoch == rxr_GetEpoch(tcon))) {
		    h_Unlock_r(host);
		    goto retryfirstclient;
		}
	    }
	    client = GetCE();
	    ObtainWriteLock(&client->lock);
	    client->refCount = 1;
	    client->host = host;
	    client->next = host->FirstClient;
	    host->FirstClient = client;
#if FS_STATS_DETAILED
	    client->InSameNetwork = host->InSameNetwork;
#endif /* FS_STATS_DETAILED */
	    client->ViceId = viceid;
	    client->expTime = expTime;	/* rx only */
	    client->authClass = authClass;	/* rx only */
	    client->sid = rxr_CidOf(tcon);
	    client->VenusEpoch = rxr_GetEpoch(tcon);
	    client->CPS.prlist_val = 0;
	    client->CPS.prlist_len = 0;
	    h_Unlock_r(host);
	    CurrentConnections++;	/* increment number of connections */
	}
    }
    client->prfail = fail;

    if (!(client->CPS.prlist_val) || (viceid != client->ViceId)) {
	if (client->CPS.prlist_val && (client->ViceId != ANONYMOUSID)) {
	    free(client->CPS.prlist_val);
	}
	client->CPS.prlist_val = NULL;
	client->CPS.prlist_len = 0;
	client->ViceId = viceid;
	client->expTime = expTime;

	if (viceid == ANONYMOUSID) {
	    client->CPS.prlist_len = AnonCPS.prlist_len;
	    client->CPS.prlist_val = AnonCPS.prlist_val;
	} else {
	    H_UNLOCK;
	    code = pr_GetCPS(viceid, &client->CPS);
	    H_LOCK;
	    if (code) {
		char hoststr[16];
		ViceLog(0,
			("pr_GetCPS failed(%d) for user %d, host %s:%d\n",
			 code, viceid, afs_inet_ntoa_r(client->host->host,
						       hoststr),
			 ntohs(client->host->port)));

		/* Although ubik_Call (called by pr_GetCPS) traverses thru
		 * all protection servers and reevaluates things if no
		 * sync server or quorum is found we could still end up
		 * with one of these errors. In such case we would like to
		 * reevaluate the rpc call to find if there's cps for this
		 * guy. We treat other errors (except network failures
		 * ones - i.e. code < 0) as an indication that there is no
		 * CPS for this host.  Ideally we could like to deal this
		 * problem the other way around (i.e.  if code == NOCPS
		 * ignore else retry next time) but the problem is that
		 * there're other errors (i.e.  EPERM) for which we don't
		 * want to retry and we don't know the whole code list!
		 */
		if (code < 0 || code == UNOQUORUM || code == UNOTSYNC)
		    client->prfail = 1;
	    }
	}
	/* the disabling of system:administrators is so iffy and has so many
	 * possible failure modes that we will disable it again */
	/* Turn off System:Administrator for safety  
	 * if (AL_IsAMember(SystemId, client->CPS) == 0)
	 * assert(AL_DisableGroup(SystemId, client->CPS) == 0); */
    }

    /* Now, tcon may already be set to a rock, since we blocked with no host
     * or client locks set above in pr_GetCPS (XXXX some locking is probably
     * required).  So, before setting the RPC's rock, we should disconnect
     * the RPC from the other client structure's rock.
     */
    oldClient = (struct client *)rx_GetSpecific(tcon, rxcon_client_key);
    if (oldClient && oldClient->tcon == tcon) {
	char hoststr[16];
	oldClient->tcon = (struct rx_connection *)0;
	ViceLog(0, ("FindClient: client %x(%x) already had conn %x (host %s:%d), stolen by client %x(%x)\n", 
		    oldClient, oldClient->sid, tcon, 
		    afs_inet_ntoa_r(rxr_HostOf(tcon), hoststr),
		    ntohs(rxr_PortOf(tcon)),
		    client, client->sid));
	/* rx_SetSpecific will be done immediately below */
    }
    client->tcon = tcon;
    rx_SetSpecific(tcon, rxcon_client_key, client);
    ReleaseWriteLock(&client->lock);

    return client;

}				/*h_FindClient_r */

int
h_ReleaseClient_r(struct client *client)
{
    assert(client->refCount > 0);
    client->refCount--;
    return 0;
}


/*
 * Sigh:  this one is used to get the client AGAIN within the individual
 * server routines.  This does not bother h_Holding the host, since
 * this is assumed already have been done by the server main loop.
 * It does check tokens, since only the server routines can return the
 * VICETOKENDEAD error code
 */
int
GetClient(struct rx_connection *tcon, struct client **cp)
{
    register struct client *client;

    H_LOCK;
    *cp = client = (struct client *)rx_GetSpecific(tcon, rxcon_client_key);
    if (client == NULL || client->tcon == NULL) {
	ViceLog(0,
		("GetClient: no client in conn %x (host %x:%d), VBUSYING\n",
		 tcon, rxr_HostOf(tcon),rxr_PortOf(tcon)));
	H_UNLOCK;
	return VBUSY;
    }
    if (rxr_CidOf(client->tcon) != client->sid) {
	ViceLog(0,
		("GetClient: tcon %x tcon sid %d client sid %d\n",
		 client->tcon, rxr_CidOf(client->tcon), client->sid));
	H_UNLOCK;
	return VBUSY;
    }
    if (!(client && client->tcon && rxr_CidOf(client->tcon) == client->sid)) {
	if (!client)
	    ViceLog(0, ("GetClient: no client in conn %x\n", tcon));
	else
	    ViceLog(0,
		    ("GetClient: tcon %x tcon sid %d client sid %d\n",
		     client->tcon, client->tcon ? rxr_CidOf(client->tcon)
		     : -1, client->sid));
	assert(0);
    }
    if (client && client->LastCall > client->expTime && client->expTime) {
	char hoststr[16];
	ViceLog(1,
		("Token for %s at %s:%d expired %d\n", h_UserName(client),
		 afs_inet_ntoa_r(client->host->host, hoststr),
		 ntohs(client->host->port), client->expTime));
	H_UNLOCK;
	return VICETOKENDEAD;
    }

    H_UNLOCK;
    return 0;

}				/*GetClient */


/* Client user name for short term use.  Note that this is NOT inexpensive */
char *
h_UserName(struct client *client)
{
    static char User[PR_MAXNAMELEN + 1];
    namelist lnames;
    idlist lids;

    lids.idlist_len = 1;
    lids.idlist_val = (afs_int32 *) malloc(1 * sizeof(afs_int32));
    if (!lids.idlist_val) {
	ViceLog(0, ("Failed malloc in h_UserName\n"));
	assert(0);
    }
    lnames.namelist_len = 0;
    lnames.namelist_val = (prname *) 0;
    lids.idlist_val[0] = client->ViceId;
    if (pr_IdToName(&lids, &lnames)) {
	/* We need to free id we alloced above! */
	free(lids.idlist_val);
	return "*UNKNOWN USER NAME*";
    }
    strncpy(User, lnames.namelist_val[0], PR_MAXNAMELEN);
    free(lids.idlist_val);
    free(lnames.namelist_val);
    return User;

}				/*h_UserName */


void
h_PrintStats()
{
    ViceLog(0,
	    ("Total Client entries = %d, blocks = %d; Host entries = %d, blocks = %d\n",
	     CEs, CEBlocks, HTs, HTBlocks));

}				/*h_PrintStats */


static int
h_PrintClient(register struct host *host, int held, StreamHandle_t * file)
{
    register struct client *client;
    int i;
    char tmpStr[256];
    char tbuffer[32];
    char hoststr[16];

    H_LOCK;
    if (host->hostFlags & HOSTDELETED) {
	H_UNLOCK;
	return held;
    }
    (void)afs_snprintf(tmpStr, sizeof tmpStr,
		       "Host %s:%d down = %d, LastCall %s",
		       afs_inet_ntoa_r(host->host, hoststr),
		       ntohs(host->port), (host->hostFlags & VENUSDOWN),
		       afs_ctime((time_t *) & host->LastCall, tbuffer,
				 sizeof(tbuffer)));
    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    for (client = host->FirstClient; client; client = client->next) {
	if (!client->deleted) {
	    if (client->tcon) {
		(void)afs_snprintf(tmpStr, sizeof tmpStr,
				   "    user id=%d,  name=%s, sl=%s till %s",
				   client->ViceId, h_UserName(client),
				   client->
				   authClass ? "Authenticated" :
				   "Not authenticated",
				   client->
				   authClass ? afs_ctime((time_t *) & client->
							 expTime, tbuffer,
							 sizeof(tbuffer))
				   : "No Limit\n");
		(void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	    } else {
		(void)afs_snprintf(tmpStr, sizeof tmpStr,
				   "    user=%s, no current server connection\n",
				   h_UserName(client));
		(void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	    }
	    (void)afs_snprintf(tmpStr, sizeof tmpStr, "      CPS-%d is [",
			       client->CPS.prlist_len);
	    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	    if (client->CPS.prlist_val) {
		for (i = 0; i > client->CPS.prlist_len; i++) {
		    (void)afs_snprintf(tmpStr, sizeof tmpStr, " %d",
				       client->CPS.prlist_val[i]);
		    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
		}
	    }
	    sprintf(tmpStr, "]\n");
	    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	}
    }
    H_UNLOCK;
    return held;

}				/*h_PrintClient */



/*
 * Print a list of clients, with last security level and token value seen,
 * if known
 */
void
h_PrintClients()
{
    time_t now;
    char tmpStr[256];
    char tbuffer[32];

    StreamHandle_t *file = STREAM_OPEN(AFSDIR_SERVER_CLNTDUMP_FILEPATH, "w");

    if (file == NULL) {
	ViceLog(0,
		("Couldn't create client dump file %s\n",
		 AFSDIR_SERVER_CLNTDUMP_FILEPATH));
	return;
    }
    now = FT_ApproxTime();
    (void)afs_snprintf(tmpStr, sizeof tmpStr, "List of active users at %s\n",
		       afs_ctime(&now, tbuffer, sizeof(tbuffer)));
    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    h_Enumerate(h_PrintClient, (char *)file);
    STREAM_REALLYCLOSE(file);
    ViceLog(0, ("Created client dump %s\n", AFSDIR_SERVER_CLNTDUMP_FILEPATH));
}




static int
h_DumpHost(register struct host *host, int held, StreamHandle_t * file)
{
    int i;
    char tmpStr[256];
    char hoststr[16];

    H_LOCK;
    (void)afs_snprintf(tmpStr, sizeof tmpStr,
		       "ip:%s port:%d hidx:%d cbid:%d lock:%x last:%u active:%u down:%d del:%d cons:%d cldel:%d\n\t hpfailed:%d hcpsCall:%u hcps [",
		       afs_inet_ntoa_r(host->host, hoststr), ntohs(host->port), host->index,
		       host->cblist, CheckLock(&host->lock), host->LastCall,
		       host->ActiveCall, (host->hostFlags & VENUSDOWN),
		       host->hostFlags & HOSTDELETED, host->Console,
		       host->hostFlags & CLIENTDELETED, host->hcpsfailed,
		       host->cpsCall);
    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    if (host->hcps.prlist_val)
	for (i = 0; i < host->hcps.prlist_len; i++) {
	    (void)afs_snprintf(tmpStr, sizeof tmpStr, " %d",
			       host->hcps.prlist_val[i]);
	    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	}
    sprintf(tmpStr, "] [");
    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    if (host->interface)
	for (i = 0; i < host->interface->numberOfInterfaces; i++) {
	    char hoststr[16];
	    sprintf(tmpStr, " %s:%d", 
		     afs_inet_ntoa_r(host->interface->interface[i].addr, hoststr),
		     ntohs(host->interface->interface[i].port));
	    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	}
    sprintf(tmpStr, "] holds: ");
    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);

    for (i = 0; i < h_maxSlots; i++) {
	sprintf(tmpStr, "%04x", host->holds[i]);
	(void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    }
    sprintf(tmpStr, " slot/bit: %d/%d\n", h_holdSlot(), h_holdbit());
    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);

    H_UNLOCK;
    return held;

}				/*h_DumpHost */


void
h_DumpHosts()
{
    time_t now;
    StreamHandle_t *file = STREAM_OPEN(AFSDIR_SERVER_HOSTDUMP_FILEPATH, "w");
    char tmpStr[256];
    char tbuffer[32];

    if (file == NULL) {
	ViceLog(0,
		("Couldn't create host dump file %s\n",
		 AFSDIR_SERVER_HOSTDUMP_FILEPATH));
	return;
    }
    now = FT_ApproxTime();
    (void)afs_snprintf(tmpStr, sizeof tmpStr, "List of active hosts at %s\n",
		       afs_ctime(&now, tbuffer, sizeof(tbuffer)));
    (void)STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    h_Enumerate(h_DumpHost, (char *)file);
    STREAM_REALLYCLOSE(file);
    ViceLog(0, ("Created host dump %s\n", AFSDIR_SERVER_HOSTDUMP_FILEPATH));

}				/*h_DumpHosts */


/*
 * This counts the number of workstations, the number of active workstations,
 * and the number of workstations declared "down" (i.e. not heard from
 * recently).  An active workstation has received a call since the cutoff
 * time argument passed.
 */
void
h_GetWorkStats(int *nump, int *activep, int *delp, afs_int32 cutofftime)
{
    register struct host *host;
    register int num = 0, active = 0, del = 0;

    H_LOCK;
    for (host = hostList; host; host = host->next) {
	if (!(host->hostFlags & HOSTDELETED)) {
	    num++;
	    if (host->ActiveCall > cutofftime)
		active++;
	    if (host->hostFlags & VENUSDOWN)
		del++;
	}
    }
    H_UNLOCK;
    if (nump)
	*nump = num;
    if (activep)
	*activep = active;
    if (delp)
	*delp = del;

}				/*h_GetWorkStats */


/*------------------------------------------------------------------------
 * PRIVATE h_ClassifyAddress
 *
 * Description:
 *	Given a target IP address and a candidate IP address (both
 *	in host byte order), classify the candidate into one of three
 *	buckets in relation to the target by bumping the counters passed
 *	in as parameters.
 *
 * Arguments:
 *	a_targetAddr	   : Target address.
 *	a_candAddr	   : Candidate address.
 *	a_sameNetOrSubnetP : Ptr to counter to bump when the two
 *			     addresses are either in the same network
 *			     or the same subnet.
 *	a_diffSubnetP	   : ...when the candidate is in a different
 *			     subnet.
 *	a_diffNetworkP	   : ...when the candidate is in a different
 *			     network.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	The target and candidate addresses are both in host byte
 *	order, NOT network byte order, when passed in.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void
h_ClassifyAddress(afs_uint32 a_targetAddr, afs_uint32 a_candAddr,
		  afs_int32 * a_sameNetOrSubnetP, afs_int32 * a_diffSubnetP,
		  afs_int32 * a_diffNetworkP)
{				/*h_ClassifyAddress */

    afs_uint32 targetNet;
    afs_uint32 targetSubnet;
    afs_uint32 candNet;
    afs_uint32 candSubnet;

    /*
     * Put bad values into the subnet info to start with.
     */
    targetSubnet = (afs_uint32) 0;
    candSubnet = (afs_uint32) 0;

    /*
     * Pull out the network and subnetwork numbers from the target
     * and candidate addresses.  We can short-circuit this whole
     * affair if the target and candidate addresses are not of the
     * same class.
     */
    if (IN_CLASSA(a_targetAddr)) {
	if (!(IN_CLASSA(a_candAddr))) {
	    (*a_diffNetworkP)++;
	    return;
	}
	targetNet = a_targetAddr & IN_CLASSA_NET;
	candNet = a_candAddr & IN_CLASSA_NET;
	if (IN_SUBNETA(a_targetAddr))
	    targetSubnet = a_targetAddr & IN_CLASSA_SUBNET;
	if (IN_SUBNETA(a_candAddr))
	    candSubnet = a_candAddr & IN_CLASSA_SUBNET;
    } else if (IN_CLASSB(a_targetAddr)) {
	if (!(IN_CLASSB(a_candAddr))) {
	    (*a_diffNetworkP)++;
	    return;
	}
	targetNet = a_targetAddr & IN_CLASSB_NET;
	candNet = a_candAddr & IN_CLASSB_NET;
	if (IN_SUBNETB(a_targetAddr))
	    targetSubnet = a_targetAddr & IN_CLASSB_SUBNET;
	if (IN_SUBNETB(a_candAddr))
	    candSubnet = a_candAddr & IN_CLASSB_SUBNET;
    } /*Class B target */
    else if (IN_CLASSC(a_targetAddr)) {
	if (!(IN_CLASSC(a_candAddr))) {
	    (*a_diffNetworkP)++;
	    return;
	}
	targetNet = a_targetAddr & IN_CLASSC_NET;
	candNet = a_candAddr & IN_CLASSC_NET;

	/*
	 * Note that class C addresses can't have subnets,
	 * so we leave the defaults untouched.
	 */
    } /*Class C target */
    else {
	targetNet = a_targetAddr;
	candNet = a_candAddr;
    }				/*Class D address */

    /*
     * Now, simply compare the extracted net and subnet values for
     * the two addresses (which at this point are known to be of the
     * same class)
     */
    if (targetNet == candNet) {
	if (targetSubnet == candSubnet)
	    (*a_sameNetOrSubnetP)++;
	else
	    (*a_diffSubnetP)++;
    } else
	(*a_diffNetworkP)++;

}				/*h_ClassifyAddress */


/*------------------------------------------------------------------------
 * EXPORTED h_GetHostNetStats
 *
 * Description:
 *	Iterate through the host table, and classify each (non-deleted)
 *	host entry into ``proximity'' categories (same net or subnet,
 *	different subnet, different network).
 *
 * Arguments:
 *	a_numHostsP	   : Set to total number of (non-deleted) hosts.
 *	a_sameNetOrSubnetP : Set to # hosts on same net/subnet as server.
 *	a_diffSubnetP	   : Set to # hosts on diff subnet as server.
 *	a_diffNetworkP	   : Set to # hosts on diff network as server.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	We only count non-deleted hosts.  The storage pointed to by our
 *	parameters is zeroed upon entry.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
h_GetHostNetStats(afs_int32 * a_numHostsP, afs_int32 * a_sameNetOrSubnetP,
		  afs_int32 * a_diffSubnetP, afs_int32 * a_diffNetworkP)
{				/*h_GetHostNetStats */

    register struct host *hostP;	/*Ptr to current host entry */
    register afs_uint32 currAddr_HBO;	/*Curr host addr, host byte order */

    /*
     * Clear out the storage pointed to by our parameters.
     */
    *a_numHostsP = (afs_int32) 0;
    *a_sameNetOrSubnetP = (afs_int32) 0;
    *a_diffSubnetP = (afs_int32) 0;
    *a_diffNetworkP = (afs_int32) 0;

    H_LOCK;
    for (hostP = hostList; hostP; hostP = hostP->next) {
	if (!(hostP->hostFlags & HOSTDELETED)) {
	    /*
	     * Bump the number of undeleted host entries found.
	     * In classifying the current entry's address, make
	     * sure to first convert to host byte order.
	     */
	    (*a_numHostsP)++;
	    currAddr_HBO = (afs_uint32) ntohl(hostP->host);
	    h_ClassifyAddress(FS_HostAddr_HBO, currAddr_HBO,
			      a_sameNetOrSubnetP, a_diffSubnetP,
			      a_diffNetworkP);
	}			/*Only look at non-deleted hosts */
    }				/*For each host record hashed to this index */
    H_UNLOCK;
}				/*h_GetHostNetStats */

static afs_uint32 checktime;
static afs_uint32 clientdeletetime;
static struct AFSFid zerofid;


/*
 * XXXX: This routine could use Multi-Rx to avoid serializing the timeouts.
 * Since it can serialize them, and pile up, it should be a separate LWP
 * from other events.
 */
int
CheckHost(register struct host *host, int held)
{
    register struct client *client;
    struct rx_connection *cb_conn = NULL;
    int code;

    /* Host is held by h_Enumerate */
    H_LOCK;
    for (client = host->FirstClient; client; client = client->next) {
	if (client->refCount == 0 && client->LastCall < clientdeletetime) {
	    client->deleted = 1;
	    host->hostFlags |= CLIENTDELETED;
	}
    }
    if (host->LastCall < checktime) {
	h_Lock_r(host);
	if (!(host->hostFlags & HOSTDELETED)) {
	    cb_conn = host->callback_rxcon;
	    rx_GetConnection(cb_conn);
	    if (host->LastCall < clientdeletetime) {
		host->hostFlags |= HOSTDELETED;
		if (!(host->hostFlags & VENUSDOWN)) {
		    host->hostFlags &= ~ALTADDR;	/* alternate address invalid */
		    if (host->interface) {
			H_UNLOCK;
			code =
			    RXAFSCB_InitCallBackState3(cb_conn,
						       &FS_HostUUID);
			H_LOCK;
		    } else {
			H_UNLOCK;
			code =
			    RXAFSCB_InitCallBackState(cb_conn);
			H_LOCK;
		    }
		    host->hostFlags |= ALTADDR;	/* alternate addresses valid */
		    if (code) {
			char hoststr[16];
			(void)afs_inet_ntoa_r(host->host, hoststr);
			ViceLog(0,
				("CB: RCallBackConnectBack (host.c) failed for host %s:%d\n",
				 hoststr, ntohs(host->port)));
			host->hostFlags |= VENUSDOWN;
		    }
		    /* Note:  it's safe to delete hosts even if they have call
		     * back state, because break delayed callbacks (called when a
		     * message is received from the workstation) will always send a 
		     * break all call backs to the workstation if there is no
		     *callback.
		     */
		}
	    } else {
		if (!(host->hostFlags & VENUSDOWN) && host->cblist) {
		    if (host->interface) {
			afsUUID uuid = host->interface->uuid;
			H_UNLOCK;
			code = RXAFSCB_ProbeUuid(cb_conn, &uuid);
			H_LOCK;
			if (code) {
			    if (MultiProbeAlternateAddress_r(host)) {
				char hoststr[16];
				(void)afs_inet_ntoa_r(host->host, hoststr);
				ViceLog(0,
					("ProbeUuid failed for host %s:%d\n",
					 hoststr, ntohs(host->port)));
				host->hostFlags |= VENUSDOWN;
			    }
			}
		    } else {
			H_UNLOCK;
			code = RXAFSCB_Probe(cb_conn);
			H_LOCK;
			if (code) {
			    char hoststr[16];
			    (void)afs_inet_ntoa_r(host->host, hoststr);
			    ViceLog(0,
				    ("Probe failed for host %s:%d\n", hoststr,
				     ntohs(host->port)));
			    host->hostFlags |= VENUSDOWN;
			}
		    }
		}
	    }
	    H_UNLOCK;
	    rx_PutConnection(cb_conn);
	    cb_conn=NULL;
	    H_LOCK;
	}
	h_Unlock_r(host);
    }
    H_UNLOCK;
    return held;

}				/*CheckHost */


/*
 * Set VenusDown for any hosts that have not had a call in 15 minutes and
 * don't respond to a probe.  Note that VenusDown can only be cleared if
 * a message is received from the host (see ServerLWP in file.c).
 * Delete hosts that have not had any calls in 1 hour, clients that
 * have not had any calls in 15 minutes.
 *
 * This routine is called roughly every 5 minutes.
 */
void
h_CheckHosts()
{
    afs_uint32 now = FT_ApproxTime();

    memset((char *)&zerofid, 0, sizeof(zerofid));
    /*
     * Send a probe to the workstation if it hasn't been heard from in
     * 15 minutes
     */
    checktime = now - 15 * 60;
    clientdeletetime = now - 120 * 60;	/* 2 hours ago */
    h_Enumerate(CheckHost, NULL);

}				/*h_CheckHosts */

/*
 * This is called with host locked and held. At this point, the
 * hostHashTable should not have any entries for the alternate
 * interfaces. This function has to insert these entries in the
 * hostHashTable.
 *
 * The addresses in the interfaceAddr list are in host byte order.
 */
int
initInterfaceAddr_r(struct host *host, struct interfaceAddr *interf)
{
    int i, j;
    int number, count;
    afs_uint32 myAddr;
    afs_uint16 myPort;
    int found;
    struct Interface *interface;

    assert(host);
    assert(interf);

    ViceLog(125,
	    ("initInterfaceAddr : host %x numAddr %d\n", host->host,
	     interf->numberOfInterfaces));

    number = interf->numberOfInterfaces;
    myAddr = host->host;	/* current interface address */
    myPort = host->port;	/* current port */

    /* validation checks */
    if (number < 0 || number > AFS_MAX_INTERFACE_ADDR) {
	ViceLog(0,
		("Number of alternate addresses returned is %d\n", number));
	return -1;
    }

    /*
     * Convert IP addresses to network byte order, and remove for
     * duplicate IP addresses from the interface list.
     */
    for (i = 0, count = 0, found = 0; i < number; i++) {
	interf->addr_in[i] = htonl(interf->addr_in[i]);
	for (j = 0; j < count; j++) {
	    if (interf->addr_in[j] == interf->addr_in[i])
		break;
	}
	if (j == count) {
	    interf->addr_in[count] = interf->addr_in[i];
	    if (interf->addr_in[count] == myAddr)
		found = 1;
	    count++;
	}
    }

    /*
     * Allocate and initialize an interface structure for this host.
     */
    if (found) {
	interface = (struct Interface *)
	    malloc(sizeof(struct Interface) +
		   (sizeof(struct AddrPort) * (count - 1)));
	if (!interface) {
	    ViceLog(0, ("Failed malloc in initInterfaceAddr_r\n"));
	    assert(0);
	}
	interface->numberOfInterfaces = count;
    } else {
	interface = (struct Interface *)
	    malloc(sizeof(struct Interface) + (sizeof(struct AddrPort) * count));
	assert(interface);
	interface->numberOfInterfaces = count + 1;
	interface->interface[count].addr = myAddr;
	interface->interface[count].port = myPort;
    }
    interface->uuid = interf->uuid;
    for (i = 0; i < count; i++) {
	interface->interface[i].addr = interf->addr_in[i];
	/* We store the port as 7001 because the addresses reported by 
	 * TellMeAboutYourself and WhoAreYou RPCs are only valid if they
	 * are coming from fully connected hosts (no NAT/PATs)
	 */
	interface->interface[i].port = htons(7001);
    }

    assert(!host->interface);
    host->interface = interface;

    for (i = 0; i < host->interface->numberOfInterfaces; i++) {
	char hoststr[16];
	ViceLog(125, ("--- alt address %s:%d\n", 
		       afs_inet_ntoa_r(host->interface->interface[i].addr, hoststr),
		       ntohs(host->interface->interface[i].port)));
    }

    return 0;
}

/* deleted a HashChain structure for this address and host */
/* returns 1 on success */
int
hashDelete_r(afs_uint32 addr, afs_uint16 port, struct host *host)
{
    int flag;
    register struct h_hashChain **hp, *th;

    for (hp = &hostHashTable[h_HashIndex(addr)]; (th = *hp);) {
	assert(th->hostPtr);
	if (th->hostPtr == host && th->addr == addr && th->port == port) {
	    *hp = th->next;
	    free(th);
	    flag = 1;
	    break;
	} else {
	    hp = &th->next;
	}
    }
    return flag;
}


/*
** prints out all alternate interface address for the host. The 'level'
** parameter indicates what level of debugging sets this output
*/
void
printInterfaceAddr(struct host *host, int level)
{
    int i, number;
    char hoststr[16];

    if (host->interface) {
	/* check alternate addresses */
	number = host->interface->numberOfInterfaces;
	assert(number > 0);
	for (i = 0; i < number; i++)
	    ViceLog(level, ("%s:%d ", afs_inet_ntoa_r(host->interface->interface[i].addr, hoststr),
			     ntohs(host->interface->interface[i].port)));
    }
    ViceLog(level, ("\n"));
}
