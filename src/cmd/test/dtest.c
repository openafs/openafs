/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "cmd.h"
#include <stdio.h>

static cproc (as, arock)
char *arock;
struct cmd_syndesc *as; {
    register struct cmd_item *ti;
    printf("in the pear command\n");
    printf("number is %s\n", as->parms[0].items->data);
    if (as->parms[1].items) printf("running unauthenticated\n");
    for(ti=as->parms[2].items; ti; ti=ti->next) {
	printf("spotspos %s\n", ti->data);
    }
    return 0;
}

main(argc, argv)
int argc;
char **argv; {
    register struct cmd_syndesc *ts;
    
    ts = cmd_CreateSyntax((char *) 0, cproc, (char *) 0, "describe pear");
    cmd_AddParm(ts, "-num", CMD_SINGLE, 0, "number of pears");
    cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL, "don't authenticate");
    cmd_AddParm(ts, "-spotpos", CMD_LIST, CMD_OPTIONAL, 0);
    
    return cmd_Dispatch(argc, argv);
}
