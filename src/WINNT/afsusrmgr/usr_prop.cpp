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

#include "TaAfsUsrMgr.h"
#include "usr_prop.h"
#include "usr_cpw.h"
#include "usr_col.h"
#include "winlist.h"
#include "browse.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cdayPWEXPIRATION_MAX   254

#define cFAILLOCKCOUNT_MAX     254

#define csecFAILLOCK_MAX   (csec1HOUR * 36L)

#define GWD_ASIDLIST_MEMBER   TEXT("AsidList - Members")
#define GWD_ASIDLIST_OWNER    TEXT("AsidList - Owned")


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK UserProp_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void UserProp_General_OnInitDialog (HWND hDlg);
void UserProp_General_OnDestroy (HWND hDlg);
void UserProp_General_OnUnlock (HWND hDlg);
void UserProp_General_OnChangePwNow (HWND hDlg);
void UserProp_General_OnChangePw (HWND hDlg);
void UserProp_General_OnPwExpires (HWND hDlg);
void UserProp_General_OnFailLock (HWND hDlg);
void UserProp_General_OnFailLockTime (HWND hDlg);
void UserProp_General_OnApply (HWND hDlg);
void UserProp_General_UpdateDialog (HWND hDlg);
void UserProp_General_OnExpiration (HWND hDlg);

BOOL CALLBACK UserProp_Advanced_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void UserProp_Advanced_OnInitDialog (HWND hDlg);
void UserProp_Advanced_OnDestroy (HWND hDlg);
void UserProp_Advanced_OnGrantTickets (HWND hDlg);
void UserProp_Advanced_OnHasGroupQuota (HWND hDlg);
void UserProp_Advanced_OnAdmin (HWND hDlg);
void UserProp_Advanced_OnApply (HWND hDlg);
void UserProp_Advanced_UpdateDialog (HWND hDlg);
void UserProp_Advanced_UpdateDialog_Perm (HWND hDlg, int idc, ACCOUNTACCESS aa, BOOL fMixed);

BOOL CALLBACK UserProp_Member_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void UserProp_Member_OnInitDialog (HWND hDlg);
void UserProp_Member_OnDestroy (HWND hDlg);
void UserProp_Member_OnShowType (HWND hDlg);
void UserProp_Member_OnSelect (HWND hDlg);
void UserProp_Member_OnApply (HWND hDlg);
void UserProp_Member_OnAdd (HWND hDlg);
void UserProp_Member_OnRemove (HWND hDlg);
void UserProp_Member_OnEndTask_GroupSearch (HWND hDlg, LPTASKPACKET ptp);
void UserProp_Member_PopulateList (HWND hDlg);

void UserProp_UpdateName (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void User_FreeProperties (LPUSERPROPINFO lpp)
{
   if (lpp)
      {
      if ((lpp->fApplyAdvanced || lpp->fApplyGeneral) && lpp->pUserList)
         {

         // See if we have anything to scare the user about. If the
         // "afs", "authserver" or "krbtgt" accounts have any modified
         // properties, there may be reason for concern.
         //
         BOOL fMakeChanges = TRUE;

         if (lpp->fApplyGeneral || lpp->fApplyAdvanced)
            {
            for (size_t ii = 0; ii < lpp->pUserList->cEntries; ++ii)
               {
               TCHAR szUser[ cchRESOURCE ];
               asc_ObjectNameGet_Fast (g.idClient, g.idCell, lpp->pUserList->aEntries[ ii ].idObject, szUser);

               if ( (!lstrcmpi (szUser, TEXT("afs"))) ||
                    (!lstrcmpi (szUser, TEXT("AuthServer"))) ||
                    (!lstrcmpi (szUser, TEXT("krbtgt"))) )
                  {
                  if (Message (MB_ICONASTERISK | MB_YESNO | MB_DEFBUTTON2, IDS_WARNING_TITLE, IDS_WARNING_SYSTEM_ACCOUNT, TEXT("%s"), szUser) != IDYES)
                     {
                     fMakeChanges = FALSE;
                     break;
                     }
                  }
               }
            }

         if (fMakeChanges)
            {
            for (size_t ii = 0; ii < lpp->pUserList->cEntries; ++ii)
               {
               ULONG status;
               ASOBJPROP Properties;
               if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pUserList->aEntries[ ii ].idObject, &Properties, &status))
                  continue;

               LPUSER_CHANGE_PARAMS pTask = New (USER_CHANGE_PARAMS);
               memset (pTask, 0x00, sizeof(USER_CHANGE_PARAMS));
               pTask->idUser = lpp->pUserList->aEntries[ ii ].idObject;

               // From Advanced tab:

               if (!lpp->fApplyAdvanced || lpp->fSeal_Mixed)
                  pTask->NewProperties.fEncrypt = Properties.u.UserProperties.KASINFO.fEncrypt;
               else
                  pTask->NewProperties.fEncrypt = lpp->fSeal;

               if (!lpp->fApplyAdvanced || lpp->fAdmin_Mixed)
                  pTask->NewProperties.fIsAdmin = Properties.u.UserProperties.KASINFO.fIsAdmin;
               else
                  pTask->NewProperties.fIsAdmin = lpp->fAdmin;

               if (!lpp->fApplyAdvanced || lpp->fGrantTickets_Mixed)
                  pTask->NewProperties.fCanGetTickets = Properties.u.UserProperties.KASINFO.fCanGetTickets;
               else
                  pTask->NewProperties.fCanGetTickets = lpp->fGrantTickets;

               if (!lpp->fApplyAdvanced || lpp->fGrantTickets_Mixed)
                  pTask->NewProperties.csecTicketLifetime = Properties.u.UserProperties.KASINFO.csecTicketLifetime;
               else if (!lpp->fGrantTickets)
                  pTask->NewProperties.csecTicketLifetime = 0;
               else
                  pTask->NewProperties.csecTicketLifetime = lpp->csecLifetime;

               if (!lpp->fApplyAdvanced || lpp->fGroupQuota_Mixed)
                  pTask->NewProperties.cgroupCreationQuota = Properties.u.UserProperties.PTSINFO.cgroupCreationQuota;
               else
                  pTask->NewProperties.cgroupCreationQuota = lpp->cGroupQuota;

               if (!lpp->fApplyAdvanced || lpp->fStatus_Mixed)
                  pTask->NewProperties.aaListStatus = Properties.u.UserProperties.PTSINFO.aaListStatus;
               else
                  pTask->NewProperties.aaListStatus = lpp->aaStatus;

               if (!lpp->fApplyAdvanced || lpp->fOwned_Mixed)
                  pTask->NewProperties.aaGroupsOwned = Properties.u.UserProperties.PTSINFO.aaGroupsOwned;
               else
                  pTask->NewProperties.aaGroupsOwned = lpp->aaOwned;

               if (!lpp->fApplyAdvanced || lpp->fMember_Mixed)
                  pTask->NewProperties.aaMembership = Properties.u.UserProperties.PTSINFO.aaMembership;
               else
                  pTask->NewProperties.aaMembership = lpp->aaMember;

               // From General tab:

               if (!lpp->fApplyGeneral || lpp->fCanChangePw_Mixed)
                  pTask->NewProperties.fCanChangePassword = Properties.u.UserProperties.KASINFO.fCanChangePassword;
               else
                  pTask->NewProperties.fCanChangePassword = lpp->fCanChangePw;

               if (!lpp->fApplyGeneral || lpp->fCanReusePw_Mixed)
                  pTask->NewProperties.fCanReusePasswords = Properties.u.UserProperties.KASINFO.fCanReusePasswords;
               else
                  pTask->NewProperties.fCanReusePasswords = lpp->fCanReusePw;

               if (!lpp->fApplyGeneral || lpp->fPwExpires_Mixed)
                  pTask->NewProperties.cdayPwExpire = Properties.u.UserProperties.KASINFO.cdayPwExpire;
               else
                  pTask->NewProperties.cdayPwExpire = lpp->cdayPwExpires;

               if (!lpp->fApplyGeneral || lpp->fExpires_Mixed)
                  memcpy (&pTask->NewProperties.timeExpires, &Properties.u.UserProperties.KASINFO.timeExpires, sizeof(SYSTEMTIME));
               else if (!lpp->fExpires)
                  memset (&pTask->NewProperties.timeExpires, 0x00, sizeof(SYSTEMTIME));
               else // (lpp->fExpires)
                  memcpy (&pTask->NewProperties.timeExpires, &lpp->stExpires, sizeof(SYSTEMTIME));

               if (!lpp->fApplyGeneral || lpp->fFailLock_Mixed)
                  pTask->NewProperties.cFailLogin = Properties.u.UserProperties.KASINFO.cFailLogin;
               else
                  pTask->NewProperties.cFailLogin = lpp->cFailLock;

               if (!lpp->fApplyGeneral || lpp->fFailLock_Mixed)
                  pTask->NewProperties.csecFailLoginLock = Properties.u.UserProperties.KASINFO.csecFailLoginLock;
               else
                  pTask->NewProperties.csecFailLoginLock = lpp->csecFailLock;

               StartTask (taskUSER_CHANGE, NULL, pTask);
               }
            }
         }

      if (lpp->pGroupsMember)
         asc_AsidListFree (&lpp->pGroupsMember);
      if (lpp->pGroupsOwner)
         asc_AsidListFree (&lpp->pGroupsOwner);
      if (lpp->pUserList)
         asc_AsidListFree (&lpp->pUserList);
      memset (lpp, 0x00, sizeof(USERPROPINFO));
      Delete (lpp);
      }
}


