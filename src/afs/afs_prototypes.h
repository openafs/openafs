/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef _AFS_PROTOTYPES_H_
#define _AFS_PROTOTYPES_H_

/* afs_analyze.c */
extern void afs_FinalizeReq(struct vrequest *areq);

extern int afs_Analyze(struct conn *aconn, afs_int32 acode, struct VenusFid *afid,
			register struct vrequest *areq, int op, afs_int32 locktype,
			struct cell *cellp);

/* afs_conn.c */
extern struct conn *afs_ConnBySA(struct srvAddr *sap, unsigned short aport,
			  afs_int32 acell, struct unixuser *tu,
			  int force_if_down, afs_int32 create, afs_int32 locktype);

/* afs_dcache.c */
extern void afs_dcacheInit(int afiles, int ablocks, int aDentries, int achunk,
			   int aflags);

/* afs_memcache.c */
extern void *afs_MemCacheOpen(ino_t blkno);

/* afs_osi.c */
#if AFS_GCPAGS
extern const struct AFS_UCRED *afs_osi_proc2cred(AFS_PROC *pr);
extern void afs_osi_TraverseProcTable(void);
#endif /* AFS_GCPAGS */

/* afs_osi_pag.c */
extern afs_uint32 genpag(void);
extern afs_uint32 getpag(void);
#ifdef	AFS_OSF_ENV
extern int AddPag(struct proc *p, afs_int32 aval, struct AFS_UCRED **credpp);
#else	/* AFS_OSF_ENV */
extern int AddPag(afs_int32 aval, struct AFS_UCRED **credpp);
#endif
extern afs_uint32 afs_get_pag_from_groups(gid_t g0, gid_t g1);
extern void afs_get_groups_from_pag(afs_uint32 pag, gid_t *g0p, gid_t *g1p);
extern afs_int32 PagInCred(const struct AFS_UCRED *cred);

/* OS/osi_sleep.c */
extern void afs_osi_InitWaitHandle(struct afs_osi_WaitHandle *achandle);
extern void afs_osi_CancelWait(struct afs_osi_WaitHandle *achandle);
extern int afs_osi_Wait(afs_int32 ams, struct afs_osi_WaitHandle *ahandle, int aintok);

/* afs_osifile.c */
#ifdef AFS_SGI62_ENV
extern void *osi_UFSOpen(ino_t);
#else
extern void *osi_UFSOpen();
#endif

/* afs_server.c */
extern struct server *afs_FindServer (afs_int32 aserver, ushort aport,
				     afsUUID *uuidp, afs_int32 locktype);
extern struct server *afs_GetServer(afs_uint32 *aserver, afs_int32 nservers,
				    afs_int32 acell, u_short aport,
				    afs_int32 locktype, afsUUID *uuidp,
				    afs_int32 addr_uniquifier);
extern void afs_MarkServerUpOrDown(struct srvAddr *sa, int a_isDown);
extern void afs_ServerDown(struct srvAddr *sa);


/* afs_user.c */
extern struct unixuser *afs_FindUser(afs_int32 auid, afs_int32 acell, afs_int32 locktype);
#if AFS_GCPAGS
extern afs_int32 afs_GCPAGs(afs_int32 *ReleasedCount);
extern void afs_GCPAGs_perproc_func(AFS_PROC *pproc);
#endif /* AFS_GCPAGS */

/* afs_util.c */
extern char *afs_cv2string(char *ttp, afs_uint32 aval);
extern void print_internet_address(char *preamble, struct srvAddr *sa,
			    char *postamble, int flag);
extern afs_int32 afs_data_pointer_to_int32(const void *p);


/* afs_vcache.c */
void afs_vcacheInit(int astatSize);
extern struct vcache *afs_FindVCache(struct VenusFid *afid, afs_int32 lockit,
				     afs_int32 locktype, afs_int32 *retry, afs_int32 flag);
extern afs_int32 afs_FetchStatus(struct vcache *avc, struct VenusFid *afid,
			     struct vrequest *areq,
			     struct AFSFetchStatus *Outsp);

extern afs_int32 afs_FlushVCBs(afs_int32 lockit);
extern void afs_InactiveVCache(struct vcache *avc, struct AFS_UCRED *acred);
extern struct vcache *afs_LookupVCache(struct VenusFid *afid,
				       struct vrequest *areq,
				       afs_int32 *cached, afs_int32 locktype,
				       struct vcache *adp, char *aname);
extern int afs_FlushVCache(struct vcache *avc, int *slept);
extern struct vcache *afs_GetRootVCache(struct VenusFid *afid,
					struct vrequest *areq, afs_int32 *cached,
					struct volume *tvolp, afs_int32 locktype);
extern struct vcache *afs_NewVCache(struct VenusFid *afid,
				    struct server *serverp,
				    afs_int32 lockit, afs_int32 locktype);
extern int afs_VerifyVCache2(struct vcache *avc, struct vrequest *areq);



/* afs_volume.c */
extern struct volume *afs_FindVolume(struct VenusFid *afid, afs_int32 locktype);
extern void InstallVolumeEntry(struct volume *av, struct vldbentry *ve,
			       int acell);
extern void InstallNVolumeEntry(struct volume *av, struct nvldbentry *ve,
			 int acell);
extern void InstallUVolumeEntry(struct volume *av, struct uvldbentry *ve,
			 int acell, struct cell *tcell, struct vrequest *areq);
extern void afs_ResetVolumeInfo(struct volume *tv);

#if defined(AFS_SUN5_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_AIX_ENV)
#include "../afs/osi_prototypes.h"
#endif


#endif /* _AFS_PROTOTYPES_H_ */

