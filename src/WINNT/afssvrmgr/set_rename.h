/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_RENAME_H
#define SET_RENAME_H


/*
 * TYPEDEFS ___________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiReq;
   LPIDENT lpiRW;
   } SET_RENAME_INIT_PARAMS, *LPSET_RENAME_INIT_PARAMS;

typedef struct
   {
   LPIDENT lpiFileset;
   TCHAR szNewName[ cchNAME ];
   } SET_RENAME_APPLY_PARAMS, *LPSET_RENAME_APPLY_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_ShowRename (LPIDENT lpiFileset);
void Filesets_OnEndTask_ShowRename (LPTASKPACKET ptp);


#endif

