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

#include <WINNT/afsclass.h>
#include "internal.h"


/*
 * VARIABLES __________________________________________________________________
 *
 */

LPHASHLIST IDENT::x_lIdents = NULL;
LPHASHLISTKEY IDENT::x_lkTypeServer = NULL;
LPHASHLISTKEY IDENT::x_lkFilesetID = NULL;
LPHASHLISTKEY IDENT::x_lkFilesetName = NULL;
LPHASHLISTKEY IDENT::x_lkAccountName = NULL;



/*
 * CONSTRUCTION AND LIST MANAGEMENT ___________________________________________
 *
 */

void IDENT::InitClass (void)
{
   if (x_lIdents == NULL)
      {
      x_lIdents = New (HASHLIST);
      x_lkTypeServer = x_lIdents->CreateKey ("Type and Server", IDENT::KeyTypeServer_Compare, IDENT::KeyTypeServer_HashObject, IDENT::KeyTypeServer_HashData);
      x_lkFilesetID = x_lIdents->CreateKey ("Fileset ID", IDENT::KeyFilesetID_Compare, IDENT::KeyFilesetID_HashObject, IDENT::KeyFilesetID_HashData);
      x_lkFilesetName = x_lIdents->CreateKey ("Fileset Name", IDENT::KeyFilesetName_Compare, IDENT::KeyFilesetName_HashObject, IDENT::KeyFilesetName_HashData);
      x_lkAccountName = x_lIdents->CreateKey ("Account Name", IDENT::KeyAccountName_Compare, IDENT::KeyAccountName_HashObject, IDENT::KeyAccountName_HashData);
      }
}

void IDENT::InitIdent (void)
{
   InitClass();
   m_iType = itUNUSED;
   m_szCell[0] = TEXT('\0');
   m_szServer[0] = TEXT('\0');
   m_szService[0] = TEXT('\0');
   m_szAggregate[0] = TEXT('\0');
   m_szFileset[0] = TEXT('\0');
   m_szAccount[0] = TEXT('\0');
   m_szInstance[0] = TEXT('\0');
   memset (&m_vidFileset, 0x00, sizeof(m_vidFileset));
   m_pUser = NULL;
   m_cRef = 0;
}

IDENT::~IDENT (void)
{
   x_lIdents->Remove (this);
   InitIdent();
}

void IDENT::RemoveIdentsInCell (LPIDENT lpiCell)
{
   InitClass();
   for (LPENUM pEnum = x_lIdents->FindFirst(); pEnum; pEnum = pEnum->FindNext())
      {
      LPIDENT lpi = (LPIDENT)( pEnum->GetObject() );

      if (!lstrcmpi (lpi->m_szCell, lpiCell->m_szCell))
         Delete (lpi);
      }
}

void IDENT::Update (void)
{
   x_lIdents->Update (this);
}


/*
 * ENUMERATION ________________________________________________________________
 *
 */

LPIDENT IDENT::FindFirst (HENUM *phEnum)
{
   InitClass();

   if ((*phEnum = x_lIdents->FindFirst()) == NULL)
      return NULL;

   return (LPIDENT)( (*phEnum)->GetObject() );
}


LPIDENT IDENT::FindFirst (HENUM *phEnum, VOLUMEID *pvidFileset)
{
   InitClass();

   if ((*phEnum = x_lkFilesetID->FindFirst(pvidFileset)) == NULL)
      return NULL;

   LPIDENT lpiFound;
   while ((lpiFound = (LPIDENT)((*phEnum)->GetObject())) != NULL)
      {
      if (lpiFound->m_iType == itFILESET)
         return lpiFound;

      if ((*phEnum = (*phEnum)->FindNext()) == NULL)
         break;
      }

   return NULL;
}

