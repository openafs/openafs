/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/auth.h>
#include <afs/bubasics.h>
#include <lock.h>
#include "budb.h"

typedef afs_uint32 dbadr;

struct hashTable {
    afs_int32 functionType;	/* type of hash function */
    afs_int32 threadOffset;	/* Byte offset of hash thread */
    afs_int32 entries;		/* number of entries in hash table */
    afs_int32 length;		/* number of hash table slots */
    dbadr table;		/* pointer to actual table */
    afs_int32 progress;		/* number empty buckets in oldTable */
    afs_int32 oldLength;	/* slots in old table */
    dbadr oldTable;		/* old table */
};

/* text blocks and related structures */

struct textBlock {
    afs_uint32 version;		/* for access consistency checks */
    afs_int32 size;		/* size in bytes */
    dbadr textAddr;		/* list of blocks */
    afs_int32 newsize;		/* replacement size */
    dbadr newTextAddr;		/* list of new blocks */
};

struct db_lockS {
    afs_int32 type;		/* user defined - for consistency checks */
    afs_int32 lockState;	/* locked/free */
    afs_int32 lockTime;		/* when locked */
    afs_int32 expires;		/* when timeout expires */
    afs_int32 instanceId;	/* user instance id */
    int lockHost;		/* locking host, if possible */
};

typedef struct db_lockS db_lockT;
typedef db_lockT *db_lockP;

/* hash table function types */
#define HT_dumpIden_FUNCTION 	1
#define HT_dumpName_FUNCTION	2
#define HT_tapeName_FUNCTION 	3
#define HT_volName_FUNCTION 	4

#define HT_MAX_FUNCTION 	4

/* block types */

#define free_BLOCK		0
#define volFragment_BLOCK 	1
#define volInfo_BLOCK 		2
#define tape_BLOCK 		3
#define dump_BLOCK 		4
#define MAX_STRUCTURE_BLOCK_TYPE 4
#define hashTable_BLOCK 	5
#define	text_BLOCK		6
#define NBLOCKTYPES 		7

/* This defines root pointers to all parts of the database.  It is allocated
   starting at location zero. */

#define BUDB_VERSION 1

struct dbHeader {
    afs_int32 version;		/* database version number */
    dumpId lastDumpId;		/* last dump id value */
    afs_uint32 lastTapeId;	/* last tape id allocated */
    afs_uint32 lastInstanceId;	/* last instance Id handed out */
    dbadr freePtrs[NBLOCKTYPES];	/* pointers to free blocks */
    dbadr eofPtr;		/* first free byte in db */
    afs_int32 nHTBuckets;	/* size of hashtable blocks */
    Date lastUpdate;		/* time of last change to database */
    afs_int32 ptrs[10];		/* spare pointers */
    struct hashTable volName;	/* hash tables */
    struct hashTable tapeName;
    struct hashTable dumpName;
    struct hashTable dumpIden;
    db_lockT textLocks[TB_MAX];	/* locks for text blocks */
    struct textBlock textBlock[TB_MAX];	/* raw text information */
    afs_int32 words[10];	/* spare words */
    afs_int32 checkVersion;	/* version# for consistency checking */
};

/* The database is composed of several types of structures of different sizes.
   To simplify recovery from crashes, allocation is done in fixed size blocks.
   All structures in a block are of the same type.  Each block has a header
   which identifies its type, a count of the number of free structures, and a
   pointer to the next block of the same type. */

#define BLOCKSIZE 2048
#define BlockBase(a)							\
	((((a) - sizeof(struct dbHeader)) & ~(BLOCKSIZE-1)) + 		\
	 sizeof(struct dbHeader))

struct blockHeader {
    char type;			/* type: implies structure size */
    char flags;			/* miscellaneous bits */
    short nFree;		/* number of free structures */
    dbadr next;			/* next free block same type */
};

#define	BLOCK_DATA_SIZE	(BLOCKSIZE-sizeof(struct blockHeader))

struct block {
    struct blockHeader h;
    char a[BLOCKSIZE - sizeof(struct blockHeader)];
};

