/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Utility to forcibly remove AFS software without using InstallShield */

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/cmd.h>

#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "forceremove.h"


static int DoClient34(struct cmd_syndesc *as, char *arock)
{
    DWORD status = Client34Eradicate(FALSE);

    if (status == ERROR_SUCCESS) {
	printf("Client 3.4a removal succeeded\n");
    } else {
	printf("Client 3.4a removal failed (%d)\n", (int)status);
    }
    return 0;
}

static void
SetupCmd(void)
{
    struct cmd_syndesc	*ts;

    ts = cmd_CreateSyntax("client34", DoClient34, 0,
			  "remove AFS 3.4a client");
}


int main(int argc, char *argv[])
{
    int code;

    /* initialize command syntax */
    initialize_CMD_error_table();
    SetupCmd();

    /* execute command */
    code = cmd_Dispatch(argc, argv);

    return (code);
}
