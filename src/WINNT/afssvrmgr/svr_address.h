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

