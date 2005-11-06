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
#include "svr_security.h"
#include "propcache.h"
#include "display.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ISKEYINUSE(_pkey) ((_pkey)->keyInfo.timeLastModification.wYear != 0)

typedef struct
   {
   LONG cRef;
   LPIDENT lpiServer;
   LPADMINLIST lpAdmList;
   LPKEYLIST lpKeyList;
   } SVR_SECURITY_PARAMS, *LPSVR_SECURITY_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Security_Free (LPSVR_SECURITY_PARAMS lpp);

BOOL CALLBACK Server_Lists_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_Lists_OnInitDialog (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);
void Server_Lists_OnEndTask_ListOpen (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp);
void Server_Lists_OnApply (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);
void Server_Lists_OnSelect (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);
void Server_Lists_OnAddEntry (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);
void Server_Lists_OnDelEntry (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);

BOOL CALLBACK Server_Keys_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Server_Keys_OnInitDialog (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);
void Server_Keys_OnEndTask_ListOpen (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp);
void Server_Keys_OnEndTask_CreateKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp);
void Server_Keys_OnEndTask_DeleteKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp);
void Server_Keys_OnSelect (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);
void Server_Keys_OnAddKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);
void Server_Keys_OnDelKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp);

void FormatServerKey (LPTSTR psz, LPENCRYPTIONKEY pKey);
BOOL ScanServerKey (LPENCRYPTIONKEY pKey, LPTSTR psz);

BOOL CALLBACK CreateKey_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void CreateKey_OnInitDialog (HWND hDlg, LPKEY_CREATE_PARAMS lpp);
void CreateKey_OnSelect (HWND hDlg);
void CreateKey_OnType (HWND hDlg);
void CreateKey_OnRandom (HWND hDlg, LPKEY_CREATE_PARAMS lpp);
BOOL CreateKey_OnOK (HWND hDlg, LPKEY_CREATE_PARAMS lpp);
void CreateKey_OnEndTask_Random (HWND hDlg, LPTASKPACKET ptp);


/*
 * SERVER-KEY COLUMNS _________________________________________________________
 *
 */

void Server_Key_SetDefaultView (LPVIEWINFO lpvi)
{
   lpvi->lvsView = FLS_VIEW_LIST;
   lpvi->nColsAvail = nSERVERKEYCOLUMNS;

   for (size_t iCol = 0; iCol < nSERVERKEYCOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = SERVERKEYCOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = SERVERKEYCOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = svrkeyVERSION;

   lpvi->nColsShown = 3;
   lpvi->aColumns[0] = (int)svrkeyVERSION;
   lpvi->aColumns[1] = (int)svrkeyDATA;
   lpvi->aColumns[2] = (int)svrkeyCHECKSUM;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Security (LPIDENT lpiServer, BOOL fJumpToKeys)
{
   HWND hCurrent;
   if ((hCurrent = PropCache_Search (pcSVR_SECURITY, lpiServer)) != NULL)
      {
      SetFocus (hCurrent);

      if (fJumpToKeys)
         {
         HWND hTab;
         if ((hTab = GetDlgItem (hCurrent, IDC_PROPSHEET_TABCTRL)) != NULL)
            {
            int nTabs = TabCtrl_GetItemCount (hTab);
            TabCtrl_SetCurSel (hTab, nTabs-1);

            NMHDR nm;
            nm.hwndFrom = hTab;
            nm.idFrom = IDC_PROPSHEET_TABCTRL;
            nm.code = TCN_SELCHANGE;
            SendMessage (hCurrent, WM_NOTIFY, 0, (LPARAM)&nm);
            }
         }
      }
   else
      {
      LPSVR_SECURITY_PARAMS lpp = New (SVR_SECURITY_PARAMS);
      memset (lpp, 0x00, sizeof(SVR_SECURITY_PARAMS));
      lpp->lpiServer = lpiServer;

      TCHAR szServer[ cchNAME ];
      lpiServer->GetServerName (szServer);
      LPTSTR pszTitle = FormatString (IDS_SVR_SECURITY_TITLE, TEXT("%s"), szServer);
      LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE);
      PropSheet_AddTab (psh, IDS_SVR_LIST_TAB, IDD_SVR_LISTS, (DLGPROC)Server_Lists_DlgProc, (LONG_PTR)lpp, TRUE, !fJumpToKeys);
      PropSheet_AddTab (psh, IDS_SVR_KEY_TAB,  IDD_SVR_KEYS,  (DLGPROC)Server_Keys_DlgProc,  (LONG_PTR)lpp, TRUE,  fJumpToKeys);
      PropSheet_ShowModeless (psh);
      FreeString (pszTitle);
      }
}


void Server_Security_Free (LPSVR_SECURITY_PARAMS lpp)
{
   if (lpp->lpAdmList)
      AfsClass_AdminList_Free (lpp->lpAdmList);
   if (lpp->lpKeyList)
      AfsClass_KeyList_Free (lpp->lpKeyList);
   Delete (lpp);
}


BOOL CALLBACK Server_Lists_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_LISTS, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_SECURITY_PARAMS lpp;
   if ((msg == WM_INITDIALOG_SHEET) || (msg == WM_DESTROY_SHEET))
      lpp = (LPSVR_SECURITY_PARAMS)lp;
   else
      lpp = (LPSVR_SECURITY_PARAMS)PropSheet_FindTabParam (hDlg);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         InterlockedIncrement (&lpp->cRef);
         PropCache_Add (pcSVR_SECURITY, lpp->lpiServer, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         if ((InterlockedDecrement (&lpp->cRef)) == 0)
            Server_Security_Free (lpp);
         break;

      case WM_INITDIALOG:
         Server_Lists_OnInitDialog (hDlg, lpp);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_ADMLIST_OPEN)
               Server_Lists_OnEndTask_ListOpen (hDlg, lpp, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Server_Lists_OnApply (hDlg, lpp);
               break;

            case IDC_LIST_ADD:
               Server_Lists_OnAddEntry (hDlg, lpp);
               PropSheetChanged (hDlg);
               break;

            case IDC_LIST_REMOVE:
               Server_Lists_OnDelEntry (hDlg, lpp);
               PropSheetChanged (hDlg);
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_ITEMSELECT:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_LIST_LIST))
                  {
                  Server_Lists_OnSelect (hDlg, lpp);
                  }
               break;
            }
         break;
      }

   return FALSE;
}


