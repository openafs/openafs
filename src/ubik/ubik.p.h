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

#include <stdarg.h>

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

#include <ubik_int.h>

/*! \name ubik_trans types */
#define	UBIK_READTRANS	    0
#define	UBIK_WRITETRANS	    1
/*\}*/

/*! \name ubik_lock types */
#define	LOCKREAD	    1
#define	LOCKWRITE	    2
#if !defined(UBIK_PAUSE)
#define	LOCKWAIT	    3
#endif /* UBIK_PAUSE */
/*\}*/

/*! \name ubik client flags */
#define UPUBIKONLY 	    1	/*!< only check servers presumed functional */
#define UBIK_CALL_NEW 	    2	/*!< use the semantics of ubik_Call_New */
/*\}*/

/*! \name RX services types */
#define	VOTE_SERVICE_ID	    50
#define	DISK_SERVICE_ID	    51
#define	USER_SERVICE_ID	    52	/*!< Since most applications use same port! */
/*\}*/

#define	UBIK_MAGIC	0x354545

/*! \name global ubik parameters */
#define	MAXSERVERS	    20	/*!< max number of servers */
/*\}*/

/*! version comparison macro */
#define vcmp(a,b) ((a).epoch == (b).epoch? ((a).counter - (b).counter) : ((a).epoch - (b).epoch))

/*! \name ubik_client state bits */
#define	CFLastFailed	    1	/*!< last call failed to this guy (to detect down hosts) */
/*\}*/

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#else
#include <lwp.h>
#endif

/*!
 * \brief per-client structure for ubik
 */
struct ubik_client {
    short initializationState;	/*!< ubik client init state */
    short states[MAXSERVERS];	/*!< state bits */
    struct rx_connection *conns[MAXSERVERS];
    afs_int32 syncSite;
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_t cm;
#endif
};

#ifdef AFS_PTHREAD_ENV
#define LOCK_UBIK_CLIENT(client) MUTEX_ENTER(&client->cm)
#define UNLOCK_UBIK_CLIENT(client) MUTEX_EXIT(&client->cm)
#else
#define LOCK_UBIK_CLIENT(client)
#define UNLOCK_UBIK_CLIENT(client)
#endif

#define	ubik_GetRPCConn(astr,aindex)	((aindex) >= MAXSERVERS? 0 : (astr)->conns[aindex])
#define	ubik_GetRPCHost(astr,aindex)	((aindex) >= MAXSERVERS? 0 : (astr)->hosts[aindex])

/*!
 * \brief ubik header file structure
 */
struct ubik_hdr {
    afs_int32 magic;		/*!< magic number */
    short pad1;			/*!< some 0-initd padding */
    short size;			/*!< header allocation size */
    struct ubik_version version;	/*!< the version for this file */
};

/*!
 * \brief representation of a ubik transaction
 */
struct ubik_trans {
    struct ubik_dbase *dbase;	/*!< corresponding database */
    struct ubik_trans *next;	/*!< in the list */
    afs_int32 locktype;		/*!< transaction lock */
    struct ubik_trunc *activeTruncs;	/*!< queued truncates */
    struct ubik_tid tid;	/*!< transaction id of this trans (if write trans.) */
    afs_int32 minCommitTime;	/*!< time before which this trans can't commit */
    afs_int32 seekFile;		/*!< seek ptr: file number */
    afs_int32 seekPos;		/*!< seek ptr: offset therein */
    short flags;		/*!< trans flag bits */
    char type;			/*!< type of trans */
    iovec_wrt iovec_info;
    iovec_buf iovec_data;
};

/*!
 * \brief representation of a truncation operation
 */
struct ubik_trunc {
    struct ubik_trunc *next;
    afs_int32 file;		/*!< file to truncate */
    afs_int32 length;		/*!< new size */
};

struct ubik_stat {
    afs_int32 size;
    afs_int32 mtime;
};

#if defined(UKERNEL)
#include "afs/lock.h"
#else /* defined(UKERNEL) */
#include <lock.h>		/* just to make sure we've got this */
#endif /* defined(UKERNEL) */

/*!
 * \brief representation of a ubik database.
 *
 * Contains info on low-level disk access routines
 * for use by disk transaction module.
 */
