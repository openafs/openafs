/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef UBIK_H
#define UBIK_H

/* these are now appended by the error table compiler */
#if 0
/* ubik error codes */
#define	UMINCODE	100000	/* lowest ubik error code */
#define	UNOQUORUM	100000	/* no quorum elected */
#define	UNOTSYNC	100001	/* not synchronization site (should work on sync site) */
#define	UNHOSTS		100002	/* too many hosts */
#define	UIOERROR	100003	/* I/O error writing dbase or log */
#define	UINTERNAL	100004	/* mysterious internal error */
#define	USYNC		100005	/* major synchronization error */
#define	UNOENT		100006	/* file not found when processing dbase */
#define	UBADLOCK	100007	/* bad lock range size (must be 1) */
#define	UBADLOG		100008	/* read error reprocessing log */
#define	UBADHOST	100009	/* problems with host name */
#define	UBADTYPE	100010	/* bad operation for this transaction type */
#define	UTWOENDS	100011	/* two commits or aborts done to transaction */
#define	UDONE		100012	/* operation done after abort (or commmit) */
#define	UNOSERVERS	100013	/* no servers appear to be up */
#define	UEOF		100014	/* premature EOF */
#define	ULOGIO		100015	/* error writing log file */
#define	UMAXCODE	100100	/* largest ubik error code */

#endif

#if defined(UKERNEL)
#include "ubik_int.h"
#else /* defined(UKERNEL) */
#include <ubik_int.h>
#endif /* defined(UKERNEL) */

/* ubik_trans types */
#define	UBIK_READTRANS	    0
#define	UBIK_WRITETRANS	    1

/* ubik_lock types */
#define	LOCKREAD	    1
#define	LOCKWRITE	    2
#if !defined(UBIK_PAUSE)
#define	LOCKWAIT	    3
#endif /* UBIK_PAUSE */

/* ubik client flags */
#define UPUBIKONLY 	    1	/* only check servers presumed functional */

/* RX services types */
#define	VOTE_SERVICE_ID	    50
#define	DISK_SERVICE_ID	    51
#define	USER_SERVICE_ID	    52	/* Since most applications use same port! */

#define	UBIK_MAGIC	0x354545

/* global ubik parameters */
#define	MAXSERVERS	    20	/* max number of servers */

/* version comparison macro */
#define vcmp(a,b) ((a).epoch == (b).epoch? ((a).counter - (b).counter) : ((a).epoch - (b).epoch))

/* ubik_client state bits */
#define	CFLastFailed	    1	/* last call failed to this guy (to detect down hosts) */

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#include <assert.h>
#endif

/* per-client structure for ubik */
struct ubik_client {
    short initializationState;	/* ubik client init state */
    short states[MAXSERVERS];	/* state bits */
    struct rx_connection *conns[MAXSERVERS];
    afs_int32 syncSite;
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_t cm;
#endif
};

#ifdef AFS_PTHREAD_ENV
#define LOCK_UBIK_CLIENT(client) assert(pthread_mutex_lock(&client->cm)==0)
#define UNLOCK_UBIK_CLIENT(client) assert(pthread_mutex_unlock(&client->cm)==0)
#else
#define LOCK_UBIK_CLIENT(client)
#define UNLOCK_UBIK_CLIENT(client)
#endif

#define	ubik_GetRPCConn(astr,aindex)	((aindex) >= MAXSERVERS? 0 : (astr)->conns[aindex])
#define	ubik_GetRPCHost(astr,aindex)	((aindex) >= MAXSERVERS? 0 : (astr)->hosts[aindex])

/* ubik header file structure */
struct ubik_hdr {
    afs_int32 magic;		/* magic number */
    short pad1;			/* some 0-initd padding */
    short size;			/* header allocation size */
    struct ubik_version version;	/* the version for this file */
};

