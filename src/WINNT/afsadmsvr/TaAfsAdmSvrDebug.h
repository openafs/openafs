/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRDEBUG_H
#define TAAFSADMSVRDEBUG_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <WINNT/TaAfsAdmSvr.h>


/*
 * DETAIL LEVELS ______________________________________________________________
 *
 */

#define dlSTANDARD      0x00000001
#define dlWARNING       0x00000002
#define dlERROR         0x00000004
#define dlCONNECTION    0x00000008
#define dlOPERATION     0x00000010
#define dlDETAIL        0x00000020
#define dlDETAIL2       0x00000040
#define dlALL           0x00FFFFFF

#define dlINDENT1       0x01000000
#define dlINDENT2       0x02000000
#define dlINDENT3       0x03000000
#define dlRESERVED      0xFF000000

#ifdef DEBUG
#define dlDEFAULT       (dlSTANDARD | dlERROR | dlWARNING | dlCONNECTION | dlOPERATION | dlDETAIL)
#else
#define dlDEFAULT       (dlSTANDARD | dlERROR | dlCONNECTION)
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

extern void cdecl Print (DWORD dwLevel, LPTSTR pszLine, ...);
extern void cdecl Print (LPTSTR pszLine, ...);

extern DWORD GetPrintDetailLevel (DWORD dwLevel);
extern void SetPrintDetailLevel (DWORD dwLevel);


#endif // TAAFSADMSVRDEBUG_H
