/*
 * Copyright (c) 2002 - 2004, Stockholms universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 * 
 * Redistribution is not permitted
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>

#include "rxgk_locl.h"
#include "rxgk_proto.cs.h"
#include "test.cs.h"

/*
 *
 */

static u_long
str2addr (const char *s)
{
    struct in_addr server;
    struct hostent *h;

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif
    if (inet_addr(s) != INADDR_NONE)
        return inet_addr(s);
    h = gethostbyname (s);
    if (h != NULL) {
	memcpy (&server, h->h_addr_list[0], sizeof(server));
	return server.s_addr;
    }
    return 0;
}


static int
get_krb5_token(krb5_context ctx, krb5_keyblock **key, RXGK_Token *token)
{
    krb5_error_code ret;
    krb5_creds in_creds, *out_creds;
    krb5_ccache id;
    char *realm = "L.NXS.SE";
    int realm_len = strlen(realm);

    memset(token, 0, sizeof(*token));

    ret = krb5_cc_default (ctx, &id);
    if (ret)
	return ret;

    memset(&in_creds, 0, sizeof(in_creds));
    ret = krb5_build_principal(ctx, &in_creds.server,
			       realm_len, realm, "afs", NULL);
    if(ret)
	return ret;
    ret = krb5_build_principal(ctx, &in_creds.client,
			       realm_len, realm, "lha", NULL);
    if(ret){
	krb5_free_principal(ctx, in_creds.server);
	return ret;
    }
    in_creds.session.keytype = KEYTYPE_DES; /* XXX */
    ret = krb5_get_credentials(ctx, 0, id, &in_creds, &out_creds);
    krb5_free_principal(ctx, in_creds.server);
    krb5_free_principal(ctx, in_creds.client);
    if(ret) 
	return ret;

    token->val = malloc(out_creds->ticket.length);
    if (token->val == NULL) {
	krb5_free_creds(ctx, out_creds);
	return ENOMEM;
    }
    token->len = out_creds->ticket.length;
    memcpy(token->val, out_creds->ticket.data, out_creds->ticket.length);

    ret = krb5_copy_keyblock(ctx, &out_creds->session, key);

    krb5_free_creds(ctx, out_creds);

    return ret;
}

/*
 *
 */

static void
test_est_context(krb5_context context, uint32_t addr, int port, 
		 RXGK_Token *ticket, krb5_keyblock *key)
{
    RXGK_Token auth_token;
    krb5_keyblock skey;
    int32_t kvno;
    int ret;

    /* kernel */

    ret = rxgk5_get_auth_token(context, addr, port, 
			       TEST_RXGK_SERVICE,
			       ticket, &auth_token, key, &skey, &kvno);
    if (ret)
	errx(1, "rxgk5_get_auth_token: %d", ret);
	
    printf("EstablishKrb5Context succeeded "
	   "len: %d, version: %d, enctype: %d\n",
	   auth_token.len, kvno, skey.keytype);
}

static void
test_rxgk_conn(krb5_context context, uint32_t addr, int port, 
	       RXGK_Token *ticket, krb5_keyblock *key)
{
    struct rx_securityClass *secobj;
    struct rx_connection *conn;
    int ret;
    char bar[100];
    int32_t a111;

    secobj = rxgk_k5_NewClientSecurityObject(rxgk_crypt,
					     key,
					     0,
					     ticket->len,
					     ticket->val,
					     TEST_RXGK_SERVICE,
					     context);

    conn = rx_NewConnection(addr, port, TEST_SERVICE_ID, secobj, 4);
    if (conn == NULL)
	errx(1, "NewConnection");

    ret = TEST_get_hundraelva(conn, &a111, bar);

    rx_DestroyConnection(conn);

    if (ret)
	errx(1, "TEST_get_hundraelva: %d", ret);

    printf("get_hundraelva return %d (should be 111) (bar = \"%s\")\n",
	   (int)a111, bar);
}


/*
 *
 */

int
main(int argc, char **argv)
{
    RXGK_Token ticket;
    krb5_context context;
    krb5_keyblock *key;
    int port, ret;
    uint32_t addr;
    char *saddr;
    PROCESS pid;

    setprogname(argv[0]);

    port = htons(TEST_DEFAULT_PORT);
    saddr = "127.0.0.1";

    krb5_init_context(&context);

    LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid);
    
    ret = rx_Init (0);
    if (ret)
	errx (1, "rx_Init failed");

    addr = str2addr(saddr);

    ret = get_krb5_token(context, &key, &ticket);
    if (ret)
	errx(1, "get_krb5_token: %d", ret);

    if (0) {
	test_est_context(context, addr, port, &ticket, key);
    } else {
	test_rxgk_conn(context, addr, port, &ticket, key);
    }

    rx_Finalize();

    krb5_free_keyblock(context, key);
    krb5_free_context(context);

    return 0;
}

