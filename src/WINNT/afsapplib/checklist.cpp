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
#include <windowsx.h>
#include <commctrl.h>
#include <WINNT/TaLocale.h>
#include <WINNT/subclass.h>
#include <WINNT/checklist.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cxCHECKBOX        (2+9+2)
#define cyCHECKBOX        (2+9+2)

#define cxAFTER_CHECKBOX  4


#define LB_GETHIT     (WM_USER+298)
#define LB_SETHIT     (WM_USER+299)   // int iItem=wp

/*
 * int LB_GetHit (HWND hList)
 * void LB_SetHit (HWND hList, int iItem)
 *
 */
#define LB_GetHit(_hList) \
        SendMessage(_hList,LB_GETHIT,(WPARAM)0,(LPARAM)0)
#define LB_SetHit(_hList,_ii) \
        SendMessage(_hList,LB_SETHIT,(WPARAM)_ii,(LPARAM)0)


/*
 * VARIABLES __________________________________________________________________
 *
 */

LONG procListbox = 0;

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

HRESULT CALLBACK CheckListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp);

HRESULT CALLBACK CheckList_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void CheckList_OnDrawItem (HWND hList, int id, LPDRAWITEMSTRUCT lpds);
void CheckList_OnDrawCheckbox (HWND hList, int id, LPDRAWITEMSTRUCT lpds);
void CheckList_OnDrawText (HWND hList, int id, LPDRAWITEMSTRUCT lpds);

BOOL CheckList_OnHitTest (HWND hList, int id);

BOOL CheckList_OnGetHit   (HWND hList, WPARAM wp, LPARAM lp);
BOOL CheckList_OnSetHit   (HWND hList, WPARAM wp, LPARAM lp);
BOOL CheckList_OnGetCheck (HWND hList, WPARAM wp, LPARAM lp);
BOOL CheckList_OnSetCheck (HWND hList, WPARAM wp, LPARAM lp);

void CheckList_OnButtonDown (HWND hList);
void CheckList_OnButtonUp (HWND hList);
void CheckList_OnDoubleClick (HWND hList);
void CheckList_OnMouseMove (HWND hList);

void CheckList_OnSetCheck_Selected (HWND hList, BOOL fCheck);

void CheckList_RedrawCheck (HWND hList, int ii);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) CheckListReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL CheckListReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = Allocate (cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget != 0)
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      Free (*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}
#endif


BOOL RegisterCheckListClass (void)
{
   static BOOL fRegistered = FALSE;

   if (!fRegistered)
      {
      WNDCLASS wc;
      GetClassInfo (THIS_HINST, TEXT("LISTBOX"), &wc);
      procListbox = PtrToLong(wc.lpfnWndProc);

      wc.style = CS_GLOBALCLASS;
      wc.lpfnWndProc = (WNDPROC)CheckListProc;
      wc.hInstance = THIS_HINST;
      wc.hCursor = LoadCursor (NULL, MAKEINTRESOURCE (IDC_ARROW));
      wc.hbrBackground = CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
      wc.lpszClassName = WC_CHECKLIST;

	  if (RegisterClass (&wc)) {
	      fRegistered = TRUE;
	  } else {
	      OutputDebugString("CheckList class registration failed\n");
	  }
      }

   return fRegistered;
}


BOOL IsCheckList (HWND hList)
{
   TCHAR szClassName[256];
   if (!GetClassName (hList, szClassName, 256))
      return FALSE;

   if (lstrcmp (szClassName, WC_CHECKLIST))
      return FALSE;

   return TRUE;
}


