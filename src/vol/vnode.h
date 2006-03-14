/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		vnode.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#define Date afs_uint32

struct Volume;			/* Potentially forward definition. */

typedef struct ViceLock {
    int lockCount;
    int lockTime;
} ViceLock;

#define ViceLockCheckLocked(vptr) ((vptr)->lockTime == 0)
#define ViceLockClear(vptr) ((vptr)->lockCount = (vptr)->lockTime = 0)

#define ROOTVNODE 1

/*typedef enum {vNull=0, vFile=1, vDirectory=2, vSymlink=3} VnodeType;*/
typedef unsigned int VnodeType;
#define vNull 0
#define vFile 1
#define vDirectory 2
#define vSymlink 3

/*typedef enum {vLarge=0,vSmall=1} VnodeClass;*/
#define vLarge	0
#define vSmall	1
typedef int VnodeClass;
#define VNODECLASSWIDTH 1
#define VNODECLASSMASK	((1<<VNODECLASSWIDTH)-1)
#define nVNODECLASSES	(VNODECLASSMASK+1)

struct VnodeClassInfo {
    struct Vnode *lruHead;	/* Head of list of vnodes of this class */
    int diskSize;		/* size of vnode disk object, power of 2 */
    int logSize;		/* log 2 diskSize */
    int residentSize;		/* resident size of vnode */
    int cacheSize;		/* Vnode cache size */
    bit32 magic;		/* Magic number for this type of vnode,
				 * for as long as we're using vnode magic
				 * numbers */
    int allocs;			/* Total number of successful allocation
				 * requests; this is the same as the number
				 * of sanity checks on the vnode index */
    int gets, reads;		/* Number of VGetVnodes and corresponding
				 * reads */
    int writes;			/* Number of vnode writes */
};

extern struct VnodeClassInfo VnodeClassInfo[nVNODECLASSES];

#define vnodeTypeToClass(type)  ((type) == vDirectory? vLarge: vSmall)
#define vnodeIdToClass(vnodeId) ((vnodeId-1)&VNODECLASSMASK)
#define vnodeIdToBitNumber(v) (((v)-1)>>VNODECLASSWIDTH)
/* The following calculation allows for a header record at the beginning
   of the index.  The header record is the same size as a vnode */
#define vnodeIndexOffset(vcp,vnodeNumber) \
    ((vnodeIdToBitNumber(vnodeNumber)+1)<<(vcp)->logSize)
#define bitNumberToVnodeNumber(b,class) ((VnodeId)(((b)<<VNODECLASSWIDTH)+(class)+1))
#define vnodeIsDirectory(vnodeNumber) (vnodeIdToClass(vnodeNumber) == vLarge)

typedef struct VnodeDiskObject {
    unsigned int type:3;	/* Vnode is file, directory, symbolic link
				 * or not allocated */
    unsigned int cloned:1;	/* This vnode was cloned--therefore the inode
				 * is copy-on-write; only set for directories */
    unsigned int modeBits:12;	/* Unix mode bits */
    signed int linkCount:16;	/* Number of directory references to vnode
				 * (from single directory only!) */
    bit32 length;		/* Number of bytes in this file */
    Unique uniquifier;		/* Uniquifier for the vnode; assigned
				 * from the volume uniquifier (actually
				 * from nextVnodeUnique in the Volume
				 * structure) */
    FileVersion dataVersion;	/* version number of the data */
    afs_int32 vn_ino_lo;	/* inode number of the data attached to
				 * this vnode - entire ino for standard */
    Date unixModifyTime;	/* set by user */
    UserId author;		/* Userid of the last user storing the file */
    UserId owner;		/* Userid of the user who created the file */
    VnodeId parent;		/* Parent directory vnode */
    bit32 vnodeMagic;		/* Magic number--mainly for file server
				 * paranoia checks */
#   define	  SMALLVNODEMAGIC	0xda8c041F
#   define	  LARGEVNODEMAGIC	0xad8765fe
    /* Vnode magic can be removed, someday, if we run need the room.  Simply
     * have to be sure that the thing we replace can be VNODEMAGIC, rather
     * than 0 (in an old file system).  Or go through and zero the fields,
     * when we notice a version change (the index version number) */
    ViceLock lock;		/* Advisory lock */
    Date serverModifyTime;	/* Used only by the server; for incremental
				 * backup purposes */
    afs_int32 group;		/* unix group */
    afs_int32 vn_ino_hi;	/* high part of 64 bit inode. */
    bit32 reserved6;
    /* Missing:
     * archiving/migration
     * encryption key
     */
} VnodeDiskObject;

#define SIZEOF_SMALLDISKVNODE	64
#define CHECKSIZE_SMALLVNODE\
	(sizeof(VnodeDiskObject) == SIZEOF_SMALLDISKVNODE)
#define SIZEOF_LARGEDISKVNODE	256

