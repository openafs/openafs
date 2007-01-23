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
#include <stdarg.h>
#include <errno.h>
#include "k5ssl.h"

krb5_error_code
krb5_build_principal_va(krb5_context context,
    krb5_principal *princp,
    int rlen,
    char const *realm, va_list aap)
{
    va_list ap;
    int i;
    char *cp;
    int code;
    krb5_principal princ;

    code = ENOMEM;
    ap = aap;
    for (i = 0; va_arg(ap, char *); ++i)
	;
    princ = malloc(sizeof *princ);
    if (!princ) return code;
    memset(princ, 0, sizeof *princ);
    if (!(princ->realm.data = malloc(rlen + 1)))
	goto Failed;
    memcpy(princ->realm.data, realm, rlen);
    rlen[princ->realm.data] = 0;
    princ->realm.length = rlen;
    if (!(princ->data = malloc(i * sizeof *princ->data)))
	goto Failed;
    memset(princ->data, 0, i * sizeof *princ->data);
    ap = aap;
    for (i = 0; cp = va_arg(ap, char *); ++i) {
	if (!(princ->data[i].data = malloc(1 + (princ->data[i].length = strlen(cp)))))
	    goto Failed;
	memcpy(princ->data[i].data, cp, princ->data[i].length);
	princ->data[i].length[princ->data[i].data] = 0;
    }
    princ->length = i;
    *princp = princ;
    princ = 0;
    code = 0;
Failed:
    if (princ) krb5_free_principal(context, princ);
    return code;
}

krb5_error_code
krb5_build_principal(krb5_context context,
    krb5_principal *princp,
    int rlen,
    char const *realm, ...)
{
    int r;
    va_list ap;
    va_start(ap, realm);
    r = krb5_build_principal_va(context, princp, rlen, realm, ap);
    va_end(ap);
    return r;
}
