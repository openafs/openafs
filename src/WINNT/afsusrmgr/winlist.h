#ifndef WINDOWLIST_H
#define WINDOWLIST_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   wltUSER_PROPERTIES,
   wltGROUP_PROPERTIES,
   wltCELL_PROPERTIES
   } WINDOWLISTTYPE;

#define ASID_ANY ((ASID)-1)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void WindowList_Add (HWND hWnd, WINDOWLISTTYPE wlt, ASID idObject = 0);
HWND WindowList_Search (WINDOWLISTTYPE wlt, ASID idObject = ASID_ANY);
void WindowList_Remove (HWND hWnd);


#endif

