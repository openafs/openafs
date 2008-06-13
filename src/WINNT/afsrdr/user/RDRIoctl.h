
extern void RDR_InitIoctl(void);

extern void RDR_SetupIoctl(ULONG index, cm_fid_t *parentFid, cm_fid_t *rootFid, cm_user_t *userp);

extern void RDR_CleanupIoctl(ULONG index);

extern afs_int32 RDR_IoctlRead(cm_user_t *userp, ULONG RequestId, ULONG BufferLength, void *MappedBuffer,
                               ULONG *pBytesProcessed, cm_req_t *reqp);

extern afs_int32 RDR_IoctlWrite(cm_user_t *userp, ULONG RequestId, ULONG BufferLength, void *MappedBuffer, 
                                cm_req_t *reqp);

#ifdef RDR_IOCTL_PRIVATE
typedef struct RDR_ioctl {
    struct RDR_ioctl *next, *prev;
    ULONG             index;
    cm_fid_t          parentFid;
    cm_fid_t          rootFid;
    cm_scache_t      *parentScp;
    cm_ioctl_t        ioctl;
} RDR_ioctl_t;

/* procedure implementing an ioctl */
typedef long (RDR_ioctlProc_t)(RDR_ioctl_t *, struct cm_user *userp);

extern void RDR_IoctlPrepareWrite(RDR_ioctl_t *ioctlp);

extern afs_int32 RDR_IoctlPrepareRead(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern RDR_ioctl_t *RDR_FindIoctl(ULONG index);

extern afs_int32 RDR_ParseIoctlPath(RDR_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                                    cm_scache_t **scpp, afs_uint32 flags);

extern afs_int32 RDR_ParseIoctlParent(RDR_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                                      cm_scache_t **scpp, char *leafp);

extern afs_int32 
RDR_IoctlSetToken(struct RDR_ioctl *ioctlp, struct cm_user *userp);

extern afs_int32 RDR_IoctlGetACL(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetFileCellName(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSetACL(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlFlushAllVolumes(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlFlushVolume(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlFlushFile(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSetVolumeStatus(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetVolumeStatus(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetFid(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetOwner(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlWhereIs(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlStatMountPoint(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlDeleteMountPoint(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlCheckServers(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGag(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlCheckVolumes(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSetCacheSize(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetCacheParms(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetCell(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlNewCell(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetWsCell(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSysName(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetCellStatus(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSetCellStatus(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSetSPrefs(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetSPrefs(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlStoreBehind(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlCreateMountPoint(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_CleanFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_FlushFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_FlushVolume(cm_user_t *, cm_req_t *reqp, afs_uint32 cell, afs_uint32 volume);

extern afs_int32 cm_FlushParent(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 RDR_IoctlTraceControl(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSetToken(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetTokenIter(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetToken(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlDelToken(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlDelAllToken(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSymlink(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlIslink(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlListlink(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlDeletelink(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlMakeSubmount(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlGetRxkcrypt(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlSetRxkcrypt(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlShutdown(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlFreemountAddCell(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlFreemountRemoveCell(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlMemoryDump(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlRxStatProcess(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlRxStatPeer(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlUUIDControl(struct RDR_ioctl * ioctlp, struct cm_user *userp);

extern afs_int32 RDR_IoctlPathAvailability(struct RDR_ioctl * ioctlp, struct cm_user *userp);

extern afs_int32 RDR_IoctlGetFileType(RDR_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 RDR_IoctlVolStatTest(struct RDR_ioctl *ioctlp, struct cm_user *userp);

extern afs_int32 RDR_IoctlUnicodeControl(struct RDR_ioctl *ioctlp, struct cm_user * userp);
#endif

