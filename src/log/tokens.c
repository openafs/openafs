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

#ifdef AFS_NT40_ENV
#include <afs/param.h>
#include <afs/stds.h>
#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#else /* !AFS_NT40_ENV */
#include <stdio.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/file.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>		/*time(), ctime() */
#include <pwd.h>
#endif /* AFS_NT40_ENV */
#include <afs/cellconfig.h>
#ifdef AFS_RXK5
#include <afs/rxk5_utilafs.h>
#include <rx/rxk5.h>
#include <afs/rxk5_tkt.h>
#endif /* AFS_RXK5 */
#include <afs/auth.h>

#include <string.h>

#ifndef AFS_NT40_ENV

#define VIRTUE
#define VICE

#ifdef CMUWP_ENV
#include <afs/afsutil.h>	/*getv*(), getc*() routine family */
#endif /* CMUWP_ENV */

#undef VIRTUE
#undef VICE

#include "AFS_component_version_number.c"
#endif /* AFS_NT40_ENV */

int
print_afs_token(afs_int32 index, pioctl_set_token *a_token)
{
    token_rxkad *kad_token;
#ifdef AFS_RXK5
    token_rxk5 *k5_token;
    char *rxk5_princ;
    krb5_context k5context;
    krb5_principal princ;
#endif
    time_t current_time;	/*Current time of day */
    time_t tokenExpireTime;	/*When token expires */
    char expireMsg[512];
    char *expireString;		/*Char string of expiration time */
    afs_int32 code;
    char *idtype;
    int r, i;
    afstoken_soliton at[1];
    XDR xdrs[1];
	    
    if(!a_token)
	return EINVAL;

    r = 0;
    current_time = time(0);
    for (i = 0; i < a_token->tokens.tokens_len; ++i) {
	memset(at, 0, sizeof *at);
	xdrmem_create(xdrs,
	    a_token->tokens.tokens_val[i].token_opaque_val,
	    a_token->tokens.tokens_val[i].token_opaque_len,
	    XDR_DECODE);
	memset(at, 0, sizeof *at);
	if (!xdr_afstoken_soliton(xdrs, at)) {
	    xdrs->x_op = XDR_FREE;
	    xdr_afstoken_soliton(xdrs, at);
	    continue;
	}
	switch(at->at_type) {
	case AFSTOKEN_UNION_KAD:
	    kad_token = &(at->afstoken_soliton_u.at_kad);
	    tokenExpireTime = kad_token->rk_endtime;
	    if(tokenExpireTime <= current_time) {
		sprintf(expireMsg, "[ >> Expired << ]");
	    } else {
		expireString = ctime(&tokenExpireTime);
		expireString += 4;	/*Move past the day of week */
		expireString[12] = '\0';
		sprintf(expireMsg, "[Expires %s]", expireString);
	    }
	    if ((kad_token->rk_endtime - kad_token->rk_begintime) & 1)
		idtype = "AFS ID";
	    else
		idtype = "Unix UID";
	    printf("User's (%s %d) tokens for afs@", 
		idtype,
		kad_token->rk_viceid);
	    if (a_token->cell)
		printf ("%s", a_token->cell);
	    else
		printf ("??? index %d", index);
	    printf (" %s\n", expireMsg);
	    break;
    #ifdef AFS_RXK5
	case AFSTOKEN_UNION_K5:
	    rxk5_princ = 0;
	    k5context  = rxk5_get_context(0);
	    k5_token = &(at->afstoken_soliton_u.at_rxk5);
	    tokenExpireTime = Int64ToInt32(k5_token->k5_endtime);
	    if(tokenExpireTime <= current_time) {
		sprintf(expireMsg, "[ >> Expired << ]");
	    } else {
		expireString = ctime(&tokenExpireTime);
		expireString += 4;	/*Move past the day of week */
		expireString[12] = '\0';
		sprintf(expireMsg, "[Expires %s]", expireString);
	    }
	    rxk5_principal_to_krb5_principal(&princ, &k5_token->k5_client);
	    if (!princ)
		rxk5_princ = 0;
	    else krb5_unparse_name(k5context, 
			     princ, 
			     &rxk5_princ);
	    if (rxk5_princ)
		printf("K5 credential for %s", rxk5_princ);
	    else
		printf("K5 strange credential", rxk5_princ);
	    if (a_token->cell)
		printf (" in cell %s",
		    a_token->cell);
	    else
		printf (" index %d", index);
	    printf (" %s\n", expireMsg);
	    if(rxk5_princ)
		free(rxk5_princ);
	    krb5_free_principal(k5context, princ);
	    break;
    #endif
	default:
	    if (a_token->cell) {
		printf ("unknown token type %d for cell %s\n",
		    at->at_type,
		    a_token->cell);
	    } else {
		printf ("unknown token type %d index %d\n",
		    at->at_type,
		    index);
	    }
	    /* bad credential type */
	    r = -1;
	}
	xdrs->x_op = XDR_FREE;
	xdr_afstoken_soliton(xdrs, at);
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
    pioctl_set_token a_token[1];
	
#ifdef AFS_NT40_ENV
	WSADATA WSAjunk;
#endif

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

#ifdef AFS_NT40_ENV
	WSAStartup(0x0101, &WSAjunk);
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
	memset(a_token, 0, sizeof *a_token);
	rc = ktc_GetTokenEx(index, 0, a_token);
	if(!rc) {
	    print_afs_token(index, a_token);
	    free_afs_token(a_token);
	}
	if (rc == KTC_NOENT) break;
    }
    if (rc == -1 && errno) rc = errno;
    if (rc != KTC_NOENT)
	afs_com_err("tokens", rc, "while fetching tokens");
    else
	printf("   --End of list--\n");

    exit(0);
}				/*Main program */
