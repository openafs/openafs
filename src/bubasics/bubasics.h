/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __BUBASIC__
#define __BUBASIC__

/* version numbers. This version information is accessed from more than
 * one module. Version information is kept for:
 *	bucoord <-> butc interface
 *	butc <-> butm interface
 *	bu* <-> budb interface
 *
 * and as an interim measure, for
 *	dumpschedule files
 *	dump database files i.e. D*.db
 */

/* Version of butc <-> butm interface
 * These are not version flags as the name suggests.
 */

#define BUTM_VERSIONFLAG_0 0	/* every size measure is in bytes */
#define BUTM_VERSIONFLAG_1 1	/* every size measure is in kbytes */
#define BUTM_VERSIONFLAG_2 2	/* afs 3.1 features */

/* current version of the interface */
#define BUTM_MAJORVERSION BUTM_VERSIONFLAG_2

/* versions for the bucoord <-> butc interface */
#define BUTC_VERSION_1		1	/* initial version, afs 3.1 */
#define BUTC_VERSION_2          2	/* 3.4 version - added voltype to tc_dumpDesc */
#define BUTC_VERSION_3          3	/* 3.4 version - labeltape change */
#define	CUR_BUTC_VERSION	BUTC_VERSION_3

/* TAPE_VERSION Version of the current tape format used by butc
 * Notes:
 *
 * version 0:   ?? - sizes in bytes
 * version 1:   Sizes in kbytes.
 * version 2:
 *      1) Tape expiration information in label
 *      2) backup systems's EOT markers at the end of a dump
 * version 3:
 *	1) dump id and tape use count in label
 * version 4:
 *	1) Got rid of large EOT markers
 *      2) Supports appended dumps
 */

#define TAPE_VERSION_0          0
#define TAPE_VERSION_1          1
#define TAPE_VERSION_2          2
#define	TAPE_VERSION_3		3
#define	TAPE_VERSION_4		4
#define CUR_TAPE_VERSION        TAPE_VERSION_4

#define BC_SCHEDULE_MAGIC       0x74327285	/* magic # for schedules */
#define BC_SCHEDULE_V3_1        1	/* afs 3.1 */
#define BC_SCHEDULE_VERSION     BC_SCHEDULE_V3_1	/* current version */

#define BC_DUMPDB_MAGIC         0x10381645	/* magic # for dump db */
#define BC_DUMPDB_V3_1          1	/* afs 3.1 */
#define BC_DUMPDB_VERSION       BC_DUMPDB_V3_1	/* current version */

/* define ports that move to system configuration later */
#define	BC_MESSAGEPORT	    7020	/* for communicating with backup coordinator */
#define	BC_TAPEPORT	    7025	/* for communicating with tape controller */

#ifndef AFSCONF_BUDBPORT
#define AFSCONF_BUDBPORT    7021	/* for communicating with backup database */
#endif
#define BUDB_SERVICE 22314	/* service id */

#define RX_SCINDEX_NULL	0	/* No security */
#define RX_SCINDEX_VAB 	1	/* vice tokens, with bcrypt */
#define RX_SCINDEX_KAD	2	/* Kerberos/DES */
#define RX_SCINDEX_K5	5	/* Kerberos5 */

/* maximums for various text strings 
 * DON'T alter these values until all disk/tape structures can be handled
 * correctly. In particular, volume names are 64.
 */

/* dump states */
#define	DUMP_FAILED	1	/* outright failed */
#define	DUMP_PARTIAL	2	/* partial on tape */
#define	DUMP_SUCCESS	3	/* all ok */
#define DUMP_RETRY      4	/* failed, but retry */
#define DUMP_NORETRYEOT 5	/* failed, and don't retry */
#define DUMP_NOTHING    6	/* Nothing was dumped */
#define DUMP_NODUMP     7	/* don't dump, and don't retry */


#define BU_MAXNAMELEN 32	/* max for misc. names: eg volumes */
#define BU_MAXTAPELEN 32	/* for tape names */
#define BU_MAXHOSTLEN 32	/* for server (machine) names */
#define BU_MAXTOKENLEN 16	/* identifiers */
#define BU_MAXUNAMELEN 256	/* length of a user name */
#define BU_MAXCELLLEN 256	/* length of a cell name */

