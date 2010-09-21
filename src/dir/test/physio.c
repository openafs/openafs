/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * File		physio.cx
 * NOTE		This is NOT the standard physio.cx for venus or, yet alone, vice.
 *              It is a test one for use in src/dir.
 *
 */

/* First we have the kernel hacks' include files. */
#include <afsconfig.h>
#include <afs/param.h>


#include <sys/param.h>
#ifdef AFS_VFSINCL_ENV
#include <ufs/fsdir.h>
#else /* AFS_VFSINCL_ENV */
#include <sys/dir.h>
#endif /* AFS_VFSINCL_ENV */
#include <sys/user.h>
#define VIRTUE 1
#include <afs/remote.h>
#undef VIRTUE
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <itc.h>
#include <stdio.h>
#include <rx/xdr.h>

/* Here are the include file(s) for the light-weight process facility. */
#include "lwp.h"
#include "lock.h"

#define PAGESIZE 2048

ReallyRead(fid, block, data)
     long *fid;			/* View the fid as longs. */
     long block;
     char *data;
{				/* Do a real read. */
    char fname[100];
    int s, code;
    sprintf(fname, "F%d", *fid);
    s = open(fname, O_RDONLY, 0644);
    if (s < 0)
	Die("can't open cache file");
    code = lseek(s, PAGESIZE * block, 0);
    if (code < 0)
	Die("r:lseek");
    code = read(s, data, PAGESIZE);
    if (code < 0) {
	Die("read");
    }
    close(s);
    return 0;
}

ReallyWrite(fid, block, data)
     long *fid;			/* View the fid as longs. */
     long block;
     char *data;
{				/* Do a real write. */
    char fname[100];
    int s, code;
    sprintf(fname, "F%d", *fid);
    s = open(fname, O_RDWR | O_CREAT, 0644);
    if (s < 0)
	Die("can't find cache file");
    code = lseek(s, PAGESIZE * block, 0);
    if (code < 0)
	Die("w:lseek");
    code = write(s, data, PAGESIZE);
    if (code < 0)
	Die("write");
    close(s);
    return 0;
}


/* The following three routines provide the fid routines used by the buffer and directory packages. */

int
FidZap(afid)
     long *afid;
{				/* Zero out a file */
    *afid = 0;
}

int
FidZero(afid)
     long *afid;
{				/* Zero out a file */
    *afid = 0;
}

int
FidEq(afid, bfid)
     long *afid, *bfid;
{				/* Compare two fids for equality. */
    if (*afid != *bfid)
	return 0;
    return 1;
}

int
FidVolEq(afid, bfid)
     long *afid, *bfid;
{				/* Is fid in a particular volume */
    return 1;
}

int
FidCpy(dfid, sfid)
     long *dfid, *sfid;
{				/* Assign one fid to another. */
    *dfid = *sfid;
}

Die(arg)
     char *arg;
{				/* Print an error message and then exit. */
    int i, j;
    printf("Fatal error: %s\n", arg);

    i = 1;
    j = 0;
    i = i / j;

    exit(1);
}
