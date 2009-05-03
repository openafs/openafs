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

krb5_error_code
krb5_kt_read_service_key(krb5_context context,
    krb5_pointer arg,
    krb5_principal princ,
    krb5_kvno vno,
    krb5_enctype enctype,
    krb5_keyblock **keyp)
{
    krb5_keytab kt = 0;
    int code;
    krb5_keytab_entry entry[1];

    *keyp = 0;
    memset(entry, 0, sizeof *entry);
    code = arg
	? krb5_kt_resolve(context, arg, &kt)
	: krb5_kt_default (context, &kt);
    if (code) goto Done;
    code = krb5_kt_get_entry(context, kt, princ, vno, enctype, entry);
    if (code) goto Done;
    code = ENOMEM; if (!(*keyp = malloc(sizeof **keyp)))
	goto Done;
    **keyp = entry->key;
    entry->key.contents = 0;
    code = 0;
Done:
    krb5_free_keytab_entry_contents(context, entry);
    if (kt) krb5_kt_close(context, kt);
    return code;
}
