/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_RESTORE_H
#define SET_RESTORE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
  {
  LPIDENT lpi;
  TCHAR szFileset[ cchNAME ];
  TCHAR szFilename[ MAX_PATH ];
  BOOL fIncremental;
  } SET_RESTORE_PARAMS, *LPSET_RESTORE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Restore (LPIDENT lpiParent = NULL);


#endif

