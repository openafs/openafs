/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/cellconfig.h>
#include <ubik.h>

#include "common.h"

int
afstest_GetUbikClient(struct afsconf_dir *dir, char *service,
		      int serviceId,
		      struct rx_securityClass *secClass, int secIndex,
		      struct ubik_client **ubikClient)
{
    int code, i;
    struct afsconf_cell info;
    struct rx_connection *serverconns[MAXSERVERS];

    code = afsconf_GetCellInfo(dir, NULL, service, &info);
    if (code)
	return code;

    for (i = 0; i < info.numServers; i++) {
	serverconns[i] = rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
					  info.hostAddr[i].sin_port,
					  serviceId,
					  secClass, secIndex);
    }

    serverconns[i] = NULL;

    *ubikClient = NULL;

    return ubik_ClientInit(serverconns, ubikClient);

}