HRESULT CALLBACK CheckListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
    HRESULT hResult;

    switch (msg)
      {
      case WM_CREATE:
         Subclass_AddHook (GetParent(hList), CheckList_DialogProc);
         LB_SetHit (hList, -1);
         break;

      case WM_DESTROY:
         Subclass_RemoveHook (GetParent(hList), CheckList_DialogProc);
         break;

      case WM_ENABLE:
         RECT rClient;
         GetClientRect (GetParent(hList), &rClient);
         InvalidateRect (GetParent(hList), &rClient, FALSE);
         UpdateWindow (GetParent(hList));
         break;

      case WM_LBUTTONDOWN:
         CheckList_OnButtonDown (hList);
         if (GetCapture() == hList)
            {
            if (procListbox)
               {
               ReleaseCapture ();
               CallWindowProc ((WNDPROC)LongToPtr(procListbox), hList, WM_LBUTTONDOWN, wp, lp);
               CallWindowProc ((WNDPROC)LongToPtr(procListbox), hList, WM_LBUTTONUP, wp, lp);
               SetCapture (hList);
               }
            return TRUE;
            }
         break;

      case WM_LBUTTONUP:
         CheckList_OnButtonUp (hList);
         break;

      case WM_LBUTTONDBLCLK:
         CheckList_OnDoubleClick (hList);
         break;

      case WM_MOUSEMOVE:
         CheckList_OnMouseMove (hList);
         break;

      case LB_GETHIT:
         return CheckList_OnGetHit (hList, wp, lp);

      case LB_SETHIT:
         return CheckList_OnSetHit (hList, wp, lp);

      case LB_GETCHECK:
         return CheckList_OnGetCheck (hList, wp, lp);

      case LB_SETCHECK:
         return CheckList_OnSetCheck (hList, wp, lp);
      }

   if (procListbox)
      hResult = (BOOL)CallWindowProc ((WNDPROC)LongToPtr(procListbox), hList, msg, wp, lp);
   else
      hResult = (BOOL)DefWindowProc (hList, msg, wp, lp);

    return (hResult);
}


HRESULT CALLBACK CheckList_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID procOld = Subclass_FindNextHook (hDlg, CheckList_DialogProc);

   switch (msg)
      {
      case WM_MEASUREITEM:
         LPMEASUREITEMSTRUCT lpms;
         if ((lpms = (LPMEASUREITEMSTRUCT)lp) != NULL)
            {
            HDC hdc = GetDC (hDlg);
            TEXTMETRIC tm;
            GetTextMetrics (hdc, &tm);
            ReleaseDC (hDlg, hdc);
            lpms->itemHeight = max( tm.tmHeight, cyCHECKBOX );
            return TRUE;
            }
         break;

      case WM_DRAWITEM:
         LPDRAWITEMSTRUCT lpds;
         if ((lpds = (LPDRAWITEMSTRUCT)lp) != NULL)
            {
            CheckList_OnDrawItem (lpds->hwndItem, lpds->itemID, lpds);
            return TRUE;
            }
         break;

      case WM_CTLCOLORLISTBOX:
         if (IsCheckList ((HWND)lp))
            {
            static COLORREF clrLast = (COLORREF)-1;
            static HBRUSH hbrStatic = NULL;
            COLORREF clrNew = GetSysColor (IsWindowEnabled((HWND)lp) ? COLOR_WINDOW : COLOR_BTNFACE);
            if (clrNew != clrLast)
               hbrStatic = CreateSolidBrush (clrLast = clrNew);
            SetBkColor ((HDC)wp, clrLast);
            return (BOOL)(INT_PTR)hbrStatic;
            }
         break;
      }

   if (procOld)
      return CallWindowProc ((WNDPROC)procOld, hDlg, msg, wp, lp);
   else
      return FALSE;
}


void CheckList_OnDrawItem (HWND hList, int id, LPDRAWITEMSTRUCT lpds)
{
   if (id >= 0)
      {
      CheckList_OnDrawCheckbox (hList, id, lpds);
      CheckList_OnDrawText (hList, id, lpds);
      }
}


