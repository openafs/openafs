/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_EXECUTE_H
#define SVR_EXECUTE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szCommand[ MAX_PATH ];
   } SVR_EXECUTE_PARAMS, *LPSVR_EXECUTE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Execute (LPIDENT lpiServer = NULL);


#endif

