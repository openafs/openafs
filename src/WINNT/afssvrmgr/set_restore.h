#ifndef SET_RESTORE_H
#define SET_RESTORE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
  {
  LPIDENT lpi;
  TCHAR szFileset[ cchNAME ];
  TCHAR szFilename[ MAX_PATH ];
  BOOL fIncremental;
  } SET_RESTORE_PARAMS, *LPSET_RESTORE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Restore (LPIDENT lpiParent = NULL);


#endif

