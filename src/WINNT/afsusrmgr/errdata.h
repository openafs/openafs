#ifndef ERRDATA_H
#define ERRDATA_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


typedef struct
   {
   size_t cFailures;
   LPASIDLIST pAsidList;
   ULONG status;
   int idsSingle;
   int idsMultiple;
   } ERRORDATA, *LPERRORDATA;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

LPERRORDATA ED_Create (int idsSingle, int idsMultiple);
void ED_Free (LPERRORDATA ped);
void ED_RegisterStatus (LPERRORDATA ped, ASID idObject, BOOL fSuccess, ULONG status);
ULONG ED_GetFinalStatus (LPERRORDATA ped);
void ED_ShowErrorDialog (LPERRORDATA ped);


#endif

