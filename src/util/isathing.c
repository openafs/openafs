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

RCSID
    ("$Header: /cvs/openafs/src/util/isathing.c,v 1.6 2003/07/15 23:17:16 shadow Exp $");

#include <ctype.h>

/* checks a string to determine whether it's a non-negative decimal integer or not */
int
util_isint(char *str)
{
    char *i;

    for (i = str; *i && !isspace(*i); i++) {
	if (!isdigit(*i))
	    return 0;
    }

    return 1;
}
