/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AGG_PROP_H
#define AGG_PROP_H

/*
 * TYPEDEFS ___________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fIDC_AGG_WARNALLOC;
   BOOL fIDC_AGG_WARN;
   BOOL fIDC_AGG_WARN_AGGFULL_DEF;
   short wIDC_AGG_WARN_AGGFULL_PERCENT;
   } AGG_PROP_APPLY_PACKET, *LPAGG_PROP_APPLY_PACKET;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Aggregates_ShowProperties (LPIDENT lpi, size_t nAlerts, BOOL fThreshold = FALSE, HWND hParentModal = NULL);


#endif

