#ifndef SET_RELEASE_H
#define SET_RELEASE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiRW;
   BOOL fForce;
   } SET_RELEASE_PARAMS, *LPSET_RELEASE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Release (LPIDENT lpi);


#endif