typedef struct Vnode {
    struct Vnode *hashNext;	/* Next vnode on hash conflict chain */
    struct Vnode *lruNext;	/* Less recently used vnode than this one */
    struct Vnode *lruPrev;	/* More recently used vnode than this one */
    /* The lruNext, lruPrev fields are not
     * meaningful if the vnode is in use */
    bit16 hashIndex;		/* Hash table index */
#ifdef	AFS_AIX_ENV
    unsigned changed_newTime:1;	/* 1 if vnode changed, write time */
    unsigned changed_oldTime:1;	/* 1 changed, don't update time. */
    unsigned delete:1;		/* 1 if the vnode should be deleted; in
				 * this case, changed must also be 1 */
#else
    byte changed_newTime:1;	/* 1 if vnode changed, write time */
    byte changed_oldTime:1;	/* 1 changed, don't update time. */
    byte delete:1;		/* 1 if the vnode should be deleted; in
				 * this case, changed must also be 1 */
#endif
    VnodeId vnodeNumber;
    struct Volume
     *volumePtr;		/* Pointer to the volume containing this file */
    bit32 nUsers;		/* Number of lwp's who have done a VGetVnode */
    bit32 cacheCheck;		/* Must equal the value in the volume Header
				 * for the cache entry to be valid */
    struct Lock lock;		/* Internal lock */
#ifdef AFS_PTHREAD_ENV
    pthread_t writer;		/* thread holding write lock */
#else				/* AFS_PTHREAD_ENV */
    PROCESS writer;		/* Process id having write lock */
#endif				/* AFS_PTHREAD_ENV */
    IHandle_t *handle;
    VnodeDiskObject disk;	/* The actual disk data for the vnode */
} Vnode;

#define SIZEOF_LARGEVNODE \
	(sizeof(struct Vnode) - sizeof(VnodeDiskObject) + SIZEOF_LARGEDISKVNODE)
#define SIZEOF_SMALLVNODE	(sizeof (struct Vnode))

#ifdef AFS_LARGEFILE_ENV
#define VN_GET_LEN(N, V) FillInt64(N, (V)->disk.reserved6, (V)->disk.length)
#define VNDISK_GET_LEN(N, V) FillInt64(N, (V)->reserved6, (V)->length)
#define VN_SET_LEN(V, N) SplitInt64(N, (V)->disk.reserved6, (V)->disk.length)
#define VNDISK_SET_LEN(V, N) SplitInt64(N, (V)->reserved6, (V)->length)
#else /* !AFS_LARGEFILE_ENV */
#define VN_GET_LEN(N, V) (N) = (V)->disk.length;
#define VNDISK_GET_LEN(N, V) (N) = (V)->length;
#define VN_SET_LEN(V, N) (V)->disk.length = (N);
#define VNDISK_SET_LEN(V, N) (V)->length = (N);
#endif /* !AFS_LARGEFILE_ENV */

#ifdef AFS_64BIT_IOPS_ENV
#define VN_GET_INO(V) ((Inode)((V)->disk.vn_ino_lo | \
			       ((V)->disk.vn_ino_hi ? \
				(((Inode)(V)->disk.vn_ino_hi)<<32) : 0)))

#define VN_SET_INO(V, I) ((V)->disk.vn_ino_lo = (int)((I)&0xffffffff), \
			   ((V)->disk.vn_ino_hi = (I) ? \
			    (int)(((I)>>32)&0xffffffff) : 0))

#define VNDISK_GET_INO(V) ((Inode)((V)->vn_ino_lo | \
				   ((V)->vn_ino_hi ? \
				    (((Inode)(V)->vn_ino_hi)<<32) : 0)))

#define VNDISK_SET_INO(V, I) ((V)->vn_ino_lo = (int)(I&0xffffffff), \
			      ((V)->vn_ino_hi = (I) ? \
			       (int)(((I)>>32)&0xffffffff) : 0))
#else
#define VN_GET_INO(V) ((V)->disk.vn_ino_lo)
#define VN_SET_INO(V, I) ((V)->disk.vn_ino_lo = (I))
#define VNDISK_GET_INO(V) ((V)->vn_ino_lo)
#define VNDISK_SET_INO(V, I) ((V)->vn_ino_lo = (I))
#endif

#define VVnodeDiskACL(v)     /* Only call this with large (dir) vnode!! */ \
	((AL_AccessList *) (((byte *)(v))+SIZEOF_SMALLDISKVNODE))
#define  VVnodeACL(vnp) (VVnodeDiskACL(&(vnp)->disk))
/* VAclSize is defined this way to allow information in the vnode header
   to grow, in a POSSIBLY upward compatible manner.  SIZEOF_SMALLDISKVNODE
   is the maximum size of the basic vnode.  The vnode header of either type
   can actually grow to this size without conflicting with the ACL on larger
   vnodes */
#define VAclSize(vnp)		(SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE)
#define VAclDiskSize(v)		(SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE)
/*extern int VolumeHashOffset(); */
extern int VolumeHashOffset_r(void);
extern int VInitVnodes(VnodeClass class, int nVnodes);
/*extern VInitVnodes_r();*/
extern Vnode *VGetVnode(Error * ec, struct Volume *vp, VnodeId vnodeNumber,
			int locktype);
extern Vnode *VGetVnode_r(Error * ec, struct Volume *vp, VnodeId vnodeNumber,
			  int locktype);
extern void VPutVnode(Error * ec, register Vnode * vnp);
extern void VPutVnode_r(Error * ec, register Vnode * vnp);
extern int VVnodeWriteToRead(Error * ec, register Vnode * vnp);
extern int VVnodeWriteToRead_r(Error * ec, register Vnode * vnp);
extern Vnode *VAllocVnode(Error * ec, struct Volume *vp, VnodeType type);
extern Vnode *VAllocVnode_r(Error * ec, struct Volume *vp, VnodeType type);
/*extern VFreeVnode();*/
extern Vnode *VGetFreeVnode_r(struct VnodeClassInfo *vcp);