/* representation of a ubik transaction */
struct ubik_trans {
    struct ubik_dbase *dbase;	/* corresponding database */
    struct ubik_trans *next;	/* in the list */
    afs_int32 locktype;		/* transaction lock */
    struct ubik_trunc *activeTruncs;	/* queued truncates */
    struct ubik_tid tid;	/* transaction id of this trans (if write trans.) */
    afs_int32 minCommitTime;	/* time before which this trans can't commit */
    afs_int32 seekFile;		/* seek ptr: file number */
    afs_int32 seekPos;		/* seek ptr: offset therein */
    short flags;		/* trans flag bits */
    char type;			/* type of trans */
    iovec_wrt iovec_info;
    iovec_buf iovec_data;
};

/* representation of a truncation operation */
struct ubik_trunc {
    struct ubik_trunc *next;
    afs_int32 file;		/* file to truncate */
    afs_int32 length;		/* new size */
};

struct ubik_stat {
    afs_int32 size;
    afs_int32 mtime;
};

#if defined(UKERNEL)
#include "afs/lock.h"
#else /* defined(UKERNEL) */
#include <lock.h>		/* just to make sure we've go this */
#endif /* defined(UKERNEL) */

/* representation of a ubik database.  Contains info on low-level disk access routines
    for use by disk transaction module.
*/
struct ubik_dbase {
    char *pathName;		/* root name for dbase */
    struct ubik_trans *activeTrans;	/* active transaction list */
    struct ubik_version version;	/* version number */
#if defined(UKERNEL)
    struct afs_lock versionLock;	/* lock on version number */
#else				/* defined(UKERNEL) */
    struct Lock versionLock;	/* lock on version number */
#endif				/* defined(UKERNEL) */
    afs_int32 tidCounter;	/* last RW or RO trans tid counter */
    afs_int32 writeTidCounter;	/* last write trans tid counter */
    afs_int32 flags;		/* flags */
    /* physio procedures */
    int (*read) (struct ubik_dbase * adbase, afs_int32 afile, char *abuffer,
		 afs_int32 apos, afs_int32 alength);
    int (*write) (struct ubik_dbase * adbase, afs_int32 afile, char *abuffer,
		  afs_int32 apos, afs_int32 alength);
    int (*truncate) (struct ubik_dbase * adbase, afs_int32 afile,
		     afs_int32 asize);
    int (*sync) (struct ubik_dbase * adbase, afs_int32 afile);
    int (*stat) (struct ubik_dbase * adbase, afs_int32 afid,
		 struct ubik_stat * astat);
    int (*open) (struct ubik_dbase * adbase, afs_int32 afid);
    int (*setlabel) (struct ubik_dbase * adbase, afs_int32 afile, struct ubik_version * aversion);	/* set the version label */
    int (*getlabel) (struct ubik_dbase * adbase, afs_int32 afile, struct ubik_version * aversion);	/* retrieve the version label */
    int (*getnfiles) (struct ubik_dbase * adbase);	/* find out number of files */
    short readers;		/* number of current read transactions */
    struct ubik_version cachedVersion;	/* version of caller's cached data */
};

/* procedures for automatically authenticating ubik connections */
extern int (*ubik_CRXSecurityProc) ();
extern char *ubik_CRXSecurityRock;
extern int (*ubik_SRXSecurityProc) ();
extern char *ubik_SRXSecurityRock;
extern int (*ubik_CheckRXSecurityProc) ();
extern char *ubik_CheckRXSecurityRock;

/****************INTERNALS BELOW ****************/

#ifdef UBIK_INTERNALS
/* some ubik parameters */
#define	UBIK_PAGESIZE	    1024	/* fits in current r packet */
#define	UBIK_LOGPAGESIZE    10	/* base 2 log thereof */
#define	NBUFFERS	    20	/* number of 1K buffers */
#define	HDRSIZE		    64	/* bytes of header per dbfile */

/* ubik_dbase flags */
#define	DBWRITING	    1	/* are any write trans. in progress */
#if defined(UBIK_PAUSE)
#define DBVOTING            2	/* the beacon task is polling */
#endif /* UBIK_PAUSE */

