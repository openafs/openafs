/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRSEARCH_H
#define TAAFSADMSVRSEARCH_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <WINNT/TaAfsAdmSvr.h>


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL AfsAdmSvr_SearchRefresh (ASID idSearchScope, ASOBJTYPE ObjectType, AFSADMSVR_SEARCH_REFRESH SearchRefresh, ULONG *pStatus);

BOOL AfsAdmSvr_Search_AllInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_ServersInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_ServicesInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_PartitionsInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_VolumesInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_UsersInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_GroupsInCell (LPASIDLIST *ppList, ASID idCell, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_AllInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_ServicesInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_PartitionsInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_VolumesInServer (LPASIDLIST *ppList, ASID idServer, LPTSTR pszPattern, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_VolumesInPartition (LPASIDLIST *ppList, ASID idPartition, LPTSTR pszPattern, ULONG *pStatus = NULL);

BOOL AfsAdmSvr_Search_ServerInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_ServiceInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_PartitionInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_VolumeInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_UserInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_GroupInCell (ASID *pidObject, ASID idCell, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_ServiceInServer (ASID *pidObject, ASID idServer, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_PartitionInServer (ASID *pidObject, ASID idServer, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_VolumeInServer (ASID *pidObject, ASID idServer, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_VolumeInPartition (ASID *pidObject, ASID idPartition, LPTSTR pszName, ULONG *pStatus = NULL);

BOOL AfsAdmSvr_Search_OneUser (ASID *pidObject, ASID idScope, LPTSTR pszName, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_Search_OneGroup (ASID *pidObject, ASID idScope, LPTSTR pszName, ULONG *pStatus = NULL);

void AfsAdmSvr_Search_Advanced (LPASIDLIST *ppList, LPAFSADMSVR_SEARCH_PARAMS pSearchParams);

#endif // TAAFSADMSVRSEARCH_H
