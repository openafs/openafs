#ifndef SVR_INSTALL_H
#define SVR_INSTALL_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szSource[ MAX_PATH ];
   TCHAR szTarget[ MAX_PATH ];
   } SVR_INSTALL_PARAMS, *LPSVR_INSTALL_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Install (LPIDENT lpiServer = NULL);


#endif

