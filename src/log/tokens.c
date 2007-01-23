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

#include <stdio.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/file.h>
#include <rx/xdr.h>
#include <errno.h>
#include <sys/types.h>
#include <afs/auth.h>
#include <time.h>		/*time(), ctime() */
#include <pwd.h>
#ifdef USING_SSL
#include "k5ssl.h"
#else
#include <krb5.h>
#endif
#include <afs/cellconfig.h>
#ifdef AFS_RXK5
#include <afs/rxk5_utilafs.h>
#include <afs/rxk5_tkt.h>
#endif
#include <afs/afs_token.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


#define VIRTUE
#define VICE

#ifdef CMUWP_ENV
#include <afs/afsutil.h>	/*getv*(), getc*() routine family */
#endif /* CMUWP_ENV */

#undef VIRTUE
#undef VICE

#include "AFS_component_version_number.c"

/*
 *  Get AFS token at index ix, using new kernel token interface. 
 */
int
ktc_GetTokenEx(
    afs_int32 index,
    char *cell,
    afs_token **a_token);

int
print_afs_token(afs_int32 index, afs_token *a_token)
{
    rxkad_token *kad_token;
#ifdef AFS_RXK5
    rxk5_token *k5_token;
    krb5_creds *k5creds;
    char *rxk5_princ;
    krb5_context k5context;
#endif
    time_t current_time;	/*Current time of day */
    time_t tokenExpireTime;	/*When token expires */
    char expireMsg[512];
    char *expireString;		/*Char string of expiration time */
    afs_int32 code;
    char *idtype;
	    
    if(!a_token)
	return EINVAL;

    current_time = time(0);
    switch(a_token->cu->cu_type) {
    case CU_KAD:
	kad_token = &(a_token->cu->cu_u.cu_kad);
	tokenExpireTime = kad_token->token.endtime;
	if(tokenExpireTime <= current_time) {
	    sprintf(expireMsg, "[ >> Expired << ]");
	} else {
	    expireString = ctime(&tokenExpireTime);
	    expireString += 4;	/*Move past the day of week */
	    expireString[12] = '\0';
	    sprintf(expireMsg, "[Expires %s]", expireString);
	}
	if ((kad_token->token.endtime - kad_token->token.begintime) & 1)
	    idtype = "AFS ID";
	else
	    idtype = "Unix UID";
	printf("User's (%s %d) tokens for afs@%s", 
	    idtype,
	    kad_token->token.viceid,
	    kad_token->cell_name);
	if (!a_token->cell)
	    printf (" index %s", index);
	else if (strcmp(a_token->cell, kad_token->cell_name))
	    printf (" cell %s", a_token->cell);
	printf (" %s\n", expireMsg);
	break;
#ifdef AFS_RXK5
    case CU_K5:
	k5creds = 0;
	rxk5_princ = 0;
	k5context  = rxk5_get_context(0);
	k5_token  = &(a_token->cu->cu_u.cu_rxk5);
	tokenExpireTime = k5_token->endtime;
	if(tokenExpireTime <= current_time) {
	    sprintf(expireMsg, "[ >> Expired << ]");
	} else {
	    expireString = ctime(&tokenExpireTime);
	    expireString += 4;	/*Move past the day of week */
	    expireString[12] = '\0';
	    sprintf(expireMsg, "[Expires %s]", expireString);
	}
	code = afs_token_to_k5_creds(a_token, &k5creds);
	code = krb5_unparse_name(k5context, 
				     k5creds->client, 
				     &rxk5_princ);
	printf("K5 credential for %s", rxk5_princ);
	if (a_token->cell)
	    printf (" in cell %s",
		a_token->cell);
	else
	    printf (" index %d", index);
	printf (" %s\n", expireMsg);
	if(rxk5_princ)
	    free(rxk5_princ);
	if(k5creds)
	    krb5_free_creds(k5context, k5creds);
	break;
#endif
    default:
	if (a_token->cell) {
	    printf ("unknown token type %d for cell %s\n",
		a_token->cu->cu_type,
		a_token->cell);
	} else {
	    printf ("unknown token type %d index %d\n",
		a_token->cu->cu_type,
		index);
	}
	/* bad credential type */
	return -1;
    }

    return 0;
}

int
main(int argc, char **argv)
{				/*Main program */
    int cellNum;		/*Cell entry number */
    int rc;			/*Return value from U_CellGetLocalTokens */
    time_t current_time;	/*Current time of day */
    time_t tokenExpireTime;	/*When token expires */
    char *expireString;		/*Char string of expiration time */
    char UserName[MAXKTCNAMELEN * 2 + 2]; /*Printable user name */
    struct ktc_principal serviceName, clientName;	/* service name for ticket */
    struct ktc_token token;	/* the token we're printing */
    afs_int32 index;
    afs_token *a_token;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif

    /* has no args ... support for help flag */

    if (argc > 1) {
	/* syntax from AFS Com Ref Man p9-39 */

	printf("Usage: tokens [-help]\n");
	fflush(stdout);
	exit(0);
    }

    printf("\nTokens held by the Cache Manager:\n\n");
    cellNum = 0;
    current_time = time(0);

    for(index = 0; index < 200; ++index) {
	rc = ktc_GetTokenEx(index, 0, &a_token);
	if(!rc && a_token) {
	    print_afs_token(index, a_token);
	    free_afs_token(a_token);
	}
	if (rc == KTC_NOENT) break;
    }
    if (rc == -1 && errno) rc = errno;
    if (rc != KTC_NOENT)
	com_err("tokens", rc, "while fetching tokens");
    else
	printf("   --End of list--\n");

    exit(0);
}				/*Main program */
