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

#include "TaAfsAdmSvrClientInternal.h"

extern "C" {
#include <afs/afs_AdminErrors.h>
} // extern "C"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cmsecLOCAL_BIND_TIMEOUT  (15L * 1000L)  // wait up to 15 seconds to bind
#define cmsecLOCAL_BIND_SLEEP     (1L * 1000L)  // sleep for a second between


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL ValidateBinding (RPC_NS_HANDLE hBind, UINT_PTR *pidClient, ULONG *pStatus);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL ADMINAPI BindToAdminServer (LPCTSTR pszAddress, BOOL fWait, UINT_PTR *pidClient, ULONG *pStatus)
{
   RPC_STATUS status = 0;

   unsigned char *pszPROTOCOL = (unsigned char *)"ncacn_ip_tcp";
   unsigned char *pszENTRYNAME = (unsigned char *)AFSADMSVR_ENTRYNAME_DEFAULT;
   unsigned char szEndpoint[ 32 ];
   wsprintf ((LPTSTR)szEndpoint, "%lu", AFSADMSVR_ENDPOINT_DEFAULT);

   for (DWORD dwTickStart = GetTickCount(); ; )
      {
#ifdef notdef
      // First we'll enumerate the name services around here to see if
      // an admin server is already running.
      //
      RPC_NS_HANDLE hEnum;
      if ((status = RpcNsBindingImportBegin (RPC_C_NS_SYNTAX_DEFAULT, pszENTRYNAME, ITaAfsAdminSvr_v1_0_c_ifspec, NULL, &hEnum)) == 0)
         {
         RPC_BINDING_HANDLE hBind;
         status = RpcNsBindingImportNext (hEnum, &hBind);
         RpcNsBindingImportDone (&hEnum);

         if (status)
            RpcBindingFree (&hBind);
         else if (ValidateBinding (hBind, pidClient, (ULONG*)&status))
            return TRUE;
         else if (status != RPC_S_CALL_FAILED_DNE) // server rejected us!
            break;
         }
#endif
      // Failing that, we'll try to bind to the well-known endpoint that the
      // admin server may have had to use. (if RpcNsBindingExport failed.)
      //
      unsigned char *pszStringBinding = NULL;
      if ((status = RpcStringBindingCompose (NULL, pszPROTOCOL, (unsigned char *)pszAddress, szEndpoint, NULL, &pszStringBinding)) == 0)
         {
         RPC_BINDING_HANDLE hBind;
         status = RpcBindingFromStringBinding (pszStringBinding, &hBind);
         RpcStringFree (&pszStringBinding);

         if (status)
            RpcBindingFree (&hBind);
         else if (ValidateBinding (hBind, pidClient, (ULONG*)&status))
            return TRUE;
         else if (status != RPC_S_CALL_FAILED_DNE) // server rejected us!
            break;
         }

      // If we can't wait any longer, fail. Otherwise, sleep for a little bit
      // and try again.
      //
      if ((!fWait) || (GetTickCount() - dwTickStart > cmsecLOCAL_BIND_TIMEOUT))
         break;

      Sleep (cmsecLOCAL_BIND_SLEEP);
      }

   if (pStatus)
      *pStatus = (LONG)status;
   return FALSE;
}


BOOL ADMINAPI UnbindFromAdminServer (UINT_PTR idClient, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      ULONG status;
      AfsAdmSvr_Disconnect (idClient, &status);
      }
   RpcExcept(1)
      ;
   RpcEndExcept

   if ((status = RpcBindingFree (&hBindTaAfsAdminSvr)) != 0)
      rc = FALSE;

   if (!rc && pStatus)
      *pStatus = (LONG)status;
   return rc;
}


BOOL ADMINAPI ForkNewAdminServer (ULONG *pStatus)
{
   // Before we can fork a new process, we have to find the program to run.
   //
   TCHAR szFile[ MAX_PATH ];
   GetModuleFileName (GetModuleHandle(NULL), szFile, MAX_PATH);

   LPTSTR pch;
   if ((pch = (LPTSTR)lstrrchr (szFile, TEXT('\\'))) != NULL)
      *(1+pch) = TEXT('\0');
   lstrcat (szFile, AFSADMSVR_PROGRAM);

   if (GetFileAttributes (szFile) == (DWORD)0xFFFFFFFF)
      {
      lstrcpy (szFile, AFSADMSVR_PROGRAM);  // hope it's on the path
      }

   // Try to launch the program. Error codes are returns <= 32.
   // Remember to add the "Timed" keyword, so it will shut itself down
   // if it's idle too long, and the "Manual" keyword so it won't automatically
   // start opening a cell and looking around.
   //
   wsprintf (&szFile[ lstrlen(szFile) ], TEXT(" %s %s"), AFSADMSVR_KEYWORD_TIMED, AFSADMSVR_KEYWORD_MANUAL);

   UINT hInst;
   if ((hInst = WinExec (szFile, SW_HIDE)) <= 32)
      {
      if (pStatus)
         *pStatus = (DWORD)hInst;
      return FALSE;
      }

   return TRUE;
}


BOOL ValidateBinding (RPC_NS_HANDLE hBind, UINT_PTR *pidClient, ULONG *pStatus)
{
   RPC_NS_HANDLE hBindOld = hBindTaAfsAdminSvr;
   BOOL rc = FALSE;
   ULONG status = RPC_S_CALL_FAILED_DNE;

   hBindTaAfsAdminSvr = hBind;

   RpcTryExcept
      {
      STRING szMyName;
      gethostname (szMyName, cchSTRING);

      rc = AfsAdmSvr_Connect (szMyName, pidClient, &status);
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc)
      hBindTaAfsAdminSvr = hBindOld;
   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


LPCTSTR ADMINAPI ResolveAddress (LPCTSTR pszAddress)
{
   if (!pszAddress || !*pszAddress)
      return NULL;

   // The caller may have specified an IP address or a server name.
   // If the former, we're done; if the latter, we'll have to look up
   // the server's IP address.
   //
   if ((*pszAddress >= TEXT('0')) && (*pszAddress <= TEXT('9')))
      return pszAddress;

   HOSTENT *pEntry;
   if ((pEntry = gethostbyname (pszAddress)) == NULL)
      return pszAddress;  // we'll try it by name, but it probly won't work.

   try {
      static TCHAR szResolved[ 1024 ];
      lstrcpy (szResolved, inet_ntoa (*(struct in_addr *)pEntry->h_addr));
      return szResolved;
   } catch (...) {
      return pszAddress;  // we'll try it by name, but it probly won't work.
   }
}