void User_ShowProperties (LPASIDLIST pUserList, USERPROPTAB uptTarget)
{
   LPUSERPROPINFO lpp = New (USERPROPINFO);
   memset (lpp, 0x00, sizeof(USERPROPINFO));
   lpp->pUserList = pUserList;
   lpp->fDeleteMeOnClose = TRUE;
   lpp->fShowModal = FALSE;

   if (pUserList && pUserList->cEntries)
      lpp->fMachine = fIsMachineAccount (pUserList->aEntries[0].idObject);

   User_ShowProperties (lpp, uptTarget);
}


void User_ShowProperties (LPUSERPROPINFO lpp, USERPROPTAB uptTarget)
{
   HWND hSheet = NULL;

   // If we've been asked to view properties for only one user, and there is
   // already a window open for that user, switch to it.
   //
   if (lpp->pUserList)
      {
      if (lpp->pUserList->cEntries == 1)
         {
         ASID idUser = lpp->pUserList->aEntries[0].idObject;

         if ((hSheet = WindowList_Search (wltUSER_PROPERTIES, idUser)) != NULL)
            {
            SetForegroundWindow (hSheet);
            if (uptTarget != uptANY)
               {
               HWND hTab = GetDlgItem (hSheet, IDC_PROPSHEET_TABCTRL);
               int nTabs = TabCtrl_GetItemCount (hTab);
               if (nTabs < nUSERPROPTAB_MAX)
                  uptTarget = (USERPROPTAB)( ((int)uptTarget-1) );
               TabCtrl_SwitchToTab (hTab, uptTarget);
               }
            return;
            }
         }

      // Otherwise, make sure there are no Properties windows open for any
      // of the selected users
      //
      for (size_t iUser = 0; iUser < lpp->pUserList->cEntries; ++iUser)
         {
         ASID idUser = lpp->pUserList->aEntries[ iUser ].idObject;

         // See if there's a Properties window open that is dedicated to
         // this user
         //
         if ((hSheet = WindowList_Search (wltUSER_PROPERTIES, idUser)) != NULL)
            {
            ErrorDialog (0, IDS_ERROR_USER_MULTIPROP);
            return;
            }

         // See if there is a multiple-user properties window open; if so,
         // test it to see if it covers this user
         //
         if ((hSheet = WindowList_Search (wltUSER_PROPERTIES, 0)) != NULL)
            {
            LPUSERPROPINFO pInfo = (LPUSERPROPINFO)PropSheet_FindTabParam (hSheet);
            if (pInfo && pInfo->pUserList && asc_AsidListTest (&pInfo->pUserList, idUser))
               {
               ErrorDialog (0, IDS_ERROR_USER_MULTIPROP);
               return;
               }
            }
         }
      }

   // Okay, we're clear to create the new properties window.
   //
   LPTSTR pszTitle;
   if (!lpp->pUserList)
      {
      if (lpp->fMachine)
         pszTitle = FormatString (IDS_NEWMACHINE_PROPERTIES_TITLE);
      else
         pszTitle = FormatString (IDS_NEWUSER_PROPERTIES_TITLE);
      }
   else if (lpp->pUserList->cEntries > 1)
      {
      if (lpp->fMachine)
         pszTitle = FormatString (IDS_MACHINE_PROPERTIES_TITLE_MULTIPLE);
      else
         pszTitle = FormatString (IDS_USER_PROPERTIES_TITLE_MULTIPLE);
      }
   else
      {
      TCHAR szUser[ cchRESOURCE ];
      User_GetDisplayName (szUser, lpp->pUserList->aEntries[0].idObject);
      if (lpp->fMachine)
         pszTitle = FormatString (IDS_MACHINE_PROPERTIES_TITLE, TEXT("%s"), szUser);
      else
         pszTitle = FormatString (IDS_USER_PROPERTIES_TITLE, TEXT("%s"), szUser);
      }

   if (lpp->fMachine && (uptTarget == uptGENERAL))
      uptTarget = uptADVANCED;

   LPPROPSHEET psh = PropSheet_Create (pszTitle, TRUE, lpp->hParent, (LPARAM)lpp);
   if (!lpp->fMachine)
      PropSheet_AddTab (psh, 0, (lpp->pUserList) ? IDD_USER_GENERAL : IDD_NEWUSER_GENERAL, (DLGPROC)UserProp_General_DlgProc, (LPARAM)lpp, TRUE, (uptTarget == uptGENERAL) ? TRUE : FALSE);
   PropSheet_AddTab (psh, 0, (lpp->pUserList) ? (lpp->fMachine ? IDD_MACHINE_ADVANCED : IDD_USER_ADVANCED) : (lpp->fMachine ? IDD_NEWMACHINE_ADVANCED : IDD_NEWUSER_ADVANCED), (DLGPROC)UserProp_Advanced_DlgProc, (LPARAM)lpp, TRUE, (uptTarget == uptADVANCED) ? TRUE : FALSE);
   PropSheet_AddTab (psh, 0, (lpp->pUserList) ? (lpp->fMachine ? IDD_MACHINE_MEMBER : IDD_USER_MEMBER) : (lpp->fMachine ? IDD_NEWMACHINE_MEMBER : IDD_NEWUSER_MEMBER), (DLGPROC)UserProp_Member_DlgProc, (LPARAM)lpp, TRUE, (uptTarget == uptMEMBERSHIP) ? TRUE : FALSE);

   if (lpp->fShowModal)
      PropSheet_ShowModal (psh, PumpMessage);
   else
      PropSheet_ShowModeless (psh, SW_SHOW);
}


