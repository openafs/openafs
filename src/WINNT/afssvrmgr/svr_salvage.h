#ifndef SVR_SALVAGE_H
#define SVR_SALVAGE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiSalvage;
   TCHAR szTempDir[ MAX_PATH ];
   TCHAR szLogFile[ MAX_PATH ];
   int nProcesses;
   BOOL fForce;
   BOOL fReadonly;
   BOOL fLogInodes;
   BOOL fLogRootInodes;
   BOOL fRebuildDirs;
   BOOL fReadBlocks;
   } SVR_SALVAGE_PARAMS, *LPSVR_SALVAGE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Salvage (LPIDENT lpi);


#endif

