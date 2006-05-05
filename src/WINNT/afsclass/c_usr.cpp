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

#include <WINNT/afsclass.h>
#include "internal.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define USERACCESS_TO_ACCOUNTACCESS(_ua) ( ((_ua) == PTS_USER_OWNER_ACCESS) ? aaOWNER_ONLY : aaANYONE )


/*
 * ROUTINES ___________________________________________________________________
 *
 */

USER::USER (LPCELL lpCellParent, LPTSTR pszPrincipal, LPTSTR pszInstance)
{
   m_lpiCell = lpCellParent->GetIdentifier();
   lstrcpy (m_szPrincipal, pszPrincipal);
   lstrcpy (m_szInstance, (pszInstance) ? pszInstance : TEXT(""));

   m_lpiThis = NULL;
   m_fStatusOutOfDate = TRUE;
   m_mszOwnerOf = NULL;
   m_mszMemberOf = NULL;
   memset(&m_us, 0, sizeof(m_us));
}


USER::~USER (void)
{
   if (m_lpiThis)
      m_lpiThis->m_cRef --;

   FreeString (m_mszOwnerOf);
   FreeString (m_mszMemberOf);
}


void USER::SendDeleteNotifications (void)
{
   NOTIFYCALLBACK::SendNotificationToAll (evtDestroy, GetIdentifier());
}


void USER::Close (void)
{
   AfsClass_Leave();
}


LPIDENT USER::GetIdentifier (void)
{
   if (m_lpiThis == NULL)
      {
      if ((m_lpiThis = IDENT::FindIdent (this)) == NULL)
         m_lpiThis = New2 (IDENT,(this));
      m_lpiThis->m_cRef ++;
      }

   return m_lpiThis;
}


void USER::Invalidate (void)
{
   m_fStatusOutOfDate = TRUE;
}


LPCELL USER::OpenCell (ULONG *pStatus)
{
   return m_lpiCell->OpenCell (pStatus);
}


void USER::GetName (LPTSTR pszPrincipal, LPTSTR pszInstance)
{
   if (pszPrincipal)
      lstrcpy (pszPrincipal, m_szPrincipal);
   if (pszInstance)
      lstrcpy (pszInstance, m_szInstance);
}


BOOL USER::GetStatus (LPUSERSTATUS lpus, BOOL fNotify, ULONG *pStatus)
{
   if (!RefreshStatus (fNotify, pStatus))
      return FALSE;

   memcpy (lpus, &m_us, sizeof(USERSTATUS));
   return TRUE;
}


PVOID USER::GetUserParam (void)
{
   return GetIdentifier()->GetUserParam();
}


void USER::SetUserParam (PVOID pUserParam)
{
   GetIdentifier()->SetUserParam (pUserParam);
}


BOOL USER::GetOwnerOf (LPTSTR *ppmsz, ULONG *pStatus)
{
   if (!RefreshStatus (TRUE, pStatus))
      return FALSE;
   *ppmsz = CloneMultiString (m_mszOwnerOf);
   return TRUE;
}


BOOL USER::GetMemberOf (LPTSTR *ppmsz, ULONG *pStatus)
{
   if (!RefreshStatus (TRUE, pStatus))
      return FALSE;
   *ppmsz = CloneMultiString (m_mszMemberOf);
   return TRUE;
}


