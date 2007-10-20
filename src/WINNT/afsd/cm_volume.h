/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CM_VOLUME_H_ENV__
#define __CM_VOLUME_H_ENV__ 1

#define VL_MAXNAMELEN                   65

#define CM_VOLUME_MAGIC    ('V' | 'O' <<8 | 'L'<<16 | 'M'<<24)

enum volstatus {vl_online, vl_busy, vl_offline, vl_alldown, vl_unknown};

typedef struct cm_vol_state {
    afs_uint32      ID;                 /* by mx */
    struct cm_volume *nextp;            /* volumeIDHashTable; by cm_volumeLock */
    cm_serverRef_t *serversp;           /* by mx */
    enum volstatus  state;              /* by mx */
    afs_uint32      flags;              /* by mx */
} cm_vol_state_t;

typedef struct cm_volume {
    osi_queue_t q;                      /* LRU queue; cm_volumeLock */
    afs_uint32  magic;
    struct cm_volume *allNextp;	        /* allVolumes; by cm_volumeLock */
    struct cm_volume *nameNextp;        /* volumeNameHashTable; by cm_volumeLock */
    cm_cell_t *cellp;		        /* never changes */
    char namep[VL_MAXNAMELEN];		/* name of the normal volume - assigned during allocation; */
                                        /* by cm_volumeLock */
    struct cm_vol_state rw;	        /* by cm_volumeLock */
    struct cm_vol_state ro;		/* by cm_volumeLock */
    struct cm_vol_state bk;		/* by cm_volumeLock */
    struct cm_fid dotdotFid;	        /* parent of volume root */
    osi_mutex_t mx;
    afs_uint32 flags;			/* by mx */
    afs_uint32 refCount;		/* by cm_volumeLock */
} cm_volume_t;

#define CM_VOLUMEFLAG_RESET	   1	/* reload this info on next use */
#define CM_VOLUMEFLAG_IN_HASH      2
#define CM_VOLUMEFLAG_IN_LRU_QUEUE 4


typedef struct cm_volumeRef {
    struct cm_volumeRef * next;
    afs_uint32  volID;
} cm_volumeRef_t;

extern void cm_InitVolume(int newFile, long maxVols);

extern long cm_GetVolumeByName(struct cm_cell *cellp, char *volNamep, 
                               struct cm_user *userp, struct cm_req *reqp, 
                               afs_uint32 flags, cm_volume_t **outVolpp);

extern long cm_GetVolumeByID(struct cm_cell *cellp, afs_uint32 volumeID,
                             cm_user_t *userp, cm_req_t *reqp, 
                             afs_uint32 flags, cm_volume_t **outVolpp);

#define CM_GETVOL_FLAG_CREATE               1
#define CM_GETVOL_FLAG_NO_LRU_UPDATE        2

/* hash define.  Must not include the cell, since the callback revocation code
 * doesn't necessarily know the cell in the case of a multihomed server
 * contacting us from a mystery address.
 */
#define CM_VOLUME_ID_HASH(volid)   ((unsigned long) volid \
					% cm_data.volumeHashTableSize)

#define CM_VOLUME_NAME_HASH(name)  (SDBMHash(name) % cm_data.volumeHashTableSize)

extern afs_uint32 SDBMHash(const char *);

extern void cm_GetVolume(cm_volume_t *volp);

extern void cm_PutVolume(cm_volume_t *volp);

extern long cm_GetROVolumeID(cm_volume_t *volp);

extern void cm_ForceUpdateVolume(struct cm_fid *fidp, cm_user_t *userp,
	cm_req_t *reqp);

extern cm_serverRef_t **cm_GetVolServers(cm_volume_t *volp, afs_uint32 volume);

extern void cm_ChangeRankVolume(cm_server_t *tsp);

extern void cm_RefreshVolumes(void);

extern long cm_ValidateVolume(void);

extern long cm_ShutdownVolume(void);

extern int cm_DumpVolumes(FILE *outputFile, char *cookie, int lock);

extern int cm_VolNameIsID(char *aname);

extern void cm_RemoveVolumeFromNameHashTable(cm_volume_t * volp);

extern void cm_RemoveVolumeFromIDHashTable(cm_volume_t * volp, afs_uint32 volType);

extern void cm_AddVolumeToNameHashTable(cm_volume_t * volp);

extern void cm_AddVolumeToIDHashTable(cm_volume_t * volp, afs_uint32 volType);

extern void cm_AdjustVolumeLRU(cm_volume_t *volp);

extern void cm_RemoveVolumeFromLRU(cm_volume_t *volp);

extern void cm_CheckOfflineVolumes(void);

extern long cm_CheckOfflineVolume(cm_volume_t *volp, afs_uint32 volID);

extern void cm_UpdateVolumeStatus(cm_volume_t *volp, afs_uint32 volID);

extern void cm_VolumeStatusNotification(cm_volume_t * volp, afs_uint32 volID, enum volstatus old, enum volstatus new);

extern enum volstatus cm_GetVolumeStatus(cm_volume_t *volp, afs_uint32 volID);
#endif /*  __CM_VOLUME_H_ENV__ */
