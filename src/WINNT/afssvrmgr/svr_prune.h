#ifndef SVR_PRUNE_H
#define SVR_PRUNE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   BOOL fBAK;
   BOOL fOLD;
   BOOL fCore;
   } SVR_PRUNE_PARAMS, *LPSVR_PRUNE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Prune (LPIDENT lpiServer = NULL, BOOL fBAK = TRUE, BOOL fOLD = TRUE, BOOL fCore = TRUE);


#endif

