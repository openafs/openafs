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

#define PTSGROUPACCESS_TO_ACCOUNTACCESS(_ga) \
   ( ((_ga) == PTS_GROUP_OWNER_ACCESS) ? aaOWNER_ONLY : \
     ((_ga) == PTS_GROUP_ACCESS) ? aaGROUP_ONLY : aaANYONE )


/*
 * ROUTINES ___________________________________________________________________
 *
 */

PTSGROUP::PTSGROUP (LPCELL lpCellParent, LPTSTR pszGroup)
{
   m_lpiCell = lpCellParent->GetIdentifier();
   lstrcpy (m_szName, pszGroup);

   m_lpiThis = NULL;
   m_fStatusOutOfDate = TRUE;
   m_mszMembers = NULL;
   m_mszMemberOf = NULL;
   m_mszOwnerOf = NULL;
   memset(&m_gs, 0, sizeof(m_gs));
}


PTSGROUP::~PTSGROUP (void)
{
   if (m_lpiThis)
      m_lpiThis->m_cRef --;

   FreeString (m_mszMembers);
   FreeString (m_mszMemberOf);
   FreeString (m_mszOwnerOf);
}


void PTSGROUP::SendDeleteNotifications (void)
{
   NOTIFYCALLBACK::SendNotificationToAll (evtDestroy, GetIdentifier());
}


void PTSGROUP::Close (void)
{
   AfsClass_Leave();
}


LPIDENT PTSGROUP::GetIdentifier (void)
{
   if (m_lpiThis == NULL)
      {
      if ((m_lpiThis = IDENT::FindIdent (this)) == NULL)
         m_lpiThis = New2 (IDENT,(this));
      m_lpiThis->m_cRef ++;
      }

   return m_lpiThis;
}


void PTSGROUP::Invalidate (void)
{
   m_fStatusOutOfDate = TRUE;
}


LPCELL PTSGROUP::OpenCell (ULONG *pStatus)
{
   return m_lpiCell->OpenCell (pStatus);
}


void PTSGROUP::GetName (LPTSTR pszGroup)
{
   if (pszGroup)
      lstrcpy (pszGroup, m_szName);
}


BOOL PTSGROUP::GetStatus (LPPTSGROUPSTATUS lpgs, BOOL fNotify, ULONG *pStatus)
{
   if (!RefreshStatus (fNotify, pStatus))
      return FALSE;

   memcpy (lpgs, &m_gs, sizeof(PTSGROUPSTATUS));
   return TRUE;
}


PVOID PTSGROUP::GetUserParam (void)
{
   return GetIdentifier()->GetUserParam();
}


void PTSGROUP::SetUserParam (PVOID pUserParam)
{
   GetIdentifier()->SetUserParam (pUserParam);
}


BOOL PTSGROUP::GetMembers (LPTSTR *ppmsz, ULONG *pStatus)
{
   if (!RefreshStatus (TRUE, pStatus))
      return FALSE;
   *ppmsz = CloneMultiString (m_mszMembers);
   return TRUE;
}


BOOL PTSGROUP::GetMemberOf (LPTSTR *ppmsz, ULONG *pStatus)
{
   if (!RefreshStatus (TRUE, pStatus))
      return FALSE;
   *ppmsz = CloneMultiString (m_mszMemberOf);
   return TRUE;
}


BOOL PTSGROUP::GetOwnerOf (LPTSTR *ppmsz, ULONG *pStatus)
{
   if (!RefreshStatus (TRUE, pStatus))
      return FALSE;
   *ppmsz = CloneMultiString (m_mszOwnerOf);
   return TRUE;
}


void PTSGROUP::ChangeIdentName (LPTSTR pszNewName)
{
   LPIDENT lpi;
   if ((lpi = GetIdentifier()) != NULL)
      {
      lstrcpy (lpi->m_szAccount, pszNewName);
      lpi->Update();
      }
   lstrcpy (m_szName, pszNewName);

   LPCELL lpCell;
   if ((lpCell = OpenCell()) != NULL)
      {
      lpCell->m_lGroups->Update(this);
      lpCell->Close();
      }
}


