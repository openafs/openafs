/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_PRUNE_H
#define SVR_PRUNE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   BOOL fBAK;
   BOOL fOLD;
   BOOL fCore;
   } SVR_PRUNE_PARAMS, *LPSVR_PRUNE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Prune (LPIDENT lpiServer = NULL, BOOL fBAK = TRUE, BOOL fOLD = TRUE, BOOL fCore = TRUE);


#endif

