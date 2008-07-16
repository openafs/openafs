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
    ("$Header: /cvs/openafs/src/util/sys.c,v 1.6.14.1 2007/10/31 04:26:18 shadow Exp $");

#include <stdio.h>

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    printf("%s\n", SYS_NAME);
    return 0;
}
