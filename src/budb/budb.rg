/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* The backup database interface. */
/* PA - these should not be defined here - import from bubasics */
#define BU_MAXNAMELEN    32		/* names of objects: volumes */
#define BU_MAXTAPELEN    32		/* names of objects: tapes */
#define BU_MAXHOSTLEN    32		/* names of server machines */
#define BU_MAXTOKENLEN   16		/* identifiers */
#define BU_MAXUNAMELEN  256		/* length of user name */
#define BU_MAXINAMELEN  128		/* length of user name */
#define BU_MAXCELLLEN   256		/* length of a cell name */
#define	BU_MAXPATHLEN	256		/* length of schedule path name */

/* then some type definitions */

%#ifndef NEVERDATE
%#define NEVERDATE 037777777777		/* a date that will never come */
%#endif
%#ifndef Date
%#define Date afs_uint32
%#endif
#define Date afs_uint32

/* first some constants */

/* Bumped from 1 to 2 for changing nFrags in budb_VolumeEntry to tapeSeq */
const BUDB_MAJORVERSION = 2;		/* version number of this interface */

/* then some type definitions */

%#ifndef dumpId
%#define dumpId Date
%#endif
#define dumpId Date

/* types of text blocks */
%#define	TB_DUMPSCHEDULE	0
%#define	TB_VOLUMESET	1
%#define	TB_TAPEHOSTS	2

%#define	TB_NUM		3	/* no. of block types */
%#define	TB_MAX		6	/* unused items are spares */

/* TB_NUM must be <= TB_MAX */

struct budb_principal { /* identifies a principal identity */
    char name[BU_MAXUNAMELEN];
    char instance[BU_MAXINAMELEN];
    char cell[BU_MAXCELLLEN];
};

struct budb_tapeSet { /* describes a tape sequence */
    afs_int32  id;				/* id of tapeSet, assigned by budb */
    char  tapeServer[BU_MAXHOSTLEN];	/* name of server where this tape is (used by ADSM buta) */
    char  format[BU_MAXTAPELEN];	/* for  printf to make tape name */
    /* Sequence numbers are assumed to be relatively small and relatively
     * densely packed in 0<=seq<maxTapes. */
    afs_int32  maxTapes;			/* maximum number of tapes in seq. */
    afs_int32  a; afs_int32 b;			/* linear transforms for tape */
};
  /* "function" that makes a tape name from a tapeSet and a sequence number.
     The resulting string must be less than BU_MAXTAPELEN. */
%#define budb_MakeTapeName(name,set,seq) sprintf (name, (set)->format, (set)->a*(seq) + (set)->b)

struct budb_dumpEntry { /* describes a dump */
    dumpId id;				/* identifier of this dump */
    dumpId parent;			/* parent dump */
    afs_int32  level;			/* level in multi-level incremental */
    afs_int32  flags;			/* various bits described below */
    char  volumeSetName[BU_MAXNAMELEN];	/* volume set */
    char  dumpPath[BU_MAXPATHLEN];	/* full path of dump sched. node */
    char  name[BU_MAXNAMELEN];		/* volset.dump */
    Date  created;			/* creation date of dump */
    Date  incTime;			/* time for incrementals, 0 => full */
    afs_int32 nVolumes;			/* number of vol fragments in dump */
    struct budb_tapeSet tapes;		/* tapes containing dump */
    struct budb_principal dumper;	/* name of person running doing dump */
    dumpId initialDumpID;		/* The dumpid of the initisl dump (for appended dumps) */
    dumpId appendedDumpID;		/* ID of the appended dump */
};
/* dump flag bit definitions */
%#define BUDB_DUMP_INCOMPLETE (1<<0)	/* some vols omitted due to errors */
%#define BUDB_DUMP_TAPEERROR  (1<<1)	/* tape error during dump */
%#define BUDB_DUMP_INPROGRESS (1<<2)
%#define BUDB_DUMP_ABORTED    (1<<3)	/* aborted: prob. dump unavailable */
%#define BUDB_DUMP_XBSA_NSS   (1<<8)    /* dump was done with a client    */
                                        /* that doesn't support switching */
                                        /* of servers                     */
