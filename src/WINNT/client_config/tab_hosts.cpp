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
#include <afs/cm_config.h>
}

#include "afs_config.h"
#include "tab_hosts.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void HostsTab_OnInitDialog (HWND hDlg);
BOOL HostsTab_OnApply (HWND hDlg);
void HostsTab_OnSelect (HWND hDlg);
void HostsTab_OnAdd (HWND hDlg);
void HostsTab_OnEdit (HWND hDlg);
void HostsTab_OnRemove (HWND hDlg);

void HostsTab_FillList (HWND hDlg);

BOOL CALLBACK CellEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void CellEdit_OnInitDialog (HWND hDlg);
void CellEdit_OnDestroy (HWND hDlg);
void CellEdit_OnApply (HWND hDlg);
void CellEdit_OnSelect (HWND hDlg);
void CellEdit_OnAdd (HWND hDlg);
void CellEdit_OnEdit (HWND hDlg);
void CellEdit_OnRemove (HWND hDlg);
void CellEdit_Enable (HWND hDlg);
void CellEdit_AddServerEntry (HWND hDlg, PCELLDBLINE pLine, int iOrder);
int CALLBACK CellEdit_SortFunction (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2);

BOOL CALLBACK ServerEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void ServerEdit_OnInitDialog (HWND hDlg);
BOOL ServerEdit_OnOK (HWND hDlg);

