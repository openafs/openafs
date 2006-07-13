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

#if defined(AFS_AIX41_ENV)
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

#include <afs/ktc.h>

void
aix_ktc_setup_ticket_file(char * userName)
{
#if defined(AFS_KERBEROS_ENV)
    struct passwd *pwd;

    setpwent();			/* open the pwd database */
    pwd = getpwnam(userName);
    if (pwd) {
	if (chown(ktc_tkt_string_uid(pwd->pw_uid), 
		  pwd->pw_uid, pwd->pw_gid) < 0) {
	    perror("chown: ");
	}
    } else {
	perror("getpwnam : ");
    }
    endpwent();			/* close the pwd database */
#endif /* AFS_KERBEROS_ENV */
}

#endif /* AFS_AIX41_ENV */
