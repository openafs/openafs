/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVR_H
#define TAAFSADMSVR_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <rpc.h>
#include <rpcndr.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include <WINNT/TaLocale.h>
#include <WINNT/iTaAfsAdmSvr.h>
#include <WINNT/TaAfsAdmSvrCommon.h>
#ifndef TAAFSADMSVRCLIENT_H
#include <WINNT/AfsAppLib.h>
#endif // TAAFSADMSVRCLIENT_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

      // The admin server can be started on a machine just by running
      // its .EXE; if the "timed" keyword is given on its command-line,
      // it will shut itself down if it's idle for N minutes. If the "Manual"
      // keyword is not present, the local cell will automatically be opened
      // for administration and its contents refreshed (the scope of the auto-
      // refresh can be limited to users or volumes by also adding one of
      // the Scope keywords)
      //
#define AFSADMSVR_PROGRAM               "TaAfsAdmSvr.exe"
#define AFSADMSVR_KEYWORD_TIMED         "Timed"
#define AFSADMSVR_KEYWORD_MANUAL        "Manual"
#define AFSADMSVR_KEYWORD_SCOPE_USERS   "Users"
#define AFSADMSVR_KEYWORD_SCOPE_VOLUMES "Volumes"
#define AFSADMSVR_KEYWORD_DEBUG         "Debug"

      // Ordinarily, the admin server will export its binding handles
      // under the following identity:
      //
#define AFSADMSVR_ENTRYNAME_DEFAULT     "/.:/Autohandle_TaAfsAdmSvr"

      // On my Win98 box, RpcNsBindingExport() always fails for some
      // inexplicable reason (error 6BF, RPC_S_CALL_FAILED_DNE, which
      // is about as generic a "didn't work" error code as you can get.)
      // Presuming that this is not the only box in the world which can't
      // do this--even though it's documented to work on Win95, perhaps
      // it's widespread?--the admin server will detect failure of this
      // routine and attempt to bind to a particular default endpoint;
      // if the client can't find any valid binding handles through
      // RpcNsBinding* lookups, it will try this well-known endpoint as
      // a last-ditch effort.
      //
#define AFSADMSVR_ENDPOINT_DEFAULT      1025


#endif // TAAFSADMSVR_H

