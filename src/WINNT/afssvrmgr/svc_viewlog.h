/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVC_VIEWLOG_H
#define SVC_VIEWLOG_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   BOOL fTriedDownload;
   LPIDENT lpiService;
   LPIDENT lpiServer;
   TCHAR szRemote[ MAX_PATH ];
   TCHAR szLocal[ MAX_PATH ];
   size_t nDownloadAttempts;
   } SVC_VIEWLOG_PACKET, *LPSVC_VIEWLOG_PACKET;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Services_ShowServiceLog (LPIDENT lpi = NULL);
void Services_ShowServerLog (LPIDENT lpiServer, LPTSTR pszFilename = NULL);


#endif

