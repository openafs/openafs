/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * All Rights Reserved
 */
#ifndef _ICL_H__ENV_
#define _ICL_H__ENV_ 1

#ifdef	KERNEL
#include "afs/param.h"
#include "afs_osi.h"
#include "lock.h"
#include "afs_trace.h"
#else
#include <lock.h>
typedef struct Lock afs_lock_t;
#endif

#define ICL_LOGSPERSET		8	/* max logs per set */
#define ICL_DEFAULTEVENTS	1024	/* default events per event set */
#define ICL_DEFAULT_LOGSIZE	60*1024	/* number of words in default log size */

#define osi_dlock_t afs_lock_t
extern osi_dlock_t afs_icl_lock;	/* lock for log and set refcounts */
extern int afs_icl_inited;
extern struct afs_icl_log *afs_icl_allLogs;
extern struct afs_icl_set *afs_icl_allSets;

/* define an in-core logging package */
struct afs_icl_set {
    afs_int32 refCount;		/* reference count */
    afs_int32 states;		/* state flags */
    osi_dlock_t lock;		/* lock */
    struct afs_icl_set *nextp;	/* next dude in all known tables */
    char *name;			/* name of set */
    struct afs_icl_log *logs[ICL_LOGSPERSET];	/* logs */
    afs_int32 nevents;		/* count of events */
    char *eventFlags;		/* pointer to event flags */
};

/* set flags */
#define ICL_SETF_DELETED	1
#define	ICL_SETF_ACTIVE		2
#define	ICL_SETF_FREED		4
#define	ICL_SETF_PERSISTENT	8

#ifdef ICL_DEFAULT_ENABLED
#define ICL_DEFAULT_SET_STATES	0	/* was ICL_SETF_ACTIVE */
#else /* ICL_DEFAULT_ENABLED */

#ifdef ICL_DEFAULT_DISABLED
#define ICL_DEFAULT_SET_STATES	ICL_SETF_FREED
#endif /* ICL_DEFAULT_DISABLED */

#endif /* ICL_DEFAULT_ENABLED */

#ifndef ICL_DEFAULT_SET_STATES
/* if not changed already, default to ACTIVE on created sets */
#define ICL_DEFAULT_SET_STATES	0	/* was ICL_SETF_ACTIVE */
#endif /* ICL_DEFAULT_SET_STATES */

/* bytes required by eventFlags array, for x events */
#define ICL_EVENTBYTES(x)	((((x) - 1) | 7) + 1)

/* functions for finding a particular event */
#define ICL_EVENTBYTE(x)	(((x) & 0x3ff) >> 3)
#define ICL_EVENTMASK(x)	(1 << ((x) & 0x7))
#define ICL_EVENTOK(setp, x)	((x&0x3ff) >= 0 && (x&0x3ff) < (setp)->nevents)

/* define ICL syscalls by name!! */
#define ICL_OP_COPYOUT		1
#define	ICL_OP_ENUMLOGS		2
#define	ICL_OP_CLRLOG		3
#define	ICL_OP_CLRSET		4
#define ICL_OP_CLRALL		5
#define ICL_OP_ENUMSETS		6
#define	ICL_OP_SETSTAT		7
#define	ICL_OP_SETSTATALL	8
#define ICL_OP_SETLOGSIZE	9
#define	ICL_OP_ENUMLOGSBYSET	10
#define ICL_OP_GETSETINFO	11
#define ICL_OP_GETLOGINFO	12
#define ICL_OP_COPYOUTCLR	13
/* Determine version number and type size info from icl package. */
#define ICL_OP_VERSION		14

/* define setstat commands */
#define ICL_OP_SS_ACTIVATE	1
#define ICL_OP_SS_DEACTIVATE	2
#define ICL_OP_SS_FREE		3

/* define set status flags */
#define	ICL_FLAG_ACTIVE		1
#define	ICL_FLAG_FREED		2

