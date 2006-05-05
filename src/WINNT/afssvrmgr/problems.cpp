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
#include "problems.h"
#include "alert.h"
#include "svc_viewlog.h"
#include "set_prop.h"
#include "agg_prop.h"
#include "creds.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Problems_OnInitDialog (HWND hDlg, LPIDENT lpi);
void Problems_OnRefresh (HWND hDlg, LPIDENT lpi);
void Problems_OnRedraw (HWND hDlg, LPIDENT lpi);
void Problems_OnRemedy (HWND hDlg, LPIDENT lpi);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Problems_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_PROBLEMS, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPIDENT lpi = (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG:
         NotifyMe (WHEN_OBJECT_CHANGES, lpi, hDlg, 0);
         Problems_OnInitDialog (hDlg, lpi);
         Problems_OnRefresh (hDlg, lpi);
         break;

      case WM_NOTIFY_FROM_DISPATCH:
         Problems_OnRefresh (hDlg, lpi);
         Delete ((LPNOTIFYSTRUCT)lp);
         break;

      case WM_DESTROY:
         DontNotifyMeEver (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_PROBLEM_REMEDY:
               Problems_OnRemedy (hDlg, lpi);
               break;
            }
         break;

      case WM_CTLCOLORSTATIC:
         if ((HWND)lp == GetDlgItem (hDlg, IDC_PROBLEM_TEXT))
            {
            SetBkColor ((HDC)wp, GetSysColor (COLOR_WINDOW));
            return CreateSolidBrush (GetSysColor (COLOR_WINDOW))?TRUE:FALSE;
            }
         break;

      case WM_VSCROLL:
         int posOld;
         int posNew;
         posOld = GetScrollPos (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SB_CTL);
         posNew = posOld;

         switch (LOWORD(wp))
            {
            case SB_LINEUP:
               --posNew;
               break;
            case SB_LINEDOWN:
               ++posNew;
               break;
            case SB_PAGEUP:
               --posNew;
               break;
            case SB_PAGEDOWN:
               ++posNew;
               break;
            case SB_THUMBTRACK:
               posNew = HIWORD(wp);
               break;
            case SB_TOP:
               posNew = 0;
               break;
            case SB_BOTTOM:
               posNew = nAlertsMAX;
               break;
            }

         SCROLLINFO si;
         memset (&si, 0x00, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_RANGE;
         GetScrollInfo (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SB_CTL, &si);
         posNew = limit (0, posNew, si.nMax);

         if (posNew != posOld)
            {
            SetScrollPos (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SB_CTL, posNew, (LOWORD(wp) == SB_THUMBTRACK) ? FALSE : TRUE);
            Problems_OnRedraw (hDlg, lpi);
            }
         break;
      }

   return FALSE;
}


void Problems_OnInitDialog (HWND hDlg, LPIDENT lpi)
{
   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_PROBLEM_TITLE, szText, cchRESOURCE);

   LPTSTR pszTitle = NULL;

   if (lpi->fIsServer())
      {
      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);
      pszTitle = FormatString (szText, TEXT("%s"), szServer);
      }
   else if (lpi->fIsService())
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szService[ cchNAME ];
      lpi->GetServerName (szServer);
      lpi->GetServiceName (szService);
      pszTitle = FormatString (szText, TEXT("%s%s"), szServer, szService);
      }
   else if (lpi->fIsAggregate())
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      lpi->GetServerName (szServer);
      lpi->GetAggregateName (szAggregate);
      pszTitle = FormatString (szText, TEXT("%s%s"), szServer, szAggregate);
      }
   else if (lpi->fIsFileset())
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpi->GetServerName (szServer);
      lpi->GetAggregateName (szAggregate);
      lpi->GetFilesetName (szFileset);
      pszTitle = FormatString (szText, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      }

   if (pszTitle == NULL)
      SetDlgItemText (hDlg, IDC_PROBLEM_TITLE, TEXT(""));
   else
      {
      SetDlgItemText (hDlg, IDC_PROBLEM_TITLE, pszTitle);
      FreeString (pszTitle);
      }

   ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SW_HIDE);
   ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_HEADER), SW_HIDE);
   ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_REMEDY), SW_HIDE);
   SetDlgItemText (hDlg, IDC_PROBLEM_TEXT, TEXT(""));
}


