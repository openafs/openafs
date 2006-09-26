/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _NTOPS_H_
#define _NTOPS_H_


/* We don't include Unix afssyscalls.h, so: */
#define VALID_INO(I) ((I != (__int64)-1) && (I != (__int64)0))

/* minimum size of string to hand to PrintInode */
#define AFS_INO_STR_LENGTH 32
typedef char afs_ino_str_t[AFS_INO_STR_LENGTH];

char *PrintInode(char *s, Inode ino);

/* Basic file operations */
extern FD_t nt_open(char *name, int flags, int mode);
extern int nt_close(FD_t fd);
extern int nt_write(FD_t fd, char *buf, size_t size);
extern int nt_read(FD_t fd, char *buf, size_t size);
extern int nt_size(FD_t fd);
extern int nt_getFileCreationTime(FD_t fd, FILETIME * ftime);
extern int nt_setFileCreationTime(FD_t fd, FILETIME * ftime);
extern int nt_sync(int cdrive);
extern int nt_ftruncate(FD_t fd, int len);
extern int nt_fsync(FD_t fd);
extern int nt_seek(FD_t fd, int off, int where);
extern FILE *nt_fdopen(IHandle_t * h, char *fdperms);
extern int nt_unlink(char *name);

/* Inode operations */
extern Inode nt_MakeSpecIno(int type);
extern Inode nt_icreate(IHandle_t * h, char *p, int p1, int p2, int p3,
			int p4);
extern FD_t nt_iopen(IHandle_t * h);
extern int nt_irelease(IHandle_t * h);
int nt_iread(IHandle_t * h, int offset, char *buf, int size);
int nt_iwrite(IHandle_t * h, int offset, char *buf, int size);
extern int nt_dec(IHandle_t * h, Inode ino, int p1);
extern int nt_inc(IHandle_t * h, Inode ino, int p1);
extern int nt_GetLinkCount(FdHandle_t * h, Inode ino, int lockit);
int nt_ListAFSFiles(char *dev,
		    int (*write_fun) (FILE * fp, struct ViceInodeInfo *,
				      char *dir, char *file), FILE * fp,
		    int (*judge_fun) (struct ViceInodeInfo *, int vid, void *rock),
		    int singleVolumeNumber, void *rock);
int ListViceInodes(char *devname, char *mountedOn, char *resultFile,
		   int (*judgeInode) (struct ViceInodeInfo * info, int vid, void *rock),
		   int singleVolumeNumber, int *forcep, int forceR,
		   char *wpath, void *rock);


int nt_HandleToName(char *name, IHandle_t * h);
char *nt_HandleToVolDir(char *name, IHandle_t * h);

extern int Testing;

#endif /* _NTOPS_H_ */
