/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

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

