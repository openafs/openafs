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

    LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid);

    ret = rx_Init (port);
    if (ret)
	errx (1, "rx_Init failed");

    secureindex = 5;
    memset(secureobj, 0, sizeof(secureobj));
    secureobj[4] = 
	rxgk_NewServerSecurityObject(rxgk_auth,
				     "afs@L.NXS.SE",
				     NULL,
				     rxgk_default_get_key,
				     NULL,
				     TEST_RXGK_SERVICE);
    
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