BOOL CALLBACK UserProp_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_USER_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         UserProp_General_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         UserProp_General_OnDestroy (hDlg);
         break;

      case WM_ASC_NOTIFY_OBJECT:
         UserProp_General_UpdateDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               UserProp_General_OnApply (hDlg);
               break;

            case IDC_USER_UNLOCK:
               UserProp_General_OnUnlock (hDlg);
               break;

            case IDC_USER_CPW_NOW:
               UserProp_General_OnChangePwNow (hDlg);
               break;

            case IDC_USER_CPW:
               UserProp_General_OnChangePw (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_PWEXPIRES:
               UserProp_General_OnPwExpires (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_EXPIRES:
               UserProp_General_OnExpiration (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_FAILLOCK:
               UserProp_General_OnFailLock (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_FAILLOCK_INFINITE:
            case IDC_USER_FAILLOCK_FINITE:
               UserProp_General_OnFailLockTime (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_RPW:
            case IDC_USER_PWEXPIRATION:
            case IDC_USER_EXPIRE_DATE:
            case IDC_USER_EXPIRE_TIME:
            case IDC_USER_FAILLOCK_COUNT:
            case IDC_USER_FAILLOCK_DURATION:
               PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void UserProp_General_OnInitDialog (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // Indicate we want to know if anything changes with these users
   //
   if (lpp->pUserList)
      {
      LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
      memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
      pTask->hNotify = hDlg;
      asc_AsidListCopy (&pTask->pAsidList, &lpp->pUserList);
      StartTask (taskOBJECT_LISTEN, NULL, pTask);
      }

   // Fix the name shown on the dialog
   //
   UserProp_UpdateName (hDlg);

   // Update the dialog's display
   //
   if (lpp->pUserList && (lpp->pUserList->cEntries > 1))
      EnableWindow (GetDlgItem (hDlg, IDC_USER_CPW_NOW), FALSE);

   UserProp_General_UpdateDialog (hDlg);
}


void UserProp_General_OnDestroy (HWND hDlg)
{
   // Indicate we no longer care if anything changes with these users
   //
   LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
   memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
   pTask->hNotify = hDlg;
   StartTask (taskOBJECT_LISTEN, NULL, pTask);
}


void UserProp_General_UpdateDialog (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // Most of the controls on the dialog can be gathered by looping
   // through our pUserList.
   //
   BOOL fGotAnyData = FALSE;

   for (size_t ii = 0; lpp->pUserList && (ii < lpp->pUserList->cEntries); ++ii)
      {
      ULONG status;
      ASOBJPROP Properties;
      if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pUserList->aEntries[ ii ].idObject, &Properties, &status))
         continue;

      if (!fGotAnyData)
         {
         lpp->fCanChangePw = Properties.u.UserProperties.KASINFO.fCanChangePassword;
         lpp->fCanReusePw = Properties.u.UserProperties.KASINFO.fCanReusePasswords;
         lpp->cdayPwExpires = Properties.u.UserProperties.KASINFO.cdayPwExpire;
         lpp->cFailLock = Properties.u.UserProperties.KASINFO.cFailLogin;
         lpp->csecFailLock = Properties.u.UserProperties.KASINFO.csecFailLoginLock;
         lpp->fExpires = fIsValidDate (&Properties.u.UserProperties.KASINFO.timeExpires);
         if (lpp->fExpires)
            lpp->stExpires = Properties.u.UserProperties.KASINFO.timeExpires;
         else
            GetLocalSystemTime (&lpp->stExpires);
         fGotAnyData = TRUE;
         }
      else
         {
         if (lpp->fCanChangePw != Properties.u.UserProperties.KASINFO.fCanChangePassword)
            lpp->fCanChangePw_Mixed = TRUE;
         if (lpp->fCanReusePw != Properties.u.UserProperties.KASINFO.fCanReusePasswords)
            lpp->fCanReusePw_Mixed = TRUE;
         if (lpp->cdayPwExpires != Properties.u.UserProperties.KASINFO.cdayPwExpire)
            lpp->fPwExpires_Mixed = TRUE;
         if (lpp->cFailLock != Properties.u.UserProperties.KASINFO.cFailLogin)
            lpp->fFailLock_Mixed = TRUE;
         if (lpp->csecFailLock != Properties.u.UserProperties.KASINFO.csecFailLoginLock)
            lpp->fFailLock_Mixed = TRUE;
         if (lpp->fExpires != fIsValidDate (&Properties.u.UserProperties.KASINFO.timeExpires))
            lpp->fExpires_Mixed = TRUE;
         if (memcmp (&lpp->stExpires, &Properties.u.UserProperties.KASINFO.timeExpires, sizeof(SYSTEMTIME)))
            lpp->fExpires_Mixed = TRUE;
         }
      }

   // Set up the dialog controls to reflect what we just learned
   //
   if (lpp->fCanChangePw_Mixed)
      Set3State (hDlg, IDC_USER_CPW);
   else
      Set2State (hDlg, IDC_USER_CPW);
   CheckDlgButton (hDlg, IDC_USER_CPW, (lpp->fCanChangePw_Mixed) ? BST_INDETERMINATE : lpp->fCanChangePw);

   if (lpp->fCanReusePw_Mixed)
      Set3State (hDlg, IDC_USER_RPW);
   else
      Set2State (hDlg, IDC_USER_RPW);
   CheckDlgButton (hDlg, IDC_USER_RPW, (lpp->fCanReusePw_Mixed) ? BST_INDETERMINATE : lpp->fCanReusePw);

   if (lpp->fPwExpires_Mixed)
      Set3State (hDlg, IDC_USER_PWEXPIRES);
   else
      Set2State (hDlg, IDC_USER_PWEXPIRES);
   CheckDlgButton (hDlg, IDC_USER_PWEXPIRES, (lpp->fPwExpires_Mixed) ? BST_INDETERMINATE : (lpp->cdayPwExpires == 0) ? BST_UNCHECKED : BST_CHECKED);

   if (fHasSpinner (GetDlgItem (hDlg, IDC_USER_PWEXPIRATION)))
      SP_SetPos (GetDlgItem (hDlg, IDC_USER_PWEXPIRATION), lpp->cdayPwExpires);
   else
      CreateSpinner (GetDlgItem (hDlg, IDC_USER_PWEXPIRATION), 10, FALSE, 1, lpp->cdayPwExpires, cdayPWEXPIRATION_MAX);

   if (lpp->fExpires_Mixed)
      Set3State (hDlg, IDC_USER_EXPIRES);
   else
      Set2State (hDlg, IDC_USER_EXPIRES);
   CheckDlgButton (hDlg, IDC_USER_EXPIRES, (lpp->fExpires_Mixed) ? BST_INDETERMINATE : lpp->fExpires);

   SYSTEMTIME stExpires;
   if (fIsValidDate (&lpp->stExpires))
      memcpy (&stExpires, &lpp->stExpires, sizeof(SYSTEMTIME));
   else
      {
      GetSystemTime (&stExpires);
      stExpires.wYear ++;
      }

   // Our controls use local time, not GMT--so translate the account expiration
   // date before we display it
   //
   FILETIME ftGMT;
   SystemTimeToFileTime (&stExpires, &ftGMT);
   FILETIME ftLocal;
   FileTimeToLocalFileTime (&ftGMT, &ftLocal);
   SYSTEMTIME stLocal;
   FileTimeToSystemTime (&ftLocal, &stLocal);

   DA_SetDate (GetDlgItem (hDlg, IDC_USER_EXPIRE_DATE), &stLocal);
   TI_SetTime (GetDlgItem (hDlg, IDC_USER_EXPIRE_TIME), &stLocal);

   if (lpp->fFailLock_Mixed)
      Set3State (hDlg, IDC_USER_FAILLOCK);
   else
      Set2State (hDlg, IDC_USER_FAILLOCK);
   CheckDlgButton (hDlg, IDC_USER_FAILLOCK, (lpp->fFailLock_Mixed) ? BST_INDETERMINATE : (lpp->cFailLock == 0) ? BST_UNCHECKED : BST_CHECKED);

   if (fHasSpinner (GetDlgItem (hDlg, IDC_USER_FAILLOCK_COUNT)))
      SP_SetPos (GetDlgItem (hDlg, IDC_USER_FAILLOCK_COUNT), lpp->cFailLock);
   else
      CreateSpinner (GetDlgItem (hDlg, IDC_USER_FAILLOCK_COUNT), 10, FALSE, 1, lpp->cFailLock, cFAILLOCKCOUNT_MAX);

   CheckDlgButton (hDlg, IDC_USER_FAILLOCK_INFINITE, (lpp->csecFailLock == 0) ? TRUE : FALSE);
   CheckDlgButton (hDlg, IDC_USER_FAILLOCK_FINITE,   (lpp->csecFailLock != 0) ? TRUE : FALSE);

   SYSTEMTIME stMin;
   SYSTEMTIME stNow;
   SYSTEMTIME stMax;
   ULONG csecMin = 1;
   ULONG csecNow = lpp->csecFailLock;
   ULONG csecMax = csecFAILLOCK_MAX;
   SET_ELAPSED_TIME_FROM_SECONDS (&stMin, csecMin);
   SET_ELAPSED_TIME_FROM_SECONDS (&stNow, csecNow);
   SET_ELAPSED_TIME_FROM_SECONDS (&stMax, csecMax);
   EL_SetRange (GetDlgItem (hDlg, IDC_USER_FAILLOCK_DURATION), &stMin, &stMax);
   EL_SetTime (GetDlgItem (hDlg, IDC_USER_FAILLOCK_DURATION), &stNow);

   UserProp_General_OnChangePw (hDlg);
   UserProp_General_OnPwExpires (hDlg);
   UserProp_General_OnFailLock (hDlg);
   UserProp_General_OnFailLockTime (hDlg);
   UserProp_General_OnExpiration (hDlg);
}


void UserProp_General_OnChangePw (HWND hDlg)
{
   BOOL fEnable = (IsDlgButtonChecked (hDlg, IDC_USER_CPW) == BST_CHECKED);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_RPW), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_PWEXPIRES), fEnable);
   UserProp_General_OnPwExpires (hDlg);
}


