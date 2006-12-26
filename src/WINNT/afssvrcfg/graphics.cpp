/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES ___________________________________________________________________
 *
 */
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"
#include "config.h"
#include "graphics.h"
#include "resource.h"


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static const COLORREF STEP_IN_PROGRESS_COLOR = 0x00FF00;		// Green
static const COLORREF STEP_TO_BE_DONE_COLOR = 0xFF0000;			// Blue


/*
 * STATIC FUNCTIONS _________________________________________________________________
 *
 */
static void EraseRect(HDC hdc, RECT rect)
{
    HBRUSH hbr = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    HGDIOBJ hbrOld = SelectObject(hdc, hbr);

    HPEN hPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNFACE));
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hbrOld);

    DeleteObject(hPen);
    DeleteObject(hbr);
}	

static void DrawCircle(HDC hdc, RECT rect, COLORREF crCircleColor)
{
    HBRUSH hBrush = CreateSolidBrush(crCircleColor);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 1, crCircleColor);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    OffsetRect(&rect, 1, -1);

    int midX = rect.left + ((rect.right - rect.left) / 2);
    int midY = rect.top + ((rect.bottom - rect.top) / 2);
	
    MoveToEx(hdc, midX - 1, midY - 2, 0);
    LineTo(hdc, midX + 2, midY - 2);

    MoveToEx(hdc, midX - 2, midY - 1, 0);
    LineTo(hdc, midX + 3, midY - 1);

    MoveToEx(hdc, midX - 2, midY, 0);
    LineTo(hdc, midX + 3, midY);

    MoveToEx(hdc, midX - 2, midY + 1, 0);
    LineTo(hdc, midX + 3, midY + 1);

    MoveToEx(hdc, midX - 1, midY + 2, 0);
    LineTo(hdc, midX + 2, midY + 2);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);

    DeleteObject(hPen);
    DeleteObject(hBrush);
}	

static void DrawCheckmark(HDC hdc, RECT rect)
{
#define cxCHECKBOX        (2+9+2)
#define cyCHECKBOX        (2+9+2)
	
    // Checkmark
    HPEN hpNew = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HGDIOBJ hpOld = (HPEN)SelectObject(hdc, hpNew);

    POINT ptCheckbox;
    ptCheckbox.x = rect.left;
    ptCheckbox.y = rect.top + ((rect.bottom - rect.top) - cyCHECKBOX) / 2;
	
    MoveToEx(hdc, ptCheckbox.x +3, ptCheckbox.y+5, NULL);
    LineTo(hdc, ptCheckbox.x +5, ptCheckbox.y+7);
    LineTo(hdc, ptCheckbox.x+10, ptCheckbox.y+2);

    MoveToEx(hdc, ptCheckbox.x +3, ptCheckbox.y+6, NULL);
    LineTo(hdc, ptCheckbox.x +5, ptCheckbox.y+8);
    LineTo(hdc, ptCheckbox.x+10, ptCheckbox.y+3);

    MoveToEx(hdc, ptCheckbox.x +3, ptCheckbox.y+7, NULL);
    LineTo(hdc, ptCheckbox.x +5, ptCheckbox.y+9);
    LineTo(hdc, ptCheckbox.x+10, ptCheckbox.y+4);

    SelectObject(hdc, hpOld);
    DeleteObject(hpNew);
}

static void DrawX(HDC hdc, RECT rect)
{
    // Red X
    static COLORREF crXColor = 0X0000FF;

    HBRUSH hbrRed = CreateSolidBrush(crXColor);
    HGDIOBJ hbrOld = SelectObject(hdc, hbrRed);

    HPEN hPen = CreatePen(PS_SOLID, 1, crXColor);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    OffsetRect(&rect, 3, 0);

    rect.top++;
    rect.bottom++;

    int nLen = 7;

    MoveToEx(hdc, rect.left, rect.top, 0);
    LineTo(hdc, rect.left + nLen, rect.top + nLen);

    MoveToEx(hdc, rect.left, rect.top + 1, 0);
    LineTo(hdc, rect.left + nLen, rect.top + nLen + 1);

    MoveToEx(hdc, rect.left, rect.top - 1, 0);
    LineTo(hdc, rect.left + nLen, rect.top + nLen - 1);

    MoveToEx(hdc, rect.left + nLen - 1, rect.top, 0);
    LineTo(hdc, rect.left - 1, rect.top + nLen);

    MoveToEx(hdc, rect.left + nLen - 1, rect.top + 1, 0);
    LineTo(hdc, rect.left - 1, rect.top + nLen + 1);

    MoveToEx(hdc, rect.left + nLen - 1, rect.top - 1, 0);
    LineTo(hdc, rect.left - 1, rect.top + nLen - 1);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hbrOld);

    DeleteObject(hPen);
    DeleteObject(hbrRed);
}