struct ubik_dbase {
    char *pathName;		/*!< root name for dbase */
    struct ubik_trans *activeTrans;	/*!< active transaction list */
    struct ubik_version version;	/*!< version number */
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_t versionLock;	/*!< lock on version number */
#elif defined(UKERNEL)
    struct afs_lock versionLock;	/*!< lock on version number */
#else				/* defined(UKERNEL) */
    struct Lock versionLock;	/*!< lock on version number */
#endif				/* defined(UKERNEL) */
    afs_int32 tidCounter;	/*!< last RW or RO trans tid counter */
    afs_int32 writeTidCounter;	/*!< last write trans tid counter */
    afs_int32 flags;		/*!< flags */
    /* physio procedures */
    int (*read) (struct ubik_dbase * adbase, afs_int32 afile, void *abuffer,
		 afs_int32 apos, afs_int32 alength);
    int (*write) (struct ubik_dbase * adbase, afs_int32 afile, void *abuffer,
		  afs_int32 apos, afs_int32 alength);
    int (*truncate) (struct ubik_dbase * adbase, afs_int32 afile,
		     afs_int32 asize);
    int (*sync) (struct ubik_dbase * adbase, afs_int32 afile);
    int (*stat) (struct ubik_dbase * adbase, afs_int32 afid,
		 struct ubik_stat * astat);
    void (*open) (struct ubik_dbase * adbase, afs_int32 afid);
    int (*setlabel) (struct ubik_dbase * adbase, afs_int32 afile, struct ubik_version * aversion);	/*!< set the version label */
    int (*getlabel) (struct ubik_dbase * adbase, afs_int32 afile, struct ubik_version * aversion);	/*!< retrieve the version label */
    int (*getnfiles) (struct ubik_dbase * adbase);	/*!< find out number of files */
    short readers;		/*!< number of current read transactions */
    struct ubik_version cachedVersion;	/*!< version of caller's cached data */
#ifdef UKERNEL
    struct afs_lock cache_lock;
#else
    struct Lock cache_lock; /*!< protects cached application data */
#endif
#ifdef AFS_PTHREAD_ENV
    pthread_cond_t version_cond;    /*!< condition variable to manage changes to version */
    pthread_cond_t flags_cond;      /*!< condition variable to manage changes to flags */
#endif
};

/**
 * ubik_CheckCache callback function.
 *
 * @param[in] atrans  ubik transaction
 * @param[in] rock    rock passed to ubik_CheckCache
 *
 * @return operation status
 *   @retval 0        cache was read properly
 */
typedef int (*ubik_updatecache_func) (struct ubik_trans *atrans, void *rock);

/*! \name procedures for automatically authenticating ubik connections */
extern int (*ubik_CRXSecurityProc) (void *, struct rx_securityClass **,
				    afs_int32 *);
extern void *ubik_CRXSecurityRock;
extern int (*ubik_SRXSecurityProc) (void *, struct rx_securityClass **,
				    afs_int32 *);
extern void *ubik_SRXSecurityRock;
extern int (*ubik_CheckRXSecurityProc) (void *, struct rx_call *);
extern void *ubik_CheckRXSecurityRock;
/*\}*/

/*
 * For applications that make use of ubik_BeginTransReadAnyWrite, writing
 * processes must not update the application-level cache as they write,
 * or else readers can read the new cache before the data is committed to
 * the db. So, when a commit occurs, the cache must be updated right then.
 * If set, this function will be called during commits of write transactions,
 * to update the application-level cache after a write. This will be called
 * immediately after the local disk commit succeeds, and it will be called
 * with a lock held that prevents other threads from reading from the cache
 * or the db in general.
 *
 * Note that this function MUST be set in order to make use of
 * ubik_BeginTransReadAnyWrite.
 */
extern int (*ubik_SyncWriterCacheProc) (void);

/****************INTERNALS BELOW ****************/

#ifdef UBIK_INTERNALS
/*! \name some ubik parameters */
#define	UBIK_PAGESIZE	    1024	/*!< fits in current r packet */
#define	UBIK_LOGPAGESIZE    10	/*!< base 2 log thereof */
#define	NBUFFERS	    20	/*!< number of 1K buffers */
#define	HDRSIZE		    64	/*!< bytes of header per dbfile */
/*\}*/

/*! \name ubik_dbase flags */
#define	DBWRITING	    1	/*!< are any write trans. in progress */
#if defined(UBIK_PAUSE)
#define DBVOTING            2	/*!< the beacon task is polling */
#endif /* UBIK_PAUSE */
/*\}*/