LPIDENT IDENT::FindNext (HENUM *phEnum)
{
   if (*phEnum == NULL)
      return NULL;

   for (;;)
      {
      if ((*phEnum = (*phEnum)->FindNext()) == NULL)
         break;

      LPIDENT lpiFound;
      if ((lpiFound = (LPIDENT)((*phEnum)->GetObject())) == NULL)
         break;

      if (lpiFound->m_iType == itFILESET)
         return lpiFound;
      }

   return NULL;
}

void IDENT::FindClose (HENUM *phEnum)
{
   if (*phEnum != NULL)
      {
      Delete (*phEnum);
      (*phEnum) = NULL;
      }
}


/*
 * CELL IDENT _________________________________________________________________
 *
 */

IDENT::IDENT (LPCELL lpCell)
{
   InitIdent();

   if (ASSERT( lpCell != NULL ))
      {
      m_iType = itCELL;
      lpCell->GetName (m_szCell);
      x_lIdents->Add (this);
      }
}

BOOL IDENT::fIsCell (void)
{
   return (m_iType == itCELL) ? TRUE : FALSE;
}

LPCELL IDENT::OpenCell (ULONG *pStatus)
{
   if (!ASSERT( m_iType != itUNUSED ))
      return NULL;

   return CELL::ReopenCell (m_szCell, pStatus);
}

LPIDENT IDENT::GetCell (void)
{
   if (!ASSERT( m_iType != itUNUSED ))
      return NULL;
   if (m_iType == itCELL)
      return this;

   return FindIdent (itCELL, m_szCell, NULL, NULL, NULL, NULL, NULL);
}

void IDENT::GetCellName (LPTSTR pszName)
{
   if (!ASSERT( m_iType != itUNUSED ))
      {
      *pszName = 0;
      return;
      }

   lstrcpy (pszName, m_szCell);
}


/*
 * USER IDENT _________________________________________________________________
 *
 */

IDENT::IDENT (LPUSER lpUser)
{
   InitIdent();

   if (ASSERT( lpUser != NULL ))
      {
      m_iType = itUSER;
      lstrcpy (m_szCell, lpUser->m_lpiCell->m_szCell);
      lpUser->GetName (m_szAccount, m_szInstance);
      x_lIdents->Add (this);
      }
}

BOOL IDENT::fIsUser (void)
{
   return (m_iType == itUSER) ? TRUE : FALSE;
}

LPUSER IDENT::OpenUser (ULONG *pStatus)
{
   LPUSER lpUser = NULL;

   if (!ASSERT( m_iType == itUSER ))
      return NULL;

   LPCELL lpCell;
   if ((lpCell = CELL::ReopenCell (m_szCell, pStatus)) != NULL)
      {
      lpUser = lpCell->OpenUser (m_szAccount, m_szInstance, pStatus);
      lpCell->Close();
      }

   return lpUser;
}

LPIDENT IDENT::GetUser (void)
{
   if (!ASSERT( m_iType == itUSER ))
      return NULL;
   return this;
}


void IDENT::GetUserName (LPTSTR pszName, LPTSTR pszInstance)
{
   if (!ASSERT( m_iType == itUSER ))
      {
      *pszName = 0;
      if (pszInstance)
         *pszInstance = 0;
      return;
      }

   lstrcpy (pszName, m_szAccount);
   if (pszInstance)
      lstrcpy (pszInstance, m_szInstance);
}


void IDENT::GetFullUserName (LPTSTR pszFullName)
{
   if (!ASSERT( m_iType == itUSER ))
      {
      *pszFullName = 0;
      return;
      }

   AfsClass_GenFullUserName (pszFullName, m_szAccount, m_szInstance);
}


/*
 * GROUP IDENT ________________________________________________________________
 *
 */

IDENT::IDENT (LPPTSGROUP lpGroup)
{
   InitIdent();

   if (ASSERT( lpGroup != NULL ))
      {
      m_iType = itGROUP;
      lstrcpy (m_szCell, lpGroup->m_lpiCell->m_szCell);
      lpGroup->GetName (m_szAccount);
      x_lIdents->Add (this);
      }
}

