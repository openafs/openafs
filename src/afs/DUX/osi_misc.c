/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * afs_suser
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/DUX/Attic/osi_misc.c,v 1.7 2004/07/29 03:13:44 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */

afs_suser(void *credp)
{
    int error;

    if ((error = suser(u.u_cred, &u.u_acflag)) == 0) {
	return (1);
    }
    return (0);
}
