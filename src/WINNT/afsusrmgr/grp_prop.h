/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef GRP_PROP_H
#define GRP_PROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   gptANY = -1,
   gptPROBLEMS,
   gptGENERAL,
   gptMEMBERS
   } GROUPPROPTAB;

#define nGROUPPROPTAB_MAX  3


typedef struct
   {
   LPASIDLIST pGroupList;	// NULL if creating new group
   BOOL fDeleteMeOnClose;	// TRUE to delete structure when done
   BOOL fShowModal;	// TRUE to block while showing
   HWND hParent;

   BOOL fApplyGeneral;	// TRUE to apply these fields:
   ACCOUNTACCESS aaStatus;
   BOOL fStatus_Mixed;
   ACCOUNTACCESS aaGroups;
   BOOL fGroups_Mixed;
   ACCOUNTACCESS aaMembers;
   BOOL fMembers_Mixed;
   ACCOUNTACCESS aaAdd;
   BOOL fAdd_Mixed;
   ACCOUNTACCESS aaRemove;
   BOOL fRemove_Mixed;
   TCHAR szOwner[ cchNAME ];
   BOOL fOwner_Mixed;
   TCHAR szCreator[ cchNAME ];
   BOOL fCreator_Mixed;

   LPASIDLIST pMembers;
   LPASIDLIST pGroupsOwner;
   } GROUPPROPINFO, *LPGROUPPROPINFO;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Group_ShowProperties (LPASIDLIST pAsidList, GROUPPROPTAB gptTarget = gptANY);
void Group_ShowProperties (LPGROUPPROPINFO lpp, GROUPPROPTAB gptTarget = gptANY);
void Group_FreeProperties (LPGROUPPROPINFO lpp);


#endif