BOOL IDENT::fIsGroup (void)
{
   return (m_iType == itGROUP) ? TRUE : FALSE;
}

LPPTSGROUP IDENT::OpenGroup (ULONG *pStatus)
{
   LPPTSGROUP lpGroup = NULL;

   if (!ASSERT( m_iType == itGROUP ))
      return NULL;

   LPCELL lpCell;
   if ((lpCell = CELL::ReopenCell (m_szCell, pStatus)) != NULL)
      {
      lpGroup = lpCell->OpenGroup (m_szAccount, pStatus);
      lpCell->Close();
      }

   return lpGroup;
}

LPIDENT IDENT::GetGroup (void)
{
   if (!ASSERT( m_iType == itGROUP ))
      return NULL;
   return this;
}


void IDENT::GetGroupName (LPTSTR pszName)
{
   if (!ASSERT( m_iType == itGROUP ))
      {
      *pszName = 0;
      return;
      }

   lstrcpy (pszName, m_szAccount);
}


/*
 * SERVER IDENT _______________________________________________________________
 *
 */

IDENT::IDENT (LPSERVER lpServer)
{
   InitIdent();

   if (ASSERT( lpServer != NULL ))
      {
      m_iType = itSERVER;
      lstrcpy (m_szCell, lpServer->m_lpiCell->m_szCell);
      lpServer->GetLongName (m_szServer);
      x_lIdents->Add (this);
      }
}

BOOL IDENT::fIsServer (void)
{
   return (m_iType == itSERVER) ? TRUE : FALSE;
}

LPSERVER IDENT::OpenServer (ULONG *pStatus)
{
   LPSERVER lpServer = NULL;

   if (!ASSERT( m_iType != itUNUSED && m_iType != itCELL ))
      return NULL;

   LPCELL lpCell;
   if ((lpCell = CELL::ReopenCell (m_szCell, pStatus)) != NULL)
      {
      lpServer = lpCell->OpenServer (m_szServer, pStatus);
      lpCell->Close();
      }

   return lpServer;
}

LPIDENT IDENT::GetServer (void)
{
   if (!ASSERT( m_iType != itUNUSED && m_iType != itCELL ))
      return NULL;
   if (m_iType == itSERVER)
      return this;

   return FindIdent (itSERVER, m_szCell, m_szServer, NULL, NULL, NULL, NULL);
}

void IDENT::GetLongServerName (LPTSTR pszName)
{
   if (!ASSERT( m_iType != itUNUSED && m_iType != itCELL ))
      {
      *pszName = 0;
      return;
      }

   lstrcpy (pszName, m_szServer);
}

void IDENT::GetShortServerName (LPTSTR pszName)
{
   if (!ASSERT( m_iType != itUNUSED && m_iType != itCELL ))
      {
      *pszName = 0;
      return;
      }

   SERVER::ShortenName (pszName, m_szServer, TRUE);
}

void IDENT::GetServerName (LPTSTR pszName)
{
   if (!ASSERT( m_iType != itUNUSED && m_iType != itCELL ))
      {
      *pszName = 0;
      return;
      }

   SERVER::ShortenName (pszName, m_szServer);
}


/*
 * SERVICE IDENT ______________________________________________________________
 *
 */

IDENT::IDENT (LPSERVICE lpService)
{
   InitIdent();

   if (ASSERT( lpService != NULL ))
      {
      LPSERVER lpServer;
      if ((lpServer = lpService->OpenServer()) != NULL)
         {
         m_iType = itSERVICE;
         lstrcpy (m_szCell, lpServer->m_lpiCell->m_szCell);
         lpServer->GetLongName (m_szServer);
         lpService->GetName (m_szService);
         x_lIdents->Add (this);
         lpServer->Close();
         }
      }
}

BOOL IDENT::fIsService (void)
{
   return (m_iType == itSERVICE) ? TRUE : FALSE;
}

