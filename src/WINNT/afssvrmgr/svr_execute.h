#ifndef SVR_EXECUTE_H
#define SVR_EXECUTE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szCommand[ MAX_PATH ];
   } SVR_EXECUTE_PARAMS, *LPSVR_EXECUTE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Execute (LPIDENT lpiServer = NULL);


#endif

