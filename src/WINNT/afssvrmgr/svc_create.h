/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVC_CREATE_H
#define SVC_CREATE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szService[ cchNAME ];
   TCHAR szCommand[ cchNAME ];
   TCHAR szParams[ cchNAME ];
   TCHAR szNotifier[ cchNAME ];
   TCHAR szLogFile[ cchNAME ];
   AFSSERVICETYPE type;
   BOOL fRunNow;
   SYSTEMTIME stIfCron;
   } SVC_CREATE_PARAMS, *LPSVC_CREATE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Services_Create (LPIDENT lpiServer);


#endif

