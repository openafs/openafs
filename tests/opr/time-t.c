/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 */

/* Some trivial tests for the very straightforwards OPR 100ns time
 * implementation.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <tests/tap/basic.h>

#include <opr/time.h>

int
main(int argc, char **argv)
{
    struct opr_time oprTime;
    struct timeval osTimeval;
    time_t osTime, osNow;
    plan(4);

    /* Check that FromSecs, then ToSecs results in the same value coming out */

    opr_time_FromSecs(&oprTime, 1337065355);
    osTime = opr_time_ToSecs(&oprTime);
    ok(osTime == 1337065355, "ToSecs(FromSecs(time)) == time");

    /* Check the FromTimeval, then ToTimeval result in the same value. Note that
     * our chosen microseconds field is very close to overflow */

    osTimeval.tv_sec = 1337065355;
    osTimeval.tv_usec = 999;
    opr_time_FromTimeval(&oprTime, &osTimeval);
    opr_time_ToTimeval(&oprTime, &osTimeval);
    ok(osTimeval.tv_sec == 1337065355 && osTimeval.tv_usec == 999,
       "ToTimeval(FromTimeval(timeval) == timeval)");

    /* Check that opr_time_Now looks reasonable */
    is_int(0, opr_time_Now(&oprTime), "opr_time_Now succeeds");
    osNow = time(NULL);
    osTime = opr_time_ToSecs(&oprTime);
    ok(labs(osTime - osNow) < 2, "opr_time_Now returns a reasonable value");

    return 0;
}
