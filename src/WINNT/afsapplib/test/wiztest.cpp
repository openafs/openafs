/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <afsapplib.h>
#include <math.h>
#include <stdlib.h>
#include "resource.h"

         // Animation stuff
         //
#define fpsANIMATE_OVERLAY 20 // frames per second (rate to redraw window)
#define spcANIMATE_OVERLAY 20 // seconds per cycle (complete rotation for X)
#define spcANIMATE_COLORS   3 // seconds per cycle (color fade R->G->B->R)

VOID CALLBACK Wiz_ForceGraphicRedraw (HWND hWnd, UINT msg, UINT idTimer, DWORD idTick);
void CALLBACK Wiz_DrawOverlay (LPWIZARD pWiz, HDC hdcTarget, LPRECT prTarget, HPALETTE hPal);


         // Normal wizard stuff
         //
static enum {
   sidSTEP_ONE,
   sidSTEP_TWO,
   sidSTEP_THREE,
} StateID;

BOOL CALLBACK WizStep_1_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizStep_2_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK WizStep_3_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);

static WIZARD_STATE g_aStates[] = {
   { sidSTEP_ONE,    IDD_STEP1, (DLGPROC)WizStep_1_DlgProc, 0 },
   { sidSTEP_TWO,    IDD_STEP2, (DLGPROC)WizStep_2_DlgProc, 0 },
   { sidSTEP_THREE,  IDD_STEP3, (DLGPROC)WizStep_3_DlgProc, 0 }
};

static size_t g_cStates = sizeof(g_aStates) / sizeof(g_aStates[0]);


static LPWIZARD g_pWiz = NULL;
static BOOL g_fSkipStep2 = FALSE;


int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR pszCmdLineA, int nCmdShow)
{
   srand(GetTickCount());
   SetTimer (NULL, 100, 1000L / fpsANIMATE_OVERLAY, Wiz_ForceGraphicRedraw);

   g_pWiz = new WIZARD;
   g_pWiz->SetDialogTemplate (IDD_WIZARD, IDC_WIZARD_LEFTPANE, IDC_WIZARD_RIGHTPANE, IDBACK, IDNEXT);
   g_pWiz->SetGraphic (IDB_GRAPHIC_16, IDB_GRAPHIC_256);
   g_pWiz->SetGraphicCallback (Wiz_DrawOverlay);
   g_pWiz->SetStates (g_aStates, g_cStates);

   g_pWiz->SetState (sidSTEP_ONE);
   g_pWiz->Show ();

   delete g_pWiz;
   return 0;
}


BOOL CALLBACK WizStep_Common_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDHELP:
               Message (MB_OK, IDS_HELP_TITLE, IDS_HELP_DESC);
               return TRUE;

            case IDCANCEL:
               if (Message (MB_OKCANCEL, IDS_CANCEL_TITLE, IDS_CANCEL_DESC) == IDOK)
                  g_pWiz->Show(FALSE);
               return TRUE;
            }
         break;
      }

   return FALSE;
}


BOOL CALLBACK WizStep_1_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
{
   if (WizStep_Common_DlgProc (hRHS, msg, wp, lp))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         CheckDlgButton (hRHS, IDC_GOTO_TWO, !g_fSkipStep2);
         CheckDlgButton (hRHS, IDC_GOTO_THREE, g_fSkipStep2);

         g_pWiz->EnableButtons (NEXT_BUTTON);
         g_pWiz->SetButtonText (IDNEXT, IDS_NEXT);
         g_pWiz->SetDefaultControl (IDNEXT);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDNEXT:
               g_fSkipStep2 = IsDlgButtonChecked (hRHS, IDC_GOTO_THREE);
               g_pWiz->SetState (sidSTEP_TWO);
               break;
            }
         break;
      }

   return FALSE;
}


BOOL CALLBACK WizStep_2_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
{
   if (WizStep_Common_DlgProc (hRHS, msg, wp, lp))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         g_pWiz->EnableButtons (BACK_BUTTON | NEXT_BUTTON);
         g_pWiz->SetButtonText (IDNEXT, IDS_NEXT);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_WIZARD:
               switch (HIWORD(wp))
                  {
                  case wcIS_STATE_DISABLED:
                     return g_fSkipStep2;
                  }
               break;

            case IDBACK:
               g_pWiz->SetState (sidSTEP_ONE);
               break;

            case IDNEXT:
               g_pWiz->SetState (sidSTEP_THREE);
               break;
            }
         break;
      }

   return FALSE;
}