BOOL TextToAddr (SOCKADDR_IN *pAddr, LPTSTR pszServer);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK HostsTab_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         if (g.fIsCCenter)
            Main_OnInitDialog (GetParent(hDlg));
         HostsTab_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               if (!HostsTab_OnApply (hDlg))
                  SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
               break;

            case IDC_ADD:
               HostsTab_OnAdd (hDlg);
               break;

            case IDC_EDIT:
               HostsTab_OnEdit (hDlg);
               break;

            case IDC_REMOVE:
               HostsTab_OnRemove (hDlg);
               break;

            case IDHELP:
               HostsTab_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         if (g.fIsCCenter)
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_GENERAL_CCENTER);
         else
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_CELLS);
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               HostsTab_OnSelect (hDlg);
               break;

            case FLN_LDBLCLICK:
               if (IsWindowEnabled (GetDlgItem (hDlg, IDC_EDIT)))
                  HostsTab_OnEdit (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void HostsTab_OnInitDialog (HWND hDlg)
{
   if (!g.Configuration.CellServDB.pFirst)
      {
      CSDB_ReadFile (&g.Configuration.CellServDB, NULL);

      // Fill in our list with cell names.
      //
      HostsTab_FillList (hDlg);
      HostsTab_OnSelect (hDlg);

      // If this is the Control Center applet, shove the default cell
      // name onto the tab too.
      //
      if (g.fIsCCenter)
         {
         Config_GetCellName (g.Configuration.szCell);
         SetDlgItemText (hDlg, IDC_CELL, g.Configuration.szCell);
         }
      }
}


BOOL HostsTab_CommitChanges (BOOL fForce)
{
   HWND hDlg;
   if ((hDlg = PropSheet_FindTabWindow (g.psh, (DLGPROC)HostsTab_DlgProc)) == NULL)
      return TRUE;
   if (fForce)
      SetWindowLongPtr (hDlg, DWLP_MSGRESULT, FALSE); // Make sure we try to apply
   if (HostsTab_OnApply (hDlg))
      return TRUE;
   SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
   return FALSE;
}


BOOL HostsTab_OnApply (HWND hDlg)
{
   // Don't try to do anything if we've already failed the apply
   if (GetWindowLongPtr (hDlg, DWLP_MSGRESULT))
      return FALSE;

   if (!CSDB_WriteFile (&g.Configuration.CellServDB))
      return FALSE;

   // If this is the Control Center applet, we'll have to validate
   // the cell name too.
   //
   if (g.fIsCCenter)
      {
      TCHAR szNoCell[ cchRESOURCE ];
      GetString (szNoCell, IDS_CELL_UNKNOWN);

      TCHAR szCell[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_CELL, szCell, cchRESOURCE);

      if ((!szCell[0]) || (!lstrcmpi (szNoCell, szCell)))
         {
         Message (MB_ICONASTERISK | MB_OK, GetErrorTitle(), IDS_NOCELL_DESC_CC);
         return FALSE;
         }

      if (!CSDB_FindCell (&g.Configuration.CellServDB, szCell))
         {
#ifdef AFS_AFSDB_ENV
             int ttl;
             char cellname[128], i;

             /* we pray for all ascii cellnames */
             for ( i=0 ; szCell[i] && i < (sizeof(cellname)-1) ; i++ )
                 cellname[i] = szCell[i];
             cellname[i] = '\0';

             if (cm_SearchCellByDNS(cellname, NULL, &ttl, NULL, NULL))
#endif
             {
                 Message (MB_ICONASTERISK | MB_OK, GetErrorTitle(), IDS_BADCELL_DESC_CC);
                 return FALSE;
             }
         }

      if (!Config_SetCellName (szCell))
         return FALSE;
      lstrcpy (g.Configuration.szCell, szCell);
      }

   return TRUE;
}


void HostsTab_OnSelect (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   HLISTITEM hItem = FastList_FindFirstSelected (hList);
   HLISTITEM hNext = FastList_FindNextSelected (hList, hItem);

   EnableWindow (GetDlgItem (hDlg, IDC_EDIT), !!hItem && !hNext);
   EnableWindow (GetDlgItem (hDlg, IDC_REMOVE), !!hItem);
}


void HostsTab_OnAdd (HWND hDlg)
{
   TCHAR szTitle[ cchRESOURCE ];
   GetString (szTitle, IDS_CELLADD_TITLE);

   LPPROPSHEET psh = PropSheet_Create (szTitle, FALSE, GetParent(hDlg), 0);
   psh->sh.dwFlags |= PSH_NOAPPLYNOW;  // Remove the Apply button
   psh->sh.dwFlags |= PSH_HASHELP;     // Add a Help button instead
   PropSheet_AddTab (psh, szTitle, IDD_CELL_EDIT, (DLGPROC)CellEdit_DlgProc, 0, TRUE);
   PropSheet_ShowModal (psh);

   HostsTab_FillList (hDlg);
   HostsTab_OnSelect (hDlg);
}


void HostsTab_OnEdit (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   HLISTITEM hItem = FastList_FindFirstSelected (hList);
   if (hItem)
      {
      PCELLDBLINE pLine = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);
      CELLDBLINEINFO Info;
      CSDB_CrackLine (&Info, pLine->szLine);

      LPTSTR pszTitle = FormatString (IDS_CELLEDIT_TITLE, TEXT("%s"), ((Info.szComment[0]) ? Info.szComment : Info.szCell));

      LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE, GetParent(hDlg), (LPARAM)pLine);
      psh->sh.dwFlags |= PSH_NOAPPLYNOW;  // Remove the Apply button
      psh->sh.dwFlags |= PSH_HASHELP;     // Add a Help button instead
      PropSheet_AddTab (psh, ((Info.szComment[0]) ? Info.szComment : Info.szCell), IDD_CELL_EDIT, (DLGPROC)CellEdit_DlgProc, (LPARAM)pLine, TRUE);
      PropSheet_ShowModal (psh);

      FreeString (pszTitle);
      }

   HostsTab_FillList (hDlg);
   HostsTab_OnSelect (hDlg);
}


void HostsTab_OnRemove (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   HLISTITEM hItem = FastList_FindFirstSelected (hList);
   HLISTITEM hNext = FastList_FindNextSelected (hList, hItem);

   if (!hItem)
      {
      return;
      }
   else if (hNext)
      {
      if (Message (MB_ICONEXCLAMATION | MB_OKCANCEL, GetCautionTitle(), IDS_HOSTREM_MANY) != IDOK)
         return;
      }
   else // (!hNext)
      {
      PCELLDBLINE pLine = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);
      CELLDBLINEINFO Info;
      CSDB_CrackLine (&Info, pLine->szLine);

      if (Message (MB_ICONEXCLAMATION | MB_OKCANCEL, GetCautionTitle(), IDS_HOSTREM_ONE, TEXT("%s"), Info.szCell) != IDOK)
         return;
      }

   for ( ; hItem; hItem = FastList_FindNextSelected (hList, hItem))
      {
      PCELLDBLINE pLine = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);
      CSDB_RemoveCell (&g.Configuration.CellServDB, pLine);
      }

   HostsTab_FillList (hDlg);
   HostsTab_OnSelect (hDlg);
}


