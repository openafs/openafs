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
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "k5ssl.h"


krb5_error_code
krb5_encrypt_tkt_part(krb5_context context,
    const krb5_keyblock *key,
    krb5_ticket *ticket)
{
    krb5_data *data = 0;
    int code;
    size_t enclen;

    if ((code = encode_krb5_enc_tkt_part(ticket->enc_part2, &data)))
	goto Done;
    if ((code = krb5_c_encrypt_length(context, key->enctype,
	    data->length, &enclen)))
	goto Done;
    ticket->enc_part.ciphertext.length = enclen;
    if (!(ticket->enc_part.ciphertext.data = malloc(enclen))) {
	code = ENOMEM;
	goto Done;
    }
    if ((code = krb5_c_encrypt(context, key, KRB5_KEYUSAGE_KDC_REP_TICKET,
	    0, data, &ticket->enc_part))) {
	free(ticket->enc_part.ciphertext.data);
	ticket->enc_part.ciphertext.data = 0;
    }
Done:
    if (data) {
	if (data->data)
	    free(data->data);
	free(data);
    }
    return code;
}
