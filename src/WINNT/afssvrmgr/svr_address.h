/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_ADDRESS_H
#define SVR_ADDRESS_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   SERVERSTATUS ssOld;
   SERVERSTATUS ssNew;
   } SVR_CHANGEADDR_PARAMS, *LPSVR_CHANGEADDR_PARAMS;

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_FillAddrList (HWND hDlg, LPSERVERSTATUS lpss, BOOL fCanAddUnspecified = TRUE);

void Server_ParseAddress (LPSOCKADDR_IN pAddr, LPTSTR pszText);

BOOL Server_Ping (LPSOCKADDR_IN pAddr, LPCTSTR pszServerName);

BOOL CALLBACK ChangeAddr_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


#endif

