/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_DELETE_H
#define SET_DELETE_H


/*
 * TYPEDEFS ___________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiFileset;
   BOOL fVLDB;
   BOOL fServer;
   short wGhost;
   int iddHelp;
   } SET_DELETE_PARAMS, *LPSET_DELETE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Delete (LPIDENT lpiFileset);


#endif

