#ifndef SET_DELETE_H
#define SET_DELETE_H


/*
 * TYPEDEFS ___________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiFileset;
   BOOL fVLDB;
   BOOL fServer;
   short wGhost;
   int iddHelp;
   } SET_DELETE_PARAMS, *LPSET_DELETE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Delete (LPIDENT lpiFileset);


#endif

