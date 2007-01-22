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

typedef struct cm_volume {
    afs_uint32  magic;
    cm_cell_t *cellp;		        /* never changes */
    char namep[VL_MAXNAMELEN];		/* by cm_volumeLock */
    unsigned long rwID;		        /* by cm_volumeLock */
    unsigned long roID;		        /* by cm_volumeLock */
    unsigned long bkID;		        /* by cm_volumeLock */
    struct cm_volume *nextp;	        /* by cm_volumeLock */
    struct cm_fid dotdotFid;	        /* parent of volume root */
    osi_mutex_t mx;
    long flags;			        /* by mx */
    unsigned long refCount;		/* by cm_volumeLock */
    cm_serverRef_t *rwServersp;	        /* by mx */
    cm_serverRef_t *roServersp;	        /* by mx */
    cm_serverRef_t *bkServersp;	        /* by mx */
} cm_volume_t;

#define CM_VOLUMEFLAG_RESET	1	/* reload this info on next use */

extern void cm_InitVolume(int newFile, long maxVols);

extern long cm_GetVolumeByName(struct cm_cell *, char *, struct cm_user *,
	struct cm_req *, long, cm_volume_t **);

extern long cm_GetVolumeByID(struct cm_cell *cellp, long volumeID,
	cm_user_t *userp, cm_req_t *reqp, cm_volume_t **outVolpp);

extern void cm_GetVolume(cm_volume_t *volp);

extern void cm_PutVolume(cm_volume_t *volp);

extern long cm_GetROVolumeID(cm_volume_t *volp);

extern void cm_ForceUpdateVolume(struct cm_fid *fidp, cm_user_t *userp,
	cm_req_t *reqp);

extern cm_serverRef_t **cm_GetVolServers(cm_volume_t *volp, unsigned long volume);

extern void cm_ChangeRankVolume(cm_server_t *tsp);

extern void cm_CheckVolumes(void);

extern long cm_ValidateVolume(void);

extern long cm_ShutdownVolume(void);

extern int cm_DumpVolumes(FILE *outputFile, char *cookie, int lock);
#endif /*  __CM_VOLUME_H_ENV__ */
