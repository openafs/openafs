/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "k5ssl.h"

#define DEFAULT_KEYTAB_NAME "FILE:/etc/krb5.keytab"

const char krb5i_defkeyname[] = DEFAULT_KEYTAB_NAME;

krb5_error_code
krb5_kt_default_name(krb5_context context, char *name, int size)
{
    static const char * const names[] = {"libdefaults", "default_keytab_name", 0};
    const char *cp;
    int len;
    char **strings;
    int code;

    code = krb5i_config_get_strings(context, names, &strings);
    if (code) return code;
    cp = *strings;
    free(strings);
    if (!cp) cp = krb5i_defkeyname;

    len = strlen(cp) + 1;
    if (size < len)
	return ENOSPC; /*XXX KRB5_CONFIG_NOTENUFSPACE */
    memcpy(name, cp, len);
    return 0;
}

krb5_error_code
krb5_kt_default(krb5_context context, krb5_keytab *kdescp)
{
    krb5_error_code code;
    char temp[128];
    code = krb5_kt_default_name(context, temp, sizeof temp);
    if (code) return code;
    code = krb5_kt_resolve(context, temp, kdescp);
    return code;
}