LPSERVICE IDENT::OpenService (ULONG *pStatus)
{
   LPSERVICE lpService = NULL;

   if (!ASSERT( fIsService() ))
      return NULL;

   LPCELL lpCell;
   if ((lpCell = CELL::ReopenCell (m_szCell, pStatus)) != NULL)
      {
      LPSERVER lpServer;
      if ((lpServer = lpCell->OpenServer (m_szServer, pStatus)) != NULL)
         {
         lpService = lpServer->OpenService (m_szService, pStatus);
         lpServer->Close();
         }
      lpCell->Close();
      }

   return lpService;
}

LPIDENT IDENT::GetService (void)
{
   if (!ASSERT( fIsService() ))
      return NULL;
   return this;
}

void IDENT::GetServiceName (LPTSTR pszName)
{
   if (!ASSERT( fIsService() ))
      {
      *pszName = 0;
      return;
      }

   lstrcpy (pszName, m_szService);
}


/*
 * AGGREGATE IDENT ____________________________________________________________
 *
 */

IDENT::IDENT (LPAGGREGATE lpAggregate)
{
   InitIdent();

   if (ASSERT( lpAggregate != NULL ))
      {
      LPSERVER lpServer;
      if ((lpServer = lpAggregate->OpenServer()) != NULL)
         {
         m_iType = itAGGREGATE;
         lstrcpy (m_szCell, lpServer->m_lpiCell->m_szCell);
         lpServer->GetLongName (m_szServer);
         lpAggregate->GetName (m_szAggregate);
         x_lIdents->Add (this);
         lpServer->Close();
         }
      }
}

BOOL IDENT::fIsAggregate (void)
{
   return (m_iType == itAGGREGATE) ? TRUE : FALSE;
}

LPAGGREGATE IDENT::OpenAggregate (ULONG *pStatus)
{
   LPAGGREGATE lpAggregate = NULL;

   if (!ASSERT( fIsAggregate() || fIsFileset() ))
      return NULL;

   LPCELL lpCell;
   if ((lpCell = CELL::ReopenCell (m_szCell, pStatus)) != NULL)
      {
      LPSERVER lpServer;
      if ((lpServer = lpCell->OpenServer (m_szServer, pStatus)) != NULL)
         {
         lpAggregate = lpServer->OpenAggregate (m_szAggregate, pStatus);
         lpServer->Close();
         }
      lpCell->Close();
      }

   return lpAggregate;
}

LPIDENT IDENT::GetAggregate (void)
{
   if (!ASSERT( fIsAggregate() || fIsFileset() ))
      return NULL;
   if (m_iType == itAGGREGATE)
      return this;

   return FindIdent (itAGGREGATE, m_szCell, m_szServer, NULL, m_szAggregate, NULL, NULL);
}

void IDENT::GetAggregateName (LPTSTR pszName)
{
   if (!ASSERT( fIsAggregate() || fIsFileset() ))
      {
      *pszName = 0;
      return;
      }

   lstrcpy (pszName, m_szAggregate);
}


/*
 * FILESET IDENT ______________________________________________________________
 *
 */

IDENT::IDENT (LPFILESET lpFileset)
{
   InitIdent();

   if (ASSERT( lpFileset != NULL ))
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpFileset->OpenAggregate()) != NULL)
         {
         LPSERVER lpServer;
         if ((lpServer = lpAggregate->OpenServer()) != NULL)
            {
            m_iType = itFILESET;
            lstrcpy (m_szCell, lpServer->m_lpiCell->m_szCell);
            lpServer->GetLongName (m_szServer);
            lpAggregate->GetName (m_szAggregate);
            lpFileset->GetName (m_szFileset);
            lpFileset->GetID (&m_vidFileset);
            x_lIdents->Add (this);
            lpServer->Close();
            }
         lpAggregate->Close();
         }
      }
}

BOOL IDENT::fIsFileset (void)
{
   return (m_iType == itFILESET) ? TRUE : FALSE;
}

