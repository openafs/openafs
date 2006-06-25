/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <WINNT/al_wizard.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define REFRESH_LEFT_PANE   0x00000001
#define REFRESH_RIGHT_PANE  0x00000002

#define cxRECT(_r)  ((_r).right - (_r).left)
#define cyRECT(_r)  ((_r).bottom - (_r).top)

#define clrWASH_SOLID       RGB(0,0,128)
#define clrWASH_BRIGHTEST   RGB(0,0,252)
#define clrWASH_DARKEST     RGB(0,0,4)
#define clrWASH_INCREMENT   RGB(0,0,4)
#define clrWASH_TEXT_BG     RGB(0,0,0)
#define clrWASH_TEXT_FG     RGB(255,255,255)

#define ToPALETTERGB(_rgb)  PALETTERGB(GetRValue(_rgb),GetGValue(_rgb),GetBValue(_rgb))

#define cxLEFT_BACKGROUND    20
#define cxRIGHT_BACKGROUND   20
#define cyTOP_BACKGROUND     20
#define cyBOTTOM_BACKGROUND  20

#define xTEXT                30
#define yTEXT                30
#define cxSHADOW              2
#define cySHADOW              2

#define cptWASH_TEXT_SIZE    20


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

static void MoveRect (RECT *_pr, LONG _dx, LONG _dy);
static void GetRectInParent (HWND hWnd, RECT *pr);


/*
 * ROUTINES ___________________________________________________________________
 *
 */


WIZARD::WIZARD (void)
{
   m_iddTemplate = -1;
   m_idcLeftPane = -1;
   m_idcRightPane = -1;
   m_idcBack = -1;
   m_idcNext = -1;
   m_idbGraphic16 = -1;
   m_bmpGraphic16 = NULL;
   m_idbGraphic256 = -1;
   m_bmpGraphic256 = NULL;
   m_palGraphic = NULL;
   m_aStates = NULL;
   m_cStates = 0;
   m_stCurrent = -1;
   m_fShowing = FALSE;
   m_hWnd = NULL;

   m_iddBackground = -1;
   m_hBkg = NULL;
   m_fBlue = TRUE;
   m_fnBackground = NULL;
   m_szBackground[0] = TEXT('\0');
   m_bmpBackground = NULL;
   m_pfnCallback = NULL;
   m_bmpBuffer = NULL;
   SetRectEmpty (&m_rBuffer);

   HDC hdc = GetDC (NULL);
   LOGFONT lf;
   memset (&lf, 0x00, sizeof(lf));
   GetObject (GetStockObject (DEFAULT_GUI_FONT), sizeof(lf), &lf);
   lf.lfHeight = -MulDiv (cptWASH_TEXT_SIZE, GetDeviceCaps (hdc, LOGPIXELSY), 72);
   lf.lfWidth = 0;
   m_hfBackground = CreateFontIndirect (&lf);
   ReleaseDC (NULL, hdc);

   GeneratePalette();
}


WIZARD::~WIZARD (void)
{
   if (m_hWnd && IsWindow (m_hWnd))
      DestroyWindow (m_hWnd);
   if (m_bmpGraphic16 != NULL)
      DeleteObject (m_bmpGraphic16);
   if (m_bmpGraphic256 != NULL)
      DeleteObject (m_bmpGraphic256);
   if (m_bmpBuffer != NULL)
      DeleteObject (m_bmpBuffer);
   if (m_palGraphic != NULL)
      DeleteObject (m_palGraphic);
   if (m_bmpBackground != NULL)
      DeleteObject (m_bmpBackground);
   if (m_hfBackground != NULL)
      DeleteObject (m_hfBackground);
}


HWND WIZARD::GetWindow (void)
{
   if (m_hWnd && IsWindow (m_hWnd))
      return m_hWnd;
   return NULL;
}


LPWIZARD WIZARD::GetWizard (HWND hWnd)
{
   LPWIZARD pWiz = NULL;

   try {
      if ((pWiz = (LPWIZARD)GetWindowLongPtr (hWnd, DWLP_USER)) != NULL)
         {
         if ( (pWiz->m_hWnd != hWnd) && (pWiz->m_hBkg != hWnd) )
            pWiz = NULL;
         }
   } catch(...) {
      pWiz = NULL;
   }

   return pWiz;
}


void WIZARD::SetDialogTemplate (int iddTemplate, int idcLeftPane, int idcRightPane, int idcBack, int idcNext)
{
   m_iddTemplate = iddTemplate;
   m_idcLeftPane = idcLeftPane;
   m_idcRightPane = idcRightPane;
   m_idcBack = idcBack;
   m_idcNext = idcNext;

   if (m_fShowing)
      {
      Show (FALSE);
      Show (TRUE);
      }
}


