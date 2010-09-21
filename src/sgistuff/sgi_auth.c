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


#if defined(AFS_SGI_ENV)

#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <afs/kauth.h>
#include <afs/kautils.h>

extern char *ktc_tkt_string();

/*
 * authenticate with AFS
 * returns:
 *	1 if read password but couldn't authenticate correctly via AFS
 *	0 if authenticated via AFS correctly
 */
int
afs_verify(char *uname,		/* user name trying to log in */
	   char *pword,		/* password */
	   afs_int32 * exp,	/* expiration time */
	   int quite)
{				/* no printing */
    auto char *reason;

    ka_Init(0);
    /*
     * The basic model for logging in now, is that *if* there
     * is a kerberos record for this individual user we will
     * trust kerberos (provided the user really has an account
     * locally.)  If there is no kerberos record (or the password
     * were typed incorrectly.) we would attempt to authenticate
     * against the local password file entry.  Naturally, if
     * both fail we use existing failure code.
     */
    if (ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG, uname,	/* kerberos name */
				   NULL,	/* instance */
				   NULL,	/* realm */
				   pword,	/* password */
				   0,	/* default lifetime */
				   exp,	/* spare 1/expiration */
				   0,	/* spare 2 */
				   &reason	/* error string */
	)) {
	if (!quite) {
	    printf("Unable to authenticate to AFS because %s\n", reason);
	    printf("proceeding with local authentication...\n");
	}
	return 1;
    }
    /* authenticated successfully */
    return 0;
}

char *
afs_gettktstring(void)
{
    return ktc_tkt_string();
}

#endif
