/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CELL_PROP_H
#define CELL_PROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   cptANY = -1,
   cptPROBLEMS,
   cptGENERAL,
   } CELLPROPTAB;

#define nCELLPROPTAB_MAX  2


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Cell_ShowProperties (CELLPROPTAB cptTarget = cptANY);


#endif

