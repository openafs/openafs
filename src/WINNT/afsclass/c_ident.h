/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_IDENT_H
#define AFSCLASS_IDENT_H

#include <WINNT/afsclass.h>


/*
 * IDENTIFIER CLASS ___________________________________________________________
 *
 */

typedef enum
   {
   itUNUSED,
   itCELL,
   itSERVER,
   itSERVICE,
   itAGGREGATE,
   itFILESET,
   itUSER,
   itGROUP
   } IDENTTYPE;

class IDENT
   {
   friend class CELL;
   friend class SERVER;
   friend class SERVICE;
   friend class AGGREGATE;
   friend class FILESET;
   friend class USER;
   friend class PTSGROUP;

   public:
      IDENTTYPE GetType (void);
      BOOL fIsCell (void);
      BOOL fIsServer (void);
      BOOL fIsService (void);
      BOOL fIsAggregate (void);
      BOOL fIsFileset (void);
      BOOL fIsUser (void);
      BOOL fIsGroup (void);
      size_t GetRefCount (void);

      LPCELL OpenCell (ULONG *pStatus = NULL);
      LPSERVER OpenServer (ULONG *pStatus = NULL);
      LPSERVICE OpenService (ULONG *pStatus = NULL);
      LPAGGREGATE OpenAggregate (ULONG *pStatus = NULL);
      LPFILESET OpenFileset (ULONG *pStatus = NULL);
      LPUSER OpenUser (ULONG *pStatus = NULL);
      LPPTSGROUP OpenGroup (ULONG *pStatus = NULL);

      LPIDENT GetCell (void);
      LPIDENT GetServer (void);
      LPIDENT GetService (void);
      LPIDENT GetAggregate (void);
      LPIDENT GetFileset (void);
      LPIDENT GetUser (void);
      LPIDENT GetGroup (void);

      void GetCellName (LPTSTR pszName);
      void GetLongServerName (LPTSTR pszName);
      void GetShortServerName (LPTSTR pszName);
      void GetServerName (LPTSTR pszName);
      void GetServiceName (LPTSTR pszName);
      void GetAggregateName (LPTSTR pszName);
      void GetFilesetName (LPTSTR pszName);
      void GetFilesetID (VOLUMEID *pvid);
      void GetUserName (LPTSTR pszName, LPTSTR pszInstance = NULL);
      void GetGroupName (LPTSTR pszPrincipal);
      void GetFullUserName (LPTSTR pszFullName);

      PVOID GetUserParam (void);
      void SetUserParam (PVOID pUserParam);

      static LPIDENT FindFirst (HENUM *phEnum);
      static LPIDENT FindFirst (HENUM *phEnum, VOLUMEID *pvidFileset);
      static LPIDENT FindNext (HENUM *phEnum);
      static void FindClose (HENUM *phEnum);

      static LPIDENT FindServer (LPIDENT lpiCell, LPTSTR pszServer);
      static LPIDENT FindAggregate (LPIDENT lpiServer, LPTSTR pszAggregate);
      static LPIDENT FindFileset (LPIDENT lpiParent, LPTSTR pszFileset);
      static LPIDENT FindFileset (LPIDENT lpiParent, VOLUMEID *pvidFileset);
      static LPIDENT FindUser (LPIDENT lpiCell, LPTSTR pszPrincipal, LPTSTR pszInstance = NULL);
      static LPIDENT FindGroup (LPIDENT lpiCell, LPTSTR pszGroup);

      // Private functions
      //
   private:
      void InitIdent (void);
      IDENT (LPCELL lpCell);
      IDENT (LPSERVER lpServer);
      IDENT (LPSERVICE lpSvc);
      IDENT (LPAGGREGATE lpAgg);
      IDENT (LPFILESET lpSet);
      IDENT (LPUSER lpUser);
      IDENT (LPPTSGROUP lpGroup);
      ~IDENT (void);
      void Update (void);

      static void InitClass (void);
      static void RemoveIdentsInCell (LPIDENT lpiCell);
      static LPIDENT FindIdent (LPCELL lpCell);
      static LPIDENT FindIdent (LPSERVER lpServer);
      static LPIDENT FindIdent (LPSERVICE lpService);
      static LPIDENT FindIdent (LPAGGREGATE lpAggregate);
      static LPIDENT FindIdent (LPFILESET lpFileset);
      static LPIDENT FindIdent (LPFILESET lpFileset, VOLUMEID *pvid);
      static LPIDENT FindIdent (LPUSER lpUser);
      static LPIDENT FindIdent (LPPTSGROUP lpGroup);
      static LPIDENT FindIdent (IDENTTYPE iType, LPTSTR pszCell, LPTSTR pszServer, LPTSTR pszService, LPTSTR pszAggregate, LPTSTR pszFileset, VOLUMEID *pvidFileset);
      static LPIDENT FindIdent (IDENTTYPE iType, LPTSTR pszCell, LPTSTR pszAccount, LPTSTR pszInstance);

      // Private data
      //
   private:
      IDENTTYPE m_iType;

      TCHAR m_szCell[ cchNAME ];
      TCHAR m_szServer[ cchNAME ];
      TCHAR m_szService[ cchNAME ];
      TCHAR m_szAggregate[ cchNAME ];
      TCHAR m_szFileset[ cchNAME ];
      TCHAR m_szAccount[ cchNAME ];
      TCHAR m_szInstance[ cchNAME ];
      VOLUMEID m_vidFileset;
      PVOID m_pUser;
      size_t m_cRef;

      static LPHASHLIST x_lIdents;
      static LPHASHLISTKEY x_lkTypeServer;
      static LPHASHLISTKEY x_lkFilesetID;
      static LPHASHLISTKEY x_lkFilesetName;
      static LPHASHLISTKEY x_lkAccountName;

      static BOOL CALLBACK IDENT::KeyTypeServer_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK IDENT::KeyTypeServer_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK IDENT::KeyTypeServer_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK IDENT::KeyFilesetID_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK IDENT::KeyFilesetID_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK IDENT::KeyFilesetID_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK IDENT::KeyFilesetName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK IDENT::KeyFilesetName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK IDENT::KeyFilesetName_HashData (LPHASHLISTKEY pKey, PVOID pData);

      static BOOL CALLBACK IDENT::KeyAccountName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
      static HASHVALUE CALLBACK IDENT::KeyAccountName_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
      static HASHVALUE CALLBACK IDENT::KeyAccountName_HashData (LPHASHLISTKEY pKey, PVOID pData);
   };


#endif  // AFSCLASS_IDENT_H