/* proposed maximum name lengths  PA */
#define	BU_MAX_NAME	64	/* misc. names */
#define BU_MAX_DUMP_PATH 256	/* length of schedule path name */

#define BC_MAXPORTOFFSET    58510	/* max number of port offsets in database */

#ifndef NEVERDATE
#define NEVERDATE 037777777777	/* a date that will never come */
#endif
#ifndef NEVERSTRING
#define NEVERSTRING  "NEVER                   \n"
#endif
#define cTIME(t)  ( (*(t) == NEVERDATE) ? (char *)NEVERSTRING : (char *)ctime(t) )

#ifndef Date
#define Date afs_uint32
#endif

#define	DUMP_TAPE_NAME	"Ubik_db_dump"	/* base database tape name */

/* for expiration date processing */
#define	BC_NO_EXPDATE		0	/* no expiration date */
#define	BC_ABS_EXPDATE		1	/* absolute expiration date */
#define	BC_REL_EXPDATE		2	/* relative expiration date */

/*macro which generates tape names from tapeSetName */
#define tc_MakeTapeName(name,set,seq) \
    sprintf (name, (set)->format, (seq) + (set)->b)

/* common structure definitions */
struct dlqlink {
    struct dlqlink *dlq_next;
    struct dlqlink *dlq_prev;
    afs_int32 dlq_type;
    char *dlq_structPtr;	/* enclosing structure */
};

typedef struct dlqlink dlqlinkT;
typedef dlqlinkT *dlqlinkP;


/* invariants */
#define	DLQ_HEAD	1001

#define DLQ_VOLENTRY	10
#define DLQ_FINISHTAPE	11
#define DLQ_USETAPE	12
#define DLQ_FINISHDUMP  13
#define DLQ_USEDUMP     14

/* simple assertion - for testing, don't halt processing */

#define	DLQASSERT(condition)					\
	if ( (condition) == 0 )					\
	{							\
	    printf("DLQASSERT: %s failed\n", #condition);	\
	}

/* function typing */
extern dlqlinkP dlqFront();
extern dlqlinkP dlqUnlinkb();
extern dlqlinkP dlqUnlinkf();

/* -----------------------------
 * Status management
 * -----------------------------
 */

/* status management flags
 *
 * Key to comments:
 *	C - client side only
 *	S - server side only
 *	B - client or server
 */

#define STARTING	0x1	/* C; still setting up, no server task yet */
#define	ABORT_REQUEST	0x2	/* B; user requested abort */
#define	ABORT_SENT	0x4	/* C; abort sent to server (REQ cleared) */
#define	ABORT_DONE	0x8	/* S; abort complete on server */
#define	ABORT_LOCAL	0x10	/* C; abort local task if contact lost */
#define	TASK_DONE 	0x20	/* B; task complete */
#define	SILENT		0x400	/* C; don't be verbose about termination */
#define	NOREMOVE	0x1000	/* C; don't remove from queue */

/* comm status */
#define	CONTACT_LOST	0x40	/* B; contact lost */

/* errror handling */
#define	TASK_ERROR	0x80	/* S; had fatal error, will terminate */

/* general status - copied back to client for jobs status */
#define	DRIVE_WAIT	0x100	/* S; waiting for drive to become free */
#define	OPR_WAIT	0x200	/* S; waiting for operator action */
#define CALL_WAIT       0x800	/* S; waiting for callout routine completion */

struct statusS {
    dlqlinkT link;

    afs_uint32 taskId;		/* task identifier */
    afs_uint32 dbDumpId;	/* dump id */
    afs_uint32 flags;		/* as above */
    afs_uint32 nKBytes;		/* bytes xferred */
    char volumeName[BU_MAXNAMELEN];	/* current volume (if any) */
    afs_int32 volsFailed;	/* # operation failures */
    afs_int32 lastPolled;	/* last successful poll */

    /* bucoord local */
    char taskName[64];		/* type of task */
    afs_int32 port;
    afs_int32 jobNumber;
    afs_int32 volsTotal;	/* total # vols */
    afs_int32 scheduledDump;	/* Time this dump was scheduled */
    char *cmdLine;		/* Command to exectute for this dump */
};

typedef struct statusS statusT;
typedef statusT *statusP;

#endif