#define NhtBucketS ((BLOCKSIZE-sizeof(struct blockHeader))/sizeof(dbadr))
struct htBlock {
    struct blockHeader h;
    dbadr bucket[NhtBucketS];
};

/* The database contains one volFragment for every occurence of volume data on
 * any tape. */

#define VOLFRAGMENTFLAGS 0xffff
struct volFragment {
    dbadr vol;			/* full volume info */
    dbadr sameNameChain;	/* next with same vol info */
    dbadr tape;			/* tape containing this fragment */
    dbadr sameTapeChain;	/* next fragment on tape */

    afs_int32 position;		/* on tape */
    Date clone;			/* clone date of volume */
    Date incTime;		/* time for incremental; 0 => full */
    afs_int32 startByte;	/* first byte of volume in this frag */
    afs_uint32 nBytes;		/* bytes in fragment */
    short flags;		/* miscellaneous bits */
    short sequence;		/* seq of frag in dumped volume */
};
#define NvolFragmentS 45
struct vfBlock {
    struct blockHeader h;
    struct {
	struct volFragment s;
	char pad[4];
    } a[NvolFragmentS];
};


/* This represents additional information about a file system volume that
 * changes relatively infrequently.  Its purpose is to minimize the size of the
 * folFragment structure. */

#define VOLINFOFLAGS 0xffff0000
struct volInfo {
    char name[BU_MAXNAMELEN];	/* name of volume: the hash key */
    afs_int32 flags;		/* miscellaneous bits */
    afs_int32 id;		/* read-write volume's id */
    char server[BU_MAXHOSTLEN];	/* file server */
    afs_int32 partition;	/* disk partition on server */
    afs_int32 nFrags;		/* number of fragments on list */

    dbadr nameHashChain;	/* for volume name hash table */
    dbadr sameNameHead;		/* volInfo of first struct this name */
    dbadr sameNameChain;	/* next volume with this name */
    dbadr firstFragment;	/* all volFragment with this volInfo */
};
#define NvolInfoS 20
struct viBlock {
    struct blockHeader h;
    struct {
	struct volInfo s;
	char pad[4];
    } a[NvolInfoS];
};


/* The tape structure represents information of every physical tape in the
 * backup system.  Some of the data may persist event though the tape's
 * contents are repeatedly overwritten. */

struct tape {
    char name[BU_MAXTAPELEN];	/* name of physical tape */
    afs_int32 flags;		/* miscellaneous bits */
    Date written;		/* tape writing started */
    Date expires;		/* expiration date */
    afs_uint32 nMBytes;		/* length of tape in Mbytes */
    afs_uint32 nBytes;		/* remainder of Mbytes  */
    afs_int32 nFiles;		/*  ditto for  EOFs */
    afs_int32 nVolumes;		/*  ditto for  volume fragments */
    afs_int32 seq;		/* sequence in tapeSet */

    afs_uint32 labelpos;	/* Position of the tape label */
    afs_int32 useCount;		/* # of times used */
    afs_int32 useKBytes;	/* How much of tape is used (in KBytes) */

    dbadr nameHashChain;	/* for tape name hash table */
    dbadr dump;			/* dump (tapeSet) this is part of */
    dbadr nextTape;		/* next tape in dump (tapeSet) */
    dbadr firstVol;		/* first volume fragment on tape */
};

#define NtapeS 20
struct tBlock {
    struct blockHeader h;
    struct {
	struct tape s;
	char pad[8];
    } a[NtapeS];
};

/* The structure describes every dump operation whose contents are still availe
 * on at least one tape.
 */

