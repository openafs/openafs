/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "cmd.h"
#include <stdio.h>

static int
cproc(struct cmd_syndesc *as, void *arock)
{
    register struct cmd_item *ti;
    printf("in the pear command\n");
    printf("number is %s\n", as->parms[0].items->data);
    if (as->parms[1].items)
	printf("running unauthenticated\n");
    for (ti = as->parms[2].items; ti; ti = ti->next) {
	printf("spotspos %s\n", ti->data);
    }
    return 0;
}

int
main(int argc, char **argv)
{
    register struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, cproc, NULL, "describe pear");
    cmd_AddParm(ts, "-num", CMD_SINGLE, 0, "number of pears");
    cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL, "don't authenticate");
    cmd_AddParm(ts, "-spotpos", CMD_LIST, CMD_OPTIONAL, 0);

    return cmd_Dispatch(argc, argv);
}
