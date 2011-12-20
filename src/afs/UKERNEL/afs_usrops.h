/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __AFS_USROPS_H__
#define __AFS_USROPS_H__ 1

#ifdef KERNEL
# include "afs/sysincludes.h"
# include "afsincludes.h"
#endif /* KERNEL */

/*
 * Macros to manipulate doubly linked lists
 */
#define DLL_INIT_LIST(_HEAD, _TAIL) \
    { _HEAD = NULL ; _TAIL = NULL; }

#define DLL_INSERT_TAIL(_ELEM, _HEAD, _TAIL, _NEXT, _PREV) \
{ \
    if (_HEAD == NULL) { \
	_ELEM->_NEXT = NULL; \
	_ELEM->_PREV = NULL; \
	_HEAD = _ELEM; \
	_TAIL = _ELEM; \
    } else { \
	_ELEM->_NEXT = NULL; \
	_ELEM->_PREV = _TAIL; \
	_TAIL->_NEXT = _ELEM; \
	_TAIL = _ELEM; \
    } \
}

#define DLL_DELETE(_ELEM, _HEAD, _TAIL, _NEXT, _PREV) \
{ \
    if (_ELEM->_NEXT == NULL) { \
	_TAIL = _ELEM->_PREV; \
    } else { \
	_ELEM->_NEXT->_PREV = _ELEM->_PREV; \
    } \
    if (_ELEM->_PREV == NULL) { \
	_HEAD = _ELEM->_NEXT; \
    } else { \
	_ELEM->_PREV->_NEXT = _ELEM->_NEXT; \
    } \
    _ELEM->_NEXT = NULL; \
    _ELEM->_PREV = NULL; \
}

extern char *uafs_mountDir;

extern struct afsconf_dir *afs_cdir;
extern char afs_LclCellName[64];

extern int afs_osicred_Initialized;

extern void uafs_InitClient(void);
extern void uafs_InitThread(void);
extern int uafs_Setup(const char *mount);
extern int uafs_ParseArgs(int argc, char **argv);
extern int uafs_Run(void);
extern const char* uafs_MountDir(void);
extern void uafs_RxServerProc(void);
extern int uafs_LookupLink(struct usr_vnode *vp, struct usr_vnode *parentP,
			   struct usr_vnode **vpp);
extern int uafs_LookupName(char *path, struct usr_vnode *parentP,
			   struct usr_vnode **vpp, int follow,
			   int no_eval_mtpt);
extern int uafs_LookupParent(char *path, struct usr_vnode **vpp);
extern int uafs_GetAttr(struct usr_vnode *vp, struct stat *stats);

extern int uafs_SetTokens(char *buf, int len);
extern int uafs_SetTokens_r(char *buf, int len);
extern int uafs_mkdir(char *path, int mode);
extern int uafs_mkdir_r(char *path, int mode);
extern int uafs_chdir(char *path);
extern int uafs_chdir_r(char *path);
extern int uafs_open(char *path, int flags, int mode);
extern int uafs_open_r(char *path, int flags, int mode);
extern int uafs_creat(char *path, int mode);
extern int uafs_creat_r(char *path, int mode);
extern int uafs_write(int fd, char *buf, int len);
extern int uafs_pwrite(int fd, char *buf, int len, off_t offset);
extern int uafs_pwrite_r(int fd, char *buf, int len, off_t offset);
extern int uafs_read(int fd, char *buf, int len);
extern int uafs_pread(int fd, char *buf, int leni, off_t offset);
extern int uafs_pread_r(int fd, char *buf, int len, off_t offset);
extern int uafs_pread_nocache(int fd, char *buf, int leni, off_t offset);
extern int uafs_pread_nocache_r(int fd, char *buf, int len, off_t offset);
extern int uafs_fsync(int fd);
extern int uafs_fsync_r(int fd);
extern int uafs_close(int fd);
extern int uafs_close_r(int fd);
extern int uafs_stat(char *path, struct stat *stats);
extern int uafs_stat_r(char *path, struct stat *stats);
extern int uafs_lstat(char *path, struct stat *stats);
extern int uafs_lstat_r(char *path, struct stat *stats);
extern int uafs_fstat(int fd, struct stat *stats);
extern int uafs_fstat_r(int fd, struct stat *stats);
extern int uafs_truncate(char *path, int len);
extern int uafs_truncate_r(char *path, int len);
extern int uafs_ftruncate(int fd, int len);
extern int uafs_ftruncate_r(int fd, int len);
extern int uafs_lseek(int fd, int offset, int whence);
extern int uafs_lseek_r(int fd, int offset, int whence);
extern int uafs_chmod(char *path, int mode);
extern int uafs_chmod_r(char *path, int mode);
extern int uafs_fchmod(int fd, int mode);
extern int uafs_fchmod_r(int fd, int mode);
extern int uafs_symlink(char *target, char *source);
extern int uafs_symlink_r(char *target, char *source);
extern int uafs_unlink(char *path);
extern int uafs_unlink_r(char *path);
extern int uafs_rmdir(char *path);
extern int uafs_rmdir_r(char *path);
extern int uafs_readlink(char *path, char *buf, int len);
extern int uafs_readlink_r(char *path, char *buf, int len);
extern int uafs_link(char *existing, char *new);
extern int uafs_link_r(char *existing, char *new);
extern int uafs_rename(char *old, char *new);
extern int uafs_rename_r(char *old, char *new);
extern int uafs_FlushFile(char *path);
extern int uafs_FlushFile_r(char *path);
extern usr_DIR *uafs_opendir(char *path);
extern usr_DIR *uafs_opendir_r(char *path);
extern struct usr_dirent *uafs_readdir(usr_DIR * dirp);
extern struct usr_dirent *uafs_readdir_r(usr_DIR * dirp);
extern int uafs_getdents(int fd, struct min_direct *buf, int len);
extern int uafs_getdents_r(int fd, struct min_direct *buf, int len);
extern int uafs_closedir(usr_DIR * dirp);
extern int uafs_closedir_r(usr_DIR * dirp);
extern void uafs_ThisCell(char *namep);
extern void uafs_ThisCell_r(char *namep);
extern int uafs_klog(char *user, char *cell, char *passwd, char **reason);
extern int uafs_klog_r(char *user, char *cell, char *passwd, char **reason);
extern int uafs_unlog(void);
extern int uafs_unlog_r(void);
extern void uafs_SetRxPort(int);
extern char *uafs_afsPathName(char *);
extern int uafs_RPCStatsEnableProc(void);
extern int uafs_RPCStatsDisableProc(void);
extern int uafs_RPCStatsEnablePeer(void);
extern int uafs_RPCStatsDisablePeer(void);
extern int uafs_IsRoot(char *path);
extern int uafs_statmountpoint_r(char *path);
extern int uafs_statvfs(struct statvfs *buf);
extern void uafs_Shutdown(void);
extern void uafs_mount(void);
extern void uafs_setMountDir(const char *dir);
extern int uafs_fork(int wait, void* (*cbf) (void *), void *rock);
extern int uafs_access(char *path, int amode);

#endif /* __AFS_USROPS_H__ */
