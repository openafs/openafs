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

#ifdef AFS_NT40_ENV
/* We don't include Unix afssyscalls.h on Windows, so: */
#define VALID_INO(I) ((I != (__int64)-1) && (I != (__int64)0))

/* minimum size of string to hand to PrintInode */
#define AFS_INO_STR_LENGTH 32
typedef char afs_ino_str_t[AFS_INO_STR_LENGTH];

extern char *PrintInode(char *, Inode);
#endif

/* Basic file operations */
extern FD_t nt_open(const char *name, int flags, int mode);
extern int nt_close(FD_t fd);
extern int nt_write(FD_t fd, void *buf, afs_sfsize_t size);
extern int nt_read(FD_t fd, void *buf, afs_sfsize_t size);
extern int nt_pread(FD_t fd, void * buf, afs_sfsize_t count, afs_foff_t offset);
extern int nt_pwrite(FD_t fd, const void * buf, afs_sfsize_t count, afs_foff_t offset);
extern afs_sfsize_t nt_size(FD_t fd);
extern int nt_getFileCreationTime(FD_t fd, FILETIME * ftime);
extern int nt_setFileCreationTime(FD_t fd, FILETIME * ftime);
extern int nt_sync(int cdrive);
extern int nt_ftruncate(FD_t fd, afs_foff_t len);
extern int nt_fsync(FD_t fd);
extern int nt_seek(FD_t fd, afs_foff_t off, int whence);
extern FILE *nt_fdopen(IHandle_t * h, char *fdperms);
extern int nt_unlink(char *name);

extern void nt_DevToDrive(char *drive, int dev);
extern int nt_DriveToDev(char *drive);
#endif /* _NTOPS_H_ */
