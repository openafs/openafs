/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_VNODEOPS_H
#define OPENAFS_WINNT_AFSD_CM_VNODEOPS_H 1

extern unsigned int cm_mountRootGen;

extern int cm_enableServerLocks;

extern int cm_followBackupPath;

/* parms for attribute setting call */
typedef struct cm_attr {
    int         mask;
    time_t      clientModTime;
    osi_hyper_t length;
    int         unixModeBits;
    long        owner;
    long        group;
} cm_attr_t;

#define CM_ATTRMASK_CLIENTMODTIME	1	/* set if field is valid */
#define CM_ATTRMASK_LENGTH		2	/* field is valid */
#define CM_ATTRMASK_UNIXMODEBITS	4	/* field is valid */
#define CM_ATTRMASK_OWNER		8	/* field is valid */
#define CM_ATTRMASK_GROUP		0x10	/* field is valid */

/* type of rock for lookup's searches */
typedef struct cm_lookupSearch {
    cm_fid_t      fid;
    fschar_t     *searchNamep;
    normchar_t   *nsearchNamep;
    int           found;
    int           LCfound, UCfound, NCfound, ExactFound;
    int           caseFold;
    int           hasTilde;
} cm_lookupSearch_t;

#include "cm_dir.h"

typedef int (*cm_DirFuncp_t)(struct cm_scache *, struct cm_dirEntry *, void *,
                             osi_hyper_t *entryOffsetp);

/* Special path syntax for direct references to volumes

   The syntax: @vol:<cellname>{%,#}<volume> can be used to refer to a
   specific volume in a specific cell, where:

   <cellname> : name of the cell
   <volume>   : volume name or ID
 */
#define CM_PREFIX_VOL "@vol:"
#define CM_PREFIX_VOL_CCH 5

/* arrays */

extern fschar_t cm_foldUpper[];

/* functions */

extern int cm_NoneLower(normchar_t *s);

extern int cm_NoneUpper(normchar_t *s);

extern int cm_stricmp(const char *, const char *);

extern long cm_ReadMountPoint(cm_scache_t *scp, cm_user_t *userp,
                              cm_req_t *reqp);

extern long cm_EvaluateVolumeReference(clientchar_t * namep, long flags, cm_user_t * userp,
                                       cm_req_t *reqp, cm_scache_t ** outpScpp);

#ifdef DEBUG_REFCOUNT
extern long cm_NameIDbg(cm_scache_t *rootSCachep, clientchar_t *pathp, long flags,
                        cm_user_t *userp, clientchar_t *tidPathp, cm_req_t *reqp,
                        cm_scache_t **outScpp, char *, long);

extern long cm_LookupDbg(cm_scache_t *dscp, clientchar_t *namep, long flags,
                         cm_user_t *userp, cm_req_t *reqp, cm_scache_t **outpScpp,
                         char *, long);

#define cm_Lookup(a,b,c,d,e,f)  cm_LookupDbg(a,b,c,d,e,f,__FILE__,__LINE__)
#define cm_NameI(a,b,c,d,e,f,g) cm_NameIDbg(a,b,c,d,e,f,g,__FILE__,__LINE__)
#else
extern long cm_NameI(cm_scache_t *rootSCachep, clientchar_t *pathp, long flags,
                     cm_user_t *userp, clientchar_t *tidPathp, cm_req_t *reqp,
                     cm_scache_t **outScpp);
extern long cm_Lookup(cm_scache_t *dscp, clientchar_t *namep, long flags,
                      cm_user_t *userp, cm_req_t *reqp, cm_scache_t **outpScpp);
#endif

extern long cm_LookupInternal(cm_scache_t *dscp, clientchar_t *namep, long flags,
                              cm_user_t *userp, cm_req_t *reqp,
                              cm_scache_t **outpScpp);

