#ifndef SVC_STARTSTOP_H
#define SVC_STARTSTOP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiStart;
   BOOL fTemporary;
   } SVC_START_PARAMS, *LPSVC_START_PARAMS;

typedef struct
   {
   LPIDENT lpiStop;
   BOOL fTemporary;
   } SVC_STOP_PARAMS, *LPSVC_STOP_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL Services_fRunning (LPSERVICE lpService);

void Services_Start (LPIDENT lpiService);
void Services_Restart (LPIDENT lpiService);
void Services_Stop (LPIDENT lpiService);


#endif