/* The format of the circular log is:
 * 1'st word:
 * <8 bits: size of record in longs> <6 bits: p1 type> <6 bits: p2 type>
 *     <6 bits: p3 type> <6 bits: p4 type>
 * <32 bits: opcode>
 * <32 bits: pid>
 * <32 bits: timestamp in microseconds>
 * <p1 rep>
 * <p2 rep>
 * <p3 rep>
 * <p4 rep>
 *
 * Note that the representation for each parm starts at a new 32 bit
 * offset, and only the represented parameters are marshalled.
 * You can tell if a particular parameter exists by looking at its
 * type; type 0 means the parameter does not exist.
 */

/* descriptor of the circular log.  Note that it can never be 100% full
 * since we break the ambiguity of head == tail by calling that
 * state empty.
 */
struct afs_icl_log {
    int refCount;		/* reference count */
    int setCount;		/* number of non-FREED sets pointing to this guy */
    osi_dlock_t lock;		/* lock */
    char *name;			/* log name */
    struct afs_icl_log *nextp;	/* next log in system */
    afs_int32 logSize;		/* allocated # of elements in log */
    afs_int32 logElements;	/* # of elements in log right now */
    afs_int32 *datap;		/* pointer to the data */
    afs_int32 firstUsed;	/* first element used */
    afs_int32 firstFree;	/* index of first free dude */
    long baseCookie;		/* cookie value of first entry */
    afs_int32 states;		/* state bits */
    afs_uint32 lastTS;		/* last timestamp written to this log */
};

/* macro used to compute size of parameter when in log, used by
 * afs_icl_AppendRecord below.
 *
 * Note that "tsize" and "rsize" are free variables!
 * I use rsize to determine correct alignment (and hence size).
 * The #ifdef's start to get unwieldly when the 32 bit kernels for SGI 6.2 are
 * factored in. So, I'm going to a single macro with a variable for sizeof long.
 * User space programs need to set this based on the size of the kernel long.
 * Defined in afs_call.c and venus/fstrace.c
 */
extern int afs_icl_sizeofLong;

#define ICL_SIZEHACK(t1, p1, ts1, rs1)		\
    MACRO_BEGIN \
	if ((t1) == ICL_TYPE_STRING) { \
	    ts1 = (int)((unsigned)(strlen((char *)(p1)) + 4) >> 2); \
	} else if ((t1) == ICL_TYPE_HYPER  || (t1) == ICL_TYPE_INT64) \
	    ts1 = 2; \
	else if ((t1) == ICL_TYPE_FID) \
	    ts1 = 4; \
	else if ((t1) == ICL_TYPE_INT32) \
	    ts1 = 1; \
	else \
	    ts1 = afs_icl_sizeofLong; \
	/* now add in the parameter */ \
	rs1 += ts1; \
    MACRO_END

/* log flags */
#define ICL_LOGF_DELETED	1	/* freed */
#define ICL_LOGF_WAITING	2	/* waiting for output to appear */
#define ICL_LOGF_PERSISTENT	4	/* persistent */

/* macros for calling these things easily */
#define ICL_SETACTIVE(setp)	((setp) && (setp->states & ICL_SETF_ACTIVE))
#define afs_Trace0(set, id) \
    (ICL_SETACTIVE(set) ? afs_icl_Event0(set, id, 1<<24) : 0)
#define afs_Trace1(set, id, t1, p1) \
    (ICL_SETACTIVE(set) ? afs_icl_Event1(set, id, (1<<24)+((t1)<<18), (long)(p1)) : 0)
#define afs_Trace2(set, id, t1, p1, t2, p2) \
    (ICL_SETACTIVE(set) ? afs_icl_Event2(set, id, (1<<24)+((t1)<<18)+((t2)<<12), \
				       (long)(p1), (long)(p2)) : 0)
