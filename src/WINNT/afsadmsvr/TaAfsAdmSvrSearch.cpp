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

#include "TaAfsAdmSvrInternal.h"

#define c100ns1SECOND   ((LONGLONG)10000000)


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL IsValidTime (LPSYSTEMTIME pst)
{
   return (pst->wYear > 1970) ? TRUE : FALSE;
}


BOOL AfsAdmSvr_Search_Compare (LPTSTR pszName, LPTSTR pszPattern)
{
   // An empty pattern matches everyone
   //
   if (!pszPattern || !*pszPattern)
      return TRUE;

   // Cache the expression so we only have to compile it when it changes.
   // We also add a little convenience measure: you can prepend "!" to any
   // regexp to negate the entire sense of the regexp--for instance, "!ri"
   // matches every word which doesn't have "ri" in it.
   //
   static LPREGEXP pLastExpr = NULL;
   static TCHAR szLastExpression[ 1024 ] = TEXT("");
   static BOOL fLastInclusive = TRUE;

   if (lstrcmp (pszPattern, szLastExpression))
      {
      if (pLastExpr)
         Delete (pLastExpr);
      lstrcpy (szLastExpression, pszPattern);
      if ((fLastInclusive = (*pszPattern != TEXT('!'))) == FALSE)
         ++pszPattern;
      pLastExpr = New2 (REGEXP,(pszPattern));
      }

   if (!pLastExpr) // this shouldn't happen, but be safe anyway
      return TRUE;

   return ((pLastExpr->Matches (pszName)) == fLastInclusive);
}


BOOL AfsAdmSvr_Search_Compare (LPIDENT lpi, LPTSTR pszPattern)
{
   if (lpi->GetRefCount() == 0)
      return FALSE;

   TCHAR szName[ cchSTRING ];
   switch (lpi->GetType())
      {
      case itCELL:
         lpi->GetCellName (szName);
         break;
      case itSERVER:
         lpi->GetLongServerName (szName);
         break;
      case itSERVICE:
         lpi->GetServiceName (szName);
         break;
      case itAGGREGATE:
         lpi->GetAggregateName (szName);
         break;
      case itFILESET:
         lpi->GetFilesetName (szName);
         break;
      case itUSER:
         lpi->GetUserName (szName);
         break;
      case itGROUP:
         lpi->GetGroupName (szName);
         break;
      default:
         return FALSE;
      }

   return AfsAdmSvr_Search_Compare (szName, pszPattern);
}


