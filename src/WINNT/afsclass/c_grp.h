/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_GROUP_H
#define AFSCLASS_GROUP_H

#include <WINNT/afsclass.h>


/*
 * PTSGROUP CLASS _____________________________________________________________
 *
 */

typedef struct PTSGROUPSTATUS
   {
   int nMembers;
   int uidName;
   int uidOwner;
   int uidCreator;
   ACCOUNTACCESS aaListStatus;
   ACCOUNTACCESS aaListGroupsOwned;
   ACCOUNTACCESS aaListMembers;
   ACCOUNTACCESS aaAddMember;
   ACCOUNTACCESS aaDeleteMember;
   TCHAR szOwner[ cchNAME ];
   TCHAR szCreator[ cchNAME ];
   } PTSGROUPSTATUS, *LPPTSGROUPSTATUS;


class PTSGROUP
   {
   friend class CELL;
   friend class IDENT;
   friend class USER;

   public:
      void Close (void);
      void Invalidate (void);
      BOOL RefreshStatus (BOOL fNotify = TRUE, ULONG *pStatus = NULL);

      // User properties
      //
      LPIDENT GetIdentifier (void);
      LPCELL OpenCell (ULONG *pStatus = NULL);
      void GetName (LPTSTR pszGroup);

      BOOL GetStatus (LPPTSGROUPSTATUS lpgs, BOOL fNotify = TRUE, ULONG *pStatus = NULL);

      PVOID GetUserParam (void);
      void SetUserParam (PVOID pUserParam);

      // Users and Groups
      //
      BOOL GetMembers (LPTSTR *ppmsz, ULONG *pStatus = NULL);
      BOOL GetMemberOf (LPTSTR *ppmsz, ULONG *pStatus = NULL);
      BOOL GetOwnerOf (LPTSTR *ppmsz, ULONG *pStatus = NULL);

   private:
      PTSGROUP (LPCELL lpCellParent, LPTSTR pszGroup);
      ~PTSGROUP (void);
      void SendDeleteNotifications (void);

   private:
      LPIDENT m_lpiCell;
      TCHAR m_szName[ cchNAME ];

      LPIDENT m_lpiThis;

      BOOL m_fStatusOutOfDate;
      PTSGROUPSTATUS m_gs;

      LPTSTR m_mszMembers;
      LPTSTR m_mszMemberOf;
      LPTSTR m_mszOwnerOf;

   public:
      // (used internally)
      void PTSGROUP::ChangeIdentName (LPTSTR pszNewName);
   };


#endif  // AFSCLASS_GROUP_H

