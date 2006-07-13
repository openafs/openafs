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

int
afs_getgrset(char *userName)
{
    return NULL;
}

struct group *
afs_getgrgid(int id)
{
    return NULL;
}

struct group *
afs_getgrnam(char *name)
{
    return NULL;
}

int
afs_getpwnam(int id)
{
    return NULL;
}

int
afs_getpwuid(char *name)
{
    return NULL;
}

#endif /* AFS_AIX41_ENV && !AFS_AIX51_ENV */
