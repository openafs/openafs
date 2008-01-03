/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Test driver for configuration functions. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include <afs/afs_Admin.h>
#include <afs/afs_utilAdmin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_cfgAdmin.h>

#include <afs/cellconfig.h>
#include <afs/cmd.h>


/* define some globals */
static char myCellName[MAXCELLCHARS];
static void *myTokenHandle;
static void *myCellHandle;



/* ------------------- Utility functions ---------------------- */

static const char *
GetErrorText(afs_status_t errorCode)
{
    afs_status_t st;
    const char *errorCodeText;
    static const char *failedLookupText = "unable to translate error code";

    if (util_AdminErrorCodeTranslate(errorCode, 0, &errorCodeText, &st)) {
	return errorCodeText;
    } else {
	return failedLookupText;
    }
}


/* ------------------- CellServDB functions ------------------------ */

#define CSDBCallBackId 42
static pthread_mutex_t CSDBCallBackMutex;
static pthread_cond_t CSDBCallBackCond;
static int CSDBCallBackDone;

static void ADMINAPI
CellServDbCallBack(void *callBackId, cfg_cellServDbStatus_t * statusItemP,
		   afs_status_t st)
{
    if (statusItemP != NULL) {
	printf("Updated %s with result '%s' (tid = %u)\n",
	       statusItemP->fsDbHost, GetErrorText(statusItemP->status),
	       (unsigned)pthread_self());
    } else {
	printf("Update termination (tid = %u)\n", (unsigned)pthread_self());

	(void)pthread_mutex_lock(&CSDBCallBackMutex);
	CSDBCallBackDone = 1;
	(void)pthread_mutex_unlock(&CSDBCallBackMutex);
	(void)pthread_cond_signal(&CSDBCallBackCond);
    }

    if (callBackId != (void *)CSDBCallBackId) {
	printf("Update call back ID invalid (tid = %u)\n",
	       (unsigned)pthread_self());
    }
}


