/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRCLIENTNOTIFY_H
#define TAAFSADMSVRCLIENTNOTIFY_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL AddObjectNotification (HWND hNotify, ASID idCell, ASID idObject);
void ClearObjectNotifications (HWND hNotify);
void TestForNotifications (UINT_PTR idClient, ASID idCell, ASID idObject = 0);
void NotifyObjectListeners (ASID idCell, ASID idObject);

BOOL SetActionNotification (HWND hNotify, BOOL fSet);
void NotifyActionListeners (LPASACTION pAction, BOOL fFinished);


#endif // TAAFSADMSVRCLIENTNOTIFY_H

