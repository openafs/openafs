/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		common.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#include <sys/types.h>
#include <stdio.h>
#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header: /tmp/cvstemp/openafs/src/vol/common.c,v 1.1.1.4 2001/07/14 22:24:55 hartmans Exp $");

#include <afs/afsutil.h>

int Statistics = 0;

/* VARARGS */
Log (a,b,c,d,e,f,g,h,i,j,k)
    char *a, *b, *c, *d, *e, *f, *g, *h, *i, *j, *k;
{
    int level;

    if (Statistics)
	level = -1;
    else
	level = 0;
    ViceLog(level,(a,b,c,d,e,f,g,h,i,j,k));
}

Abort(s,a,b,c,d,e,f,g,h,i,j) 
    char *s, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    ViceLog(0, ("Program aborted: "));
    ViceLog(0, (s,a,b,c,d,e,f,g,h,i,j));
    abort();
}

Quit(s,a,b,c,d,e,f,g,h,i,j) 
    char *s, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    ViceLog(0, (s,a,b,c,d,e,f,g,h,i,j));
    exit(1);
}