void UserProp_General_OnPwExpires (HWND hDlg)
{
   BOOL fEnable = IsWindowEnabled (GetDlgItem (hDlg, IDC_USER_PWEXPIRES)) && (IsDlgButtonChecked (hDlg, IDC_USER_PWEXPIRES) == BST_CHECKED);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_PWEXPIRATION), fEnable);
}


void UserProp_General_OnExpiration (HWND hDlg)
{
   BOOL fEnable = (IsDlgButtonChecked (hDlg, IDC_USER_EXPIRES) == BST_CHECKED);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_EXPIRE_DATE), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_EXPIRE_AT), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_EXPIRE_TIME), fEnable);
}


void UserProp_General_OnFailLock (HWND hDlg)
{
   BOOL fEnable = (IsDlgButtonChecked (hDlg, IDC_USER_FAILLOCK) == BST_CHECKED);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_FAILLOCK_COUNT), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_FAILLOCK_INFINITE), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_FAILLOCK_FINITE), fEnable);
   UserProp_General_OnFailLockTime (hDlg);
}


void UserProp_General_OnFailLockTime (HWND hDlg)
{
   BOOL fEnable = IsWindowEnabled (GetDlgItem (hDlg, IDC_USER_FAILLOCK_FINITE)) && (IsDlgButtonChecked (hDlg, IDC_USER_FAILLOCK_FINITE) == BST_CHECKED);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_FAILLOCK_DURATION), fEnable);
}


void UserProp_General_OnApply (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   lpp->fApplyGeneral = TRUE;

   lpp->fCanChangePw = IsDlgButtonChecked (hDlg, IDC_USER_CPW);
   lpp->fCanChangePw_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_CPW) == BST_INDETERMINATE);
   lpp->fCanReusePw = IsDlgButtonChecked (hDlg, IDC_USER_RPW);
   lpp->fCanReusePw_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_RPW) == BST_INDETERMINATE);

   lpp->fExpires = IsDlgButtonChecked (hDlg, IDC_USER_EXPIRES);
   lpp->fExpires_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_EXPIRES) == BST_INDETERMINATE);

   // Our controls use local time, not GMT--so translate the account expiration
   // date after we read it from the controls
   //
   SYSTEMTIME stLocal;
   memset (&stLocal, 0x00, sizeof(SYSTEMTIME));
   DA_GetDate (GetDlgItem (hDlg, IDC_USER_EXPIRE_DATE), &stLocal);
   TI_GetTime (GetDlgItem (hDlg, IDC_USER_EXPIRE_TIME), &stLocal);

   FILETIME ftLocal;
   SystemTimeToFileTime (&stLocal, &ftLocal);
   FILETIME ftGMT;
   LocalFileTimeToFileTime (&ftLocal, &ftGMT);
   FileTimeToSystemTime (&ftGMT, &lpp->stExpires);

   if (!IsDlgButtonChecked (hDlg, IDC_USER_PWEXPIRES))
      lpp->cdayPwExpires = 0;
   else
      lpp->cdayPwExpires = (LONG) SP_GetPos (GetDlgItem (hDlg, IDC_USER_PWEXPIRATION));
   lpp->fPwExpires_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_PWEXPIRES) == BST_INDETERMINATE);

   if (!IsDlgButtonChecked (hDlg, IDC_USER_FAILLOCK))
      lpp->cFailLock = 0;
   else
      lpp->cFailLock = (LONG) SP_GetPos (GetDlgItem (hDlg, IDC_USER_FAILLOCK_COUNT));
   lpp->fFailLock_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_FAILLOCK) == BST_INDETERMINATE);

   if (!IsDlgButtonChecked (hDlg, IDC_USER_FAILLOCK_FINITE))
      lpp->csecFailLock = 0;
   else
      {
      SYSTEMTIME stFailLock;
      EL_GetTime (GetDlgItem (hDlg, IDC_USER_FAILLOCK_DURATION), &stFailLock);
      lpp->csecFailLock = GET_SECONDS_FROM_ELAPSED_TIME (&stFailLock);
      }
}


void UserProp_General_OnUnlock (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);
   if (lpp->pUserList)
      {
      LPASIDLIST pCopy;
      asc_AsidListCopy (&pCopy, &lpp->pUserList);
      StartTask (taskUSER_UNLOCK, NULL, pCopy);
      }
}


void UserProp_General_OnChangePwNow (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);
   if (lpp->pUserList && (lpp->pUserList->cEntries == 1))
      {
      User_ShowChangePassword (hDlg, lpp->pUserList->aEntries[0].idObject);
      }
}