void Server_Lists_OnInitDialog (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   TCHAR szOld[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_LIST_NAME, szOld, cchRESOURCE);

   TCHAR szServer[ cchNAME ];
   lpp->lpiServer->GetServerName (szServer);

   LPTSTR pszText = FormatString (szOld, TEXT("%s"), szServer);
   SetDlgItemText (hDlg, IDC_LIST_NAME, pszText);
   FreeString (pszText);

   HWND hList = GetDlgItem (hDlg, IDC_LIST_LIST);

   // We'll need an imagelist, if we want icons in the list.
   //
   HIMAGELIST hLarge;
   if ((hLarge = ImageList_Create (32, 32, ILC_COLOR4 | ILC_MASK, 1, 1)) != 0)
      AfsAppLib_AddToImageList (hLarge, IDI_USER, TRUE);

   HIMAGELIST hSmall;
   if ((hSmall = ImageList_Create (16, 16, ILC_COLOR4 | ILC_MASK, 1, 1)) != 0)
      AfsAppLib_AddToImageList (hSmall, IDI_USER, FALSE);

   FastList_SetImageLists (hList, hSmall, hLarge);

   // Start loading the admin list
   //
   StartTask (taskSVR_ADMLIST_OPEN, hDlg, lpp->lpiServer);

   EnableWindow (hList, FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_LIST_ADD), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_LIST_REMOVE), FALSE);
}


void Server_Lists_OnEndTask_ListOpen (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST_LIST);

   lpp->lpAdmList = TASKDATA(ptp)->lpAdmList;

   // Populate the list
   //
   FL_StartChange (hList, TRUE);

   if (lpp->lpAdmList)
      {
      for (size_t iEntry = 0; iEntry < lpp->lpAdmList->cEntries; ++iEntry)
         {
         LPADMINLISTENTRY pEntry = &lpp->lpAdmList->aEntries[ iEntry ];
         if (pEntry->szAdmin[0] == TEXT('\0'))
            continue;

         FL_AddItem (hList, 1, (LPARAM)iEntry, 0, pEntry->szAdmin);
         }
      }

   FL_EndChange (hList, 0);
   EnableWindow (hList, (lpp->lpAdmList != NULL));
   EnableWindow (GetDlgItem (hDlg, IDC_LIST_ADD), (lpp->lpAdmList != NULL));

   Server_Lists_OnSelect (hDlg, lpp);
}


