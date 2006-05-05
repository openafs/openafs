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
#include "svr_hosts.h"
#include "propcache.h"
#include "display.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   LPHOSTLIST lpList;
   } SVR_HOSTS_PARAMS, *LPSVR_HOSTS_PARAMS;

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szHost[ cchNAME ];
   } SVR_ADDHOST_PARAMS, *LPSVR_ADDHOST_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Hosts_Free (LPSVR_HOSTS_PARAMS lpp);

BOOL CALLBACK Server_Hosts_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_Hosts_OnInitDialog (HWND hDlg, LPSVR_HOSTS_PARAMS lpp);
void Server_Hosts_OnEndTask_ListOpen (HWND hDlg, LPSVR_HOSTS_PARAMS lpp, LPTASKPACKET ptp);
void Server_Hosts_OnApply (HWND hDlg, LPSVR_HOSTS_PARAMS lpp);
void Server_Hosts_OnSelect (HWND hDlg, LPSVR_HOSTS_PARAMS lpp);
void Server_Hosts_OnAddEntry (HWND hDlg, LPSVR_HOSTS_PARAMS lpp);
void Server_Hosts_OnDelEntry (HWND hDlg, LPSVR_HOSTS_PARAMS lpp);

BOOL CALLBACK Server_AddHost_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Server_AddHost_OnInitDialog (HWND hDlg, LPSVR_ADDHOST_PARAMS lpp);
void Server_AddHost_EnableOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Hosts (LPIDENT lpiServer)
{
   HWND hCurrent;
   if ((hCurrent = PropCache_Search (pcSVR_HOSTS, lpiServer)) != NULL)
      {
      SetFocus (hCurrent);
      }
   else
      {
      LPSVR_HOSTS_PARAMS lpp = New (SVR_HOSTS_PARAMS);
      memset (lpp, 0x00, sizeof(SVR_HOSTS_PARAMS));
      lpp->lpiServer = lpiServer;

      TCHAR szServer[ cchNAME ];
      lpiServer->GetServerName (szServer);
      LPTSTR pszTitle = FormatString (IDS_SVR_HOSTS_TITLE, TEXT("%s"), szServer);
      LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE);
      PropSheet_AddTab (psh, IDS_SVR_HOST_TAB, IDD_SVR_HOSTS, (DLGPROC)Server_Hosts_DlgProc, (LONG_PTR)lpp, TRUE);
      PropSheet_ShowModeless (psh);
      FreeString (pszTitle);
      }
}


void Server_Hosts_Free (LPSVR_HOSTS_PARAMS lpp)
{
   if (lpp->lpList)
      AfsClass_HostList_Free (lpp->lpList);
   Delete (lpp);
}


BOOL CALLBACK Server_Hosts_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_HOSTS, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_HOSTS_PARAMS lpp;
   if ((msg == WM_INITDIALOG_SHEET) || (msg == WM_DESTROY_SHEET))
      lpp = (LPSVR_HOSTS_PARAMS)lp;
   else
      lpp = (LPSVR_HOSTS_PARAMS)PropSheet_FindTabParam (hDlg);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcSVR_HOSTS, lpp->lpiServer, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         Server_Hosts_OnInitDialog (hDlg, lpp);
         break;

      case WM_DESTROY:
         Server_Hosts_Free (lpp);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_HOSTLIST_OPEN)
               Server_Hosts_OnEndTask_ListOpen (hDlg, lpp, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Server_Hosts_OnApply (hDlg, lpp);
               break;

            case IDC_HOST_ADD:
               Server_Hosts_OnAddEntry (hDlg, lpp);
               PropSheetChanged (hDlg);
               break;

            case IDC_HOST_REMOVE:
               Server_Hosts_OnDelEntry (hDlg, lpp);
               PropSheetChanged (hDlg);
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_ITEMSELECT:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_HOST_LIST))
                  {
                  Server_Hosts_OnSelect (hDlg, lpp);
                  }
               break;
            }
         break;
      }

   return FALSE;
}


void Server_Hosts_OnInitDialog (HWND hDlg, LPSVR_HOSTS_PARAMS lpp)
{
   TCHAR szServer[ cchNAME ];
   lpp->lpiServer->GetServerName (szServer);

   LPTSTR pszText = FormatString (IDS_HOST_TITLE, TEXT("%s"), szServer);
   SetDlgItemText (hDlg, IDC_HOST_TITLE, pszText);
   FreeString (pszText);

   HWND hList = GetDlgItem (hDlg, IDC_HOST_LIST);

   // We'll need an imagelist, if we want icons in the list.
   //
   HIMAGELIST hLarge;
   if ((hLarge = ImageList_Create (32, 32, ILC_COLOR4 | ILC_MASK, 1, 1)) != 0)
      AfsAppLib_AddToImageList (hLarge, IDI_SERVER, TRUE);

   HIMAGELIST hSmall;
   if ((hSmall = ImageList_Create (16, 16, ILC_COLOR4 | ILC_MASK, 1, 1)) != 0)
      AfsAppLib_AddToImageList (hSmall, IDI_SERVER, FALSE);

   FastList_SetImageLists (hList, hSmall, hLarge);

   // Start loading the host list
   //
   StartTask (taskSVR_HOSTLIST_OPEN, hDlg, lpp->lpiServer);

   EnableWindow (hList, FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_HOST_ADD), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_HOST_REMOVE), FALSE);
}


