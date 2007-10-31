/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file implements the util related funtions for afscp
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "util.h"

int
DoUtilErrorTranslate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { ERROR_CODE } DoUtilErrorTranslate_parm_t;
    afs_status_t st = 0;
    int err = atoi(as->parms[ERROR_CODE].items->data);
    const char *err_str = "unknown error";

    if (util_AdminErrorCodeTranslate(err, 0, &err_str, &st)) {
	printf("%d -> %s\n", err, err_str);
    } else {
	ERR_ST_EXT("util_AdminErrorCodeTranslate failed", st);
    }

    return 0;
}

int
DoUtilDatabaseServerList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { CELL_NAME } DoUtilDatabaseServerList_parm_t;
    afs_status_t st = 0;
    void *iter = NULL;
    util_databaseServerEntry_t server;
    const char *cell = as->parms[CELL_NAME].items->data;

    if (util_DatabaseServerGetBegin(cell, &iter, &st)) {
	printf("listing database servers in cell %s\n", cell);
	while (util_DatabaseServerGetNext(iter, &server, &st)) {
	    struct in_addr ina;
	    ina.s_addr = htonl((unsigned int)server.serverAddress);
	    printf("%s %s\n", server.serverName, inet_ntoa(ina));
	}
	if (st != ADMITERATORDONE) {
	    ERR_ST_EXT("util_DatabaseServerGetNext", st);
	}
	if (!util_DatabaseServerGetDone(iter, &st)) {
	    ERR_ST_EXT("util_DatabaseServerGetDone", st);
	}
    } else {
	ERR_ST_EXT("util_DatabaseServerGetBegin", st);
    }

    return 0;
}

int
DoUtilNameToAddress(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER_NAME } DoUtilNameToAddress_parm_t;
    afs_status_t st = 0;
    const char *server = as->parms[SERVER_NAME].items->data;
    int server_addr = 0;

    if (util_AdminServerAddressGetFromName(server, &server_addr, &st)) {
	struct in_addr ina;
	ina.s_addr = htonl((unsigned int)server_addr);
	printf("address is %s\n", inet_ntoa(ina));
    } else {
	ERR_ST_EXT("util_AdminServerAddressGetFromName", st);
    }
    return 0;
}

void
SetupUtilAdminCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("UtilErrorTranslate", DoUtilErrorTranslate, NULL,
			  "translate an error code");
    cmd_AddParm(ts, "-error", CMD_SINGLE, CMD_REQUIRED, "error code");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("UtilDatabaseServerList", DoUtilDatabaseServerList,
			  NULL, "list the database servers in a cell");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_REQUIRED, "cell to list");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("UtilNameToAddress", DoUtilNameToAddress, NULL,
			  "translate a host name to an address");
    cmd_AddParm(ts, "-host", CMD_SINGLE, CMD_REQUIRED, "host name");
    SetupCommonCmdArgs(ts);
}
