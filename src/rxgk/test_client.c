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

#include "roken.h"

RCSID("$Id$");

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



/*
 *
 */

static void
test_rxgk_conn(uint32_t addr, int port,
	       rxgk_level level, RXGK_Ticket_Crypt *ticket, 
	       struct rxgk_keyblock *key)
{
    struct rx_securityClass *secobj;
    struct rx_connection *conn;
    int ret;
    char bar[100];
    int32_t a111;
    
    secobj = rxgk_NewClientSecurityObject (level, ticket, key);

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
    RXGK_Ticket_Crypt ticket;
    struct rxgk_keyblock key;
    int port, ret;
    uint32_t addr;
    char *saddr;
    PROCESS pid;
    time_t expire;
    rxgk_level level;
    gss_name_t target_name = GSS_C_NO_NAME;

    setprogname(argv[0]);

    if (argc != 2)
	errx(1, "argv != 2");

    port = htons(TEST_DEFAULT_PORT);
    saddr = "127.0.0.1";

    LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid);
    
    ret = rx_Init (0);
    if (ret)
	errx (1, "rx_Init failed");

    addr = str2addr(saddr);

    rxgk_set_log(rxgk_log_stdio, NULL);

    {
	char *target;
	OM_uint32 major_status, minor_status;
	gss_buffer_desc n;

	target = argv[1];

	n.value = target;
	n.length = strlen(target);
	
	major_status = gss_import_name(&minor_status, &n,
				       GSS_KRB5_NT_PRINCIPAL_NAME,
				       &target_name);
	if (GSS_ERROR(major_status))
	    err(1, "import name creds failed with: %d", major_status);
    }

    ret = rxgk_get_gss_cred(addr,
			    port,
			    GSS_C_NO_NAME, /* client */
			    target_name,
			    &expire,
			    &level,
			    0, /* name tag */
			    &ticket,
			    &key);
    if (ret)
	errx(1, "rxgk_get_gss_cred: %d", ret);

    test_rxgk_conn(addr, port, level, &ticket, &key);

    rx_Finalize();

    return 0;
}