/* ubik trans flags */
#define	TRDONE		    1	/* commit or abort done */
#define	TRABORT		    2	/* if TRDONE, tells if aborted */
#define TRREADANY	    4	/* read any data available in trans */
#if defined(UBIK_PAUSE)
#define TRSETLOCK           8	/* SetLock is using trans */
#define TRSTALE             16	/* udisk_end during getLock */
#endif /* UBIK_PAUSE */

/* ubik_lock flags */
#define	LWANT		    1

/* ubik system database numbers */
#define	LOGFILE		    (-1)

/* define log opcodes */
#define	LOGNEW		    100	/* start transaction */
#define	LOGEND		    101	/* commit (good) end transaction */
#define	LOGABORT	    102	/* abort (fail) transaction */
#define	LOGDATA		    103	/* data */
#define	LOGTRUNCATE	    104	/* truncate operation */

/* time constant for replication algorithms: the R time period is 20 seconds.  Both SMALLTIME
    and BIGTIME must be larger than RPCTIMEOUT+max(RPCTIMEOUT,POLLTIME),
    so that timeouts do not prevent us from getting through to our servers in time.

    We use multi-R to time out multiple down hosts concurrently.
    The only other restrictions:  BIGTIME > SMALLTIME and
    BIGTIME-SMALLTIME > MAXSKEW (the clock skew).
*/
#define MAXSKEW	10
#define POLLTIME 15
#define RPCTIMEOUT 20
#define BIGTIME 75
#define SMALLTIME 60

/* the per-server state, used by the sync site to keep track of its charges */
struct ubik_server {
    struct ubik_server *next;	/* next ptr */
    afs_uint32 addr[UBIK_MAX_INTERFACE_ADDR];	/* network order, addr[0] is primary */
    afs_int32 lastVoteTime;	/* last time yes vote received */
    afs_int32 lastBeaconSent;	/* last time beacon attempted */
    struct ubik_version version;	/* version, only used during recovery */
    struct rx_connection *vote_rxcid;	/* cid to use to contact dude for votes */
    struct rx_connection *disk_rxcid;	/* cid to use to contact dude for disk reqs */
    char lastVote;		/* true if last vote was yes */
    char up;			/* is it up? */
    char beaconSinceDown;	/* did beacon get through since last crash? */
    char currentDB;		/* is dbase up-to-date */
    char magic;			/* the one whose vote counts twice */
    char isClone;		/* is only a clone, doesn't vote */
};

/* hold and release functions on a database */
#define	DBHOLD(a)	ObtainWriteLock(&((a)->versionLock))
#define	DBRELE(a)	ReleaseWriteLock(&((a)->versionLock))

/* globals */

/* list of all servers in the system */
extern struct ubik_server *ubik_servers;
extern char amIClone;

/* network port info */
extern short ubik_callPortal;

/* urecovery state bits for sync site */
#define	UBIK_RECSYNCSITE	1	/* am sync site */
#define	UBIK_RECFOUNDDB		2	/* found acceptable dbase from quorum */
#define	UBIK_RECHAVEDB		4	/* fetched best dbase */
#define	UBIK_RECLABELDB		8	/* relabelled dbase */
#define	UBIK_RECSENTDB		0x10	/* sent best db to *everyone* */
#define	UBIK_RECSBETTER		UBIK_RECLABELDB	/* last state */

extern afs_int32 ubik_quorum;	/* min hosts in quorum */
extern struct ubik_dbase *ubik_dbase;	/* the database handled by this server */
extern afs_uint32 ubik_host[UBIK_MAX_INTERFACE_ADDR];	/* this host addr, in net order */
extern int ubik_amSyncSite;	/* sleep on this waiting to be sync site */
extern struct ubik_stats {	/* random stats */
    afs_int32 escapes;
} ubik_stats;
extern afs_int32 ubik_epochTime;	/* time when this site started */
extern afs_int32 urecovery_state;	/* sync site recovery process state */
extern struct ubik_trans *ubik_currentTrans;	/* current trans */
extern struct ubik_version ubik_dbVersion;	/* sync site's dbase version */
extern afs_int32 ubik_debugFlag;	/* ubik debug flag */
extern int ubikPrimaryAddrOnly;	/* use only primary address */

