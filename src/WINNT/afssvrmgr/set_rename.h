#ifndef SET_RENAME_H
#define SET_RENAME_H


/*
 * TYPEDEFS ___________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiReq;
   LPIDENT lpiRW;
   } SET_RENAME_INIT_PARAMS, *LPSET_RENAME_INIT_PARAMS;

typedef struct
   {
   LPIDENT lpiFileset;
   TCHAR szNewName[ cchNAME ];
   } SET_RENAME_APPLY_PARAMS, *LPSET_RENAME_APPLY_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_ShowRename (LPIDENT lpiFileset);
void Filesets_OnEndTask_ShowRename (LPTASKPACKET ptp);


#endif

