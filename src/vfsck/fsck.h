/*
 * Copyright (c) 1980, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)fsck.h	5.10 (Berkeley) 2/1/90
 */



#ifdef	AFS_HPUX_ENV
#define	MAXDUP		5000	/* limit on dup blks (per inode) */
#define	MAXBAD		5000	/* limit on bad blks (per inode) */
#else
#define	MAXDUP		10	/* limit on dup blks (per inode) */
#define	MAXBAD		10	/* limit on bad blks (per inode) */
#endif
#define MAXBUFSPACE	128*1024	/* maximum space to allocate to buffers */

#ifndef BUFSIZ
#define BUFSIZ 1024
#endif

#define	USTATE	01		/* inode not allocated */
#define	FSTATE	02		/* inode is file */
#define	DSTATE	03		/* inode is directory */
#define	DFOUND	04		/* directory found during descent */
#define	DCLEAR	05		/* directory is to be cleared */
#define	FCLEAR	06		/* file is to be cleared */
#ifdef VICE
#define VSTATE  07		/* inode is a AFS file system inode */
#endif /* VICE */
#ifdef	AFS_HPUX_ENV
#define	ACLS	1
#endif
#if defined(ACLS) && defined(AFS_HPUX_ENV)
#define	CSTATE	8		/* inode is a continuation inode */
#define	CRSTATE	16		/* continuation inode has been referenced */
#define	HASCINODE 32		/* has continuation inode associated with it */
#ifdef	VICE
#define	STATE	(USTATE|FSTATE|DSTATE|DCLEAR|CSTATE|CRSTATE|VSTATE)
#else
#define	STATE	(USTATE|FSTATE|DSTATE|DCLEAR|CSTATE|CRSTATE)
#endif
#endif /* ACLS */

#ifdef VICE
#ifdef	AFS_SUN_ENV
/* The following define is in order to utilize one vfsck for SunOS 4.1 & 4.1.1; note
 * that the ic_flags and size.val[0] will be zero except for AFS inodes in 4.1.1...
 */
#if	defined(AFS_SUN56_ENV)
#define	VICEINODE	((dp->di_vicemagic == VICEMAGIC)   \
			 && (dp->di_mode & IFMT) == IFREG)
#define OLDVICEINODE    (!dp->di_ic.ic_uid && (dp->di_ic.ic_gid== 0xfffffffe))
#else
#define	VICEINODE	(((dp->di_ic.ic_gen == VICEMAGIC) || \
			 (dp->di_ic.ic_flags || dp->di_ic.ic_size.val[0])) \
			 && (dp->di_mode & IFMT) == IFREG)
#endif /* AFS_SUN56_ENV */
#else
#define	VICEINODE	(IS_DVICEMAGIC(dp) && (dp->di_mode & IFMT) == IFREG)
#endif
#endif
#if	defined(AFS_SUN56_ENV)
#define		OFF_T		offset_t
#define		UOFF_T		u_offset_t
#else
#define		OFF_T		long
#define		UOFF_T		long
#endif /* AFS_SUN56_ENV */

/*
 * buffer cache structure.
 */
struct bufarea {
    struct bufarea *b_next;	/* free list queue */
    struct bufarea *b_prev;	/* free list queue */
    daddr_t b_bno;
    int b_size;
    int b_errs;
    int b_flags;
    union {
	char *b_buf;		/* buffer space */
	daddr_t *b_indir;	/* indirect block */
	struct fs *b_fs;	/* super block */
	struct cg *b_cg;	/* cylinder group */
	struct dinode *b_dinode;	/* inode block */
    } b_un;
    char b_dirty;
};

#define	B_INUSE 1

#define	MINBUFS		5	/* minimum number of buffers required */
struct bufarea bufhead;		/* head of list of other blks in filesys */
struct bufarea sblk;		/* file system superblock */
struct bufarea cgblk;		/* cylinder group blocks */
struct bufarea *getdatablk();