void Server_Lists_OnSelect (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST_LIST);

   BOOL fEnableRemove = TRUE;

   if (!IsWindowEnabled (hList))
      fEnableRemove = FALSE;

   if (FastList_FindFirstSelected (hList) == NULL)
      fEnableRemove = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_LIST_REMOVE), fEnableRemove);
}


void Server_Lists_OnApply (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_LIST_LIST)))
      {
      // Increment the reference counter on this admin list before handing
      // it off to the Save task. When the Save task is done, it will attempt
      // to free the list--which will decrement the counter again, and
      // actually free the list if the counter hits zero.
      //
      InterlockedIncrement (&lpp->lpAdmList->cRef);
      StartTask (taskSVR_ADMLIST_SAVE, NULL, lpp->lpAdmList);
      }
}


void Server_Lists_OnAddEntry (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   TCHAR szLocalCell[ cchNAME ];
   g.lpiCell->GetCellName (szLocalCell);

   BROWSEDLG_PARAMS pp;
   memset (&pp, 0x00, sizeof(BROWSEDLG_PARAMS));
   pp.hParent = hDlg;
   pp.idsTitle = IDS_TITLE_BROWSE_USER;
   pp.idsPrompt = IDS_PROMPT_BROWSE_USER;
   pp.bt = btLOCAL_USER;
   lstrcpy (pp.szCell, szLocalCell);
   pp.lpcl = AfsAppLib_GetCellList (HKCU, REGSTR_SETTINGS_CELLS);
   pp.szNamed[0] = TEXT('\0');
   pp.hCreds = g.hCreds;

   if (!AfsAppLib_ShowBrowseDialog (&pp))
      {
      AfsAppLib_FreeCellList (pp.lpcl);
      return;
      }

   AfsAppLib_FreeCellList (pp.lpcl);

   size_t iEntry;
   for (iEntry = 0; iEntry < lpp->lpAdmList->cEntries; ++iEntry)
      {
      LPADMINLISTENTRY pEntry = &lpp->lpAdmList->aEntries[ iEntry ];
      if (pEntry->szAdmin[0] == TEXT('\0'))
         continue;

      if (!lstrcmpi (pEntry->szAdmin, pp.szNamed))
         break;
      }

   if (iEntry >= lpp->lpAdmList->cEntries)
      {
      iEntry = AfsClass_AdminList_AddEntry (lpp->lpAdmList, pp.szNamed);
      }

   HWND hList = GetDlgItem (hDlg, IDC_LIST_LIST);
   FL_StartChange (hList, FALSE);

   HLISTITEM hItem;
   if ((hItem = FastList_FindItem (hList, (LPARAM)iEntry)) == NULL)
      {
      hItem = FL_AddItem (hList, 1, (LPARAM)iEntry, 0, pp.szNamed);
      }

   FL_EndChange (hList, (LPARAM)hItem);
}


void Server_Lists_OnDelEntry (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_LIST_LIST);
   FL_StartChange (hList, FALSE);

   HLISTITEM hItem;
   while ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      size_t iEntry = (size_t)FL_GetData (hList, hItem);
      AfsClass_AdminList_DelEntry (lpp->lpAdmList, iEntry);
      FastList_RemoveItem (hList, hItem);
      }

   FL_EndChange (hList);
}


BOOL CALLBACK Server_Keys_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SVR_KEYS, hDlg, msg, wp, lp))
      return TRUE;

   LPSVR_SECURITY_PARAMS lpp;
   if ((msg == WM_INITDIALOG_SHEET) || (msg == WM_DESTROY_SHEET))
      lpp = (LPSVR_SECURITY_PARAMS)lp;
   else
      lpp = (LPSVR_SECURITY_PARAMS)PropSheet_FindTabParam (hDlg);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         InterlockedIncrement (&lpp->cRef);
         break;

      case WM_DESTROY_SHEET:
         if ((InterlockedDecrement (&lpp->cRef)) == 0)
            Server_Security_Free (lpp);
         break;

      case WM_INITDIALOG:
         Server_Keys_OnInitDialog (hDlg, lpp);
         break;

      case WM_DESTROY:
         FL_StoreView (GetDlgItem (hDlg, IDC_KEY_LIST), &gr.viewKey);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_KEYLIST_OPEN)
               Server_Keys_OnEndTask_ListOpen (hDlg, lpp, ptp);
            else if (ptp->idTask == taskSVR_KEY_CREATE)
               Server_Keys_OnEndTask_CreateKey (hDlg, lpp, ptp);
            else if (ptp->idTask == taskSVR_KEY_DELETE)
               Server_Keys_OnEndTask_DeleteKey (hDlg, lpp, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_KEY_ADD:
               Server_Keys_OnAddKey (hDlg, lpp);
               break;

            case IDC_KEY_REMOVE:
               Server_Keys_OnDelKey (hDlg, lpp);
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_ITEMSELECT:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_KEY_LIST))
                  {
                  Server_Keys_OnSelect (hDlg, lpp);
                  }
               break;
            }
         break;
      }

   return FALSE;
}


