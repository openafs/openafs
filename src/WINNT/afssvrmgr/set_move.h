/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_MOVE_H
#define SET_MOVE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiSource;
   LPIDENT lpiTarget;
   } SET_MOVE_PARAMS, *LPSET_MOVE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_ShowMoveTo (LPIDENT lpiSource, LPIDENT lpiTarget);


#endif

