#ifndef SET_QUOTA_H
#define SET_QUOTA_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiFileset;
   size_t ckQuota;
   } SET_SETQUOTA_APPLY_PARAMS, *LPSET_SETQUOTA_APPLY_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL Filesets_SetQuota (LPIDENT lpiFileset, size_t ckQuota = 0);  // (0==ask)

size_t Filesets_PickQuota (LPIDENT lpiFileset);

void Filesets_DisplayQuota (HWND hDlg, LPFILESETSTATUS lpfs);



#endif

