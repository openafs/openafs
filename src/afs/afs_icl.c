/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "rx/rx_globals.h"
#if !defined(UKERNEL) && !defined(AFS_LINUX20_ENV)
#include "net/if.h"
#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_DARWIN60_ENV)
#include "netinet/in_var.h"
#endif
#endif /* !defined(UKERNEL) */
#ifdef AFS_LINUX22_ENV
#include "h/smp_lock.h"
#endif


struct afs_icl_set *afs_iclSetp = (struct afs_icl_set *)0;
struct afs_icl_set *afs_iclLongTermSetp = (struct afs_icl_set *)0;

#if defined(AFS_OSF_ENV) || defined(AFS_SGI61_ENV)
/* For SGI 6.2, this can is changed to 1 if it's a 32 bit kernel. */
#if defined(AFS_SGI62_ENV) && defined(KERNEL) && !defined(_K64U64)
int afs_icl_sizeofLong = 1;
#else
int afs_icl_sizeofLong = 2;
#endif /* SGI62 */
#else
#if defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)
int afs_icl_sizeofLong = 2;
#else
int afs_icl_sizeofLong = 1;
#endif
#endif

int afs_icl_inited = 0;

/* init function, called once, under afs_icl_lock */
int
afs_icl_Init(void)
{
    afs_icl_inited = 1;
    return 0;
}

int
afs_icl_InitLogs(void)
{
    struct afs_icl_log *logp;
    int code;

    /* initialize the ICL system */
    code = afs_icl_CreateLog("cmfx", 60 * 1024, &logp);
    if (code == 0)
	code =
	    afs_icl_CreateSetWithFlags("cm", logp, NULL,
				       ICL_CRSET_FLAG_DEFAULT_OFF,
				       &afs_iclSetp);
    code =
	afs_icl_CreateSet("cmlongterm", logp, NULL,
			  &afs_iclLongTermSetp);
    return code;
}


struct afs_icl_log *afs_icl_FindLog();
struct afs_icl_set *afs_icl_FindSet();