void Server_Hosts_OnEndTask_ListOpen (HWND hDlg, LPSVR_HOSTS_PARAMS lpp, LPTASKPACKET ptp)
{
   HWND hList = GetDlgItem (hDlg, IDC_HOST_LIST);

   lpp->lpList = TASKDATA(ptp)->lpHostList;

   // Populate the list
   //
   FL_StartChange (hList, TRUE);

   if (lpp->lpList)
      {
      for (size_t iEntry = 0; iEntry < lpp->lpList->cEntries; ++iEntry)
         {
         LPHOSTLISTENTRY pEntry = &lpp->lpList->aEntries[ iEntry ];
         if (pEntry->szHost[0] == TEXT('\0'))
            continue;

         FL_AddItem (hList, 1, (LPARAM)iEntry, 0, pEntry->szHost);
         }
      }

   FL_EndChange (hList, 0);
   EnableWindow (hList, (lpp->lpList != NULL));
   EnableWindow (GetDlgItem (hDlg, IDC_HOST_ADD), (lpp->lpList != NULL));

   Server_Hosts_OnSelect (hDlg, lpp);
}


void Server_Hosts_OnSelect (HWND hDlg, LPSVR_HOSTS_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_HOST_LIST);

   BOOL fEnableRemove = TRUE;

   if (!IsWindowEnabled (hList))
      fEnableRemove = FALSE;

   if (FastList_FindFirstSelected (hList) == NULL)
      fEnableRemove = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_HOST_REMOVE), fEnableRemove);
}


void Server_Hosts_OnApply (HWND hDlg, LPSVR_HOSTS_PARAMS lpp)
{
   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_HOST_LIST)))
      {
      // Increment the reference counter on this host list before handing
      // it off to the Save task. When the Save task is done, it will attempt
      // to free the list--which will decrement the counter again, and
      // actually free the list if the counter hits zero.
      //
      InterlockedIncrement (&lpp->lpList->cRef);
      StartTask (taskSVR_HOSTLIST_SAVE, NULL, lpp->lpList);
      }
}


void Server_Hosts_OnAddEntry (HWND hDlg, LPSVR_HOSTS_PARAMS lpp)
{
   LPSVR_ADDHOST_PARAMS pAdd = New (SVR_ADDHOST_PARAMS);
   memset (pAdd, 0x00, sizeof(pAdd));
   pAdd->lpiServer = lpp->lpiServer;

   if (ModalDialogParam (IDD_SVR_ADDHOST, hDlg, (DLGPROC)Server_AddHost_DlgProc, (LPARAM)pAdd) == IDOK)
      {
      size_t iEntry;
      for (iEntry = 0; iEntry < lpp->lpList->cEntries; ++iEntry)
         {
         LPHOSTLISTENTRY pEntry = &lpp->lpList->aEntries[ iEntry ];
         if (pEntry->szHost[0] == TEXT('\0'))
            continue;
         if (!lstrcmpi (pEntry->szHost, pAdd->szHost))
            break;
         }

      if (iEntry >= lpp->lpList->cEntries)
         {
         iEntry = AfsClass_HostList_AddEntry (lpp->lpList, pAdd->szHost);
         }

      HWND hList = GetDlgItem (hDlg, IDC_HOST_LIST);
      FL_StartChange (hList, FALSE);

      HLISTITEM hItem;
      if ((hItem = FastList_FindItem (hList, (LPARAM)iEntry)) == NULL)
         {
         hItem = FL_AddItem (hList, 1, (LPARAM)iEntry, 0, pAdd->szHost);
         }

      FL_EndChange (hList, (LPARAM)hItem);
      }

   Delete (pAdd);
}


void Server_Hosts_OnDelEntry (HWND hDlg, LPSVR_HOSTS_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_HOST_LIST);
   FL_StartChange (hList, FALSE);

   HLISTITEM hItem;
   while ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      size_t iEntry = (size_t)FL_GetData (hList, hItem);
      AfsClass_HostList_DelEntry (lpp->lpList, iEntry);
      FastList_RemoveItem (hList, hItem);
      }

   FL_EndChange (hList);
}


BOOL CALLBACK Server_AddHost_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_ADDHOST, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPSVR_ADDHOST_PARAMS lpp;
   if ((lpp = (LPSVR_ADDHOST_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            Server_AddHost_OnInitDialog (hDlg, lpp);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  GetDlgItemText (hDlg, IDC_ADDHOST_HOST, lpp->szHost, cchNAME);
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_ADDHOST_HOST:
                  Server_AddHost_EnableOK (hDlg);
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void Server_AddHost_OnInitDialog (HWND hDlg, LPSVR_ADDHOST_PARAMS lpp)
{
   TCHAR szDesc[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_ADDHOST_DESC, szDesc, cchRESOURCE);

   TCHAR szServer[ cchNAME ];
   lpp->lpiServer->GetServerName (szServer);

   LPTSTR pszDesc = FormatString (szDesc, TEXT("%s"), szServer);
   SetDlgItemText (hDlg, IDC_ADDHOST_DESC, pszDesc);
   FreeString (pszDesc);

   Server_AddHost_EnableOK (hDlg);
}


void Server_AddHost_EnableOK (HWND hDlg)
{
   TCHAR szHost[ cchNAME ];
   GetDlgItemText (hDlg, IDC_ADDHOST_HOST, szHost, cchNAME);

   EnableWindow (GetDlgItem (hDlg, IDOK), (szHost[0] != TEXT('\0')) ? TRUE : FALSE);
}

