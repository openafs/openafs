#ifndef BROWSE_H
#define BROWSE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   HWND hParent;
   int iddForHelp;
   int idsTitle;
   int idsPrompt;
   int idsCheck;
   ASOBJTYPE TypeToShow;
   LPASIDLIST pObjectsToSkip;
   LPASIDLIST pObjectsSelected;
   BOOL fAllowMultiple;
   TCHAR szName[ cchNAME ];
   BOOL fQuerying; // used internally
   } BROWSE_PARAMS, *LPBROWSE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL ShowBrowseDialog (LPBROWSE_PARAMS lpp);


#endif