%#define BUDB_DUMP_BUTA       (1<<11)	/* (used by ADSM buta) == 0x800 */
%#define BUDB_DUMP_ADSM	      (1<<12)	/* (used by XBSA/ADSM buta) == 0x1000 */


struct budb_tapeEntry { /* describes a tape */
    char  name[BU_MAXTAPELEN];		/* identifies tape for humans */
    afs_int32  flags;			/* various bits described below */
    Date  written;			/* date tape was last written */
    Date  expires;                      /* expiration date */
    afs_uint32  nMBytes;			/* number of Mbytes on tape */
    afs_uint32  nBytes;			/* Megabyte remainder */
    afs_int32  nFiles;			/* number of files on tape */
    afs_int32  nVolumes;			/* number of vol fragments on tape */
    afs_int32  seq;				/* sequence in tape set */

    afs_int32  labelpos;			/* position of tape label */
    afs_int32  useCount;			/* # of times used */
    afs_int32  useKBytes;                    /* How much of tape is used (in KBytes) */

    dumpId dump;			/* dump on tape (set) */
};
/* tape flag bit definitions */
%#define BUDB_TAPE_TAPEERROR    (1<<0)
%#define BUDB_TAPE_DELETED      (1<<1)
%#define BUDB_TAPE_BEINGWRITTEN (1<<2)	/* writing in progress */
%#define BUDB_TAPE_ABORTED      (1<<3)	/* aborted: tape probably garbaged */
%#define BUDB_TAPE_STAGED       (1<<4)	/* not yet on permanent media */
%#define BUDB_TAPE_WRITTEN      (1<<5)	/* tape writing finished: all OK */

/* notes:
 *    The following fields can be considered spares.
 *	incTime is not used.
 *	startByte also appears to be effectively unused.
 */

struct budb_volumeEntry { /* describes a fragment of a volume */

    /* volume information */
    char  name[BU_MAXNAMELEN];		/* unique name of volume */
    afs_int32  flags;			/* various bits described below */
    afs_int32  id;				/* volume uid */
    char  server[BU_MAXHOSTLEN];	/* server supporting volume */
    afs_int32  partition;			/* partition on server */
    afs_int32  tapeSeq;			/* Tape sequence number in this dump-set */

    /* per fragment */
    afs_int32  position;			/* position on tape */
    Date  clone;			/* time volume was cloned for dump */
    Date  incTime;			/* NOT USED */
    afs_int32  startByte;			/* first byte of volume in this frag */
    afs_uint32  nBytes;		/* number of bytes in this frag */
    afs_int32   seq;				/* sequence of frag in volume */

    /* additional location info */
    dumpId dump;			/* dump volume is part of */
    char  tape[BU_MAXTAPELEN];		/* tape containing this fragment */
};

/* volume flag bit definitions */
%#define BUDB_VOL_TAPEERROR    (1<<0)	/* tape problem during dump */
%#define BUDB_VOL_FILEERROR    (1<<1)	/* voldump aborted during dump */
%#define BUDB_VOL_BEINGWRITTEN (1<<2)
%#define BUDB_VOL_FIRSTFRAG    (1<<3)	/* same as low bits of tape position */
%#define BUDB_VOL_LASTFRAG     (1<<4)
%#define BUDB_VOL_ABORTED      (1<<5)	/* aborted: vol probably undumped */

/* procedure interface */

package BUDB_
prefix S
statindex 17

/* All these procedures take a connection parameter since they may contact the
   database via RPC. */

/* To facilitate returning large large amounts of data some of these procedures
   expect to receive a pointer to an array of structures.  The majorVersion
   number implicitly specifies the size of each array element.  The progress
   parameter is set to the number of elements actually returned.  Since the
   caller may have limited buffer space, provisions are made to get the data in
   a series of calls.  The index parameter specifies starting point of a
   continued operation: for the first call it will be zero, a negative number
   will produce an error.  If more elements are available on a subsequent call
   nextIndex is set to the index of the next element.  Otherwise nextIndex is
   set to a negative number. */