void HostsTab_FillList (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   FastList_Begin (hList);
   FastList_RemoveAll (hList);

   for (PCELLDBLINE pLine = g.Configuration.CellServDB.pFirst; pLine; pLine = pLine->pNext)
      {
      CELLDBLINEINFO Info;
      if (!CSDB_CrackLine (&Info, pLine->szLine))
         continue;
      if (!Info.szCell[0])
         continue;

      TCHAR szText[ MAX_PATH ];
      lstrcpy (szText, Info.szCell);

#if 0 // Add this if you like a more verbose Cell Hosts tab
      if (Info.szComment)
         wsprintf (&szText[ lstrlen(szText) ], TEXT(" (%s)"), Info.szComment);
#endif

      FASTLISTADDITEM ai;
      memset (&ai, 0x00, sizeof(ai));
      ai.iFirstImage = IMAGE_NOIMAGE;
      ai.iSecondImage = IMAGE_NOIMAGE;
      ai.pszText = szText;
      ai.lParam = (LPARAM)pLine;
      FastList_AddItem (hList, &ai);
      }

   FastList_End (hList);
}


BOOL CALLBACK CellEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         CellEdit_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         CellEdit_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               CellEdit_OnApply (hDlg);
               break;

            case IDC_CELL:
               CellEdit_Enable (hDlg);
               break;

            case IDC_COMMENT:
               CellEdit_Enable (hDlg);
               break;

            case IDC_ADD:
               CellEdit_OnAdd (hDlg);
               CellEdit_Enable (hDlg);
               break;

            case IDC_EDIT:
               CellEdit_OnEdit (hDlg);
               CellEdit_Enable (hDlg);
               break;

            case IDC_REMOVE:
               CellEdit_OnRemove (hDlg);
               CellEdit_Enable (hDlg);
               break;

            case IDHELP:
               CellEdit_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         if (PropSheet_FindTabParam (hDlg))
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_CELLPROP_EDIT);
         else
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_CELLPROP_ADD);
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               CellEdit_OnSelect (hDlg);
               break;

            case FLN_LDBLCLICK:
               if (IsWindowEnabled (GetDlgItem (hDlg, IDC_EDIT)))
                  CellEdit_OnEdit (hDlg);
               CellEdit_Enable (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void CellEdit_OnInitDialog (HWND hDlg)
{
   PCELLDBLINE pLine = (PCELLDBLINE)PropSheet_FindTabParam (hDlg);
   if (pLine)
      {
      CELLDBLINEINFO Info;
      CSDB_CrackLine (&Info, pLine->szLine);
      SetDlgItemText (hDlg, IDC_CELL, Info.szCell);
      SetDlgItemText (hDlg, IDC_COMMENT, Info.szComment);

      int iOrder = 0;
      for (pLine = pLine->pNext; pLine; pLine = pLine->pNext)
         {
         CELLDBLINEINFO Info;
         if (!CSDB_CrackLine (&Info, pLine->szLine))
            break;
         if (Info.szCell[0])
            break;

         CellEdit_AddServerEntry (hDlg, pLine, iOrder++);
         }
      }

   // Prepare the columns on the server list
   //
   HWND hList = GetDlgItem (hDlg, IDC_LIST);

   FASTLISTCOLUMN Column;
   Column.dwFlags = FLCF_JUSTIFY_LEFT;
   Column.cxWidth = 200;
   GetString (Column.szText, IDS_SVRCOL_COMMENT);
   FastList_SetColumn (hList, 0, &Column);

   Column.dwFlags = FLCF_JUSTIFY_LEFT;
   Column.cxWidth = 100;
   GetString (Column.szText, IDS_SVRCOL_SERVER);
   FastList_SetColumn (hList, 1, &Column);

   FastList_SetSortFunction (hList, CellEdit_SortFunction);

   // Remove the Context Help [?] thing from the title bar
   //
   DWORD dwStyle = GetWindowLong (GetParent (hDlg), GWL_STYLE);
   dwStyle &= ~DS_CONTEXTHELP;
   SetWindowLong (GetParent (hDlg), GWL_STYLE, dwStyle);

   dwStyle = GetWindowLong (GetParent (hDlg), GWL_EXSTYLE);
   dwStyle &= ~WS_EX_CONTEXTHELP;
   SetWindowLong (GetParent (hDlg), GWL_EXSTYLE, dwStyle);

   // A little cleanup and we're done!
   //
   CellEdit_Enable (hDlg);
   CellEdit_OnSelect (hDlg);
}


void CellEdit_OnDestroy (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   for (HLISTITEM hItem = FastList_FindFirst (hList); hItem; hItem = FastList_FindNext (hList, hItem))
      {
      PCELLDBLINE pInfo = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);
      Delete (pInfo);
      }
}