extern afs_int32 cm_TryBulkStat(cm_scache_t *dscp, osi_hyper_t *offsetp,
                                cm_user_t *userp, cm_req_t *reqp);

extern long cm_SetAttr(cm_scache_t *scp, cm_attr_t *attrp, cm_user_t *userp,
                       cm_req_t *reqp);

extern long cm_Create(cm_scache_t *scp, clientchar_t *namep, long flags,
                      cm_attr_t *attrp, cm_scache_t **scpp,
                      cm_user_t *userp, cm_req_t *reqp);

extern long cm_FSync(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp, afs_uint32 locked);

extern void cm_StatusFromAttr(struct AFSStoreStatus *statusp,
                              struct cm_scache *scp, struct cm_attr *attrp);

extern long cm_Unlink(cm_scache_t *dscp, fschar_t *fnamep,
                      clientchar_t *cnamep,
                      cm_user_t *userp, cm_req_t *reqp);

extern long cm_ApplyDir(cm_scache_t *scp, cm_DirFuncp_t funcp, void *parmp,
                        osi_hyper_t *startOffsetp, cm_user_t *userp, cm_req_t *reqp,
                        cm_scache_t **retscp);

extern long cm_MakeDir(cm_scache_t *dscp, clientchar_t *lastNamep, long flags,
                       cm_attr_t *attrp, cm_user_t *userp, cm_req_t *reqp,
                       cm_scache_t **scpp);

extern long cm_RemoveDir(cm_scache_t *dscp, fschar_t *lastNamep, clientchar_t *originalNamep,
                         cm_user_t *userp, cm_req_t *reqp);

extern long cm_Rename(cm_scache_t *oldDscp,
                      fschar_t *oldLastNamep, clientchar_t *normalizedOldNamep,
                      cm_scache_t *newDscp, clientchar_t *newLastNamep,
                      cm_user_t *userp, cm_req_t *reqp);

extern long cm_HandleLink(cm_scache_t *linkScp, struct cm_user *userp,
                          cm_req_t *reqp);

extern long cm_Link(cm_scache_t *dscp, clientchar_t *namep, cm_scache_t *sscp,
                    long flags, cm_user_t *userp, cm_req_t *reqp);

extern long cm_SymLink(cm_scache_t *dscp, clientchar_t *namep, fschar_t *contentsp,
                       long flags, cm_attr_t *attrp, cm_user_t *userp, cm_req_t *reqp,
                       cm_scache_t **scpp);

extern long cm_AssembleLink(cm_scache_t *linkScp, fschar_t *pathSuffixp,
                            cm_scache_t **newRootScpp, cm_space_t **newSpaceBufferp,
                            cm_user_t *userp, cm_req_t *reqp);

extern int cm_ExpandSysName(clientchar_t *inp, clientchar_t *outp, long outSizeCch,
                            unsigned int sysNameIndex);

extern long cm_Open(cm_scache_t *scp, int type, cm_user_t *userp);

extern long cm_CheckOpen(cm_scache_t *scp, int openMode, int trunc,
                         cm_user_t *userp, cm_req_t *reqp);

/*
 * Combinations of file opening access bits for AFS.
 * We don't insist on write rights to open the file with FILE_WRITE_ATTRIBUTES,
 * because we want to enable the owner to set/clear the READONLY flag.
 * The RPC will fail if you can't modify the attributes, anyway.
 */
#define AFS_ACCESS_READ (FILE_GENERIC_READ & ~SYNCHRONIZE)
#define AFS_ACCESS_WRITE ((FILE_GENERIC_WRITE & ~(READ_CONTROL | SYNCHRONIZE)) \
				& ~FILE_WRITE_ATTRIBUTES)
#define AFS_ACCESS_EXECUTE (FILE_GENERIC_EXECUTE & ~SYNCHRONIZE)

typedef struct cm_lock_data {
    cm_key_t key;
    unsigned int sLockType;
    LARGE_INTEGER LOffset, LLength;
} cm_lock_data_t;

