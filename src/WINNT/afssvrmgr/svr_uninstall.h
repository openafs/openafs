/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_UNINSTALL_H
#define SVR_UNINSTALL_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szUninstall[ MAX_PATH ];
   } SVR_UNINSTALL_PARAMS, *LPSVR_UNINSTALL_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Uninstall (LPIDENT lpiServer = NULL);


#endif