void CellEdit_OnApply (HWND hDlg)
{
   TCHAR szCell[ cchCELLDBLINE ];
   GetDlgItemText (hDlg, IDC_CELL, szCell, cchCELLDBLINE);

   TCHAR szComment[ cchCELLDBLINE ];
   GetDlgItemText (hDlg, IDC_COMMENT, szComment, cchCELLDBLINE);

   TCHAR szLinkedCell[ cchCELLDBLINE ] = TEXT("");

   // Find out if there's already an entry in CellServDB for this cell
   //
   PCELLDBLINE pCellLine;
   if ((pCellLine = CSDB_FindCell (&g.Configuration.CellServDB, szCell)) != NULL)
      {
      CELLDBLINEINFO Info;
      if (CSDB_CrackLine (&Info, pCellLine->szLine))
         lstrcpy (szLinkedCell, Info.szLinkedCell);
      }

   // Replace our cell's entry in CellServDB, or add one if necessary.
   //
   if ((pCellLine = CSDB_AddCell (&g.Configuration.CellServDB, szCell, szLinkedCell, szComment)) != NULL)
      {
      // Remove the old servers from this cell
      //
      CSDB_RemoveCellServers (&g.Configuration.CellServDB, pCellLine);

      // Add the servers from our list to the CellServDB
      //
      PCELLDBLINE pAppendTo = pCellLine;

      HWND hList = GetDlgItem (hDlg, IDC_LIST);
      for (HLISTITEM hItem = FastList_FindFirst (hList); hItem; hItem = FastList_FindNext (hList, hItem))
         {
         PCELLDBLINE pFromList = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);

         pAppendTo = CSDB_AddLine (&g.Configuration.CellServDB, pAppendTo, pFromList->szLine);
         }
      }
}


void CellEdit_OnSelect (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   HLISTITEM hItem = FastList_FindFirstSelected (hList);
   HLISTITEM hNext = FastList_FindNextSelected (hList, hItem);

   EnableWindow (GetDlgItem (hDlg, IDC_EDIT), !!hItem && !hNext);
   EnableWindow (GetDlgItem (hDlg, IDC_REMOVE), !!hItem);
}


void CellEdit_OnAdd (HWND hDlg)
{
   CELLDBLINE Line;
   memset (&Line, 0x00, sizeof(CELLDBLINE));

   int iOrder = 0;

   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   for (HLISTITEM hItem = FastList_FindFirst (hList); hItem; hItem = FastList_FindNext (hList, hItem))
      {
      PCELLDBLINE pInfo = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);
      iOrder = max (iOrder, 1+ (int)(pInfo->pNext));
      }

   if (ModalDialogParam (IDD_SERVER_EDIT, hDlg, (DLGPROC)ServerEdit_DlgProc, (LPARAM)&Line) == IDOK)
      {
      CellEdit_AddServerEntry (hDlg, &Line, iOrder);
      }
}


