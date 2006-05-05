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

#include "svrmgr.h"
#include <shellapi.h>
#include "svc_viewlog.h"
#include "svc_general.h"


#define cchMAXLOG 20480  // show at most 20k worth of log file

#define cxMIN_VIEWLOG  250 // minimum size of View Log File dialog
#define cyMIN_VIEWLOG  200 // minimum size of View Log File dialog


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdShowLog[] = {
    { IDC_SVC_VIEWLOG_DESC,     raSizeX | raRepaint,		0,	0 },
    { IDC_SVC_VIEWLOG_FILENAME, raSizeX | raRepaint,		0,	0 },
    { IDC_VIEWLOG_TEXT,         raSizeX | raSizeY | raRepaint, 	MAKELONG(cxMIN_VIEWLOG,cyMIN_VIEWLOG),	0 },
    { IDC_SVC_VIEWLOG_CONTENTS, raSizeX,			0,	0 },
    { IDOK,                     raMoveX | raMoveY,		0,	0 },
    { IDC_VIEWLOG_SAVEAS,       raMoveX | raMoveY,		0,	0 },
    { idENDLIST, 0,						0,	0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Services_ShowLog_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Services_ShowLog_TakeNextStep (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp);
void Services_ShowLog_OnEndTask (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp, LPTASKPACKET ptp);
void Services_ShowLog_OnInitDialog (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp);
void Services_ShowLog_OnSaveAs (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp);
BOOL Services_ShowLog_Pick (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp);

BOOL CALLBACK Services_PickLog_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Services_PickLog_OnInitDialog (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Services_ShowServiceLog (LPIDENT lpi)
{
   LPSVC_VIEWLOG_PACKET lpp = New (SVC_VIEWLOG_PACKET);
   lpp->lpiServer = lpi->GetServer();
   lpp->lpiService = (lpi && lpi->fIsService()) ? lpi : NULL;
   lpp->szLocal[0] = TEXT('\0');
   lpp->szRemote[0] = TEXT('\0');
   lpp->nDownloadAttempts = 0;

   HWND hDlg = ModelessDialogParam (IDD_SVC_VIEWLOG, NULL, (DLGPROC)Services_ShowLog_DlgProc, (LPARAM)lpp);
   AfsAppLib_RegisterModelessDialog (hDlg);
}


void Services_ShowServerLog (LPIDENT lpiServer, LPTSTR pszRemote)
{
   LPSVC_VIEWLOG_PACKET lpp = New (SVC_VIEWLOG_PACKET);
   lpp->lpiService = NULL;
   lpp->lpiServer = lpiServer;
   lpp->szLocal[0] = TEXT('\0');
   lpp->nDownloadAttempts = 0;

   if (pszRemote)
      lstrcpy (lpp->szRemote, pszRemote);
   else
      lpp->szRemote[0] = TEXT('\0');

   HWND hDlg = ModelessDialogParam (IDD_SVC_VIEWLOG, NULL, (DLGPROC)Services_ShowLog_DlgProc, (LPARAM)lpp);
   AfsAppLib_RegisterModelessDialog (hDlg);
}


BOOL CALLBACK Services_ShowLog_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVC_VIEWLOG, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPSVC_VIEWLOG_PACKET lpp;
   if ((lpp = (LPSVC_VIEWLOG_PACKET)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            if (gr.rViewLog.right == 0)
               GetWindowRect (hDlg, &gr.rViewLog);
            ResizeWindow (hDlg, awdShowLog, rwaMoveToHere, &gr.rViewLog);

            Services_ShowLog_TakeNextStep (hDlg, lpp);
            break;

         case WM_DESTROY:
            if (lpp->szLocal[0] != TEXT('\0'))
               DeleteFile (lpp->szLocal);
            GetWindowRect (hDlg, &gr.rViewLog);
            SetWindowLongPtr (hDlg, DWLP_USER, 0);
            Delete (lpp);
            break;

         case WM_SIZE:
            // if (lp==0), we're minimizing--don't call ResizeWindow().
            //
            if (lp != 0)
               ResizeWindow (hDlg, awdShowLog, rwaFixupGuts);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) == NULL)
               Services_ShowLog_TakeNextStep (hDlg, lpp);
            else
               {
               Services_ShowLog_OnEndTask (hDlg, lpp, ptp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
              {
              case IDOK:
              case IDCANCEL:
                 DestroyWindow (hDlg);
                 break;

              case IDC_VIEWLOG_SAVEAS:
                 Services_ShowLog_OnSaveAs (hDlg, lpp);
                 break;
              }
            break;

         case WM_CTLCOLOREDIT:
            if ((HWND)lp == GetDlgItem (hDlg, IDC_VIEWLOG_TEXT))
               {
               SetBkColor ((HDC)wp, GetSysColor (COLOR_WINDOW));
               return CreateSolidBrush (GetSysColor (COLOR_WINDOW))?TRUE:FALSE;
               }
            break;
         }
      }

   return FALSE;
}


void Services_ShowLog_TakeNextStep (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp)
{
   if (lpp->szLocal[0] != TEXT('\0'))
      {
      Services_ShowLog_OnInitDialog (hDlg, lpp);
      ShowWindow (hDlg, SW_SHOW);
      }
   else if (lpp->szRemote[0] != TEXT('\0'))
      {
      LPSVC_VIEWLOG_PACKET lppNew = New (SVC_VIEWLOG_PACKET);
      *lppNew = *lpp;
      lpp->nDownloadAttempts++;
      StartTask (taskSVC_VIEWLOG, hDlg, lppNew);  // downloads log file
      }
   else if (lpp->lpiService)
      {
      StartTask (taskSVC_FINDLOG, hDlg, lpp->lpiService);  // guesses log name
      }
   else
      {
      if (!Services_ShowLog_Pick (hDlg, lpp))
         DestroyWindow (hDlg);
      else
         PostMessage (hDlg, WM_ENDTASK, 0, 0); // Services_ShowLog_TakeNextStep
      }
}


void Services_ShowLog_OnEndTask (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp, LPTASKPACKET ptp)
{
   if (ptp->idTask == taskSVC_FINDLOG)  // tried to guess log file name?
      {
      if (ptp->rc)
         {
         lstrcpy (lpp->szRemote, TASKDATA(ptp)->pszText1);
         PostMessage (hDlg, WM_ENDTASK, 0, 0); // Services_ShowLog_TakeNextStep
         }
      else
         {
         if (!Services_ShowLog_Pick (hDlg, lpp))
            DestroyWindow (hDlg);
         else
            PostMessage (hDlg, WM_ENDTASK, 0, 0); // Services_ShowLog_TakeNextStep
         }
      }
   else if (ptp->idTask == taskSVC_VIEWLOG)  // tried to download log file?
      {
      if (ptp->rc)
         {
         lstrcpy (lpp->szLocal, TASKDATA(ptp)->pszText1);
         PostMessage (hDlg, WM_ENDTASK, 0, 0); // Services_ShowLog_TakeNextStep
         }
      else
         {
         if (lpp->lpiService && lpp->nDownloadAttempts==1)
            {
            if (!Services_ShowLog_Pick (hDlg, lpp))
               DestroyWindow (hDlg);
            else
               PostMessage (hDlg, WM_ENDTASK, 0, 0); // Services_ShowLog_TakeNextStep
            }
         else
            {
            TCHAR szServer[ cchNAME ];
            lpp->lpiServer->GetServerName (szServer);
            ErrorDialog (ptp->status, IDS_ERROR_VIEW_LOGFILE, TEXT("%s%s"), szServer, lpp->szRemote);
            DestroyWindow (hDlg);
            }
         }
      }
}


void Services_ShowLog_OnInitDialog (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp)
{
   // If we're viewing the log file for a service, record the remote
   // filename so the user won't ever have to enter it again.
   //
   if (lpp->lpiService)
      {
      LPSERVICE_PREF lpcp;
      if ((lpcp = (LPSERVICE_PREF)lpp->lpiService->GetUserParam()) != NULL)
         {
         lstrcpy (lpcp->szLogFile, lpp->szRemote);
         Services_SavePreferences (lpp->lpiService);
         }
      }

   // Prepare the ShowLog dialog's static text controls
   //
   LPTSTR psz;
   if (lpp->lpiService)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szService[ cchNAME ];
      lpp->lpiService->GetServerName (szServer);
      lpp->lpiService->GetServiceName (szService);
      psz = FormatString (IDS_VIEWLOG_FROMSERVICE, TEXT("%s%s"), szServer, szService);
      }
   else
      {
      TCHAR szServer[ cchNAME ];
      lpp->lpiServer->GetServerName (szServer);
      psz = FormatString (IDS_VIEWLOG_FROMSERVER, TEXT("%s"), szServer);
      }
   SetDlgItemText (hDlg, IDC_SVC_VIEWLOG_DESC, psz);
   FreeString (psz);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_SVC_VIEWLOG_FILENAME, szText, cchRESOURCE);
   psz = FormatString (szText, TEXT("%s"), lpp->szRemote);
   SetDlgItemText (hDlg, IDC_SVC_VIEWLOG_FILENAME, psz);
   FreeString (psz);

   // Read the log file into memory, and chunk its text into the
   // edit control on the dialog.  If the file is over cchMAXLOG chars
   // long, only read cchMAXLOG bytes from the end (make sure we start
   // the read after a carriage return).
   //
   HANDLE hFile = CreateFile (lpp->szLocal, GENERIC_READ, 0, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
   if (hFile != INVALID_HANDLE_VALUE)
      {
      BOOL fTruncated = FALSE;
      DWORD dwSize = GetFileSize (hFile, NULL);
      if (dwSize > cchMAXLOG)
         {
         // find the first \r\n within the final cchMAXLOG chars of the file
         //
         SetFilePointer (hFile, 0-cchMAXLOG, NULL, FILE_END);

         TCHAR ch;
         DWORD cbRead;
         while (ReadFile (hFile, &ch, 1, &cbRead, NULL) && cbRead)
            {
            if (ch == TEXT('\n'))
               break;
            }

         dwSize -= SetFilePointer (hFile, 0, 0, FILE_CURRENT);
         fTruncated = TRUE;
         }

      LPTSTR pszLog;
      if ((pszLog = AllocateString (dwSize)) != NULL)
         {
         DWORD cbRead;
         (void)ReadFile (hFile, pszLog, dwSize, &cbRead, NULL);
         pszLog[ cbRead ] = TEXT('\0');

         size_t cch = cbRead;
         while ( cch &&
                 (pszLog[ cch-1 ] == TEXT('\r')) ||
                 (pszLog[ cch-1 ] == TEXT('\n')) )
            {
            pszLog[ --cch ] = TEXT('\0');
            }

         SetFocus (GetDlgItem (hDlg, IDC_VIEWLOG_TEXT));
         SetDlgItemText (hDlg, IDC_VIEWLOG_TEXT, pszLog);
         SendDlgItemMessage (hDlg, IDC_VIEWLOG_TEXT, EM_LINESCROLL, 0, 0xFFFF);
         SendDlgItemMessage (hDlg, IDC_VIEWLOG_TEXT, EM_SETSEL, (WPARAM)-1, 0);

         if (fTruncated)
            {
            size_t nLines = 1;
            for (size_t ich = 0; ich < dwSize; ++ich)
               {
               if (pszLog[ich] == TEXT('\n'))
                  ++nLines;
               }

            psz = FormatString (IDS_VIEWLOG_TRUNCATED, TEXT("%lu"), nLines);
            SetDlgItemText (hDlg, IDC_SVC_VIEWLOG_CONTENTS, psz);
            FreeString (psz);
            }

         FreeString (pszLog);
         }

      CloseHandle (hFile);
      }
}


void Services_ShowLog_OnSaveAs (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp)
{
   TCHAR szFilter[ cchRESOURCE ];
   GetString (szFilter, IDS_SAVELOG_FILTER);
   TCHAR chFilter = szFilter[ lstrlen(szFilter)-1 ];
   for (LPTSTR pszFilter = szFilter;
        (*pszFilter) && ((pszFilter = (LPTSTR)lstrchr (pszFilter, chFilter)) != NULL);
        ++pszFilter)
      {
      *pszFilter = TEXT('\0');
      }

   TCHAR szSaveAs[ MAX_PATH ] = TEXT("");

   OPENFILENAME sfn;
   memset (&sfn, 0x00, sizeof(sfn));
   sfn.lStructSize = sizeof(sfn);
   sfn.hwndOwner = hDlg;
   sfn.hInstance = THIS_HINST;
   sfn.lpstrFilter = szFilter;
   sfn.nFilterIndex = 1;
   sfn.lpstrFile = szSaveAs;
   sfn.nMaxFile = MAX_PATH;
   sfn.Flags = OFN_HIDEREADONLY | OFN_NOREADONLYRETURN |
               OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
   sfn.lpstrDefExt = TEXT("txt");

   if (GetSaveFileName (&sfn))
      {
      TCHAR szzSource[ MAX_PATH+1 ];
      lstrcpy (szzSource, lpp->szLocal);
      szzSource[ lstrlen(szzSource)+1 ] = TEXT('\0');

      TCHAR szzTarget[ MAX_PATH+1 ];
      lstrcpy (szzTarget, szSaveAs);
      szzTarget[ lstrlen(szzTarget)+1 ] = TEXT('\0');

      SHFILEOPSTRUCT op;
      memset (&op, 0x00, sizeof(op));
      op.hwnd = hDlg;
      op.wFunc = FO_COPY;
      op.pFrom = szzSource;
      op.pTo = szzTarget;
      op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
      SHFileOperation (&op);
      }
}


BOOL Services_ShowLog_Pick (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp)
{
   if (ModalDialogParam (IDD_SVC_LOGNAME, NULL, (DLGPROC)Services_PickLog_DlgProc, (LPARAM)lpp) != IDOK)
      {
      return FALSE;
      }

   if (lpp->szRemote[0] == TEXT('\0'))
      {
      return FALSE;
      }

   return TRUE;
}


BOOL CALLBACK Services_PickLog_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVC_LOGNAME, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPSVC_VIEWLOG_PACKET lpp;
   if ((lpp = (LPSVC_VIEWLOG_PACKET)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            Services_PickLog_OnInitDialog (hDlg, lpp);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  GetDlgItemText (hDlg, IDC_VIEWLOG_FILENAME, lpp->szRemote, MAX_PATH);
               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;
               }
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  EnableWindow (GetDlgItem (hDlg, IDC_VIEWLOG_SERVER), TRUE);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_NOTIFY: 
            switch (((LPNMHDR)lp)->code)
               { 
               case FLN_ITEMSELECT:
                  if (IsWindowEnabled (GetDlgItem (hDlg, IDC_VIEWLOG_SERVER)))
                     {
                     LPIDENT lpi;
                     if ((lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_VIEWLOG_SERVER))) != NULL)
                        lpp->lpiServer = lpi;
                     }
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void Services_PickLog_OnInitDialog (HWND hDlg, LPSVC_VIEWLOG_PACKET lpp)
{
   SetDlgItemText (hDlg, IDC_VIEWLOG_FILENAME, lpp->szRemote);
   EnableWindow (GetDlgItem (hDlg, IDC_VIEWLOG_SERVER), FALSE);

   if (lpp->lpiService)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szService[ cchNAME ];
      lpp->lpiService->GetServerName (szServer);
      lpp->lpiService->GetServiceName (szService);

      LPTSTR pszText = FormatString (IDS_VIEWLOG_DESC_NOFILE, TEXT("%s%s"), szServer, szService);
      SetDlgItemText (hDlg, IDC_VIEWLOG_DESC, pszText);
      FreeString (pszText);

      CB_StartChange (GetDlgItem (hDlg, IDC_VIEWLOG_SERVER), TRUE);
      CB_AddItem (GetDlgItem (hDlg, IDC_VIEWLOG_SERVER), szServer, (LPARAM)(lpp->lpiServer));
      CB_EndChange (GetDlgItem (hDlg, IDC_VIEWLOG_SERVER), (LPARAM)(lpp->lpiServer));
      }
   else
      {
      LPSVR_ENUM_TO_COMBOBOX_PACKET lpEnum = New (SVR_ENUM_TO_COMBOBOX_PACKET);
      lpEnum->hCombo = GetDlgItem (hDlg, IDC_VIEWLOG_SERVER);
      lpEnum->lpiSelect = lpp->lpiServer;
      StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpEnum);
      }

   PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (hDlg, IDC_VIEWLOG_FILENAME), TRUE);
}

