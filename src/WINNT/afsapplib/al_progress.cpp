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

#include <WINNT/al_progress.h>
#include <commctrl.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define WM_UPDATE_PROGRESS WM_TIMER


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * ROUTINES ___________________________________________________________________
 *
 */

PROGRESSDISPLAY::PROGRESSDISPLAY (HWND hWnd)
{
   Init (hWnd);
}


PROGRESSDISPLAY::PROGRESSDISPLAY (HWND hParent, int iddTemplate, DLGPROC dlgproc)
{
   if (dlgproc == 0)
      dlgproc = (DLGPROC)PROGRESSDISPLAY::ProgressDisplay_StubProc;
   HWND hWnd = ModelessDialogParam (iddTemplate, hParent, dlgproc, (LPARAM)this);
   Init (hWnd);
   m_fCreatedWindow = TRUE;
}


PROGRESSDISPLAY::~PROGRESSDISPLAY (void)
{
   m_fFinished = TRUE;
   SetWindowLong (m_hWnd, DWL_USER, (LONG)0);
   DeleteCriticalSection (&m_cs);
   if (m_fCreatedWindow)
      DestroyWindow (m_hWnd);
}


void PROGRESSDISPLAY::Init (HWND hWnd)
{
   SetWindowLong (hWnd, DWL_USER, (LONG)this);
   Subclass_AddHook (hWnd, PROGRESSDISPLAY::ProgressDisplay_HookProc);

   m_msgFinish = 0;
   m_fFinished = FALSE;
   m_dwStatus = 0;
   InitializeCriticalSection (&m_cs);
   m_hWnd = hWnd;
   m_fCreatedWindow = FALSE;
   m_pfnUser = NULL;
   m_lpUser = 0;
   m_iProgress = 0;
   m_cRef = 1;

   GetDlgItemText (m_hWnd, IDC_OPERATION, m_szOperation, cchRESOURCE);
   GetDlgItemText (m_hWnd, IDC_PROGRESSTEXT, m_szProgressText, cchRESOURCE);
   SendDlgItemMessage (m_hWnd, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0,100));
   SetProgressRange (0, 100);
   SetProgress (0);
}


HWND PROGRESSDISPLAY::GetWindow (void)
{
   return m_hWnd;
}

void PROGRESSDISPLAY::GetProgressRange (int *piStart, int *piFinish)
{
   EnterCriticalSection (&m_cs);

   if (piStart)
      *piStart = m_iProgressStart;
   if (piFinish)
      *piFinish = m_iProgressFinish;

   LeaveCriticalSection (&m_cs);
}


void PROGRESSDISPLAY::SetProgressRange (int iStart, int iFinish)
{
   EnterCriticalSection (&m_cs);

   m_iProgressStart = iStart;
   m_iProgressFinish = iFinish;
   PostMessage (m_hWnd, WM_UPDATE_PROGRESS, 0, 0);

   LeaveCriticalSection (&m_cs);
}


int PROGRESSDISPLAY::GetProgress (void)
{
   EnterCriticalSection (&m_cs);

   int iProgress = m_iProgress;

   LeaveCriticalSection (&m_cs);
   return iProgress;
}


void PROGRESSDISPLAY::SetProgress (int iProgress)
{
   EnterCriticalSection (&m_cs);

   m_iProgress = max( m_iProgress, iProgress );
   m_iProgress = min( max( m_iProgress, m_iProgressStart ), m_iProgressFinish );
   PostMessage (m_hWnd, WM_UPDATE_PROGRESS, 0, 0);

   LeaveCriticalSection (&m_cs);
}


void PROGRESSDISPLAY::GetOperation (LPTSTR pszOperation)
{
   EnterCriticalSection (&m_cs);

   lstrcpy (pszOperation, m_szOperation);

   LeaveCriticalSection (&m_cs);
}


void PROGRESSDISPLAY::SetOperation (LPCTSTR pszOperation)
{
   EnterCriticalSection (&m_cs);

   lstrcpy (m_szOperation, pszOperation);
   PostMessage (m_hWnd, WM_UPDATE_PROGRESS, 0, 0);

   LeaveCriticalSection (&m_cs);
}


LPPROGRESSDISPLAY PROGRESSDISPLAY::GetProgressDisplay (HWND hWnd)
{
   LPPROGRESSDISPLAY ppd = NULL;

   try {
      if ((ppd = (LPPROGRESSDISPLAY)(GetWindowLong (hWnd, DWL_USER))) != NULL) {
         if (ppd->m_hWnd != hWnd)
            ppd = NULL;
      }
   } catch(...) {
      ppd = NULL;
   }

   return ppd;
}


