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

/* This header provides routines for dealing with the 100ns based AFS
 * time type. We hide the actual variable behind a structure, so that
 * attempts to do
 *
 *     time_t ourTime;
 *     opr_time theirTime;
 *
 *     ourTime = theirTime;
 *
 * will be caught by the compiler.
 */

#ifndef OPENAFS_OPR_TIME_H
#define OPENAFS_OPR_TIME_H

struct opr_time {
    afs_int64 time;
};

#define OPR_TIME_INIT {0LL}

static_inline void
opr_time_FromSecs(struct opr_time *out, time_t in)
{
    out->time = ((afs_int64)in) * 10000000;
}

static_inline time_t
opr_time_ToSecs(struct opr_time *in)
{
    return in->time/10000000;;
}

static_inline void
opr_time_FromMsecs(struct opr_time *out, int msecs)
{
    out->time = ((afs_int64)msecs) * 10000;
}

static_inline int
opr_time_ToMsecs(struct opr_time *in)
{
    return in->time/10000;
}

static_inline void
opr_time_FromTimeval(struct opr_time *out, struct timeval *in)
{
    out->time = ((afs_int64)in->tv_sec) * 10000000 + in->tv_usec * 10;
}

static_inline void
opr_time_ToTimeval(struct opr_time *in, struct timeval *out)
{
    out->tv_sec = in->time / 10000000;
    out->tv_usec = (in->time / 10) % 1000000;
}

static_inline int
opr_time_Now(struct opr_time *out)
{
    struct timeval tv;
    int code;

    code = gettimeofday(&tv, NULL);
    if (code == 0)
	opr_time_FromTimeval(out, &tv);

    return code;
}

static_inline int
opr_time_GreaterThan(struct opr_time *t1, struct opr_time *t2)
{
    return t1->time > t2->time;
}

static_inline int
opr_time_LessThan(struct opr_time *t1, struct opr_time *t2)
{
    return t1->time < t2->time;
}

static_inline void
opr_time_Add(struct opr_time *t1, struct opr_time *t2)
{
    t1->time += t2->time;
}

static_inline void
opr_time_Sub(struct opr_time *t1, struct opr_time *t2)
{
    t1->time -= t2->time;
}

static_inline void
opr_time_AddMsec(struct opr_time *t1, int msec)
{
    struct opr_time t2;

    opr_time_FromMsecs(&t2, msec);
    opr_time_Add(t1, &t2);
}

#endif
