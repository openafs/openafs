/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This header provides routines for dealing with the 100ns-based 64-bit AFS
 * time type, afs_time64. With this type, time is represented in units of
 * 100ns, which we call "ticks". Absolute time is the same as with unix time
 * (time since the unix epoch of 1 Jan 1970 UTC, skipping leap seconds), except
 * represented in ticks instead of seconds. The count of ticks is always
 * recorded in a signed 64-bit integer.
 *
 * The actual integer is hidden inside a structure, so that accidental
 * assignment like this will fail during compilation:
 *
 *     time_t ourTime;
 *     struct afs_time64 theirTime;
 *
 *     ourTime = theirTime;
 *
 * Any callers can still easily access the underlying raw int64 by just looking
 * at "theirTime.ticks" (but you generally should not do so; use the accessor
 * functions in this header instead).
 *
 * A note on naming:
 *
 * Using 100ns units is a common way to represent time, although there is no
 * universal standard term for the unit. We use the term "tick" for
 * convenience, since otherwise it is awkward to refer to "100ns units"
 * everywhere. Other systems use various terms for the unit:
 *
 * - Some Win32 and .NET interfaces use the term "tick" (such as
 * DateTime.Ticks). However, other interfaces use the term "tick" to mean
 * something else (such as Environment.TickCount, which uses 1ms ticks).
 *
 * - The D language and the reference implementation of NTP use the term
 * "hectonanoseconds", abbreviated to "hnsecs" or "hns", and sometimes referred
 * to as "hundreds of nanoseconds" with the same abbreviation.
 *
 * - VMS used the term "clunks" informally.
 */

#ifndef OPENAFS_OPR_TIME_H
#define OPENAFS_OPR_TIME_H

#include <afs/opr.h>
#if defined(KERNEL) && !defined(UKERNEL)
# include "afs/sysincludes.h"
#else
# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# if !defined(AFS_NT40_ENV)
#  include <sys/time.h>
# endif
# include <time.h>
#endif

#define OPR_TIME64_TICKS_PER_US    (10LL)
#define OPR_TIME64_TICKS_PER_MS    (OPR_TIME64_TICKS_PER_US * 1000LL)
#define OPR_TIME64_TICKS_PER_SEC   (OPR_TIME64_TICKS_PER_MS * 1000LL)

#define OPR_TIME64_MAX_SECS  (922337203685LL)
#define OPR_TIME64_MIN_SECS (-922337203685LL)

#define OPR_TIME64_MAX_MICROSECS  (922337203685477580LL)
#define OPR_TIME64_MIN_MICROSECS (-922337203685477580LL)

#define OPR_TIME64_MAX_TICKS (0x7FFFFFFFFFFFFFFFLL)
#define OPR_TIME64_MIN_TICKS (-OPR_TIME64_MAX_TICKS - 1)

static_inline int
opr_time64_cmp(struct afs_time64 t1, struct afs_time64 t2)
{
    if (t1.ticks > t2.ticks) {
	return 1;
    }
    if (t1.ticks < t2.ticks) {
	return -1;
    }
    return 0;
}

static_inline int
opr_time64_lt(struct afs_time64 t1, struct afs_time64 t2)
{
    return opr_time64_cmp(t1, t2) < 0;
}

static_inline int
opr_time64_lteq(struct afs_time64 t1, struct afs_time64 t2)
{
    return opr_time64_cmp(t1, t2) <= 0;
}

static_inline int
opr_time64_gt(struct afs_time64 t1, struct afs_time64 t2)
{
    return opr_time64_cmp(t1, t2) > 0;
}

static_inline int
opr_time64_gteq(struct afs_time64 t1, struct afs_time64 t2)
{
    return opr_time64_cmp(t1, t2) >= 0;
}
static_inline int
opr_time64_eq(struct afs_time64 t1, struct afs_time64 t2)
{
    return opr_time64_cmp(t1, t2) == 0;
}