static int
DoCellServDbAddHost(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    int maxUpdates;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    char *sysHost = NULL;

    if (as->parms[1].items) {
	sysHost = as->parms[1].items->data;
    }

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	CSDBCallBackDone = 0;

	if (!cfg_CellServDbAddHost
	    (hostHandle, sysHost, CellServDbCallBack, (void *)CSDBCallBackId,
	     &maxUpdates, &st)) {
	    printf("cfg_CellServDbAddHost failed (%s)\n", GetErrorText(st));
	    CSDBCallBackDone = 1;

	} else {
	    printf("cfg_CellServDbAddHost succeeded (maxUpdates = %d)\n",
		   maxUpdates);
	}

	(void)pthread_mutex_lock(&CSDBCallBackMutex);

	while (!CSDBCallBackDone) {
	    (void)pthread_cond_wait(&CSDBCallBackCond, &CSDBCallBackMutex);
	}

	(void)pthread_mutex_unlock(&CSDBCallBackMutex);

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static int
DoCellServDbRemoveHost(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    int maxUpdates;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    char *sysHost = NULL;

    if (as->parms[1].items) {
	sysHost = as->parms[1].items->data;
    }

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	CSDBCallBackDone = 0;

	if (!cfg_CellServDbRemoveHost
	    (hostHandle, sysHost, CellServDbCallBack, (void *)CSDBCallBackId,
	     &maxUpdates, &st)) {
	    printf("cfg_CellServDbRemoveHost failed (%s)\n",
		   GetErrorText(st));
	    CSDBCallBackDone = 1;

	} else {
	    printf("cfg_CellServDbRemoveHost succeeded (maxUpdates = %d)\n",
		   maxUpdates);
	}

	(void)pthread_mutex_lock(&CSDBCallBackMutex);

	while (!CSDBCallBackDone) {
	    (void)pthread_cond_wait(&CSDBCallBackCond, &CSDBCallBackMutex);
	}

	(void)pthread_mutex_unlock(&CSDBCallBackMutex);

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static int
DoCellServDbEnumerate(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    char *fsDbHost = as->parms[0].items->data;
    char *cellName;
    char *cellDbHosts;

    if (!cfg_CellServDbEnumerate(fsDbHost, &cellName, &cellDbHosts, &st)) {
	printf("cfg_CellServDbEnumerate failed (%s)\n", GetErrorText(st));
    } else {
	char *mstrp;

	printf("%s is in cell %s\n", fsDbHost, cellName);

	for (mstrp = cellDbHosts; *mstrp != '\0'; mstrp += strlen(mstrp) + 1) {
	    printf("\t%s\n", mstrp);
	}

	if (!cfg_StringDeallocate(cellName, &st)) {
	    printf("cfg_StringDeallocate failed (%s)\n", GetErrorText(st));
	}

	if (!cfg_StringDeallocate(cellDbHosts, &st)) {
	    printf("cfg_StringDeallocate failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static void
SetupCellServDbCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("CellServDbAddHost", DoCellServDbAddHost, NULL,
			  "add configuration target to server CellServDB");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-syshost", CMD_SINGLE, CMD_OPTIONAL,
		"system control host");

    ts = cmd_CreateSyntax("CellServDbRemoveHost", DoCellServDbRemoveHost, NULL,
			  "remove configuration target from server CellServDB");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-syshost", CMD_SINGLE, CMD_OPTIONAL,
		"system control host");

    ts = cmd_CreateSyntax("CellServDbEnumerate", DoCellServDbEnumerate, NULL,
			  "enumerate server CellServDB from specified host");
    cmd_AddParm(ts, "-host", CMD_SINGLE, CMD_REQUIRED, "host name");


    (void)pthread_mutex_init(&CSDBCallBackMutex, NULL);
    (void)pthread_cond_init(&CSDBCallBackCond, NULL);
}



/* ------------------- Server functions ------------------------ */


static int
DoDbServersWaitForQuorum(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    unsigned int timeout = 180;

    if (as->parms[1].items) {
	timeout = strtoul(as->parms[1].items->data, NULL, 10);
    }

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_DbServersWaitForQuorum(hostHandle, timeout, &st)) {
	    printf("cfg_DbServersWaitForQuorum failed (%s)\n",
		   GetErrorText(st));
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static int
DoFileServerStop(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_FileServerStop(hostHandle, &st)) {
	    printf("cfg_FileServerStop failed (%s)\n", GetErrorText(st));
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}

static int
DoFileServerStart(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_FileServerStart(hostHandle, &st)) {
	    printf("cfg_FileServerStart failed (%s)\n", GetErrorText(st));
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static void
SetupServerCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("DbServersWaitForQuorum", DoDbServersWaitForQuorum,
			  NULL, "wait for database servers to achieve quorum");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-timeout", CMD_SINGLE, CMD_OPTIONAL,
		"timeout in seconds");

    ts = cmd_CreateSyntax("FileServerStop", DoFileServerStop, NULL,
			  "stop and unconfigure fileserver on specified host");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");

    ts = cmd_CreateSyntax("FileServerStart", DoFileServerStart, NULL,
			  "start the fileserver on specified host");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
}



/* ------------------- Host functions ------------------------ */


static int
DoHostPartitionTableEnumerate(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    cfg_partitionEntry_t *vptable;
    int tableCount, i;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_HostPartitionTableEnumerate
	    (hostHandle, &vptable, &tableCount, &st)) {
	    printf("cfg_HostPartitionTableEnumerate failed (%s)\n",
		   GetErrorText(st));
	} else {
	    for (i = 0; i < tableCount; i++) {
		printf("Partition: %s     Device: %s\n",
		       vptable[i].partitionName, vptable[i].deviceName);
	    }

	    if (!cfg_PartitionListDeallocate(vptable, &st)) {
		printf("cfg_PartitionListDeallocate failed (%s)\n",
		       GetErrorText(st));
	    }
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static void
SetupHostCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("HostPartitionTableEnumerate",
			  DoHostPartitionTableEnumerate, NULL,
			  "enumerate vice partition table");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
}




/* ------------------- Client functions ------------------------ */


static int
DoClientCellServDbAdd(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    char *cellName = as->parms[1].items->data;
    char *dbHost = as->parms[2].items->data;

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_ClientCellServDbAdd(hostHandle, cellName, dbHost, &st)) {
	    printf("cfg_ClientCellServDbAdd failed (%s)\n", GetErrorText(st));
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static int
DoClientCellServDbRemove(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    char *cellName = as->parms[1].items->data;
    char *dbHost = as->parms[2].items->data;

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_ClientCellServDbRemove(hostHandle, cellName, dbHost, &st)) {
	    printf("cfg_ClientCellServDbRemove failed (%s)\n",
		   GetErrorText(st));
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static int
DoClientStart(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    char *timeoutStr = as->parms[1].items->data;
    unsigned int timeoutVal;

    timeoutVal = strtoul(timeoutStr, NULL, 10);

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_ClientStart(hostHandle, timeoutVal, &st)) {
	    printf("cfg_ClientStart failed (%s)\n", GetErrorText(st));
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}



static int
DoClientStop(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    char *timeoutStr = as->parms[1].items->data;
    unsigned int timeoutVal;

    timeoutVal = strtoul(timeoutStr, NULL, 10);

    if (!cfg_HostOpen(myCellHandle, cfgHost, &hostHandle, &st)) {
	printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_ClientStop(hostHandle, timeoutVal, &st)) {
	    printf("cfg_ClientStop failed (%s)\n", GetErrorText(st));
	}

	if (!cfg_HostClose(hostHandle, &st)) {
	    printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}



static int
DoClientSetCell(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    void *hostHandle;
    char *cfgHost = as->parms[0].items->data;
    char *cellName = as->parms[1].items->data;
    struct cmd_item *citem;
    char cellDbHosts[1024];
    char *dbHost = cellDbHosts;
    void *nullCellHandle;

    /* setup multistring of database hosts */
    for (citem = as->parms[2].items; citem != NULL; citem = citem->next) {
	strcpy(dbHost, citem->data);
	dbHost += strlen(citem->data) + 1;
    }
    *dbHost = '\0';

    /* use a null cell handle to avoid "cell mismatch" */
    if (!afsclient_NullCellOpen(&nullCellHandle, &st)) {
	printf("afsclient_NullCellOpen failed (%s)\n", GetErrorText(st));
    } else {
	if (!cfg_HostOpen(nullCellHandle, cfgHost, &hostHandle, &st)) {
	    printf("cfg_HostOpen failed (%s)\n", GetErrorText(st));
	} else {
	    if (!cfg_ClientSetCell(hostHandle, cellName, cellDbHosts, &st)) {
		printf("cfg_ClientSetCell failed (%s)\n", GetErrorText(st));
	    }

	    if (!cfg_HostClose(hostHandle, &st)) {
		printf("cfg_HostClose failed (%s)\n", GetErrorText(st));
	    }
	}

	if (!afsclient_CellClose(nullCellHandle, &st)) {
	    printf("afsclient_CellClose failed (%s)\n", GetErrorText(st));
	}
    }
    return (st == 0 ? 0 : 1);
}


static int
DoClientQueryStatus(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    char *cfgHost = as->parms[0].items->data;
    short cmInstalled;
    unsigned cmVersion;
    afs_status_t clientSt;
    char *clientCell;

    if (!cfg_ClientQueryStatus
	(cfgHost, &cmInstalled, &cmVersion, &clientSt, &clientCell, &st)) {
	printf("cfg_ClientQueryStatus failed (%s)\n", GetErrorText(st));
    } else {
	if (cmInstalled) {
	    printf("Client (CM) version %u is installed.\n", cmVersion);
	} else {
	    printf("Client (CM) is not installed.\n");
	}

	if (clientSt == 0) {
	    printf("Client configuration is valid; default cell is %s.\n",
		   clientCell);
	} else {
	    printf("Client configuration is not valid (%s).\n",
		   GetErrorText(clientSt));
	}
    }

    return (st == 0 ? 0 : 1);
}



static int
DoHostQueryStatus(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    char *cfgHost = as->parms[0].items->data;
    afs_status_t serverSt;
    char *serverCell;

    if (!cfg_HostQueryStatus(cfgHost, &serverSt, &serverCell, &st)) {
	printf("cfg_HostQueryStatus failed (%s)\n", GetErrorText(st));
    } else {
	if (serverSt == 0) {
	    printf("Server configuration is valid; cell is %s.\n",
		   serverCell);
	} else {
	    printf("Server configuration is not valid (%s).\n",
		   GetErrorText(serverSt));
	}
    }

    return (st == 0 ? 0 : 1);
}






static void
SetupClientCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("ClientCellServDbAdd", DoClientCellServDbAdd, NULL,
			  "add host entry to client CellServDB");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_REQUIRED, "cell name");
    cmd_AddParm(ts, "-dbhost", CMD_SINGLE, CMD_REQUIRED, "host to add");

    ts = cmd_CreateSyntax("ClientCellServDbRemove", DoClientCellServDbRemove,
			  NULL, "remove host entry from client CellServDB");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_REQUIRED, "cell name");
    cmd_AddParm(ts, "-dbhost", CMD_SINGLE, CMD_REQUIRED, "host to remove");

    ts = cmd_CreateSyntax("ClientSetCell", DoClientSetCell, NULL,
			  "set default client cell");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_REQUIRED, "cell name");
    cmd_AddParm(ts, "-dbhosts", CMD_LIST, CMD_REQUIRED, "database hosts");

    ts = cmd_CreateSyntax("ClientQueryStatus", DoClientQueryStatus, NULL,
			  "query status of client on host");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");

    ts = cmd_CreateSyntax("HostQueryStatus", DoHostQueryStatus, NULL,
			  "query status of server on host");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");


    ts = cmd_CreateSyntax("ClientStart", DoClientStart, NULL,
			  "start the client");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-timeout", CMD_SINGLE, CMD_REQUIRED, "wait timeout");


    ts = cmd_CreateSyntax("ClientStop", DoClientStop, NULL, "stop the client");
    cmd_AddParm(ts, "-cfghost", CMD_SINGLE, CMD_REQUIRED,
		"configuration host");
    cmd_AddParm(ts, "-timeout", CMD_SINGLE, CMD_REQUIRED, "wait timeout");
}




int
main(int argc, char *argv[])
{
    int code;
    afs_status_t st;
    char *whoami = argv[0];

    /* perform client initialization */

    if (!afsclient_Init(&st)) {
	printf("afsclient_Init failed (%s)\n", GetErrorText(st));
	exit(1);
    }

    if (!afsclient_LocalCellGet(myCellName, &st)) {
	printf("afsclient_LocalCellGet failed (%s)\n", GetErrorText(st));
	exit(1);
    } else {
	printf("%s running in cell %s\n\n", whoami, myCellName);
    }

    if (!afsclient_TokenGetExisting(myCellName, &myTokenHandle, &st)) {
	printf("afsclient_TokenGetExisting failed (%s)\n", GetErrorText(st));
	printf("Test will run unauthenticated\n\n");

	if (!afsclient_TokenGetNew
	    (myCellName, NULL, NULL, &myTokenHandle, &st)) {
	    printf("afsclient_TokenGetNew failed (%s)\n", GetErrorText(st));
	    exit(1);
	}
    }

    if (!afsclient_CellOpen(myCellName, myTokenHandle, &myCellHandle, &st)) {
	printf("afsclient_CellOpen failed (%s)\n", GetErrorText(st));
	exit(1);
    }

    /* initialize command syntax and globals */

    SetupCellServDbCmd();
    SetupServerCmd();
    SetupHostCmd();
    SetupClientCmd();

    /* execute command */

    code = cmd_Dispatch(argc, argv);

    /* release handles */

    if (!afsclient_CellClose(myCellHandle, &st)) {
	printf("afsclient_CellClose failed (%s)\n", GetErrorText(st));
	exit(1);
    }

    if (!afsclient_TokenClose(myTokenHandle, &st)) {
	printf("afsclient_TokenClose failed (%s)\n", GetErrorText(st));
	exit(1);
    }

    return (code);
}