void WIZARD::SetGraphic (int idbGraphic16, int idbGraphic256)
{
   LPRGBQUAD pargb = NULL;
   RGBQUAD argb[256];

   m_idbGraphic16 = idbGraphic16;
   m_idbGraphic256 = idbGraphic256;

   if (m_bmpGraphic16 != NULL)
      {
      DeleteObject (m_bmpGraphic16);
      m_bmpGraphic16 = NULL;
      }

   if (m_bmpGraphic256 != NULL)
      {
      DeleteObject (m_bmpGraphic256);
      m_bmpGraphic256 = NULL;
      }

   if (m_palGraphic != NULL)
      {
      DeleteObject (m_palGraphic);
      m_palGraphic = NULL;
      }

   m_bmpGraphic16 = (HBITMAP)TaLocale_LoadImage (idbGraphic16, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);

   if ((m_bmpGraphic256 = (HBITMAP)TaLocale_LoadImage (idbGraphic256, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)) != NULL)
      {
      BITMAP bm;
      GetObject (m_bmpGraphic256, sizeof(BITMAP), &bm);

      if ((bm.bmBitsPixel * bm.bmPlanes) == 8)
         {
         HDC hdc = CreateCompatibleDC (NULL);
         HBITMAP bmpOld = (HBITMAP)SelectObject (hdc, m_bmpGraphic256);

         GetDIBColorTable (hdc, 0, 256, argb);
         pargb = argb;

         SelectObject (hdc, bmpOld);
         DeleteDC (hdc);
         }
      }

   GeneratePalette (pargb);

   if (m_fShowing)
      {
      Refresh (REFRESH_LEFT_PANE);
      }
}


void WIZARD::SetGraphicCallback (void (CALLBACK *pfnCallback)(LPWIZARD pWiz, HDC hdcTarget, LPRECT prTarget, HPALETTE hPal))
{
   m_pfnCallback = pfnCallback;

   if (m_fShowing)
      {
      Refresh (REFRESH_LEFT_PANE);
      }
}


void WIZARD::SetStates (LPWIZARD_STATE aStates, size_t cStates)
{
   m_aStates = aStates;
   m_cStates = cStates;

   if (m_fShowing)
      {
      Refresh (REFRESH_RIGHT_PANE);
      }
}


int WIZARD::GetState (void)
{
   return m_stCurrent;
}


void WIZARD::SetState (int stNew, BOOL fForce)
{
   SendStateCommand (m_stCurrent, wcSTATE_LEAVE);

   int stOriginal = m_stCurrent;
   m_stCurrent = stNew;

   if (!fForce && SendStateCommand (stNew, wcIS_STATE_DISABLED))
      {
      if (stOriginal <= stNew)
         {
         int st;
         for (st = stNew+1; st < (int)m_cStates; ++st)
            {
            LPWIZARD_STATE pState;
            if ((pState = FindState (st)) != NULL)
               {
               m_stCurrent = st;
               if (!SendStateCommand (st, wcIS_STATE_DISABLED))
                  break;
               m_stCurrent = stOriginal;
               SendStateCommand (st, wcSTATE_ENTER);
               SendStateCommand (st, wcSTATE_LEAVE);
               }
            }
         stNew = (st < (int)m_cStates) ? st : m_stCurrent;
         }
      else // (moving backwards?)
         {
         int st;
         for (st = stNew-1; st >= 0; --st)
            {
            LPWIZARD_STATE pState;
            if ((pState = FindState (st)) != NULL)
               {
               m_stCurrent = st;
               if (!SendStateCommand (st, wcIS_STATE_DISABLED))
                  break;
               m_stCurrent = stOriginal;
               SendStateCommand (st, wcSTATE_ENTER);
               SendStateCommand (st, wcSTATE_LEAVE);
               }
            }
         stNew = (st >= 0) ? st : m_stCurrent;
         }
      }

   m_stCurrent = stNew;

   SendStateCommand (m_stCurrent, wcSTATE_ENTER);

   if (m_fShowing)
      {
      Refresh (REFRESH_RIGHT_PANE);
      }
}


