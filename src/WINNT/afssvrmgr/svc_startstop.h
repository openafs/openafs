/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVC_STARTSTOP_H
#define SVC_STARTSTOP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiStart;
   BOOL fTemporary;
   } SVC_START_PARAMS, *LPSVC_START_PARAMS;

typedef struct
   {
   LPIDENT lpiStop;
   BOOL fTemporary;
   } SVC_STOP_PARAMS, *LPSVC_STOP_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL Services_fRunning (LPSERVICE lpService);

void Services_Start (LPIDENT lpiService);
void Services_Restart (LPIDENT lpiService);
void Services_Stop (LPIDENT lpiService);


#endif

