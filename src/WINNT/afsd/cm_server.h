/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef __CM_SERVER_H_ENV__
#define __CM_SERVER_H_ENV__ 1

#include <winsock2.h>
#include <osi.h>

/* pointed to by volumes and cells without holds; cm_serverLock is obtained
 * at the appropriate times to change the pointers to these servers.
 */
typedef struct cm_server {
	struct cm_server *allNextp;		/* locked by cm_serverLock */
        struct sockaddr_in addr;		/* by mx */
        int type;				/* by mx */
	struct cm_conn *connsp;			/* locked by cm_connLock */
        long flags;				/* by mx */
        struct cm_cell *cellp;			/* cell containing this server */
	int refCount;				/* locked by cm_serverLock */
        osi_mutex_t mx;
	unsigned short ipRank;			/* server priority */
} cm_server_t;

enum repstate {not_busy, busy, offline};

typedef struct cm_serverRef {
	struct cm_serverRef *next;
	struct cm_server *server;
	enum repstate status;
} cm_serverRef_t;

/* types */
#define CM_SERVER_VLDB		1	/* a VLDB server */
#define CM_SERVER_FILE		2	/* a file server */

/* flags */
#define CM_SERVERFLAG_DOWN	1	/* server is down */

/* flags for procedures */
#define CM_FLAG_CHECKUPSERVERS		1	/* check working servers */
#define CM_FLAG_CHECKDOWNSERVERS	2	/* check down servers */

/* values for ipRank */
#define CM_IPRANK_TOP	5000	/* on same machine */
#define CM_IPRANK_HI	20000	/* on same subnet  */
#define CM_IPRANK_MED	30000	/* on same network */
#define CM_IPRANK_LOW	40000	/* on different networks */

/* the maximum number of network interfaces that this client has */ 

#define CM_MAXINTERFACE_ADDR          16
extern int cm_noIPAddr;		/* number of client network interfaces */
extern int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address */
extern int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask*/ 
extern int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
extern int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */

extern cm_server_t *cm_NewServer(struct sockaddr_in *addrp, int type,
	struct cm_cell *cellp);

extern cm_serverRef_t *cm_NewServerRef(struct cm_server *serverp);

extern long cm_ChecksumServerList(cm_serverRef_t *serversp);

extern void cm_PutServer(cm_server_t *);

extern cm_server_t *cm_FindServer(struct sockaddr_in *addrp, int type);

extern osi_rwlock_t cm_serverLock;

extern void cm_InitServer(void);

extern void cm_CheckServers(long flags, struct cm_cell *cellp);

extern cm_server_t *cm_allServersp;

extern void cm_SetServerPrefs(cm_server_t * serverp);

extern void cm_InsertServerList(cm_serverRef_t** list,cm_serverRef_t* element);

extern long cm_ChangeRankServer(cm_serverRef_t** list, cm_server_t* server); 

extern void cm_RandomizeServer(cm_serverRef_t** list); 

#endif /*  __CM_SERVER_H_ENV__ */
