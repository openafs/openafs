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

extern "C" {
#include <afs/afs_AdminErrors.h>
} // extern "C"


/*
 * VARIABLES __________________________________________________________________
 *
 */

#define cminREQ_CLIENT_PING   (csecAFSADMSVR_CLIENT_PING * 2L / 60L)

#define cREALLOC_CLIENTS          1
#define cREALLOC_OPERATIONS      16

#define cminAUTO_SHUTDOWN         2 // stop if idle for more than 2min
#define cminAUTO_SHUTDOWN_SLEEP   1 // test for stop every minute

typedef struct
   {
   TCHAR szName[ cchSTRING ];
   SOCKADDR_IN ipAddress;
   DWORD timeLastPing;
   } CLIENTINFO, *LPCLIENTINFO;

typedef struct
   {
   BOOL fInUse;
   UINT_PTR idClient;
   LPASACTION pAction;
   DWORD dwTickStart;
   } OPERATION, *LPOPERATION;

static struct
   {
   BOOL fOperational;
   LPCRITICAL_SECTION pcsAfsAdmSvr;

   LPCLIENTINFO *aClients;
   size_t cClients;
   size_t cClientsAllocated;

   LPNOTIFYCALLBACK pNotify;

   BOOL fAutoShutdown;
   HANDLE hThreadShutdown;
   DWORD timeLastIdleStart;

   OPERATION *aOperations;
   size_t cOperations;
   size_t cOperationsAllocated;
   DWORD idActionLast;

   DWORD dwScopeMin;
   } l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void AfsAdmSvr_TestShutdown (void);
DWORD WINAPI AfsAdmSvr_AutoShutdownThread (LPVOID lp);


/*
 * SYNCHRONIZATION ____________________________________________________________
 *
 */

void AfsAdmSvr_Enter (void)
{
   if (!l.pcsAfsAdmSvr)
      {
      l.pcsAfsAdmSvr = New (CRITICAL_SECTION);
      InitializeCriticalSection (l.pcsAfsAdmSvr);
      }
   EnterCriticalSection (l.pcsAfsAdmSvr);
}

void AfsAdmSvr_Leave (void)
{
   LeaveCriticalSection (l.pcsAfsAdmSvr);
}


/*
 * CLIENT INFORMATION _________________________________________________________
 *
 */

BOOL AfsAdmSvr_fIsValidClient (UINT_PTR idClient)
{
   BOOL rc = FALSE;
   AfsAdmSvr_Enter();

   for (size_t iClient = 0; !rc && iClient < l.cClientsAllocated; ++iClient)
      {
      if (idClient == (UINT_PTR)l.aClients[ iClient ])
         {
         if (l.aClients[ iClient ]->timeLastPing + cminREQ_CLIENT_PING > AfsAdmSvr_GetCurrentTime())
            rc = TRUE;
         }
      }

   AfsAdmSvr_Leave();
   return rc;
}


BOOL AfsAdmSvr_AttachClient (LPCTSTR pszName, PVOID *pidClient, ULONG *pStatus)
{
   AfsAdmSvr_Enter();
   size_t iClient;
   for (iClient = 0; iClient < l.cClientsAllocated; ++iClient)
      {
      if (!l.aClients[ iClient ])
         break;
      }
   if (!REALLOC (l.aClients, l.cClientsAllocated, 1+iClient, cREALLOC_CLIENTS))
      {
      *pidClient = NULL;
      return FALSE;
      }
   if ((l.aClients[ iClient ] = New (CLIENTINFO)) == NULL)
      {
      *pidClient = NULL;
      return FALSE;
      }
   memset (l.aClients[ iClient ], 0x00, sizeof(CLIENTINFO));
   lstrcpy (l.aClients[ iClient ]->szName, pszName);
   l.aClients[ iClient ]->timeLastPing = AfsAdmSvr_GetCurrentTime();
   l.cClients ++;

   if (!AfsAdmSvr_ResolveName (&l.aClients[ iClient ]->ipAddress, l.aClients[ iClient ]->szName))
      memset (&l.aClients[ iClient ]->ipAddress, 0x00, sizeof(SOCKADDR_IN));

   *pidClient = (PVOID)(l.aClients[ iClient ]);
   AfsAdmSvr_Leave();
   return TRUE;
}


