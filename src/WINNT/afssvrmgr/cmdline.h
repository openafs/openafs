/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CMDLINE_H
#define CMDLINE_H

/*
 * TYPEDEFS _________________________________________________________________
 *
 */

typedef enum
   {
   opCLOSEAPP,	// close the application entirely
   opNORMAL,	// proceed normally
   opNOCELLDIALOG,	// ...but don't ask for a cell
   opLOOKUPERRORCODE,	// just show the "lookup error code" dlg
   } CMDLINEOP;

CMDLINEOP ParseCommandLine (LPTSTR pszCmdLine);


#endif