extern long cm_CheckNTOpen(cm_scache_t *scp, unsigned int desiredAccess,
                           unsigned int createDisp, cm_user_t *userp,
                           cm_req_t *reqp, cm_lock_data_t ** ldpp);

extern long cm_CheckNTOpenDone(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp,
			       cm_lock_data_t ** ldpp);

extern long cm_CheckNTDelete(cm_scache_t *dscp, cm_scache_t *scp,
                             cm_user_t *userp, cm_req_t *reqp);

extern long cm_EvaluateSymLink(cm_scache_t *dscp, cm_scache_t *linkScp,
                               cm_scache_t **outScpp, cm_user_t *userp,
                               cm_req_t *reqp);

extern long cm_FollowMountPoint(cm_scache_t *scp, cm_scache_t *dscp, cm_user_t *userp,
                                cm_req_t *reqp, cm_scache_t **outScpp);


extern long cm_Lock(cm_scache_t *scp, unsigned char sLockType,
                    LARGE_INTEGER LOffset, LARGE_INTEGER LLength, cm_key_t key,
                    int allowWait, cm_user_t *userp, cm_req_t *reqp,
                    cm_file_lock_t **lockpp);

#define CM_UNLOCK_BY_FID 	0x0001

extern long cm_UnlockByKey(cm_scache_t * scp,
                           cm_key_t key,
                           afs_uint32 flags,
                           cm_user_t * userp,
                           cm_req_t * reqp);

#define CM_UNLOCK_FLAG_MATCH_RANGE      0x01

extern long cm_Unlock(cm_scache_t *scp, unsigned char sLockType,
                      LARGE_INTEGER LOffset, LARGE_INTEGER LLength, cm_key_t key,
                      afs_uint32 flags, cm_user_t *userp, cm_req_t *reqp);

extern long cm_LockCheckRead(cm_scache_t *scp,
                             LARGE_INTEGER LOffset,
                             LARGE_INTEGER LLength,
                             cm_key_t key);

extern long cm_LockCheckWrite(cm_scache_t *scp,
                              LARGE_INTEGER LOffset,
                              LARGE_INTEGER LLength,
                              cm_key_t key);

extern void cm_CheckLocks(void);

extern void cm_ReleaseAllLocks(void);

extern void cm_LockMarkSCacheLost(cm_scache_t * scp);

extern long cm_RetryLock(cm_file_lock_t *oldFileLock, int client_is_dead);

#define CM_SESSION_SMB      0xffff
#define CM_SESSION_IFS      0xfffe
#define CM_SESSION_CMINT    0xfffd
#define CM_SESSION_RESERVED 0xfff0

extern cm_key_t cm_GenerateKey(afs_uint16 session_id, afs_offs_t process_id, afs_uint16 file_id);

extern int cm_KeyEquals(cm_key_t * k1, cm_key_t * k2, int flags);

#define MAX_SYMLINK_COUNT 64

/* make this big enough so that one buffer of dir pages won't overflow.  We'll
 * check anyway, but we want to minimize the chance that we have to leave stuff
 * unstat'd.
 */
#define CM_BULKMAX		(3 * AFSCBMAX)

/* rock for bulk stat calls */
typedef struct cm_bulkStat {
    osi_hyper_t bufOffset;	/* only do it for things in this buffer page */

    /* info for the actual call */
    int counter;			/* next free slot */
    AFSFid fids[CM_BULKMAX];
    AFSFetchStatus stats[CM_BULKMAX];
    AFSCallBack callbacks[CM_BULKMAX];
} cm_bulkStat_t;

extern afs_int32 cm_TryBulkStatRPC(cm_scache_t *dscp, cm_bulkStat_t *bbp,
                                   cm_user_t *userp, cm_req_t *reqp);

#endif /*  OPENAFS_WINNT_AFSD_CM_VNODEOPS_H */