/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
void PaintStepGraphic(HWND hwnd, STEP_STATE state)
{
    PAINTSTRUCT ps;

    HDC hdc = BeginPaint(hwnd, &ps);
    _ASSERTE(hdc);

    RECT rect;
    GetClientRect(hwnd, &rect);

    InflateRect(&rect, -2, -2);

    // First erase the background
    EraseRect(hdc, rect);

    // Draw an image that corresponds to the state
    switch (state) {
    case SS_STEP_IN_PROGRESS:	
	DrawCircle(hdc, rect, STEP_IN_PROGRESS_COLOR);
	break;

    case SS_STEP_TO_BE_DONE:	
	DrawCircle(hdc, rect, STEP_TO_BE_DONE_COLOR);
	break;

    case SS_STEP_FINISHED:		
	DrawCheckmark(hdc, rect);
	break;

	
    case SS_STEP_FAILED: 
	DrawX(hdc, rect);
	break;
    }
	
    EndPaint(hwnd, &ps);
}


#define clrWHITE           RGB(255,255,255)
#define clrHIGHLIGHT       RGB(192,192,192)
#define clrSHADOW          RGB(128,128,128)
#define clrBLACK           RGB(100,100,100)
#define clrBAR_INT_LEFT    RGB(0,255,0)
#define clrBAR_INT_RIGHT   RGB(128,0,0)
#define clrARROW_INTERIOR  RGB(128,128,0)
#define clrTEXT_CURRENT    RGB(255,255,255)
#define clrTEXT_STEP       RGB(0,255,0)

#define cxLEFT_MARGIN      10
#define cxRIGHT_MARGIN     10
#define cyBOTTOM_MARGIN     5
#define cyAREA             100
#define cyBELOW_CURRENT    15
#define cyBELOW_ARROW      10