int
Afscall_icl(long opcode, long p1, long p2, long p3, long p4, long *retval)
{
    afs_int32 *lp, elts, flags;
    register afs_int32 code;
    struct afs_icl_log *logp;
    struct afs_icl_set *setp;
#if defined(AFS_SGI61_ENV) || defined(AFS_SUN57_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    size_t temp;
#else /* AFS_SGI61_ENV */
#if defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)
    afs_uint64 temp;
#else
    afs_uint32 temp;
#endif
#endif /* AFS_SGI61_ENV */
    char tname[65];
    afs_int32 startCookie;
    afs_int32 allocated;
    struct afs_icl_log *tlp;

#ifdef	AFS_SUN5_ENV
    if (!afs_suser(CRED())) {	/* only root can run this code */
	return (EACCES);
    }
#else
    if (!afs_suser(NULL)) {	/* only root can run this code */
#if defined(KERNEL_HAVE_UERROR)
	setuerror(EACCES);
	return EACCES;
#else
	return EPERM;
#endif
    }
#endif
    switch (opcode) {
    case ICL_OP_COPYOUTCLR:	/* copy out data then clear */
    case ICL_OP_COPYOUT:	/* copy ouy data */
	/* copyout: p1=logname, p2=&buffer, p3=size(words), p4=&cookie
	 * return flags<<24 + nwords.
	 * updates cookie to updated start (not end) if we had to
	 * skip some records.
	 */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	AFS_COPYIN((char *)p4, (char *)&startCookie, sizeof(afs_int32), code);
	if (code)
	    return code;
	logp = afs_icl_FindLog(tname);
	if (!logp)
	    return ENOENT;
#define	BUFFERSIZE	AFS_LRALLOCSIZ
	lp = (afs_int32 *) osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	elts = BUFFERSIZE / sizeof(afs_int32);
	if (p3 < elts)
	    elts = p3;
	flags = (opcode == ICL_OP_COPYOUT) ? 0 : ICL_COPYOUTF_CLRAFTERREAD;
	code =
	    afs_icl_CopyOut(logp, lp, &elts, (afs_uint32 *) & startCookie,
			    &flags);
	if (code) {
	    osi_FreeLargeSpace((struct osi_buffer *)lp);
	    break;
	}
	AFS_COPYOUT((char *)lp, (char *)p2, elts * sizeof(afs_int32), code);
	if (code)
	    goto done;
	AFS_COPYOUT((char *)&startCookie, (char *)p4, sizeof(afs_int32),
		    code);
	if (code)
	    goto done;
#if defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)
	if (!(IS64U))
	    *retval = ((long)((flags << 24) | (elts & 0xffffff))) << 32;
	else
#endif
	    *retval = (flags << 24) | (elts & 0xffffff);
      done:
	afs_icl_LogRele(logp);
	osi_FreeLargeSpace((struct osi_buffer *)lp);
	break;

    case ICL_OP_ENUMLOGS:	/* enumerate logs */
	/* enumerate logs: p1=index, p2=&name, p3=sizeof(name), p4=&size.
	 * return 0 for success, otherwise error.
	 */
	for (tlp = afs_icl_allLogs; tlp; tlp = tlp->nextp) {
	    if (p1-- == 0)
		break;
	}
	if (!tlp)
	    return ENOENT;	/* past the end of file */
	temp = strlen(tlp->name) + 1;
	if (temp > p3)
	    return EINVAL;
	AFS_COPYOUT(tlp->name, (char *)p2, temp, code);
	if (!code)		/* copy out size of log */
	    AFS_COPYOUT((char *)&tlp->logSize, (char *)p4, sizeof(afs_int32),
			code);
	break;

    case ICL_OP_ENUMLOGSBYSET:	/* enumerate logs by set name */
	/* enumerate logs: p1=setname, p2=index, p3=&name, p4=sizeof(name).
	 * return 0 for success, otherwise error.
	 */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	setp = afs_icl_FindSet(tname);
	if (!setp)
	    return ENOENT;
	if (p2 > ICL_LOGSPERSET)
	    return EINVAL;
	if (!(tlp = setp->logs[p2]))
	    return EBADF;
	temp = strlen(tlp->name) + 1;
	if (temp > p4)
	    return EINVAL;
	AFS_COPYOUT(tlp->name, (char *)p3, temp, code);
	break;

    case ICL_OP_CLRLOG:	/* clear specified log */
	/* zero out the specified log: p1=logname */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	logp = afs_icl_FindLog(tname);
	if (!logp)
	    return ENOENT;
	code = afs_icl_ZeroLog(logp);
	afs_icl_LogRele(logp);
	break;

    case ICL_OP_CLRSET:	/* clear specified set */
	/* zero out the specified set: p1=setname */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	setp = afs_icl_FindSet(tname);
	if (!setp)
	    return ENOENT;
	code = afs_icl_ZeroSet(setp);
	afs_icl_SetRele(setp);
	break;

    case ICL_OP_CLRALL:	/* clear all logs */
	/* zero out all logs -- no args */
	code = 0;
	ObtainWriteLock(&afs_icl_lock, 178);
	for (tlp = afs_icl_allLogs; tlp; tlp = tlp->nextp) {
	    tlp->refCount++;	/* hold this guy */
	    ReleaseWriteLock(&afs_icl_lock);
	    /* don't clear persistent logs */
	    if ((tlp->states & ICL_LOGF_PERSISTENT) == 0)
		code = afs_icl_ZeroLog(tlp);
	    ObtainWriteLock(&afs_icl_lock, 179);
	    if (--tlp->refCount == 0)
		afs_icl_ZapLog(tlp);
	    if (code)
		break;
	}
	ReleaseWriteLock(&afs_icl_lock);
	break;

    case ICL_OP_ENUMSETS:	/* enumerate all sets */
	/* enumerate sets: p1=index, p2=&name, p3=sizeof(name), p4=&states.
	 * return 0 for success, otherwise error.
	 */
	for (setp = afs_icl_allSets; setp; setp = setp->nextp) {
	    if (p1-- == 0)
		break;
	}
	if (!setp)
	    return ENOENT;	/* past the end of file */
	temp = strlen(setp->name) + 1;
	if (temp > p3)
	    return EINVAL;
	AFS_COPYOUT(setp->name, (char *)p2, temp, code);
	if (!code)		/* copy out size of log */
	    AFS_COPYOUT((char *)&setp->states, (char *)p4, sizeof(afs_int32),
			code);
	break;

    case ICL_OP_SETSTAT:	/* set status on a set */
	/* activate the specified set: p1=setname, p2=op */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	setp = afs_icl_FindSet(tname);
	if (!setp)
	    return ENOENT;
	code = afs_icl_SetSetStat(setp, p2);
	afs_icl_SetRele(setp);
	break;

    case ICL_OP_SETSTATALL:	/* set status on all sets */
	/* activate the specified set: p1=op */
	code = 0;
	ObtainWriteLock(&afs_icl_lock, 180);
	for (setp = afs_icl_allSets; setp; setp = setp->nextp) {
	    setp->refCount++;	/* hold this guy */
	    ReleaseWriteLock(&afs_icl_lock);
	    /* don't set states on persistent sets */
	    if ((setp->states & ICL_SETF_PERSISTENT) == 0)
		code = afs_icl_SetSetStat(setp, p1);
	    ObtainWriteLock(&afs_icl_lock, 181);
	    if (--setp->refCount == 0)
		afs_icl_ZapSet(setp);
	    if (code)
		break;
	}
	ReleaseWriteLock(&afs_icl_lock);
	break;

    case ICL_OP_SETLOGSIZE:	/* set size of log */
	/* set the size of the specified log: p1=logname, p2=size (in words) */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	logp = afs_icl_FindLog(tname);
	if (!logp)
	    return ENOENT;
	code = afs_icl_LogSetSize(logp, p2);
	afs_icl_LogRele(logp);
	break;

    case ICL_OP_GETLOGINFO:	/* get size of log */
	/* zero out the specified log: p1=logname, p2=&logSize, p3=&allocated */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	logp = afs_icl_FindLog(tname);
	if (!logp)
	    return ENOENT;
	allocated = !!logp->datap;
	AFS_COPYOUT((char *)&logp->logSize, (char *)p2, sizeof(afs_int32),
		    code);
	if (!code)
	    AFS_COPYOUT((char *)&allocated, (char *)p3, sizeof(afs_int32),
			code);
	afs_icl_LogRele(logp);
	break;

    case ICL_OP_GETSETINFO:	/* get state of set */
	/* zero out the specified set: p1=setname, p2=&state */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code)
	    return code;
	setp = afs_icl_FindSet(tname);
	if (!setp)
	    return ENOENT;
	AFS_COPYOUT((char *)&setp->states, (char *)p2, sizeof(afs_int32),
		    code);
	afs_icl_SetRele(setp);
	break;

    default:
	code = EINVAL;
    }

    return code;
}


afs_lock_t afs_icl_lock;

/* exported routine: a 4 parameter event */
int
afs_icl_Event4(register struct afs_icl_set *setp, afs_int32 eventID,
	       afs_int32 lAndT, long p1, long p2, long p3, long p4)
{
    afs_int32 mask;
    register int i;
    register afs_int32 tmask;
    int ix;

    /* If things aren't init'ed yet (or the set is inactive), don't panic */
    if (!ICL_SETACTIVE(setp))
	return 0;

    AFS_ASSERT_GLOCK();
    mask = lAndT >> 24 & 0xff;	/* mask of which logs to log to */
    ix = ICL_EVENTBYTE(eventID);
    ObtainReadLock(&setp->lock);
    if (setp->eventFlags[ix] & ICL_EVENTMASK(eventID)) {
	for (i = 0, tmask = 1; i < ICL_LOGSPERSET; i++, tmask <<= 1) {
	    if (mask & tmask) {
		afs_icl_AppendRecord(setp->logs[i], eventID, lAndT & 0xffffff,
				     p1, p2, p3, p4);
	    }
	    mask &= ~tmask;
	    if (mask == 0)
		break;		/* break early */
	}
    }
    ReleaseReadLock(&setp->lock);
    return 0;
}

/* Next 4 routines should be implemented via var-args or something.
 * Whole purpose is to avoid compiler warnings about parameter # mismatches.
 * Otherwise, could call afs_icl_Event4 directly.
 */
