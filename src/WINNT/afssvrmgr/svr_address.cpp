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

#include "svrmgr.h"
#include "svr_address.h"
#include <stdlib.h>

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void ChangeAddr_OnInitDialog (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp);
void ChangeAddr_Enable (HWND hDlg, BOOL fEnable);
void ChangeAddr_OnSelect (HWND hDlg);
void ChangeAddr_OnEndTask_Init (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp, LPTASKPACKET ptp);
void ChangeAddr_OnRemove (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp);
void ChangeAddr_OnChange (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp);

BOOL CALLBACK NewAddr_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void NewAddr_OnInitDialog (HWND hDlg, LPSOCKADDR_IN pAddr);
void NewAddr_OnOK (HWND hDlg, LPSOCKADDR_IN pAddr);


/*
 * SERVER ADDRESSES ___________________________________________________________
 *
 */

void Server_FillAddrList (HWND hDlg, LPSERVERSTATUS lpss, BOOL fCanAddUnspecified)
{
   HWND hList = GetDlgItem (hDlg, IDC_SVR_ADDRESSES);

   LB_StartChange (hList, TRUE);

   if (!lpss || !lpss->nAddresses)
      {
      if (fCanAddUnspecified)
         (void)LB_AddItem (hList, IDS_SVR_NO_ADDR, (LPARAM)-1);
      }
   else for (size_t iAddr = 0; iAddr < lpss->nAddresses; ++iAddr)
      {
      int AddrInt;
      AfsClass_AddressToInt (&AddrInt, &lpss->aAddresses[ iAddr ]);
      if (AddrInt == 0)
         continue;

      LPTSTR pszAddress = FormatString (TEXT("%1"), TEXT("%a"), &lpss->aAddresses[ iAddr ]);
      (void)LB_AddItem (hList, pszAddress, (LPARAM)iAddr);
      FreeString (pszAddress);
      }

   LB_EndChange (hList, 0);
}


void Server_ParseAddress (LPSOCKADDR_IN pAddr, LPTSTR pszText)
{
   int addrNetwork = inet_addr (pszText);

   memset (pAddr, 0x00, sizeof(SOCKADDR_IN));
   pAddr->sin_family = AF_INET;
   pAddr->sin_addr.s_addr = addrNetwork;
}


BOOL Server_Ping (LPSOCKADDR_IN pAddr, LPCTSTR pszServerName)
{
   BOOL rc = FALSE;

   try {
      struct hostent *phe;
      if ((phe = gethostbyname (pszServerName)) == NULL)
         memset (pAddr, 0x00, sizeof(SOCKADDR_IN));
      else
         {
         memset (pAddr, 0x00, sizeof(SOCKADDR_IN));
         pAddr->sin_family = 2;
         pAddr->sin_port = 0;
         pAddr->sin_addr.s_addr = *(ULONG *)phe->h_addr;
         rc = TRUE;
         }
      }
   catch (...)
      {
      memset (pAddr, 0x00, sizeof(SOCKADDR_IN));
      }

   return rc;
}


BOOL CALLBACK ChangeAddr_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_ADDRESS, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   LPSVR_CHANGEADDR_PARAMS lpp;
   if ((lpp = (LPSVR_CHANGEADDR_PARAMS)GetWindowLong (hDlg, DWL_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            ChangeAddr_OnInitDialog (hDlg, lpp);
            StartTask (taskSVR_PROP_INIT, hDlg, lpp->lpiServer);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSVR_PROP_INIT)
                  ChangeAddr_OnEndTask_Init (hDlg, lpp, ptp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDC_SVR_ADDRESSES:
                  if (HIWORD(wp) == LBN_SELCHANGE)
                     ChangeAddr_OnSelect (hDlg);
                  break;

               case IDC_ADDR_CHANGE:
                  ChangeAddr_OnChange (hDlg, lpp);
                  break;

               case IDC_ADDR_REMOVE:
                  ChangeAddr_OnRemove (hDlg, lpp);
                  break;

               case IDOK:
                  EndDialog (hDlg, IDOK);
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, IDCANCEL);
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void ChangeAddr_OnInitDialog (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp)
{
   TCHAR szName[ cchNAME ];
   lpp->lpiServer->GetServerName (szName);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_TITLE, szText, cchRESOURCE);

   LPTSTR pszTitle = FormatString (szText, TEXT("%s"), szName);
   SetDlgItemText (hDlg, IDC_TITLE, pszTitle);
   FreeString (pszTitle);

   HWND hList = GetDlgItem (hDlg, IDC_SVR_ADDRESSES);
   LB_StartChange (hList, TRUE);
   LB_AddItem (hList, IDS_QUERYING, 0);
   LB_EndChange (hList, 0);

   ChangeAddr_Enable (hDlg, FALSE);
}


void ChangeAddr_Enable (HWND hDlg, BOOL fEnable)
{
   EnableWindow (GetDlgItem (hDlg, IDC_SVR_ADDRESSES), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_ADDR_CHANGE), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_ADDR_REMOVE), fEnable);

   if (fEnable)
      ChangeAddr_OnSelect (hDlg);
}


void ChangeAddr_OnSelect (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_SVR_ADDRESSES);
   BOOL fSelected = (LB_GetSelected (hList) != -1) ? TRUE : FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_ADDR_CHANGE), fSelected);
   EnableWindow (GetDlgItem (hDlg, IDC_ADDR_REMOVE), fSelected);
}


