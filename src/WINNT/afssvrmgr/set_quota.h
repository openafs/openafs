/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_QUOTA_H
#define SET_QUOTA_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiFileset;
   size_t ckQuota;
   } SET_SETQUOTA_APPLY_PARAMS, *LPSET_SETQUOTA_APPLY_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL Filesets_SetQuota (LPIDENT lpiFileset, size_t ckQuota = 0);  // (0==ask)

size_t Filesets_PickQuota (LPIDENT lpiFileset);

void Filesets_DisplayQuota (HWND hDlg, LPFILESETSTATUS lpfs);



#endif

