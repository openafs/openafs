#ifndef SET_CREATEREP_H
#define SET_CREATEREP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiSource;
   LPIDENT lpiTarget;
   } SET_CREATEREP_PARAMS, *LPSET_CREATEREP_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_CreateReplica (LPIDENT lpiSource, LPIDENT lpiTarget = NULL);


#endif