LPFILESET IDENT::OpenFileset (ULONG *pStatus)
{
   LPFILESET lpFileset = NULL;

   if (!ASSERT( fIsFileset() ))
      return NULL;

   LPCELL lpCell;
   if ((lpCell = CELL::ReopenCell (m_szCell, pStatus)) != NULL)
      {
      LPSERVER lpServer;
      if ((lpServer = lpCell->OpenServer (m_szServer, pStatus)) != NULL)
         {
         LPAGGREGATE lpAggregate;
         if ((lpAggregate = lpServer->OpenAggregate (m_szAggregate, pStatus)) != NULL)
            {
            lpFileset = lpAggregate->OpenFileset (&m_vidFileset, pStatus);
            lpAggregate->Close();
            }
         lpServer->Close();
         }
      lpCell->Close();
      }

   return lpFileset;
}

LPIDENT IDENT::GetFileset (void)
{
   if (!ASSERT( fIsFileset() ))
      return NULL;
   return this;
}

void IDENT::GetFilesetName (LPTSTR pszName)
{
   if (!ASSERT( fIsFileset() ))
      {
      *pszName = 0;
      return;
      }

   lstrcpy (pszName, m_szFileset);
}

void IDENT::GetFilesetID (VOLUMEID *pvidFileset)
{
   if (!ASSERT( fIsFileset() ))
      {
      memset (pvidFileset, 0x00, sizeof(VOLUMEID));
      return;
      }

   memcpy (pvidFileset, &m_vidFileset, sizeof(VOLUMEID));
}


/*
 * NON- TYPE-SPECIFIC ROUTINES ________________________________________________
 *
 */

IDENTTYPE IDENT::GetType (void)
{
   return m_iType;
}

PVOID IDENT::GetUserParam (void)
{
   return m_pUser;
}

void IDENT::SetUserParam (PVOID pUser)
{
   m_pUser = pUser;
}

size_t IDENT::GetRefCount (void)
{
   return m_cRef;
}


/*
 * HASH KEYS __________________________________________________________________
 *
 * A list of all IDENT objects is maintained using a HashList, which has two
 * index keys: one on [type+servername], and another on [filesetid].
 *
 */

typedef struct
   {
   IDENTTYPE iType;
   LPCTSTR pszServer;
   } KeyTypeServer;

BOOL CALLBACK IDENT::KeyTypeServer_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   if (((LPIDENT)pObject)->m_iType != ((KeyTypeServer*)pData)->iType)
      return FALSE;
   if (lstrcmp (((LPIDENT)pObject)->m_szServer, ((KeyTypeServer*)pData)->pszServer))
      return FALSE;
   return TRUE;
}

HASHVALUE CALLBACK IDENT::KeyTypeServer_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   KeyTypeServer kts;
   kts.iType = ((LPIDENT)pObject)->m_iType;
   kts.pszServer = ((LPIDENT)pObject)->m_szServer;
   return IDENT::KeyTypeServer_HashData (pKey, &kts);
}

HASHVALUE CALLBACK IDENT::KeyTypeServer_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HashString( ((KeyTypeServer*)pData)->pszServer ) + ((KeyTypeServer*)pData)->iType);
}


BOOL CALLBACK IDENT::KeyFilesetID_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !memcmp (&((LPIDENT)pObject)->m_vidFileset, (VOLUMEID*)pData, sizeof(VOLUMEID));
}

HASHVALUE CALLBACK IDENT::KeyFilesetID_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return IDENT::KeyFilesetID_HashData (pKey, &((LPIDENT)pObject)->m_vidFileset);
}

HASHVALUE CALLBACK IDENT::KeyFilesetID_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HASHVALUE)(*((VOLUMEID*)pData));
}


BOOL CALLBACK IDENT::KeyFilesetName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !lstrcmpi (((LPIDENT)pObject)->m_szFileset, (LPTSTR)pData);
}

