/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

#ifndef _AFS_VICED_HOST_H
#define _AFS_VICED_HOST_H

#include "fs_stats.h"		/*File Server stats package */

#ifdef AFS_PTHREAD_ENV
/*
 * There are three locks in the host package.
 * the global hash lock protects hash chains.
 * the global list lock protects the list of hosts.
 * a mutex in each host structure protects the structure.
 * precedence is host_listlock_mutex, host->mutex, host_glock_mutex.
 */
#include <rx/rx_globals.h>
#include <afs/afs_assert.h>
#include <pthread.h>
extern pthread_mutex_t host_glock_mutex;
#define H_LOCK MUTEX_ENTER(&host_glock_mutex);
#define H_UNLOCK MUTEX_EXIT(&host_glock_mutex);
extern pthread_key_t viced_uclient_key;
#else /* AFS_PTHREAD_ENV */
#define H_LOCK
#define H_UNLOCK
#endif /* AFS_PTHREAD_ENV */

#define h_MAXHOSTTABLEENTRIES 1000
#define h_HASHENTRIES 256	/* Power of 2 */
#define h_MAXHOSTTABLES 200
#define h_HTSPERBLOCK 512	/* Power of 2 */
#define h_HTSHIFT 9		/* log base 2 of HTSPERBLOCK */

struct Identity {
    char valid;			/* zero if UUID is unknown */
    afsUUID uuid;
};

struct AddrPort  {
    afs_uint32 addr;		/* in network byte order */
    afs_uint16 port;		/* in network byte order */
    afs_int16  valid;
};

struct Interface {
    afsUUID uuid;
    int numberOfInterfaces;
    struct AddrPort interface[1];/* there are actually more than one here */
    /* in network byte order */
};

struct host {
    struct host *next, *prev;	/* linked list of all hosts */
    struct rx_connection *callback_rxcon;	/* rx callback connection */
    afs_uint32 refCount; /* reference count */
    afs_uint32 host;		/* IP address of host interface that is
				 * currently being used, in network
				 * byte order */
    afs_uint16 port;		/* port address of host */
    char Console;		/* XXXX This host is a console */
    unsigned short hostFlags;		/*  bit map */
#if FS_STATS_DETAILED
    char InSameNetwork;		/*Is host's addr in the same network as
				 * the File Server's? */
    char dummy[3];		/* for padding */
#endif				/* FS_STATS_DETAILED */
    char hcpsfailed;		/* Retry the cps call next time */
    prlist hcps;		/* cps for hostip acls */
    afs_uint32 LastCall;	/* time of last call from host */
    afs_uint32 ActiveCall;	/* time of any call but gettime,
                                   getstats and getcaps */
    struct client *FirstClient;	/* first connection from host */
    afs_uint32 cpsCall;		/* time of last cps call from this host */
    struct Interface *interface;	/* all alternate addr for client */
    afs_uint32 cblist;		/* index of a cb in the per-host circular CB list */
    /*
     * These don't get zeroed, keep them at the end. If index doesn't
     * follow an unsigned short then we need to pad to ensure that
     * the index fields isn't zeroed. XXX
     */
    afs_uint32 index;		/* Host table index, for vicecb.c */
    struct Lock lock;		/* Write lock for synchronization of
				 * VenusDown flag */
#ifdef AFS_PTHREAD_ENV
    pthread_cond_t cond;	/* used to wait on hcpsValid */
#endif				/* AFS_PTHREAD_ENV */
};

/* * Don't zero the index, lock or condition varialbles */
#define HOST_TO_ZERO(H) (int)(((char *)(&((H)->index))-(char *)(H)))

struct h_AddrHashChain {
    struct host *hostPtr;
    struct h_AddrHashChain *next;
    afs_uint32 addr;
    afs_uint16 port;
};

struct h_UuidHashChain {
    struct host *hostPtr;
    struct h_UuidHashChain *next;
};