void CheckList_OnDrawCheckbox (HWND hList, int id, LPDRAWITEMSTRUCT lpds)
{
   // Step 1: erase around the checkbox.
   //
   COLORREF clrBack = GetSysColor (COLOR_WINDOW);
   if (lpds->itemState & ODS_DISABLED)
      clrBack = GetSysColor (COLOR_BTNFACE);

   HBRUSH hbr = CreateSolidBrush (clrBack);

   POINT ptCheckbox;
   ptCheckbox.x = lpds->rcItem.left;
   ptCheckbox.y = lpds->rcItem.top + ((lpds->rcItem.bottom - lpds->rcItem.top) - cyCHECKBOX) /2;

   // step 1a: fill in above the checkbox
   RECT rr;
   rr.top    = lpds->rcItem.top;
   rr.left   = lpds->rcItem.left;
   rr.right  = lpds->rcItem.left + cxCHECKBOX;
   rr.bottom = ptCheckbox.y;
   FillRect (lpds->hDC, &rr, hbr);

   // step 1b: fill in below the checkbox
   rr.top = ptCheckbox.y + cyCHECKBOX;
   rr.bottom = lpds->rcItem.bottom;
   FillRect (lpds->hDC, &rr, hbr);

   // step 1c: fill in to the right of the checkbox
   rr.top = lpds->rcItem.top;
   rr.left = lpds->rcItem.left + cxCHECKBOX;
   rr.right = lpds->rcItem.left + cxCHECKBOX + cxAFTER_CHECKBOX;
   rr.bottom = lpds->rcItem.bottom;
   FillRect (lpds->hDC, &rr, hbr);

   DeleteObject (hbr);

   // Step 2: draw the checkbox itself
   //
   HPEN hpOld;
   HPEN hpNew;

   // step 2a: draw the btnshadow upper-left lines
   hpNew = CreatePen (PS_SOLID, 1, GetSysColor (COLOR_BTNSHADOW));
   hpOld = (HPEN)SelectObject (lpds->hDC, hpNew);
   MoveToEx (lpds->hDC, ptCheckbox.x, ptCheckbox.y +cyCHECKBOX -2, NULL);
   LineTo (lpds->hDC, ptCheckbox.x, ptCheckbox.y);
   LineTo (lpds->hDC, ptCheckbox.x +cxCHECKBOX-1, ptCheckbox.y);
   SelectObject (lpds->hDC, hpOld);
   DeleteObject (hpNew);

   // step 2b: draw the black upper-left lines
   hpNew = CreatePen (PS_SOLID, 1, RGB(0,0,0));
   hpOld = (HPEN)SelectObject (lpds->hDC, hpNew);
   MoveToEx (lpds->hDC, ptCheckbox.x+1, ptCheckbox.y+cyCHECKBOX-3, NULL);
   LineTo (lpds->hDC, ptCheckbox.x+1, ptCheckbox.y+1);
   LineTo (lpds->hDC, ptCheckbox.x+cxCHECKBOX-2, ptCheckbox.y+1);
   SelectObject (lpds->hDC, hpOld);
   DeleteObject (hpNew);

   // step 2c: draw the btnface lower-right lines
   hpNew = CreatePen (PS_SOLID, 1, GetSysColor (COLOR_BTNFACE));
   hpOld = (HPEN)SelectObject (lpds->hDC, hpNew);
   MoveToEx (lpds->hDC, ptCheckbox.x+1, ptCheckbox.y+cyCHECKBOX-2, NULL);
   LineTo (lpds->hDC, ptCheckbox.x+cxCHECKBOX-2, ptCheckbox.y+cyCHECKBOX-2);
   LineTo (lpds->hDC, ptCheckbox.x+cxCHECKBOX-2, ptCheckbox.y);
   SelectObject (lpds->hDC, hpOld);
   DeleteObject (hpNew);

   // step 2d: draw the btnhighlight lower-right lines
   hpNew = CreatePen (PS_SOLID, 1, GetSysColor (COLOR_BTNHIGHLIGHT));
   hpOld = (HPEN)SelectObject (lpds->hDC, hpNew);
   MoveToEx (lpds->hDC, ptCheckbox.x, ptCheckbox.y+cyCHECKBOX-1, NULL);
   LineTo (lpds->hDC, ptCheckbox.x+cxCHECKBOX-1, ptCheckbox.y+cyCHECKBOX-1);
   LineTo (lpds->hDC, ptCheckbox.x+cxCHECKBOX-1, ptCheckbox.y-1);
   SelectObject (lpds->hDC, hpOld);
   DeleteObject (hpNew);

   // step 2e: draw the background field
   //
   BOOL fHit = CheckList_OnHitTest (hList, id);
   BOOL fChecked = (BOOL)LB_GetCheck (hList, id);

   clrBack = GetSysColor (COLOR_WINDOW);
   if ( (lpds->itemState & ODS_DISABLED) ||
        (fHit && (LB_GetHit(hList) == id)))
      {
      clrBack = GetSysColor (COLOR_BTNFACE);
      }

   rr.top  = ptCheckbox.y +2;
   rr.left  = ptCheckbox.x +2;
   rr.right = ptCheckbox.x +cxCHECKBOX -2;
   rr.bottom = ptCheckbox.y +cyCHECKBOX -2;

   hbr = CreateSolidBrush (clrBack);
   FillRect (lpds->hDC, &rr, hbr);
   DeleteObject (hbr);

   // step 2f: draw the checkmark (if appropriate)
   //
   if (fChecked)
      {
      hpNew = CreatePen (PS_SOLID, 1, (lpds->itemState & ODS_DISABLED) ? GetSysColor(COLOR_BTNSHADOW) : RGB(0,0,0));
      hpOld = (HPEN)SelectObject (lpds->hDC, hpNew);

      MoveToEx (lpds->hDC, ptCheckbox.x +3, ptCheckbox.y+5, NULL);
      LineTo (lpds->hDC, ptCheckbox.x +5, ptCheckbox.y+7);
      LineTo (lpds->hDC, ptCheckbox.x+10, ptCheckbox.y+2);

      MoveToEx (lpds->hDC, ptCheckbox.x +3, ptCheckbox.y+6, NULL);
      LineTo (lpds->hDC, ptCheckbox.x +5, ptCheckbox.y+8);
      LineTo (lpds->hDC, ptCheckbox.x+10, ptCheckbox.y+3);

      MoveToEx (lpds->hDC, ptCheckbox.x +3, ptCheckbox.y+7, NULL);
      LineTo (lpds->hDC, ptCheckbox.x +5, ptCheckbox.y+9);
      LineTo (lpds->hDC, ptCheckbox.x+10, ptCheckbox.y+4);

      SelectObject (lpds->hDC, hpOld);
      DeleteObject (hpNew);
      }
}


