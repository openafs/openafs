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
#define NHASHENT 128	/* entries in the hash tbl */
#define MAXPAGES 128	/* max pages in a dir */
#define	BIGMAXPAGES 1023   /* new big max pages */
#define EPP 64		/* dir entries per page */
#define LEPP 6		/* log above */
/* When this next field changs, it is crucial to modify MakeDir, since the latter is responsible for marking these entries as allocated.  Also change the salvager. */
#define DHE 12		/* entries in a dir header above a pages header alone. */

#define FFIRST 1
#define FNEXT 2

struct MKFid
    {/* A file identifier. */
    afs_int32 vnode;	/* file's vnode slot */
    afs_int32 vunique;	/* the slot incarnation number */
    };

struct PageHeader
    {/* A page header entry. */
    unsigned short pgcount;	/* number of pages, or 0 if old-style */
    unsigned short tag;		/* 1234 in network byte order */
    char freecount;	/* unused, info in dirHeader structure */
    char freebitmap[EPP/8];
    char padding[32-(5+EPP/8)];
    };

struct DirHeader
    {/* A directory header object. */
    struct PageHeader header;
    char alloMap[MAXPAGES];    /* one byte per 2K page */
    unsigned short hashTable[NHASHENT];
    };

struct DirEntry
    {/* A directory entry */
    char flag;
    char length;	/* currently unused */
    unsigned short next;
    struct MKFid fid;
    char name[16];
    };

struct DirXEntry
    {/* A directory extension entry. */
    char name[32];
    };

struct DirPage0
    {/* A page in a directory. */
    struct DirHeader header;
    struct DirEntry entry[1];
    };

struct DirPage1
    {/* A page in a directory. */
    struct PageHeader header;
    struct DirEntry entry[1];
    };

/*
 * Note that this declaration is seen in both the kernel code and the
 * user space code.  One implementation is in afs/afs_buffer.c; the
 * other is in dir/buffer.c.
 */
extern int DVOffset(void *ap);



/* Prototypes */
extern int NameBlobs (char *name);
extern int Create (char *dir, char *entry, afs_int32 *vfid);
extern int Length (char *dir);
extern int Delete (char *dir, char *entry);
extern int FindBlobs (char *dir, int nblobs);
extern void AddPage (char *dir, int pageno);
extern void FreeBlobs(char *dir, register int firstblob, int nblobs);
extern int MakeDir (char *dir, afs_int32 *me, afs_int32 *parent);
extern int Lookup (char *dir, char *entry, register afs_int32 *fid);
extern int LookupOffset (char *dir, char *entry, register afs_int32 *fid, long *offsetp);
extern int EnumerateDir (char *dir, int (*hookproc)(void *dir, char *name, afs_int32 vnode, afs_int32 unique), void *hook);
extern int IsEmpty (char *dir);
extern struct DirEntry *GetBlob (char *dir, afs_int32 blobno);
extern int DirHash (register char *string);

#ifdef KERNEL
extern int afs_dir_NameBlobs (char *name);
extern int afs_dir_Create (char *dir, char *entry, afs_int32 *vfid);
extern int afs_dir_Length (char *dir);
extern int afs_dir_Delete (char *dir, char *entry);
extern int afs_dir_MakeDir (char *dir, afs_int32 *me, afs_int32 *parent);
extern int afs_dir_Lookup (char *dir, char *entry, register afs_int32 *fid);
extern int afs_dir_LookupOffset (char *dir, char *entry, register afs_int32 *fid, long *offsetp);
extern int afs_dir_EnumerateDir (char *dir, int (*hookproc)(void *dir, char *name, afs_int32 vnode, afs_int32 unique), void *hook);
extern int afs_dir_IsEmpty (char *dir);
extern struct DirEntry *afs_dir_GetBlob (char *dir, afs_int32 blobno);
#endif



#endif /*	!defined(__AFS_DIR_H) */
