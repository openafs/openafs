/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SUBCLASS_H
#define SUBCLASS_H

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef EXPORTED
#define EXPORTED
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 * These routines provide a consistent method for multiple children in
 * a dialog to add and remove subclasses, without stepping on each
 * others' toes.
 *
 * Old code like this:
 *
 *    // Install subclass on create
 *    LONG oldProc = GetWindowLong (hWnd, GWL_WNDPROC);
 *    SetWindowLong (hWnd, GWL_WNDPROC, (LONG)MyWindowProc);
 *
 *    // Use
 *    if (oldProc)
 *       CallWindowProc(oldProc,hWnd,msg,wp,lp);
 *    else
 *       DefWindowProc(hWnd,msg,wp,lp);
 *
 *    // Uninstall subclass on destroy
 *    SetWindowLong (hWnd, GWL_WNDPROC, (LONG)oldProc);
 *
 * Will eat itself unless all changes are peeled back in the opposite
 * order in which subclasses were installed.  This technique won't:
 *
 *    // Install subclass on create
 *    Subclass_AddHook (hWnd, MyWindowProc);
 *
 *    // Use
 *    PVOID oldProc = Subclass_FindNextHook (hWnd, MyWindowProc);
 *    if (oldProc)
 *       CallWindowProc(oldProc,hWnd,msg,wp,lp);
 *    else
 *       DefWindowProc(hWnd,msg,wp,lp);
 *
 *    // Uninstall subclass on destroy
 *    Subclass_RemoveHook (hWnd, MyWindowProc);
 *
 * Note that if five calls are made to add "Subclass_AddHook" giving
 * the same hTarget and wndProc, then five successive calls to _Remove
 * will be necessary to remove the hook; also note that the wndProc
 * will (of course) be called only *once* per message.
 *
 */

EXPORTED BOOL Subclass_AddHook (HWND hTarget, PVOID wndProc);
EXPORTED void Subclass_RemoveHook (HWND hTarget, PVOID wndProc);
EXPORTED PVOID Subclass_FindNextHook (HWND hTarget, PVOID wndProcMine);


#endif

