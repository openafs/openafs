#ifndef SET_MOVE_H
#define SET_MOVE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiSource;
   LPIDENT lpiTarget;
   } SET_MOVE_PARAMS, *LPSET_MOVE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_ShowMoveTo (LPIDENT lpiSource, LPIDENT lpiTarget);


#endif

