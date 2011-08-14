
/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_SERVER_H
#define OPENAFS_WINNT_AFSD_CM_SERVER_H 1

#include <winsock2.h>
#include <osi.h>

/* this value is set to 1022 in order to  */
#define NUM_SERVER_VOLS         (32 - sizeof(void *) / 4)
typedef struct cm_server_vols {
    afs_uint32             ids[NUM_SERVER_VOLS];
    struct cm_server_vols *nextp;
} cm_server_vols_t;

/* pointed to by volumes and cells without holds; cm_serverLock is obtained
 * at the appropriate times to change the pointers to these servers.
 */
typedef struct cm_server {
    struct cm_server *allNextp;		/* locked by cm_serverLock */
    struct sockaddr_in addr;		/* by mx */
    int type;				/* by mx */
    struct cm_conn *connsp;		/* locked by cm_connLock */
    afs_int32 flags;			/* by mx */
    afs_int32 waitCount;		/* by mx */
    afs_int32 capabilities;		/* by mx */
    struct cm_cell *cellp;		/* cell containing this server */
    afs_int32 refCount;		        /* Interlocked with cm_serverLock */
    osi_mutex_t mx;
    unsigned short ipRank;		/* server priority */
    cm_server_vols_t *  vols;           /* by mx */
    time_t downTime;                    /* by mx */
    afsUUID uuid;                       /* by mx */
    unsigned short adminRank;		/* only set if admin sets a rank */
} cm_server_t;

enum repstate {srv_not_busy, srv_busy, srv_offline, srv_deleted};

typedef struct cm_serverRef {
    struct cm_serverRef *next;      /* locked by cm_serverLock */
    struct cm_server *server;       /* locked by cm_serverLock */
    enum repstate status;           /* locked by cm_serverLock */
    afs_int32 refCount;             /* locked by cm_serverLock */
    afs_uint32 volID;               /* locked by cm_serverLock */
} cm_serverRef_t;

/* types */
#define CM_SERVER_VLDB		1	/* a VLDB server */
#define CM_SERVER_FILE		2	/* a file server */

/* flags */
#define CM_SERVERFLAG_DOWN	0x1	/* server is down */
#define CM_SERVERFLAG_PREF_SET	0x2     /* server preference set by user */
#define CM_SERVERFLAG_PINGING 	0x4 	/* a ping against this server in progress */
#define CM_SERVERFLAG_NO64BIT   0x8     /* server has no support for
                                           64-bit operations. */
#define CM_SERVERFLAG_NOINLINEBULK 0x10	/* server has no support for inline bulk */
#define CM_SERVERFLAG_UUID      0x20    /* server uuid is known */

/* flags for procedures */
#define CM_FLAG_CHECKUPSERVERS		1	/* check working servers */
#define CM_FLAG_CHECKDOWNSERVERS	2	/* check down servers */
#define CM_FLAG_CHECKVLDBSERVERS        4       /* check only vldb servers */
#define CM_FLAG_CHECKFILESERVERS        8       /* check only file servers */

/* values for ipRank */
#define CM_IPRANK_TOP	5000	/* on same machine */
#define CM_IPRANK_HI	20000	/* on same subnet  */
#define CM_IPRANK_MED	30000	/* on same network */
#define CM_IPRANK_LOW	40000	/* on different networks */

/* the maximum number of network interfaces that this client has */

#define CM_MAXINTERFACE_ADDR          16

extern cm_server_t *cm_NewServer(struct sockaddr_in *addrp, int type,
	struct cm_cell *cellp, afsUUID *uuidp, afs_uint32 flags);

extern cm_serverRef_t *cm_NewServerRef(struct cm_server *serverp, afs_uint32 volID);

extern void cm_GetServerRef(cm_serverRef_t *tsrp, int locked);

extern afs_int32 cm_PutServerRef(cm_serverRef_t *tsrp, int locked);

extern LONG_PTR cm_ChecksumServerList(cm_serverRef_t *serversp);

extern void cm_GetServer(cm_server_t *);

extern void cm_GetServerNoLock(cm_server_t *);

extern void cm_PutServer(cm_server_t *);

extern void cm_PutServerNoLock(cm_server_t *);

extern cm_server_t *cm_FindServer(struct sockaddr_in *addrp, int type, int locked);

extern osi_rwlock_t cm_serverLock;

extern osi_rwlock_t cm_syscfgLock;

extern void cm_InitServer(void);

extern void cm_CheckServers(afs_uint32 flags, struct cm_cell *cellp);

extern cm_server_t *cm_allServersp;

extern afs_int32 cm_RankServer(cm_server_t * server);

extern void cm_RankUpServers();

extern void cm_SetServerPrefs(cm_server_t * serverp);

extern void cm_InsertServerList(cm_serverRef_t** list,cm_serverRef_t* element);

extern long cm_ChangeRankServer(cm_serverRef_t** list, cm_server_t* server);

extern void cm_RandomizeServer(cm_serverRef_t** list);

extern void cm_FreeServer(cm_server_t* server);

#define CM_FREESERVERLIST_DELETE 1

extern void cm_FreeServerList(cm_serverRef_t** list, afs_uint32 flags);

extern void cm_ForceNewConnectionsAllServers(void);

extern void cm_SetServerNo64Bit(cm_server_t * serverp, int no64bit);

extern void cm_SetServerNoInlineBulk(cm_server_t * serverp, int no);

extern cm_server_t * cm_FindServerByIP(afs_uint32 addr, unsigned short port, int type, int locked);

extern cm_server_t * cm_FindServerByUuid(afsUUID* uuid, int type, int locked);

extern void cm_SetLanAdapterChangeDetected(void);

extern void cm_RemoveVolumeFromServer(cm_server_t * serverp, afs_uint32 volID);

extern int cm_DumpServers(FILE *outputFile, char *cookie, int lock);

extern int cm_ServerEqual(cm_server_t *srv1, cm_server_t *srv2);

/* Protected by cm_syscfgLock (rw) */
extern int cm_noIPAddr;         /* number of client network interfaces */
extern int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address in host order */
extern int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask in host order*/
extern int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
extern int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */
extern int cm_LanAdapterChangeDetected;

/* Protected by cm_serverLock */
extern cm_server_t *cm_allServersp;
extern afs_uint32   cm_numFileServers;
extern afs_uint32   cm_numVldbServers;
#endif /*  OPENAFS_WINNT_AFSD_CM_SERVER_H */