void CheckList_OnDrawText (HWND hList, int id, LPDRAWITEMSTRUCT lpds)
{
   // Step 1: erase around the text.
   //
   COLORREF clrBack = GetSysColor (COLOR_WINDOW);
   COLORREF clrFore = GetSysColor (COLOR_WINDOWTEXT);
   if (lpds->itemState & ODS_DISABLED)
      {
      clrBack = GetSysColor (COLOR_BTNFACE);
      clrFore = GetSysColor (COLOR_GRAYTEXT);
      }
   else if (lpds->itemState & ODS_SELECTED)
      {
      clrBack = GetSysColor (COLOR_HIGHLIGHT);
      clrFore = GetSysColor (COLOR_HIGHLIGHTTEXT);
      }

   HBRUSH hbr = CreateSolidBrush (clrBack);

   // step 1a: find out how big the text is, and where it should go
   // (remember to add a few spaces to the front, so there's a little bit
   // of highlighted leading space before the text begins)
   TCHAR szText[256] = TEXT(" ");
   SendMessage (hList, LB_GETTEXT, (WPARAM)id, (LPARAM)&szText[lstrlen(szText)]);

   SIZE sText;
   GetTextExtentPoint (lpds->hDC, szText, lstrlen(szText), &sText);

   POINT ptText;
   ptText.x = lpds->rcItem.left + cxCHECKBOX + cxAFTER_CHECKBOX;
   ptText.y = lpds->rcItem.top + ((lpds->rcItem.bottom - lpds->rcItem.top) - sText.cy) /2;

   // step 1b: fill in above the text
   RECT rr;
   rr.top    = lpds->rcItem.top;
   rr.left   = ptText.x;
   rr.right  = lpds->rcItem.right;
   rr.bottom = ptText.y;
   FillRect (lpds->hDC, &rr, hbr);

   // step 1c: fill in below the text
   rr.top    = ptText.y;
   rr.bottom = lpds->rcItem.bottom;
   FillRect (lpds->hDC, &rr, hbr);

   // step 1d: fill in to the right of the text
   rr.top    = lpds->rcItem.top;
   rr.left   = ptText.x + sText.cx;
   rr.right  = lpds->rcItem.right;
   rr.bottom = lpds->rcItem.bottom;
   FillRect (lpds->hDC, &rr, hbr);

   DeleteObject (hbr);

   // Step 2: draw the text itself
   //
   rr.top    = ptText.y;
   rr.left   = ptText.x;
   rr.right  = ptText.x + sText.cx;
   rr.bottom = ptText.y + sText.cy;

   COLORREF clrBG = SetBkColor (lpds->hDC, clrBack);
   COLORREF clrFG = SetTextColor (lpds->hDC, clrFore);

   DRAWTEXTPARAMS Params;
   memset (&Params, 0x00, sizeof(DRAWTEXTPARAMS));
   Params.cbSize = sizeof(Params);
   Params.iTabLength = 15;

   SetBkMode (lpds->hDC, OPAQUE);
   DrawTextEx (lpds->hDC, szText, -1, &rr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_EXPANDTABS | DT_NOPREFIX | DT_TABSTOP | DT_NOCLIP, &Params);

   SetTextColor (lpds->hDC, clrFG);
   SetBkColor (lpds->hDC, clrBG);

   // Step 3: draw the focus rect (if appropriate)
   //
   if (lpds->itemState & ODS_FOCUS)
      {
      rr.top = lpds->rcItem.top;
      rr.left = ptText.x;
      rr.right = lpds->rcItem.right;
      rr.bottom = lpds->rcItem.bottom;
      DrawFocusRect (lpds->hDC, &rr);
      }
}