struct dump {
    /* similar to budb_dumpEntry */
    afs_int32 id;		/* unique identifier for dump */
    dumpId parent;		/* parent dump */
    afs_int32 level;		/* dump level */
    afs_int32 flags;		/* miscellaneous bits */
    char volumeSet[BU_MAXNAMELEN];	/* name of volume that was dumped */
    char dumpPath[BU_MAX_DUMP_PATH];	/* "path" name of dump level */
    char dumpName[BU_MAXNAMELEN];	/* dump name */
    Date created;		/* time dump initiated */
/*  Date  incTime;			target time for incremental dumps */
    afs_int32 nVolumes;		/* number of volumes in dump */
    struct budb_tapeSet tapes;	/* tapes for this dump */
    struct ktc_principal dumper;	/* user doing dump */
    afs_uint32 initialDumpID;	/* The dumpid of the initisl dump (for appended dumps) */
    dbadr appendedDumpChain;	/* Ptr to dump appended to this dump */

    dbadr idHashChain;		/* for dump id hash table */
    dbadr nameHashChain;	/* for dump name hash table */
    dbadr firstTape;		/* first tape in dump (tapeSet) */
};

typedef struct dump dbDumpT;
typedef dbDumpT *dbDumpP;

#define NdumpS 3
struct dBlock {
    struct blockHeader h;
    struct {
	struct dump s;
	char pad[44];
    } a[NdumpS];
};

/* This structure is used to cache the hash table blocks for a database hash
   table.  The htBlock structure is modified slightly to accomodate this
   mechanism.  On disk the next ptr links the consecutive blocks of the hash
   table together.  In memory this information is inferred */

struct memoryHTBlock {
    int valid;			/* block needs to be read in */
    dbadr a;			/* where stored in db */
    struct htBlock b;
};

struct memoryHashTable {
    struct hashTable *ht;	/* ptr to appropriate db.h hashtable */
    /* these are host byte order version of the corresponding HT fields */
    int threadOffset;		/* Byte offset of hash thread */
    int length;			/* number of hash table slots */
    int oldLength;		/* slots in old table */
    int progress;		/* number empty buckets in oldTable */
    /* these are the memory copies of the hash table buckets */
    int size;			/* allocated size of array */
    struct memoryHTBlock *(*blocks);	/* ptr to array of ht block pointers */
    int oldSize;		/*  ditto for old HT */
    struct memoryHTBlock *(*oldBlocks);
};

struct memoryDB {		/* in core copies of database structures */
    struct Lock lock;
    Date readTime;
    struct dbHeader h;
    struct memoryHashTable volName;
    struct memoryHashTable tapeName;
    struct memoryHashTable dumpName;
    struct memoryHashTable dumpIden;
    struct textBlock textBlock[TB_NUM];
};
extern struct memoryDB db;

#define set_header_word(ut,field,value) \
    dbwrite ((ut), ((char *)&(db.h.field) - (char *)&db.h), \
	     ((db.h.field = (value)), (char *)&(db.h.field)), \
	     sizeof(afs_int32))

#define set_word_offset(ut,a,b,offset,value) 			      \
    dbwrite ((ut), (a)+(offset),				      \
	     (*(afs_int32 *)((char *)(b) + (offset)) = (value),	      \
	      (char *)((char *)(b) + (offset))),		      \
	     sizeof(afs_int32))

#define set_word_addr(ut,a,b,addr,value) 			      \
    dbwrite ((ut), (a)+((char *)(addr) - (char *)(b)),		      \
	     (*(afs_int32 *)(addr) = (value),			      \
	      (char *)(addr)),					      \
	     sizeof(afs_int32))

#ifdef notdef
/* simple min/max macros */
#define MIN(x,y)        ((x) < (y) ? (x) : (y))
#define MAX(x,y)        ((x) > (y) ? (x) : (y))
#endif /* notdef */

struct memoryHashTable *ht_GetType();
extern afs_uint32 ht_HashEntry();
extern dbadr ht_LookupBucket();

extern afs_int32 dbwrite(struct ubik_trans *ut, afs_int32 pos, void *buff, afs_int32 len);
extern afs_int32 dbread(struct ubik_trans *ut, afs_int32 pos, void *buff, afs_int32 len);
extern afs_int32 cdbread(struct ubik_trans *ut, int type, afs_int32 pos, void *buff, afs_int32 len);
extern void db_panic(char *reason);
extern void ht_Reset(struct memoryHashTable *mht);
