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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>

#include <afs/param.h>
#include <lwp.h>
#include <rx/rx_null.h>
#include <rx/rx.h>

#include <com_err.h>

#include "afscp.h"
#include "afscp_internal.h"

#ifndef AFSCONF_CLIENTNAME
#include <afs/dirpath.h>
#define AFSCONF_CLIENTNAME AFSDIR_CLIENT_ETC_DIRPATH
#endif

static int init=0;
extern int RXAFSCB_ExecuteRequest();
static struct rx_securityClass *sc;
static struct rx_service *serv;
extern PROCESS rx_listenerPid;
static int start_cb_server()
{

     sc=rxnull_NewServerSecurityObject();
     serv=rx_NewService(0,1,"afs", &sc, 1, RXAFSCB_ExecuteRequest);
     if (!serv)
          return 1;
     rx_StartServer(0);
     return 0;
}


int afscp_init(const char *cell)
{
     if (init)
          return 0;
     if (_rx_InitRandomPort())
	     return -1;
     init=1;
     if (start_cb_server()) {
          printf("Cannot start callback service\n");
	  return -1;
     }
     init=2;
     if (cell)
          return afs_setdefaultcell(cell);
     return 0;
}

void afscp_finalize(void) {
     if (!init)
          return;

     ReturnAllCallBacks();
     IOMGR_Sleep(1);
     rx_Finalize();
#if 0
     LWP_DestroyProcess(rx_listenerPid);
     close(serv->socket);
     rxi_FreeService(serv);
#endif
     IOMGR_Finalize();
     LWP_TerminateProcessSupport();
#if 1
     close(serv->socket);
#endif
}