void Server_Keys_OnInitDialog (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   TCHAR szServer[ cchNAME ];
   lpp->lpiServer->GetServerName (szServer);

   LPTSTR pszText = FormatString (IDS_KEYNAME_NOTIME, TEXT("%s"), szServer);
   SetDlgItemText (hDlg, IDC_KEY_NAME, pszText);
   FreeString (pszText);

   HWND hList = GetDlgItem (hDlg, IDC_KEY_LIST);
   FL_RestoreView (hList, &gr.viewKey);
   FastList_SetSortFunction (hList, FastList_SortFunc_Numeric);
   FastList_SetSortStyle (hList, 0, FALSE);

   // We'll need an imagelist, if we want icons in the list.
   //
   HIMAGELIST hSmall = AfsAppLib_CreateImageList (FALSE);
   FastList_SetImageLists (hList, hSmall, NULL);

   // Start loading the server keys
   //
   StartTask (taskSVR_KEYLIST_OPEN, hDlg, lpp->lpiServer);

   EnableWindow (hList, FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_KEY_ADD), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_KEY_REMOVE), FALSE);
}


void Server_Keys_OnEndTask_CreateKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp)
{
   StartTask (taskSVR_KEYLIST_OPEN, hDlg, lpp->lpiServer);
}


void Server_Keys_OnEndTask_DeleteKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp)
{
   StartTask (taskSVR_KEYLIST_OPEN, hDlg, lpp->lpiServer);
}


void Server_Keys_OnEndTask_ListOpen (HWND hDlg, LPSVR_SECURITY_PARAMS lpp, LPTASKPACKET ptp)
{
   HWND hList = GetDlgItem (hDlg, IDC_KEY_LIST);

   if (lpp->lpKeyList)
      AfsClass_KeyList_Free (lpp->lpKeyList);
   lpp->lpKeyList = TASKDATA(ptp)->lpKeyList;

   // Populate the list
   //
   LPARAM iVerSelect = FL_StartChange (hList, TRUE);

   SYSTEMTIME stLastMod;
   memset (&stLastMod, 0x00, sizeof(stLastMod));

   if (lpp->lpKeyList)
      {
      for (size_t iKey = 0; iKey < lpp->lpKeyList->cKeys; ++iKey)
         {
         LPSERVERKEY pKey = &lpp->lpKeyList->aKeys[ iKey ];
         if (!ISKEYINUSE(pKey))
            continue;

         TCHAR szVersion[ cchRESOURCE ];
         wsprintf (szVersion, TEXT("%lu"), pKey->keyVersion);

         TCHAR szValue[ cchRESOURCE ];
         FormatServerKey (szValue, &pKey->keyData);

         TCHAR szChecksum[ cchRESOURCE ];
         wsprintf (szChecksum, TEXT("%lu"), pKey->keyInfo.dwChecksum);

         FL_AddItem (hList, &gr.viewKey, (LPARAM)pKey->keyVersion, imageSERVERKEY, szVersion, szValue, szChecksum);

         stLastMod = pKey->keyInfo.timeLastModification;
         }
      }

   FL_EndChange (hList, iVerSelect);
   EnableWindow (hList, (lpp->lpKeyList != NULL));
   EnableWindow (GetDlgItem (hDlg, IDC_KEY_ADD), (lpp->lpKeyList != NULL));

   // Retitle the list
   //
   if (lpp->lpKeyList && stLastMod.wYear)
      {
      TCHAR szServer[ cchNAME ];
      lpp->lpiServer->GetServerName (szServer);

      LPTSTR pszText = FormatString (IDS_KEYNAME_WITHTIME, TEXT("%s%t"), szServer, &stLastMod);
      SetDlgItemText (hDlg, IDC_KEY_NAME, pszText);
      FreeString (pszText);
      }

   Server_Lists_OnSelect (hDlg, lpp);
}