HASHVALUE CALLBACK IDENT::KeyFilesetName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return IDENT::KeyFilesetName_HashData (pKey, &((LPIDENT)pObject)->m_szFileset);
}

HASHVALUE CALLBACK IDENT::KeyFilesetName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return HashString ((LPTSTR)pData);
}


BOOL CALLBACK IDENT::KeyAccountName_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return !lstrcmpi (((LPIDENT)pObject)->m_szAccount, (LPTSTR)pData);
}

HASHVALUE CALLBACK IDENT::KeyAccountName_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return IDENT::KeyAccountName_HashData (pKey, &((LPIDENT)pObject)->m_szAccount);
}

HASHVALUE CALLBACK IDENT::KeyAccountName_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return HashString ((LPTSTR)pData);
}


/*
 * SEARCHING __________________________________________________________________
 *
 */

LPIDENT IDENT::FindIdent (LPCELL lpCell)
{
   TCHAR szCell[ cchNAME ];
   lpCell->GetName (szCell);
   return FindIdent (itCELL, szCell, NULL, NULL, NULL, NULL, NULL);
}

LPIDENT IDENT::FindIdent (LPUSER lpUser)
{
   TCHAR szCell[ cchNAME ];
   TCHAR szAccount[ cchNAME ];
   TCHAR szInstance[ cchNAME ];
   lpUser->m_lpiCell->GetCellName (szCell);
   lpUser->GetName (szAccount, szInstance);
   return FindIdent (itUSER, szCell, szAccount, szInstance);
}

LPIDENT IDENT::FindIdent (LPPTSGROUP lpGroup)
{
   TCHAR szCell[ cchNAME ];
   TCHAR szAccount[ cchNAME ];
   lpGroup->m_lpiCell->GetCellName (szCell);
   lpGroup->GetName (szAccount);
   return FindIdent (itGROUP, szCell, szAccount, NULL);
}

LPIDENT IDENT::FindIdent (LPSERVER lpServer)
{
   TCHAR szCell[ cchNAME ];
   TCHAR szServer[ cchNAME ];
   lpServer->m_lpiCell->GetCellName (szCell);
   lpServer->GetLongName (szServer);
   return FindIdent (itSERVER, szCell, szServer, NULL, NULL, NULL, NULL);
}

LPIDENT IDENT::FindIdent (LPSERVICE lpService)
{
   LPIDENT lpi = NULL;

   LPSERVER lpServer;
   if ((lpServer = lpService->OpenServer()) != NULL)
      {
      TCHAR szCell[ cchNAME ];
      TCHAR szServer[ cchNAME ];
      TCHAR szService[ cchNAME ];
      lpServer->m_lpiCell->GetCellName (szCell);
      lpServer->GetLongName (szServer);
      lpService->GetName (szService);

      lpi = FindIdent (itSERVICE, szCell, szServer, szService, NULL, NULL, NULL);

      lpServer->Close();
      }

   return lpi;
}

LPIDENT IDENT::FindIdent (LPAGGREGATE lpAggregate)
{
   LPIDENT lpi = NULL;

   LPSERVER lpServer;
   if ((lpServer = lpAggregate->OpenServer()) != NULL)
      {
      TCHAR szCell[ cchNAME ];
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      lpServer->m_lpiCell->GetCellName (szCell);
      lpServer->GetLongName (szServer);
      lpAggregate->GetName (szAggregate);

      lpi = FindIdent (itAGGREGATE, szCell, szServer, NULL, szAggregate, NULL, NULL);

      lpServer->Close();
      }

   return lpi;
}

