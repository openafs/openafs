#ifndef GENERAL_H
#define GENERAL_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

/*
 *** InterlockedIncrementByWindow
 *** InterlockedDecrementByWindow
 *
 * Associates a zero-initialized LONG with an HWND, and calls Interlocked*()
 * on that LONG.
 *
 */

LONG InterlockedIncrementByWindow (HWND hWnd);
LONG InterlockedDecrementByWindow (HWND hWnd);


#endif