BOOL CALLBACK UserProp_Advanced_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   if (AfsAppLib_HandleHelp ((lpp && lpp->fMachine) ? IDD_MACHINE_ADVANCED : IDD_USER_ADVANCED, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         if (lpp && lpp->pUserList && !lpp->fShowModal)
            {
            if (lpp->pUserList->cEntries == 1)
               WindowList_Add (hDlg, wltUSER_PROPERTIES, lpp->pUserList->aEntries[0].idObject);
            else // (lpp->pUserList->cEntries > 1)
               WindowList_Add (hDlg, wltUSER_PROPERTIES, 0);
            }
         break;

      case WM_DESTROY_SHEET:
         WindowList_Remove (hDlg);
         if (lpp && lpp->fDeleteMeOnClose)
            User_FreeProperties (lpp);
         break;

      case WM_INITDIALOG:
         UserProp_Advanced_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         UserProp_Advanced_OnDestroy (hDlg);
         break;

      case WM_ASC_NOTIFY_OBJECT:
         UserProp_Advanced_UpdateDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               UserProp_Advanced_OnApply (hDlg);
               break;

            case IDC_USER_TGS:
               UserProp_Advanced_OnGrantTickets (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_GROUP_HASQUOTA:
               UserProp_Advanced_OnHasGroupQuota (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_ADMIN:
               UserProp_Advanced_OnAdmin (hDlg);
               PropSheetChanged (hDlg);
               break;

            case IDC_USER_NOSEAL:
            case IDC_USER_LIFETIME:
            case IDC_USER_GROUP_QUOTA:
            case IDC_USER_PERM_STATUS:
            case IDC_USER_PERM_OWNED:
            case IDC_USER_PERM_MEMBER:
               PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void UserProp_Advanced_OnInitDialog (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // Indicate we want to know if anything changes with these users
   //
   if (lpp->pUserList)
      {
      LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
      memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
      pTask->hNotify = hDlg;
      asc_AsidListCopy (&pTask->pAsidList, &lpp->pUserList);
      StartTask (taskOBJECT_LISTEN, NULL, pTask);
      }

   // Fix the name shown on the dialog
   //
   UserProp_UpdateName (hDlg);

   // Fill in the information about the selected users
   //
   UserProp_Advanced_UpdateDialog (hDlg);
}


void UserProp_Advanced_OnDestroy (HWND hDlg)
{
   // Indicate we no longer care if anything changes with these users
   //
   LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
   memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
   pTask->hNotify = hDlg;
   StartTask (taskOBJECT_LISTEN, NULL, pTask);
}


void UserProp_Advanced_OnGrantTickets (HWND hDlg)
{
   BOOL fEnable = (IsDlgButtonChecked (hDlg, IDC_USER_TGS) == BST_CHECKED);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_LIFETIME), fEnable);
}


void UserProp_Advanced_OnHasGroupQuota (HWND hDlg)
{
   BOOL fEnable = (IsDlgButtonChecked (hDlg, IDC_USER_GROUP_HASQUOTA) == BST_CHECKED);
   if (!IsWindowEnabled (GetDlgItem (hDlg, IDC_USER_GROUP_HASQUOTA)))
      fEnable = FALSE;
   EnableWindow (GetDlgItem (hDlg, IDC_USER_GROUP_QUOTA), fEnable);
}


void UserProp_Advanced_OnAdmin (HWND hDlg)
{
   BOOL fAdmin = (IsDlgButtonChecked (hDlg, IDC_USER_ADMIN) == BST_CHECKED);
   EnableWindow (GetDlgItem (hDlg, IDC_USER_GROUP_HASQUOTA), !fAdmin);

   if (fAdmin)
      {
      CheckDlgButton (hDlg, IDC_USER_GROUP_HASQUOTA, TRUE);
      SP_SetPos (GetDlgItem (hDlg, IDC_USER_GROUP_QUOTA), 9999);
      }

   UserProp_Advanced_OnHasGroupQuota (hDlg);
}


void UserProp_Advanced_UpdateDialog (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // Fill in the user's key. If this dialog represents more than one
   // user, put in some stock text instead.
   //
   if ((lpp->pUserList) && (lpp->pUserList->cEntries > 1))
      {
      TCHAR szText[ cchRESOURCE ];
      GetString (szText, IDS_USER_KEY_MULTIPLE);
      SetDlgItemText (hDlg, IDC_USER_KEY, szText);
      }
   else if (lpp->pUserList && (lpp->pUserList->cEntries == 1))
      {
      ULONG status;
      ASOBJPROP Properties;
      if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pUserList->aEntries[ 0 ].idObject, &Properties, &status))
         {
         TCHAR szText[ cchRESOURCE ];
         GetString (szText, IDS_USER_KEY_UNKNOWN);
         SetDlgItemText (hDlg, IDC_USER_KEY, szText);
         }
      else if (!Properties.u.UserProperties.fHaveKasInfo)
         {
         TCHAR szText[ cchRESOURCE ];
         GetString (szText, IDS_USER_KEY_UNKNOWN);
         SetDlgItemText (hDlg, IDC_USER_KEY, szText);
         }
      else
         {
         TCHAR szReadableKey[ cchRESOURCE ];
         FormatServerKey (szReadableKey, Properties.u.UserProperties.KASINFO.keyData);

         LPTSTR pszText = FormatString (IDS_USER_KEY, TEXT("%s%lu%lu"), szReadableKey, Properties.u.UserProperties.KASINFO.keyVersion, Properties.u.UserProperties.KASINFO.dwKeyChecksum);
         SetDlgItemText (hDlg, IDC_USER_KEY, pszText);
         FreeString (pszText);
         }
      }

   // Fill in the other fields
   //
   BOOL fGotAnyData = FALSE;

   for (size_t ii = 0; lpp->pUserList && (ii < lpp->pUserList->cEntries); ++ii)
      {
      ULONG status;
      ASOBJPROP Properties;
      if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pUserList->aEntries[ ii ].idObject, &Properties, &status))
         continue;

      if (!fGotAnyData)
         {
         lpp->fSeal = Properties.u.UserProperties.KASINFO.fEncrypt;
         lpp->fAdmin = Properties.u.UserProperties.KASINFO.fIsAdmin;
         lpp->fGrantTickets = Properties.u.UserProperties.KASINFO.fCanGetTickets;
         lpp->csecLifetime = Properties.u.UserProperties.KASINFO.csecTicketLifetime;
         lpp->cGroupQuota = Properties.u.UserProperties.PTSINFO.cgroupCreationQuota;
         lpp->aaStatus = Properties.u.UserProperties.PTSINFO.aaListStatus;
         lpp->aaOwned = Properties.u.UserProperties.PTSINFO.aaGroupsOwned;
         lpp->aaMember = Properties.u.UserProperties.PTSINFO.aaMembership;
         fGotAnyData = TRUE;
         }
      else // (fGotAnyData)
         {
         if (lpp->fSeal != Properties.u.UserProperties.KASINFO.fEncrypt)
            lpp->fSeal_Mixed = TRUE;
         if (lpp->fAdmin != Properties.u.UserProperties.KASINFO.fIsAdmin)
            lpp->fAdmin_Mixed = TRUE;
         if (lpp->fGrantTickets != Properties.u.UserProperties.KASINFO.fCanGetTickets)
            lpp->fGrantTickets_Mixed = TRUE;
         if (lpp->csecLifetime != (ULONG)Properties.u.UserProperties.KASINFO.csecTicketLifetime)
            lpp->fGrantTickets_Mixed = TRUE;
         if (lpp->cGroupQuota != Properties.u.UserProperties.PTSINFO.cgroupCreationQuota)
            lpp->fGroupQuota_Mixed = TRUE;
         if (lpp->aaStatus != Properties.u.UserProperties.PTSINFO.aaListStatus)
            lpp->fStatus_Mixed = TRUE;
         if (lpp->aaOwned != Properties.u.UserProperties.PTSINFO.aaGroupsOwned)
            lpp->fOwned_Mixed = TRUE;
         if (lpp->aaMember != Properties.u.UserProperties.PTSINFO.aaMembership)
            lpp->fMember_Mixed = TRUE;
         }
      }

   // Fill in the dialog's state
   //
   CheckDlgButton (hDlg, IDC_USER_CREATE_KAS, lpp->fCreateKAS);
   CheckDlgButton (hDlg, IDC_USER_CREATE_PTS, lpp->fCreatePTS);

   if (lpp->fSeal_Mixed)
      Set3State (hDlg, IDC_USER_NOSEAL);
   else
      Set2State (hDlg, IDC_USER_NOSEAL);
   CheckDlgButton (hDlg, IDC_USER_NOSEAL, (lpp->fSeal_Mixed) ? BST_INDETERMINATE : (!lpp->fSeal));

   if (lpp->fAdmin_Mixed)
      Set3State (hDlg, IDC_USER_ADMIN);
   else
      Set2State (hDlg, IDC_USER_ADMIN);
   CheckDlgButton (hDlg, IDC_USER_ADMIN, (lpp->fAdmin_Mixed) ? BST_INDETERMINATE : lpp->fAdmin);

   if (lpp->fGrantTickets_Mixed)
      Set3State (hDlg, IDC_USER_TGS);
   else
      Set2State (hDlg, IDC_USER_TGS);
   CheckDlgButton (hDlg, IDC_USER_TGS, (lpp->fGrantTickets_Mixed) ? BST_INDETERMINATE : lpp->fGrantTickets);

   SYSTEMTIME stMin;
   SYSTEMTIME stNow;
   SYSTEMTIME stMax;
   ULONG csecMin = 0;
   ULONG csecNow = lpp->csecLifetime;
   ULONG csecMax = csecTICKETLIFETIME_MAX;
   SET_ELAPSED_TIME_FROM_SECONDS (&stMin, csecMin);
   SET_ELAPSED_TIME_FROM_SECONDS (&stNow, csecNow);
   SET_ELAPSED_TIME_FROM_SECONDS (&stMax, csecMax);
   EL_SetRange (GetDlgItem (hDlg, IDC_USER_LIFETIME), &stMin, &stMax);
   EL_SetTime (GetDlgItem (hDlg, IDC_USER_LIFETIME), &stNow);

   if (lpp->fGroupQuota_Mixed)
      Set3State (hDlg, IDC_USER_GROUP_HASQUOTA);
   else
      Set2State (hDlg, IDC_USER_GROUP_HASQUOTA);
   CheckDlgButton (hDlg, IDC_USER_GROUP_HASQUOTA, (lpp->fGroupQuota_Mixed) ? BST_INDETERMINATE : (lpp->cGroupQuota == cGROUPQUOTA_INFINITE) ? BST_UNCHECKED : BST_CHECKED);

   if (fHasSpinner (GetDlgItem (hDlg, IDC_USER_GROUP_QUOTA)))
      SP_SetPos (GetDlgItem (hDlg, IDC_USER_GROUP_QUOTA), lpp->cGroupQuota);
   else
      CreateSpinner (GetDlgItem (hDlg, IDC_USER_GROUP_QUOTA), 10, FALSE, cGROUPQUOTA_MIN, lpp->cGroupQuota, cGROUPQUOTA_MAX);

   UserProp_Advanced_UpdateDialog_Perm (hDlg, IDC_USER_PERM_STATUS, lpp->aaStatus, lpp->fStatus_Mixed);
   UserProp_Advanced_UpdateDialog_Perm (hDlg, IDC_USER_PERM_OWNED, lpp->aaOwned, lpp->fOwned_Mixed);
   UserProp_Advanced_UpdateDialog_Perm (hDlg, IDC_USER_PERM_MEMBER, lpp->aaMember, lpp->fMember_Mixed);

   UserProp_Advanced_OnGrantTickets (hDlg);
   UserProp_Advanced_OnHasGroupQuota (hDlg);
   UserProp_Advanced_OnAdmin (hDlg);
}


