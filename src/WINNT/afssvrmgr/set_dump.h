/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_DUMP_H
#define SET_DUMP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   TCHAR szFilename[ MAX_PATH ];
   BOOL fDumpByDate;
   SYSTEMTIME stDump;
   } SET_DUMP_PARAMS, *LPSET_DUMP_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Dump (LPIDENT lpi);


#endif

