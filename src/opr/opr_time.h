/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
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
