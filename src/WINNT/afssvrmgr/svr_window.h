/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_WINDOW_H
#define SVR_WINDOW_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define SERVERWINDOW_PREVIEWPANE  g.hMain   /* pass this as hWnd below... */

typedef struct
   {
   LPIDENT lpi;
   RECT rWindow;
   BOOL fOpen;
   } SVR_SETWINDOWPOS_PARAMS, *LPSVR_SETWINDOWPOS_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Open (LPIDENT lpiServer, LPRECT prWindow);
void Server_Close (LPIDENT lpiServer);
void Server_CloseAll (BOOL fUserRequested);

void Server_PrepareTabControl (HWND hTab);

HWND Server_GetCurrentTab (HWND hWnd);
CHILDTAB Server_GetDisplayedTab (HWND hWnd);
void Server_SelectServer (HWND hDlg, LPIDENT lpiNew, BOOL fForceRedraw = FALSE);
LPIDENT Server_GetServer (HWND hWnd);
LPIDENT Server_GetServerForChild (HWND hChild);
HWND Server_GetWindowForChild (HWND hChild);
void Server_ForceRedraw (HWND hWnd);
void Server_DisplayTab (HWND hWnd, CHILDTAB iTab);
void Server_Uncover (HWND hWnd);

void Server_OnKey_Return (void);
void Server_OnKey_Menu (void);
void Server_OnKey_Esc (void);
void Server_OnKey_Properties (void);
void Server_OnKey_Tab (HWND hDlg, BOOL fForward);
void Server_OnKey_CtrlTab (HWND hDlg, BOOL fForward);


#endif