#define	dirty(bp)	(bp)->b_dirty = 1
#define	initbarea(bp) \
	(bp)->b_dirty = 0; \
	(bp)->b_bno = (daddr_t)-1; \
	(bp)->b_flags = 0;

#define	sbdirty()	sblk.b_dirty = 1
#define	cgdirty()	cgblk.b_dirty = 1
#define	sblock		(*sblk.b_un.b_fs)
#define	cgrp		(*cgblk.b_un.b_cg)

#ifdef	AFS_OSF_ENV
/*
 * struct direct -> struct dirent
*/
#define	direct	dirent
#endif /* AFS_OSF_ENV */

enum fixstate { DONTKNOW, NOFIX, FIX };

struct inodesc {
    enum fixstate id_fix;	/* policy on fixing errors */
    int (*id_func) ();		/* function to be applied to blocks of inode */
    ino_t id_number;		/* inode number described */
    ino_t id_parent;		/* for DATA nodes, their parent */
    daddr_t id_blkno;		/* current block number being examined */
    int id_numfrags;		/* number of frags contained in block */
    OFF_T id_filesize;		/* for DATA nodes, the size of the directory */
    int id_loc;			/* for DATA nodes, current location in dir */
    int id_entryno;		/* for DATA nodes, current entry number */
    struct direct *id_dirp;	/* for DATA nodes, ptr to current entry */
    char *id_name;		/* for DATA nodes, name to find or enter */
    char id_type;		/* type of descriptor, DATA or ADDR */
};
/* file types */
#define	DATA	1
#define	ADDR	2

/*
 * Linked list of duplicate blocks.
 *
 * The list is composed of two parts. The first part of the
 * list (from duplist through the node pointed to by muldup)
 * contains a single copy of each duplicate block that has been
 * found. The second part of the list (from muldup to the end)
 * contains duplicate blocks that have been found more than once.
 * To check if a block has been found as a duplicate it is only
 * necessary to search from duplist through muldup. To find the
 * total number of times that a block has been found as a duplicate
 * the entire list must be searched for occurences of the block
 * in question. The following diagram shows a sample list where
 * w (found twice), x (found once), y (found three times), and z
 * (found once) are duplicate block numbers:
 *
 *    w -> y -> x -> z -> y -> w -> y
 *    ^		     ^
 *    |		     |
 * duplist	  muldup
 */
struct dups {
    struct dups *next;
    daddr_t dup;
};
struct dups *duplist;		/* head of dup list */
struct dups *muldup;		/* end of unique duplicate dup block numbers */

/*
 * Linked list of inodes with zero link counts.
 */
struct zlncnt {
    struct zlncnt *next;
    ino_t zlncnt;
};
struct zlncnt *zlnhead;		/* head of zero link count list */

char *devname;			/* name of device being checked */
long dev_bsize;			/* computed value of DEV_BSIZE */
long secsize;			/* actual disk sector size */
char nflag;			/* assume a no response */
char yflag;			/* assume a yes response */
int bflag;			/* location of alternate super block */
int qflag;			/* less verbose flag */
int debug;			/* output debugging info */
int cvtflag;			/* convert to old file system format */
char preen;			/* just fix normal inconsistencies */

char hotroot;			/* checking root device */
char havesb;			/* superblock has been read */
int fsmodified;			/* 1 => write done to file system */
int fsreadfd;			/* file descriptor for reading file system */
int fswritefd;			/* file descriptor for writing file system */

daddr_t maxfsblock;		/* number of blocks in the file system */
char *blockmap;			/* ptr to primary blk allocation map */
ino_t maxino;			/* number of inodes in file system */
ino_t lastino;			/* last inode in use */
char *statemap;			/* ptr to inode state table */
short *lncntp;			/* ptr to link count table */

char pathname[BUFSIZ];		/* current pathname */
char *pathp;			/* ptr to current position in pathname */
char *endpathname;		/* ptr to current end of pathname */

ino_t lfdir;			/* lost & found directory inode number */
char *lfname;			/* lost & found directory name */
int lfmode;			/* lost & found directory creation mode */

daddr_t n_blks;			/* number of blocks in use */
daddr_t n_files;		/* number of files in use */

