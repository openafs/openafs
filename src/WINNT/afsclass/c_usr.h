/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_USER_H
#define AFSCLASS_USER_H

#include <WINNT/afsclass.h>
#include <WINNT/c_svc.h>


/*
 * USER CLASS _________________________________________________________________
 *
 */

typedef struct USERSTATUS
   {
   BOOL fHaveKasInfo;
   BOOL fHavePtsInfo;

   struct
      {
      BOOL fIsAdmin;
      BOOL fCanGetTickets;
      BOOL fEncrypt;
      BOOL fCanChangePassword;
      BOOL fCanReusePasswords;
      SYSTEMTIME timeExpires;
      SYSTEMTIME timeLastPwChange;
      SYSTEMTIME timeLastMod;
      LPIDENT lpiLastMod;
      LONG csecTicketLifetime;
      int keyVersion;
      ENCRYPTIONKEY key;
      DWORD dwKeyChecksum;
      LONG cdayPwExpire;
      LONG cFailLogin;
      LONG csecFailLoginLock;
      } KASINFO;

   struct
      {
      LONG cgroupCreationQuota;
      LONG cgroupMember;
      int uidName;
      int uidOwner;
      int uidCreator;
      TCHAR szOwner[ cchNAME ];
      TCHAR szCreator[ cchNAME ];
      ACCOUNTACCESS aaListStatus;
      ACCOUNTACCESS aaGroupsOwned;
      ACCOUNTACCESS aaMembership;
      } PTSINFO;

   } USERSTATUS, *LPUSERSTATUS;


class USER
   {
   friend class CELL;
   friend class IDENT;
   friend class PTSGROUP;

   public:
      void Close (void);
      void Invalidate (void);
      BOOL RefreshStatus (BOOL fNotify = TRUE, ULONG *pStatus = NULL);

      // User properties
      //
      LPIDENT GetIdentifier (void);
      LPCELL OpenCell (ULONG *pStatus = NULL);
      void GetName (LPTSTR pszPrincipal, LPTSTR pszInstance = NULL);

      BOOL GetStatus (LPUSERSTATUS lpus, BOOL fNotify = TRUE, ULONG *pStatus = NULL);

      PVOID GetUserParam (void);
      void SetUserParam (PVOID pUserParam);

      // Groups
      //
      BOOL GetOwnerOf (LPTSTR *ppmsz, ULONG *pStatus = NULL);
      BOOL GetMemberOf (LPTSTR *ppmsz, ULONG *pStatus = NULL);

      static void SplitUserName (LPCTSTR pszFull, LPTSTR pszName, LPTSTR pszInstance);
      static BOOL IsMachineAccount (LPCTSTR pszName);

   private:
      USER (LPCELL lpCellParent, LPTSTR pszPrincipal, LPTSTR pszInstance);
      ~USER (void);
      void SendDeleteNotifications (void);

   private:
      LPIDENT m_lpiCell;
      TCHAR m_szPrincipal[ cchNAME ];
      TCHAR m_szInstance[ cchNAME ];

      LPIDENT m_lpiThis;

      BOOL m_fStatusOutOfDate;
      USERSTATUS m_us;

      LPTSTR m_mszOwnerOf;
      LPTSTR m_mszMemberOf;
   };


#endif  // AFSCLASS_USER_H