void CALLBACK PaintPageGraphic(LPWIZARD pWiz, HDC hdc, LPRECT prTarget, HPALETTE hpal)
{
    static HFONT hFont = AfsAppLib_CreateFont(IDS_GRAPHIC_FONT);
    static HPEN hPenWhite = CreatePen(PS_SOLID, 1, clrWHITE);
    static HPEN hPenHighlight = CreatePen(PS_SOLID, 1, clrHIGHLIGHT);
    static HPEN hPenShadow = CreatePen(PS_SOLID, 1, clrSHADOW);
    static HPEN hPenBlack = CreatePen(PS_SOLID, 1, clrBLACK);
    static HPEN hPenBarIntLeft = CreatePen(PS_SOLID, 1, clrBAR_INT_LEFT);
    static HPEN hPenBarIntRight = CreatePen(PS_SOLID, 1, clrBAR_INT_RIGHT);
    static HPEN hPenArrowInterior = CreatePen(PS_SOLID, 1, clrARROW_INTERIOR);

    // First find out where we'll be drawing things.
    RECT rArea;
    rArea.top = prTarget->bottom - cyAREA - cyBOTTOM_MARGIN;
    rArea.bottom = prTarget->bottom - cyBOTTOM_MARGIN;
    rArea.left = prTarget->left + cxLEFT_MARGIN;
    rArea.right = prTarget->right - cxRIGHT_MARGIN;

    // Draw the "Current Step:" text
    HGDIOBJ hFontOld = SelectObject(hdc, hFont);
    COLORREF clrTextOld = SetTextColor (hdc, clrTEXT_CURRENT);
    SetBkMode (hdc, TRANSPARENT);

    TCHAR szText[cchRESOURCE];
    GetResString(IDS_CURRENT_STEP, szText);

    RECT rText = rArea;
    DWORD dwFlags = DT_CENTER | DT_TOP | DT_SINGLELINE;
    DrawTextEx (hdc, szText, lstrlen(szText), &rText, dwFlags | DT_CALCRECT, NULL);

    rText.right = rArea.right;
    DrawTextEx (hdc, szText, lstrlen(szText), &rText, dwFlags, NULL);

    // Draw the progress bar; it should look like this:
    // wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww   // (w=white, b=black...
    // whhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhb   //  h=highlight, s=shadow...
    // whllllllllllllllllrrrrrrrrrrrrrrrsb   //  l=left/int, r=right/int)
    // whllllllllllllllllrrrrrrrrrrrrrrrsb   //  l=left/int, r=right/int)
    // whssssssssssssssssssssssssssssssssb   //  h=highlight, s=shadow...
    // wbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb   //  h=highlight, s=shadow...

    // Oh--we'll need to know where the pointer's point should go. We'll
    // make that be where the leftmost dot of the pointer's tip, and the
    // rightmost dot that's colored "l". One state 0, we want the pointer
    // to be all the way to the left--and on state {g_nNumStates-1}, we want
    // it all the way to the right

    RECT rBar = rArea;
    rBar.top = rText.bottom + cyBELOW_CURRENT;
    rBar.bottom = rBar.top + 6;

    RECT rBarInterior = rBar;
    InflateRect (&rBarInterior, -2, -2);

    int nStepSize = (rBarInterior.right - rBarInterior.left) / (g_nNumStates-1);
    int xCurPos = rBarInterior.left + (g_pWiz->GetState() * nStepSize);
    if (!g_pWiz->GetState())
	xCurPos = rBarInterior.left-1;  // don't draw *any* green
    else if (g_pWiz->GetState() == (int)(g_nNumStates-1))
	xCurPos = rBarInterior.right-1;  // don't draw *any* red

    // Draw that bar!
    HGDIOBJ hPenOld = SelectObject (hdc, hPenWhite);
    MoveToEx (hdc, rBar.left, rBar.bottom-1, 0);
    LineTo (hdc, rBar.left, rBar.top);
    LineTo (hdc, rBar.right, rBar.top);
    MoveToEx (hdc, rBar.left, rBar.bottom, 0);

    SelectObject (hdc, hPenHighlight);
    MoveToEx (hdc, rBar.left+1, rBar.bottom-2, 0);
    LineTo (hdc, rBar.left+1, rBar.top+1);
    LineTo (hdc, rBar.right-1, rBar.top+1);

    SelectObject (hdc, hPenShadow);
    MoveToEx (hdc, rBar.left+2, rBar.bottom-2, 0);
    LineTo (hdc, rBar.right-2, rBar.bottom-2);
    LineTo (hdc, rBar.right-2, rBar.top+1);

    SelectObject (hdc, hPenBlack);
    MoveToEx (hdc, rBar.left+1, rBar.bottom-1, 0);
    LineTo (hdc, rBar.right-1, rBar.bottom-1);
    LineTo (hdc, rBar.right-1, rBar.top);

    if (xCurPos >= rBarInterior.left) {
	SelectObject (hdc, hPenBarIntLeft);
	MoveToEx (hdc, rBarInterior.left, rBarInterior.top, 0);
	LineTo (hdc, xCurPos+1, rBarInterior.top);
	MoveToEx (hdc, rBarInterior.left, rBarInterior.top+1, 0);
	LineTo (hdc, xCurPos+1, rBarInterior.top+1);
    }

    if (xCurPos < rBarInterior.right-1) {
	SelectObject (hdc, hPenBarIntRight);
	MoveToEx (hdc, xCurPos+1, rBarInterior.top, 0);
	LineTo (hdc, rBarInterior.right, rBarInterior.top);
	MoveToEx (hdc, xCurPos+1, rBarInterior.top+1, 0);
	LineTo (hdc, rBarInterior.right, rBarInterior.top+1);
    }
    SelectObject (hdc, hPenOld);

    // Draw the arrow underneath it; it should look like this:
    //             wb
    //            whsb
    //           whassb
    //          whaaassb
    //         whaaaaassb
    //        wssssssssssb
    // Remember that the topmost "w" is where xCurPos is.

    RECT rArrow;
    rArrow.top = rBar.bottom +1;
    rArrow.bottom = rArrow.top +6;
    rArrow.left = xCurPos -5;
    rArrow.right = xCurPos +7;

    hPenOld = SelectObject (hdc, hPenWhite);
    MoveToEx (hdc, rArrow.left, rArrow.bottom-1, 0);
    LineTo (hdc, xCurPos+1, rArrow.top-1);

    SelectObject (hdc, hPenHighlight);
    MoveToEx (hdc, rArrow.left+2, rArrow.bottom-2, 0);
    LineTo (hdc, xCurPos+1, rArrow.top);

    SelectObject (hdc, hPenShadow);
    MoveToEx (hdc, rArrow.left+1, rArrow.bottom-1, 0);
    LineTo (hdc, rArrow.right-1, rArrow.bottom-1);
    MoveToEx (hdc, xCurPos+1, rArrow.top+1, 0);
    LineTo (hdc, rArrow.right, rArrow.bottom);
    MoveToEx (hdc, xCurPos+1, rArrow.top+2, 0);
    LineTo (hdc, rArrow.right-1, rArrow.bottom);

    SelectObject (hdc, hPenBlack);
    MoveToEx (hdc, xCurPos+1, rArrow.top, 0);
    LineTo (hdc, rArrow.right, rArrow.bottom);

    //             wb
    //            whsb
    //           whassb
    //          whaaassb
    //         whaaaaassb
    //        wssssssssssb

    SelectObject (hdc, hPenArrowInterior);
    MoveToEx (hdc, xCurPos, rArrow.top+2, 0);
    LineTo (hdc, xCurPos+1, rArrow.top+2);
    MoveToEx (hdc, xCurPos-1, rArrow.top+3, 0);
    LineTo (hdc, xCurPos+2, rArrow.top+3);
    MoveToEx (hdc, xCurPos-2, rArrow.top+4, 0);
    LineTo (hdc, xCurPos+3, rArrow.top+4);

    SelectObject (hdc, hPenOld);

    // Draw the description text
    SetTextColor (hdc, clrTEXT_STEP);
    GetResString(g_StateDesc[g_pWiz->GetState()], szText);

    rText = rArea;
    rText.top = rArrow.bottom + cyBELOW_ARROW;
    dwFlags = DT_CENTER | DT_TOP | DT_WORDBREAK;
    DrawTextEx (hdc, szText, lstrlen(szText), &rText, dwFlags, NULL);

    SetTextColor (hdc, clrTextOld);
    SelectObject (hdc, hFontOld);
}	

