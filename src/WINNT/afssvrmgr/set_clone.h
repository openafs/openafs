#ifndef SET_CLONE_H
#define SET_CLONE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   BOOL fUsePrefix;
   BOOL fExcludePrefix;
   TCHAR szPrefix[ MAX_PATH ];
   BOOL fEnumedServers;
   BOOL fEnumedAggregs;
   } SET_CLONESYS_PARAMS, *LPSET_CLONESYS_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Clone (LPIDENT lpi);  // pass fileset, aggregate, server or cell


#endif

