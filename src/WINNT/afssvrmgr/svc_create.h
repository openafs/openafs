#ifndef SVC_CREATE_H
#define SVC_CREATE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   TCHAR szService[ cchNAME ];
   TCHAR szCommand[ cchNAME ];
   TCHAR szParams[ cchNAME ];
   TCHAR szNotifier[ cchNAME ];
   TCHAR szLogFile[ cchNAME ];
   SERVICETYPE type;
   BOOL fRunNow;
   SYSTEMTIME stIfCron;
   } SVC_CREATE_PARAMS, *LPSVC_CREATE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Services_Create (LPIDENT lpiServer);


#endif

