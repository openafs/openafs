/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CM_DIR_ENV__
#define __CM_DIR_ENV__ 1

#define CM_DIR_PAGESIZE		2048		/* bytes per page */
#define CM_DIR_NHASHENT		256		/* entries in the hash tbl == NHSIZE */
#define CM_DIR_MAXPAGES		128		/* max pages in a dir */
#define CM_DIR_BIGMAXPAGES	1023		/* new big max pages */
#define CM_DIR_EPP		64		/* dir entries per page */
#define CM_DIR_LEPP		6		/* log above */
#define CM_DIR_CHUNKSIZE	32		/* bytes per dir entry chunk */

/* When this next field changs, it is crucial to modify MakeDir, since the latter is
 * responsible for marking these entries as allocated.  Also change
 * the salvager.
 */
#define CM_DIR_DHE		12		/* entries in a dir header above a pages
						 * header alone.
						 */

typedef struct cm_dirFid {
	/* A file identifier. */
	afs_int32 vnode;	/* file's vnode slot */
	afs_int32 unique;	/* the slot incarnation number */
} cm_dirFid_t;

typedef struct cm_pageHeader {
	/* A page header entry. */
	unsigned short pgcount;	/* number of pages, or 0 if old-style */
	unsigned short tag;		/* 1234 in network byte order */
	char freeCount;	/* unused, info in dirHeader structure */
	char freeBitmap[CM_DIR_EPP/8];
	char padding[32-(5+CM_DIR_EPP/8)];	/* pad to one 32-byte entry */
} cm_pageHeader_t;

/* a total of 13 32-byte entries, 1 for the header that in all pages, and
 * 12 more special ones for the entries in a the first page.
 */
typedef struct cm_dirHeader {
	/* A directory header object. */
	cm_pageHeader_t header;
	char alloMap[CM_DIR_MAXPAGES];    /* one byte per 2K page */
	unsigned short hashTable[CM_DIR_NHASHENT];
} cm_dirHeader_t;

/* this represents a directory entry.  We use strlen to find out how many bytes are
 * really in the dir entry; it is always a multiple of 32.
 */
typedef struct cm_dirEntry {
	/* A directory entry */
	char flag;
	char length;	/* currently unused */
	unsigned short next;
	cm_dirFid_t fid;
	char name[16];
} cm_dirEntry_t;

#ifdef UNUSED
typedef struct cm_dirXEntry {
	/* A directory extension entry. */
	char name[32];
} cm_dirXEntry_t;

typedef struct cm_dirPage0 {
	/* A page in a directory. */
	cm_dirHeader_t header;
	cm_dirEntry_t entry[1];
} cm_dirPage0_t;

typedef struct cm_dirPage1 {
	/* A page in a directory. */
	cm_pageHeader_t header;
	cm_dirEntry_t entry[1];
} cm_dirPage1_t;
#endif /* UNUSED */

extern int cm_NameEntries(char *namep, size_t *lenp);

#endif /*  __CM_DIR_ENV__ */