BOOL WIZARD::Show (BOOL fShowReq)
{
   if (m_fShowing && !fShowReq)
      {
      m_fShowing = FALSE;
      if (m_hWnd && IsWindow (m_hWnd))
         DestroyWindow (m_hWnd);
      m_hWnd = NULL;
      }
   else if (!m_fShowing && fShowReq)
      {
      if (m_iddTemplate == -1)
         return FALSE;
      if (m_idcLeftPane == -1)
         return FALSE;
      if (m_idcRightPane == -1)
         return FALSE;
      if (m_idcBack == -1)
         return FALSE;
      if (m_idcNext == -1)
         return FALSE;

      if ((m_hWnd = ModelessDialogParam (m_iddTemplate, m_hBkg, (DLGPROC)WIZARD::Template_DlgProc, (LPARAM)this)) == NULL)
         return FALSE;
      m_fShowing = TRUE;

      MSG msg;
      while (GetMessage (&msg, 0, 0, NULL))
         {
         if (!IsDialogMessage (m_hWnd, &msg))
            {
            TranslateMessage (&msg);
            DispatchMessage (&msg);
            }

         if (!m_fShowing || !m_hWnd || !IsWindow (m_hWnd))
            break;
         }
      }

   return TRUE;
}


void WIZARD::EnableButton (int idcButton, BOOL fEnable)
{
   EnableWindow (GetDlgItem (m_hWnd, idcButton), fEnable);
}


void WIZARD::EnableButtons (DWORD dwButtonFlags)
{
   EnableWindow (GetDlgItem (m_hWnd, m_idcBack), (dwButtonFlags & BACK_BUTTON) ? TRUE : FALSE);
   EnableWindow (GetDlgItem (m_hWnd, m_idcNext), (dwButtonFlags & NEXT_BUTTON) ? TRUE : FALSE);
}


void WIZARD::SetButtonText (int idcButton, int idsText)
{
   HWND hButton;
   if ((hButton = GetDlgItem (m_hWnd, idcButton)) != NULL)
      {
      TCHAR szText[ cchRESOURCE ];
      GetString (szText, idsText);
      SetWindowText (hButton, szText);
      }
}


void WIZARD::SetDefaultControl (int idc)
{
   HWND hControl;
   if ((hControl = GetDlgItem (m_hWnd, idc)) == NULL)
      {
      HWND hRHS;
      if ((hRHS = GetRightHandWindow()) != NULL)
         hControl = GetDlgItem (hRHS, idc);
      }

   if (hControl)
      {
      PostMessage (m_hWnd, WM_NEXTDLGCTL, (WPARAM)hControl, TRUE);
      }
}


void WIZARD::SetBackground (int iddBackground, BOOL fBlue, DLGPROC dlgproc)
{
   m_iddBackground = iddBackground;
   m_fBlue = fBlue;
   m_fnBackground = dlgproc;

   if (m_hBkg && IsWindow (m_hBkg))
      {
      ShowBackground (FALSE);
      ShowBackground (TRUE);
      }
}


void WIZARD::SetBackgroundText (int idsText, HFONT hf)
{
   GetString (m_szBackground, idsText);

   if ((hf != NULL) && (hf != m_hfBackground))
      {
      if (m_hfBackground != NULL)
         DeleteObject (m_hfBackground);
      m_hfBackground = hf;
      }

   if (m_hBkg && IsWindow (m_hBkg))
      {
      Background_OnSize();
      }
}


void WIZARD::SetBackgroundText (LPTSTR pszText, HFONT hf)
{
   if (!pszText)
      m_szBackground[0] = TEXT('\0');
   else
      lstrcpy (m_szBackground, pszText);

   if ((hf != NULL) && (hf != m_hfBackground))
      {
      if (m_hfBackground != NULL)
         DeleteObject (m_hfBackground);
      m_hfBackground = hf;
      }

   if (m_hBkg && IsWindow (m_hBkg))
      {
      Background_OnSize();
      }
}


BOOL WIZARD::ShowBackground (BOOL fShow)
{
   if (!fShow && m_hBkg && IsWindow (m_hBkg))
      {
      DestroyWindow (m_hBkg);
      m_hBkg = NULL;
      }
   else if (fShow && !(m_hBkg && IsWindow (m_hBkg)))
      {
      if ((m_hBkg = ModelessDialogParam (m_iddBackground, NULL, (DLGPROC)WIZARD::Background_DlgProc, (LPARAM)this)) == NULL)
         return FALSE;

      RECT rDesktop;
      SystemParametersInfo (SPI_GETWORKAREA, 0, &rDesktop, 0);

      WINDOWPLACEMENT wpl;
      wpl.length = sizeof(wpl);
      wpl.flags = 0;
      wpl.showCmd = (GetWindowLong (m_hBkg, GWL_STYLE) & WS_MAXIMIZE) ? SW_SHOWMAXIMIZED : SW_SHOW;
      wpl.ptMinPosition.x = 0;
      wpl.ptMinPosition.y = 0;
      wpl.ptMaxPosition.x = 0;
      wpl.ptMaxPosition.y = 0;
      wpl.rcNormalPosition.left = rDesktop.left + cxLEFT_BACKGROUND;
      wpl.rcNormalPosition.right = rDesktop.right - cxRIGHT_BACKGROUND;
      wpl.rcNormalPosition.top = rDesktop.top + cyTOP_BACKGROUND;
      wpl.rcNormalPosition.bottom = rDesktop.bottom - cyBOTTOM_BACKGROUND;
      SetWindowPlacement (m_hBkg, &wpl);
      }

   return TRUE;
}


