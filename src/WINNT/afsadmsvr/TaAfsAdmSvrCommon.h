/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRCOMMON_H
#define TAAFSADMSVRCOMMON_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

      // ASIDLIST - Managed type for lists of cell objects
      //
LPASIDLIST AfsAdmSvr_CreateAsidList (void);
LPASIDLIST AfsAdmSvr_CopyAsidList (LPASIDLIST pListSource);
BOOL AfsAdmSvr_AddToAsidList (LPASIDLIST *ppList, ASID idObject, LPARAM lp);
BOOL AfsAdmSvr_RemoveFromAsidList (LPASIDLIST *ppList, ASID idObject);
BOOL AfsAdmSvr_RemoveFromAsidListByIndex (LPASIDLIST *ppList, size_t iIndex);
BOOL AfsAdmSvr_SetAsidListParam (LPASIDLIST *ppList, ASID idObject, LPARAM lp);
BOOL AfsAdmSvr_SetAsidListParamByIndex (LPASIDLIST *ppList, size_t iIndex, LPARAM lp);
BOOL AfsAdmSvr_IsInAsidList (LPASIDLIST *ppList, ASID idObject, LPARAM *pParam = NULL);
void AfsAdmSvr_FreeAsidList (LPASIDLIST *ppList);

      // ASOBJPROPLIST - Managed type for lists of cell objects
      //
LPASOBJPROPLIST AfsAdmSvr_CreateObjPropList (void);
LPASOBJPROPLIST AfsAdmSvr_CopyObjPropList (LPASOBJPROPLIST pListSource);
BOOL AfsAdmSvr_AddToObjPropList (LPASOBJPROPLIST *ppList, LPASOBJPROP pProperties, LPARAM lp);
BOOL AfsAdmSvr_RemoveFromObjPropList (LPASOBJPROPLIST *ppList, ASID idObject);
BOOL AfsAdmSvr_IsInObjPropList (LPASOBJPROPLIST *ppList, ASID idObject, LPASOBJPROP pProperties = NULL, LPARAM *pParam = NULL);
void AfsAdmSvr_FreeObjPropList (LPASOBJPROPLIST *ppList);

      // ASACTIONLIST - Managed type for lists of cell objects
      //
LPASACTIONLIST AfsAdmSvr_CreateActionList (void);
LPASACTIONLIST AfsAdmSvr_CopyActionList (LPASACTIONLIST pListSource);
BOOL AfsAdmSvr_AddToActionList (LPASACTIONLIST *pLispt, LPASACTION pAction);
BOOL AfsAdmSvr_RemoveFromActionList (LPASACTIONLIST *ppList, DWORD idAction);
BOOL AfsAdmSvr_IsInActionList (LPASACTIONLIST *ppList, DWORD idAction, LPASACTION pAction = NULL);
void AfsAdmSvr_FreeActionList (LPASACTIONLIST *ppList);


#endif // TAAFSADMSVRCOMMON_H