/* The flag bits specify which entries are being requested.  They are search
   operations that use name, start, and end to select a subset of entries to be
   returned.  Not all combinations are meaning full or supported. */

/* These are NOT flags. These are values. PA */

%#define BUDB_OP_NAMES	    (0x7)
%#define BUDB_OP_STARTS	    (0x7<<3)
%#define BUDB_OP_ENDS	    (0x7<<6)
%#define BUDB_OP_TIMES	    (0x3<<9)
%#define BUDB_OP_MISC	    (0xff<<16)

/* defining the meaning of "name" */
%#define BUDB_OP_DUMPNAME   (1<<0)
%#define BUDB_OP_VOLUMENAME (2<<0)
%#define BUDB_OP_TAPENAME   (3<<0)
%#define BUDB_OP_TAPESEQ    (4<<0)

/* "start" is a time value */
%#define BUDB_OP_STARTTIME  (1<<3)
  /* "end" delimits a range of times */
%#define BUDB_OP_RANGE      (1<<6)
  /* "end" specifies number of earlier entries */
%#define BUDB_OP_NPREVIOUS  (2<<6)
  /* "end" specifies number of later entries */
%#define BUDB_OP_NFOLLOWING (3<<6)
/* start is dump id (name may be null), return all entries */
%#define BUDB_OP_DUMPID     (2<<3)

/* defining the which type of time values */
%#define BUDB_OP_CLONETIME  (1<<9)	/* use clone time */
%#define BUDB_OP_DUMPTIME   (2<<9)	/* use dump time (create?) */
%#define BUDB_OP_INCTIME    (3<<9)	/* use inc time */

/* Miscellaneous bits: */
  /* for volumes: return only first fragment */
%#define BUDB_OP_FIRSTFRAG  (1<<16)

/* maximum number of elements returnable by these functions */
const BUDB_MAX_RETURN_LIST = 1000;

typedef struct budb_volumeEntry budb_volumeList<BUDB_MAX_RETURN_LIST>;
typedef struct budb_dumpEntry budb_dumpList<BUDB_MAX_RETURN_LIST>;
typedef struct budb_tapeEntry budb_tapeList<BUDB_MAX_RETURN_LIST>;
typedef afs_int32                  budb_dumpsList<BUDB_MAX_RETURN_LIST>;
typedef char charListT<>;

%#define BUDB_TEXT_COMPLETE	1

/* structures for database dump generation and interpretation */

/* structure types */
%#define	SD_DBHEADER		1
%#define	SD_DUMP			2
%#define	SD_TAPE			3
%#define	SD_VOLUME		4
%#define	SD_TEXT_DUMPSCHEDULE	5
%#define	SD_TEXT_VOLUMESET	6
%#define	SD_TEXT_TAPEHOSTS	7
%#define	SD_END			8

/* ListDumps in flags */
%#define	BUDB_OP_DATES		(0x01)
%#define	BUDB_OP_GROUPID		(0x02)
/* ListDumps out flags */
%#define	BUDB_OP_APPDUMP		(0x01)
%#define	BUDB_OP_DBDUMP		(0x02)

/* database header - minimal version that is dumped. Allows values of important
 *	state variables to be saved/restored.
 */

struct DbHeader
{
    afs_int32 dbversion;			/* database version */
    afs_int32 created;			/* creation time */
    char cell[BU_MAXCELLLEN];		/* whose database */
    dumpId lastDumpId;			/* last dump id generated */
    afs_uint32 lastInstanceId;		/* last lock instance */
    afs_uint32 lastTapeId;			/* last tape id */
};

/* header prefix for each structure. Identifies the structure following */
struct structDumpHeader
{
     afs_int32 type;			/* structure type */
     afs_int32 structversion;	/* version of following structure */
     afs_int32 size;    		/* size in bytes of following structure */
};

/* General Interface routines - alphabetic order */

/* This adds a volume to particular dump and tape.  It is called after the
   volume has been written to tape and allows the database to attach the volume
   information to the structures for its containing dump and tape.  The
   description of the volume must be specified on input, including the vldb
   information, incTime, and a description of the volume's fragmention. */

