/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AL_WIZARD_H
#define AL_WIZARD_H

#include <windows.h>
#include <prsht.h>
#include <WINNT/TaLocale.h>
#include <WINNT/subclass.h>

#ifndef EXPORTED
#define EXPORTED
#endif


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef THIS_HINST
#define THIS_HINST   (HINSTANCE)GetModuleHandle(NULL)
#endif

#define IDC_WIZARD   898  // WM_COMMAND,IDC_WIZARD: HIWORD(wp)=WIZARD_COMAMND

typedef class EXPORTED WIZARD WIZARD, *LPWIZARD;


/*
 * WIZARD CLASS _______________________________________________________________
 *
 */

typedef enum
   {
   wcSTATE_ENTER,	// now entering state (even w/o display)
   wcSTATE_LEAVE,	// now leaving state (even w/o display)
   wcIS_STATE_DISABLED,	// return TRUE if state disabled
   } WIZARD_COMMAND;

typedef struct
   {
   int st;	// numeric state identifier
   int idd;	// dialog template for wizard pane
   DLGPROC dlgproc;	// dialog procedure for this state
   LPARAM lp;	// lparam for dialog initialization
   } WIZARD_STATE, *LPWIZARD_STATE;

#define BACK_BUTTON    0x0001
#define NEXT_BUTTON    0x0002

class EXPORTED WIZARD
   {
   public:

      WIZARD (void);
      ~WIZARD (void);

      static LPWIZARD GetWizard (HWND hWnd);
      HWND GetWindow (void);

      void SetDialogTemplate (int iddTemplate, int idcLeftPane, int idcRightPane, int idcBack, int idcNext);
      void SetGraphic (int idbGraphic16, int idbGraphic256);
      void SetStates (LPWIZARD_STATE aStates, size_t cStates);
      void SetGraphicCallback (void (CALLBACK *pfnCallback)(LPWIZARD pWiz, HDC hdcTarget, LPRECT prTarget, HPALETTE hPal));

      int GetState (void);
      void SetState (int stNew, BOOL fForce = FALSE);
      BOOL Show (BOOL fShow = TRUE);

      void SetBackground (int iddBackground, BOOL fBlue = TRUE, DLGPROC dlgproc = NULL);
      void SetBackgroundText (LPTSTR pszText, HFONT hfText = NULL);
      void SetBackgroundText (int idsText, HFONT hfText = NULL);
      BOOL ShowBackground (BOOL fShow = TRUE);
      HWND GetBackgroundWindow (void);

      void EnableButton (int idcButton, BOOL fEnable);
      void EnableButtons (DWORD dwButtonFlags = BACK_BUTTON | NEXT_BUTTON);
      void SetButtonText (int idcButton, int idsText);
      void SetDefaultControl (int idc);

      BOOL SendStateCommand (int st, WIZARD_COMMAND wc);

   private:

      void Refresh (DWORD dwRefFlags);
      HWND GetRightHandWindow (void);
      void GeneratePalette (RGBQUAD *pargb = NULL);
      LPWIZARD_STATE FindState (int stFind);

      static BOOL CALLBACK Background_DlgProc (HWND hBkg, UINT msg, WPARAM wp, LPARAM lp);
      static BOOL CALLBACK Background_PaintHook (HWND hBkg, UINT msg, WPARAM wp, LPARAM lp);
      void Background_OnInitDialog (HWND hBkg);
      void Background_OnSize (void);
      void Background_OnDestroy (void);
      void Background_OnClose (void);
      BOOL Background_OnPaint (void);

      static BOOL CALLBACK Template_DlgProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
      static BOOL CALLBACK Template_LeftPaneHook (HWND hLHS, UINT msg, WPARAM wp, LPARAM lp);
      BOOL Template_ForwardMessage (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
      void Template_OnInitDialog (HWND hWnd);
      BOOL Template_OnPaintLHS (HWND hLHS);

      int m_iddTemplate;
      int m_idcLeftPane;
      int m_idcRightPane;
      int m_idcBack;
      int m_idcNext;
      int m_idbGraphic16;
      HBITMAP m_bmpGraphic16;
      int m_idbGraphic256;
      HBITMAP m_bmpGraphic256;
      HPALETTE m_palGraphic;
      LPWIZARD_STATE m_aStates;
      size_t m_cStates;
      int m_stCurrent;
      BOOL m_fShowing;
      HWND m_hWnd;

      int m_iddBackground;
      BOOL m_fBlue;
      HWND m_hBkg;
      DLGPROC m_fnBackground;
      TCHAR m_szBackground[ cchRESOURCE ];
      HBITMAP m_bmpBackground;
      HFONT m_hfBackground;
      HBITMAP m_bmpBuffer;
      RECT m_rBuffer;

      void (CALLBACK *m_pfnCallback)(LPWIZARD pWiz, HDC hdcTarget, LPRECT prTarget, HPALETTE hPal);
   };


#endif

