/*
 * Copyright (c) 2002 - 2004, Stockholms universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 * 
 * Redistribution is not permitted
 */

#include "rxgk_locl.h"

#include <rx/rx.h>
#include "rxgk_proto.h"
#include "rxgk_proto.cs.h"

#include <errno.h>

int
rxgk5_get_auth_token(krb5_context context, uint32_t addr, int port, 
		     uint32_t serviceId,
		     RXGK_Token *token,
		     RXGK_Token *auth_token, krb5_keyblock *key,
		     krb5_keyblock *skey,
		     int32_t *kvno)
{
    struct rx_securityClass *secobj;
    struct rx_connection *conn;
    RXGK_Token challange, reply_token;
    uint32_t num;
    int ret;

    memset(skey, 0, sizeof(*skey));

    secobj = rxnull_NewClientSecurityObject();

    conn = rx_NewConnection(addr, port, serviceId, secobj, 0);
    if (conn == NULL)
	return ENETDOWN;

    num = arc4random();

    ret = rxk5_mutual_auth_client_generate(context, key, num, &challange);
    if (ret) {
	rx_DestroyConnection(conn);
	return ret;
    }

    ret = RXGK_EstablishKrb5Context(conn, token, &challange,
				    &reply_token, kvno, auth_token);
    if (ret) {
	rx_DestroyConnection(conn);
	return ret;
    }

    ret = rxk5_mutual_auth_client_check(context, key, num, &reply_token, skey);

    rx_DestroyConnection(conn);

    return ret;
}