void CellEdit_OnEdit (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   HLISTITEM hItem = FastList_FindFirstSelected (hList);
   PCELLDBLINE pInfo = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);

   CELLDBLINE Line;
   memcpy (&Line, pInfo, sizeof(CELLDBLINE));

   if (ModalDialogParam (IDD_SERVER_EDIT, hDlg, (DLGPROC)ServerEdit_DlgProc, (LPARAM)&Line) == IDOK)
      {
      CELLDBLINEINFO Info;
      CSDB_CrackLine (&Info, Line.szLine);

      TCHAR szServer[ cchRESOURCE ];
      lstrcpy (szServer, inet_ntoa (*(struct in_addr *)&Info.ipServer));

      FastList_SetItemText (hList, hItem, 0, Info.szComment);
      FastList_SetItemText (hList, hItem, 1, szServer);

      lstrcpy (pInfo->szLine, Line.szLine);
      }
}


void CellEdit_OnRemove (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);
   FastList_Begin (hList);

   HLISTITEM hItem;
   while ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      PCELLDBLINE pInfo = (PCELLDBLINE)FastList_GetItemParam (hList, hItem);
      Delete (pInfo);
      FastList_RemoveItem (hList, hItem);
      }

   FastList_End (hList);
}


void CellEdit_Enable (HWND hDlg)
{
   BOOL fEnable = TRUE;

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_CELL, szText, cchRESOURCE);
   if (!szText[0])
      fEnable = FALSE;

   if (!FastList_FindFirst (GetDlgItem (hDlg, IDC_LIST)))
      fEnable = FALSE;

   EnableWindow (GetDlgItem (GetParent (hDlg), IDOK), fEnable);
}


void CellEdit_AddServerEntry (HWND hDlg, PCELLDBLINE pLine, int iOrder)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST);

   PCELLDBLINE pCopy = New (CELLDBLINE);
   memcpy (pCopy, pLine, sizeof(CELLDBLINE));
   pCopy->pPrev = NULL;
   pCopy->pNext = (PCELLDBLINE)iOrder;

   CELLDBLINEINFO Info;
   CSDB_CrackLine (&Info, pCopy->szLine);

   TCHAR szServer[ cchRESOURCE ];
   lstrcpy (szServer, inet_ntoa (*(struct in_addr *)&Info.ipServer));

   FASTLISTADDITEM ai;
   memset (&ai, 0x00, sizeof(ai));
   ai.iFirstImage = IMAGE_NOIMAGE;
   ai.iSecondImage = IMAGE_NOIMAGE;
   ai.pszText = Info.szComment;
   ai.lParam = (LPARAM)pCopy;
   HLISTITEM hItem = FastList_AddItem (hList, &ai);

   FastList_SetItemText (hList, hItem, 1, szServer);
}


int CALLBACK CellEdit_SortFunction (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2)
{
   if (!hItem1 || !hItem2)
      return 0;

   PCELLDBLINE pLine1 = (PCELLDBLINE)lpItem1;
   PCELLDBLINE pLine2 = (PCELLDBLINE)lpItem2;

   int iOrder1 = (int)(pLine1->pNext);
   int iOrder2 = (int)(pLine2->pNext);
   return iOrder1 - iOrder2;
}


BOOL CALLBACK ServerEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         ServerEdit_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               if (ServerEdit_OnOK (hDlg))
                  EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDHELP:
               ServerEdit_DlgProc (hDlg, WM_HELP, 0, 0);
               break;

            case IDC_ADDR_SPECIFIC:
            case IDC_ADDR_LOOKUP:
               EnableWindow (GetDlgItem (hDlg, IDC_SERVER), IsDlgButtonChecked (hDlg, IDC_ADDR_SPECIFIC));
               break;
            }
         break;

      case WM_HELP:
         PCELLDBLINE pLine;
         pLine = (PCELLDBLINE)GetWindowLongPtr (hDlg, DWLP_USER);

         CELLDBLINEINFO Info;
         if (!CSDB_CrackLine (&Info, pLine->szLine))
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_CELLPROP_SERVER_ADD);
         else
            WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_CELLPROP_SERVER_EDIT);
         break;
      }

   return FALSE;
}