struct client {
    struct client *next;	/* next client entry for host */
    struct host *host;		/* ptr to parent host entry */
    afs_int32 sid;		/* Connection number from this host */
    prlist CPS;			/* cps for authentication */
    int ViceId;			/* Vice ID of user */
    afs_int32 expTime;		/* RX-only: expiration time */
    afs_uint32 LastCall;	/* time of last call */
    afs_uint32 VenusEpoch;	/* Venus start time--used to identify
				 * venus.  Actually, now an extension of the
				 * sid, which is why it moved.
				 */
    afs_int32 refCount;		/* reference count */
    char deleted;		/* True if this client should be deleted
				 * when there are no more users of the
				 * structure */
    char authClass;		/* auth type, RX-only */
    char prfail;		/* True if prserver couldn't be contacted */
#if FS_STATS_DETAILED
    char InSameNetwork;		/* Is client's IP address in the same
				 * network as ours? */
#else				/* FS_STATS_DETAILED */
    char dummy;			/* For padding */
#endif				/* FS_STATS_DETAILED */
    struct Lock lock;		/* lock to ensure CPS valid if entry
				 * on host's clients list. */
};

/* Don't zero the lock */
#define CLIENT_TO_ZERO(C)	((int)(((char *)(&((C)->lock))-(char *)(C))))


/*
 * key for the client structure stored in connection specific data
 */
extern int rxcon_client_key;

/* Some additional functions to get at client information.  Client must have
   an active connection for this to work.  If a lwp is working on a request
   for the client, then the client must have a connection */
/* N.B. h_UserName returns pointer to static data; also relatively expensive */
extern char *h_UserName(struct client *client);
#define h_Lock(host)    ObtainWriteLock(&(host)->lock)
extern int h_Lock_r(struct host *host);
#define h_Unlock(host)  ReleaseWriteLock(&(host)->lock)
#define h_Unlock_r(host)  ReleaseWriteLock(&(host)->lock)

#define	AddCallBack(host, fid)	AddCallBack1((host), (fid), (afs_uint32 *)0, 1/*CB_NORMAL*/, 0)
#define	AddVolCallBack(host, fid) AddCallBack1((host), (fid), (afs_uint32 *)0, 3/*CB_VOLUME*/, 0)
#define	AddBulkCallBack(host, fid) AddCallBack1((host), (fid), (afs_uint32 *)0, 4/*CB_BULK*/, 0)

/* A simple refCount replaces per-thread hold mechanism.  The former
 * hold semantics are not different from refcounting, except with respect
 * to cross-thread assertions.  In this change, refcount is protected by
 * H_LOCK, just like former hold bitmap.  A future change will replace locks
 * th lock-free operations.  */

#define h_Hold_r(x) \
do { \
	++((x)->refCount); \
} while(0)

#define h_Decrement_r(x) \
do { \
	--((x)->refCount); \
} while (0)

#define h_Release_r(x) \
do { \
	h_Decrement_r(x); \
	if (((x)->refCount < 1) && \
		(((x)->hostFlags & HOSTDELETED) || \
		 ((x)->hostFlags & CLIENTDELETED))) h_TossStuff_r((x));	 \
} while(0)

/* operations on the global linked list of hosts */
#define h_InsertList_r(h) 	(h)->next =  hostList;			\
				(h)->prev = 0;				\
				hostList ? (hostList->prev = (h)):0; 	\
				hostList = (h);                         \
			        hostCount++;
#define h_DeleteList_r(h)	osi_Assert(hostCount>0);                    \
				hostCount--;                                \
				(h)->next ? ((h)->next->prev = (h)->prev):0;\
				(h)->prev ? ((h)->prev->next = (h)->next):0;\
				( h == hostList )? (hostList = h->next):0;

extern int DeleteAllCallBacks_r(struct host *host, int deletefe);
extern int DeleteCallBack(struct host *host, AFSFid * fid);
extern int MultiProbeAlternateAddress_r(struct host *host);
extern int BreakDelayedCallBacks_r(struct host *host);
extern int AddCallBack1(struct host *host, AFSFid * fid, afs_uint32 * thead, int type,
	     int locked);
extern int BreakCallBack(struct host *xhost, AFSFid * fid, int flag);
extern int DeleteFileCallBacks(AFSFid * fid);
extern int CleanupTimedOutCallBacks(void);
extern int CleanupTimedOutCallBacks_r(void);
extern int MultiBreakCallBackAlternateAddress(struct host *host, struct AFSCBFids *afidp);
extern int MultiBreakCallBackAlternateAddress_r(struct host *host,
				     struct AFSCBFids *afidp);
extern int DumpCallBackState(void);
extern int PrintCallBackStats(void);
extern void *ShutDown(void *);
extern void ShutDownAndCore(int dopanic);

extern int h_Lookup_r(afs_uint32 hostaddr, afs_uint16 hport,
		      struct host **hostp);