/*!\name ubik trans flags */
#define	TRDONE		    1	/*!< commit or abort done */
#define	TRABORT		    2	/*!< if #TRDONE, tells if aborted */
#define TRREADANY	    4	/*!< read any data available in trans */
#if defined(UBIK_PAUSE)
#define TRSETLOCK           8	/*!< SetLock is using trans */
#define TRSTALE             16	/*!< udisk_end during getLock */
#endif /* UBIK_PAUSE */
#define TRCACHELOCKED       32  /*!< this trans has locked dbase->cache_lock
                                 *   (meaning, this trans has called
                                 *   ubik_CheckCache at some point */
#define TRREADWRITE         64  /*!< read even if there's a conflicting ubik-
                                 *   level write lock */
/*\}*/

/*! \name ubik_lock flags */
#define	LWANT		    1
/*\}*/

/*! \name ubik system database numbers */
#define	LOGFILE		    (-1)
/*\}*/

/*! \name define log opcodes */
#define	LOGNEW		    100	/*!< start transaction */
#define	LOGEND		    101	/*!< commit (good) end transaction */
#define	LOGABORT	    102	/*!< abort (fail) transaction */
#define	LOGDATA		    103	/*!< data */
#define	LOGTRUNCATE	    104	/*!< truncate operation */
/*\}*/

/*!
 * \name timer constants
 * time constant for replication algorithms: the R time period is 20 seconds.  Both
 * #SMALLTIME and #BIGTIME must be larger than #RPCTIMEOUT+max(#RPCTIMEOUT, #POLLTIME),
 * so that timeouts do not prevent us from getting through to our servers in time.
 *
 * We use multi-R to time out multiple down hosts concurrently.
 * The only other restrictions:  #BIGTIME > #SMALLTIME and
 * #BIGTIME-#SMALLTIME > #MAXSKEW (the clock skew).
 */
#define MAXSKEW	10
#define POLLTIME 15
#define RPCTIMEOUT 20
#define BIGTIME 75
#define SMALLTIME 60
/*\}*/

/*!
 * \brief the per-server state, used by the sync site to keep track of its charges
 */
struct ubik_server {
    struct ubik_server *next;	/*!< next ptr */
    afs_uint32 addr[UBIK_MAX_INTERFACE_ADDR];	/*!< network order, addr[0] is primary */
    afs_int32 lastVoteTime;	/*!< last time yes vote received */
    afs_int32 lastBeaconSent;	/*!< last time beacon attempted */
    struct ubik_version version;	/*!< version, only used during recovery */
    struct rx_connection *vote_rxcid;	/*!< cid to use to contact dude for votes */
    struct rx_connection *disk_rxcid;	/*!< cid to use to contact dude for disk reqs */
    char lastVote;		/*!< true if last vote was yes */
    char up;			/*!< is it up? */
    char beaconSinceDown;	/*!< did beacon get through since last crash? */
    char currentDB;		/*!< is dbase up-to-date */
    char magic;			/*!< the one whose vote counts twice */
    char isClone;		/*!< is only a clone, doesn't vote */
};

/*! \name hold and release functions on a database */
#ifdef AFS_PTHREAD_ENV
# define	DBHOLD(a)	MUTEX_ENTER(&((a)->versionLock))
# define	DBRELE(a)	MUTEX_EXIT(&((a)->versionLock))
#else /* !AFS_PTHREAD_ENV */
# define	DBHOLD(a)	ObtainWriteLock(&((a)->versionLock))
# define	DBRELE(a)	ReleaseWriteLock(&((a)->versionLock))
#endif /* !AFS_PTHREAD_ENV */
/*\}*/

/* globals */

/*!name list of all servers in the system */
extern struct ubik_server *ubik_servers;
extern char amIClone;
/*\}*/

/*! \name network port info */
extern short ubik_callPortal;
/*\}*/

/*! \name urecovery state bits for sync site */
#define	UBIK_RECSYNCSITE	1	/* am sync site */
#define	UBIK_RECFOUNDDB		2	/* found acceptable dbase from quorum */
#define	UBIK_RECHAVEDB		4	/* fetched best dbase */
#define	UBIK_RECLABELDB		8	/* relabelled dbase */
#define	UBIK_RECSENTDB		0x10	/* sent best db to *everyone* */
#define	UBIK_RECSBETTER		UBIK_RECLABELDB	/* last state */
/*\}*/

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
extern int uphys_close(int afd);
extern int uphys_stat(struct ubik_dbase *adbase, afs_int32 afid,
		      struct ubik_stat *astat);
extern int uphys_read(struct ubik_dbase *adbase, afs_int32 afile,
		      void *abuffer, afs_int32 apos,
		      afs_int32 alength);
extern int uphys_write(struct ubik_dbase *adbase, afs_int32 afile,
		       void *abuffer, afs_int32 apos,
		       afs_int32 alength);
extern int uphys_truncate(struct ubik_dbase *adbase, afs_int32 afile,
			  afs_int32 asize);
extern int uphys_getnfiles(struct ubik_dbase *adbase);
extern int uphys_getlabel(struct ubik_dbase *adbase, afs_int32 afile,
			  struct ubik_version *aversion);
extern int uphys_setlabel(struct ubik_dbase *adbase, afs_int32 afile,
			  struct ubik_version *aversion);
extern int uphys_sync(struct ubik_dbase *adbase, afs_int32 afile);
extern void uphys_invalidate(struct ubik_dbase *adbase,
			     afs_int32 afid);

/*! \name recovery.c */
extern int urecovery_ResetState(void);
extern int urecovery_LostServer(void);
extern int urecovery_AllBetter(struct ubik_dbase *adbase,
			       int areadAny);
extern int urecovery_AbortAll(struct ubik_dbase *adbase);
extern int urecovery_CheckTid(struct ubik_tid *atid);
extern int urecovery_Initialize(struct ubik_dbase *adbase);
extern void *urecovery_Interact(void *);
extern int DoProbe(struct ubik_server *server);
/*\}*/

/*! \name ubik.c */
extern afs_int32 ContactQuorum_NoArguments(afs_int32 (*proc)
						       (struct rx_connection *,
							ubik_tid *),
					   struct ubik_trans *atrans,
					   int aflags);

extern afs_int32 ContactQuorum_DISK_Lock(struct ubik_trans *atrans,
					 int aflags,
					 afs_int32 file, afs_int32 position,
					 afs_int32 length, afs_int32 type);

extern afs_int32 ContactQuorum_DISK_Write(struct ubik_trans *atrans,
					  int aflags,
					  afs_int32 file, afs_int32 position,
					  bulkdata *data);

extern afs_int32 ContactQuorum_DISK_Truncate(struct ubik_trans *atrans,
					     int aflags,
					     afs_int32 file, afs_int32 length);

extern afs_int32 ContactQuorum_DISK_WriteV(struct ubik_trans *atrans,
					   int aflags,
					   iovec_wrt * io_vector,
					   iovec_buf *io_buffer);

extern afs_int32 ContactQuorum_DISK_SetVersion(struct ubik_trans *atrans,
					       int aflags,
					       ubik_version *OldVersion,
					       ubik_version *NewVersion);

extern void panic(char *format, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);

extern afs_uint32 ubikGetPrimaryInterfaceAddr(afs_uint32 addr);
/*\}*/

/*! \name beacon.c */
struct afsconf_cell;
extern void ubeacon_Debug(struct ubik_debug *aparm);
extern int ubeacon_AmSyncSite(void);
extern int ubeacon_InitServerListByInfo(afs_uint32 ame,
					struct afsconf_cell *info,
					char clones[]);
extern int ubeacon_InitServerList(afs_uint32 ame, afs_uint32 aservers[]);
extern void *ubeacon_Interact(void *);
/*\}*/

/*! \name disk.c */
extern void udisk_Debug(struct ubik_debug *aparm);
extern int udisk_Invalidate(struct ubik_dbase *adbase, afs_int32 afid);
extern int udisk_read(struct ubik_trans *atrans, afs_int32 afile,
		      void *abuffer, afs_int32 apos, afs_int32 alen);
extern int udisk_truncate(struct ubik_trans *atrans, afs_int32 afile,
			  afs_int32 alength);
extern int udisk_write(struct ubik_trans *atrans, afs_int32 afile,
		       void *abuffer, afs_int32 apos, afs_int32 alen);
extern int udisk_begin(struct ubik_dbase *adbase, int atype,
		       struct ubik_trans **atrans);
extern int udisk_commit(struct ubik_trans *atrans);
extern int udisk_abort(struct ubik_trans *atrans);
extern int udisk_end(struct ubik_trans *atrans);
/*\}*/

/*! \name lock.c */
extern int  ulock_getLock(struct ubik_trans *atrans, int atype, int await);
extern void ulock_relLock(struct ubik_trans *atrans);
extern void ulock_Debug(struct ubik_debug *aparm);
/*\}*/

/*! \name vote.c */
extern int uvote_ShouldIRun(void);
extern afs_int32 uvote_GetSyncSite(void);
extern int uvote_Init(void);
extern void ubik_vprint(const char *format, va_list ap)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 0);

