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
    ("$Header$");

#if defined(AFS_AIX41_ENV) && !defined(AFS_AIX51_ENV)
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

#include "aix_ident_prototypes.h"
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
     * Initialize the exported interface routines. Except the authenticate one
     * the others are currently mainly noops.
     */
    meths->method_chpass = afs_chpass;
    meths->method_authenticate = afs_authenticate;
    meths->method_passwdexpired = afs_passwdexpired;
    meths->method_passwdrestrictions = afs_passwdrestrictions;

    /*
     * These we need to bring in because, for afs users, /etc/security/user's
     * "registry" must non-local (i.e. DCE) since otherwise it assumes it's a
     * local domain and uses valid_crypt(passwd) to validate the afs passwd
     * which, of course, will fail. NULL return from these routine simply
     * means use the local version ones after all.
     */
    meths->method_getgrgid = afs_getgrgid;
    meths->method_getgrset = afs_getgrset;
    meths->method_getgrnam = afs_getgrnam;
    meths->method_getpwnam = afs_getpwnam;
    meths->method_getpwuid = afs_getpwuid;
    return (0);
}

#endif /* AFS_AIX41_ENV && !AFS_AIX51_ENV */