void ServerEdit_OnInitDialog (HWND hDlg)
{
    PCELLDBLINE pLine = (PCELLDBLINE)GetWindowLongPtr (hDlg, DWLP_USER);

   TCHAR szTitle[ cchRESOURCE ];
   CELLDBLINEINFO Info;
   if (!CSDB_CrackLine (&Info, pLine->szLine))
      GetString (szTitle, IDS_ADDSERVER_TITLE);
   else
      GetString (szTitle, IDS_EDITSERVER_TITLE);
   SetWindowText (hDlg, szTitle);

   SOCKADDR_IN Addr;
   memset (&Addr, 0x00, sizeof(SOCKADDR_IN));
   Addr.sin_family = AF_INET;
   Addr.sin_addr.s_addr = Info.ipServer;
   SA_SetAddr (GetDlgItem (hDlg, IDC_SERVER), &Addr);

   CheckDlgButton (hDlg, IDC_ADDR_SPECIFIC, !!Info.ipServer);
   CheckDlgButton (hDlg, IDC_ADDR_LOOKUP, !Info.ipServer);
   EnableWindow (GetDlgItem (hDlg, IDC_SERVER), IsDlgButtonChecked (hDlg, IDC_ADDR_SPECIFIC));

   SetDlgItemText (hDlg, IDC_COMMENT, Info.szComment);
}


BOOL ServerEdit_OnOK (HWND hDlg)
{
   PCELLDBLINE pLine = (PCELLDBLINE)GetWindowLongPtr (hDlg, DWLP_USER);

   TCHAR szComment[ cchCELLDBLINE ];
   GetDlgItemText (hDlg, IDC_COMMENT, szComment, cchCELLDBLINE);

   SOCKADDR_IN Addr;
   if (IsDlgButtonChecked (hDlg, IDC_ADDR_SPECIFIC))
      {
      SA_GetAddr (GetDlgItem (hDlg, IDC_SERVER), &Addr);
      lstrcpy (szComment, inet_ntoa (*(struct in_addr *)&Addr.sin_addr.s_addr));
      }
   if (!TextToAddr (&Addr, szComment))
      {
      Message (MB_ICONHAND, GetErrorTitle(), IDS_BADLOOKUP_DESC, TEXT("%s"), szComment);
      return FALSE;
      }

   TCHAR szServer[ cchCELLDBLINE ];
   lstrcpy (szServer, inet_ntoa (*(struct in_addr *)&Addr.sin_addr.s_addr));

   CSDB_FormatLine (pLine->szLine, szServer, NULL, szComment, FALSE);
   return TRUE;
}


BOOL TextToAddr (SOCKADDR_IN *pAddr, LPTSTR pszServer)
{
   if (!pszServer || !*pszServer)
      return FALSE;

   try {
      memset (pAddr, 0x00, sizeof(SOCKADDR_IN));
      pAddr->sin_family = AF_INET;

      if ((*pszServer >= TEXT('0')) && (*pszServer <= TEXT('9')))
         {
         if ((pAddr->sin_addr.s_addr = inet_addr (pszServer)) == 0)
            return FALSE;

         HOSTENT *pEntry;
         if ((pEntry = gethostbyaddr ((char*)&pAddr->sin_addr.s_addr, sizeof(int), AF_INET)) == NULL)
            return FALSE;

         if (pEntry->h_name[0])
            lstrcpy (pszServer, pEntry->h_name);  // return the server's fqdn
         }
      else // Need to lookup the server by its name
         {
         HOSTENT *pEntry;
         if ((pEntry = gethostbyname (pszServer)) == NULL)
            return FALSE;

         memcpy (&pAddr->sin_addr, (struct in_addr *)pEntry->h_addr, sizeof(struct in_addr));
         if (!pAddr->sin_addr.s_addr)
            return FALSE;

         if (pEntry->h_name[0])
            lstrcpy (pszServer, pEntry->h_name);  // return the server's fqdn
         }

      return TRUE;
      }
   catch(...)
      {
      return FALSE;
      }
}

