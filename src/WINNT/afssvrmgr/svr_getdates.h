#ifndef SVR_GETDATES_H
#define SVR_GETDATES_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szFilename[ MAX_PATH ];
   } SVR_GETDATES_PARAMS, *LPSVR_GETDATES_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_GetDates (LPIDENT lpiServer = NULL);


#endif