LPIDENT IDENT::FindIdent (LPFILESET lpFileset)
{
   LPIDENT lpi = NULL;

   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpFileset->OpenAggregate()) != NULL)
      {
      LPSERVER lpServer;
      if ((lpServer = lpAggregate->OpenServer()) != NULL)
         {
         TCHAR szCell[ cchNAME ];
         TCHAR szServer[ cchNAME ];
         TCHAR szAggregate[ cchNAME ];
         VOLUMEID vidFileset;
         lpServer->m_lpiCell->GetCellName (szCell);
         lpServer->GetLongName (szServer);
         lpAggregate->GetName (szAggregate);
         lpFileset->GetID (&vidFileset);

         lpi = FindIdent (itFILESET, szCell, szServer, NULL, szAggregate, NULL, &vidFileset);

         lpServer->Close();
         }

      lpAggregate->Close();
      }

   return lpi;
}

LPIDENT IDENT::FindIdent (LPFILESET lpFileset, VOLUMEID *pvid)
{
   TCHAR szCell[ cchNAME ];
   lpFileset->m_lpiCell->GetCellName (szCell);

   // since all read-only replicas of a fileset share the same ID,
   // you can't use this routine to search for one.  BUT, you can
   // use it to look up a read-write or backup regardless of the
   // aggregate on which it sits, because there should be only one.
   // When FindIdent() realizes we didn't supply an aggregate to
   // match against, it will just skip comparing aggregates and
   // return the first IDENT it finds which matches the given ID.
   //
   return FindIdent (itFILESET, szCell, NULL, NULL, NULL, NULL, pvid);
}


LPIDENT IDENT::FindServer (LPIDENT lpiCell, LPTSTR pszServer)
{
   TCHAR szCell[ cchNAME ];
   lpiCell->GetCellName (szCell);

   return FindIdent (itSERVER, szCell, pszServer, NULL, NULL, NULL, NULL);
}


LPIDENT IDENT::FindAggregate (LPIDENT lpiServer, LPTSTR pszAggregate)
{
   TCHAR szCell[ cchNAME ];
   TCHAR szServer[ cchNAME ];
   lpiServer->GetCellName (szCell);
   lpiServer->GetLongServerName (szServer);

   return FindIdent (itAGGREGATE, szCell, szServer, NULL, pszAggregate, NULL, NULL);
}


LPIDENT IDENT::FindFileset (LPIDENT lpiParent, LPTSTR pszFileset)
{
   TCHAR szCell[ cchNAME ];
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];

   LPTSTR pszServer = NULL;
   LPTSTR pszAggregate = NULL;

   lpiParent->GetCellName (szCell);

   if (!lpiParent->fIsCell())
      {
      pszServer = szServer;
      lpiParent->GetLongServerName (szServer);
      }

   if (lpiParent->fIsAggregate())
      {
      pszAggregate = szAggregate;
      lpiParent->GetAggregateName (szAggregate);
      }

   return FindIdent (itFILESET, szCell, pszServer, NULL, pszAggregate, pszFileset, NULL);
}


LPIDENT IDENT::FindFileset (LPIDENT lpiParent, VOLUMEID *pvidFileset)
{
   TCHAR szCell[ cchNAME ];
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];

   LPTSTR pszServer = NULL;
   LPTSTR pszAggregate = NULL;

   lpiParent->GetCellName (szCell);

   if (!lpiParent->fIsCell())
      {
      pszServer = szServer;
      lpiParent->GetLongServerName (szServer);
      }

   if (lpiParent->fIsAggregate())
      {
      pszAggregate = szAggregate;
      lpiParent->GetAggregateName (szAggregate);
      }

   return FindIdent (itFILESET, szCell, pszServer, NULL, pszAggregate, NULL, pvidFileset);
}


LPIDENT IDENT::FindUser (LPIDENT lpiCell, LPTSTR pszPrincipal, LPTSTR pszInstance)
{
   return FindIdent (itUSER, lpiCell->m_szCell, pszPrincipal, pszInstance);
}


LPIDENT IDENT::FindGroup (LPIDENT lpiCell, LPTSTR pszGroup)
{
   return FindIdent (itGROUP, lpiCell->m_szCell, pszGroup, NULL);
}


