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

#include "TaAfsAdmSvrInternal.h"

/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   CALLBACKTYPE Type;
   BOOL fFinished;
   LPASACTION pAction;
   } CALLBACKDATA, *LPCALLBACKDATA;

static struct
   {
   HANDLE heCallback;
   LPHASHLIST pListCallbacks;
   BOOL fStopManagers;
   size_t cManagers;
   } l;


/*
 * CALLBACKS __________________________________________________________________
 *
 */

void AfsAdmSvr_FreeCallbackData (LPCALLBACKDATA pData)
{
   if (pData)
      {
      if (pData->pAction)
         Delete (pData->pAction);
      Delete (pData);
      }
}


void AfsAdmSvr_CallbackManager (void)
{
   AfsAdmSvr_Enter();
   if ((++l.cManagers) == 1)
      {
      l.heCallback = CreateEvent (NULL, TRUE, FALSE, TEXT("AfsAdmSvr_CallbackManager Event"));
      l.pListCallbacks = New (HASHLIST);
      }
   AfsAdmSvr_Leave();

   for (;;)
      {
      WaitForSingleObjectEx (l.heCallback, INFINITE, FALSE);

      if (l.fStopManagers)
         break;

      // We must ensure that we don't block the server's operations because
      // a callback doesn't go through; since other operations may need
      // access to the l.pListCallbacks structure in order to queue new
      // callbacks, we can't leave it locked by issuing callbacks while
      // enumerating it. Instead we'll copy the list into a local copy,
      // clear it, and enumerate that local copy--other threads can then
      // continue to add new requests to l.pListCallbacks.
      //
      AfsAdmSvr_Enter();

      LPHASHLIST pList = New (HASHLIST);
      LPENUM pEnum;
      for (pEnum = l.pListCallbacks->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         LPCALLBACKDATA pData = (LPCALLBACKDATA)( pEnum->GetObject() );
         pList->Add (pData);
         l.pListCallbacks->Remove (pData);
         }

      ResetEvent (l.heCallback);
      AfsAdmSvr_Leave();

      // Now enumerate that copied list, and issue callbacks for each item.
      //
      for (pEnum = pList->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         LPCALLBACKDATA pData = (LPCALLBACKDATA)( pEnum->GetObject() );

         try {
            switch (pData->Type)
               {
               case cbtACTION:
                  AfsAdmSvrCallback_Action (pData->pAction, pData->fFinished);
                  break;
               }
         } catch(...) {
            ;
         }

         pList->Remove (pData);
         AfsAdmSvr_FreeCallbackData (pData);
         }

      Delete (pList);
      }

   AfsAdmSvr_Enter();
   if ((--l.cManagers) == 0)
      {
      Delete (l.pListCallbacks);
      l.pListCallbacks = NULL;
      CloseHandle (l.heCallback);
      l.heCallback = NULL;
      }
   AfsAdmSvr_Leave();
}


void AfsAdmSvr_PostCallback (CALLBACKTYPE Type, BOOL fFinished, LPASACTION pAction, DWORD dwRemoveMe)
{
   AfsAdmSvr_Enter();
   if (l.pListCallbacks)
      {
      LPCALLBACKDATA pData = New (CALLBACKDATA);
      memset (pData, 0x00, sizeof(CALLBACKDATA));
      pData->Type = Type;
      pData->fFinished = fFinished;
      if (pAction)
         {
         pData->pAction = New (ASACTION);
         memcpy (pData->pAction, pAction, sizeof(ASACTION));
         }

      l.pListCallbacks->Add (pData);
      SetEvent (l.heCallback);
      }
   AfsAdmSvr_Leave();
}


void AfsAdmSvr_PostCallback (CALLBACKTYPE Type, BOOL fFinished, LPASACTION pAction)
{
   AfsAdmSvr_PostCallback (Type, fFinished, pAction, 0);
}


void AfsAdmSvr_StopCallbackManagers (void)
{
   AfsAdmSvr_Enter();
   if (l.cManagers)
      {
      l.fStopManagers = TRUE;
      SetEvent (l.heCallback);
      }
   AfsAdmSvr_Leave();
}