BOOL PTSGROUP::RefreshStatus (BOOL fNotify, ULONG *pStatus)
{
   BOOL rc = TRUE;
   DWORD status = 0;

   if (m_fStatusOutOfDate)
      {
      m_fStatusOutOfDate = FALSE;

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusBegin, GetIdentifier());

      memset (&m_gs, 0x00, sizeof(m_gs));

      FreeString (m_mszOwnerOf);
      m_mszOwnerOf = NULL;

      FreeString (m_mszMemberOf);
      m_mszMemberOf = NULL;

      FreeString (m_mszMembers);
      m_mszMembers = NULL;

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
            WORKERPACKET wpGet;
            wpGet.wpPtsGroupGet.hCell = hCell;
            wpGet.wpPtsGroupGet.pszGroup = m_szName;
            if (!Worker_DoTask (wtaskPtsGroupGet, &wpGet, &status))
               rc = FALSE;
            else
               {
               m_gs.nMembers = wpGet.wpPtsGroupGet.Entry.membershipCount;
               m_gs.uidName = wpGet.wpPtsGroupGet.Entry.nameUid;
               m_gs.uidOwner = wpGet.wpPtsGroupGet.Entry.ownerUid;
               m_gs.uidCreator = wpGet.wpPtsGroupGet.Entry.creatorUid;

               m_gs.aaListStatus = PTSGROUPACCESS_TO_ACCOUNTACCESS (wpGet.wpPtsGroupGet.Entry.listStatus);
               m_gs.aaListGroupsOwned = PTSGROUPACCESS_TO_ACCOUNTACCESS (wpGet.wpPtsGroupGet.Entry.listGroupsOwned);
               m_gs.aaListMembers = PTSGROUPACCESS_TO_ACCOUNTACCESS (wpGet.wpPtsGroupGet.Entry.listMembership);
               m_gs.aaAddMember = PTSGROUPACCESS_TO_ACCOUNTACCESS (wpGet.wpPtsGroupGet.Entry.listAdd);
               m_gs.aaDeleteMember = PTSGROUPACCESS_TO_ACCOUNTACCESS (wpGet.wpPtsGroupGet.Entry.listDelete);
               CopyAnsiToString (m_gs.szOwner, wpGet.wpPtsGroupGet.Entry.owner);
               CopyAnsiToString (m_gs.szCreator, wpGet.wpPtsGroupGet.Entry.creator);
               }

            // Grab the list of users which belong to this group
            //
            WORKERPACKET wpBegin;
            wpBegin.wpPtsGroupMemberListBegin.hCell = hCell;
            wpBegin.wpPtsGroupMemberListBegin.pszGroup = m_szName;
            if (Worker_DoTask (wtaskPtsGroupMemberListBegin, &wpBegin, &status))
               {
               for (;;)
                  {
                  TCHAR szMember[ cchNAME ];

                  WORKERPACKET wpNext;
                  wpNext.wpPtsGroupMemberListNext.hEnum = wpBegin.wpPtsGroupMemberListBegin.hEnum;
                  wpNext.wpPtsGroupMemberListNext.pszMember = szMember;
                  if (!Worker_DoTask (wtaskPtsGroupMemberListNext, &wpNext))
                     break;

                  FormatMultiString (&m_mszMembers, FALSE, TEXT("%1"), TEXT("%s"), szMember);
                  }

               WORKERPACKET wpDone;
               wpDone.wpPtsGroupMemberListDone.hEnum = wpBegin.wpPtsGroupMemberListBegin.hEnum;
               Worker_DoTask (wtaskPtsGroupMemberListDone, &wpDone);
               }

            // Grab the list of groups which this group owns
            //
            wpBegin.wpPtsOwnedGroupListBegin.hCell = hCell;
            wpBegin.wpPtsOwnedGroupListBegin.pszOwner = m_szName;
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

            // Grab the list of groups to which this group belongs
            //
            wpBegin.wpPtsUserMemberListBegin.hCell = hCell;
            wpBegin.wpPtsUserMemberListBegin.pszUser = m_szName;
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
            }

         lpCell->Close();
         }

      if (fNotify)
         NOTIFYCALLBACK::SendNotificationToAll (evtRefreshStatusEnd, GetIdentifier(), ((rc) ? 0 : status));
      }

   if (pStatus && !rc)
      *pStatus = status;
   return TRUE;
}

