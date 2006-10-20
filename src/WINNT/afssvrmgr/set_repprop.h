/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_REPPROP_H
#define SET_REPPROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiReq;
   LPIDENT lpiRW;
   FILESETSTATUS fs;
   } SET_REPPROP_INIT_PARAMS, *LPSET_REPPROP_INIT_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_ShowReplication (HWND hDlg, LPIDENT lpiFileset, LPIDENT lpiTarget = NULL);

void Filesets_OnEndTask_ShowReplication (LPTASKPACKET ptp);


#endif