int
afs_icl_Event3(register struct afs_icl_set *setp, afs_int32 eventID,
	       afs_int32 lAndT, long p1, long p2, long p3)
{
    return afs_icl_Event4(setp, eventID, lAndT, p1, p2, p3, (long)0);
}

int
afs_icl_Event2(register struct afs_icl_set *setp, afs_int32 eventID,
	       afs_int32 lAndT, long p1, long p2)
{
    return afs_icl_Event4(setp, eventID, lAndT, p1, p2, (long)0, (long)0);
}

int
afs_icl_Event1(register struct afs_icl_set *setp, afs_int32 eventID,
	       afs_int32 lAndT, long p1)
{
    return afs_icl_Event4(setp, eventID, lAndT, p1, (long)0, (long)0,
			  (long)0);
}

int
afs_icl_Event0(register struct afs_icl_set *setp, afs_int32 eventID,
	       afs_int32 lAndT)
{
    return afs_icl_Event4(setp, eventID, lAndT, (long)0, (long)0, (long)0,
			  (long)0);
}

struct afs_icl_log *afs_icl_allLogs = 0;

/* function to purge records from the start of the log, until there
 * is at least minSpace long's worth of space available without
 * making the head and the tail point to the same word.
 *
 * Log must be write-locked.
 */
static void
afs_icl_GetLogSpace(register struct afs_icl_log *logp, afs_int32 minSpace)
{
    register unsigned int tsize;

    while (logp->logSize - logp->logElements <= minSpace) {
	/* eat a record */
	tsize = ((logp->datap[logp->firstUsed]) >> 24) & 0xff;
	logp->logElements -= tsize;
	logp->firstUsed += tsize;
	if (logp->firstUsed >= logp->logSize)
	    logp->firstUsed -= logp->logSize;
	logp->baseCookie += tsize;
    }
}

/* append string astr to buffer, including terminating null char.
 *
 * log must be write-locked.
 */
#define ICL_CHARSPERLONG	4
static void
afs_icl_AppendString(struct afs_icl_log *logp, char *astr)
{
    char *op;			/* ptr to char to write */
    int tc;
    register int bib;		/* bytes in buffer */

    bib = 0;
    op = (char *)&(logp->datap[logp->firstFree]);
    while (1) {
	tc = *astr++;
	*op++ = tc;
	if (++bib >= ICL_CHARSPERLONG) {
	    /* new word */
	    bib = 0;
	    if (++(logp->firstFree) >= logp->logSize) {
		logp->firstFree = 0;
		op = (char *)&(logp->datap[0]);
	    }
	    logp->logElements++;
	}
	if (tc == 0)
	    break;
    }
    if (bib > 0) {
	/* if we've used this word at all, allocate it */
	if (++(logp->firstFree) >= logp->logSize) {
	    logp->firstFree = 0;
	}
	logp->logElements++;
    }
}

/* add a long to the log, ignoring overflow (checked already) */
#define ICL_APPENDINT32(lp, x) \
    MACRO_BEGIN \
        (lp)->datap[(lp)->firstFree] = (x); \
	if (++((lp)->firstFree) >= (lp)->logSize) { \
		(lp)->firstFree = 0; \
	} \
        (lp)->logElements++; \
    MACRO_END

#if defined(AFS_OSF_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64)) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
#define ICL_APPENDLONG(lp, x) \
    MACRO_BEGIN \
	ICL_APPENDINT32((lp), ((x) >> 32) & 0xffffffffL); \
	ICL_APPENDINT32((lp), (x) & 0xffffffffL); \
    MACRO_END

#else /* AFS_OSF_ENV */
#define ICL_APPENDLONG(lp, x) ICL_APPENDINT32((lp), (x))
#endif /* AFS_OSF_ENV */

/* routine to tell whether we're dealing with the address or the
 * object itself
 */
int
afs_icl_UseAddr(int type)
{
    if (type == ICL_TYPE_HYPER || type == ICL_TYPE_STRING
	|| type == ICL_TYPE_FID || type == ICL_TYPE_INT64)
	return 1;
    else
	return 0;
}

/* Function to append a record to the log.  Written for speed
 * since we know that we're going to have to make this work fast
 * pretty soon, anyway.  The log must be unlocked.
 */

void
afs_icl_AppendRecord(register struct afs_icl_log *logp, afs_int32 op,
		     afs_int32 types, long p1, long p2, long p3, long p4)
{
    int rsize;			/* record size in longs */
    register int tsize;		/* temp size */
    osi_timeval_t tv;
    int t1, t2, t3, t4;

    t4 = types & 0x3f;		/* decode types */
    types >>= 6;
    t3 = types & 0x3f;
    types >>= 6;
    t2 = types & 0x3f;
    types >>= 6;
    t1 = types & 0x3f;

    osi_GetTime(&tv);		/* It panics for solaris if inside */
    ObtainWriteLock(&logp->lock, 182);
    if (!logp->datap) {
	ReleaseWriteLock(&logp->lock);
	return;
    }

    /* get timestamp as # of microseconds since some time that doesn't
     * change that often.  This algorithm ticks over every 20 minutes
     * or so (1000 seconds).  Write a timestamp record if it has.
     */
    if (tv.tv_sec - logp->lastTS > 1024) {
	/* the timer has wrapped -- write a timestamp record */
	if (logp->logSize - logp->logElements <= 5)
	    afs_icl_GetLogSpace(logp, 5);

	ICL_APPENDINT32(logp,
			(afs_int32) (5 << 24) + (ICL_TYPE_UNIXDATE << 18));
	ICL_APPENDINT32(logp, (afs_int32) ICL_INFO_TIMESTAMP);
	ICL_APPENDINT32(logp, (afs_int32) 0);	/* use thread ID zero for clocks */
	ICL_APPENDINT32(logp,
			(afs_int32) (tv.tv_sec & 0x3ff) * 1000000 +
			tv.tv_usec);
	ICL_APPENDINT32(logp, (afs_int32) tv.tv_sec);

	logp->lastTS = tv.tv_sec;
    }

    rsize = 4;			/* base case */
    if (t1) {
	/* compute size of parameter p1.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t1, p1);
    }
    if (t2) {
	/* compute size of parameter p2.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t2, p2);
    }
    if (t3) {
	/* compute size of parameter p3.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t3, p3);
    }
    if (t4) {
	/* compute size of parameter p4.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t4, p4);
    }

    /* At this point, we've computed all of the parameter sizes, and
     * have in rsize the size of the entire record we want to append.
     * Next, we check that we actually have room in the log to do this
     * work, and then we do the append.
     */
    if (rsize > 255) {
	ReleaseWriteLock(&logp->lock);
	return;			/* log record too big to express */
    }

    if (logp->logSize - logp->logElements <= rsize)
	afs_icl_GetLogSpace(logp, rsize);

    ICL_APPENDINT32(logp,
		    (afs_int32) (rsize << 24) + (t1 << 18) + (t2 << 12) +
		    (t3 << 6) + t4);
    ICL_APPENDINT32(logp, (afs_int32) op);
    ICL_APPENDINT32(logp, (afs_int32) osi_ThreadUnique());
    ICL_APPENDINT32(logp,
		    (afs_int32) (tv.tv_sec & 0x3ff) * 1000000 + tv.tv_usec);

    if (t1) {
	/* marshall parameter 1 now */
	if (t1 == ICL_TYPE_STRING) {
	    afs_icl_AppendString(logp, (char *)p1);
	} else if (t1 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p1)->high);
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p1)->low);
	} else if (t1 == ICL_TYPE_INT64) {
#ifndef WORDS_BIGENDIAN
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[0]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) p1);
	    ICL_APPENDINT32(logp, (afs_int32) 0);
