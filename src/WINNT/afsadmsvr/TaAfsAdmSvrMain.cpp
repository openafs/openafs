/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsAdmSvrInternal.h"

/*
 * ROUTINES ___________________________________________________________________
 *
 */

#ifdef DEBUG
LPARAM CALLBACK AfsAdmSvr_Debug_ThreadProc (PVOID lp)
{
   ShowMemoryManager();

   MSG msg;
   while (GetMessage (&msg, 0, 0, 0))
      {
      if (!IsMemoryManagerMessage (&msg))
         {
         TranslateMessage (&msg);
         DispatchMessage (&msg);
         }
      }

   return 0;
}
#endif


int cdecl main (int argc, char **argv)
{
   BOOL fSuccess = FALSE;

   Print (TEXT("Initializing..."));

   WSADATA Data;
   WSAStartup (0x0101, &Data);

   // Parse the command-line
   //
   UINT_PTR dwAutoScope = AFSADMSVR_SCOPE_VOLUMES | AFSADMSVR_SCOPE_USERS;

   for (--argc,++argv; argc; --argc,++argv)
      {
      if (!lstrcmpi (*argv, AFSADMSVR_KEYWORD_TIMED))
         AfsAdmSvr_EnableAutoShutdown (TRUE);
      else if (!lstrcmpi (*argv, AFSADMSVR_KEYWORD_MANUAL))
         dwAutoScope = 0;
      else if (!lstrcmpi (*argv, AFSADMSVR_KEYWORD_SCOPE_USERS))
         dwAutoScope &= ~AFSADMSVR_SCOPE_VOLUMES;
      else if (!lstrcmpi (*argv, AFSADMSVR_KEYWORD_SCOPE_VOLUMES))
         dwAutoScope &= ~AFSADMSVR_SCOPE_USERS;
#ifdef DEBUG
      else if (!lstrcmpi (*argv, AFSADMSVR_KEYWORD_DEBUG))
         CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)AfsAdmSvr_Debug_ThreadProc, 0, 0, 0);
#endif
      }

   // Prepare to listen for RPCs
   //
   unsigned char *pszPROTOCOL = (unsigned char *)"ncacn_ip_tcp";
   unsigned char *pszENTRYNAME = (unsigned char *)AFSADMSVR_ENTRYNAME_DEFAULT;
   unsigned char *pszANNOTATION = (unsigned char *)"Transarc AFS Administrative Server";
   unsigned char szEndpoint[ 32 ];
   wsprintf ((LPTSTR)szEndpoint, "%lu", AFSADMSVR_ENDPOINT_DEFAULT);
   int cMAX_CALLS = 50;

   // Clean up any broken interface registration
   //
   RpcServerUnregisterIf (ITaAfsAdminSvr_v1_0_s_ifspec, 0, FALSE);
#ifdef notdef
   RpcNsBindingUnexport (RPC_C_NS_SYNTAX_DEFAULT, pszENTRYNAME, ITaAfsAdminSvr_v1_0_s_ifspec, NULL);
#endif

   // Register our interface
   //
   RPC_STATUS status;
   if ((status = RpcServerUseProtseq (pszPROTOCOL, cMAX_CALLS, NULL)) != 0)
      {
      Print (dlERROR, TEXT("RpcServerUseProtseq failed; error 0x%08lX"), status);
      }
   else if ((status = RpcServerRegisterIf (ITaAfsAdminSvr_v1_0_s_ifspec, 0, 0)) != 0)
      {
      Print (dlERROR, TEXT("RpcServerRegisterIf failed; error 0x%08lX"), status);
      }
   else
      {
      // Always try to register on port 1025; that's the easiest thing for
      // some clients to find. We'll only fail if we (a) can't use 1025, and
      // (b) can't export our bindings.
      //
      BOOL fGotPort = FALSE;
      if (RpcServerUseProtseqEp (pszPROTOCOL, cMAX_CALLS, szEndpoint, NULL) == 0)
         fGotPort = TRUE;
      else
         Print (dlWARNING, TEXT("RpcServerUseProtseqEp failed (benign); error 0x%08lX"), status);

      RPC_BINDING_VECTOR *pBindingVector;
      if ((status = RpcServerInqBindings (&pBindingVector)) != 0)
         {
         Print (dlERROR, TEXT("RpcServerRegisterIf failed; error 0x%08lX"), status);
         }
      else if ((status = RpcEpRegister (ITaAfsAdminSvr_v1_0_s_ifspec, pBindingVector, NULL, pszANNOTATION)) != 0)
         {
         Print (dlERROR, TEXT("RpcEpRegister failed; error 0x%08lX"), status);
         }
      else
         {
         BOOL fExportedBinding = FALSE;

#ifdef notdef
         if ((status = RpcNsBindingExport (RPC_C_NS_SYNTAX_DEFAULT, pszENTRYNAME, ITaAfsAdminSvr_v1_0_s_ifspec, pBindingVector, NULL)) == 0)
            fExportedBinding = TRUE;
         else
            Print (dlWARNING, TEXT("RpcNsBindingExport failed (benign); error 0x%08lX"), status);
#endif

         if (!fExportedBinding && !fGotPort)
            {
            Print (dlERROR, TEXT("RpcNsBindingExport failed; error 0x%08lX"), status);
            Print (dlERROR, TEXT("Could not bind to port %s or export bindings; terminating"), szEndpoint);
            }
         else
            {
            AfsAdmSvr_Startup();

            Print (TEXT("Ready.\n"));

            // If not asked to open cells manually, fork a thread to start opening
            // the default local cell
            //
            if (dwAutoScope)
               {
               DWORD dwThreadID;
               CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)AfsAdmSvr_AutoOpen_ThreadProc, (PVOID)dwAutoScope, 0, &dwThreadID);
               }

            // Listen for requests until someone calls StopListen
            //
            if ((status = RpcServerListen (1, cMAX_CALLS, FALSE)) != 0)
               {
               Print (dlERROR, TEXT("RpcServerListen failed; error 0x%08lX"), status);
               }
            else
               {
               fSuccess = TRUE;
               }

            AfsAdmSvr_Shutdown();
            }

#ifdef notdef
         if (fExportedBinding)
            {
            if ((status = RpcNsBindingUnexport (RPC_C_NS_SYNTAX_DEFAULT, pszENTRYNAME, ITaAfsAdminSvr_v1_0_s_ifspec, NULL)) != 0)
               {
               Print (dlWARNING, TEXT("RpcNsBindingExport failed; error 0x%08lX"), status);
               }
            }
#endif
         if ((status = RpcEpUnregister (ITaAfsAdminSvr_v1_0_s_ifspec, pBindingVector, NULL)) != 0)
            {
            Print (dlWARNING, TEXT("RpcEpUnregister failed; error 0x%08lX"), status);
            }
         }
      }

   Print (TEXT("Shutting down...\n"));

   if ((status = RpcServerUnregisterIf (0, 0, FALSE)) != 0)
      {
      Print (dlWARNING, TEXT("RpcServerUnregisterIf failed; error 0x%08lX"), status);
      exit (-1);
      }

   return (fSuccess) ? (0) : (-1);
}


extern "C" void __RPC_FAR * __RPC_USER MIDL_user_allocate (size_t cbAllocate)
{
   return (void __RPC_FAR *)Allocate (cbAllocate);
}


extern "C" void __RPC_USER MIDL_user_free (void __RPC_FAR *pData)
{
   Free (pData);
}