void ChangeAddr_OnEndTask_Init (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp, LPTASKPACKET ptp)
{
   if (!ptp->rc)
      {
      TCHAR szName[ cchNAME ];
      lpp->lpiServer->GetServerName (szName);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_SERVER_STATUS, TEXT("%s"), szName);
      }
   else
      {
      memcpy (&lpp->ssOld, &TASKDATA(ptp)->ss, sizeof(SERVERSTATUS));
      memcpy (&lpp->ssNew, &TASKDATA(ptp)->ss, sizeof(SERVERSTATUS));
      ChangeAddr_Enable (hDlg, TRUE);
      }

   Server_FillAddrList (hDlg, &lpp->ssNew, FALSE);
}


void ChangeAddr_OnRemove (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_SVR_ADDRESSES);

   int iSel;
   if ((iSel = LB_GetSelected (hList)) != -1)
      {
      TCHAR szItem[ cchRESOURCE ] = TEXT("");
      SendMessage (hList, LB_GETTEXT, iSel, (LPARAM)szItem);

      SOCKADDR_IN AddrSel;
      Server_ParseAddress (&AddrSel, szItem);

      int AddrSelInt;
      AfsClass_AddressToInt (&AddrSelInt, &AddrSel);

      if (AddrSelInt != 0)
         {
         for (size_t iAddr = 0; iAddr < lpp->ssOld.nAddresses; ++iAddr)
            {
            int OldAddrInt;
            AfsClass_AddressToInt (&OldAddrInt, &lpp->ssOld.aAddresses[iAddr]);

            int NewAddrInt;
            AfsClass_AddressToInt (&NewAddrInt, &lpp->ssNew.aAddresses[iAddr]);

            if ((OldAddrInt == AddrSelInt) || (NewAddrInt == AddrSelInt))
               {
               AfsClass_IntToAddress (&lpp->ssNew.aAddresses[iAddr], 0);
               }
            }
         }

      Server_FillAddrList (hDlg, &lpp->ssNew, FALSE);
      ChangeAddr_OnSelect (hDlg);
      }
}


void ChangeAddr_OnChange (HWND hDlg, LPSVR_CHANGEADDR_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_SVR_ADDRESSES);

   int iSel;
   if ((iSel = LB_GetSelected (hList)) != -1)
      {
      TCHAR szItem[ cchRESOURCE ] = TEXT("");
      SendMessage (hList, LB_GETTEXT, iSel, (LPARAM)szItem);

      SOCKADDR_IN AddrSel;
      Server_ParseAddress (&AddrSel, szItem);

      int AddrSelInt;
      AfsClass_AddressToInt (&AddrSelInt, &AddrSel);

      if (AddrSelInt != 0)
         {
         SOCKADDR_IN AddrNew = AddrSel;
         if (ModalDialogParam (IDD_SVR_NEWADDR, hDlg, (DLGPROC)NewAddr_DlgProc, (LPARAM)&AddrNew) != IDOK)
            return;

         int AddrNewInt;
         AfsClass_AddressToInt (&AddrNewInt, &AddrNew);
         if (AddrNewInt && (AddrNewInt != AddrSelInt))
            {

            // First see if the new IP address is already in the server's
            // list of IP addresses--if so, just delete the old address.
            //
            size_t iAddr;
            for (iAddr = 0; iAddr < lpp->ssOld.nAddresses; ++iAddr)
               {
               int OldAddrInt;
               AfsClass_AddressToInt (&OldAddrInt, &lpp->ssOld.aAddresses[iAddr]);

               int NewAddrInt;
               AfsClass_AddressToInt (&NewAddrInt, &lpp->ssNew.aAddresses[iAddr]);

               if ((OldAddrInt == AddrNewInt) || (NewAddrInt == AddrNewInt))
                  {
                  AddrNewInt = 0;
                  break;
                  }
               }

            // Now update the SERVERSTATUS structure.
            //
            for (iAddr = 0; iAddr < lpp->ssOld.nAddresses; ++iAddr)
               {
               int OldAddrInt;
               AfsClass_AddressToInt (&OldAddrInt, &lpp->ssOld.aAddresses[iAddr]);

               int NewAddrInt;
               AfsClass_AddressToInt (&NewAddrInt, &lpp->ssNew.aAddresses[iAddr]);

               if ((OldAddrInt == AddrSelInt) || (NewAddrInt == AddrSelInt))
                  {
                  AfsClass_IntToAddress (&lpp->ssNew.aAddresses[iAddr], AddrNewInt);
                  break;
                  }
               }
            }
         }

      Server_FillAddrList (hDlg, &lpp->ssNew, FALSE);
      ChangeAddr_OnSelect (hDlg);
      }
}


BOOL CALLBACK NewAddr_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_NEWADDR, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   LPSOCKADDR_IN pAddr;
   if ((pAddr = (LPSOCKADDR_IN)GetWindowLong (hDlg, DWL_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            NewAddr_OnInitDialog (hDlg, pAddr);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  NewAddr_OnOK (hDlg, pAddr);
                  EndDialog (hDlg, IDOK);
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, IDCANCEL);
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void NewAddr_OnInitDialog (HWND hDlg, LPSOCKADDR_IN pAddr)
{
   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_TITLE, szText, cchRESOURCE);

   LPTSTR pszTitle = FormatString (szText, TEXT("%a"), pAddr);
   SetDlgItemText (hDlg, IDC_TITLE, pszTitle);
   FreeString (pszTitle);

   SA_SetAddr (GetDlgItem (hDlg, IDC_ADDRESS), pAddr);
}


void NewAddr_OnOK (HWND hDlg, LPSOCKADDR_IN pAddr)
{
   SA_GetAddr (GetDlgItem (hDlg, IDC_ADDRESS), pAddr);
}

