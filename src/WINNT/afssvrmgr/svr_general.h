/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_GENERAL_H
#define SVR_GENERAL_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define perDEFAULT_AGGFULL_WARNING   90
#define perDEFAULT_SETFULL_WARNING   75

#define fDEFAULT_SVCSTOP_WARNING   TRUE


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen);
void Server_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiServer);

PVOID Server_LoadPreferences (LPIDENT lpiServer);
BOOL Server_SavePreferences (LPIDENT lpiServer);


#endif

