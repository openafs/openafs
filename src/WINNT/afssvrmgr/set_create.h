/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_CREATE_H
#define SET_CREATE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiParent;
   TCHAR szName[ cchNAME ];
   size_t ckQuota;
   BOOL fCreateClone;
   } SET_CREATE_PARAMS, *LPSET_CREATE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Create (LPIDENT lpiParent = NULL);


#endif

