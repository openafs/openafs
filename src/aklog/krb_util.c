/*
 * This file replaces some of the routines in the Kerberos utilities.
 * It is based on the Kerberos library modules:
 * 	send_to_kdc.c
 * 
 * Copyright 1987, 1988, 1992 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char rcsid_send_to_kdc_c[] =
"$Id$";
#endif /* lint */

#if 0
#include <kerberosIV/mit-copyright.h>
#endif
#include <afs/stds.h>
#include "aklog.h"
#include "afsconfig.h"
#if USING_SSL
#include "k5ssl/k5ssl.h"
#else
#include <krb5.h>
#endif

#ifndef MAX_HSTNM
#define MAX_HSTNM 100
#endif

#ifdef WINDOWS

#include "aklog.h"		/* for struct afsconf_cell */

#else /* !WINDOWS */

#include <afs/param.h>
#if 0
#include <afs/cellconfig.h>
#else
/* hack so this builds in clean environment */
#include <auth/cellconfig.p.h>
#endif

#endif /* WINDOWS */

#include <string.h>

#define S_AD_SZ sizeof(struct sockaddr_in)

/* XXX returns static storage, so not thread safe. */
char *afs_realm_of_cell(krb5_context context, struct afsconf_cell *cellconfig)
{
    static char krbrlm[REALM_SZ+1];
	char **hrealms = 0;
	krb5_error_code retval;

    if (!cellconfig)
	return 0;
    if (retval = krb5_get_host_realm(context,
				cellconfig->hostName[0], &hrealms))
		return 0; 
	if(!hrealms[0]) return 0;
	strcpy(krbrlm, hrealms[0]);

	if (hrealms) krb5_free_host_realm(context, hrealms);
    
    return krbrlm;
}