void AfsAdmSvr_DetachClient (UINT_PTR idClient)
{
   AfsAdmSvr_Enter();
   size_t iClient;
   for (iClient = 0; iClient < l.cClientsAllocated; ++iClient)
      {
      if (idClient == (UINT_PTR)(l.aClients[ iClient ]))
         break;
      }
   if (iClient < l.cClientsAllocated)
      {
      Delete (l.aClients[ iClient ]);
      l.aClients[ iClient ] = NULL;
      l.cClients --;
      }
   AfsAdmSvr_TestShutdown();
   AfsAdmSvr_Leave();
}


LPCTSTR AfsAdmSvr_GetClientName (UINT_PTR idClient)
{
   static TCHAR szName[ cchSTRING ];
   LPCTSTR pszName = NULL;
   AfsAdmSvr_Enter();

   for (size_t iClient = 0; !pszName && iClient < l.cClientsAllocated; ++iClient)
      {
      if (idClient == (UINT_PTR)(l.aClients[ iClient ]))
         {
         lstrcpy (szName, l.aClients[ iClient ]->szName);
         pszName = szName;
         }
      }

   AfsAdmSvr_Leave();
   return pszName;
}


LPSOCKADDR_IN AfsAdmSvr_GetClientAddress (UINT_PTR idClient)
{
   static SOCKADDR_IN ipAddress;
   LPSOCKADDR_IN pAddress = NULL;
   AfsAdmSvr_Enter();

   for (size_t iClient = 0; !pAddress && iClient < l.cClientsAllocated; ++iClient)
      {
      if (idClient == (UINT_PTR)(l.aClients[ iClient ]))
         {
         memcpy (&ipAddress, &l.aClients[ iClient ]->ipAddress, sizeof(SOCKADDR_IN));
         pAddress = &ipAddress;
         }
      }

   AfsAdmSvr_Leave();
   return pAddress;
}


void AfsAdmSvr_PingClient (UINT_PTR idClient)
{
   AfsAdmSvr_Enter();

   for (size_t iClient = 0; iClient < l.cClientsAllocated; ++iClient)
      {
      if (idClient == (UINT_PTR)(l.aClients[ iClient ]))
         {
         l.aClients[ iClient ]->timeLastPing = AfsAdmSvr_GetCurrentTime();
         }
      }

   AfsAdmSvr_Leave();
}


DWORD AfsAdmSvr_GetCurrentTime (void) // returns counter in ~minute increments
{
   static WORD wMonthish = 0;  // One "Monthish" is 49.7 days
   static WORD wTickLast = 0;
   DWORD dwTick = GetTickCount();
   WORD wTickNow = HIWORD(dwTick);
   if (wTickNow < wTickLast)  // wrapped over a Monthish?
      ++wMonthish;
   wTickLast = wTickNow;
   return MAKELONG(wTickNow,wMonthish);
}


/*
 * STARTUP/SHUTDOWN ___________________________________________________________
 *
 */

void AfsAdmSvr_Startup (void)
{
   l.pNotify = New2 (NOTIFYCALLBACK,(AfsAdmSvr_NotifyCallback, 0));
   l.fOperational = FALSE;

   ULONG status;
   if (AfsClass_Initialize (&status))
      l.fOperational = TRUE;
   else
      {
      Print (dlERROR, TEXT("Could not initialize AfsClass (fatal error 0x%08lX)"), status);
      Print (dlERROR, TEXT("Remaining active to tell potential clients about the problem"));
      }

   if (!l.hThreadShutdown)
      {
      DWORD dwThreadID;
      l.hThreadShutdown = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)AfsAdmSvr_AutoShutdownThread, (LPVOID)0, 0, &dwThreadID);
      }
}


void AfsAdmSvr_Shutdown (void)
{
   Delete (l.pNotify);
   l.pNotify = NULL;
   l.fOperational = FALSE;
}


/*
 * GENERAL ____________________________________________________________________
 *
 */

BOOL FALSE_ (ULONG status, ULONG *pStatus, size_t iOp)
{
   if (pStatus)
      *pStatus = status;
   if (iOp != (size_t)-2)
      AfsAdmSvr_EndOperation (iOp);
   return FALSE;
}

BOOL Leave_FALSE_ (ULONG status, ULONG *pStatus, size_t iOp)
{
   AfsAdmSvr_Leave();
   if (pStatus)
      *pStatus = status;
   if (iOp != (size_t)-2)
      AfsAdmSvr_EndOperation (iOp);
   return FALSE;
}