HWND WIZARD::GetBackgroundWindow (void)
{
   return m_hBkg;
}


void WIZARD::Refresh (DWORD dwRefFlags)
{
   if (dwRefFlags & REFRESH_LEFT_PANE)
      {
      HWND hLHS;
      if ((hLHS = GetDlgItem (m_hWnd, m_idcLeftPane)) != NULL)
         {
         RECT rr;
         GetClientRect (hLHS, &rr);
         InvalidateRect (hLHS, &rr, TRUE);
         UpdateWindow (hLHS);
         }
      }

   if (dwRefFlags & REFRESH_RIGHT_PANE)
      {
      HWND hRHS;
      if ((hRHS = GetDlgItem (m_hWnd, m_idcRightPane)) != NULL)
         {
         HWND hOld = GetRightHandWindow();

         LPWIZARD_STATE pState;
         if ((pState = FindState (m_stCurrent)) != NULL)
            {
            int stCurrent = m_stCurrent;

            HWND hNew;
            if ((hNew = ModelessDialogParam (pState->idd, m_hWnd, pState->dlgproc, pState->lp)) != NULL)
               {
               if (stCurrent != m_stCurrent)
                  DestroyWindow (hNew);
               else
                  {
                  RECT rRHS;
                  GetRectInParent (hRHS, &rRHS);
                  SetWindowPos (hNew, NULL, rRHS.left, rRHS.top, cxRECT(rRHS), cyRECT(rRHS), SWP_NOZORDER | SWP_NOACTIVATE);
                  SetWindowLong (hNew, GWL_ID, pState->idd);

                  ShowWindow (hNew, SW_SHOW);
                  ShowWindow (hRHS, SW_HIDE);
                  }
               }
            }

         if (hOld != NULL)
            DestroyWindow (hOld);
         }
      }
}


HWND WIZARD::GetRightHandWindow (void)
{
   if (!m_fShowing || !m_hWnd || !IsWindow (m_hWnd))
      return NULL;

   HWND hRHS = NULL;

   for (HWND hFound = ::GetWindow (m_hWnd, GW_CHILD);
        hFound != NULL;
        hFound = ::GetWindow (hFound, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];
      if (GetClassName (hFound, szClassName, cchRESOURCE))
         {
         if (!lstrcmp (szClassName, TEXT("#32770"))) // WC_DIALOG
            {
            if (!hRHS || IsWindowVisible(hRHS))
               hRHS = hFound;
            }
         }
      }

   return hRHS;
}


void WIZARD::GeneratePalette (LPRGBQUAD argb)
{
   HDC hdc = GetDC (NULL);
   WORD wDepthDisplay = (WORD)GetDeviceCaps (hdc, BITSPIXEL);
   ReleaseDC (NULL, hdc);

   if (wDepthDisplay == 8)
      {
      BYTE aPalBuffer[ sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY) ];
      LPLOGPALETTE pPal = (LPLOGPALETTE)aPalBuffer;
      pPal->palVersion = 0x300;
      pPal->palNumEntries = 256;

      size_t ii;
      for (ii = 0; ii < 256; ++ii)
         {
         pPal->palPalEntry[ ii ].peRed   = (argb) ? argb[ ii ].rgbRed : 0;
         pPal->palPalEntry[ ii ].peGreen = (argb) ? argb[ ii ].rgbGreen : 0;
         pPal->palPalEntry[ ii ].peBlue  = (argb) ? argb[ ii ].rgbBlue : 0;
         pPal->palPalEntry[ ii ].peFlags = 0;
         }

      for (COLORREF clr = clrWASH_DARKEST; clr <= clrWASH_BRIGHTEST; clr += clrWASH_INCREMENT)
         {            
         for (ii = 0; ii < 256; ++ii)
            {
            if ( (pPal->palPalEntry[ ii ].peRed   == GetRValue (clr)) &&
                 (pPal->palPalEntry[ ii ].peGreen == GetGValue (clr)) &&
                 (pPal->palPalEntry[ ii ].peBlue  == GetBValue (clr)) )
               break;
            }
         if (ii == 256)
            {
            for (ii = 10; ii < 246; ++ii)
               {
               if ( (pPal->palPalEntry[ ii ].peRed == 0) &&
                    (pPal->palPalEntry[ ii ].peGreen == 0) &&
                    (pPal->palPalEntry[ ii ].peBlue == 0) )
                  break;
               if ( (pPal->palPalEntry[ ii ].peRed == 255) &&
                    (pPal->palPalEntry[ ii ].peGreen == 255) &&
                    (pPal->palPalEntry[ ii ].peBlue == 255) )
                  break;
               }
            if (ii == 246)
               break;

            pPal->palPalEntry[ ii ].peRed   = GetRValue (clr);
            pPal->palPalEntry[ ii ].peGreen = GetGValue (clr);
            pPal->palPalEntry[ ii ].peBlue  = GetBValue (clr);
            }
         }

      m_palGraphic = CreatePalette (pPal);
      }
}


