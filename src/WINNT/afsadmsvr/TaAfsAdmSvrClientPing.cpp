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
#include <WINNT/AfsAppLib.h>


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct
   {
   HANDLE hPingThread;
   UINT_PTR *adwClients;
   size_t cdwClients;

   HANDLE hCallbackThread;
   size_t cReqCallback;
   } l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

DWORD WINAPI ClientPingThread (LPVOID lp);

DWORD WINAPI ClientCallbackThread (LPVOID lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void StartPingThread (UINT_PTR idClient)
{
   asc_Enter();

   size_t ii;
   for (ii = 0; ii < l.cdwClients; ++ii)
      {
      if (!l.adwClients[ ii ])
         break;
      }
   if (REALLOC (l.adwClients, l.cdwClients, 1+ii, 1))
      {
      l.adwClients[ ii ] = idClient;
      }

   if (!l.hPingThread)
      {
      DWORD dwThreadID;
      l.hPingThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)ClientPingThread, (LPVOID)0, 0, &dwThreadID);
      }

   asc_Leave();
}


void StopPingThread (UINT_PTR idClient)
{
   asc_Enter();

   for (size_t ii = 0; ii < l.cdwClients; ++ii)
      {
      if (l.adwClients[ ii ] == idClient)
         l.adwClients[ ii ] = 0;
      }

   asc_Leave();
}


DWORD WINAPI ClientPingThread (LPVOID lp)
{
   for (;;)
      {
      Sleep (csecAFSADMSVR_CLIENT_PING * 1000L);  // server adds race allowance

      asc_Enter();

      for (size_t ii = 0; ii < l.cdwClients; ++ii)
         {
         UINT_PTR idClient;
         if ((idClient = l.adwClients[ ii ]) == 0)
            continue;

         asc_Leave();

         RpcTryExcept
            {
            ULONG status;
            if (!AfsAdmSvr_Ping (idClient, &status))
               {
               if (status == ERROR_INVALID_HANDLE) // we've been disconnected!
                  StopPingThread (idClient);
               }
            }
         RpcExcept(1)
            ;
         RpcEndExcept

         asc_Enter();
         }

      asc_Leave();
      }

   l.hPingThread = NULL;
   return 0;
}


void StartCallbackThread (void)
{
   asc_Enter();
   if ((++l.cReqCallback) == 1)
      {
      DWORD dwThreadID;
      l.hCallbackThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)ClientCallbackThread, (LPVOID)0, 0, &dwThreadID);
      }
   asc_Leave();
}


void StopCallbackThread (void)
{
   asc_Enter();
   if (!(l.cReqCallback) || !(--l.cReqCallback))
      {
      if (l.hCallbackThread)
         {
         TerminateThread (l.hCallbackThread, 0);
         l.hCallbackThread = NULL;
         }
      }
   asc_Leave();
}


DWORD WINAPI ClientCallbackThread (LPVOID lp)
{
   // The callback thread's task is simple: it initiates a particular
   // RPC, which never returns. (Well, actually, it will return if the
   // server shuts down.) By leaving a thread active, the server has a
   // context in which to perform callback calls.
   //
   RpcTryExcept
      {
      AfsAdmSvr_CallbackHost();
      }
   RpcExcept(1)
      ;
   RpcEndExcept

   l.hCallbackThread = NULL;
   return 0;
}

