/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_CREATEREP_H
#define SET_CREATEREP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiSource;
   LPIDENT lpiTarget;
   } SET_CREATEREP_PARAMS, *LPSET_CREATEREP_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_CreateReplica (LPIDENT lpiSource, LPIDENT lpiTarget = NULL);


#endif