LPWIZARD_STATE WIZARD::FindState (int stFind)
{
   for (size_t ii = 0; ii < m_cStates; ++ii)
      {
      if (m_aStates[ ii ].st == stFind)
         return &m_aStates[ ii ];
      }
   return NULL;
}


BOOL WIZARD::SendStateCommand (int st, WIZARD_COMMAND wc)
{
   BOOL rc = FALSE;

   LPWIZARD_STATE pState;
   if ((pState = FindState (st)) != NULL)
      {
      rc = (BOOL)CallWindowProc ((WNDPROC)(pState->dlgproc), NULL, WM_COMMAND, MAKEWPARAM(IDC_WIZARD,(WORD)wc), (LPARAM)this);
      }

   return rc;
}


BOOL CALLBACK WIZARD::Template_DlgProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hWnd, DWLP_USER, lp);

   LPWIZARD pWiz = (LPWIZARD)GetWindowLongPtr (hWnd, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG:
         if (pWiz)
            pWiz->Template_OnInitDialog (hWnd);
         break;

      case WM_DESTROY:
         if (pWiz && (pWiz->m_hWnd == hWnd))
            pWiz->Show (FALSE);
         break;

      case WM_COMMAND:
         return pWiz->Template_ForwardMessage (hWnd, msg, wp, lp);
      }

   return FALSE;
}


BOOL CALLBACK WIZARD::Template_LeftPaneHook (HWND hLHS, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hLHS, WIZARD::Template_LeftPaneHook);

   switch (msg)
      {
      case WM_PAINT:
         LPWIZARD pWiz;
         if ((pWiz = WIZARD::GetWizard (GetParent (hLHS))) != NULL)
            {
            if (pWiz->Template_OnPaintLHS (hLHS))
               return TRUE;
            }
         break;

      case WM_DESTROY:
         Subclass_RemoveHook (hLHS, WIZARD::Template_LeftPaneHook);
         break;
      }

   if (oldProc)
      return (BOOL)CallWindowProc ((WNDPROC)oldProc, hLHS, msg, wp, lp);
   else
      return (BOOL)DefWindowProc (hLHS, msg, wp, lp);
}


BOOL WIZARD::Template_ForwardMessage (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   HWND hRHS;
   if ((hRHS = GetRightHandWindow()) != NULL)
      return (BOOL)SendMessage (hRHS, msg, wp, lp);

   LPWIZARD_STATE pState;
   if ((pState = FindState (m_stCurrent)) != NULL)
      return (BOOL)CallWindowProc ((WNDPROC)(pState->dlgproc), hWnd, msg, wp, lp);

   return FALSE;
}


void WIZARD::Template_OnInitDialog (HWND hWnd)
{
   m_hWnd = hWnd;
   m_fShowing = TRUE;

   HWND hLHS;
   if ((hLHS = GetDlgItem (m_hWnd, m_idcLeftPane)) != NULL)
      {
      Subclass_AddHook (hLHS, WIZARD::Template_LeftPaneHook);
      }

   HWND hRHS;
   if ((hRHS = GetDlgItem (m_hWnd, m_idcRightPane)) != NULL)
      {
      ShowWindow (hRHS, SW_HIDE);
      }

   Refresh (REFRESH_RIGHT_PANE);

   ShowWindow (m_hWnd, SW_SHOW);
}