PVOID NULL_ (ULONG status, ULONG *pStatus, size_t iOp)
{
   if (pStatus)
      *pStatus = status;
   if (iOp != (size_t)-2)
      AfsAdmSvr_EndOperation (iOp);
   return NULL;
}

PVOID Leave_NULL_ (ULONG status, ULONG *pStatus, size_t iOp)
{
   AfsAdmSvr_Leave();
   if (pStatus)
      *pStatus = status;
   if (iOp != (size_t)-2)
      AfsAdmSvr_EndOperation (iOp);
   return NULL;
}

BOOL TRUE_ (ULONG *pStatus, size_t iOp)
{
   if (pStatus)
      *pStatus = 0;
   if (iOp != (size_t)-2)
      AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}

BOOL Leave_TRUE_ (ULONG *pStatus, size_t iOp)
{
   AfsAdmSvr_Leave();
   if (pStatus)
      *pStatus = 0;
   if (iOp != (size_t)-2)
      AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}

IDENTTYPE GetAsidType (ASID idObject)
{
   IDENTTYPE iType;
   try {
      iType = ((LPIDENT)idObject)->GetType();
   } catch(...) {
      iType = itUNUSED;
   }
   return iType;
}


BOOL AfsAdmSvr_ResolveName (LPSOCKADDR_IN pAddress, LPTSTR pszName)
{
   if ((pszName[0] >= TEXT('0')) && (pszName[0] <= TEXT('9')))
      {
      int ipAddress;
      if ((ipAddress = inet_addr (pszName)) == INADDR_NONE)
         return FALSE;

      memset (pAddress, 0x00, sizeof(SOCKADDR_IN));
      pAddress->sin_family = AF_INET;
      pAddress->sin_addr.s_addr = ipAddress;

      HOSTENT *pEntry;
      if ((pEntry = gethostbyaddr ((char*)&ipAddress, sizeof(ipAddress), AF_INET)) != NULL)
         lstrcpy (pszName, pEntry->h_name);
      }
   else // (!isdigit(szServer[0]))
      {
      HOSTENT *pEntry;
      if ((pEntry = gethostbyname (pszName)) == NULL)
         return FALSE;

      memset (pAddress, 0x00, sizeof(SOCKADDR_IN));
      pAddress->sin_family = AF_INET;
      pAddress->sin_addr.s_addr = *(int *)pEntry->h_addr;

      lstrcpy (pszName, pEntry->h_name);
      }

   return TRUE;
}


/*
 * AUTO-SHUTDOWN ______________________________________________________________
 *
 */

DWORD WINAPI AfsAdmSvr_AutoShutdownThread (LPVOID lp)
{
   for (;;)
      {
      AfsAdmSvr_Enter();

      BOOL fShutdown = l.fAutoShutdown;

      // If there are any clients connected, forcably disconnect any
      // that haven't pinged us for too long
      //
      for (size_t iClient = 0; iClient < l.cClientsAllocated; ++iClient)
         {
         if (!l.aClients[ iClient ])
            continue;
         if (l.aClients[ iClient ]->timeLastPing + cminREQ_CLIENT_PING <= AfsAdmSvr_GetCurrentTime())
            {
            Print (dlCONNECTION, "Client 0x%08lX idle for too long; detaching", l.aClients[ iClient ]);
            AfsAdmSvr_DetachClient ((UINT_PTR)l.aClients[ iClient ]);
            }
         }

      // If any operations are in progress, we can't shutdown.
      //
      if (l.cOperations)
         fShutdown = FALSE;

      // If any clients are still connected, we can't shutdown.
      //
      if (l.cClients)
         fShutdown = FALSE;

      // If we haven't been idle long enough, we can't shutdown
      //
      if (!l.timeLastIdleStart)
         fShutdown = FALSE;
      else if (l.timeLastIdleStart + cminAUTO_SHUTDOWN > AfsAdmSvr_GetCurrentTime())
         fShutdown = FALSE;

      // That's it; can we stop now?
      //
      if (fShutdown)
         {
         Print ("Idle for too long; shutting down.");
         RpcMgmtStopServerListening (NULL);
         AfsAdmSvr_StopCallbackManagers();
         }

      AfsAdmSvr_Leave();

      if (fShutdown)
         break;

      Sleep (cminAUTO_SHUTDOWN_SLEEP * 60L * 1000L);
      }

   return 0;
}