/* this extern gives the sync site's db version, with epoch of 0 if none yet */

/* phys.c */
extern int uphys_close(register int afd);
extern int uphys_stat(struct ubik_dbase *adbase, afs_int32 afid,
		      struct ubik_stat *astat);
extern int uphys_read(register struct ubik_dbase *adbase, afs_int32 afile,
		      register char *abuffer, afs_int32 apos,
		      afs_int32 alength);
extern int uphys_write(register struct ubik_dbase *adbase, afs_int32 afile,
		       register char *abuffer, afs_int32 apos,
		       afs_int32 alength);
extern int uphys_truncate(register struct ubik_dbase *adbase, afs_int32 afile,
			  afs_int32 asize);
extern int uphys_getnfiles(register struct ubik_dbase *adbase);
extern int uphys_getlabel(register struct ubik_dbase *adbase, afs_int32 afile,
			  struct ubik_version *aversion);
extern int uphys_setlabel(register struct ubik_dbase *adbase, afs_int32 afile,
			  struct ubik_version *aversion);
extern int uphys_sync(register struct ubik_dbase *adbase, afs_int32 afile);


/* recovery.c */
extern int urecovery_ResetState(void);
extern int urecovery_LostServer(void);
extern int urecovery_AllBetter(register struct ubik_dbase *adbase,
			       int areadAny);
extern int urecovery_AbortAll(struct ubik_dbase *adbase);
extern int urecovery_CheckTid(register struct ubik_tid *atid);
extern int urecovery_Initialize(register struct ubik_dbase *adbase);
extern int urecovery_Interact(void);
extern int DoProbe(struct ubik_server *server);


extern int ubeacon_Interact();
extern int sdisk_Interact();
extern int uvote_Interact();
extern int DISK_Abort();
extern int DISK_Begin();
extern int DISK_ReleaseLocks();
extern int DISK_Commit();
extern int DISK_Lock();
extern int DISK_Write();
extern int DISK_WriteV();
extern int DISK_Truncate();
extern int DISK_SetVersion();

/* disk.c */
extern int udisk_abort(struct ubik_trans *atrans);

/* lock.c */
extern void ulock_relLock(struct ubik_trans *atrans);

#endif /* UBIK_INTERNALS */

extern afs_int32 ubik_nBuffers;

/*
 * Public function prototypes
 */

extern int ubik_ParseClientList(int argc, char **argv, afs_int32 * aothers);

extern unsigned int afs_random(void
    );

extern int ubik_ClientInit(register struct rx_connection **serverconns,
			   struct ubik_client **aclient);

extern afs_int32 ubik_ClientDestroy(struct ubik_client *aclient);

extern afs_int32 ubik_CallIter(int (*aproc) (), struct ubik_client *aclient,
			       afs_int32 aflags, int *apos, long p1, long p2,
			       long p3, long p4, long p5, long p6, long p7,
			       long p8, long p9, long p10, long p11, long p12,
			       long p13, long p14, long p15, long p16);

extern struct rx_connection *ubik_RefreshConn(struct rx_connection *tc);

/* ubik.c */
extern int ubik_BeginTrans(register struct ubik_dbase *dbase,
			   afs_int32 transMode, struct ubik_trans **transPtr);
extern int ubik_EndTrans(register struct ubik_trans *transPtr);

/* uinit.c */

extern afs_int32 ugen_ClientInit(int noAuthFlag, char *confDir, char *cellName,
				 afs_int32 sauth, 
				 struct ubik_client **uclientp,
				 int (*secproc) (), char *funcName, 
				 afs_int32 gen_rxkad_level, 
				 afs_int32 maxservers, char *serviceid, 
				 afs_int32 deadtime, afs_uint32 server, 
				 afs_uint32 port, afs_int32 usrvid);

#endif /* UBIK_H */
