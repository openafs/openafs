/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef COMMAND_H
#define COMMAND_H


/*
 * POPUP MENUS ________________________________________________________________
 *
 */

typedef enum
   {
   pmUSER,
   pmGROUP,
   pmMACHINE
   } POPUPMENU;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void OnRightClick (POPUPMENU pm, HWND hList, POINT *pptScreen = NULL);

void OnContextCommand (WORD wCmd);


#endif