LPIDENT IDENT::FindIdent (IDENTTYPE iType, LPTSTR pszCell, LPTSTR pszAccount, LPTSTR pszInstance)
{
   for (LPENUM pEnum = x_lkAccountName->FindFirst (pszAccount); pEnum; pEnum = pEnum->FindNext())
      {
      LPIDENT lpiFound = (LPIDENT)( pEnum->GetObject() );
      if (lpiFound->m_iType != iType)
         continue;
      if (lstrcmpi (pszCell, lpiFound->m_szCell))
         continue;
      if (pszInstance && lstrcmpi (pszInstance, lpiFound->m_szInstance))
         continue;
      Delete (pEnum);
      return lpiFound;
      }
   return NULL;
}

LPIDENT IDENT::FindIdent (IDENTTYPE iType, LPTSTR pszCell, LPTSTR pszServer, LPTSTR pszService, LPTSTR pszAggregate, LPTSTR pszFileset, VOLUMEID *pvidFileset)
{
   InitClass();

   // Since all IDENTs are maintained in a hashlist, we have several
   // possible techniques for finding any given IDENT. For this particular
   // hashlist, we have two keys: by type and server, and by volume ID.
   // If possible we want to use the latter; otherwise, use the former.
   //
   if ((iType == itFILESET) && (pvidFileset))
      {
      for (LPENUM pEnum = x_lkFilesetID->FindFirst (pvidFileset); pEnum; pEnum = pEnum->FindNext())
         {
         // Only volumes which match this fileset ID will get here.
         //
         LPIDENT lpi = (LPIDENT)( pEnum->GetObject() );

         if (lpi->m_iType != itFILESET)
            continue;
         if (pszCell && lstrcmpi (lpi->m_szCell, pszCell))
            continue;
         if (pszServer && lstrcmpi (lpi->m_szServer, pszServer))
            continue;
         if (pszAggregate && lstrcmpi (lpi->m_szAggregate, pszAggregate))
            continue;

         Delete (pEnum);
         return lpi;
         }
      }
   else if ((!pszServer) && (iType != itCELL))
      {
      LPTSTR pszFilesetSearch = (pszFileset) ? pszFileset : TEXT("");

      for (LPENUM pEnum = x_lkFilesetName->FindFirst (pszFilesetSearch); pEnum; pEnum = pEnum->FindNext())
         {
         // Only idents which match this fileset name will get here.
         //
         LPIDENT lpi = (LPIDENT)( pEnum->GetObject() );

         if (iType != lpi->m_iType)
            continue;
         if (pszCell && lstrcmpi (lpi->m_szCell, pszCell))
            continue;
         if ((iType == itSERVICE) && lstrcmpi (lpi->m_szService, pszService))
            continue;
         if ((iType == itAGGREGATE) && pszAggregate && lstrcmpi (lpi->m_szAggregate, pszAggregate))
            continue;

         Delete (pEnum);
         return lpi;
         }
      }
   else // Look up the IDENT based on its type and (optional) server name
      {
      KeyTypeServer kts;
      kts.iType = iType;
      kts.pszServer = (pszServer) ? pszServer : TEXT("");

      for (LPENUM pEnum = x_lkTypeServer->FindFirst (&kts); pEnum; pEnum = pEnum->FindNext())
         {
         // Only idents which match this type and server name will get here.
         //
         LPIDENT lpi = (LPIDENT)( pEnum->GetObject() );

         if (pszCell && lstrcmpi (lpi->m_szCell, pszCell))
            continue;
         if ((iType == itSERVICE) && lstrcmpi (lpi->m_szService, pszService))
            continue;
         if ((iType == itAGGREGATE) && pszAggregate && lstrcmpi (lpi->m_szAggregate, pszAggregate))
            continue;
         if ((iType == itFILESET) && pszFileset && lstrcmpi (lpi->m_szFileset, pszFileset))
            continue;

         Delete (pEnum);
         return lpi;
         }
      }

   return NULL;
}

