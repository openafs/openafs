/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_AFSSYSCALLS_H
#define AFS_AFSSYSCALLS_H

#ifndef AFS_NT40_ENV

/* Declare Inode type. */
#include <afs/param.h>
#ifdef AFS_NAMEI_ENV
#include <sys/types.h>
#if AFS_64BIT_ENV
typedef afs_uint64 Inode;
#else
typedef u_int64_t Inode;
#endif
#else /* AFS_NAMEI_ENV */
#ifdef AFS_64BIT_IOPS_ENV
#ifdef AFS_SGI62_ENV
typedef uint64_t Inode;
#else
error Need 64 bit Inode defined.
#endif				/* AFS_SGI62_ENV */
#else /* AFS_64BIT_IOPS_ENV */
typedef unsigned int Inode;
#endif

#ifdef AFS_DEBUG_IOPS
extern FILE *inode_debug_log;
#define AFS_DEBUG_IOPS_LOG(F) (inode_debug_log = (F))
#else
#define AFS_DEBUG_IOPS_LOG(F)
#endif

/* Glue layer. */
#ifdef AFS_LINUX20_ENV
int proc_afs_syscall(long syscall, long param1, long param2, long param3, 
		     long param4, int *rval);
#endif

/* Declarations for inode system calls. */
#ifdef AFS_SGI_XFS_IOPS_ENV
extern uint64_t icreatename64(int dev, char *partname, int p0, int p1, int p2,
			      int p3);
extern int iopen64(int dev, uint64_t inode, int usrmod);
extern int iinc64(int dev, uint64_t inode, int inode_p1);
extern int idec64(int dev, uint64_t inode, int inode_p1);
extern int ilistinode64(int dev, uint64_t inode, void *data, int *datalen);

#ifdef AFS_DEBUG_IOPS
extern uint64_t debug_icreatename64(int dev, char *partname, int p0, int p1,
				    int p2, int p3, char *file, int line);
extern int debug_iopen64(int dev, uint64_t inode, int usrmod, char *file,
			 int line);
extern int debug_iinc64(int dev, uint64_t inode, int inode_p1, char *file,
			int line);
extern int debug_idec64(int dev, uint64_t inode, int inode_p1, char *file,
			int line);

#endif /* AFS_DEBUG_IOPS */
#endif /* AFS_SGI_XFS_IOPS_ENV */

#ifdef AFS_64BIT_IOPS_ENV
extern int inode_read(int dev, Inode inode, int inode_p1, uint32_t offset,
		      char *cbuf, uint32_t count);
extern int inode_write(int dev, Inode inode, int inode_p1, uint32_t offset,
		       char *cbuf, uint32_t count);
#else
extern int inode_read();
extern int inode_write();
#endif

#ifdef AFS_SGI_VNODE_GLUE
/* flag: 1 = has NUMA, 0 = no NUMA, -1 = kernel decides. */
extern int afs_init_kernel_config(int flag);
#endif /* AFS_SGI_VNODE_GLUE */

#endif /* AFS_NAMEI_ENV */


/* minimum size of string to hand to PrintInode */
#define AFS_INO_STR_LENGTH 32
typedef char afs_ino_str_t[AFS_INO_STR_LENGTH];

/* Print either 32 or 64 bit inode numbers. char * may be NULL. In which case
 * a local statis is returned.
 */
#ifdef AFS_64BIT_IOPS_ENV
extern char *PrintInode(afs_ino_str_t, Inode);
#else
extern char *PrintInode();
#endif

/* Some places in the code assume icreate can return 0 when there's
 * an error.
 */
#define VALID_INO(I) ((I) != (Inode)-1 && (I) != (Inode)0)

#ifndef AFS_NAMEI_ENV

/* Definitions of inode macros. */
#ifdef AFS_SGI_XFS_IOPS_ENV
#ifdef AFS_DEBUG_IOPS
#define ICREATE(DEV, NAME, NI, P0, P1, P2, P3) \
	 debug_icreatename64(DEV, NAME, P0, P1, P2, P3, __FILE__, __LINE__)
#define IDEC(DEV, INO, VID)	debug_idec64(DEV, INO, VID, __FILE__, __LINE__)
#define IINC(DEV, INO, VID)	debug_iinc64(DEV, INO, VID, __FILE__, __LINE__)
#define IOPEN(DEV, INO, MODE) debug_iopen64(DEV, INO, MODE, __FILE__, __LINE__)
#else
#define ICREATE(DEV, NAME, NI, P0, P1, P2, P3) \
		icreatename64(DEV, NAME, P0, P1, P2, P3)
#define IDEC(DEV, INO, VID)	idec64(DEV, INO, VID)
#define IINC(DEV, INO, VID)	iinc64(DEV, INO, VID)
#define IOPEN(DEV, INO, MODE)	iopen64(DEV, INO, MODE)
#endif
#define AFS_IOPS_DEFINED 1
#endif /* AFS_SGI_IOPS_ENV */


#ifndef AFS_IOPS_DEFINED
#ifdef AFS_DEBUG_IOPS
#else
#define ICREATE(DEV, NAME, NI, P0, P1, P2, P3) \
	icreate(DEV, NI, P0, P1, P2, P3)
#define IDEC(DEV, INO, VID)	idec(DEV, INO, VID)
#define IINC(DEV, INO, VID)	iinc(DEV, INO, VID)
#define IOPEN(DEV, INO, MODE)	iopen(DEV, INO, MODE)
#endif
#endif /* AFS_IOPS_DEFINED */


#endif /* AFS_NAMEI_ENV */

/* Declarations for lsetpag and lpioctl */
int     lsetpag(void);
int     lpioctl(char *path, int cmd, char *cmarg, int follow);

#endif /* AFS_NT40_ENV */

#endif /* AFS_AFSSYSCALLS_H */
