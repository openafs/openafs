/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* User space file system operations for Unix platforms. */

#ifndef _AFS_NAMEI_OPS_H_H_
#define _AFS_NAMEI_OPS_H_H_

#ifdef AFS_NAMEI_ENV

/* Basic file operations */
extern FILE *namei_fdopen(IHandle_t * h, char *fdperms);
extern int namei_unlink(char *name);

/* Inode operations */
extern Inode namei_MakeSpecIno(int volid, int type);
extern Inode namei_icreate(IHandle_t * lh, char *part, afs_uint32 p1,
			   afs_uint32 p2, afs_uint32 p3, afs_uint32 p4);
extern IHandle_t *namei_icreate_init(IHandle_t *lh, int dev, char *part,
                                     afs_uint32 p1, afs_uint32 p2,
                                     afs_uint32 p3, afs_uint32 p4);
extern FD_t namei_iopen(IHandle_t * h);
extern int namei_irelease(IHandle_t * h);
afs_sfsize_t namei_iread(IHandle_t * h, afs_foff_t offset, char *buf,
			 afs_fsize_t size);
afs_sfsize_t namei_iwrite(IHandle_t * h, afs_foff_t offset, char *buf,
			  afs_fsize_t size);
extern int namei_dec(IHandle_t * h, Inode ino, int p1);
extern int namei_inc(IHandle_t * h, Inode ino, int p1);
extern int namei_GetLinkCount(FdHandle_t * h, Inode ino, int lockit, int fixup, int nowrite);
extern int namei_SetLinkCount(FdHandle_t * h, Inode ino, int count, int locked);
extern int namei_ViceREADME(char *partition);
extern int namei_FixSpecialOGM(FdHandle_t *h, int check);
#include "nfs.h"
#include "viceinode.h"
int namei_ListAFSFiles(char *dev,
		       int (*write_fun) (FILE * fp,
					 struct ViceInodeInfo * info,
					 char *dir, char *file), FILE * fp,
		       int (*judge_fun) (struct ViceInodeInfo * info,
					 afs_uint32 vid, void *rock),
		       afs_uint32 singleVolumeNumber, void *rock);
int ListViceInodes(char *devname, char *mountedOn, FILE *inodeFile,
		   int (*judgeInode) (struct ViceInodeInfo * info, afs_uint32 vid,
				      void *rock),
		   afs_uint32 singleVolumeNumber, int *forcep, int forceR,
		   char *wpath, void *rock);

#define NAMEI_LCOMP_LEN 32
#define NAMEI_PATH_LEN 256

#ifdef AFS_NT40_ENV
#define NAMEI_DRIVE_LEN 3
#define NAMEI_HASH_LEN 2
#define NAMEI_COMP_LEN 18
typedef struct {
    char n_drive[NAMEI_DRIVE_LEN];
    char n_voldir[NAMEI_COMP_LEN];
    char n_dir[NAMEI_HASH_LEN];
    char n_inode[NAMEI_COMP_LEN];
    char n_path[NAMEI_PATH_LEN];
} namei_t;
#else
#define NAMEI_SCOMP_LEN 12
typedef struct {
    char n_base[NAMEI_LCOMP_LEN];
    char n_voldir1[NAMEI_SCOMP_LEN];
    char n_voldir2[NAMEI_LCOMP_LEN];
    char n_dir1[NAMEI_SCOMP_LEN];
    char n_dir2[NAMEI_SCOMP_LEN];
    char n_inode[NAMEI_LCOMP_LEN];
    char n_path[NAMEI_PATH_LEN];
} namei_t;
#endif

void namei_HandleToName(namei_t * name, IHandle_t * h);
int namei_ConvertROtoRWvolume(char *pname, afs_uint32 volumeId);
int namei_replace_file_by_hardlink(IHandle_t *hLink, IHandle_t *hTarget);

# ifdef AFS_SALSRV_ENV
#  include <afs/work_queue.h>
extern void namei_SetWorkQueue(struct afs_work_queue *wq);
# endif

#endif /* AFS_NAMEI_ENV */

#endif /* _AFS_NAMEI_OPS_H_H_ */
