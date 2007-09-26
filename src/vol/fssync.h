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

/*
	System:		VICE-TWO
	Module:		fssync.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef __fssync_h_
#define __fssync_h_


#define FSYNC_PROTO_VERSION     2


/* FSYNC command codes */
#define FSYNC_VOL_ON		SYNC_COM_CODE_DECL(0)	/* Volume online */
#define FSYNC_VOL_OFF		SYNC_COM_CODE_DECL(1)	/* Volume offline */
#define FSYNC_VOL_LISTVOLUMES	SYNC_COM_CODE_DECL(2)	/* Update local volume list */
#define FSYNC_VOL_NEEDVOLUME	SYNC_COM_CODE_DECL(3)	/* Put volume in whatever mode (offline, or whatever)
							 * best fits the attachment mode provided in reason */
#define FSYNC_VOL_MOVE	        SYNC_COM_CODE_DECL(4)	/* Generate temporary relocation information
							 * for this volume to another site, to be used
							 * if this volume disappears */
#define	FSYNC_VOL_BREAKCBKS	SYNC_COM_CODE_DECL(5)	/* Break all the callbacks on this volume */
#define FSYNC_VOL_DONE		SYNC_COM_CODE_DECL(6)	/* Done with this volume (used after a delete).
							 * Don't put online, but remove from list */
#define FSYNC_VOL_QUERY         SYNC_COM_CODE_DECL(7)   /* query the volume state */
#define FSYNC_VOL_QUERY_HDR     SYNC_COM_CODE_DECL(8)   /* query the volume disk data structure */
#define FSYNC_VOL_QUERY_VOP     SYNC_COM_CODE_DECL(9)   /* query the volume for pending vol op info */
#define FSYNC_VOL_STATS_GENERAL SYNC_COM_CODE_DECL(10)  /* query the general volume package statistics */
#define FSYNC_VOL_STATS_VICEP   SYNC_COM_CODE_DECL(11)  /* query the per-partition volume package stats */
#define FSYNC_VOL_STATS_HASH    SYNC_COM_CODE_DECL(12)  /* query the per hash-chain volume package stats */
#define FSYNC_VOL_STATS_HDR     SYNC_COM_CODE_DECL(13)  /* query the volume header cache statistics */
#define FSYNC_VOL_STATS_VLRU    SYNC_COM_CODE_DECL(14)  /* query the VLRU statistics */

/* FSYNC reason codes */
#define FSYNC_WHATEVER		SYNC_REASON_CODE_DECL(0)  /* XXXX */
#define FSYNC_SALVAGE		SYNC_REASON_CODE_DECL(1)  /* volume is being salvaged */
#define FSYNC_MOVE		SYNC_REASON_CODE_DECL(2)  /* volume is being moved */
#define FSYNC_OPERATOR		SYNC_REASON_CODE_DECL(3)  /* operator forced volume offline */
#define FSYNC_EXCLUSIVE         SYNC_REASON_CODE_DECL(4)  /* somebody else has the volume offline */
#define FSYNC_UNKNOWN_VOLID     SYNC_REASON_CODE_DECL(5)  /* volume id not known by fileserver */
#define FSYNC_HDR_NOT_ATTACHED  SYNC_REASON_CODE_DECL(6)  /* volume header not currently attached */
#define FSYNC_NO_PENDING_VOL_OP SYNC_REASON_CODE_DECL(7)  /* no volume operation pending */
#define FSYNC_VOL_PKG_ERROR     SYNC_REASON_CODE_DECL(8)  /* error in the volume package */

/* FSYNC response codes */

/* FSYNC flag codes */

struct offlineInfo {
    afs_uint32 volumeID;
    char partName[16];
};

typedef struct FSSYNC_VolOp_hdr {
    afs_uint32 volume;          /* volume id associated with request */
    char partName[16];		/* partition name, e.g. /vicepa */
} FSSYNC_VolOp_hdr;

typedef struct FSSYNC_VolOp_command {
    SYNC_command_hdr * hdr;
    FSSYNC_VolOp_hdr * vop;
    SYNC_command * com;
    struct offlineInfo * v;
    struct offlineInfo * volumes;
} FSSYNC_VolOp_command;

typedef struct FSSYNC_VolOp_info {
    SYNC_command_hdr com;
    FSSYNC_VolOp_hdr vop;
} FSSYNC_VolOp_info;


typedef struct FSSYNC_StatsOp_hdr {
    union {
	afs_uint32 vlru_generation;
	afs_uint32 hash_bucket;
	char partName[16];
    } args;
} FSSYNC_StatsOp_hdr;

typedef struct FSSYNC_StatsOp_command {
    SYNC_command_hdr * hdr;
    FSSYNC_StatsOp_hdr * sop;
    SYNC_command * com;
} FSSYNC_StatsOp_command;



/*
 * common interfaces
 */
extern void FSYNC_Init(void);

/* 
 * fsync client interfaces 
 */
extern void FSYNC_clientFinis(void);
extern int FSYNC_clientInit(void);
extern int FSYNC_clientChildProcReconnect(void);

/* generic low-level interface */
extern afs_int32 FSYNC_askfs(SYNC_command * com, SYNC_response * res);

/* generic higher-level interface */
extern afs_int32 FSYNC_GenericOp(void * ext_hdr, size_t ext_len,
				 int command, int reason,
				 SYNC_response * res);

/* volume operations interface */
extern afs_int32 FSYNC_VolOp(VolumeId volume, char *partName, int com, int reason, 
			     SYNC_response * res);

/* statistics query interface */
extern afs_int32 FSYNC_StatsOp(FSSYNC_StatsOp_hdr * scom, int command, int reason,
			       SYNC_response * res_in);

#endif /* __fssync_h_ */
