#ifndef CELL_PROP_H
#define CELL_PROP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   cptANY = -1,
   cptPROBLEMS,
   cptGENERAL,
   } CELLPROPTAB;

#define nCELLPROPTAB_MAX  2


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Cell_ShowProperties (CELLPROPTAB cptTarget = cptANY);


#endif