#endif /* AFS_64BIT_CLIENT */
#else /* WORDS_BIGENDIAN */
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[1]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) 0);
	    ICL_APPENDINT32(logp, (afs_int32) p1);
#endif /* AFS_64BIT_CLIENT */
#endif /* WORDS_BIGENDIAN */
	} else if (t1 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[2]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p1)[3]);
	}
#if defined(AFS_OSF_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64)) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
	else if (t1 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32) p1);
#endif /* AFS_OSF_ENV */
	else
	    ICL_APPENDLONG(logp, p1);
    }
    if (t2) {
	/* marshall parameter 2 now */
	if (t2 == ICL_TYPE_STRING)
	    afs_icl_AppendString(logp, (char *)p2);
	else if (t2 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p2)->high);
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p2)->low);
	} else if (t2 == ICL_TYPE_INT64) {
#ifndef WORDS_BIGENDIAN
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[0]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) p2);
	    ICL_APPENDINT32(logp, (afs_int32) 0);
#endif /* AFS_64BIT_CLIENT */
#else /* WORDS_BIGENDIAN */
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[1]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) 0);
	    ICL_APPENDINT32(logp, (afs_int32) p2);
#endif /* AFS_64BIT_CLIENT */
#endif /* WORDS_BIGENDIAN */
	} else if (t2 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[2]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p2)[3]);
	}
#if defined(AFS_OSF_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64)) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
	else if (t2 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32) p2);
#endif /* AFS_OSF_ENV */
	else
	    ICL_APPENDLONG(logp, p2);
    }
    if (t3) {
	/* marshall parameter 3 now */
	if (t3 == ICL_TYPE_STRING)
	    afs_icl_AppendString(logp, (char *)p3);
	else if (t3 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p3)->high);
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p3)->low);
	} else if (t3 == ICL_TYPE_INT64) {
#ifndef WORDS_BIGENDIAN
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[0]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) p3);
	    ICL_APPENDINT32(logp, (afs_int32) 0);
#endif /* AFS_64BIT_CLIENT */
#else /* WORDS_BIGENDIAN */
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[1]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) 0);
	    ICL_APPENDINT32(logp, (afs_int32) p3);
#endif /* AFS_64BIT_CLIENT */
#endif /* WORDS_BIGENDIAN */
	} else if (t3 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[2]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p3)[3]);
	}
#if defined(AFS_OSF_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64)) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
	else if (t3 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32) p3);
#endif /* AFS_OSF_ENV */
	else
	    ICL_APPENDLONG(logp, p3);
    }
    if (t4) {
	/* marshall parameter 4 now */
	if (t4 == ICL_TYPE_STRING)
	    afs_icl_AppendString(logp, (char *)p4);
	else if (t4 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p4)->high);
	    ICL_APPENDINT32(logp,
			    (afs_int32) ((struct afs_hyper_t *)p4)->low);
	} else if (t4 == ICL_TYPE_INT64) {
#ifndef WORDS_BIGENDIAN
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[0]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) p4);
	    ICL_APPENDINT32(logp, (afs_int32) 0);
#endif /* AFS_64BIT_CLIENT */
#else /* WORDS_BIGENDIAN */
#ifdef AFS_64BIT_CLIENT
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[1]);
#else /* AFS_64BIT_CLIENT */
	    ICL_APPENDINT32(logp, (afs_int32) 0);
	    ICL_APPENDINT32(logp, (afs_int32) p4);
#endif /* AFS_64BIT_CLIENT */
#endif /* WORDS_BIGENDIAN */
	} else if (t4 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[0]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[1]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[2]);
	    ICL_APPENDINT32(logp, (afs_int32) ((afs_int32 *) p4)[3]);
	}
#if defined(AFS_OSF_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64)) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
	else if (t4 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32) p4);
#endif /* AFS_OSF_ENV */
	else
	    ICL_APPENDLONG(logp, p4);
    }
    ReleaseWriteLock(&logp->lock);
}

/* create a log with size logSize; return it in *outLogpp and tag
 * it with name "name."
 */
int
afs_icl_CreateLog(char *name, afs_int32 logSize,
		  struct afs_icl_log **outLogpp)
{
    return afs_icl_CreateLogWithFlags(name, logSize, /*flags */ 0, outLogpp);
}

/* create a log with size logSize; return it in *outLogpp and tag
 * it with name "name."  'flags' can be set to make the log unclearable.
 */
int
afs_icl_CreateLogWithFlags(char *name, afs_int32 logSize, afs_uint32 flags,
			   struct afs_icl_log **outLogpp)
{
    register struct afs_icl_log *logp;

    /* add into global list under lock */
    ObtainWriteLock(&afs_icl_lock, 183);
    if (!afs_icl_inited)
	afs_icl_Init();