/*
 * *out = in + add
 *
 * If the result cannot be represented, return ERANGE.
 */
static_inline int
opr_time64_add_safe(struct afs_time64 in, struct afs_time64 add,
		    struct afs_time64 *out)
{
    if (in.ticks > 0 && add.ticks > 0) {
	if (in.ticks > OPR_TIME64_MAX_TICKS - add.ticks) {
	    return ERANGE;
	}
    }
    if (in.ticks < 0 && add.ticks < 0) {
	if (in.ticks < OPR_TIME64_MIN_TICKS - add.ticks) {
	    return ERANGE;
	}
    }
    out->ticks = in.ticks + add.ticks;
    return 0;
}

/*
 * *out = in - sub
 *
 * If the result cannot be represented, return ERANGE.
 */
static_inline int
opr_time64_sub_safe(struct afs_time64 in, struct afs_time64 sub,
		    struct afs_time64 *out)
{
    if (in.ticks >= 0 && sub.ticks < 0) {
	if (in.ticks > OPR_TIME64_MAX_TICKS + sub.ticks) {
	    return ERANGE;
	}
    }
    if (in.ticks < 0 && sub.ticks > 0) {
	if (in.ticks < OPR_TIME64_MIN_TICKS + sub.ticks) {
	    return ERANGE;
	}
    }
    out->ticks = in.ticks - sub.ticks;
    return 0;
}

/*
 * Same as opr_time64_sub_safe(), but we return the result directly. If the
 * result cannot be represented, we assert instead of returning an error.
 * Because we can assert, do NOT use this function with untrusted data! That
 * is, data from the net or user input. Use opr_time64_sub_safe() instead.
 *
 * It's generally okay to use this function if you're subtracting known small
 * values (for example, subtracting a hard-coded 5 seconds from
 * opr_time64_now()); since if that overflows an afs_time64, then probably
 * nothing is going to be able to function.
 */
static_inline struct afs_time64
opr_time64_sub(struct afs_time64 in, struct afs_time64 sub)
{
    struct afs_time64 val;
    opr_Verify(opr_time64_sub_safe(in, sub, &val) == 0);
    return val;
}

/*
 * Initialize an afs_time64 from the given number of ticks. This should not
 * usually be necessary, except when decoding an afs_time64 from the net or
 * from disk, etc. It is preferred to use this function instead of setting an
 * afs_time64's ticks directly, to make it easier to track who is doing this if
 * needed.
 */
static_inline struct afs_time64
opr_time64_fromTicks(afs_int64 ticks)
{
    struct afs_time64 val;
    val.ticks = ticks;
    return val;
}

/*
 * Initialize an afs_time64 from the given number of seconds. If the result
 * cannot be represented as an afs_time64, return ERANGE.
 */
static_inline int
opr_time64_fromSecs_safe(afs_int64 in, struct afs_time64 *out)
{
    if (in < OPR_TIME64_MIN_SECS || in > OPR_TIME64_MAX_SECS) {
	return ERANGE;
    }
    out->ticks = in * OPR_TIME64_TICKS_PER_SEC;
    return 0;
}

/*
 * Same as opr_time64_fromSecs_safe(), but asserts on error. Do NOT use with
 * untrusted data!
 */
static_inline struct afs_time64
opr_time64_fromSecs(afs_int64 in)
{
    struct afs_time64 val;
    opr_Verify(opr_time64_fromSecs_safe(in, &val) == 0);
    return val;
}

static_inline int
opr_time64_fromMicrosecs_safe(afs_int64 in, struct afs_time64 *out)
{
    if (in < OPR_TIME64_MIN_MICROSECS || in > OPR_TIME64_MAX_MICROSECS) {
	return ERANGE;
    }
    out->ticks = in * OPR_TIME64_TICKS_PER_US;
    return 0;
}