void AfsAdmSvr_EnableAutoShutdown (BOOL fEnable)
{
   AfsAdmSvr_Enter();

   l.fAutoShutdown = fEnable;

   if (fEnable)
      Print (dlDETAIL, TEXT("Auto-shutdown enabled, trigger = %lu minutes idle time"), cminAUTO_SHUTDOWN);
   else
      Print (dlDETAIL, TEXT("Auto-shutdown on idle disabled"));

   AfsAdmSvr_Leave();
}


void AfsAdmSvr_TestShutdown (void)
{
   if (!l.cOperations && !l.cClients)
      {
      l.timeLastIdleStart = AfsAdmSvr_GetCurrentTime();
      }
}


size_t AfsAdmSvr_BeginOperation (UINT_PTR idClient, LPASACTION pAction)
{
   AfsAdmSvr_Enter();

   ++l.cOperations;

   size_t iOp;
   for (iOp = 0; iOp < l.cOperationsAllocated; ++iOp)
      {
      if (!l.aOperations[ iOp ].fInUse)
         break;
      }
   if (!REALLOC (l.aOperations, l.cOperationsAllocated, 1+iOp, cREALLOC_OPERATIONS))
      {
      AfsAdmSvr_Leave();
      return (size_t)(-1);
      }

   l.aOperations[ iOp ].idClient = idClient;
   l.aOperations[ iOp ].pAction = NULL;
   l.aOperations[ iOp ].fInUse = TRUE;

   if (pAction)
      {
      l.aOperations[ iOp ].pAction = New (ASACTION);
      memcpy (l.aOperations[ iOp ].pAction, pAction, sizeof(ASACTION));
      l.aOperations[ iOp ].pAction->idAction = ++l.idActionLast;
      l.aOperations[ iOp ].pAction->idClient = idClient;
      l.aOperations[ iOp ].pAction->csecActive = 0;

      TCHAR szDesc[256];
      switch (l.aOperations[ iOp ].pAction->Action)
         {
         case ACTION_REFRESH:
            wsprintf (szDesc, TEXT("Refresh (scope=0x%08lX)"), l.aOperations[ iOp ].pAction->u.Refresh.idScope);
            break;
         case ACTION_SCOUT:
            wsprintf (szDesc, TEXT("Scout (scope=0x%08lX)"), l.aOperations[ iOp ].pAction->u.Refresh.idScope);
            break;
         case ACTION_USER_CHANGE:
            wsprintf (szDesc, TEXT("ChangeUser (user=0x%08lX)"), l.aOperations[ iOp ].pAction->u.User_Change.idUser);
            break;
         case ACTION_USER_PW_CHANGE:
            wsprintf (szDesc, TEXT("SetUserPassword (user=0x%08lX)"), l.aOperations[ iOp ].pAction->u.User_Pw_Change.idUser);
            break;
         case ACTION_USER_UNLOCK:
            wsprintf (szDesc, TEXT("UnlockUser (user=0x%08lX)"), l.aOperations[ iOp ].pAction->u.User_Unlock.idUser);
            break;
         case ACTION_USER_CREATE:
            wsprintf (szDesc, TEXT("CreateUser (user=%s)"), l.aOperations[ iOp ].pAction->u.User_Create.szUser);
            break;
         case ACTION_USER_DELETE:
            wsprintf (szDesc, TEXT("CreateUser (user=0x%08lX)"), l.aOperations[ iOp ].pAction->u.User_Delete.idUser);
            break;
         case ACTION_GROUP_CHANGE:
            wsprintf (szDesc, TEXT("ChangeGroup (group=0x%08lX)"), l.aOperations[ iOp ].pAction->u.Group_Change.idGroup);
            break;
         case ACTION_GROUP_MEMBER_ADD:
            wsprintf (szDesc, TEXT("AddGroupMember (group=0x%08lX, user=0x%08lX)"), l.aOperations[ iOp ].pAction->u.Group_Member_Add.idGroup, l.aOperations[ iOp ].pAction->u.Group_Member_Add.idUser);
            break;
         case ACTION_GROUP_MEMBER_REMOVE:
            wsprintf (szDesc, TEXT("RemoveGroupMember (group=0x%08lX, user=0x%08lX)"), l.aOperations[ iOp ].pAction->u.Group_Member_Remove.idGroup, l.aOperations[ iOp ].pAction->u.Group_Member_Remove.idUser);
            break;
         case ACTION_GROUP_RENAME:
            wsprintf (szDesc, TEXT("RenameGroup (group=0x%08lX, new name=%s)"), l.aOperations[ iOp ].pAction->u.Group_Rename.idGroup, l.aOperations[ iOp ].pAction->u.Group_Rename.szNewName);
            break;
         case ACTION_GROUP_DELETE:
            wsprintf (szDesc, TEXT("CreateGroup (group=0x%08lX)"), l.aOperations[ iOp ].pAction->u.Group_Delete.idGroup);
            break;
         case ACTION_CELL_CHANGE:
            wsprintf (szDesc, TEXT("ChangeCell (cell=0x%08lX)"), l.aOperations[ iOp ].pAction->idCell);
            break;
         default:
            wsprintf (szDesc, TEXT("Unknown Action (#%lu)"), l.aOperations[ iOp ].pAction->Action);
            break;
         }
      Print (dlOPERATION, TEXT("Starting action 0x%08lX: %s"), l.aOperations[ iOp ].pAction->idAction, szDesc);

      AfsAdmSvr_PostCallback (cbtACTION, FALSE, l.aOperations[ iOp ].pAction);
      }

   l.aOperations[ iOp ].dwTickStart = GetTickCount();
   AfsAdmSvr_Leave();
   return iOp;
}


