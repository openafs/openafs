/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_SYNCVLDB_H
#define SVR_SYNCVLDB_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fForce;
   } SVR_SYNCVLDB_PARAMS, *LPSVR_SYNCVLDB_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_SyncVLDB (LPIDENT lpi);


#endif

