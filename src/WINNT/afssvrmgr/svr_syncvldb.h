#ifndef SVR_SYNCVLDB_H
#define SVR_SYNCVLDB_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fForce;
   } SVR_SYNCVLDB_PARAMS, *LPSVR_SYNCVLDB_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_SyncVLDB (LPIDENT lpi);


#endif