#define	clearinode(dp)	(*(dp) = zino)
struct dinode zino;

/* only change i_gen if this is a VFS but not VICE fsck */
#ifdef VICE
#define zapino(x)	(*(x) = zino)
#else
#define zapino(x)	zino.di_gen = (x)->di_gen+1; (*(x) = zino)
#endif /* VICE */

int isconvert;			/* converting */

#ifdef VICE
int nViceFiles;			/* number of vice files seen */
#if	defined(AFS_SUN_ENV)
int iscorrupt;			/* known to be corrupt/inconsistent */
#endif
#ifdef	AFS_SUN_ENV
char fixstate;			/* is FsSTATE to be fixed */
char rebflg;			/* needs reboot if set */
int isdirty;			/* 1 => write pending to file system */
#endif /* AFS_SUN_ENV */
#ifdef	AFS_SUN5_ENV
#define	FSTYPE_MAX	8
char *fstype;
				/* remount okay if clear */
char fflag;			/* force fsck to check a mounted fs */
char mountedfs;			/* checking mounted device */
int oflag;
int mflag;
int exitstat;
int pflag;
int fsflag;
int rflag;			/* check raw file systems */
#include <sys/sysmacros.h>
FILE *logfile;			/* additional place for log message, for non-root file systems */
#else /* AFS_SUN5_ENV */
#ifdef	AFS_OSF_ENV
FILE *logfile;			/* additional place for log message, for non-root file systems */
char fflag;			/* force fsck to check a mounted fs */
#else /* AFS_OSF_ENV */
struct _iobuf *logfile;		/* additional place for log message, for non-root file systems */
#endif /* AFS_OSF_ENV */
#endif /* AFS_SUN5_ENV */
#endif /* VICE */

#define	setbmap(blkno)	setbit(blockmap, blkno)
#define	testbmap(blkno)	isset(blockmap, blkno)
#define	clrbmap(blkno)	clrbit(blockmap, blkno)

#define	STOP	0x01
#define	SKIP	0x02
#define	KEEPON	0x04
#define	ALTERED	0x08
#define	FOUND	0x10

#include <time.h>		/* for time() */
struct dinode *ginode();
struct bufarea *getblk();
ino_t allocino();
int findino();

/* global variables to be reset in new fork by "setup" */
struct bufarea *mlk_pbp;
ino_t mlk_startinum;

#ifdef AFS_SUN_ENV
int setup();
struct mntent *mntdup();
#endif

#if	defined(AFS_HPUX_ENV)
int pclean;
#endif

#ifdef	AFS_HPUX_ENV
char fflag;			/* force fsck to check a mounted fs */
int pclean;
int mflag;
#define	BLK	((dp->di_mode & IFMT) == IFBLK)
#define	CHR	((dp->di_mode & IFMT) == IFCHR)
#define	LNK	((dp->di_mode & IFMT) == IFLNK)
#ifdef IC_FASTLINK
#define FASTLNK (LNK && (dp->di_flags & IC_FASTLINK))
#else
#define FASTLNK (0)
#endif
#if defined(ACLS) && defined(AFS_HPUX_ENV)
daddr_t n_cont;			/* number of continuation inodes seen */
#define	CONT	((dp->di_mode & IFMT) == IFCONT)	/* continuation inode */
#define	SPECIAL	(BLK || CHR || CONT)
#else
#define	SPECIAL	(BLK || CHR)
#endif
#endif /* AFS_HPUX_ENV */

#if defined(AFS_HPUX110_ENV)
/* For backward compatibility */
#define cg_link         cg_unused[0]
#define cg_rlink        cg_unused[1]
#define fs_link         fs_unused[0]
#define fs_rlink        fs_unused[1]
#endif /* AFS_HPUX110_ENV */

#ifdef AFS_SUN59_ENV
/* diskaddr_t is longlong */
int bread(int fd, char *buf, diskaddr_t blk, long size);
int bwrite(int fd, char *buf,  diskaddr_t blk, long size);
#endif
