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
#include <netdb.h>

#include "rxgk_locl.h"
#include "rxgk_proto.ss.h"
#include "test.ss.h"

#include "roken.h"

RCSID("$Id$");

/*
 *
 */

int
STEST_get_hundraelva(struct rx_call *call, int32_t *foo, char *bar)
{
    *foo = 111;
    snprintf(bar, 100, "hej");
    return 0;
}

/*
 *
 */

int
main(int argc, char **argv)
{
    struct rx_securityClass *secureobj[5];
    struct rx_service *service;
    int secureindex;
    PROCESS pid;
    int port = htons(TEST_DEFAULT_PORT);
    int ret;
    struct rxgk_server_params params;

    setprogname(argv[0]);

    LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid);

    if (argc != 2)
	errx(1, "argv != 2");

    rxgk_set_log(rxgk_log_stdio, NULL);

    ret = rx_Init (port);
    if (ret)
	errx (1, "rx_Init failed");

    memset(&params, 0, sizeof(params));

    params.connection_lifetime = 60*60;
    params.bytelife = 30;
    params.enctypes.len = 0;
    params.enctypes.val = NULL;

    secureindex = 5;
    memset(secureobj, 0, sizeof(secureobj));
    secureobj[4] = 
	rxgk_NewServerSecurityObject(RXGK_WIRE_ENCRYPT, argv[1], &params);

    service = rx_NewService (0,
			     TEST_SERVICE_ID,
			     "rxgk-test", 
			     secureobj, 
			     secureindex, 
			     TEST_ExecuteRequest);
    if (service == NULL) 
	errx(1, "Cant create server");

    rx_StartServer(1) ;

    return 0;
}
