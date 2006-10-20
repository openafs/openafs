/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_CLONE_H
#define SET_CLONE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fUsePrefix;
   BOOL fExcludePrefix;
   TCHAR szPrefix[ MAX_PATH ];
   BOOL fEnumedServers;
   BOOL fEnumedAggregs;
   } SET_CLONESYS_PARAMS, *LPSET_CLONESYS_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Clone (LPIDENT lpi);  // pass fileset, aggregate, server or cell


#endif