/*
 * Similar to opr_time64_fromSecs_safe(), but the given time is given in
 * seconds and microseconds.
 *
 * This doesn't take a struct timeval directly for convenience when we're
 * dealing with timeval-like structs that aren't literally struct timeval
 * (e.g., struct rx_clock, or some KERNEL environments).
 */
static_inline int
opr_time64_fromTimeval_safe(afs_int64 sec, afs_int64 microsec,
			    struct afs_time64 *out)
{
    int code;
    struct afs_time64 val;
    struct afs_time64 val_usec;

    code = opr_time64_fromSecs_safe(sec, &val);
    if (code != 0) {
	return code;
    }

    code = opr_time64_fromMicrosecs_safe(microsec, &val_usec);
    if (code != 0) {
	return code;
    }

    return opr_time64_add_safe(val, val_usec, out);
}

/*
 * Get the raw 'ticks' value from the given afs_time64. This should not
 * usually be necessary, but it is preferred to use this over directly
 * referencing the 'ticks' field, to make it easier to track who is using this
 * and make it less likely to accidentally modify the 'ticks' field.
 */
static_inline afs_int64
opr_time64_toTicks(struct afs_time64 in)
{
    return in.ticks;
}

/* 'long long' version of opr_time64_toTicks() for printf() convenience. */
static_inline long long
opr_time64_toTicksLL(struct afs_time64 in)
{
    return opr_time64_toTicks(in);
}

/*
 * Convert the given afs_time64 time into whole seconds. This does not return
 * errors, but is "safer" than opr_time64_toSecs() because the output argument
 * prevents us from accidentally truncating the result in a 32-bit int.
 */
static_inline void
opr_time64_toSecs_safe(struct afs_time64 in, afs_int64 *out)
{
    *out = in.ticks / OPR_TIME64_TICKS_PER_SEC;
}

/*
 * More convenient form of opr_time64_toSecs_safe(), when we're sure the
 * returned value will not be truncated and using opr_time64_toSecs_safe() is
 * very cumbersome.
 *
 * In other words, this is okay:
 *
 *     if (opr_time64_toSecs(elapsed) > 5)
 *
 * But this is not, since the return value will be truncated:
 *
 *     afs_int32 secs = opr_time64_toSecs(elapsed);
 *
 * Try to use opr_time64_toSecs_safe() where possible instead.
 */
static_inline afs_int64
opr_time64_toSecs(struct afs_time64 in)
{
    afs_int64 val;
    opr_time64_toSecs_safe(in, &val);
    return val;
}

/*
 * Same as opr_time64_toSecs_safe(), but converts the time into seconds
 * represented by an afs_uint32. If the result cannot be represented as an
 * afs_uint32, the returned value wraps around.
 *
 * That is: 2^32   -> 0
 *	    2^32+1 -> 1
 *	    -1	   -> 2^32-1
 */
static_inline void
opr_time64_toUint32_wrap(struct afs_time64 in, afs_uint32 *out)
{
    static const afs_int64 limit = MAX_AFS_UINT32 + 1LL;
    afs_int64 secs;
    opr_time64_toSecs_safe(in, &secs);
    *out = (secs % limit + limit) % limit;
}

#if !defined(KERNEL) || defined(UKERNEL)
/*
 * Version of opr_time64_toSecs() that converts to a system time_t
 * specifically. Only use this when you need to actually use a time_t (for
 * example, when dealing with libc functions).
 */
static_inline void
opr_time64_toTimeT(struct afs_time64 in, time_t *out)
{
    afs_int64 secs;

    opr_time64_toSecs_safe(in, &secs);

# if defined(_AIX) && !defined(__64BIT__) && !defined(_LINUX_SOURCE_COMPAT)
#  warning "time_t appears to be 32 bits; we may not work beyond year 2038"
    /*
     * On AIX, time_t is 32 bits wide unless we are 64-bit, or
     * _LINUX_SOURCE_COMPAT is defined. Let things still compile for now, but
     * in the future this will need to break, since there is no way to function
     * sanely on a system with a 32-bit time_t.
     */
    opr_Assert(secs <= MAX_AFS_INT32);
    opr_Assert(secs >= MIN_AFS_INT32);
# else
    opr_StaticAssert(sizeof(time_t) >= sizeof(afs_int64));
# endif

    *out = secs;
}