void Server_Keys_OnSelect (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_KEY_LIST);

   BOOL fEnableRemove = TRUE;

   if (!IsWindowEnabled (hList))
      fEnableRemove = FALSE;

   if (FastList_FindFirstSelected (hList) == NULL)
      fEnableRemove = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_KEY_REMOVE), fEnableRemove);
}


void Server_Keys_OnAddKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   LPKEY_CREATE_PARAMS lpCreate = New (KEY_CREATE_PARAMS);
   memset (lpCreate, 0x00, sizeof(KEY_CREATE_PARAMS));
   lpCreate->lpiServer = lpp->lpiServer;
   lpCreate->keyVersion = 0;

   for (size_t iKey = 0; iKey < lpp->lpKeyList->cKeys; ++iKey)
      {
      LPSERVERKEY pKey = &lpp->lpKeyList->aKeys[ iKey ];
      if (!ISKEYINUSE(pKey))
         continue;
      lpCreate->keyVersion = max (lpCreate->keyVersion, 1+pKey->keyVersion);
      }

   if (ModalDialogParam (IDD_SVR_CREATEKEY, hDlg, (DLGPROC)CreateKey_DlgProc, (LPARAM)lpCreate) != IDOK)
      {
      Delete (lpCreate);
      }
   else
      {
      StartTask (taskSVR_KEY_CREATE, hDlg, lpCreate);
      }
}


void Server_Keys_OnDelKey (HWND hDlg, LPSVR_SECURITY_PARAMS lpp)
{
   HWND hList = GetDlgItem (hDlg, IDC_KEY_LIST);

   HLISTITEM hItem;
   if ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      LPKEY_DELETE_PARAMS lpDel = New (KEY_DELETE_PARAMS);
      lpDel->lpiServer = lpp->lpiServer;
      lpDel->keyVersion = (int)FL_GetData (hList, hItem);
      StartTask (taskSVR_KEY_DELETE, hDlg, lpDel);
      }
}


void FormatServerKey (LPTSTR psz, LPENCRYPTIONKEY pKey)
{
   size_t ii;
   for (ii = 0; ii < ENCRYPTIONKEY_LEN; ++ii)
      {
      if (pKey->key[ii])
         break;
      }
   if (ii == ENCRYPTIONKEY_LEN)
      {
      GetString (psz, IDS_SVRKEY_DATA_UNKNOWN);
      return;
      }

   for (ii = 0; ii < ENCRYPTIONKEY_LEN; ++ii)
      {
	  WORD ch1 = (WORD)(pKey->key[ii]) / 64;
	  WORD ch2 = ((WORD)(pKey->key[ii]) - (ch1 * 64)) / 8;
	  WORD ch3 = ((WORD)(pKey->key[ii]) - (ch1 * 64) - (ch2 * 8));
	  wsprintf (psz, TEXT("\\%0d%0d%0d"), ch1, ch2, ch3);
	  psz += 4;
      }

   *psz = TEXT('\0');
}


BOOL ScanServerKey (LPENCRYPTIONKEY pKey, LPTSTR psz)
{
   size_t ich;
   for (ich = 0; psz && *psz; )
      {
      if (ich == ENCRYPTIONKEY_LEN)
         return FALSE;

      if (*psz != '\\')
         {
         pKey->key[ ich++ ] = (BYTE)*psz++;
         continue;
         }

      if ((lstrlen(psz) < 4) || (!isdigit(*(1+psz))))
         return FALSE;

      ++psz; // skip the backslash
      WORD ch1 = (WORD)((*psz++) - TEXT('0'));
      WORD ch2 = (WORD)((*psz++) - TEXT('0'));
      WORD ch3 = (WORD)((*psz++) - TEXT('0'));
      pKey->key[ ich++ ] = (BYTE)( ch1 * 64 + ch2 * 8 + ch3 );
      }

   return (ich == ENCRYPTIONKEY_LEN) ? TRUE : FALSE;
}


