/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
/* Copyright (C) 1996 Transarc Corporation - All rights reserved */
/*
 * COPYRIGHT IBM CORPORATION 1996
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <afs/auth.h>
#include "cmd.h"
#include <afsint.h>
#include <winsock2.h>

CommandProc (as, arock)
  char *arock;
  struct cmd_syndesc *as;
{
  afs_int32 code, ecode=0;
  struct ktc_principal server;
  struct cmd_item *itp;

  if (as->parms[0].items) {	/* A cell is provided */
     for (itp=as->parms[0].items; itp; itp = itp->next) {
	strcpy(server.cell, itp->data);
	server.instance[0] = '\0';
	strcpy(server.name, "afs");
	code = ktc_ForgetToken(&server);
	if (code) {
	   printf("unlog: could not discard tickets for cell %s, code %d\n",
		  itp->data, code);
	   ecode = code;                   /* return last error */
	}
     }
  } else {
     ecode = ktc_ForgetAllTokens ();
     if (ecode)
        printf("unlog: could not discard tickets, code %d\n", ecode);
  }
  return ecode;
}


main(argc, argv)
  int argc;
  char *argv[];

{
  struct cmd_syndesc *ts;
  afs_int32 code;
  WSADATA WSAjunk;

  WSAStartup(0x0101, &WSAjunk);

  ts = cmd_CreateSyntax((char *) 0, CommandProc, 0, "Release Kerberos authentication");
  cmd_AddParm(ts, "-cell", CMD_LIST, CMD_OPTIONAL, "cell name");

  code = cmd_Dispatch(argc, argv);
  return (code);
}
