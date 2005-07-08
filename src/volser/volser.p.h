/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _VOLSER_
#define _VOLSER_ 1

#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#include <pthread.h>
#endif

/* vflags, representing state of the volume */
#define	VTDeleteOnSalvage	1	/* delete on next salvage */
#define	VTOutOfService		2	/* never put this volume online */
#define	VTDeleted		4	/* deleted, don't do anything else */

/* iflags, representing "attach mode" for this volume at the start of this transaction */
#define	ITOffline	1	/* volume offline on server (returns VOFFLINE) */
#define	ITBusy		2	/* volume busy on server (returns VBUSY) */
#define	ITReadOnly	8	/* volume readonly on client, readwrite on server -DO NOT USE */
#define	ITCreate	0x10	/* volume does not exist correctly yet */
#define	ITCreateVolID	0x1000	/* create volid */

/* tflags, representing transaction state */
#define	TTDeleted	1	/* delete transaction not yet freed due  to high refCount */

/* other names for volumes in voldefs.h */
#define volser_RW	0
#define volser_RO	1
#define	volser_BACK	2

#define	THOLD(tt)	((tt)->refCount++)

struct volser_trans {
    struct volser_trans *next;	/* next ptr in active trans list */
    afs_int32 tid;		/* transaction id */
    afs_int32 time;		/* time transaction was last active (for timeouts) */
    afs_int32 creationTime;	/* time the transaction started */
    afs_int32 returnCode;	/* transaction error code */
    struct Volume *volume;	/* pointer to open volume */
    afs_int32 volid;		/* open volume's id */
    afs_int32 partition;	/* open volume's partition */
    afs_int32 dumpTransId;	/* other side's trans id during a dump */
    afs_int32 dumpSeq;		/* next sequence number to use during a dump */
    short refCount;		/* reference count on this structure */
    short iflags;		/* initial attach mode flags (IT*) */
    char vflags;		/* current volume status flags (VT*) */
    char tflags;		/* transaction flags (TT*) */
    char incremental;		/* do an incremental restore */
    /* the fields below are useful for debugging */
    char lastProcName[30];	/* name of the last procedure which used transaction */
    struct rx_call *rxCallPtr;	/* pointer to latest associated rx_call */

};

/* This is how often the garbage collection thread wakes up and 
 * checks for transactions that have timed out: BKGLoop()
 */
#define GCWAKEUP            30

struct volser_dest {
    afs_int32 destHost;
    afs_int32 destPort;
    afs_int32 destSSID;
};

#define	MAXHELPERS	    10
/* flags for vol helper busyFlags array.  First, VHIdle goes on when a server
 * becomes idle.  Next, idle flag is cleared and VHRequest goes on when
 * trans is queued.  Finally, VHRequest goes off (but VHIdle stays off) when
 * helper is done.  VHIdle goes on again when an lwp waits for work.
 */
#define	VHIdle		    1	/* vol helper is waiting for a request here */
#define	VHRequest	    2	/* a request has been queued here */
extern struct volser_trans *QI_GlobalWriteTrans;

/* the stuff below is from errors.h in vol directory */
#define VICE_SPECIAL_ERRORS	101	/* Lowest special error code */

#define VSALVAGE	101	/* Volume needs salvage */
#define VNOVNODE	102	/* Bad vnode number quoted */
#define VNOVOL		103	/* Volume not attached, doesn't exist, 
				 * not created or not online */
#define VVOLEXISTS	104	/* Volume already exists */
#define VNOSERVICE	105	/* Volume is not in service (i.e. it's
				 * is out of funds, is obsolete, or somesuch) */
#define VOFFLINE	106	/* Volume is off line, for the reason
				 * given in the offline message */
#define VONLINE		107	/* Volume is already on line */
#define VDISKFULL	108	/* Partition is "full", i.e. rougly within
				 * n% of full */
#define VOVERQUOTA	109	/* Volume max quota exceeded */
#define VBUSY		110	/* Volume temporarily unavailable; try again.
				 * The volume should be available again shortly; if
				 * it isn't something is wrong.  Not normally to be
				 * propagated to the application level */
#define VMOVED		111	/* Volume has moved to another server; do a VGetVolumeInfo
				 * to THIS server to find out where */

#define MyPort 5003
#define NameLen 80
#define VLDB_MAXSERVERS 10
#define VOLSERVICE_ID 4
#define INVALID_BID 0
#define VOLSER_MAXVOLNAME 65
#define VOLSER_OLDMAXVOLNAME 32
#define	VOLMAXPARTS	255

/*flags used for interfacing with the  backup system */
struct volDescription {		/*used for interfacing with the backup system */
    char volName[VOLSER_MAXVOLNAME];	/* should be VNAMESIZE as defined in volume.h */
    afs_int32 volId;
    int volSize;
    afs_int32 volFlags;
    afs_int32 volCloneId;
};

struct partList {		/*used by the backup system */
    afs_int32 partId[VOLMAXPARTS];
    afs_int32 partFlags[VOLMAXPARTS];
};

#define	STDERR	stderr
#define	STDOUT	stdout

#define ISNAMEVALID(name) (strlen(name) < (VOLSER_OLDMAXVOLNAME - 9))

/* values for flags in struct nvldbEntry */
#define RW_EXISTS 0x1000
#define RO_EXISTS 0x2000
#define BACK_EXISTS 0x4000

/* values for serverFlags in struct nvldbEntry */
#define NEW_REPSITE 0x01
#define ITSROVOL    0x02
#define ITSRWVOL    0x04
#define ITSBACKVOL  0x08
#define RO_DONTUSE  0x20

#define VLOP_RESTORE 0x100	/*this is bogus, clashes with VLOP_DUMP */
#define VLOP_ADDSITE 0x80	/*this is bogus, clashes with VLOP_DELETE */
#define PARTVALID 0x01
#define CLONEVALID 0x02
#define CLONEZAPPED 0x04
#define IDVALID 0x08
#define NAMEVALID 0x10
#define SIZEVALID 0x20
#define ENTRYVALID 0x40
#define REUSECLONEID 0x80
#define	VOK 0x02

/* Values for the UV_RestoreVolume flags parameter */
/* Also used for UV_CopyVolume and UV_CloneVolume */
#define RV_FULLRST	0x00001
#define RV_OFFLINE	0x00002
#define RV_CRDUMP	0x00010
#define RV_CRKEEP	0x00020
#define RV_CRNEW	0x00040
#define RV_LUDUMP	0x00100
#define RV_LUKEEP	0x00200
#define RV_LUNEW	0x00400
#define RV_RDONLY	0x10000
#define RV_CPINCR	0x20000
#define RV_NOVLDB	0x40000
#define RV_NOCLONE	0x80000

struct ubik_client;
extern afs_uint32 vsu_GetVolumeID(char *astring, struct ubik_client *acstruct, afs_int32 *errp);
extern int vsu_ExtractName(char rname[], char name[]);
extern afs_int32 vsu_ClientInit(int noAuthFlag, char *confDir, char *cellName, afs_int32 sauth, struct ubik_client **uclientp, int (*secproc)());
extern void vsu_SetCrypt(int cryptflag);

#endif /* _VOLSER_ */