void UserProp_Advanced_UpdateDialog_Perm (HWND hDlg, int idc, ACCOUNTACCESS aa, BOOL fMixed)
{
   CB_StartChange (GetDlgItem (hDlg, idc), TRUE);
   CB_AddItem (GetDlgItem (hDlg, idc), IDS_ACCOUNTACCESS_OWNER, aaOWNER_ONLY);
   CB_AddItem (GetDlgItem (hDlg, idc), IDS_ACCOUNTACCESS_ANYONE, aaANYONE);
   if (fMixed)
      CB_AddItem (GetDlgItem (hDlg, idc), IDS_ACCOUNTACCESS_MIXED, (LPARAM)-1);
   CB_EndChange (GetDlgItem (hDlg, idc), TRUE);

   LPARAM lpSelect = (fMixed) ? ((LPARAM)-1) : (LPARAM)aa;
   CB_SetSelectedByData (GetDlgItem (hDlg, idc), lpSelect);
}


void UserProp_Advanced_OnApply (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   lpp->fApplyAdvanced = TRUE;
   lpp->fCreateKAS = IsDlgButtonChecked (hDlg, IDC_USER_CREATE_KAS);
   lpp->fCreatePTS = IsDlgButtonChecked (hDlg, IDC_USER_CREATE_PTS);

   if (IsWindow (GetDlgItem (hDlg, IDC_USER_NOSEAL)))
      {
      lpp->fSeal = !IsDlgButtonChecked (hDlg, IDC_USER_NOSEAL);
      lpp->fSeal_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_NOSEAL) == BST_INDETERMINATE);
      }

   if (IsWindow (GetDlgItem (hDlg, IDC_USER_ADMIN)))
      {
      lpp->fAdmin = IsDlgButtonChecked (hDlg, IDC_USER_ADMIN);
      lpp->fAdmin_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_ADMIN) == BST_INDETERMINATE);
      }

   if (IsWindow (GetDlgItem (hDlg, IDC_USER_TGS)))
      {
      lpp->fGrantTickets = IsDlgButtonChecked (hDlg, IDC_USER_TGS);
      lpp->fGrantTickets_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_TGS) == BST_INDETERMINATE);
      }

   if (IsWindow (GetDlgItem (hDlg, IDC_USER_LIFETIME)))
      {
      SYSTEMTIME stLifetime;
      EL_GetTime (GetDlgItem (hDlg, IDC_USER_LIFETIME), &stLifetime);
      lpp->csecLifetime = GET_SECONDS_FROM_ELAPSED_TIME (&stLifetime);
      }

   if (IsWindow (GetDlgItem (hDlg, IDC_USER_GROUP_HASQUOTA)))
      {
      if (!IsDlgButtonChecked (hDlg, IDC_USER_GROUP_HASQUOTA))
         lpp->cGroupQuota = cGROUPQUOTA_INFINITE;
      else // (IsDlgButtonChecked (hDlg, IDC_USER_GROUP_HASQUOTA))
         lpp->cGroupQuota = (LONG) SP_GetPos (GetDlgItem (hDlg, IDC_USER_GROUP_QUOTA));
      lpp->fGroupQuota_Mixed = (IsDlgButtonChecked (hDlg, IDC_USER_GROUP_HASQUOTA) == BST_INDETERMINATE);
      }

   lpp->aaStatus = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_USER_PERM_STATUS));
   lpp->fStatus_Mixed = (lpp->aaStatus == (ACCOUNTACCESS)-1) ? TRUE : FALSE;

   lpp->aaOwned = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_USER_PERM_OWNED));
   lpp->fOwned_Mixed = (lpp->aaOwned == (ACCOUNTACCESS)-1) ? TRUE : FALSE;

   lpp->aaMember = (ACCOUNTACCESS)CB_GetSelectedData (GetDlgItem (hDlg, IDC_USER_PERM_MEMBER));
   lpp->fMember_Mixed = (lpp->aaMember == (ACCOUNTACCESS)-1) ? TRUE : FALSE;
}


BOOL CALLBACK UserProp_Member_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   if (AfsAppLib_HandleHelp ((lpp && lpp->fMachine) ? IDD_MACHINE_MEMBER : IDD_USER_MEMBER, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         UserProp_Member_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         UserProp_Member_OnDestroy (hDlg);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskGROUP_SEARCH)
               UserProp_Member_OnEndTask_GroupSearch (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               UserProp_Member_OnApply (hDlg);
               break;

            case IDC_USER_SHOW_MEMBER:
            case IDC_USER_SHOW_OWNER:
               UserProp_Member_OnShowType (hDlg);
               break;

            case IDC_MEMBER_ADD:
               UserProp_Member_OnAdd (hDlg);
               break;

            case IDC_MEMBER_REMOVE:
               UserProp_Member_OnRemove (hDlg);
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               UserProp_Member_OnSelect (hDlg);
               break;
            }
         break;
      }
   return FALSE;
}


void UserProp_Member_OnInitDialog (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // If we've come in here with a valid set of groups to display,
   // copy those. We'll need the copies if the user cancels the dialog.
   //
   SetWindowData (hDlg, GWD_ASIDLIST_MEMBER, (LPARAM)(lpp->pGroupsMember));
   SetWindowData (hDlg, GWD_ASIDLIST_OWNER,  (LPARAM)(lpp->pGroupsOwner));

   LPASIDLIST pList;
   if ((pList = lpp->pGroupsMember) != NULL)
      asc_AsidListCopy (&lpp->pGroupsMember, &pList);
   if ((pList = lpp->pGroupsOwner) != NULL)
      asc_AsidListCopy (&lpp->pGroupsOwner, &pList);

   // If this prop sheet reflects more than one user, disable the
   // Ownership button (we do this primarily because the Add/Remove
   // buttons have no straight-forward semantic for that case).
   // Actually, instead of disabling the thing, we'll hide the buttons
   // entirely and resize the list/move the title.
   //
   if (lpp->pUserList && (lpp->pUserList->cEntries > 1))
      {
      ShowWindow (GetDlgItem (hDlg, IDC_USER_SHOW_MEMBER), SW_HIDE);
      ShowWindow (GetDlgItem (hDlg, IDC_USER_SHOW_OWNER), SW_HIDE);

      RECT rButton;
      GetRectInParent (GetDlgItem (hDlg, IDC_USER_SHOW_MEMBER), &rButton);

      RECT rTitle;
      GetRectInParent (GetDlgItem (hDlg, IDC_GROUPS_TITLE), &rTitle);

      RECT rList;
      GetRectInParent (GetDlgItem (hDlg, IDC_GROUPS_LIST), &rList);

      LONG cyDelta = rTitle.top - rButton.top;

      SetWindowPos (GetDlgItem (hDlg, IDC_GROUPS_TITLE), NULL,
                    rTitle.left, rTitle.top-cyDelta,
                    rTitle.right-rTitle.left, rTitle.bottom-rTitle.top,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

      SetWindowPos (GetDlgItem (hDlg, IDC_GROUPS_LIST), NULL,
                    rList.left, rList.top-cyDelta,
                    rList.right-rList.left, rList.bottom-rList.top+cyDelta,
                    SWP_NOZORDER | SWP_NOACTIVATE);
      }

   // Apply an imagelist to the dialog's fastlist
   //
   FastList_SetImageLists (GetDlgItem (hDlg, IDC_GROUPS_LIST), AfsAppLib_CreateImageList (FALSE), NULL);

   // Select a checkbox and pretend that the user clicked it; that will
   // make the dialog automatically re-populate the list of groups
   //
   CheckDlgButton (hDlg, IDC_USER_SHOW_MEMBER, TRUE);
   UserProp_Member_OnShowType (hDlg);
}


void UserProp_Member_OnDestroy (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   LPASIDLIST pList;
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_MEMBER)) != NULL)
      lpp->pGroupsMember = pList;
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_OWNER)) != NULL)
      lpp->pGroupsOwner = pList;
}


