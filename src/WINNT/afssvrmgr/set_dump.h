#ifndef SET_DUMP_H
#define SET_DUMP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpi;
   TCHAR szFilename[ MAX_PATH ];
   BOOL fDumpByDate;
   SYSTEMTIME stDump;
   } SET_DUMP_PARAMS, *LPSET_DUMP_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_Dump (LPIDENT lpi);


#endif

