/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef ERRDATA_H
#define ERRDATA_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


typedef struct
   {
   size_t cFailures;
   LPASIDLIST pAsidList;
   ULONG status;
   int idsSingle;
   int idsMultiple;
   } ERRORDATA, *LPERRORDATA;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

LPERRORDATA ED_Create (int idsSingle, int idsMultiple);
void ED_Free (LPERRORDATA ped);
void ED_RegisterStatus (LPERRORDATA ped, ASID idObject, BOOL fSuccess, ULONG status);
ULONG ED_GetFinalStatus (LPERRORDATA ped);
void ED_ShowErrorDialog (LPERRORDATA ped);


#endif

