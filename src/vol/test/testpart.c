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

RCSID("$Header: /tmp/cvstemp/openafs/src/vol/test/testpart.c,v 1.1.1.4 2001/07/14 22:25:06 hartmans Exp $");

#include <stdio.h>

Log(a, b, c, d, e, f, g, h, i, j, k)
{
printf(a,b,c,d,e,f,g,h,i,j,k);
}

iopen() {}

AssertionFailed()
{
printf("assertion failed\n");
exit(-1);
}

Abort()
{
printf("Aborting\n");
exit(-1);
}

main(argc, argv)
int argc;
char **argv; {
    VInitVolumePackage(1, 0, 0, 0, 0);
    VPrintDiskStats();

}