void AfsAdmSvr_EndOperation (size_t iOp)
{
   AfsAdmSvr_Enter();

   if ((iOp != (size_t)-1) && (iOp < l.cOperationsAllocated) && (l.aOperations[ iOp ].fInUse))
      {
      if (l.aOperations[ iOp ].pAction)
         {
         Print (dlOPERATION, TEXT("Ending action 0x%08lX"), l.aOperations[ iOp ].pAction->idAction);
         AfsAdmSvr_PostCallback (cbtACTION, TRUE, l.aOperations[ iOp ].pAction);
         Delete (l.aOperations[ iOp ].pAction);
         }
      memset (&l.aOperations[ iOp ], 0x00, sizeof(l.aOperations[ iOp ]));
      l.cOperations --;
      }

   AfsAdmSvr_TestShutdown();
   AfsAdmSvr_Leave();
}


BOOL AfsAdmSvr_GetOperation (DWORD idAction, LPASACTION pAction)
{
   AfsAdmSvr_Enter();

   for (size_t iOp = 0; iOp < l.cOperationsAllocated; ++iOp)
      {
      if (!l.aOperations[ iOp ].fInUse)
         continue;
      if (!l.aOperations[ iOp ].pAction)
         continue;
      if (l.aOperations[ iOp ].pAction->idAction != idAction)
         continue;

      memcpy (pAction, l.aOperations[ iOp ].pAction, sizeof(ASACTION));
      pAction->csecActive = (GetTickCount() - l.aOperations[ iOp ].dwTickStart) / 1000;
      AfsAdmSvr_Leave();
      return TRUE;
      }

   AfsAdmSvr_Leave();
   return FALSE;
}


LPASACTIONLIST AfsAdmSvr_GetOperations (UINT_PTR idClientSearch, ASID idCellSearch)
{
   LPASACTIONLIST pList = AfsAdmSvr_CreateActionList();
   AfsAdmSvr_Enter();

   for (WORD iOp = 0; iOp < l.cOperationsAllocated; ++iOp)
      {
      if (!l.aOperations[ iOp ].fInUse)
         continue;
      if (!l.aOperations[ iOp ].pAction)
         continue;
      if (idClientSearch && ((UINT_PTR)idClientSearch != l.aOperations[ iOp ].pAction->idClient))
         continue;
      if (idCellSearch && (idCellSearch != l.aOperations[ iOp ].pAction->idCell))
         continue;

      ASACTION Action;
      memcpy (&Action, l.aOperations[ iOp ].pAction, sizeof(ASACTION));
      Action.csecActive = (GetTickCount() - l.aOperations[ iOp ].dwTickStart) / 1000;
      if (!AfsAdmSvr_AddToActionList (&pList, &Action))
         {
         AfsAdmSvr_FreeActionList (&pList);
         break;
         }
      }

   AfsAdmSvr_Leave();
   return pList;
}


