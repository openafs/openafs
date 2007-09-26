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
#include <assert.h>
#include <pthread.h>
extern pthread_mutex_t host_glock_mutex;
#define H_LOCK \
    assert(pthread_mutex_lock(&host_glock_mutex) == 0)
#define H_UNLOCK \
    assert(pthread_mutex_unlock(&host_glock_mutex) == 0)
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

#define h_threadsPerSlot	32	/* bits per afs_int32 */
#define h_threadsShift		5	/* for multiply/divide */
#define h_threadsMask		31	/* for remainder */

/* size of the hold array for each host */
#define h_maxSlots	(((MAX_FILESERVER_THREAD+h_threadsPerSlot-1)>>h_threadsShift)+1)

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
    afs_int32 holds[h_maxSlots];
    /* holds on this host; 1 bit per lwp.
     * A hold prevents this structure and
     * inferior structures from disappearing */
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

/* all threads whose thread-id is greater than the size of the hold array,
** then use the most significant bit in the 'hold' field in the host structure 
*/
#ifdef AFS_PTHREAD_ENV
#define h_lwpIndex() ( ((long)(pthread_getspecific(rx_thread_id_key)) > \
		        ((h_maxSlots << h_threadsShift)-1)) ? \
				(h_maxSlots << h_threadsShift) -1 : \
				(long)(pthread_getspecific(rx_thread_id_key)) )
#else /* AFS_PTHREAD_ENV */
#define h_lwpIndex() ( (LWP_Index() > ((h_maxSlots << h_threadsShift)-1)) ? \
					(h_maxSlots << h_threadsShift) -1 : \
					LWP_Index() )
#endif /* AFS_PTHREAD_ENV */
#define h_holdIndex()( h_lwpIndex() & h_threadsMask)
#define h_holdSlot() ( h_lwpIndex() >> h_threadsShift)	/*index in 'holds' */
#define h_holdbit()  ( 1<<h_holdIndex() )

#define h_Hold_r(host)   ((host)->holds[h_holdSlot()] |= h_holdbit())
extern int h_Release(register struct host *host);
extern int h_Release_r(register struct host *host);

#define h_Held_r(host)   ((h_holdbit() & (host)->holds[h_holdSlot()]) != 0)
extern int h_OtherHolds_r(register struct host *host);
#define h_Lock(host)    ObtainWriteLock(&(host)->lock)
extern int h_Lock_r(register struct host *host);
#define h_Unlock(host)  ReleaseWriteLock(&(host)->lock)
#define h_Unlock_r(host)  ReleaseWriteLock(&(host)->lock)

#define	AddCallBack(host, fid)	AddCallBack1((host), (fid), (afs_uint32 *)0, 1/*CB_NORMAL*/, 0)
#define	AddVolCallBack(host, fid) AddCallBack1((host), (fid), (afs_uint32 *)0, 3/*CB_VOLUME*/, 0)
#define	AddBulkCallBack(host, fid) AddCallBack1((host), (fid), (afs_uint32 *)0, 4/*CB_BULK*/, 0)

/* operations on the global linked list of hosts */
#define h_InsertList_r(h) 	(h)->next =  hostList;			\
				(h)->prev = 0;				\
				hostList ? (hostList->prev = (h)):0; 	\
				hostList = (h);                         \
			        hostCount++;
#define h_DeleteList_r(h)	assert(hostCount>0);                        \
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
extern int ShutDown(void);
extern void ShutDownAndCore(int dopanic);

extern struct host *h_Alloc(register struct rx_connection *r_con);
extern struct host *h_Alloc_r(register struct rx_connection *r_con);
extern struct host *h_Lookup_r(afs_uint32 hostaddr, afs_uint16 hport,
			       int *heldp);
extern void   hashInsert_r(afs_uint32 addr, afs_uint16 port, 
			   struct host* host);
extern struct host *h_LookupUuid_r(afsUUID * uuidp);
extern void h_Enumerate(int (*proc) (), char *param);
extern void h_Enumerate_r(int (*proc) (), struct host *enumstart, char *param);
extern struct host *h_GetHost_r(struct rx_connection *tcon);
extern struct client *h_FindClient_r(struct rx_connection *tcon);
extern int h_ReleaseClient_r(struct client *client);
extern struct client *h_ID2Client(afs_int32 vid);
extern int GetClient(struct rx_connection *tcon, struct client **cp);
extern int PutClient(struct client **cp);
extern void h_PrintStats();
extern void h_PrintClients();
extern void h_GetWorkStats();
extern void h_flushhostcps(register afs_uint32 hostaddr,
			   register afs_uint16 hport);
extern void h_GetHostNetStats(afs_int32 * a_numHostsP, afs_int32 * a_sameNetOrSubnetP,
		  afs_int32 * a_diffSubnetP, afs_int32 * a_diffNetworkP);
extern int h_NBLock_r(register struct host *host);
extern void h_DumpHosts();
extern void h_InitHostPackage();
extern void h_CheckHosts();
struct Interface *MultiVerifyInterface_r();
extern int initInterfaceAddr_r(struct host *host, struct interfaceAddr *interf);
extern void h_AddHostToAddrHashTable_r(afs_uint32 addr, afs_uint16 port, struct host * host);
extern void h_AddHostToUuidHashTable_r(afsUUID * uuid, struct host * host);
extern int h_DeleteHostFromAddrHashTable_r(afs_uint32 addr, afs_uint16 port, struct host *host);
extern int initInterfaceAddr_r(struct host *host, struct interfaceAddr *interf);
extern int addInterfaceAddr_r(struct host *host, afs_uint32 addr, afs_uint16 port);
extern int removeInterfaceAddr_r(struct host *host, afs_uint32 addr, afs_uint16 port);


#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * state serialization
 */
extern int h_SaveState(void);
extern int h_RestoreState(void);
#endif

#define H_ENUMERATE_BAIL(held)        ((held)|0x80000000)
#define H_ENUMERATE_ISSET_BAIL(held)  ((held)&0x80000000)
#define H_ENUMERATE_ISSET_HELD(held)  ((held)&0x7FFFFFFF)

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
#endif /* _AFS_VICED_HOST_H */
