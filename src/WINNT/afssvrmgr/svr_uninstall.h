#ifndef SVR_UNINSTALL_H
#define SVR_UNINSTALL_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szUninstall[ MAX_PATH ];
   } SVR_UNINSTALL_PARAMS, *LPSVR_UNINSTALL_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Uninstall (LPIDENT lpiServer = NULL);


#endif

