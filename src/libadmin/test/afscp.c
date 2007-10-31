/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.
 */

/* Test driver for admin functions. */

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

#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include "common.h"
#include "bos.h"
#include "client.h"
#include "kas.h"
#include "pts.h"
#include "util.h"
#include "vos.h"

/*
 * Globals
 */

void *cellHandle;
void *tokenHandle;
#ifdef AFS_DARWIN_ENV
pthread_mutex_t des_init_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t des_random_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rxkad_random_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* AFS_DARWIN_ENV */

/*
 * Before processing any command, process the common arguments and
 * get an appropriate cell handle
 */

static int
MyBeforeProc(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    int no_auth = 0;
    int auth = 0;
    char auth_cell[MAXCELLCHARS];
    char exec_cell[MAXCELLCHARS];

    /*
     * Check what kind of authentication is necessary based upon
     * the arguments passed
     */

    /*
     * Check for noauth first
     */

    if (as->parms[NOAUTH_PARAM].items) {
	no_auth = 1;
	if (as->parms[USER_PARAM].items) {
	    ERR_EXT("you can't specify both -noauth and -authuser");
	}
	if (as->parms[PASSWORD_PARAM].items) {
	    ERR_EXT("you can't specify both -noauth and -authpassword");
	}
	if (as->parms[AUTHCELL_PARAM].items) {
	    ERR_EXT("you can't specify both -noauth and -authcell");
	}
    }

    /*
     * Check for user name password and auth cell
     * It's ok to specify user name and password, but not auth cell
     * in that case, we assume that the auth cell is the local cell.
     */

    if (as->parms[USER_PARAM].items) {
	if (!as->parms[PASSWORD_PARAM].items) {
	    ERR_EXT
		("you must specify -authpassword if you specify -authuser");
	}
	if (as->parms[AUTHCELL_PARAM].items) {
	    strcpy(auth_cell, as->parms[AUTHCELL_PARAM].items->data);
	} else {
	    if (!afsclient_LocalCellGet(auth_cell, &st)) {
		ERR_ST_EXT("can't get local cell name", st);
	    }
	}
    }

    /*
     * Get the execution cell.  If this parameter wasn't passed, we
     * assume the command should execute in the local cell.
     */

    if (as->parms[EXECCELL_PARAM].items) {
	strcpy(exec_cell, as->parms[EXECCELL_PARAM].items->data);
    } else {
	if (!afsclient_LocalCellGet(exec_cell, &st)) {
	    ERR_ST_EXT("can't get local cell name", st);
	}
    }

    /*
     * Get a token handle and a cell handle for this invocation
     */

    if (no_auth) {
	if (!afsclient_TokenGetNew
	    (auth_cell, (const char *)0, (const char *)0, &tokenHandle,
	     &st)) {
	    ERR_ST_EXT("can't get noauth tokens", st);
	}
    } else {
	if (!afsclient_TokenGetNew
	    (auth_cell, (const char *)as->parms[USER_PARAM].items->data,
	     (const char *)as->parms[PASSWORD_PARAM].items->data,
	     &tokenHandle, &st)) {
	    ERR_ST_EXT("can't get tokens", st);
	}
    }

    if (!afsclient_CellOpen(exec_cell, tokenHandle, &cellHandle, &st)) {
	ERR_ST_EXT("can't open cell", st);
    }

    return 0;
}

static int
MyAfterProc(struct cmd_syndesc *as,void *arock)
{

    afsclient_CellClose(cellHandle, (afs_status_p) 0);
    afsclient_TokenClose(tokenHandle, (afs_status_p) 0);
    return 0;

}


void
SetupCommonCmdArgs(struct cmd_syndesc *as)
{
    cmd_Seek(as, USER_PARAM);
    cmd_AddParm(as, "-authuser", CMD_SINGLE, CMD_OPTIONAL,
		"user name to use for authentication");
    cmd_AddParm(as, "-authpassword", CMD_SINGLE, CMD_OPTIONAL,
		"password to use for authentication");
    cmd_AddParm(as, "-authcell", CMD_SINGLE, CMD_OPTIONAL,
		"cell to use for authentication");
    cmd_AddParm(as, "-execcell", CMD_SINGLE, CMD_OPTIONAL,
		"cell where command will execute");
    cmd_AddParm(as, "-noauth", CMD_FLAG, CMD_OPTIONAL,
		"run this command unauthenticated");
}

int
main(int argc, char *argv[])
{
    int code;
    afs_status_t st;
    char *whoami = argv[0];

    /* perform client initialization */

    if (!afsclient_Init(&st)) {
	ERR_ST_EXT("can't init afs client", st);
    }

    /* initialize command syntax and globals */

    cmd_SetBeforeProc(MyBeforeProc, NULL);
    cmd_SetAfterProc(MyAfterProc, NULL);
    SetupBosAdminCmd();
    SetupClientAdminCmd();
    SetupKasAdminCmd();
    SetupPtsAdminCmd();
    SetupUtilAdminCmd();
    SetupVosAdminCmd();

    /* execute command */

    code = cmd_Dispatch(argc, argv);

    return (code);
}