BOOL WIZARD::Template_OnPaintLHS (HWND hLHS)
{
   BOOL fShow16 = FALSE;
   BOOL fShow256 = FALSE;

   HDC hdc = GetDC (NULL);
   WORD wDepthDisplay = (WORD)GetDeviceCaps (hdc, BITSPIXEL);
   ReleaseDC (NULL, hdc);

   if ( (m_bmpGraphic256 != NULL) && (wDepthDisplay >= 8) )
      fShow256 = TRUE;
   else if (m_bmpGraphic16 != NULL)
      fShow16 = TRUE;
   else
      return FALSE;

   PAINTSTRUCT ps;
   HDC hdcTarget;
   if ((hdcTarget = BeginPaint (hLHS, &ps)) != NULL)
      {
      HBITMAP bmpSource = (fShow256) ? m_bmpGraphic256 : m_bmpGraphic16;

      BITMAP bm;
      GetObject (bmpSource, sizeof(BITMAP), &bm);

      RECT rWindow;
      GetClientRect (hLHS, &rWindow);

      RECT rSource = { 0, 0, bm.bmWidth, bm.bmHeight };
      RECT rTarget = rWindow;

      if (cxRECT(rSource) > cxRECT(rTarget))
         {
         MoveRect (&rSource, (cxRECT(rSource) - cxRECT(rTarget)) / 2, 0);
         rSource.right = rSource.left + cxRECT(rTarget);
         }
      else if (cxRECT(rSource) < cxRECT(rTarget))
         {
         MoveRect (&rTarget, (cxRECT(rTarget) - cxRECT(rSource)) / 2, 0);
         }

      if (cyRECT(rSource) > cyRECT(rTarget))
         {
         MoveRect (&rSource, 0, (cyRECT(rSource) - cyRECT(rTarget)) / 2);
         rSource.bottom = rSource.top + cyRECT(rTarget);
         }
      else if (cyRECT(rSource) < cyRECT(rTarget))
         {
         MoveRect (&rTarget, 0, (cyRECT(rTarget) - cyRECT(rSource)) / 2);
         }
      rTarget.right = rTarget.left + cxRECT(rSource);
      rTarget.bottom = rTarget.top + cyRECT(rSource);

      // If the user has supplied a custom draw-proc, then we should
      // do our rendering to an off-screen bitmap.
      //
      HDC hdcFinalTarget = NULL;
      HBITMAP bmpTempTarget = NULL;
      if (m_pfnCallback)
         {
         // First make sure our offscreen buffer is large enough
         //
         if (!m_bmpBuffer || (cxRECT(m_rBuffer) < cxRECT(rWindow)) || (cyRECT(m_rBuffer) < cyRECT(rWindow)))
            {
            if (m_bmpBuffer != NULL)
               DeleteObject (m_bmpBuffer);
            if ((m_bmpBuffer = CreateCompatibleBitmap (hdcTarget, cxRECT(rWindow), cyRECT(rWindow))) != NULL)
               {
               m_rBuffer.right = cxRECT(rWindow);  // m_rBuffer.left=already 0
               m_rBuffer.bottom = cyRECT(rWindow); // m_rBuffer.top=already 0
               }
            }

         // Then set up to double-buffer, if possible
         //
         if (m_bmpBuffer)
            {
            hdcFinalTarget = hdcTarget;
            hdcTarget = CreateCompatibleDC (hdcFinalTarget);
            bmpTempTarget = (HBITMAP)SelectObject (hdcTarget, m_bmpBuffer);
            }
         }

      HDC hdcSource = CreateCompatibleDC (hdcTarget);
      HBITMAP bmpOld = (HBITMAP)SelectObject (hdcSource, bmpSource);
      HPALETTE palOld = NULL;
      if ((wDepthDisplay == 8) && (m_palGraphic != NULL) && (fShow256))
         {
         palOld = SelectPalette (hdcTarget, m_palGraphic, FALSE);
         RealizePalette (hdcTarget);
         }

      if ( (bm.bmWidth < cxRECT(rWindow)) || (bm.bmHeight < cyRECT(rWindow)) )
         {
         COLORREF clrFill = GetPixel (hdcSource, 0, rSource.bottom -1);
         clrFill = ToPALETTERGB(clrFill);
         HBRUSH hbrFill = CreateSolidBrush (clrFill);

         if (bm.bmWidth < cxRECT(rWindow))
            {
            RECT rr;
            rr = rWindow;
            rr.right = rTarget.left;
            FillRect (hdcTarget, &rr, hbrFill);

            rr = rWindow;
            rr.left = rTarget.right;
            FillRect (hdcTarget, &rr, hbrFill);
            }

         if (bm.bmHeight < cyRECT(rWindow))
            {
            RECT rr;
            rr = rWindow;
            rr.bottom = rTarget.top;
            FillRect (hdcTarget, &rr, hbrFill);

            rr = rWindow;
            rr.top = rTarget.bottom;
            FillRect (hdcTarget, &rr, hbrFill);
            }

         DeleteObject (hbrFill);
         }

      BitBlt (hdcTarget, rTarget.left, rTarget.top, cxRECT(rTarget), cyRECT(rTarget), hdcSource, rSource.left, rSource.top, SRCCOPY);

      // Call the user-supplied callback function (if there is one)
      //
      if (m_pfnCallback)
         {
         (*m_pfnCallback)(this, hdcTarget, &rWindow, (palOld) ? m_palGraphic : NULL);
         }

      if (palOld != NULL)
         SelectPalette (hdcTarget, palOld, FALSE);

      // If we've been drawing to an off-screen bitmap, blit the result to
      // the display.
      //
      if (hdcFinalTarget)
         {
         BitBlt (hdcFinalTarget, rWindow.left, rWindow.top, cxRECT(rWindow), cyRECT(rWindow), hdcTarget, 0, 0, SRCCOPY);
         SelectObject (hdcTarget, bmpTempTarget);
         DeleteDC (hdcTarget);
         hdcTarget = hdcFinalTarget;
         }

      SelectObject (hdcSource, bmpOld);
      DeleteDC (hdcSource);

      EndPaint (hLHS, &ps);
      }

   return TRUE;
}


