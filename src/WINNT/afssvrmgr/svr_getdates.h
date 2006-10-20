/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_GETDATES_H
#define SVR_GETDATES_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szFilename[ MAX_PATH ];
   } SVR_GETDATES_PARAMS, *LPSVR_GETDATES_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_GetDates (LPIDENT lpiServer = NULL);


#endif

