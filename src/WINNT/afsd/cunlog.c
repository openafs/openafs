/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
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
