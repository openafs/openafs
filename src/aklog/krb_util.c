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

#include <afsconfig.h>

#if 0
#include <kerberosIV/mit-copyright.h>
#endif
#include <afs/stds.h>
#define KERBEROS_APPLE_DEPRECATED(x)
#include "aklog.h"
#include <krb5.h>

#ifndef MAX_HSTNM
#define MAX_HSTNM 100
#endif

#include <afs/cellconfig.h>

#include <string.h>
#include <ctype.h>

#define S_AD_SZ sizeof(struct sockaddr_in)

char *afs_realm_of_cell(krb5_context context, struct afsconf_cell *cellconfig, int fallback)
{
    static char krbrlm[REALM_SZ+1];
	char **hrealms = 0;
	krb5_error_code retval;

    if (!cellconfig)
	return 0;

    if (fallback) {
	char * p;
	p = strchr(cellconfig->hostName[0], '.');
	if (p++)
	    strcpy(krbrlm, p);
	else
	    strcpy(krbrlm, cellconfig->name);
	for (p=krbrlm; *p; p++) {
	    if (islower(*p))
		*p = toupper(*p);
	}
    } else {
	if ((retval = krb5_get_host_realm(context,
					 cellconfig->hostName[0], &hrealms)))
	    return 0;
	if(!hrealms[0]) return 0;
	strcpy(krbrlm, hrealms[0]);

	if (hrealms) krb5_free_host_realm(context, hrealms);
    }
    return krbrlm;
}