BOOL CALLBACK WizStep_3_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
{
   if (WizStep_Common_DlgProc (hRHS, msg, wp, lp))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         g_pWiz->EnableButtons (BACK_BUTTON | NEXT_BUTTON);
         g_pWiz->SetButtonText (IDNEXT, IDS_FINISH);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDBACK:
               g_pWiz->SetState (sidSTEP_TWO);
               break;

            case IDNEXT:
               g_pWiz->Show (FALSE);
               break;
            }
         break;
      }

   return FALSE;
}


VOID CALLBACK Wiz_ForceGraphicRedraw (HWND hWnd, UINT msg, UINT idTimer, DWORD idTick)
{
   HWND hParent;
   if ((hParent = g_pWiz->GetWindow()) != NULL)
      {
      HWND hLHS;
      if ((hLHS = GetDlgItem (hParent, IDC_WIZARD_LEFTPANE)) != NULL)
         {
         RECT rClient;
         GetClientRect (hLHS, &rClient);
         InvalidateRect (hLHS, &rClient, TRUE);
         UpdateWindow (hLHS);
         }
      }
}


/*
 * TOTALLY GRATUITOUS DRAWING STUFF ___________________________________________
 *
 */

#ifndef PI
#define PI (3.14159265358979323846264338)
#endif
#ifndef TWOPI
#define TWOPI (PI * 2)
#endif

void NormalizeAngle (double &dAng)
{
   while (dAng < 0.00)
      dAng += TWOPI;
   while (dAng >= TWOPI)
      dAng -= TWOPI;
}


void Wiz_DrawOverlay_Line (HDC hdcTarget, LPRECT prTarget, LONG xCenter, LONG yCenter, double dAngle)
{
   // There are four important angles for us to figure out; these are the
   // angles between the current center-point and each of the corners
   // of our bounding rectangle.
   //
   double dUR = atan2 (yCenter - prTarget->top, prTarget->right - xCenter);
   double dUL = atan2 (yCenter - prTarget->top, prTarget->left - xCenter);
   double dLL = atan2 (yCenter - prTarget->bottom, prTarget->left - xCenter);
   double dLR = atan2 (yCenter - prTarget->bottom, prTarget->right - xCenter);

   NormalizeAngle (dUR);
   NormalizeAngle (dUL);
   NormalizeAngle (dLL);
   NormalizeAngle (dLR);
   NormalizeAngle (dAngle);

   // Now that we know this, we can determine how long the line segment should
   // be (because we can easily find which face of the bounding rectangle it
   // will intersect).
   //
   LONG cxLine = 0;
   LONG cyLine = 0;
   if (dAngle >= dUR && dAngle < dUL) // top face
      {
      cyLine = yCenter - prTarget->top;
      double dLength = cyLine / cos((PI/2)-dAngle);
      cxLine = (LONG)( dLength * sin((PI/2)-dAngle) );
      cyLine = 0-cyLine;
      }
   else if (dAngle >= dUL && dAngle < dLL) // left face
      {
      cxLine = prTarget->left - xCenter;
      double dLength = cxLine / cos(PI-dAngle);
      cyLine = (LONG)( dLength * sin(PI-dAngle) );
      }
   else if (dAngle >= dLL && dAngle < dLR) // bottom face
      {
      cyLine = prTarget->bottom - yCenter;
      double dLength = cyLine / cos((PI*3/2)-dAngle);
      cxLine = 0 - (LONG)( dLength * sin((PI*3/2)-dAngle) );
      }
   else // right face
      {
      cxLine = prTarget->right - xCenter;
      double dLength = cxLine / cos(dAngle);
      cyLine = 0 - (LONG)( dLength * sin(dAngle) );
      }

   MoveToEx (hdcTarget, xCenter, yCenter, 0);
   LineTo (hdcTarget, xCenter + cxLine, yCenter + cyLine);
}


