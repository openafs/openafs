/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#if	!defined(__AFS_DIR_H)

#define __AFS_DIR_H

#define AFS_PAGESIZE 2048	/* bytes per page */
#define NHASHENT 128		/* entries in the hash tbl */
#define MAXPAGES 128		/* max pages in a dir */
#define	BIGMAXPAGES 1023	/* new big max pages */
#define EPP 64			/* dir entries per page */
#define LEPP 6			/* log above */
/* When this next field changs, it is crucial to modify MakeDir, since the
 * latter is responsible for marking these entries as allocated.  Also
 * change the salvager. */
#define DHE 12			/* entries in a dir header above a pages header alone. */

#define FFIRST 1
#define FNEXT 2

struct MKFid {			/* A file identifier. */
    afs_int32 vnode;		/* file's vnode slot */
    afs_int32 vunique;		/* the slot incarnation number */
};

struct PageHeader {
    /* A page header entry. */
    unsigned short pgcount;	/* number of pages, or 0 if old-style */
    unsigned short tag;		/* 1234 in network byte order */
    char freecount;		/* unused, info in dirHeader structure */
    char freebitmap[EPP / 8];
    char padding[32 - (5 + EPP / 8)];
};

struct DirHeader {
    /* A directory header object. */
    struct PageHeader header;
    char alloMap[MAXPAGES];	/* one byte per 2K page */
    unsigned short hashTable[NHASHENT];
};

struct DirEntry {
    /* A directory entry */
    char flag;
    char length;		/* currently unused */
    unsigned short next;
    struct MKFid fid;
    char name[16];
};

struct DirXEntry {
    /* A directory extension entry. */
    char name[32];
};

struct DirPage0 {
    /* A page in a directory. */
    struct DirHeader header;
    struct DirEntry entry[1];
};

struct DirPage1 {
    /* A page in a directory. */
    struct PageHeader header;
    struct DirEntry entry[1];
};

/*
 * Note that this declaration is seen in both the kernel code and the
 * user space code.  One implementation is in afs/afs_buffer.c; the
 * other is in dir/buffer.c.
 */
extern int DVOffset(void *ap);


/* This is private to buffer.c */
struct buffer;

/* Prototypes */
extern int NameBlobs(char *name);
extern int Create(void *dir, char *entry, void *vfid);
extern int Length(void *dir);
extern int Delete(void *dir, char *entry);
extern int FindBlobs(void *dir, int nblobs);
extern void AddPage(void *dir, int pageno);
extern void FreeBlobs(void *dir, int firstblob, int nblobs);
extern int MakeDir(void *dir, afs_int32 * me, afs_int32 * parent);
extern int Lookup(void *dir, char *entry, void *fid);
extern int LookupOffset(void *dir, char *entry, void *fid, long *offsetp);
extern int EnumerateDir(void *dir,
			int (*hookproc) (void *dir, char *name,
					 afs_int32 vnode, afs_int32 unique),
			void *hook);
extern int IsEmpty(void *dir);
extern struct DirEntry *GetBlob(void *dir, afs_int32 blobno);
extern int GetVerifiedBlob(void *dir, afs_int32 blobno, struct DirEntry **);
extern int DirHash(char *string);

extern int DStat(int *abuffers, int *acalls, int *aios);
extern void DRelease(void *loc, int flag);
extern int DVOffset(void *ap);
extern int DFlushVolume(afs_int32 vid);
extern int DFlushEntry(afs_int32 *fid);
extern int InverseLookup (void *dir, afs_uint32 vnode, afs_uint32 unique,
			  char *name, afs_uint32 length);

/* The kernel uses different versions of these, and prototypes them
   in afs_prototypes.h */
#ifndef KERNEL
extern int DInit(int abuffers);
extern void *DRead(afs_int32 *fid, int page);
extern int DFlush(void);
extern void *DNew(afs_int32 *fid, int page);
extern void DZap(afs_int32 *fid);

/* salvage.c */

extern int DirOK(void *);
extern int DirSalvage(void *, void *, afs_int32, afs_int32,
                      afs_int32, afs_int32);

#endif

#ifdef KERNEL
extern int afs_dir_NameBlobs(char *name);
extern int afs_dir_Create(void *dir, char *entry, void *vfid);
extern int afs_dir_Length(void *dir);
extern int afs_dir_Delete(void *dir, char *entry);
extern int afs_dir_MakeDir(void *dir, afs_int32 * me, afs_int32 * parent);
extern int afs_dir_Lookup(void *dir, char *entry, void *fid);
extern int afs_dir_LookupOffset(void *dir, char *entry, void *fid,
				long *offsetp);
extern int afs_dir_EnumerateDir(void *dir,
				int (*hookproc) (void *dir, char *name,
						 afs_int32 vnode,
						 afs_int32 unique),
				void *hook);
extern int afs_dir_IsEmpty(void *dir);
extern int afs_dir_ChangeFid(void *dir, char *entry, afs_uint32 *old_fid,
               		     afs_uint32 *new_fid);
extern struct DirEntry *afs_dir_GetBlob(void *dir, afs_int32 blobno);
extern int afs_dir_GetVerifiedBlob(void *, afs_int32, struct DirEntry **);
#endif

#endif /*       !defined(__AFS_DIR_H) */