BOOL CALLBACK CreateKey_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static LPKEY_CREATE_PARAMS lpp = NULL;

   switch (msg)
      {
      case WM_INITDIALOG:
         lpp = (LPKEY_CREATE_PARAMS)lp;
         CreateKey_OnInitDialog (hDlg, lpp);
         break;

      case WM_DESTROY:
         lpp = NULL;
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_GETRANDOMKEY)
               CreateKey_OnEndTask_Random (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               if (CreateKey_OnOK (hDlg, lpp))
                  EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_KEY_BYSTRING:
            case IDC_KEY_BYDATA:
               CreateKey_OnSelect (hDlg);
               break;

            case IDC_KEY_STRING:
            case IDC_KEY_DATA:
               CreateKey_OnType (hDlg);
               break;

            case IDC_KEY_RANDOM:
               CreateKey_OnRandom (hDlg, lpp);
               break;
            }
         break;
      }

   return FALSE;
}


void CreateKey_OnInitDialog (HWND hDlg, LPKEY_CREATE_PARAMS lpp)
{
   TCHAR szOld[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_KEY_TITLE, szOld, cchRESOURCE);

   TCHAR szServer[ cchNAME ];
   lpp->lpiServer->GetServerName (szServer);

   LPTSTR pszText = FormatString (szOld, TEXT("%s"), szServer);
   SetDlgItemText (hDlg, IDC_KEY_TITLE, pszText);
   FreeString (pszText);

   CheckDlgButton (hDlg, IDC_KEY_BYSTRING, TRUE);
   SetDlgItemText (hDlg, IDC_KEY_STRING, lpp->szString);

   CreateSpinner (GetDlgItem (hDlg, IDC_KEY_VERSION), 10, FALSE, 0, lpp->keyVersion, 255);
   CreateKey_OnSelect (hDlg);
}


void CreateKey_OnSelect (HWND hDlg)
{
   EnableWindow (GetDlgItem (hDlg, IDC_KEY_STRING), IsDlgButtonChecked (hDlg, IDC_KEY_BYSTRING));
   EnableWindow (GetDlgItem (hDlg, IDC_KEY_DATA),   IsDlgButtonChecked (hDlg, IDC_KEY_BYDATA));
   EnableWindow (GetDlgItem (hDlg, IDC_KEY_RANDOM), IsDlgButtonChecked (hDlg, IDC_KEY_BYDATA));
   CreateKey_OnType (hDlg);
}


void CreateKey_OnType (HWND hDlg)
{
   BOOL fEnable = FALSE;

   if (IsDlgButtonChecked (hDlg, IDC_KEY_BYSTRING))
      {
      TCHAR szKey[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_KEY_STRING, szKey, cchRESOURCE);
      if (szKey[0] != TEXT('\0'))
         fEnable = TRUE;
      }
   else // (IsDlgButtonChecked (hDlg, IDC_KEY_BYDATA))
      {
      TCHAR szKey[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_KEY_DATA, szKey, cchRESOURCE);

      ENCRYPTIONKEY key;
      if (ScanServerKey (&key, szKey))
         fEnable = TRUE;
      }

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void CreateKey_OnRandom (HWND hDlg, LPKEY_CREATE_PARAMS lpp)
{
   StartTask (taskSVR_GETRANDOMKEY, hDlg, lpp->lpiServer);
}


BOOL CreateKey_OnOK (HWND hDlg, LPKEY_CREATE_PARAMS lpp)
{
   if (IsDlgButtonChecked (hDlg, IDC_KEY_BYSTRING))
      {
      GetDlgItemText (hDlg, IDC_KEY_STRING, lpp->szString, cchRESOURCE);
      }
   else // (!IsDlgButtonChecked (hDlg, IDC_KEY_BYSTRING))
      {
      TCHAR szKey[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_KEY_DATA, szKey, cchRESOURCE);

      if (!ScanServerKey (&lpp->key, szKey))
         return FALSE;
      }

   lpp->keyVersion = (int) SP_GetPos (GetDlgItem (hDlg, IDC_KEY_VERSION));
   return TRUE;
}


void CreateKey_OnEndTask_Random (HWND hDlg, LPTASKPACKET ptp)
{
   if (!ptp->rc)
      {
      EnableWindow (GetDlgItem (hDlg, IDC_KEY_RANDOM), FALSE);
      }
   else
      {
      TCHAR szKey[ cchRESOURCE ];
      FormatServerKey (szKey, &TASKDATA(ptp)->key);
      SetDlgItemText (hDlg, IDC_KEY_DATA, szKey);
      }
}

