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

