/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_PROP_H
#define SET_PROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fIDC_SET_WARN;
   BOOL fIDC_SET_WARN_SETFULL_DEF;
   WORD wIDC_SET_WARN_SETFULL_PERCENT;
   } SET_PROP_APPLY_PARAMS, *LPSET_PROP_APPLY_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_ShowProperties (LPIDENT lpiFileset, size_t nAlerts, BOOL fThreshold = FALSE);


#endif

