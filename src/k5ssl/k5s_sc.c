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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "k5ssl.h"

krb5_error_code
krb5_decrypt_tkt_part(krb5_context context, const krb5_keyblock *key,
    krb5_ticket *ticket)
{
    int code;
    krb5_data data[1];

    if (!krb5_c_valid_enctype(ticket->enc_part.enctype))
	return KRB5_PROG_ETYPE_NOSUPP;
    if (!(data->data = malloc(data->length = ticket->enc_part.ciphertext.length)))
	return ENOMEM;
    if ((code = krb5_c_decrypt(context, key, KRB5_KEYUSAGE_KDC_REP_TICKET,
	    0, &ticket->enc_part, data)))
	goto Done;
    code = decode_krb5_enc_tkt_part(data, &ticket->enc_part2);
Done:
    if (data->data) {
	memset(data->data, 0, data->length);
	free(data->data);
    }
    return code;
}