BOOL AfsAdmSvr_SearchRefresh (ASID idSearchScope, ASOBJTYPE ObjectType, AFSADMSVR_SEARCH_REFRESH SearchRefresh, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   if (SearchRefresh == SEARCH_ALL_OBJECTS)
      {
      if (GetAsidType (idSearchScope) == itCELL)
         {
         LPCELL lpCell;
         if ((lpCell = ((LPIDENT)idSearchScope)->OpenCell (&status)) == NULL)
            rc = FALSE;
         else
            {
            lpCell->RefreshAll();
            lpCell->Close();
            }
         }
      else // (GetAsidType (idSearchScope) != itCELL)
         {
         LPSERVER lpServer;
         if ((lpServer = ((LPIDENT)idSearchScope)->OpenServer (&status)) == NULL)
            rc = FALSE;
         else
            {
            // What do we need to refresh on this server?
            switch (ObjectType)
               {
               case TYPE_SERVICE:
                  lpServer->RefreshStatus();
                  lpServer->RefreshServices();
                  break;

               case TYPE_PARTITION:
               case TYPE_VOLUME:
                  lpServer->RefreshStatus();
                  lpServer->RefreshAggregates();
                  break;

               default:
                  lpServer->RefreshAll();
                  break;
               }
            lpServer->Close();
            }
         }
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_AllInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetCell() != (LPIDENT)idCell)
         continue;
      if (!AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
         continue;
      AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_ServersInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idCell)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum, TRUE, &status); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         LPIDENT lpiFind = lpServer->GetIdentifier();
         if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
            AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
         lpServer->Close();
         }
      lpCell->ServerFindClose (&hEnum);
      if (status && (status != ADMITERATORDONE))
         rc = FALSE;
      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_ServicesInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itSERVICE)
         continue;
      if (lpiFind->GetCell() != (LPIDENT)idCell)
         continue;
      if (!AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
         continue;
      AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_PartitionsInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itAGGREGATE)
         continue;
      if (lpiFind->GetCell() != (LPIDENT)idCell)
         continue;
      if (!AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
         continue;
      AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_VolumesInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itFILESET)
         continue;
      if (lpiFind->GetCell() != (LPIDENT)idCell)
         continue;
      if (!AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
         continue;
      AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_UsersInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idCell)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPUSER lpUser = lpCell->UserFindFirst (&hEnum, TRUE, &status); lpUser; lpUser = lpCell->UserFindNext (&hEnum))
         {
         LPIDENT lpiFind = lpUser->GetIdentifier();
         if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
            AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
         lpUser->Close();
         }
      lpCell->UserFindClose (&hEnum);
      if (status && (status != ADMITERATORDONE))
         rc = FALSE;
      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_GroupsInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idCell)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPPTSGROUP lpGroup = lpCell->GroupFindFirst (&hEnum, TRUE, &status); lpGroup; lpGroup = lpCell->GroupFindNext (&hEnum))
         {
         LPIDENT lpiFind = lpGroup->GetIdentifier();
         if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
            AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
         lpGroup->Close();
         }
      lpCell->GroupFindClose (&hEnum);
      if (status && (status != ADMITERATORDONE))
         rc = FALSE;
      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_AllInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPSERVER lpServer;
   if ((lpServer = ((LPIDENT)idServer)->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&hEnum, TRUE, &status); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&hEnum))
         {
         LPIDENT lpiFind = lpAggregate->GetIdentifier();
         if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
            AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
         lpAggregate->Close();
         }
      lpServer->AggregateFindClose (&hEnum);
      if (status && (status != ADMITERATORDONE))
         rc = FALSE;
      else
         {
         for (LPSERVICE lpService = lpServer->ServiceFindFirst (&hEnum, TRUE, &status); lpService; lpService = lpServer->ServiceFindNext (&hEnum))
            {
            LPIDENT lpiFind = lpService->GetIdentifier();
            if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
               AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
            lpService->Close();
            }
         lpServer->ServiceFindClose (&hEnum);
         if (status && (status != ADMITERATORDONE))
            rc = FALSE;
         }
      lpServer->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_ServicesInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPSERVER lpServer;
   if ((lpServer = ((LPIDENT)idServer)->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPSERVICE lpService = lpServer->ServiceFindFirst (&hEnum, TRUE, &status); lpService; lpService = lpServer->ServiceFindNext (&hEnum))
         {
         LPIDENT lpiFind = lpService->GetIdentifier();
         if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
            AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
         lpService->Close();
         }
      lpServer->ServiceFindClose (&hEnum);
      if (status && (status != ADMITERATORDONE))
         rc = FALSE;
      lpServer->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_PartitionsInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPSERVER lpServer;
   if ((lpServer = ((LPIDENT)idServer)->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&hEnum, TRUE, &status); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&hEnum))
         {
         LPIDENT lpiFind = lpAggregate->GetIdentifier();
         if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
            AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
         lpAggregate->Close();
         }
      lpServer->AggregateFindClose (&hEnum);
      if (status && (status != ADMITERATORDONE))
         rc = FALSE;
      lpServer->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_VolumesInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itFILESET)
         continue;
      if (lpiFind->GetServer() != (LPIDENT)idServer)
         continue;
      if (!AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
         continue;
      AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_VolumesInPartition (LPASIDLIST *ppList, ASID idPartition, LPTSTR pszPattern, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPAGGREGATE lpAggregate;
   if ((lpAggregate = ((LPIDENT)idPartition)->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPFILESET lpFileset = lpAggregate->FilesetFindFirst (&hEnum, TRUE, &status); lpFileset; lpFileset = lpAggregate->FilesetFindNext (&hEnum))
         {
         LPIDENT lpiFind = lpFileset->GetIdentifier();
         if (AfsAdmSvr_Search_Compare (lpiFind, pszPattern))
            AfsAdmSvr_AddToAsidList (ppList, (ASID)lpiFind, 0);
         lpFileset->Close();
         }
      lpAggregate->FilesetFindClose (&hEnum);
      if (status && (status != ADMITERATORDONE))
         rc = FALSE;
      lpAggregate->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_ServerInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idCell)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPSERVER lpServer;
      if ((lpServer = lpCell->OpenServer (pszName, &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpServer->GetIdentifier());
         lpServer->Close();
         }
      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_ServiceInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = ERROR_INVALID_PARAMETER;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itSERVICE)
         continue;
      if (lpiFind->GetCell() != (LPIDENT)idCell)
         continue;
      if (lpiFind->GetRefCount() == 0)
         return FALSE;

      TCHAR szService[ cchNAME ];
      lpiFind->GetServiceName (szService);
      if (!lstrcmpi (pszName, szService))
         {
         *pidObject = (ASID)lpiFind;
         rc = TRUE;
         break;
         }
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_PartitionInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = ERROR_INVALID_PARAMETER;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itAGGREGATE)
         continue;
      if (lpiFind->GetCell() != (LPIDENT)idCell)
         continue;
      if (lpiFind->GetRefCount() == 0)
         return FALSE;

      TCHAR szAggregate[ cchNAME ];
      lpiFind->GetAggregateName (szAggregate);
      if (!lstrcmpi (pszName, szAggregate))
         {
         *pidObject = (ASID)lpiFind;
         rc = TRUE;
         break;
         }
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_VolumeInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = ERROR_INVALID_PARAMETER;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itFILESET)
         continue;
      if (lpiFind->GetCell() != (LPIDENT)idCell)
         continue;
      if (lpiFind->GetRefCount() == 0)
         return FALSE;

      TCHAR szFileset[ cchNAME ];
      lpiFind->GetFilesetName (szFileset);
      if (!lstrcmpi (szFileset, pszName))
         {
         *pidObject = (ASID)lpiFind;
         rc = TRUE;
         break;
         }
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_UserInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idCell)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPUSER lpUser;
      if ((lpUser = lpCell->OpenUser (pszName, NULL, &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpUser->GetIdentifier());
         lpUser->Close();
         }
      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_GroupInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idCell)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPPTSGROUP lpGroup;
      if ((lpGroup = lpCell->OpenGroup (pszName, &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpGroup->GetIdentifier());
         lpGroup->Close();
         }
      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_ServiceInServer (ASID *pidObject, ASID idServer, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPSERVER lpServer;
   if ((lpServer = ((LPIDENT)idServer)->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPSERVICE lpService;
      if ((lpService = lpServer->OpenService (pszName, &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpService->GetIdentifier());
         lpService->Close();
         }
      lpServer->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_PartitionInServer (ASID *pidObject, ASID idServer, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPSERVER lpServer;
   if ((lpServer = ((LPIDENT)idServer)->OpenServer (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpServer->OpenAggregate (pszName, &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpAggregate->GetIdentifier());
         lpAggregate->Close();
         }
      lpServer->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_VolumeInServer (ASID *pidObject, ASID idServer, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = FALSE;
   ULONG status = ERROR_INVALID_PARAMETER;

   HENUM hEnum;
   for (LPIDENT lpiFind = IDENT::FindFirst (&hEnum); lpiFind; lpiFind = IDENT::FindNext (&hEnum))
      {
      if (lpiFind->GetType() != itFILESET)
         continue;
      if (lpiFind->GetServer() != (LPIDENT)idServer)
         continue;
      if (lpiFind->GetRefCount() == 0)
         return FALSE;

      TCHAR szFileset[ cchNAME ];
      lpiFind->GetFilesetName (szFileset);
      if (!lstrcmpi (pszName, szFileset))
         {
         *pidObject = (ASID)lpiFind;
         rc = TRUE;
         break;
         }
      }
   IDENT::FindClose (&hEnum);

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_VolumeInPartition (ASID *pidObject, ASID idPartition, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPAGGREGATE lpAggregate;
   if ((lpAggregate = ((LPIDENT)idPartition)->OpenAggregate (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPFILESET lpFileset;
      if ((lpFileset = lpAggregate->OpenFileset (pszName, &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpFileset->GetIdentifier());
         lpFileset->Close();
         }
      lpAggregate->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAdmSvr_Search_OneUser (ASID *pidObject, ASID idScope, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idScope)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPUSER lpUser;
      if ((lpUser = lpCell->OpenUser (pszName, TEXT(""), &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpUser->GetIdentifier());
         lpUser->Close();
         }

      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}

BOOL AfsAdmSvr_Search_OneGroup (ASID *pidObject, ASID idScope, LPTSTR pszName, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   LPCELL lpCell;
   if ((lpCell = ((LPIDENT)idScope)->OpenCell (&status)) == NULL)
      rc = FALSE;
   else
      {
      LPPTSGROUP lpGroup;
      if ((lpGroup = lpCell->OpenGroup (pszName, &status)) == NULL)
         rc = FALSE;
      else
         {
         *pidObject = (ASID)(lpGroup->GetIdentifier());
         lpGroup->Close();
         }

      lpCell->Close();
      }

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


void AfsAdmSvr_Search_Advanced (LPASIDLIST *ppList, LPAFSADMSVR_SEARCH_PARAMS pSearchParams)
{
   ULONG status;

   if (ppList && (*ppList) && pSearchParams && (pSearchParams->SearchType != SEARCH_NO_LIMITATIONS))
      {
      for (size_t iEntry = 0; iEntry < (*ppList)->cEntries; )
         {
         BOOL fDelete = TRUE;

         // Try to grab the properties for this object. If we succeed,
         // we can determine if the object matches the necessary criteria
         // to indicate we should keep it in the list.
         //
         ASID idObject = (*ppList)->aEntries[ iEntry ].idObject;

         LPASOBJPROP pProperties;
         if ((pProperties = AfsAdmSvr_GetCurrentProperties (idObject, &status)) != NULL)
            {
            if ( (pProperties->verProperties >= verPROP_FIRST_SCAN) ||
                 (AfsAdmSvr_ObtainFullProperties (pProperties, &status)) )
               {
               // We managed to get the full properties for this object.
               // Now check its properties against our search criteria--
               // only if it has exactly the necessary criteria will we
               // clear {fDelete}.
               //
               switch (pSearchParams->SearchType)
                  {
                  case SEARCH_EXPIRES_BEFORE:
                     if ((pProperties->Type == TYPE_USER) && (pProperties->u.UserProperties.fHaveKasInfo))
                        {
                        if (IsValidTime (&pProperties->u.UserProperties.KASINFO.timeExpires))
                           {
                           FILETIME ftExpires;
                           SystemTimeToFileTime (&pProperties->u.UserProperties.KASINFO.timeExpires, &ftExpires);

                           FILETIME ftThreshhold;
                           SystemTimeToFileTime (&pSearchParams->SearchTime, &ftThreshhold);

                           if (CompareFileTime (&ftExpires, &ftThreshhold) <= 0)
                              fDelete = FALSE;
                           }
                        }
                     break;

                  case SEARCH_PASSWORD_EXPIRES_BEFORE:
                     if ((pProperties->Type == TYPE_USER) && (pProperties->u.UserProperties.fHaveKasInfo))
                        {
                        if (pProperties->u.UserProperties.KASINFO.cdayPwExpire)
                           {
                           FILETIME ftPwExpires;
                           SystemTimeToFileTime (&pProperties->u.UserProperties.KASINFO.timeLastPwChange, &ftPwExpires);

                           // A FILETIME is a count-of-100ns-intervals since 1601.
                           // We need to increase the {ftPwExpires} time by the
                           // number of days in KASINFO.cdayPwExpires, so we'll
                           // be adding a big number to our FILETIME structure.
                           //
                           LARGE_INTEGER ldw;
                           ldw.HighPart = ftPwExpires.dwHighDateTime;
                           ldw.LowPart = ftPwExpires.dwLowDateTime;
                           ldw.QuadPart += c100ns1SECOND * (LONGLONG)csec1DAY * (LONGLONG)pProperties->u.UserProperties.KASINFO.cdayPwExpire;
                           ftPwExpires.dwHighDateTime = (DWORD)ldw.HighPart;
                           ftPwExpires.dwLowDateTime = (DWORD)ldw.LowPart;

                           FILETIME ftThreshhold;
                           SystemTimeToFileTime (&pSearchParams->SearchTime, &ftThreshhold);

                           if (CompareFileTime (&ftPwExpires, &ftThreshhold) <= 0)
                              fDelete = FALSE;
                           }
                        }
                     break;
                  }
               }
            }

         // Okay, we've made our choice--remove it, or not.
         //
         if (fDelete)
            AfsAdmSvr_RemoveFromAsidListByIndex (ppList, iEntry);
         else
            ++iEntry;
         }
      }
}

