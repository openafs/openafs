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