    for (logp = afs_icl_allLogs; logp; logp = logp->nextp) {
	if (strcmp(logp->name, name) == 0) {
	    /* found it already created, just return it */
	    logp->refCount++;
	    *outLogpp = logp;
	    if (flags & ICL_CRLOG_FLAG_PERSISTENT) {
		ObtainWriteLock(&logp->lock, 184);
		logp->states |= ICL_LOGF_PERSISTENT;
		ReleaseWriteLock(&logp->lock);
	    }
	    ReleaseWriteLock(&afs_icl_lock);
	    return 0;
	}
    }

    logp = (struct afs_icl_log *)
	osi_AllocSmallSpace(sizeof(struct afs_icl_log));
    memset((caddr_t) logp, 0, sizeof(*logp));

    logp->refCount = 1;
    logp->name = osi_AllocSmallSpace(strlen(name) + 1);
    strcpy(logp->name, name);
    LOCK_INIT(&logp->lock, "logp lock");
    logp->logSize = logSize;
    logp->datap = NULL;		/* don't allocate it until we need it */

    if (flags & ICL_CRLOG_FLAG_PERSISTENT)
	logp->states |= ICL_LOGF_PERSISTENT;

    logp->nextp = afs_icl_allLogs;
    afs_icl_allLogs = logp;
    ReleaseWriteLock(&afs_icl_lock);

    *outLogpp = logp;
    return 0;
}

/* called with a log, a pointer to a buffer, the size of the buffer
 * (in *bufSizep), the starting cookie (in *cookiep, use 0 at the start)
 * and returns data in the provided buffer, and returns output flags
 * in *flagsp.  The flag ICL_COPYOUTF_MISSEDSOME is set if we can't
 * find the record with cookie value cookie.
 */
int
afs_icl_CopyOut(register struct afs_icl_log *logp, afs_int32 * bufferp,
		afs_int32 * bufSizep, afs_uint32 * cookiep,
		afs_int32 * flagsp)
{
    afs_int32 nwords;		/* number of words to copy out */
    afs_uint32 startCookie;	/* first cookie to use */
    afs_int32 outWords;		/* words we've copied out */
    afs_int32 inWords;		/* max words to copy out */
    afs_int32 code;		/* return code */
    afs_int32 ix;		/* index we're copying from */
    afs_int32 outFlags;		/* return flags */
    afs_int32 inFlags;		/* flags passed in */
    afs_int32 end;

    inWords = *bufSizep;	/* max to copy out */
    outWords = 0;		/* amount copied out */
    startCookie = *cookiep;
    outFlags = 0;
    inFlags = *flagsp;
    code = 0;

    ObtainWriteLock(&logp->lock, 185);
    if (!logp->datap) {
	ReleaseWriteLock(&logp->lock);
	goto done;
    }

    /* first, compute the index of the start cookie we've been passed */
    while (1) {
	/* (re-)compute where we should start */
	if (startCookie < logp->baseCookie) {
	    if (startCookie)	/* missed some output */
		outFlags |= ICL_COPYOUTF_MISSEDSOME;
	    /* skip to the first available record */
	    startCookie = logp->baseCookie;
	    *cookiep = startCookie;
	}

	/* compute where we find the first element to copy out */
	ix = logp->firstUsed + startCookie - logp->baseCookie;
	if (ix >= logp->logSize)
	    ix -= logp->logSize;

	/* if have some data now, break out and process it */
	if (startCookie - logp->baseCookie < logp->logElements)
	    break;

	/* At end of log, so clear it if we need to */
	if (inFlags & ICL_COPYOUTF_CLRAFTERREAD) {
	    logp->firstUsed = logp->firstFree = 0;
	    logp->logElements = 0;
	}
	/* otherwise, either wait for the data to arrive, or return */
	if (!(inFlags & ICL_COPYOUTF_WAITIO)) {
	    ReleaseWriteLock(&logp->lock);
	    code = 0;
	    goto done;
	}
	logp->states |= ICL_LOGF_WAITING;
	ReleaseWriteLock(&logp->lock);
	afs_osi_Sleep(&logp->lock);
	ObtainWriteLock(&logp->lock, 186);
    }
    /* copy out data from ix to logSize or firstFree, depending
     * upon whether firstUsed <= firstFree (no wrap) or otherwise.
     * be careful not to copy out more than nwords.
     */
    if (ix >= logp->firstUsed) {
	if (logp->firstUsed <= logp->firstFree)
	    /* no wrapping */
	    end = logp->firstFree;	/* first element not to copy */
	else
	    end = logp->logSize;
	nwords = inWords;	/* don't copy more than this */
	if (end - ix < nwords)
	    nwords = end - ix;
	if (nwords > 0) {
	    memcpy((char *)bufferp, (char *)&logp->datap[ix],
		   sizeof(afs_int32) * nwords);
	    outWords += nwords;
	    inWords -= nwords;
	    bufferp += nwords;
	}
	/* if we're going to copy more out below, we'll start here */
	ix = 0;
    }
    /* now, if active part of the log has wrapped, there's more stuff
     * starting at the head of the log.  Copy out more from there.
     */
    if (logp->firstUsed > logp->firstFree && ix < logp->firstFree
	&& inWords > 0) {
	/* (more to) copy out from the wrapped section at the
	 * start of the log.  May get here even if didn't copy any
	 * above, if the cookie points directly into the wrapped section.
	 */
	nwords = inWords;
	if (logp->firstFree - ix < nwords)
	    nwords = logp->firstFree - ix;
	memcpy((char *)bufferp, (char *)&logp->datap[ix],
	       sizeof(afs_int32) * nwords);
	outWords += nwords;
	inWords -= nwords;
	bufferp += nwords;
    }

    ReleaseWriteLock(&logp->lock);

  done:
    if (code == 0) {
	*bufSizep = outWords;
	*flagsp = outFlags;
    }
    return code;
}

/* return basic parameter information about a log */
int
afs_icl_GetLogParms(struct afs_icl_log *logp, afs_int32 * maxSizep,
		    afs_int32 * curSizep)
{
    ObtainReadLock(&logp->lock);
    *maxSizep = logp->logSize;
    *curSizep = logp->logElements;
    ReleaseReadLock(&logp->lock);
    return 0;
}


