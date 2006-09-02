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
    ("$Header: /cvs/openafs/src/tsm41/aix_auth_common.c,v 1.1.2.2 2006/07/13 17:19:41 shadow Exp $");

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

int
afs_authenticate(char *userName, char *response, int *reenter, char **message)
{
    char *reason, *pword, prompt[256];
    struct passwd *pwd;
    int code, unixauthneeded, password_expires = -1;

    *reenter = 0;
    *message = (char *)0;
    if (response) {
	pword = response;
    } else {
	sprintf(prompt, "Enter AFS password for %s: ", userName);
	pword = getpass(prompt);
	if (strlen(pword) == 0) {
	    printf
		("Unable to read password because zero length passord is illegal\n");
	    *message = (char *)malloc(256);
	    sprintf(*message,
		    "Unable to read password because zero length passord is illegal\n");
	    return AUTH_FAILURE;
	}
    }

    if ((pwd = getpwnam(userName)) == NULL) {
	*message = (char *)malloc(256);
	sprintf(*message, "getpwnam for user failed\n");
	return AUTH_FAILURE;
    }

    if (code =
	ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG,
				   userName, (char *)0, (char *)0, pword, 0,
				   &password_expires, 0, &reason)) {
	if (code == KANOENT)
	    return AUTH_NOTFOUND;
	*message = (char *)malloc(1024);
	sprintf(*message, "Unable to authenticate to AFS because %s.\n",
		reason);
	return AUTH_FAILURE;
    }
    aix_ktc_setup_ticket_file(userName);
    return AUTH_SUCCESS;
}

int
afs_chpass(char *userName, char *oldPasswd, char *newPasswd, char **message)
{
    return AUTH_SUCCESS;
}

int
afs_passwdexpired(char *userName, char **message)
{
    return AUTH_SUCCESS;
}

int
afs_passwdrestrictions(char *userName, char *newPasswd, char *oldPasswd,
		       char **message)
{
    return AUTH_SUCCESS;
}

char *
afs_getpasswd(char * userName)
{
    errno = ENOSYS;
    return NULL;
}

#endif /* AFS_AIX41_ENV */
