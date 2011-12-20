/* AUTORIGHTS
Copyright (C) 2003 - 2010 Chaskiel Grundman
All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <afsconfig.h>
#include <afs/param.h>

#include <rx/rx_null.h>
#include <rx/rx.h>
#include "afscp.h"
#include "afscp_internal.h"

#ifndef AFSCONF_CLIENTNAME
#include <afs/dirpath.h>
#define AFSCONF_CLIENTNAME AFSDIR_CLIENT_ETC_DIRPATH
#endif

static int init = 0;
static struct rx_securityClass *sc;
static struct rx_service *serv;

static int
start_cb_server(void)
{
    sc = rxnull_NewServerSecurityObject();
    serv = rx_NewService(0, 1, "afs", &sc, 1, RXAFSCB_ExecuteRequest);
    if (serv == NULL) {
	return 1;
    }
    rx_StartServer(0);
    return 0;
}


int
afscp_Init(const char *cell)
{
    int code;
    if (init != 0) {
	return 0;
    }
    code = rx_Init(0);
    if (code != 0) {
	return -1;
    }
    init = 1;
    code = start_cb_server();
    if (code != 0) {
	printf("Cannot start callback service\n");
	return -1;
    }
    init = 2;
    if (cell != NULL)
	code = afscp_SetDefaultCell(cell);
    else
	code = 0;
    return code;
}

void
afscp_Finalize(void)
{
    if (init == 0) {
	return;
    }
    afscp_ReturnAllCallBacks();
    afscp_FreeAllCells();
    afscp_FreeAllServers();
    rx_Finalize();
#ifdef AFS_NT40_ENV
    closesocket(serv->socket);
#else
    close(serv->socket);
#endif
}
