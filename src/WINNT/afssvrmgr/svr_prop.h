#ifndef SVR_PROP_H
#define SVR_PROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   BOOL fEnableAuth;
   } SVR_SETAUTH_PARAMS, *LPSVR_SETAUTH_PARAMS;


typedef struct
   {
   LPIDENT lpiServer;

   BOOL fIDC_SVR_WARN_AGGFULL;
   WORD wIDC_SVR_WARN_AGGFULL_PERCENT;

   BOOL fIDC_SVR_WARN_SETFULL;
   WORD wIDC_SVR_WARN_SETFULL_PERCENT;

   BOOL fIDC_SVR_WARN_AGGALLOC;
   BOOL fIDC_SVR_WARN_SVCSTOP;
   BOOL fIDC_SVR_WARN_TIMEOUT;
   BOOL fIDC_SVR_WARN_SETNOVLDB;
   BOOL fIDC_SVR_WARN_SETNOSERV;
   BOOL fIDC_SVR_WARN_AGGNOSERV;

   BOOL fIDC_SVR_AUTOREFRESH;
   size_t dwIDC_SVR_AUTOREFRESH_MINUTES;
   } SVR_SCOUT_APPLY_PACKET, *LPSVR_SCOUT_APPLY_PACKET;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_ShowProperties (LPIDENT lpiServer, size_t nAlerts);


#endif

