/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_SALVAGE_H
#define SVR_SALVAGE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiSalvage;
   TCHAR szTempDir[ MAX_PATH ];
   TCHAR szLogFile[ MAX_PATH ];
   int nProcesses;
   BOOL fForce;
   BOOL fReadonly;
   BOOL fLogInodes;
   BOOL fLogRootInodes;
   BOOL fRebuildDirs;
   BOOL fReadBlocks;
   } SVR_SALVAGE_PARAMS, *LPSVR_SALVAGE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Salvage (LPIDENT lpi);


#endif

