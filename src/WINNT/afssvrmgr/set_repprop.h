#ifndef SET_REPPROP_H
#define SET_REPPROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiReq;
   LPIDENT lpiRW;
   FILESETSTATUS fs;
   } SET_REPPROP_INIT_PARAMS, *LPSET_REPPROP_INIT_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_ShowReplication (HWND hDlg, LPIDENT lpiFileset, LPIDENT lpiTarget = NULL);

void Filesets_OnEndTask_ShowReplication (LPTASKPACKET ptp);


#endif

