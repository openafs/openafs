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
cproc1(struct cmd_syndesc *as, void *arock)
{
    printf("in the apple command\n");
    return 0;
}

static int
cproc2(struct cmd_syndesc *as, void *arock)
{
    register struct cmd_item *ti;
    printf("in the pear command\n");
    printf("number is %s\n", as->parms[0].items->data);
    if (as->parms[1].items)
	printf("running unauthenticated\n");
    for (ti = as->parms[2].items; ti; ti = ti->next) {
	printf("spotspos %s\n", ti->data);
    }
    if (as->parms[8].items)
	printf("cell name %s\n", as->parms[8].items->data);
    return 0;
}

int
main(int argc, char **argv)
{
    register struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("apple", cproc1, NULL, "describe apple");
    cmd_CreateAlias(ts, "appl");

    ts = cmd_CreateSyntax("pear", cproc2, NULL, "describe pear");
    cmd_AddParm(ts, "-num", CMD_LIST, 0, "number of pears");
    cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL, "don't authenticate");
    cmd_AddParm(ts, "-spotpos", CMD_LIST, CMD_OPTIONAL | CMD_EXPANDS, 0);
    cmd_Seek(ts, 8);
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_CreateAlias(ts, "alias");

    return cmd_Dispatch(argc, argv);
}
