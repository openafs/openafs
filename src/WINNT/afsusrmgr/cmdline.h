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
   } CMDLINEOP;

CMDLINEOP ParseCommandLine (LPTSTR pszCmdLine);


#endif