/*
 * Similar to ctime(3), but we take an afs_time64, and we trim off the trailing
 * newline of the returned string. If the system ctime(3) call fails, the
 * returned string instead consists of the number of seconds between brackets,
 * for example: "[12345]".
 *
 * Like ctime(3), this returns a pointer to a static buffer, so try not to use
 * this in multithreaded code.
 */
static_inline char *
opr_time64_ctime(struct afs_time64 in)
{
    static char buf[26];
    time_t secs;
    char *str;
    char *nl;

    opr_time64_toTimeT(in, &secs);
    str = ctime(&secs);
    if (str == NULL) {
	snprintf(buf, sizeof(buf), "[%lld]", (long long)secs);
	return buf;
    }

    /* Trim off trailing \n. */
    nl = strchr(str, '\n');
    if (nl != NULL) {
	*nl = '\0';
    }

    return str;
}

static_inline int
opr_time64_now_safe(struct afs_time64 *out)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
	/*
	 * Even for _safe(), don't return an error. The only possible error is
	 * maybe EFAULT; if that happens, that's basically a segfault, so act
	 * like a segfault happened and crash.
	 *
	 * Do not call opr_Verify/opr_Assert, since opr_Assert calls this.
	 */
	opr_abort();
	return EIO;
    }
    return opr_time64_fromTimeval_safe(tv.tv_sec, tv.tv_usec, out);
}

# if defined(__GNUC__) && __GNUC__ <= 4
/*
 * gcc 4.8.8 has been seen to erroneously flag the return value from
 * opr_time64_now() as maybe uninitialized. Work around it by always
 * initializing the return value up front for older gcc, without sacrificing
 * compiler uninitialized warnings on platforms or newer gcc.
 */
#  define WORKAROUND_WUNINITIALIZED
# endif

static_inline struct afs_time64
opr_time64_now(void)
{
    struct afs_time64 now;
# ifdef WORKAROUND_WUNINITIALIZED
    now.ticks = 0;
# endif

    opr_Verify(opr_time64_now_safe(&now) == 0);
    opr_Assert(now.ticks != 0);
    return now;
}
#endif /* !KERNEL || UKERNEL */

/*
 * Same as opr_time64_sub(), but the result is converted to seconds (as a
 * signed 64-bit integer) using opr_time64_toSecs_safe().
 *
 * As with opr_time64_sub(), do NOT use with untrusted data!
 */
static_inline afs_int64
opr_time64_sub_toSecs(struct afs_time64 in, struct afs_time64 sub)
{
    afs_int64 val;
    opr_time64_toSecs_safe(opr_time64_sub(in, sub), &val);
    return val;
}

#if !defined(KERNEL) || defined(UKERNEL)
/*
 * A simple helper for rate-limiting some action. Use like so:
 *
 * static struct afs_time64 lastlog;
 * if (opr_time64_ratelimit(&lastlog, 60)) {
 *     ViceLog(0, ("Some possibly-frequent message.\n"));
 * }
 *
 * This will try to limit the relevant log message to being logged about once
 * per 60 seconds, using 'lastlog' as a place to store the last log time.
 * Protecting 'a_last' from access in other threads is the responsibility of
 * the caller.
 */
static_inline int
opr_time64_ratelimit(struct afs_time64 *a_last, int secs)
{
    struct afs_time64 now = opr_time64_now();
    struct afs_time64 last = *a_last;

    if (opr_time64_lt(now, last) ||
	opr_time64_sub_toSecs(now, last) >= secs) {

	*a_last = now;
	return 1;
    }
    return 0;
}
#endif /* !KERNEL || UKERNEL */

#endif /* OPENAFS_OPR_TIME_H */