void MoveRect (RECT *_pr, LONG _dx, LONG _dy)
{
   _pr->left += _dx;
   _pr->right += _dx;
   _pr->top += _dy;
   _pr->bottom += _dy;
}


void GetRectInParent (HWND hWnd, RECT *pr)
{
   POINT pt;

   GetWindowRect (hWnd, pr);

   pr->right -= pr->left;
   pr->bottom -= pr->top;	// right/bottom == width/height for now

   pt.x = pr->left;
   pt.y = pr->top;

   ScreenToClient (GetParent (hWnd), &pt);

   pr->left = pt.x;
   pr->top = pt.y;
   pr->right += pr->left;
   pr->bottom += pr->top;
}


BOOL CALLBACK WIZARD::Background_DlgProc (HWND hBkg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hBkg, DWLP_USER, lp);

   LPWIZARD pWiz = (LPWIZARD)GetWindowLongPtr (hBkg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG:
         if (pWiz)
            pWiz->Background_OnInitDialog (hBkg);
         break;

      case WM_SIZE:
         if (pWiz)
            pWiz->Background_OnSize();
         break;

      case WM_DESTROY:
         if (pWiz)
            pWiz->Background_OnDestroy();
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
               if (pWiz)
                  pWiz->Background_OnClose();
               else
                  DestroyWindow (hBkg);
               break;
            }
         break;
      }

   if (pWiz && pWiz->m_fnBackground)
      {
      if (CallWindowProc ((WNDPROC)(pWiz->m_fnBackground), hBkg, msg, wp, lp))
         return TRUE;
      }

   return FALSE;
}


BOOL CALLBACK WIZARD::Background_PaintHook (HWND hBkg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hBkg, WIZARD::Background_PaintHook);

   switch (msg)
      {
      case WM_PAINT:
         LPWIZARD pWiz;
         if ((pWiz = WIZARD::GetWizard (hBkg)) != NULL)
            {
            if (pWiz->Background_OnPaint())
               return TRUE;
            }
         break;

      case WM_DESTROY:
         Subclass_RemoveHook (hBkg, WIZARD::Background_PaintHook);
         break;
      }

   if (oldProc)
      return (BOOL)CallWindowProc ((WNDPROC)oldProc, hBkg, msg, wp, lp);
   else
      return (BOOL)DefWindowProc (hBkg, msg, wp, lp);
}



void WIZARD::Background_OnInitDialog (HWND hBkg)
{
   m_hBkg = hBkg;
   Background_OnSize();
   Subclass_AddHook (m_hBkg, (DLGPROC)WIZARD::Background_PaintHook);
}


