/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_INSTALL_H
#define SVR_INSTALL_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szSource[ MAX_PATH ];
   TCHAR szTarget[ MAX_PATH ];
   } SVR_INSTALL_PARAMS, *LPSVR_INSTALL_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Install (LPIDENT lpiServer = NULL);


#endif