/* hold and release logs */
int
afs_icl_LogHold(register struct afs_icl_log *logp)
{
    ObtainWriteLock(&afs_icl_lock, 187);
    logp->refCount++;
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* hold and release logs, called with lock already held */
int
afs_icl_LogHoldNL(register struct afs_icl_log *logp)
{
    logp->refCount++;
    return 0;
}

/* keep track of how many sets believe the log itself is allocated */
int
afs_icl_LogUse(register struct afs_icl_log *logp)
{
    ObtainWriteLock(&logp->lock, 188);
    if (logp->setCount == 0) {
	/* this is the first set actually using the log -- allocate it */
	if (logp->logSize == 0) {
	    /* we weren't passed in a hint and it wasn't set */
	    logp->logSize = ICL_DEFAULT_LOGSIZE;
	}
	logp->datap =
	    (afs_int32 *) afs_osi_Alloc(sizeof(afs_int32) * logp->logSize);
#ifdef	KERNEL_HAVE_PIN
	pin((char *)logp->datap, sizeof(afs_int32) * logp->logSize);
#endif
    }
    logp->setCount++;
    ReleaseWriteLock(&logp->lock);
    return 0;
}

/* decrement the number of real users of the log, free if possible */
int
afs_icl_LogFreeUse(register struct afs_icl_log *logp)
{
    ObtainWriteLock(&logp->lock, 189);
    if (--logp->setCount == 0) {
	/* no more users -- free it (but keep log structure around) */
#ifdef	KERNEL_HAVE_PIN
	unpin((char *)logp->datap, sizeof(afs_int32) * logp->logSize);
#endif
	afs_osi_Free(logp->datap, sizeof(afs_int32) * logp->logSize);
	logp->firstUsed = logp->firstFree = 0;
	logp->logElements = 0;
	logp->datap = NULL;
    }
    ReleaseWriteLock(&logp->lock);
    return 0;
}

/* set the size of the log to 'logSize' */
int
afs_icl_LogSetSize(register struct afs_icl_log *logp, afs_int32 logSize)
{
    ObtainWriteLock(&logp->lock, 190);
    if (!logp->datap) {
	/* nothing to worry about since it's not allocated */
	logp->logSize = logSize;
    } else {
	/* reset log */
	logp->firstUsed = logp->firstFree = 0;
	logp->logElements = 0;

	/* free and allocate a new one */
#ifdef	KERNEL_HAVE_PIN
	unpin((char *)logp->datap, sizeof(afs_int32) * logp->logSize);
#endif
	afs_osi_Free(logp->datap, sizeof(afs_int32) * logp->logSize);
	logp->datap =
	    (afs_int32 *) afs_osi_Alloc(sizeof(afs_int32) * logSize);
#ifdef	KERNEL_HAVE_PIN
	pin((char *)logp->datap, sizeof(afs_int32) * logSize);
#endif
	logp->logSize = logSize;
    }
    ReleaseWriteLock(&logp->lock);

    return 0;
}

/* free a log.  Called with afs_icl_lock locked. */
int
afs_icl_ZapLog(register struct afs_icl_log *logp)
{
    register struct afs_icl_log **lpp, *tp;

    for (lpp = &afs_icl_allLogs, tp = *lpp; tp; lpp = &tp->nextp, tp = *lpp) {
	if (tp == logp) {
	    /* found the dude we want to remove */
	    *lpp = logp->nextp;
	    osi_FreeSmallSpace(logp->name);
#ifdef KERNEL_HAVE_PIN
	    unpin((char *)logp->datap, sizeof(afs_int32) * logp->logSize);
#endif
	    afs_osi_Free(logp->datap, sizeof(afs_int32) * logp->logSize);
	    osi_FreeSmallSpace(logp);
	    break;		/* won't find it twice */
	}
    }
    return 0;
}

/* do the release, watching for deleted entries */
int
afs_icl_LogRele(register struct afs_icl_log *logp)
{
    ObtainWriteLock(&afs_icl_lock, 191);
    if (--logp->refCount == 0 && (logp->states & ICL_LOGF_DELETED)) {
	afs_icl_ZapLog(logp);	/* destroys logp's lock! */
    }
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* do the release, watching for deleted entries, log already held */
int
afs_icl_LogReleNL(register struct afs_icl_log *logp)
{
    if (--logp->refCount == 0 && (logp->states & ICL_LOGF_DELETED)) {
	afs_icl_ZapLog(logp);	/* destroys logp's lock! */
    }
    return 0;
}

/* zero out the log */
int
afs_icl_ZeroLog(register struct afs_icl_log *logp)
{
    ObtainWriteLock(&logp->lock, 192);
    logp->firstUsed = logp->firstFree = 0;
    logp->logElements = 0;
    logp->baseCookie = 0;
    ReleaseWriteLock(&logp->lock);
    return 0;
}

/* free a log entry, and drop its reference count */
int
afs_icl_LogFree(register struct afs_icl_log *logp)
{
    ObtainWriteLock(&logp->lock, 193);
    logp->states |= ICL_LOGF_DELETED;
    ReleaseWriteLock(&logp->lock);
    afs_icl_LogRele(logp);
    return 0;
}

/* find a log by name, returning it held */
struct afs_icl_log *
afs_icl_FindLog(char *name)
{
    register struct afs_icl_log *tp;
    ObtainWriteLock(&afs_icl_lock, 194);
    for (tp = afs_icl_allLogs; tp; tp = tp->nextp) {
	if (strcmp(tp->name, name) == 0) {
	    /* this is the dude we want */
	    tp->refCount++;
	    break;
	}
    }
    ReleaseWriteLock(&afs_icl_lock);
    return tp;
}

int
afs_icl_EnumerateLogs(int (*aproc)
		        (char *name, char *arock, struct afs_icl_log * tp),
		      char *arock)
{
    register struct afs_icl_log *tp;
    register afs_int32 code;

    code = 0;
    ObtainWriteLock(&afs_icl_lock, 195);
    for (tp = afs_icl_allLogs; tp; tp = tp->nextp) {
	tp->refCount++;		/* hold this guy */
	ReleaseWriteLock(&afs_icl_lock);
	ObtainReadLock(&tp->lock);
	code = (*aproc) (tp->name, arock, tp);
	ReleaseReadLock(&tp->lock);
	ObtainWriteLock(&afs_icl_lock, 196);
	if (--tp->refCount == 0)
	    afs_icl_ZapLog(tp);
	if (code)
	    break;
    }
    ReleaseWriteLock(&afs_icl_lock);
    return code;
}

struct afs_icl_set *afs_icl_allSets = 0;

int
afs_icl_CreateSet(char *name, struct afs_icl_log *baseLogp,
		  struct afs_icl_log *fatalLogp,
		  struct afs_icl_set **outSetpp)
{
    return afs_icl_CreateSetWithFlags(name, baseLogp, fatalLogp,
				      /*flags */ 0, outSetpp);
}

/* create a set, given pointers to base and fatal logs, if any.
 * Logs are unlocked, but referenced, and *outSetpp is returned
 * referenced.  Function bumps reference count on logs, since it
 * addds references from the new afs_icl_set.  When the set is destroyed,
 * those references will be released.
 */
int
afs_icl_CreateSetWithFlags(char *name, struct afs_icl_log *baseLogp,
			   struct afs_icl_log *fatalLogp, afs_uint32 flags,
			   struct afs_icl_set **outSetpp)
{
    register struct afs_icl_set *setp;
    register int i;
    afs_int32 states = ICL_DEFAULT_SET_STATES;

    ObtainWriteLock(&afs_icl_lock, 197);
    if (!afs_icl_inited)
	afs_icl_Init();

    for (setp = afs_icl_allSets; setp; setp = setp->nextp) {
	if (strcmp(setp->name, name) == 0) {
	    setp->refCount++;
	    *outSetpp = setp;
	    if (flags & ICL_CRSET_FLAG_PERSISTENT) {
		ObtainWriteLock(&setp->lock, 198);
		setp->states |= ICL_SETF_PERSISTENT;
		ReleaseWriteLock(&setp->lock);
	    }
	    ReleaseWriteLock(&afs_icl_lock);
	    return 0;
	}
    }

    /* determine initial state */
    if (flags & ICL_CRSET_FLAG_DEFAULT_ON)
	states = ICL_SETF_ACTIVE;
    else if (flags & ICL_CRSET_FLAG_DEFAULT_OFF)
	states = ICL_SETF_FREED;
    if (flags & ICL_CRSET_FLAG_PERSISTENT)
	states |= ICL_SETF_PERSISTENT;

    setp = (struct afs_icl_set *)afs_osi_Alloc(sizeof(struct afs_icl_set));
    memset((caddr_t) setp, 0, sizeof(*setp));
    setp->refCount = 1;
    if (states & ICL_SETF_FREED)
	states &= ~ICL_SETF_ACTIVE;	/* if freed, can't be active */
    setp->states = states;

    LOCK_INIT(&setp->lock, "setp lock");
    /* next lock is obtained in wrong order, hierarchy-wise, but
     * it doesn't matter, since no one can find this lock yet, since
     * the afs_icl_lock is still held, and thus the obtain can't block.
     */
    ObtainWriteLock(&setp->lock, 199);
    setp->name = osi_AllocSmallSpace(strlen(name) + 1);
    strcpy(setp->name, name);
    setp->nevents = ICL_DEFAULTEVENTS;
    setp->eventFlags = afs_osi_Alloc(ICL_DEFAULTEVENTS);
#ifdef	KERNEL_HAVE_PIN
    pin((char *)setp->eventFlags, ICL_DEFAULTEVENTS);
#endif
    for (i = 0; i < ICL_DEFAULTEVENTS; i++)
	setp->eventFlags[i] = 0xff;	/* default to enabled */

    /* update this global info under the afs_icl_lock */
    setp->nextp = afs_icl_allSets;
    afs_icl_allSets = setp;
    ReleaseWriteLock(&afs_icl_lock);

    /* set's basic lock is still held, so we can finish init */
    if (baseLogp) {
	setp->logs[0] = baseLogp;
	afs_icl_LogHold(baseLogp);
	if (!(setp->states & ICL_SETF_FREED))
	    afs_icl_LogUse(baseLogp);	/* log is actually being used */
    }
    if (fatalLogp) {
	setp->logs[1] = fatalLogp;
	afs_icl_LogHold(fatalLogp);
	if (!(setp->states & ICL_SETF_FREED))
	    afs_icl_LogUse(fatalLogp);	/* log is actually being used */
    }
    ReleaseWriteLock(&setp->lock);

    *outSetpp = setp;
    return 0;
}

/* function to change event enabling information for a particular set */
int
afs_icl_SetEnable(struct afs_icl_set *setp, afs_int32 eventID, int setValue)
{
    char *tp;

    ObtainWriteLock(&setp->lock, 200);
    if (!ICL_EVENTOK(setp, eventID)) {
	ReleaseWriteLock(&setp->lock);
	return -1;
    }
    tp = &setp->eventFlags[ICL_EVENTBYTE(eventID)];
    if (setValue)
	*tp |= ICL_EVENTMASK(eventID);
    else
	*tp &= ~(ICL_EVENTMASK(eventID));
    ReleaseWriteLock(&setp->lock);
    return 0;
}

/* return indication of whether a particular event ID is enabled
 * for tracing.  If *getValuep is set to 0, the event is disabled,
 * otherwise it is enabled.  All events start out enabled by default.
 */
int
afs_icl_GetEnable(struct afs_icl_set *setp, afs_int32 eventID, int *getValuep)
{
    ObtainReadLock(&setp->lock);
    if (!ICL_EVENTOK(setp, eventID)) {
	ReleaseWriteLock(&setp->lock);
	return -1;
    }
    if (setp->eventFlags[ICL_EVENTBYTE(eventID)] & ICL_EVENTMASK(eventID))
	*getValuep = 1;
    else
	*getValuep = 0;
    ReleaseReadLock(&setp->lock);
    return 0;
}

/* hold and release event sets */
int
afs_icl_SetHold(register struct afs_icl_set *setp)
{
    ObtainWriteLock(&afs_icl_lock, 201);
    setp->refCount++;
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* free a set.  Called with afs_icl_lock locked */
int
afs_icl_ZapSet(register struct afs_icl_set *setp)
{
    register struct afs_icl_set **lpp, *tp;
    int i;
    register struct afs_icl_log *tlp;

    for (lpp = &afs_icl_allSets, tp = *lpp; tp; lpp = &tp->nextp, tp = *lpp) {
	if (tp == setp) {
	    /* found the dude we want to remove */
	    *lpp = setp->nextp;
	    osi_FreeSmallSpace(setp->name);
#ifdef	KERNEL_HAVE_PIN
	    unpin((char *)setp->eventFlags, ICL_DEFAULTEVENTS);
#endif
	    afs_osi_Free(setp->eventFlags, ICL_DEFAULTEVENTS);
	    for (i = 0; i < ICL_LOGSPERSET; i++) {
		if ((tlp = setp->logs[i]))
		    afs_icl_LogReleNL(tlp);
	    }
	    osi_FreeSmallSpace(setp);
	    break;		/* won't find it twice */
	}
    }
    return 0;
}

/* do the release, watching for deleted entries */
int
afs_icl_SetRele(register struct afs_icl_set *setp)
{
    ObtainWriteLock(&afs_icl_lock, 202);
    if (--setp->refCount == 0 && (setp->states & ICL_SETF_DELETED)) {
	afs_icl_ZapSet(setp);	/* destroys setp's lock! */
    }
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* free a set entry, dropping its reference count */
int
afs_icl_SetFree(register struct afs_icl_set *setp)
{
    ObtainWriteLock(&setp->lock, 203);
    setp->states |= ICL_SETF_DELETED;
    ReleaseWriteLock(&setp->lock);
    afs_icl_SetRele(setp);
    return 0;
}

/* find a set by name, returning it held */
struct afs_icl_set *
afs_icl_FindSet(char *name)
{
    register struct afs_icl_set *tp;
    ObtainWriteLock(&afs_icl_lock, 204);
    for (tp = afs_icl_allSets; tp; tp = tp->nextp) {
	if (strcmp(tp->name, name) == 0) {
	    /* this is the dude we want */
	    tp->refCount++;
	    break;
	}
    }
    ReleaseWriteLock(&afs_icl_lock);
    return tp;
}

/* zero out all the logs in the set */
int
afs_icl_ZeroSet(struct afs_icl_set *setp)
{
    register int i;
    int code = 0;
    int tcode;
    struct afs_icl_log *logp;

    ObtainReadLock(&setp->lock);
    for (i = 0; i < ICL_LOGSPERSET; i++) {
	logp = setp->logs[i];
	if (logp) {
	    afs_icl_LogHold(logp);
	    tcode = afs_icl_ZeroLog(logp);
	    if (tcode != 0)
		code = tcode;	/* save the last bad one */
	    afs_icl_LogRele(logp);
	}
    }
    ReleaseReadLock(&setp->lock);
    return code;
}

int
afs_icl_EnumerateSets(int (*aproc)
		        (char *name, char *arock, struct afs_icl_log * tp),
		      char *arock)
{
    register struct afs_icl_set *tp, *np;
    register afs_int32 code;

    code = 0;
    ObtainWriteLock(&afs_icl_lock, 205);
    for (tp = afs_icl_allSets; tp; tp = np) {
	tp->refCount++;		/* hold this guy */
	ReleaseWriteLock(&afs_icl_lock);
	code = (*aproc) (tp->name, arock, (struct afs_icl_log *)tp);
	ObtainWriteLock(&afs_icl_lock, 206);
	np = tp->nextp;		/* tp may disappear next, but not np */
	if (--tp->refCount == 0 && (tp->states & ICL_SETF_DELETED))
	    afs_icl_ZapSet(tp);
	if (code)
	    break;
    }
    ReleaseWriteLock(&afs_icl_lock);
    return code;
}

int
afs_icl_AddLogToSet(struct afs_icl_set *setp, struct afs_icl_log *newlogp)
{
    register int i;
    int code = -1;

    ObtainWriteLock(&setp->lock, 207);
    for (i = 0; i < ICL_LOGSPERSET; i++) {
	if (!setp->logs[i]) {
	    setp->logs[i] = newlogp;
	    code = i;
	    afs_icl_LogHold(newlogp);
	    if (!(setp->states & ICL_SETF_FREED)) {
		/* bump up the number of sets using the log */
		afs_icl_LogUse(newlogp);
	    }
	    break;
	}
    }
    ReleaseWriteLock(&setp->lock);
    return code;
}

int
afs_icl_SetSetStat(struct afs_icl_set *setp, int op)
{
    int i;
    afs_int32 code;
    struct afs_icl_log *logp;

    ObtainWriteLock(&setp->lock, 208);
    switch (op) {
    case ICL_OP_SS_ACTIVATE:	/* activate a log */
	/*
	 * If we are not already active, see if we have released
	 * our demand that the log be allocated (FREED set).  If
	 * we have, reassert our desire.
	 */
	if (!(setp->states & ICL_SETF_ACTIVE)) {
	    if (setp->states & ICL_SETF_FREED) {
		/* have to reassert desire for logs */
		for (i = 0; i < ICL_LOGSPERSET; i++) {
		    logp = setp->logs[i];
		    if (logp) {
			afs_icl_LogHold(logp);
			afs_icl_LogUse(logp);
			afs_icl_LogRele(logp);
		    }
		}
		setp->states &= ~ICL_SETF_FREED;
	    }
	    setp->states |= ICL_SETF_ACTIVE;
	}
	code = 0;
	break;

    case ICL_OP_SS_DEACTIVATE:	/* deactivate a log */
	/* this doesn't require anything beyond clearing the ACTIVE flag */
	setp->states &= ~ICL_SETF_ACTIVE;
	code = 0;
	break;

    case ICL_OP_SS_FREE:	/* deassert design for log */
	/* 
	 * if we are already in this state, do nothing; otherwise
	 * deassert desire for log
	 */
	if (setp->states & ICL_SETF_ACTIVE)
	    code = EINVAL;
	else {
	    if (!(setp->states & ICL_SETF_FREED)) {
		for (i = 0; i < ICL_LOGSPERSET; i++) {
		    logp = setp->logs[i];
		    if (logp) {
			afs_icl_LogHold(logp);
			afs_icl_LogFreeUse(logp);
			afs_icl_LogRele(logp);
		    }
		}
		setp->states |= ICL_SETF_FREED;
	    }
	    code = 0;
	}
	break;

    default:
	code = EINVAL;
    }
    ReleaseWriteLock(&setp->lock);
    return code;
}