extern void ubik_print(const char *format, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);

extern void ubik_dprint(const char *format, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);

extern void ubik_dprint_25(const char *format, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);
/*\}*/

#endif /* UBIK_INTERNALS */

extern afs_int32 ubik_nBuffers;

/*!
 * \name Public function prototypes
 */

/*! \name ubik.c */
struct afsconf_cell;
extern int ubik_ServerInitByInfo(afs_uint32 myHost, short myPort,
				 struct afsconf_cell *info, char clones[],
				 const char *pathName,
				 struct ubik_dbase **dbase);
extern int ubik_ServerInit(afs_uint32 myHost, short myPort,
			   afs_uint32 serverList[],
			   const char *pathName, struct ubik_dbase **dbase);
extern int ubik_BeginTrans(struct ubik_dbase *dbase,
			   afs_int32 transMode, struct ubik_trans **transPtr);
extern int ubik_BeginTransReadAny(struct ubik_dbase *dbase,
				  afs_int32 transMode,
				  struct ubik_trans **transPtr);
extern int ubik_BeginTransReadAnyWrite(struct ubik_dbase *dbase,
                                       afs_int32 transMode,
                                       struct ubik_trans **transPtr);
extern int ubik_AbortTrans(struct ubik_trans *transPtr);

extern int ubik_EndTrans(struct ubik_trans *transPtr);
extern int ubik_Read(struct ubik_trans *transPtr, void *buffer,
		     afs_int32 length);
