#ifndef TAAFSADMSVRCLIENTNOTIFY_H
#define TAAFSADMSVRCLIENTNOTIFY_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL AddObjectNotification (HWND hNotify, ASID idCell, ASID idObject);
void ClearObjectNotifications (HWND hNotify);
void TestForNotifications (DWORD idClient, ASID idCell, ASID idObject = 0);
void NotifyObjectListeners (ASID idCell, ASID idObject);

BOOL SetActionNotification (HWND hNotify, BOOL fSet);
void NotifyActionListeners (LPASACTION pAction, BOOL fFinished);


#endif // TAAFSADMSVRCLIENTNOTIFY_H