void AfsAdmSvr_Action_StartRefresh (ASID idScope)
{
   switch (GetAsidType (idScope))
      {
      case itCELL:
         AfsAdmSvr_MarkRefreshThread (idScope);
         // fall through

      case itSERVER:
         ASACTION Action;
         memset (&Action, 0x00, sizeof(Action));
         Action.Action = ACTION_REFRESH;
         Action.idCell = (ASID)( ((LPIDENT)idScope)->GetCell() );
         Action.u.Refresh.idScope = idScope;
         (void)AfsAdmSvr_BeginOperation (0, &Action);
         break;

      default:
         // Don't bother listing status-refreshes as ongoing operations
         // for any granularity smaller than the server; they'll occur
         // really frequently, and finish really quickly.
         break;
      }
}


void AfsAdmSvr_Action_StopRefresh (ASID idScope)
{
   AfsAdmSvr_Enter();

   for (size_t iOp = 0; iOp < l.cOperationsAllocated; ++iOp)
      {
      if (!l.aOperations[ iOp ].fInUse)
         continue;
      if (!l.aOperations[ iOp ].pAction)
         continue;
      if (l.aOperations[ iOp ].pAction->Action != ACTION_REFRESH)
         continue;
      if (l.aOperations[ iOp ].pAction->u.Refresh.idScope != idScope)
         continue;

      AfsAdmSvr_EndOperation (iOp);
      break;
      }

   if (GetAsidType (idScope) == itCELL)
      {
      AfsAdmSvr_MarkRefreshThread (idScope);
      }

   AfsAdmSvr_Leave();
}


DWORD WINAPI AfsAdmSvr_AutoOpen_ThreadProc (PVOID lp)
{
   DWORD dwScope = PtrToUlong(lp);
   ULONG status;

   if (!l.fOperational)
      return 0;

   // First we'll have to find out which cell to open
   //
   TCHAR szCell[ cchNAME ];
   if (!CELL::GetDefaultCell (szCell, &status))
      {
      Print (dlERROR, TEXT("CELL::GetDefaultCell failed; error 0x%08lX"), status);
      }
   else
      {
      // Then try to actually open the cell
      //
      Print (dlSTANDARD, TEXT("Auto-opening cell %s; scope=%s"), szCell, (dwScope == (AFSADMSVR_SCOPE_VOLUMES | AFSADMSVR_SCOPE_USERS)) ? TEXT("full") : (dwScope == AFSADMSVR_SCOPE_VOLUMES) ? TEXT("volumes") : TEXT("users"));

      LPIDENT lpiCell;
      if ((lpiCell = CELL::OpenCell ((LPTSTR)szCell, &status)) == NULL)
         {
         Print (dlERROR, TEXT("Auto-open of cell %s failed; error 0x%08lX"), szCell, status);
         }
      else
         {
         LPCELL lpCell;
         if ((lpCell = lpiCell->OpenCell (&status)) == NULL)
            {
            Print (dlERROR, TEXT("Auto-open: OpenCell failed; error 0x%08lX"), status);
            }
         else
            {
            AfsAdmSvr_AddToMinScope (dwScope);
            if (!lpCell->RefreshAll (&status))
               Print (dlERROR, TEXT("Auto-open: RefreshCell failed; error 0x%08lX"), status);
            else
               Print (dlSTANDARD, TEXT("Auto-open of cell %s successful"), szCell);
            lpCell->Close();

            // We intentionally do not call CELL::CloseCell() here--as would
            // ordinarily be necessary to balance our CELL::OpenCell() call
            // above--because we never want to close our cache for this cell.
            // The point of calling AutoOpen() up front is to keep an admin
            // server alive and ready for use on a particular cell--calling
            // CELL::CloseCell() here negates that purpose.

            }
         }
      }

   return 0;
}


void AfsAdmSvr_AddToMinScope (DWORD dwScope)
{
   l.dwScopeMin |= dwScope;
   AfsClass_SpecifyRefreshDomain (l.dwScopeMin);
}


void AfsAdmSvr_SetMinScope (DWORD dwScope)
{
   l.dwScopeMin = dwScope;
}


DWORD AfsAdmSvr_GetMinScope (void)
{
   return l.dwScopeMin;
}

