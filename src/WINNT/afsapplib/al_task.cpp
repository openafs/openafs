/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <WINNT/afsapplib.h>


/*
 * VARIABLES __________________________________________________________________
 *
 */

static LPTASKQUEUE_PARAMS ptqp = NULL;

typedef struct TASKQUEUEITEM
   {
   struct TASKQUEUEITEM *pNext;
   int idTask;
   HWND hReply;
   PVOID lpUser;
   } TASKQUEUEITEM, *LPTASKQUEUEITEM;

static LPTASKQUEUEITEM pTaskQueuePop = NULL;
static LPTASKQUEUEITEM pTaskQueuePushAfter = NULL;
static CRITICAL_SECTION csTaskQueue;

static size_t nThreadsRunning = 0;
static size_t nRequestsActive = 0;



/*
 * PROTOTYPES _________________________________________________________________
 *
 */

DWORD WINAPI Task_ThreadProc (PVOID lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void AfsAppLib_InitTaskQueue (LPTASKQUEUE_PARAMS lpp)
{
   if (ptqp != NULL)
      {
      Delete (ptqp);
      ptqp = NULL;
      }

   if (lpp && lpp->fnCreateTaskPacket && lpp->fnPerformTask && lpp->fnFreeTaskPacket)
      {
      ptqp = New (TASKQUEUE_PARAMS);
      memcpy (ptqp, lpp, sizeof(TASKQUEUE_PARAMS));
      }
}


void StartTask (int idTask, HWND hReply, PVOID lpUser)
{
   if (!ptqp)
      return;

   static BOOL fStarted = FALSE;
   if (!fStarted)
      {
      fStarted = TRUE;
      InitializeCriticalSection (&csTaskQueue);
      }

   // Then push this notification onto our TaskQueue, so that the task
   // thread will pop off each request in turn and take a look at it.
   // Ideally, PostThreadMessage() and GetMessage() would implement all this
   // garbage, but that doesn't work properly for some reason...
   //
   EnterCriticalSection (&csTaskQueue);

   if ((++nRequestsActive) > nThreadsRunning)
      {
      if ((!ptqp->nThreadsMax) || (nThreadsRunning < (size_t)(ptqp->nThreadsMax)))
         {
         DWORD dwThreadID;
         HANDLE hThread;
         if ((hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)Task_ThreadProc, 0, 0, &dwThreadID)) != NULL)
            {
            SetThreadPriority (hThread, THREAD_PRIORITY_BELOW_NORMAL);
            ++nThreadsRunning;
            }
         }
      }

   LPTASKQUEUEITEM lptqi = New (TASKQUEUEITEM);
   lptqi->pNext = NULL;
   lptqi->idTask = idTask;
   lptqi->hReply = hReply;
   lptqi->lpUser = lpUser;

   if (pTaskQueuePushAfter != NULL)
      pTaskQueuePushAfter->pNext = lptqi;
   pTaskQueuePushAfter = lptqi;

   if (pTaskQueuePop == NULL)
      pTaskQueuePop = lptqi;

   LeaveCriticalSection (&csTaskQueue);
}


DWORD WINAPI Task_ThreadProc (PVOID lp)
{
   BOOL fJustDidTask = FALSE;

   for (;;)
      {
      BOOL fFound = FALSE;
      TASKQUEUEITEM tqi;

      EnterCriticalSection (&csTaskQueue);

      if (fJustDidTask)
         {
         --nRequestsActive;
         fJustDidTask = FALSE;
         }

      if (pTaskQueuePop != NULL)
         {
         LPTASKQUEUEITEM lptqiNext = pTaskQueuePop->pNext;
         tqi = *pTaskQueuePop;
         Delete (pTaskQueuePop);
         if (pTaskQueuePushAfter == pTaskQueuePop)
            pTaskQueuePushAfter = NULL;
         pTaskQueuePop = lptqiNext;
         fFound = TRUE;
         fJustDidTask = TRUE;
         }

      // is this thread unnecessary?--that is, if we couldn't find anything
      // to do, then kill off all threads.
      //
      if (!fFound)
         {
         --nThreadsRunning;
         LeaveCriticalSection (&csTaskQueue);
         break;
         }

      LeaveCriticalSection (&csTaskQueue);

      if (fFound)
         {
         LPTASKPACKET ptp;
         if ((ptp = (*ptqp->fnCreateTaskPacket)(tqi.idTask, tqi.hReply, tqi.lpUser)) != NULL)
            {
            (*ptqp->fnPerformTask)(ptp);
            }

         if (tqi.hReply && IsWindow (tqi.hReply))
            {
            PostMessage (tqi.hReply, WM_ENDTASK, 0, (LPARAM)ptp);
            }
         else if (ptp != NULL)
            {
            (*ptqp->fnFreeTaskPacket)(ptp);
            }
         }
      }

   return 0;
}

