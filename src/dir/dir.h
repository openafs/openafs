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

struct DirBuffer {
    void *buffer;
    void *data;
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

/* Prototypes */
#ifdef KERNEL
struct dcache;
typedef struct dcache * dir_file_t;
#else
struct DirHandle;
typedef struct DirHandle * dir_file_t;
extern void Die(const char *msg) AFS_NORETURN;
#endif

extern int afs_dir_NameBlobs(char *name);
extern int afs_dir_Create(dir_file_t dir, char *entry, void *vfid);
extern int afs_dir_Length(dir_file_t dir);
extern int afs_dir_Delete(dir_file_t dir, char *entry);
extern int afs_dir_MakeDir(dir_file_t dir, afs_int32 * me,
			   afs_int32 * parent);
extern int afs_dir_Lookup(dir_file_t dir, char *entry, void *fid);
extern int afs_dir_LookupOffset(dir_file_t dir, char *entry, void *fid,
				long *offsetp);
extern int afs_dir_EnumerateDir(dir_file_t dir,
				int (*hookproc) (void *, char *name,
						 afs_int32 vnode, 
						 afs_int32 unique),
				void *hook);
extern int afs_dir_IsEmpty(dir_file_t dir);
extern int afs_dir_GetBlob(dir_file_t dir, afs_int32 blobno,
			   struct DirBuffer *);
extern int afs_dir_GetBlobWithErrno(dir_file_t dir, afs_int32 blobno,
			   struct DirBuffer *, int *physerr);
extern int afs_dir_GetVerifiedBlob(dir_file_t dir, afs_int32 blobno,
				   struct DirBuffer *);
extern int afs_dir_DirHash(char *string);

extern int afs_dir_InverseLookup (void *dir, afs_uint32 vnode,
				  afs_uint32 unique, char *name,
				  afs_uint32 length);

extern int afs_dir_ChangeFid(dir_file_t dir, char *entry,
		             afs_uint32 *old_fid, afs_uint32 *new_fid);

/* buffer operations */

extern void DInit(int abuffers);
extern int DRead(dir_file_t fid, int page, struct DirBuffer *);
extern int DReadWithErrno(dir_file_t fid, int page, struct DirBuffer *, int *physerr);
extern int DFlush(void);
extern int DFlushVolume(afs_int32);
extern int DNew(dir_file_t fid, int page, struct DirBuffer *);
extern void DZap(dir_file_t fid);
extern void DRelease(struct DirBuffer *loc, int flag);
extern int DStat(int *abuffers, int *acalls, int *aios);
extern int DFlushVolume(afs_int32 vid);
extern int DFlushEntry(dir_file_t fid);
extern int DVOffset(struct DirBuffer *);

/* salvage.c */

#ifndef KERNEL
extern int DirOK(void *);
extern int DirSalvage(void *, void *, afs_int32, afs_int32,
                      afs_int32, afs_int32);

#endif

#endif /*       !defined(__AFS_DIR_H) */
