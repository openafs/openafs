/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * demand attach fs
 * salvage server interface
 */
#ifndef _AFS_VOL_SALVSYNC_H
#define _AFS_VOL_SALVSYNC_H

#ifdef AFS_DEMAND_ATTACH_FS
#include "daemon_com.h"


#define SALVSYNC_PROTO_VERSION        1


/* SALVSYNC command codes */
#define SALVSYNC_NOP            SYNC_COM_CODE_DECL(0)   /* just return stats */
#define SALVSYNC_SALVAGE	SYNC_COM_CODE_DECL(1)	/* schedule a salvage */
#define SALVSYNC_CANCEL         SYNC_COM_CODE_DECL(2)   /* Cancel a salvage */
#define SALVSYNC_RAISEPRIO      SYNC_COM_CODE_DECL(3)   /* move a salvage operation to
							 * the head of the work queue */
#define SALVSYNC_QUERY          SYNC_COM_CODE_DECL(4)   /* query the status of a salvage */
#define SALVSYNC_CANCELALL      SYNC_COM_CODE_DECL(5)   /* cancel all pending salvages */

/* SALVSYNC reason codes */
#define SALVSYNC_WHATEVER	SYNC_REASON_CODE_DECL(0)  /* XXXX */
#define SALVSYNC_ERROR		SYNC_REASON_CODE_DECL(1)  /* volume is in error state */
#define SALVSYNC_OPERATOR	SYNC_REASON_CODE_DECL(2)  /* operator forced salvage */
#define SALVSYNC_SHUTDOWN       SYNC_REASON_CODE_DECL(3)  /* cancel due to shutdown */
#define SALVSYNC_NEEDED         SYNC_REASON_CODE_DECL(4)  /* needsSalvaged flag set */

/* SALVSYNC response codes */

/* SALVSYNC flags */
#define SALVSYNC_FLAG_VOL_STATS_VALID SYNC_FLAG_CODE_DECL(0) /* volume stats in response are valid */

/* SALVSYNC command state fields */
#define SALVSYNC_STATE_UNKNOWN        0         /* unknown state */
#define SALVSYNC_STATE_QUEUED         1         /* salvage request on queue */
#define SALVSYNC_STATE_SALVAGING      2         /* salvage is happening now */
#define SALVSYNC_STATE_ERROR          3         /* salvage ended in an error */
#define SALVSYNC_STATE_DONE           4         /* last salvage ended successfully */


typedef struct SALVSYNC_command_hdr {
    afs_uint32 prio;
    afs_uint32 volume;
    char partName[16];		/* partition name, e.g. /vicepa */
} SALVSYNC_command_hdr;

typedef struct SALVSYNC_response_hdr {
    afs_int32 state;
    afs_int32 prio;
    afs_int32 sq_len;
    afs_int32 pq_len;
} SALVSYNC_response_hdr;

typedef struct SALVSYNC_command {
    SYNC_command_hdr * hdr;
    SALVSYNC_command_hdr * sop;
    SYNC_command * com;
} SALVSYNC_command;

typedef struct SALVSYNC_response {
    SYNC_response_hdr * hdr;
    SALVSYNC_response_hdr * sop;
    SYNC_response * res;
} SALVSYNC_response;

typedef struct SALVSYNC_command_info {
    SYNC_command_hdr com;
    SALVSYNC_command_hdr sop;
} SALVSYNC_command_info;

struct SalvageQueueNode {
    struct rx_queue q;
    struct rx_queue hash_chain;
    afs_uint32 state;
    struct SALVSYNC_command_info command;
    afs_int32 partition_id;
    int pid;
};


/* Prototypes from salvsync.c */

/* online salvager client interfaces */
extern int SALVSYNC_clientFinis(void);
extern int SALVSYNC_clientInit(void);
extern int SALVSYNC_clientReconnect(void);
extern afs_int32 SALVSYNC_askSalv(SYNC_command * com, SYNC_response * res);
extern afs_int32 SALVSYNC_SalvageVolume(VolumeId volume, char *partName, int com, int reason,
					afs_uint32 prio, SYNC_response * res);

/* salvage server interfaces */
extern void SALVSYNC_salvInit(void);
extern struct SalvageQueueNode * SALVSYNC_getWork(void);
extern void SALVSYNC_doneWork(struct SalvageQueueNode *, int result);
extern void SALVSYNC_doneWorkByPid(int pid, int result);

#endif /* AFS_DEMAND_ATTACH_FS */

#endif /* _AFS_VOL_SALVSYNC_H */