extern struct host *h_LookupUuid_r(afsUUID * uuidp);
extern void h_Enumerate(int (*proc) (struct host *, void *), void *param);
extern void h_Enumerate_r(int (*proc) (struct host *, void *), struct host *enumstart, void *param);
extern struct host *h_GetHost_r(struct rx_connection *tcon);
extern struct client *h_FindClient_r(struct rx_connection *tcon, afs_int32 *viceid);
extern int h_ReleaseClient_r(struct client *client);
extern void h_TossStuff_r(struct host *host);
extern void h_EnumerateClients(afs_int32 vid,
                               int (*proc)(struct client *client, void *rock),
                               void *arock);
extern int GetClient(struct rx_connection *tcon, struct client **cp);
extern int PutClient(struct client **cp);
extern void h_PrintStats(void);
extern void h_PrintClients(void);
extern void h_GetWorkStats(int *, int *, int *, afs_int32);
extern void h_GetWorkStats64(afs_uint64 *, afs_uint64 *, afs_uint64 *, afs_int32);
extern void h_flushhostcps(afs_uint32 hostaddr,
			   afs_uint16 hport);
extern void h_GetHostNetStats(afs_int32 * a_numHostsP, afs_int32 * a_sameNetOrSubnetP,
		  afs_int32 * a_diffSubnetP, afs_int32 * a_diffNetworkP);
extern int h_NBLock_r(struct host *host);
extern void h_DumpHosts(void);
extern void h_InitHostPackage(int hquota);
extern void h_CheckHosts(void );
extern int initInterfaceAddr_r(struct host *host, struct interfaceAddr *interf);
extern void h_AddHostToAddrHashTable_r(afs_uint32 addr, afs_uint16 port, struct host * host);
extern void h_AddHostToUuidHashTable_r(afsUUID * uuid, struct host * host);
extern int h_DeleteHostFromAddrHashTable_r(afs_uint32 addr, afs_uint16 port, struct host *host);
extern int h_DeleteHostFromUuidHashTable_r(struct host *host);
extern int initInterfaceAddr_r(struct host *host, struct interfaceAddr *interf);
extern int addInterfaceAddr_r(struct host *host, afs_uint32 addr, afs_uint16 port);
extern int removeInterfaceAddr_r(struct host *host, afs_uint32 addr, afs_uint16 port);
extern afs_int32 hpr_Initialize(struct ubik_client **);
extern int hpr_End(struct ubik_client *);
extern int hpr_IdToName(idlist *ids, namelist *names);
extern int hpr_NameToId(namelist *names, idlist *ids);

#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * state serialization
 */
extern int h_SaveState(void);
extern int h_RestoreState(void);
#endif

#define H_ENUMERATE_BAIL(flags)        ((flags)|0x80000000)
#define H_ENUMERATE_ISSET_BAIL(flags)  ((flags)&0x80000000)

struct host *(hosttableptrs[h_MAXHOSTTABLES]);	/* Used by h_itoh */
#define h_htoi(host) ((host)->index)	/* index isn't zeroed, no need to lock */
#define h_itoh(hostindex) (hosttableptrs[(hostindex)>>h_HTSHIFT]+((hostindex)&(h_HTSPERBLOCK-1)))

#define rxr_GetEpoch(aconn) (((struct rx_connection *)(aconn))->epoch)

#define rxr_CidOf(aconn) (((struct rx_connection *)(aconn))->cid)

#define rxr_PortOf(aconn) \
    rx_PortOf(rx_PeerOf(((struct rx_connection *)(aconn))))

#define rxr_HostOf(aconn) \
    rx_HostOf(rx_PeerOf((struct rx_connection *)(aconn)))

#define HCPS_INPROGRESS			0x01	/*set when CPS is being updated */
#define HCPS_WAITING			0x02	/*waiting for CPS to get updated */
#define ALTADDR				0x04	/*InitCallBack is being done */
#define VENUSDOWN			0x08	/* venus CallBack failed */
#define HOSTDELETED			0x10	/* host delated */
#define CLIENTDELETED			0x20	/* client deleted */
#define RESETDONE			0x40	/* callback reset done */
#define HFE_LATER                       0x80	/* host has FE_LATER callbacks */
#define HERRORTRANS                    0x100	/* do error translation */
#define HWHO_INPROGRESS                0x200    /* set when WhoAreYou running */
#define HCBREAK                        0x400    /* flag for a multi CB break */
#endif /* _AFS_VICED_HOST_H */
