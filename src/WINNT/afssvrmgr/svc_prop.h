/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVC_PROP_H
#define SVC_PROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fIDC_SVC_WARNSTOP;
   } SVC_PROP_APPLY_PACKET, *LPSVC_PROP_APPLY_PACKET;

typedef struct
   {
   LPIDENT lpi;
   BOOL fGeneral;
   SYSTEMTIME stGeneral;
   BOOL fNewBinary;
   SYSTEMTIME stNewBinary;
   } SVC_RESTARTTIMES_PARAMS, *LPSVC_RESTARTTIMES_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Services_ShowProperties (LPIDENT lpiService, size_t nAlerts);


#endif

