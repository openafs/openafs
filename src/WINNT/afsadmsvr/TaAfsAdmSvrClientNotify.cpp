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

typedef struct
   {
   ASID idCell;
   ASID idObject;
   HWND hNotify;
   } LISTENER, *LPLISTENER;

static struct
   {
   LPHASHLIST pListeners;
   LPHASHLISTKEY pListenersKeyObject;

   HWND *ahActionListeners;
   size_t chActionListeners;
   } l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK ListenersKeyObject_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
HASHVALUE CALLBACK ListenersKeyObject_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
HASHVALUE CALLBACK ListenersKeyObject_HashData (LPHASHLISTKEY pKey, PVOID pData);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL AddObjectNotification (HWND hNotify, ASID idCell, ASID idObject)
{
   asc_Enter();

   if (!l.pListeners)
      {
      l.pListeners = New (HASHLIST);
      l.pListenersKeyObject = l.pListeners->CreateKey (TEXT("idObject"), ListenersKeyObject_Compare, ListenersKeyObject_HashObject, ListenersKeyObject_HashData);
      }

   LPLISTENER pl = New (LISTENER);
   pl->idCell = idCell;
   pl->idObject = idObject;
   pl->hNotify = hNotify;
   l.pListeners->Add (pl);

   asc_Leave();
   return TRUE;
}


void ClearObjectNotifications (HWND hNotify)
{
   asc_Enter();

   if (l.pListeners)
      {
      for (LPENUM pEnum = l.pListeners->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         LPLISTENER pl = (LPLISTENER)( pEnum->GetObject() );
         if (pl->hNotify == hNotify)
            {
            l.pListeners->Remove (pl);
            Delete (pl);
            }
         }
      }

   asc_Leave();
}


void TestForNotifications (UINT_PTR idClient, ASID idCell, ASID idObject)
{
   if (l.pListeners)
      {
      // First we'll zip through our list of listeners and
      // build an ASIDLIST reflecting the objects in this cell
      // for which we're listening.
      //
      LPASIDLIST pAsidList = NULL;
      for (LPENUM pEnum = l.pListeners->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         LPLISTENER pl = (LPLISTENER)( pEnum->GetObject() );
         if (pl->idCell != idCell)
            continue;
         if (idObject && (pl->idObject != idObject))
            continue;

         if (!pAsidList)
            {
            if (!asc_AsidListCreate (&pAsidList))
               break;
            }
         if (!asc_AsidListAddEntry (&pAsidList, pl->idObject, 0))
            break;
         }

      // Then we'll call one of our cache routines, which in turn will tell
      // the admin server what version of the properties we have for each
      // of these objects; if any have newer properties available, we'll
      // get them back--and that will cause us to send out notifications to
      // our listeners.
      //
      if (pAsidList)
         {
         ULONG status;
         (void)RefreshCachedProperties (idClient, idCell, pAsidList, GET_ALL_DATA, &status);
         }
      }
}


void NotifyObjectListeners (ASID idCell, ASID idObject)
{
   // If we get here, our cache of information for the specified object
   // has just been updated. Check for listeners who may be interested
   // in changes to this object.
   //
   if (l.pListeners)
      {
      for (LPENUM pEnum = l.pListenersKeyObject->FindFirst (&idObject); pEnum; pEnum = pEnum->FindNext())
         {
         LPLISTENER pl = (LPLISTENER)( pEnum->GetObject() );
         if (pl->idCell != idCell)
            continue;

         if (!IsWindow (pl->hNotify))
            {
            l.pListeners->Remove (pl);
            Delete (pl);
            continue;
            }

         PostMessage (pl->hNotify, WM_ASC_NOTIFY_OBJECT, (WPARAM)0, (LPARAM)idObject);
         }
      }
}


/*
 * HASHLIST KEYS ______________________________________________________________
 *
 */

BOOL CALLBACK ListenersKeyObject_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return (((LPLISTENER)pObject)->idObject == *(ASID*)pData) ? TRUE : FALSE;
}

HASHVALUE CALLBACK ListenersKeyObject_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return ListenersKeyObject_HashData (pKey, &((LPLISTENER)pObject)->idObject);
}

HASHVALUE CALLBACK ListenersKeyObject_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HASHVALUE)*(ASID*)pData;
}



/*
 * ACTION NOTIFICATIONS _______________________________________________________
 *
 */

BOOL SetActionNotification (HWND hNotify, BOOL fSet)
{
   BOOL rc = TRUE;
   size_t ii = 0;

   asc_Enter();

   if (!fSet)
      {
      for (ii = 0; ii < l.chActionListeners; ++ii)
         {
         if (l.ahActionListeners[ ii ] == hNotify)
            l.ahActionListeners[ ii ] = NULL;
         }
      }
   else // (fSet)
      {
      for (ii = 0; ii < l.chActionListeners; ++ii)
         {
         if (l.ahActionListeners[ ii ] == NULL)
            break;
         }
      if (!REALLOC (l.ahActionListeners, l.chActionListeners, 1+ii, 1))
         {
         rc = FALSE;
         }
      else
         {
         l.ahActionListeners[ ii ] = hNotify;
         }
      }

   asc_Leave();
   return rc;
}


void NotifyActionListeners (LPASACTION pAction, BOOL fFinished)
{
   asc_Enter();

   for (size_t ii = 0; ii < l.chActionListeners; ++ii)
      {
      if (IsWindow (l.ahActionListeners[ ii ]))
         {
         LPASACTION pActionPost = New (ASACTION);
         memcpy (pActionPost, pAction, sizeof(ASACTION));
         PostMessage (l.ahActionListeners[ ii ], WM_ASC_NOTIFY_ACTION, (WPARAM)fFinished, (LPARAM)pActionPost);
         }
      }

   asc_Leave();
}

