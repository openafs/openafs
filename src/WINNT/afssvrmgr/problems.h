/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef PROBLEMS_H
#define PROBLEMS_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define PropSheet_AddProblemsTab(_psh,_idd,_lpi,_nAlerts) \
   ( ((_nAlerts) == 0) ? TRUE : \
     (PropSheet_AddTab (_psh, IDS_PROBLEMS, _idd, (DLGPROC)Problems_DlgProc, (LPARAM)_lpi, TRUE)) )


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Problems_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


#endif