void Problems_OnRefresh (HWND hDlg, LPIDENT lpi)
{
   size_t nAlerts = Alert_GetCount (lpi);

   LPTSTR pszText = FormatString (IDS_PROBLEM_BOX, TEXT("%lu"), nAlerts);
   SetDlgItemText (hDlg, IDC_PROBLEM_BOX, pszText);
   FreeString (pszText);

   switch (nAlerts)
      {
      case 0:
         ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SW_HIDE);
         ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_HEADER), SW_HIDE);
         ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_REMEDY), SW_HIDE);

         TCHAR szText[ cchRESOURCE ];
         if (lpi->fIsServer())
            GetString (szText, IDS_SERVER_NO_PROBLEMS);
         else if (lpi->fIsService())
            GetString (szText, IDS_SERVICE_NO_PROBLEMS);
         else if (lpi->fIsAggregate())
            GetString (szText, IDS_AGGREGATE_NO_PROBLEMS);
         else if (lpi->fIsFileset())
            GetString (szText, IDS_FILESET_NO_PROBLEMS);
	 else
	     wsprintf (szText, TEXT("UNEXPECTED CONDITION in problems.cpp"));

         SetDlgItemText (hDlg, IDC_PROBLEM_TEXT, szText);
         break;

      case 1:
         ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SW_HIDE);
         ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_HEADER), SW_HIDE);
         Problems_OnRedraw (hDlg, lpi);
         break;

      default: // two or more
         ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SW_SHOW);
         ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_HEADER), SW_SHOW);

         SCROLLINFO si;
         si.cbSize = sizeof(si);
         si.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
         si.nMin = 0;
         si.nMax = (int)(nAlerts-1);
         si.nPage = 1;
         si.nPos = 0;
         si.nTrackPos = 0;

         SetScrollInfo (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SB_CTL, &si, TRUE);
         Problems_OnRedraw (hDlg, lpi);
         break;
      }
}


void Problems_OnRedraw (HWND hDlg, LPIDENT lpi)
{
   int iAlert = 0;

   if (IsWindowVisible (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL)))
      {
      iAlert = GetScrollPos (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SB_CTL);
      }

   TCHAR szText[ cchRESOURCE ];
   wsprintf (szText, TEXT("%lu:"), 1+iAlert);
   SetWindowText (GetDlgItem (hDlg, IDC_PROBLEM_HEADER), szText);

   LPTSTR pszText1 = Alert_GetDescription (lpi, iAlert, TRUE);
   LPTSTR pszText2 = Alert_GetRemedy (lpi, iAlert);
   LPTSTR pszText3 = Alert_GetButton (lpi, iAlert);

   LPTSTR pszProblem = FormatString (TEXT("%1\n\n%2"), TEXT("%s%s"),
                                 (pszText1) ? pszText1 : TEXT(""),
                                 (pszText2) ? pszText2 : TEXT(""));
   SetWindowText (GetDlgItem (hDlg, IDC_PROBLEM_TEXT), pszProblem);
   FreeString (pszProblem);

   if (pszText3 == NULL)
      ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_REMEDY), SW_HIDE);
   else
      {
      SetWindowText (GetDlgItem (hDlg, IDC_PROBLEM_REMEDY), pszText3);
      ShowWindow (GetDlgItem (hDlg, IDC_PROBLEM_REMEDY), SW_SHOW);
      }

   FreeString (pszText3);
   FreeString (pszText2);
   FreeString (pszText1);
}


void Problems_OnRemedy (HWND hDlg, LPIDENT lpi)
{
   int iAlert = GetScrollPos (GetDlgItem (hDlg, IDC_PROBLEM_SCROLL), SB_CTL);

   LPIDENT lpiTarget = Alert_GetIdent (lpi, iAlert);
   ALERT alert = Alert_GetAlert (lpi, iAlert);

   switch (alert)
      {
      case alertTIMEOUT:
         if (lpiTarget && lpiTarget->fIsCell())
            {
            StartTask (taskREFRESH, NULL, lpiTarget);
            }
         else if (lpiTarget)
            {
            StartTask (taskREFRESH, NULL, lpiTarget->GetServer());
            }
         break;

      case alertFULL:
         if (lpiTarget && lpiTarget->fIsFileset())
            {
            size_t nAlerts = Alert_GetCount (lpiTarget);
            Filesets_ShowProperties (lpiTarget, nAlerts, TRUE);
            }
         else if (lpiTarget && lpiTarget->fIsAggregate())
            {
            size_t nAlerts = Alert_GetCount (lpiTarget);
            Aggregates_ShowProperties (lpiTarget, nAlerts, TRUE);
            }
         break;

      case alertNO_VLDBENT:
         // No button in this case
         break;

      case alertNO_SVRENT:
         // No button in this case
         break;

      case alertSTOPPED:
         Services_ShowServiceLog (lpiTarget);
         break;

      case alertBADCREDS:
         NewCredsDialog();
         break;

      case alertOVERALLOC:
         // No button in this case
         break;

      case alertSTATE_NO_VNODE:
      case alertSTATE_NO_SERVICE:
      case alertSTATE_OFFLINE:
         // No button in these cases
         break;
      }
}


/*
 * ParseFilesetName
 *
 */

void ParseFilesetName (LPIDENT lpiSearch, LPTSTR pszBase, LPTSTR pszEnding)
{
   TCHAR szFileset[ cchNAME ];
   lpiSearch->GetFilesetName (szFileset);

   LPTSTR pszDot;
   if ((pszDot = (LPTSTR)lstrrchr (szFileset, TEXT('.'))) != NULL)
      {
      if ( lstrcmpi (pszDot, cszENDING_CLONE) &&
           lstrcmpi (pszDot, cszENDING_REPLICA) )
         {
         pszDot = NULL;
         }
      }

   if (pszDot == NULL)
      {
      lstrcpy (pszBase, szFileset);
      *pszEnding = NULL;
      }
   else // (pszDot != NULL)
      {
      lstrcpy (pszBase, szFileset);
      lstrcpy (pszEnding, pszDot);
      pszBase[ pszEnding - szFileset ] = TEXT('\0');
      }
}