void WIZARD::Background_OnSize (void)
{
   if (m_bmpBackground)
      {
      DeleteObject (m_bmpBackground);
      m_bmpBackground = NULL;
      }

   RECT rr;
   GetClientRect (m_hBkg, &rr);

   if (m_fBlue)
      {
      HDC hdc = GetDC (NULL);
      WORD wDepthDisplay = (WORD)GetDeviceCaps (hdc, BITSPIXEL);
      ReleaseDC (NULL, hdc);

      hdc = GetDC (m_hBkg);
      HDC hdcTarget = CreateCompatibleDC (hdc);

      if ((m_bmpBackground = CreateCompatibleBitmap (hdc, cxRECT(rr), cyRECT(rr))) != NULL)
         {
         HBITMAP bmpOld = (HBITMAP)SelectObject (hdcTarget, m_bmpBackground);

         HPALETTE palOld = NULL;
         if (m_palGraphic)
            {
            palOld = SelectPalette (hdcTarget, m_palGraphic, FALSE);
            RealizePalette (hdcTarget);
            }

         size_t yy = 0;
         size_t cy = cyRECT(rr) / ((clrWASH_BRIGHTEST - clrWASH_DARKEST) / clrWASH_INCREMENT);
         for (COLORREF clr = clrWASH_BRIGHTEST; clr >= clrWASH_DARKEST; clr -= clrWASH_INCREMENT)
            {
            RECT rSection = rr;
            rSection.top = (LONG)yy;
            rSection.bottom = (LONG)(yy +cy);
            HBRUSH hbr = CreateSolidBrush (ToPALETTERGB(clr));
            FillRect (hdcTarget, &rSection, hbr);
            DeleteObject (hbr);
            yy += cy;
            }

         if (m_szBackground[0] != TEXT('\0'))
            {
            HFONT hfOld = (HFONT)SelectObject (hdcTarget, m_hfBackground);
            COLORREF clrOld = SetTextColor (hdcTarget, clrWASH_TEXT_BG);
            SetBkMode (hdcTarget, TRANSPARENT);

            RECT rText = rr;
            rText.left += xTEXT + cxSHADOW;
            rText.top += yTEXT + cySHADOW;
            DrawTextEx (hdcTarget, m_szBackground, lstrlen(m_szBackground), &rText, DT_NOPREFIX | DT_LEFT, NULL);

            rText.left -= cxSHADOW;
            rText.top -= cySHADOW;
            SetTextColor (hdcTarget, clrWASH_TEXT_FG);
            DrawTextEx (hdcTarget, m_szBackground, lstrlen(m_szBackground), &rText, DT_NOPREFIX | DT_LEFT, NULL);

            SetTextColor (hdcTarget, clrOld);
            SelectObject (hdcTarget, hfOld);
            }

         if (palOld)
            SelectPalette (hdcTarget, palOld, FALSE);

         SelectObject (hdcTarget, bmpOld);
         }

      DeleteDC (hdcTarget);
      ReleaseDC (m_hBkg, hdc);
      }

   InvalidateRect (m_hBkg, &rr, TRUE);
   UpdateWindow (m_hBkg);
}


void WIZARD::Background_OnDestroy (void)
{
   if (m_bmpBackground)
      {
      DeleteObject (m_bmpBackground);
      m_bmpBackground = NULL;
      }
}


void WIZARD::Background_OnClose (void)
{
   LPWIZARD_STATE pState;

   if (m_hWnd && IsWindow (m_hWnd))
      {
      PostMessage (m_hWnd, WM_COMMAND, MAKEWPARAM(IDCANCEL,BN_CLICKED), (LPARAM)GetDlgItem(m_hWnd,IDCANCEL));
      }
   else if ((pState = FindState (m_stCurrent)) != NULL)
      {
      CallWindowProc ((WNDPROC)(pState->dlgproc), m_hBkg, WM_COMMAND, MAKEWPARAM(IDCANCEL,BN_CLICKED), 0);
      }
   else
      {
      ShowBackground (FALSE);
      }
}


BOOL WIZARD::Background_OnPaint (void)
{
   if (!m_bmpBackground)
      return FALSE;
      
   PAINTSTRUCT ps;
   HDC hdcTarget;
   if ((hdcTarget = BeginPaint (m_hBkg, &ps)) != NULL)
      {
      BITMAP bm;
      GetObject (m_bmpBackground, sizeof(BITMAP), &bm);

      HDC hdcSource = CreateCompatibleDC (hdcTarget);
      HBITMAP bmpOld = (HBITMAP)SelectObject (hdcSource, m_bmpBackground);
      HPALETTE palOld = NULL;
      if (m_palGraphic)
         {
         palOld = SelectPalette (hdcSource, m_palGraphic, FALSE);
         RealizePalette (hdcTarget);
         }

      BitBlt (hdcTarget, 0, 0, bm.bmWidth, bm.bmHeight, hdcSource, 0, 0, SRCCOPY);

      if (palOld != NULL)
         SelectPalette (hdcTarget, palOld, FALSE);
      SelectObject (hdcSource, bmpOld);
      DeleteDC (hdcSource);

      EndPaint (m_hBkg, &ps);
      }

   return TRUE;
}