AddVolume
 (INOUT struct budb_volumeEntry *vol);

/* This creates a new dump.  On input the dumpEntry specifies the containing
   tape set, the dump name, the incTime, and the identity of the dumper.  On
   output the dump's id is set. */

CreateDump
 (INOUT struct budb_dumpEntry *dump);

DeleteDump (IN dumpId id,               /* the dump ids to delete */
            IN Date   fromTime,         /* time to delete dump from */
            IN Date   toTime,           /* time to delete dumps to */
	    INOUT budb_dumpsList *dumps);/* List of dumps deleted */

/* This is called, probably infrequently, to remove a tape from the database.
   The assumption is that sometimes tapes are retired or lost and this routine
   facilitates cleaning up the database. */

DeleteTape
 (IN struct budb_tapeEntry *tape);	/* tape info */

DeleteVDP
 ( IN string dsname<BU_MAXNAMELEN>,	/* dump name */
   IN string dumpPath<BU_MAXPATHLEN>,	/* dump node path name */
   IN afs_int32 curDumpId);			/* current dump Id for exclusion */

FindClone
 ( IN afs_int32 dumpID,			/* id of dump to start with */
   IN string volName<BU_MAXNAMELEN>,	/* clone time required for volName */
INOUT afs_int32  *clonetime);		/* returned clone time */

FindDump
 ( IN string volumeName<BU_MAXNAMELEN>,	/* name of volume to look for */
   IN afs_int32 beforeDate,			/* must be before this date */
INOUT struct budb_dumpEntry *deptr);	/* returned dump information */

FindLatestDump
 (IN  string vsname<BU_MAXNAMELEN>,   /* name of volumeset to look for */
  IN  string dname<BU_MAXPATHLEN>,    /* name of dump to look for */
  OUT budb_dumpEntry *dumpentry);

MakeDumpAppended
 (IN  afs_int32 appendedDumpID,            /* Append this dump to .... */
  IN  afs_int32 intialDumpID,              /* ... this initial dump */
  IN  afs_int32 startTapeSeq);             /* Tape sequence the dump starts at */

FindLastTape
 (IN  afs_int32 dumpID,                        /* id of dump to */
  OUT struct budb_dumpEntry   *dumpEntry, /* Dump   information to return */
  OUT struct budb_tapeEntry   *tapeEntry, /* Tape   information to return */
  OUT struct budb_volumeEntry *volEntry); /* Volume information to return */

/* This notifies the database that the dump is finished.  Some status bits can
   be specified on input. */

FinishDump
 (INOUT struct budb_dumpEntry *dump);

/* This is called when writing to the tape has been completed.  The tapeEntry
   includes some status bits on input, such as whether any tape errors were
   encountered.  Volumes and dumps on the tape are marked as safe if the status
   was good. */

FinishTape
 (INOUT struct budb_tapeEntry *tape);

GetDumps
 (IN  afs_int32  majorVersion,		/* version of interface structures */
  IN  afs_int32  flags,			/* search & select controls */
  IN  string name<BU_MAXNAMELEN>,	/* s&s parameters */
  IN  afs_int32  start,
  IN  afs_int32  end,
  IN  afs_int32  index,			/* start index of returned entries */
  OUT afs_int32 *nextIndex,			/* output index for next call */
  OUT afs_int32 *dbUpdate,			/* time of last db change */
  INOUT budb_dumpList *dumps);		/* structure list */

GetTapes
 (IN  afs_int32  majorVersion,		/* version of interface structures */
  IN  afs_int32  flags,			/* search & select controls */
  IN  string name<BU_MAXNAMELEN>,	/* s&s parameters */
  IN  afs_int32  start,
  IN  afs_int32  end,			/* reserved: MBZ */
  IN  afs_int32  index,			/* start index of returned entries */
  OUT afs_int32 *nextIndex,			/* output index for next call */
  OUT afs_int32 *dbUpdate,			/* time of last db change */
  INOUT struct budb_tapeList *tapes);	/* structure list */