extern int ubik_Flush(struct ubik_trans *transPtr);
extern int ubik_Write(struct ubik_trans *transPtr, void *buffer,
		      afs_int32 length);
extern int ubik_Seek(struct ubik_trans *transPtr, afs_int32 fileid,
		     afs_int32 position);
extern int ubik_Tell(struct ubik_trans *transPtr, afs_int32 * fileid,
		     afs_int32 * position);
extern int ubik_Truncate(struct ubik_trans *transPtr,
			 afs_int32 length);
extern int ubik_SetLock(struct ubik_trans *atrans, afs_int32 apos,
			afs_int32 alen, int atype);
extern int ubik_WaitVersion(struct ubik_dbase *adatabase,
			    struct ubik_version *aversion);
extern int ubik_GetVersion(struct ubik_trans *atrans,
			   struct ubik_version *avers);
extern int ubik_CheckCache(struct ubik_trans *atrans,
                           ubik_updatecache_func check,
                           void *rock);
/*\}*/

/*! \name ubikclient.c */

extern int ubik_ParseClientList(int argc, char **argv, afs_uint32 * aothers);
extern unsigned int afs_random(void);
extern int ubik_ClientInit(struct rx_connection **serverconns,
			   struct ubik_client **aclient);
extern afs_int32 ubik_ClientDestroy(struct ubik_client *aclient);
extern struct rx_connection *ubik_RefreshConn(struct rx_connection *tc);
#ifdef UBIK_LEGACY_CALLITER
extern afs_int32 ubik_CallIter(int (*aproc) (), struct ubik_client *aclient,
			       afs_int32 aflags, int *apos, long p1, long p2,
			       long p3, long p4, long p5, long p6, long p7,
			       long p8, long p9, long p10, long p11, long p12,
			       long p13, long p14, long p15, long p16);
extern afs_int32 ubik_Call_New(int (*aproc) (), struct ubik_client
			       *aclient, afs_int32 aflags, long p1, long p2,
			       long p3, long p4, long p5, long p6, long p7,
			       long p8, long p9, long p10, long p11, long p12,
			       long p13, long p14, long p15, long p16);
#endif
/*\}*/

/* \name ubikcmd.c */
extern int ubik_ParseServerList(int argc, char **argv, afs_uint32 *ahost,
				afs_uint32 *aothers);
/*\}*/

/* \name uinit.c */

struct rx_securityClass;
extern afs_int32 ugen_ClientInit(int noAuthFlag, const char *confDir,
				 char *cellName, afs_int32 sauth,
				 struct ubik_client **uclientp,
				 int (*secproc) (struct rx_securityClass *sc,
					 	 afs_int32 scIndex),
				 char *funcName,
				 afs_int32 gen_rxkad_level,
				 afs_int32 maxservers, char *serviceid,
				 afs_int32 deadtime, afs_uint32 server,
				 afs_uint32 port, afs_int32 usrvid);

#endif /* UBIK_H */