#define afs_Trace3(set, id, t1, p1, t2, p2, t3, p3) \
    (ICL_SETACTIVE(set) ? afs_icl_Event3(set, id, (1<<24)+((t1)<<18)+((t2)<<12)+((t3)<<6), \
				       (long)(p1), (long)(p2), (long)(p3)) : 0)
#define afs_Trace4(set, id, t1, p1, t2, p2, t3, p3, t4, p4) \
    (ICL_SETACTIVE(set) ? afs_icl_Event4(set, id, (1<<24)+((t1)<<18)+((t2)<<12)+((t3)<<6)+(t4), \
				       (long)(p1), (long)(p2), (long)(p3), (long)(p4)) : 0)

/* types for icl_trace macros; values must be less than 64.  If
 * you add a new type here, don't forget to check for ICL_MAXEXPANSION
 * changing.
 */
#define ICL_TYPE_NONE		0	/* parameter doesn't exist */
#define ICL_TYPE_LONG		1
#define ICL_TYPE_INT32		7
#define ICL_TYPE_POINTER	2
#define ICL_TYPE_HYPER		3
#define ICL_TYPE_STRING		4
#define ICL_TYPE_FID		5
#define	ICL_TYPE_UNIXDATE	6
#define ICL_TYPE_INT64		8

#ifdef AFS_64BIT_CLIENT
#define ICL_TYPE_OFFSET         ICL_TYPE_INT64
#define ICL_HANDLE_OFFSET(x)    (&x)
#else /* AFS_64BIT_CLIENT */
#define ICL_TYPE_OFFSET         ICL_TYPE_INT64
#define ICL_HANDLE_OFFSET(x)    (x)
#endif /* AFS_64BIT_CLIENT */

/* max # of words put in the printf buffer per parameter */
#define ICL_MAXEXPANSION	4

/* define flags to be used by afs_icl_CreateSetWithFlags */
#define ICL_CRSET_FLAG_DEFAULT_ON	1
#define ICL_CRSET_FLAG_DEFAULT_OFF	2
#define ICL_CRSET_FLAG_PERSISTENT	4

/* define flags to be used by afs_icl_CreateLogWithFlags */
#define ICL_CRLOG_FLAG_PERSISTENT	1

/* input flags */
#define ICL_COPYOUTF_WAITIO		1	/* block for output */
#define ICL_COPYOUTF_CLRAFTERREAD	2	/* clear log after reading */
/* output flags */
#define ICL_COPYOUTF_MISSEDSOME		1	/* missed some output */

#define lock_ObtainWrite	ObtainWriteLock
#define lock_ReleaseWrite	ReleaseWriteLock

#ifdef	KERNEL
extern struct afs_icl_set *afs_iclSetp;	/* standard icl trace */
/* A separate icl set to collect long term debugging info. */
extern struct afs_icl_set *afs_iclLongTermSetp;
#else
#define	osi_Alloc		malloc
#define	osi_Free(a,b)		free(a)

#define ICL_RPC_MAX_SETS (64)
#define ICL_RPC_MAX_LOGS (64)

typedef struct afs_icl_setinfo {
    u_char setName[32];
    afs_uint32 states;
} afs_icl_setinfo_t;

typedef struct afs_icl_loginfo {
    u_char logName[32];
    afs_uint32 logSize;
    afs_uint32 logElements;
    afs_uint32 states;
} afs_icl_loginfo_t;

typedef struct afs_icl_bulkSetinfo {
    afs_uint32 count;
    afs_icl_setinfo_t setinfo[1];
} afs_icl_bulkSetinfo_t;

typedef struct afs_icl_bulkLoginfo {
    afs_uint32 count;
    afs_icl_loginfo_t loginfo[1];
} afs_icl_bulkLoginfo_t;

#endif


#define	ICL_ERROR_NOSET		9001
#define	ICL_ERROR_NOLOG		9002
#define	ICL_ERROR_CANTOPEN	9003
#define	ICL_INFO_TIMESTAMP	9004

#endif /* _ICL_H__ENV_ */