void UserProp_Member_OnShowType (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // If we've already obtained the list of groups we should be displaying,
   // just go show it.
   //
   if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
      {
      if (lpp->pGroupsMember)
         {
         UserProp_Member_PopulateList (hDlg);
         return;
         }
      }
   else // (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_OWNER))
      {
      if (lpp->pGroupsOwner)
         {
         UserProp_Member_PopulateList (hDlg);
         return;
         }
      }

   // Otherwise, we'll have to start a background task to do the querying.
   // Change the text above the list to "Querying...", disable the buttons,
   // and remove any items in the list.
   //
   if (!lpp->pUserList)
      {
      // Generate a few empty ASID list; this is a new user account we're
      // displaying, and the caller hasn't yet added any groups.
      //
      if (!lpp->pGroupsMember)
         {
         if (!asc_AsidListCreate (&lpp->pGroupsMember))
            return;
         }
      if (!lpp->pGroupsOwner)
         {
         if (!asc_AsidListCreate (&lpp->pGroupsOwner))
            return;
         }
      UserProp_Member_OnShowType (hDlg);
      }
   else // (lpp->pUserList)
      {
      TCHAR szTitle[ cchRESOURCE ];
      GetString (szTitle, IDS_QUERYING_LONG);
      SetDlgItemText (hDlg, IDC_GROUPS_TITLE, szTitle);

      EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_ADD), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_REMOVE), FALSE);

      FastList_RemoveAll (GetDlgItem (hDlg, IDC_GROUPS_LIST));

      // Then start a background task to grab an ASIDLIST of groups which
      // match the specified search criteria.
      //
      LPGROUP_SEARCH_PARAMS pTask = New (GROUP_SEARCH_PARAMS);
      asc_AsidListCopy (&pTask->pUserList, &lpp->pUserList);
      pTask->fMembership = IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER);
      StartTask (taskGROUP_SEARCH, hDlg, pTask);
      }
}


void UserProp_Member_OnEndTask_GroupSearch (HWND hDlg, LPTASKPACKET ptp)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // The search is complete. If we've just obtained an ASIDLIST successfully,
   // associate it with the dialog and repopulate the display.
   //
   if (ptp->rc && TASKDATA(ptp)->pAsidList)
      {
      if (TASKDATA(ptp)->fMembership)
         {
         if (!lpp->pGroupsMember)
            {
            asc_AsidListCopy (&lpp->pGroupsMember, &TASKDATA(ptp)->pAsidList);
            UserProp_Member_PopulateList (hDlg);
            }
         }
      else // (!TASKDATA(ptp)->fMembership)
         {
         if (!lpp->pGroupsOwner)
            {
            asc_AsidListCopy (&lpp->pGroupsOwner, &TASKDATA(ptp)->pAsidList);
            UserProp_Member_PopulateList (hDlg);
            }
         }
      }
}


void UserProp_Member_PopulateList (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // Clear the list of groups
   //
   HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
   FastList_Begin (hList);
   FastList_RemoveAll (hList);

   // We should have an ASIDLIST associated with the dialog to reflect
   // which groups to display. Find it, and repopulate the list on the display.
   //
   LPASIDLIST pGroupList;
   if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
      pGroupList = lpp->pGroupsMember;
   else // (!IsDlgButtonChecked (hDlg, IDC_USER_SHOW_OWNER))
      pGroupList = lpp->pGroupsOwner;

   if (pGroupList)
      {
      TCHAR szMixed[ cchRESOURCE ];
      GetString (szMixed, IDS_MEMBER_MIXED);

      for (size_t ii = 0; ii < pGroupList->cEntries; ++ii)
         {
         ULONG status;
         TCHAR szName[ cchNAME ];
         if (!asc_ObjectNameGet_Fast (g.idClient, g.idCell, pGroupList->aEntries[ ii ].idObject, szName, &status))
            continue;
         if (!pGroupList->aEntries[ ii ].lParam)
            lstrcat (szName, szMixed);

         FASTLISTADDITEM flai;
         memset (&flai, 0x00, sizeof(flai));
         flai.iFirstImage = imageGROUP;
         flai.iSecondImage = IMAGE_NOIMAGE;
         flai.pszText = szName;
         flai.lParam = (LPARAM)(pGroupList->aEntries[ ii ].idObject);
         FastList_AddItem (hList, &flai);
         }
      }

   FastList_End (hList);

   // Fix the buttons, and the text at the top of the list
   //
   TCHAR szTitle[ cchRESOURCE ];
   if (lpp->fMachine)
      {
      if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
         {
         if (!lpp->pUserList) 
            GetString (szTitle, IDS_NEWMACHINE_SHOW_MEMBER_TITLE);
         else if (lpp->pUserList->cEntries == 1)
            GetString (szTitle, IDS_MACHINE_SHOW_MEMBER_TITLE);
         else
            GetString (szTitle, IDS_MACHINE_SHOW_MEMBER_TITLE_MULTIPLE);
         }
      else
         {
         if (!lpp->pUserList)
            GetString (szTitle, IDS_NEWMACHINE_SHOW_OWNER_TITLE);
         else
            GetString (szTitle, IDS_MACHINE_SHOW_OWNER_TITLE);
         }
      }
   else
      {
      if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
         {
         if (!lpp->pUserList) 
            GetString (szTitle, IDS_NEWUSER_SHOW_MEMBER_TITLE);
         else if (lpp->pUserList->cEntries == 1)
            GetString (szTitle, IDS_USER_SHOW_MEMBER_TITLE);
         else
            GetString (szTitle, IDS_USER_SHOW_MEMBER_TITLE_MULTIPLE);
         }
      else
         {
         if (!lpp->pUserList)
            GetString (szTitle, IDS_NEWUSER_SHOW_OWNER_TITLE);
         else
            GetString (szTitle, IDS_USER_SHOW_OWNER_TITLE);
         }
      }

   SetDlgItemText (hDlg, IDC_GROUPS_TITLE, szTitle);

   EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_ADD), TRUE);
   UserProp_Member_OnSelect (hDlg);
}


void UserProp_Member_OnSelect (HWND hDlg)
{
   BOOL fEnable = IsWindowEnabled (GetDlgItem (hDlg, IDC_MEMBER_ADD));
   if (fEnable && !FastList_FindFirstSelected (GetDlgItem (hDlg, IDC_GROUPS_LIST)))
      fEnable = FALSE;
   EnableWindow (GetDlgItem (hDlg, IDC_MEMBER_REMOVE), fEnable);
}