BOOL USER::RefreshStatus (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;
   DWORD kasStatus = 0;
   DWORD ptsStatus = 0;

   if (m_fStatusOutOfDate)
      {
      m_fStatusOutOfDate = FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      memset (&m_us, 0x00, sizeof(m_us));

      FreeString (m_mszOwnerOf);
      m_mszOwnerOf = NULL;

      FreeString (m_mszMemberOf);
      m_mszMemberOf = NULL;

      TCHAR szFullName[ cchNAME ];
      AfsClass_GenFullUserName (szFullName, m_szPrincipal, m_szInstance);

      LPCELL lpCell;
      if ((lpCell = OpenCell (&status)) == NULL)
         rc = FALSE;
      else
         {
         PVOID hCell;
         if ((hCell = lpCell->GetCellObject (&status)) == NULL)
            rc = FALSE;
         else
            {
            // Try to get KAS information.
            //
            WORKERPACKET wpGetKas;
            wpGetKas.wpKasPrincipalGet.hCell = hCell;
            wpGetKas.wpKasPrincipalGet.hServer = lpCell->GetKasObject (&kasStatus);
            wpGetKas.wpKasPrincipalGet.pszPrincipal = m_szPrincipal;
            wpGetKas.wpKasPrincipalGet.pszInstance = m_szInstance;

            if (Worker_DoTask (wtaskKasPrincipalGet, &wpGetKas, &kasStatus))
               {
               m_us.fHaveKasInfo = TRUE;

               TCHAR szLastModPrincipal[ cchNAME ];
               TCHAR szLastModInstance[ cchNAME ];
               CopyAnsiToString (szLastModPrincipal, wpGetKas.wpKasPrincipalGet.Data.lastModPrincipal.principal);
               CopyAnsiToString (szLastModInstance, wpGetKas.wpKasPrincipalGet.Data.lastModPrincipal.instance);

               m_us.KASINFO.fIsAdmin = (wpGetKas.wpKasPrincipalGet.Data.adminSetting == KAS_ADMIN) ? TRUE : FALSE;
               m_us.KASINFO.fCanGetTickets = (wpGetKas.wpKasPrincipalGet.Data.tgsSetting == TGS) ? TRUE : FALSE;
               m_us.KASINFO.fEncrypt = (wpGetKas.wpKasPrincipalGet.Data.encSetting == ENCRYPT) ? TRUE : FALSE;
               m_us.KASINFO.fCanChangePassword = (wpGetKas.wpKasPrincipalGet.Data.cpwSetting == CHANGE_PASSWORD) ? TRUE : FALSE;
               m_us.KASINFO.fCanReusePasswords = (wpGetKas.wpKasPrincipalGet.Data.rpwSetting == REUSE_PASSWORD) ? TRUE : FALSE;
               AfsClass_UnixTimeToSystemTime (&m_us.KASINFO.timeExpires, wpGetKas.wpKasPrincipalGet.Data.userExpiration);
               AfsClass_UnixTimeToSystemTime (&m_us.KASINFO.timeLastPwChange, wpGetKas.wpKasPrincipalGet.Data.lastChangePasswordTime);
               AfsClass_UnixTimeToSystemTime (&m_us.KASINFO.timeLastMod, wpGetKas.wpKasPrincipalGet.Data.lastModTime);
               m_us.KASINFO.lpiLastMod = IDENT::FindUser (m_lpiCell, szLastModPrincipal, szLastModInstance);
               m_us.KASINFO.csecTicketLifetime = wpGetKas.wpKasPrincipalGet.Data.maxTicketLifetime;
               m_us.KASINFO.keyVersion = wpGetKas.wpKasPrincipalGet.Data.keyVersion;
               memcpy (&m_us.KASINFO.key.key, &wpGetKas.wpKasPrincipalGet.Data.key.key, ENCRYPTIONKEY_LEN);
               m_us.KASINFO.dwKeyChecksum = wpGetKas.wpKasPrincipalGet.Data.keyCheckSum;
               m_us.KASINFO.cdayPwExpire = wpGetKas.wpKasPrincipalGet.Data.daysToPasswordExpire;
               m_us.KASINFO.cFailLogin = wpGetKas.wpKasPrincipalGet.Data.failLoginCount;
               m_us.KASINFO.csecFailLoginLock = wpGetKas.wpKasPrincipalGet.Data.lockTime;
               }

            // Try to get PTS information.
            //
            WORKERPACKET wpGetPts;
            wpGetPts.wpPtsUserGet.hCell = hCell;
            wpGetPts.wpPtsUserGet.pszUser = szFullName;
            if (Worker_DoTask (wtaskPtsUserGet, &wpGetPts, &ptsStatus))
               {
               m_us.fHavePtsInfo = TRUE;

               m_us.PTSINFO.cgroupCreationQuota = wpGetPts.wpPtsUserGet.Entry.groupCreationQuota;
               m_us.PTSINFO.cgroupMember = wpGetPts.wpPtsUserGet.Entry.groupMembershipCount;
               m_us.PTSINFO.uidName = wpGetPts.wpPtsUserGet.Entry.nameUid;
               m_us.PTSINFO.uidOwner = wpGetPts.wpPtsUserGet.Entry.ownerUid;
               m_us.PTSINFO.uidCreator = wpGetPts.wpPtsUserGet.Entry.creatorUid;

               CopyAnsiToString (m_us.PTSINFO.szOwner, wpGetPts.wpPtsUserGet.Entry.owner);
               CopyAnsiToString (m_us.PTSINFO.szCreator, wpGetPts.wpPtsUserGet.Entry.creator);

               m_us.PTSINFO.aaListStatus = USERACCESS_TO_ACCOUNTACCESS (wpGetPts.wpPtsUserGet.Entry.listStatus);
               m_us.PTSINFO.aaGroupsOwned = USERACCESS_TO_ACCOUNTACCESS (wpGetPts.wpPtsUserGet.Entry.listGroupsOwned);
               m_us.PTSINFO.aaMembership = USERACCESS_TO_ACCOUNTACCESS (wpGetPts.wpPtsUserGet.Entry.listMembership);
               }

            // Grab the list of groups to which this user belongs
            //
            WORKERPACKET wpBegin;
            wpBegin.wpPtsUserMemberListBegin.hCell = hCell;
            wpBegin.wpPtsUserMemberListBegin.pszUser = szFullName;
            if (Worker_DoTask (wtaskPtsUserMemberListBegin, &wpBegin, &status))
               {
               for (;;)
                  {
                  TCHAR szGroup[ cchNAME ];

                  WORKERPACKET wpNext;
                  wpNext.wpPtsUserMemberListNext.hEnum = wpBegin.wpPtsUserMemberListBegin.hEnum;
                  wpNext.wpPtsUserMemberListNext.pszGroup = szGroup;
                  if (!Worker_DoTask (wtaskPtsUserMemberListNext, &wpNext))
                     break;

                  FormatMultiString (&m_mszMemberOf, FALSE, TEXT("%1"), TEXT("%s"), szGroup);
                  }

               WORKERPACKET wpDone;
               wpDone.wpPtsUserMemberListDone.hEnum = wpBegin.wpPtsUserMemberListBegin.hEnum;
               Worker_DoTask (wtaskPtsUserMemberListDone, &wpDone);
               }

            // Grab the list of groups which this user owns
            //
            wpBegin.wpPtsOwnedGroupListBegin.hCell = hCell;
            wpBegin.wpPtsOwnedGroupListBegin.pszOwner = szFullName;
            if (Worker_DoTask (wtaskPtsOwnedGroupListBegin, &wpBegin, &status))
               {
               for (;;)
                  {
                  TCHAR szGroup[ cchNAME ];

                  WORKERPACKET wpNext;
                  wpNext.wpPtsOwnedGroupListNext.hEnum = wpBegin.wpPtsOwnedGroupListBegin.hEnum;
                  wpNext.wpPtsOwnedGroupListNext.pszGroup = szGroup;
                  if (!Worker_DoTask (wtaskPtsOwnedGroupListNext, &wpNext))
                     break;

                  FormatMultiString (&m_mszOwnerOf, FALSE, TEXT("%1"), TEXT("%s"), szGroup);
                  }

               WORKERPACKET wpDone;
               wpDone.wpPtsOwnedGroupListDone.hEnum = wpBegin.wpPtsOwnedGroupListBegin.hEnum;
               Worker_DoTask (wtaskPtsOwnedGroupListDone, &wpDone);
               }
            }

         lpCell->Close();
         }

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (rc && (!m_us.fHaveKasInfo) && (!status) && kasStatus)
      {
      status = kasStatus;
      rc = FALSE;
      }
   if (rc && (!m_us.fHavePtsInfo) && (!status) && ptsStatus)
      {
      status = ptsStatus;
      // not fatal; rc remains TRUE
      }
   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}


void USER::SplitUserName (LPCTSTR pszFull, LPTSTR pszName, LPTSTR pszInstance)
{
   if (pszName)
      lstrcpy (pszName, pszFull);
   if (pszInstance)
      lstrcpy (pszInstance, TEXT(""));

   if (!USER::IsMachineAccount (pszFull))
      {
      if (pszName && pszInstance)
         {
         LPTSTR pchDot;
         if ((pchDot = (LPTSTR)lstrchr (pszName, TEXT('.'))) != NULL)
            {
            *pchDot = TEXT('\0');
            lstrcpy (pszInstance, &pchDot[1]);
            }
         }
      }
}


BOOL USER::IsMachineAccount (LPCTSTR pszName)
{
   for ( ; pszName && *pszName; ++pszName)
      {
      if (!( (*pszName == TEXT('.')) || ((*pszName >= TEXT('0')) && (*pszName <= TEXT('9'))) ))
         return FALSE;
      }
   return TRUE;
}

