/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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

#define SALSRV_EXIT_VOLGROUP_LINK 10


#ifdef AFS_DEMAND_ATTACH_FS
#include "daemon_com.h"
#include "voldefs.h"


#define SALVSYNC_PROTO_VERSION_V1     1
#define SALVSYNC_PROTO_VERSION_V2     2
#define SALVSYNC_PROTO_VERSION        SALVSYNC_PROTO_VERSION_V2


/** 
 * SALVSYNC protocol command codes.
 */
typedef enum {
    SALVSYNC_OP_NOP           = SYNC_COM_CODE_DECL(0),     /**< just return stats */
    SALVSYNC_OP_SALVAGE       = SYNC_COM_CODE_DECL(1),     /**< schedule a salvage */
    SALVSYNC_OP_CANCEL        = SYNC_COM_CODE_DECL(2),     /**< cancel a salvage */
    SALVSYNC_OP_RAISEPRIO     = SYNC_COM_CODE_DECL(3),     /**< raise salvage priority */
    SALVSYNC_OP_QUERY         = SYNC_COM_CODE_DECL(4),     /**< query status of a salvage */
    SALVSYNC_OP_CANCELALL     = SYNC_COM_CODE_DECL(5),     /**< cancel all pending salvages */
    SALVSYNC_OP_LINK          = SYNC_COM_CODE_DECL(6),     /**< link a clone to its parent */
    SALVSYNC_OP_MAX_ID /* must be at end of enum */
} SALVSYNC_op_code_t;

#define SALVSYNC_NOP         SALVSYNC_OP_NOP
#define SALVSYNC_SALVAGE     SALVSYNC_OP_SALVAGE
#define SALVSYNC_CANCEL      SALVSYNC_OP_CANCEL
#define SALVSYNC_RAISEPRIO   SALVSYNC_OP_RAISEPRIO
#define SALVSYNC_QUERY       SALVSYNC_OP_QUERY
#define SALVSYNC_CANCELALL   SALVSYNC_OP_CANCELALL
#define SALVSYNC_LINK        SALVSYNC_OP_LINK

/**
 * SALVSYNC protocol reason codes.
 */
typedef enum {
    SALVSYNC_REASON_WHATEVER   = SYNC_REASON_CODE_DECL(0), /**< XXX */
    SALVSYNC_REASON_ERROR      = SYNC_REASON_CODE_DECL(1), /**< volume is in error state */
    SALVSYNC_REASON_OPERATOR   = SYNC_REASON_CODE_DECL(2), /**< operator forced salvage */
    SALVSYNC_REASON_SHUTDOWN   = SYNC_REASON_CODE_DECL(3), /**< cancel due to shutdown */
    SALVSYNC_REASON_NEEDED     = SYNC_REASON_CODE_DECL(4), /**< needsSalvaged flag set */
    SALVSYNC_REASON_MAX_ID /* must be at end of enum */
} SALVSYNC_reason_code_t;

#define SALVSYNC_WHATEVER    SALVSYNC_REASON_WHATEVER
#define SALVSYNC_ERROR       SALVSYNC_REASON_ERROR
#define SALVSYNC_OPERATOR    SALVSYNC_REASON_OPERATOR
#define SALVSYNC_SHUTDOWN    SALVSYNC_REASON_SHUTDOWN
#define SALVSYNC_NEEDED      SALVSYNC_REASON_NEEDED

/* SALVSYNC response codes */

/* SALVSYNC flags */
#define SALVSYNC_FLAG_VOL_STATS_VALID SYNC_FLAG_CODE_DECL(0) /* volume stats in response are valid */

/** 
 * SALVSYNC command state.
 */
typedef enum {
    SALVSYNC_STATE_UNKNOWN = 0,       /**< unknown state */
    SALVSYNC_STATE_QUEUED  = 1,       /**< salvage request is queued */
    SALVSYNC_STATE_SALVAGING = 2,     /**< salvage is happening now */
    SALVSYNC_STATE_ERROR = 3,         /**< salvage ended in an error */
    SALVSYNC_STATE_DONE = 4           /**< last salvage ended successfully */
} SALVSYNC_command_state_t;


/**
 * on-wire salvsync protocol payload.
 */
typedef struct SALVSYNC_command_hdr {
    afs_uint32 hdr_version;     /**< salvsync protocol header version */
    afs_uint32 prio;            /**< salvage priority */
    afs_uint32 volume;          /**< volume on which to operate */
    afs_uint32 parent;          /**< parent volume (for vol group linking command) */
    char partName[16];		/**< partition name, e.g. /vicepa */
    afs_uint32 reserved[6];
} SALVSYNC_command_hdr;

typedef struct SALVSYNC_response_hdr {
    afs_int32 state;
    afs_int32 prio;
    afs_int32 sq_len;
    afs_int32 pq_len;
    afs_uint32 reserved[4];
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

typedef enum {
    SALVSYNC_VOLGROUP_PARENT,
    SALVSYNC_VOLGROUP_CLONE
} SalvageQueueNodeType_t;

struct SalvageQueueNode {
    struct rx_queue q;
    struct rx_queue hash_chain;
    SalvageQueueNodeType_t type;
    union {
	struct SalvageQueueNode * parent;
	struct SalvageQueueNode * children[VOLMAXTYPES];
    } volgroup;
    SALVSYNC_command_state_t state;
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
extern afs_int32 SALVSYNC_LinkVolume(VolumeId parent, VolumeId clone,
				     char * partName, SYNC_response * res_in);

/* salvage server interfaces */
extern void SALVSYNC_salvInit(void);
extern struct SalvageQueueNode * SALVSYNC_getWork(void);
extern void SALVSYNC_doneWork(struct SalvageQueueNode *, int result);
extern void SALVSYNC_doneWorkByPid(int pid, int result);

#endif /* AFS_DEMAND_ATTACH_FS */

#endif /* _AFS_VOL_SALVSYNC_H */