void UserProp_Member_OnAdd (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   LPBROWSE_PARAMS pParams = New (BROWSE_PARAMS);
   memset (pParams, 0x00, sizeof(BROWSE_PARAMS));
   pParams->hParent = GetParent(hDlg);
   pParams->iddForHelp = IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER) ? IDD_BROWSE_JOIN : IDD_BROWSE_OWN;
   pParams->idsTitle = IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER) ? IDS_BROWSE_TITLE_JOIN : IDS_BROWSE_TITLE_OWN;
   pParams->idsPrompt = IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER) ? IDS_BROWSE_PROMPT_JOIN : IDS_BROWSE_PROMPT_OWN;
   pParams->idsCheck = IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER) ? IDS_BROWSE_CHECK_JOIN : IDS_BROWSE_CHECK_OWN;
   pParams->TypeToShow = TYPE_GROUP;
   pParams->fAllowMultiple = TRUE;

   // First prepare an ASIDLIST which reflects only the groups which
   // all selected users own/are members in.
   //
   LPASIDLIST pGroupList;
   if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
      pGroupList = lpp->pGroupsMember;
   else // (!IsDlgButtonChecked (hDlg, IDC_USER_SHOW_OWNER))
      pGroupList = lpp->pGroupsOwner;

   if (pGroupList)
      {
      asc_AsidListCreate (&pParams->pObjectsToSkip);
      for (size_t ii = 0; ii < pGroupList->cEntries; ++ii)
         {
         if (pGroupList->aEntries[ ii ].lParam) // all users have this group?
            asc_AsidListAddEntry (&pParams->pObjectsToSkip, pGroupList->aEntries[ ii ].idObject, 0);
         }
      }

   if (ShowBrowseDialog (pParams))
      {
      // The user added some groups; modify pGroupList (and the dialog,
      // at the same time).
      //
      HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
      FastList_Begin (hList);

      for (size_t ii = 0; ii < pParams->pObjectsSelected->cEntries; ++ii)
         {
         ASID idGroup = pParams->pObjectsSelected->aEntries[ ii ].idObject;

         // The user has chosen to add group {idGroup}. If it's not in our
         // list at all, add it (and a new entry for the display). If it
         // *is* in our list, make sure its lParam is 1--indicating all users
         // get it--and modify the display's entry to remove the "(some)"
         // marker.
         //
         LPARAM lParam;
         if (!asc_AsidListTest (&pGroupList, idGroup, &lParam))
            {
            ULONG status;
            TCHAR szName[ cchNAME ];
            if (!asc_ObjectNameGet_Fast (g.idClient, g.idCell, idGroup, szName, &status))
               continue;

            asc_AsidListAddEntry (&pGroupList, idGroup, TRUE);

            FASTLISTADDITEM flai;
            memset (&flai, 0x00, sizeof(flai));
            flai.iFirstImage = imageGROUP;
            flai.iSecondImage = IMAGE_NOIMAGE;
            flai.pszText = szName;
            flai.lParam = (LPARAM)idGroup;
            FastList_AddItem (hList, &flai);
            PropSheetChanged (hDlg);
            }
         else if (!lParam)
            {
            ULONG status;
            TCHAR szName[ cchNAME ];
            if (!asc_ObjectNameGet_Fast (g.idClient, g.idCell, idGroup, szName, &status))
               continue;

            asc_AsidListSetEntryParam (&pGroupList, idGroup, TRUE);

            HLISTITEM hItem;
            if ((hItem = FastList_FindItem (hList, (LPARAM)idGroup)) != NULL)
               FastList_SetItemText (hList, hItem, 0, szName);
            PropSheetChanged (hDlg);
            }
         }

      if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
         lpp->pGroupsMember = pGroupList;
      else // (!IsDlgButtonChecked (hDlg, IDC_USER_SHOW_OWNER))
         lpp->pGroupsOwner = pGroupList;

      FastList_End (hList);
      }

   if (pParams->pObjectsToSkip)
      asc_AsidListFree (&pParams->pObjectsToSkip);
   if (pParams->pObjectsSelected)
      asc_AsidListFree (&pParams->pObjectsSelected);
   Delete (pParams);
}


void UserProp_Member_OnRemove (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // Find which list-of-groups is currently being displayed.
   //
   LPASIDLIST pGroupList;
   if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
      pGroupList = lpp->pGroupsMember;
   else // (!IsDlgButtonChecked (hDlg, IDC_USER_SHOW_OWNER))
      pGroupList = lpp->pGroupsOwner;

   // The user wants to remove some groups; modify pGroupList
   // (and the dialog, at the same time).
   //
   HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
   FastList_Begin (hList);

   HLISTITEM hItem;
   while ((hItem = FastList_FindFirstSelected (hList)) != NULL)
      {
      ASID idGroup = (ASID)FastList_GetItemParam (hList, hItem);
      FastList_RemoveItem (hList, hItem);
      asc_AsidListRemoveEntry (&pGroupList, idGroup);
      PropSheetChanged (hDlg);
      }

   if (IsDlgButtonChecked (hDlg, IDC_USER_SHOW_MEMBER))
      lpp->pGroupsMember = pGroupList;
   else // (!IsDlgButtonChecked (hDlg, IDC_USER_SHOW_OWNER))
      lpp->pGroupsOwner = pGroupList;

   FastList_End (hList);
}


void UserProp_Member_OnApply (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   // Free the old backup ASIDLIST copies we attached to the dialog during
   // WM_INITDIALOG processing.
   //
   LPASIDLIST pList;
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_MEMBER)) != NULL)
      asc_AsidListFree (&pList);
   if ((pList = (LPASIDLIST)GetWindowData (hDlg, GWD_ASIDLIST_OWNER)) != NULL)
      asc_AsidListFree (&pList);
   SetWindowData (hDlg, GWD_ASIDLIST_MEMBER, 0);
   SetWindowData (hDlg, GWD_ASIDLIST_OWNER, 0);

   if (lpp->pUserList)
      {
      // Start a background task to update the membership and ownership lists
      //
      if (lpp->pGroupsMember)
         {
         LPUSER_GROUPLIST_SET_PARAMS pTask = New (USER_GROUPLIST_SET_PARAMS);
         memset (pTask, 0x00, sizeof(USER_GROUPLIST_SET_PARAMS));
         asc_AsidListCopy (&pTask->pUsers, &lpp->pUserList);
         asc_AsidListCopy (&pTask->pGroups, &lpp->pGroupsMember);
         pTask->fMembership = TRUE;
         StartTask (taskUSER_GROUPLIST_SET, NULL, pTask);
         }
      if (lpp->pGroupsOwner)
         {
         LPUSER_GROUPLIST_SET_PARAMS pTask = New (USER_GROUPLIST_SET_PARAMS);
         memset (pTask, 0x00, sizeof(USER_GROUPLIST_SET_PARAMS));
         asc_AsidListCopy (&pTask->pUsers, &lpp->pUserList);
         asc_AsidListCopy (&pTask->pGroups, &lpp->pGroupsOwner);
         pTask->fMembership = FALSE;
         StartTask (taskUSER_GROUPLIST_SET, NULL, pTask);
         }
      }
}


void UserProp_UpdateName (HWND hDlg)
{
   LPUSERPROPINFO lpp = (LPUSERPROPINFO)PropSheet_FindTabParam (hDlg);

   if (IsWindow (GetDlgItem (hDlg, IDC_USER_NAME)))
      {
      if (!lpp->pUserList || (lpp->pUserList->cEntries == 1))
         {
         TCHAR szText[ cchRESOURCE ];
         GetDlgItemText (hDlg, IDC_USER_NAME, szText, cchRESOURCE);

         ULONG status;
         TCHAR szName[ cchRESOURCE ];
         if (lpp->pUserList)
            User_GetDisplayName (szName, lpp->pUserList->aEntries[ 0 ].idObject);
         else
            asc_ObjectNameGet_Fast (g.idClient, g.idCell, g.idCell, szName, &status);

         if (lpp->pUserList)
            {
            ASOBJPROP Properties;
            if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pUserList->aEntries[ 0 ].idObject, &Properties, &status))
               AppendUID (szName, Properties.u.UserProperties.PTSINFO.uidName);
            }

         LPTSTR pszText = FormatString (szText, TEXT("%s"), szName);
         SetDlgItemText (hDlg, IDC_USER_NAME, pszText);
         FreeString (pszText);
         }
      else if (lpp->pUserList && (lpp->pUserList->cEntries > 1))
         {
         LPTSTR pszText = CreateNameList (lpp->pUserList, IDS_USER_NAME_MULTIPLE);
         SetDlgItemText (hDlg, IDC_USER_NAME, pszText);
         FreeString (pszText);
         }
      }
}

