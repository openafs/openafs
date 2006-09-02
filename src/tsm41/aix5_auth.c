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
    ("$Header: /cvs/openafs/src/tsm41/aix5_auth.c,v 1.1.2.2 2006/07/13 17:19:41 shadow Exp $");

#if defined(AFS_AIX51_ENV)
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <locale.h>
#include <nl_types.h>
#include <pwd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <errno.h>
#include <usersec.h>

#include <afs/kauth.h>
#include <afs/kautils.h>

#include "aix_auth_prototypes.h"

int
afs_initialize(struct secmethod_table *meths)
{
    /*
     * Initialize kauth package here so we don't have to call it
     * each time we call the authenticate routine.      
     */
    ka_Init(0);
    memset(meths, 0, sizeof(struct secmethod_table));

    /*
     * Initialize the exported interface routines.
     * Aside from method_authenticate, these are just no-ops.
     */
    meths->method_chpass = afs_chpass;
    meths->method_authenticate = afs_authenticate;
    meths->method_passwdexpired = afs_passwdexpired;
    meths->method_passwdrestrictions = afs_passwdrestrictions;
    meths->method_getpasswd = afs_getpasswd;

    return (0);
}

#endif /* AFS_AIX51_ENV */