BOOL CheckList_OnHitTest (HWND hList, int id)
{
   RECT rr;
   SendMessage (hList, LB_GETITEMRECT, (WPARAM)id, (LPARAM)&rr);
   rr.right = rr.left + cxCHECKBOX;

   DWORD dw = GetMessagePos();
   POINT pt = { LOWORD(dw), HIWORD(dw) };
   ScreenToClient (hList, &pt);

   return PtInRect (&rr, pt);
}


BOOL CheckList_OnGetHit (HWND hList, WPARAM wp, LPARAM lp)
{
   return (BOOL)GetWindowLongPtr (hList, GWLP_USERDATA);
}


BOOL CheckList_OnSetHit (HWND hList, WPARAM wp, LPARAM lp)
{
   int iItem = (int)wp;
   SetWindowLongPtr (hList, GWLP_USERDATA, iItem);
   return TRUE;
}


BOOL CheckList_OnGetCheck (HWND hList, WPARAM wp, LPARAM lp)
{
   int iItem = (int)wp;

   return (BOOL)SendMessage (hList, LB_GETITEMDATA, (WPARAM)iItem, 0);
}


BOOL CheckList_OnSetCheck (HWND hList, WPARAM wp, LPARAM lp)
{
   int iItem = (int)wp;
   BOOL fCheck = (BOOL)lp;

   SendMessage (hList, LB_SETITEMDATA, (WPARAM)iItem, (LPARAM)fCheck);

   CheckList_RedrawCheck (hList, iItem);

   return TRUE;
}