void CALLBACK Wiz_DrawOverlay (LPWIZARD pWiz, HDC hdcTarget, LPRECT prTarget, HPALETTE hPal)
{
   static double dAngleNext = 0.00;
   static double dColorRotNext = 0.00;  // 0'=Red, 120'=Green, 240'=Blue
   static double xCenterNext = -1.0;
   static double yCenterNext = -1.0;
   static double cxCenterMove = 0.0;
   static double cyCenterMove = 0.0;

   // First choose a good color. If we're using a palletized display
   // (i.e., 256-color or worse), we'll just draw in white; otherwise,
   // we'll draw in pretty rotating colors.
   //
   COLORREF clr = RGB(255,255,255);

   WORD wDepthDisplay;
   if ((wDepthDisplay = (WORD)GetDeviceCaps (hdcTarget, BITSPIXEL)) > 8)
      {
      double dPortionRed = cos (dColorRotNext);
      double dPortionGreen = cos (dColorRotNext + (TWOPI/3));
      double dPortionBlue = cos (dColorRotNext - (TWOPI/3));

      dPortionRed = max (dPortionRed, 0.00);
      dPortionGreen = max (dPortionGreen, 0.00);
      dPortionBlue = max (dPortionBlue, 0.00);

      clr = RGB( (BYTE)(255.0 * dPortionRed),
                 (BYTE)(255.0 * dPortionGreen),
                 (BYTE)(255.0 * dPortionBlue) );

      if ((dColorRotNext += TWOPI / (fpsANIMATE_OVERLAY * spcANIMATE_COLORS)) >= TWOPI)
         dColorRotNext -= TWOPI;
      }

   // Create a pen in the chosen color
   //
   HPEN hpNew = CreatePen (PS_SOLID, 1, clr);
   HPEN hpOld = (HPEN)SelectObject (hdcTarget, hpNew);

   // Make sure we start our X in the center
   //
   if (xCenterNext < 0)
      {
      xCenterNext = prTarget->left + (prTarget->right  - prTarget->left)/2;
      yCenterNext = prTarget->top  + (prTarget->bottom - prTarget->top)/2;
      }

   // Then draw the necessary lines; first the box (to prove we know where
   // the boundaries of the visible rectangle are), then the X (to prove
   // we can make things that look neat).
   //
   MoveToEx (hdcTarget, prTarget->left, prTarget->top, 0);
   LineTo (hdcTarget, prTarget->right-1, prTarget->top);
   LineTo (hdcTarget, prTarget->right-1, prTarget->bottom-1);
   LineTo (hdcTarget, prTarget->left, prTarget->bottom-1);
   LineTo (hdcTarget, prTarget->left, prTarget->top);

   Wiz_DrawOverlay_Line (hdcTarget, prTarget, (LONG)xCenterNext, (LONG)yCenterNext, dAngleNext);
   Wiz_DrawOverlay_Line (hdcTarget, prTarget, (LONG)xCenterNext, (LONG)yCenterNext, dAngleNext + (PI/2));
   Wiz_DrawOverlay_Line (hdcTarget, prTarget, (LONG)xCenterNext, (LONG)yCenterNext, dAngleNext + (PI));
   Wiz_DrawOverlay_Line (hdcTarget, prTarget, (LONG)xCenterNext, (LONG)yCenterNext, dAngleNext - (PI/2));

   // Make the X rotate a little bit for each frame
   //
   if ((dAngleNext += TWOPI / (fpsANIMATE_OVERLAY * spcANIMATE_OVERLAY)) >= TWOPI)
      dAngleNext -= TWOPI;

   // If the center isn't moving yet, kick it.
   //
   if (!cxCenterMove && !cyCenterMove)
      {
      cxCenterMove = (double)(rand() % 7500) / 1000.0;
      cyCenterMove = (double)(rand() % 7500) / 1000.0;
      }

   // Bounce the center around
   //
   if ((xCenterNext += cxCenterMove) < prTarget->left)
      {
      xCenterNext = prTarget->left;
      if (cxCenterMove < 0)
         cxCenterMove = 0 - cxCenterMove;
      }
   else if (xCenterNext >= prTarget->right)
      {
      xCenterNext = prTarget->right-1;
      if (cxCenterMove > 0)
         cxCenterMove = 0 - cxCenterMove;
      }

   if ((yCenterNext += cyCenterMove) < prTarget->top)
      {
      yCenterNext = prTarget->top;
      if (cyCenterMove < 0)
         cyCenterMove = 0 - cyCenterMove;
      }
   else if (yCenterNext >= prTarget->bottom)
      {
      yCenterNext = prTarget->bottom-1;
      if (cyCenterMove > 0)
         cyCenterMove = 0 - cyCenterMove;
      }

   // We're done; clean up.
   //
   SelectObject (hdcTarget, hpOld);
   DeleteObject (hpNew);
}