GetVolumes
 (IN  afs_int32  majorVersion,		/* version of interface structures */
  IN  afs_int32  flags,			/* search & select controls */
  IN  string name<BU_MAXNAMELEN>,	/*  - parameters for search */
  IN  afs_int32  start,			/*  - usage depends which BUDP_OP_* */
  IN  afs_int32  end,			/*  - bits are set */
  IN  afs_int32  index,			/* start index of returned entries */
  OUT afs_int32 *nextIndex,			/* output index for next call */
  OUT afs_int32 *dbUpdate,			/* time of last db change */
  INOUT budb_volumeList *volumes);	/* structure list */

/* Called when a tape is about to be used.  It deletes from the database the
   previous contents of the tape, if any, and marks it as "being written".  The
   tapeEntry identifies the tape name and dump on input.  The updated entry is
   returned on output. */

UseTape
 (INOUT struct budb_tapeEntry *tape,	/* tape info */
  OUT afs_int32 *new);			/* set if tape is new */

/* text file management calls - alphabetic */

GetText
 (  IN  afs_uint32 lockHandle,
    IN  afs_int32 textType,			/* which type of text */
    IN  afs_int32 maxLength,
    IN 	afs_int32 offset,
    OUT afs_int32 *nextOffset,
    OUT charListT *charListPtr);

GetTextVersion
 (  IN afs_int32 textType,
   OUT afs_uint32 *tversion);

SaveText
 (  IN  afs_uint32 lockHandle,		/* which type of text */
    IN  afs_int32 textType,			/* which type of text */
    IN	afs_int32 offset,			/* offset into text block */
    IN  afs_int32 flags,
    IN  charListT *charListPtr);		/* text */

/* Lock management interface routines */

FreeAllLocks
 (  IN afs_uint32 instanceId );		/* identifies user */

FreeLock
 (  IN afs_uint32 lockHandle);		/* identifies lock */

GetInstanceId
 (  OUT afs_uint32 *instanceId );	/* instance of a user */

GetLock
 (  IN	afs_uint32 instanceId,		/* instance of user */
    IN	afs_int32 lockName,			/* which lock */
    IN	afs_int32 expiration,		/* # secs after which lock released */
    OUT afs_uint32 *lockHandle);		/* returned lock handle */

/* database dump and reconstruction */

DbVerify
 (  OUT	afs_int32 *status,			/* 0=ok, 1=inconsistent */
    OUT	afs_int32 *orphans,			/* orphan block count */
    OUT afs_int32 *host			/* host where checks done */
 );

DumpDB
 (  IN  int  firstcall,                 /* First call requesting data */
    IN	afs_int32 maxLength,			/* max transfer size */
    OUT	charListT *charListPtr,		/* byte stream out */
    OUT	afs_int32	*flags );		/* status flags */

RestoreDbHeader
 (  IN	struct DbHeader *header );	/* restore database header */

/* Debug and test interface routines
 *
 * These routines provide a low level interface that can be used to test out
 * the backup database.
 */

T_GetVersion(OUT afs_int32 *majorVersion);

T_DumpHashTable (IN afs_int32 type, IN string filename<32>);

T_DumpDatabase (IN string filename<32>);

/* This adds a list of volumes to particular dump and tape.  It is
 * called after the volume has been written to tape and allows the
 * database to attach the volume information to the structures for its
 * containing dump and tape.  The description of the volume must be
 * specified on input, including the vldb information, incTime, and a
 * description of the volume's fragmention.
 */
AddVolumes
 (IN struct budb_volumeList *vols);

ListDumps (IN afs_int32 sflags,           /* search flags */
	   IN string    name<BU_MAXPATHLEN>, /* unused (reserved) */
	   IN afs_int32 group,	     	  /* group Id */
           IN Date      fromTime,         /* time to delete dump from */
           IN Date      toTime,           /* time to delete dumps to */
	   INOUT budb_dumpsList *dumps,   /* List of dumps */
	   INOUT budb_dumpsList *flags);  /* parallel flag for each dump */
