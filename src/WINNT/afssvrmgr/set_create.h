#ifndef SET_CREATE_H
#define SET_CREATE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiParent;
   TCHAR szName[ cchNAME ];
   size_t ckQuota;
   BOOL fCreateClone;
   } SET_CREATE_PARAMS, *LPSET_CREATE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Create (LPIDENT lpiParent = NULL);


#endif