BOOL CALLBACK PROGRESSDISPLAY::ProgressDisplay_StubProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   return FALSE;
}


BOOL CALLBACK PROGRESSDISPLAY::ProgressDisplay_HookProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldproc = Subclass_FindNextHook (hWnd, PROGRESSDISPLAY::ProgressDisplay_HookProc);

   switch (msg)
      {
      case WM_UPDATE_PROGRESS:
         if (!wp && !lp)
            {
            LPPROGRESSDISPLAY ppd;
            if ((ppd = PROGRESSDISPLAY::GetProgressDisplay (hWnd)) != NULL)
               ppd->OnUpdate();
            return TRUE;
            }
         break;

      case WM_DESTROY:
         Subclass_RemoveHook (hWnd, PROGRESSDISPLAY::ProgressDisplay_HookProc);
         break;
      }

   if (oldproc)
      return CallWindowProc ((WNDPROC)oldproc, hWnd, msg, wp, lp);
   else
      return DefWindowProc (hWnd, msg, wp, lp);
}


void PROGRESSDISPLAY::SetFinishMessage (int msgFinish)
{
   m_msgFinish = msgFinish;
}


void PROGRESSDISPLAY::Show (DWORD (CALLBACK *pfn)(LPPROGRESSDISPLAY ppd, LPARAM lp), LPARAM lp)
{
   m_pfnUser = pfn;
   m_lpUser = lp;

   InterlockedIncrement (&m_cRef);

   ShowWindow (m_hWnd, SW_SHOW);

   DWORD dwThreadID;
   HANDLE hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)(PROGRESSDISPLAY::ThreadProc), this, 0, &dwThreadID);

   if (m_msgFinish == 0)
      {
      MSG msg;
      while (GetMessage (&msg, 0, 0, NULL))
         {
         if (!IsDialogMessage (m_hWnd, &msg))
            {
            TranslateMessage (&msg);
            DispatchMessage (&msg);
            }

         EnterCriticalSection (&m_cs);
         BOOL fBreak = m_fFinished;
         LeaveCriticalSection (&m_cs);
         if (fBreak)
            break;
         }
      }
}


void PROGRESSDISPLAY::Finish (DWORD dwStatus)
{
   EnterCriticalSection (&m_cs);

   m_fFinished = TRUE;
   m_dwStatus = dwStatus;
   if (m_msgFinish)
      PostMessage (m_hWnd, m_msgFinish, 0, 0);
   else
      PostMessage (m_hWnd, WM_UPDATE_PROGRESS, 0, 0);

   LeaveCriticalSection (&m_cs);

   if (InterlockedDecrement (&m_cRef) == 0)
      Delete (this);
}


DWORD PROGRESSDISPLAY::GetStatus (void)
{
   EnterCriticalSection (&m_cs);
   DWORD dwStatus = (m_fFinished) ? m_dwStatus : ERROR_BUSY;
   LeaveCriticalSection (&m_cs);
   return dwStatus;
}


void PROGRESSDISPLAY::Close (void)
{
   EnterCriticalSection (&m_cs);

   if (m_fCreatedWindow)
      {
      DestroyWindow (m_hWnd);
      m_hWnd = NULL;
      }

   LeaveCriticalSection (&m_cs);

   if (InterlockedDecrement (&m_cRef) == 0)
      Delete (this);
}


void PROGRESSDISPLAY::OnUpdate (void)
{
   EnterCriticalSection (&m_cs);

   int perComplete;
   if (m_iProgressFinish <= m_iProgressStart)
      perComplete = 100;
   else
      perComplete = 100 * (m_iProgress - m_iProgressStart) / (m_iProgressFinish - m_iProgressStart);

   SendDlgItemMessage (m_hWnd, IDC_PROGRESS, PBM_SETPOS, (WPARAM)perComplete, 0);

   LPTSTR pszProgressText = FormatString (m_szProgressText, TEXT("%lu"), perComplete);
   SetDlgItemText (m_hWnd, IDC_PROGRESSTEXT, pszProgressText);
   FreeString (pszProgressText);

   SetDlgItemText (m_hWnd, IDC_OPERATION, m_szOperation);

   LeaveCriticalSection (&m_cs);
}


DWORD WINAPI PROGRESSDISPLAY::ThreadProc (PVOID lp)
{
   LPPROGRESSDISPLAY ppd;
   if ((ppd = (LPPROGRESSDISPLAY)lp) != NULL) {
      DWORD dwStatus;

      try {
         dwStatus = (*(ppd->m_pfnUser))(ppd, ppd->m_lpUser);
      } catch(...) {
         dwStatus = ERROR_PROCESS_ABORTED;
      }

      ppd->Finish (dwStatus);
   }
   return 0;
}

