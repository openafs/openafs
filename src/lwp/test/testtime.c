/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/time.h>

main()
{
    struct timeval blat[10], notmuch;
    unsigned int approx[3];
    int i;
    approx[0] = FT_ApproxTime();
    notmuch.tv_sec = 0;
    notmuch.tv_usec = 300000;
    for (i = 0; i < 5; i++) {
	FT_GetTimeOfDay(&blat[i], 0);
	select(0, 0, 0, 0, &notmuch);
    }
    sleep(5);
    approx[1] = FT_ApproxTime();
    for (i = 5; i < 10; i++)
	FT_GetTimeOfDay(&blat[i], 0);
    sleep(5);
    approx[2] = FT_ApproxTime();
    for (i = 0; i < 10; i++)
	printf("%u.%u\n", blat[i].tv_sec, blat[i].tv_usec);
    printf("\n%u %u %u\n", approx[0], approx[1], approx[2]);
}