void CheckList_OnMouseMove (HWND hList)
{
   if (GetCapture() == hList)
      {
      int ii = (int)LB_GetHit (hList);
      if (ii != -1)
         {
         CheckList_RedrawCheck (hList, ii);
         }
      }
}


void CheckList_OnButtonDown (HWND hList)
{
   if (IsWindowEnabled (hList))
      {
      DWORD dw = GetMessagePos();
      POINT pt = { LOWORD(dw), HIWORD(dw) };
      ScreenToClient (hList, &pt);

      int ii = (int)SendMessage (hList, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x,pt.y));
      if (HIWORD(ii) == 0)
         {
         BOOL fHit = CheckList_OnHitTest (hList, ii);
         if (fHit)
            {
            SetCapture (hList);
            LB_SetHit (hList, ii);

            CheckList_RedrawCheck (hList, ii);
            }
         }
      }
}


void CheckList_OnButtonUp (HWND hList)
{
   if (GetCapture() == hList)
      {
      ReleaseCapture();

      int ii = (int)LB_GetHit (hList);
      if (ii != -1)
         {
         LB_SetHit (hList, -1);

         BOOL fHit = CheckList_OnHitTest (hList, ii);

         if (!fHit)
            CheckList_RedrawCheck (hList, ii);
         else
            {
            BOOL fChecked = (BOOL)LB_GetCheck (hList, ii);
            CheckList_OnSetCheck_Selected (hList, !fChecked);
            }
         }
      }
}


void CheckList_OnDoubleClick (HWND hList)
{
   if (IsWindowEnabled (hList))
      {
      DWORD dw = GetMessagePos();
      POINT pt = { LOWORD(dw), HIWORD(dw) };
      ScreenToClient (hList, &pt);

      int ii = (int)SendMessage (hList, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x,pt.y));
      if (HIWORD(ii) == 0)
         {
         BOOL fChecked = (BOOL)LB_GetCheck (hList, ii);

         CheckList_OnSetCheck_Selected (hList, !fChecked);
         }
      }
}


void CheckList_OnSetCheck_Selected (HWND hList, BOOL fCheck)
{
   static int   *aSel = NULL;
   static size_t cSel = 0;

   if (GetWindowLong(hList,GWL_STYLE) & LBS_MULTIPLESEL)
      {
      size_t cReq = SendMessage(hList,LB_GETSELCOUNT,0,0);
      if ((cReq) && (cReq != LB_ERR) && REALLOC(aSel,cSel,cReq,4))
         {
         size_t iMax = (size_t)SendMessage (hList, LB_GETSELITEMS, (WPARAM)cSel, (LPARAM)aSel);
         if (iMax != LB_ERR)
            {
            for (size_t ii = 0; ii < iMax; ++ii)
               {
               LB_SetCheck (hList, aSel[ii], fCheck);
               }
            }
         }
      }
   else // single-sel listbox
      {
      int ii = (int)SendMessage (hList, LB_GETCURSEL, 0, 0);
      if (ii != LB_ERR)
         {
         LB_SetCheck (hList, ii, fCheck);
         }
      }

   SendMessage (GetParent(hList), WM_COMMAND, MAKELONG(GetWindowLong(hList,GWL_ID),LBN_CLICKED), (LPARAM)hList);
}


void CheckList_RedrawCheck (HWND hList, int ii)
{
   RECT rr;
   SendMessage (hList, LB_GETITEMRECT, (WPARAM)ii, (LPARAM)&rr);
   rr.right = rr.left + cxCHECKBOX;
   InvalidateRect (hList, &rr, TRUE);
   UpdateWindow (hList);
}

