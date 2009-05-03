/*
 * Copyright (c) 2005
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

#include <afs/stds.h>
#include <afsconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <assert.h>
#include <assert.h>
#include <com_err.h>
#include <string.h>

#include "rxk5imp.h"
#include "rxk5c.h"
#include "rxk5errors.h"

int rxk5_default_get_key(void *arg,
    krb5_context context,
#ifdef USING_SHISHI
    char *server,
#else
    krb5_principal server,
#endif
    int enctype,
    int kvno,
    krb5_keyblock *key)
{
    int code;
#ifdef USING_SHISHI
    Shishi_key *sk;
    char *sname = 0, *srealm = 0;

    code = shishi_parse_name(context, server, &sname, &srealm);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    sk = shishi_keys_for_serverrealm_in_file(context,
	(char*)arg, sname, srealm);
    if (!sk) {
printf ("rxk5_default_get_key: shishi_keys_for_serverrealm_in_file failed %s @ %s\n",
sname, srealm);
	code = RXK5UNKNOWNKEY;
    }
    key->sk = sk;
Done:
    if (sname) free(sname);
    if (srealm) free(srealm);
#else
    krb5_keytab keytab = 0;
    krb5_keytab_entry ktentry[1];

    memset(ktentry, 0, sizeof *ktentry);
    code = krb5_kt_resolve(context, (char*)arg, &keytab);
    if (code) goto Done;
    code = krb5_kt_get_entry(context, keytab, server, kvno, enctype, ktentry);
    if (code) goto Done;
    /* This either works or not.  Either way, we exit */
#ifdef USING_HEIMDAL
    code = krb5_copy_keyblock_contents(context, &ktentry->keyblock, key);
Done:
    krb5_kt_free_entry(context, ktentry);
#else
    code = krb5_copy_keyblock_contents(context, &ktentry->key, key);
Done:
    krb5_free_keytab_entry_contents(context, ktentry);
#endif
    if (keytab) (void) krb5_kt_close(context, keytab);
#endif
    return code;
}
