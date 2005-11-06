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
#include <stdlib.h>
#include <WINNT/al_resource.h>	// To see if you have IDC_HSPLIT etc
#include <WINNT/resize.h>
#include <WINNT/subclass.h>
#include <WINNT/TaLocale.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef limit
#define limit(_a,_x,_b)    (min( max( (_a),(_x) ), (_b) ))
#endif
#ifndef inlimit
#define inlimit(_a,_x,_b)  ( (_x >= _a) && (_x <= _b) )
#endif

#ifndef THIS_HINST
#define THIS_HINST  (GetModuleHandle (NULL))
#endif

#ifndef GWL_USER
#define GWL_USER 0
#endif

#ifndef cxRECT
#define cxRECT(_r)  ((_r).right - (_r).left)
#endif
#ifndef cyRECT
#define cyRECT(_r)  ((_r).bottom - (_r).top)
#endif

typedef struct	// SplitterData
   {
   LONG         *pcDelta;	// pointer to LONG to hold cx/yDelta
   int           idWnd1;	// first window to split between
   int           idWnd2;	// second window to split between
   rwWindowData *awd;	// data list for using splitter
   BOOL          fX;	// TRUE if moves in X, FALSE if in Y
   BOOL          fDragging;	// TRUE if dragging with the mouse
   POINT         ptStart;	// point at which started dragging
   BOOL          fMovedBeforeCreate;	// TRUE if windows moved before create
   } SplitterData;


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   HWND  hWnd;
   RECT  rWnd;
   LONG  cxDeltaCenter;
   LONG  cyDeltaCenter;
   rwWindowData *awdLast;
   } WindowList;

static WindowList *awl;
static size_t      cwl = 0;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

int            rwFindOrAddWnd             (HWND, rwWindowData * = 0);
void           rwFindAndRemoveWnd         (HWND);
DWORD          rwFindAction               (rwWindowData *, int);

BOOL           WhereShouldSplitterGo      (HWND, int, int, RECT *, BOOL *);
void           EnsureSplitterRegistered   (void);
LONG APIENTRY  SplitterWndProc            (HWND, UINT, WPARAM, LPARAM);
void           ResizeSplitter             (HWND, SplitterData *, LONG, LONG*);
void           FindSplitterMinMax         (HWND, SplitterData *, LONG,
                                           LONG*, LONG*);

void FindResizeLimits (HWND hWnd, LONG *pcxMin, LONG *pcxMax, LONG *pcyMin, LONG *pcyMax, rwWindowData * = 0);

BOOL CALLBACK Resize_DialogProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) ResizeReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL ResizeReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)Allocate (cbElement * cNew)) == NULL)
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


void ResizeWindow (HWND hWnd, rwWindowData *awd, rwAction rwa, RECT *pr)
{
   static BOOL fInHere = FALSE;  // prevent reentrancy during SetWindowPos().
   int    ii;
   RECT   rOld, rNew;

   if (fInHere)
      return;

            // We maintain list of where-was-this-window-last-time; find
            // This window in that list, or add it.
            //
   if ((ii = rwFindOrAddWnd (hWnd, awd)) == -1)
      goto lblDONE;

   rOld = awl[ii].rWnd;	// previous position

            // If the window disappears, then remove its entry from the
            // list of windows.
            //
   if (!IsWindow (hWnd))
      {
      awl[ii].hWnd = NULL;
      goto lblDONE;
      }

            // If told to, move and/or resize this parent window.
            //
   if (rwa == rwaMoveToHere)
      {
      if (pr == NULL)
         goto lblDONE;

      fInHere = TRUE;

      SetWindowPos (hWnd, NULL,
                    pr->left,
                    pr->top,
                    pr->right - pr->left,
                    pr->bottom - pr->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);

      fInHere = FALSE;

      rNew = *pr;
      }
   else
      {
      GetWindowRect (hWnd, &rNew);
      }

            // Window has moved from {rOld} to {rNew},
            //    or
            // Window's client area has been changed by {*pr}
            //
   if (rwa == rwaMoveToHere || rwa == rwaFixupGuts || rwa == rwaNewClientArea)
      {
      LONG  dx, dy;
      LONG  dxc, dyc;

      if (rwa == rwaNewClientArea)
         {
         dx = pr->right - pr->left;
         dy = pr->bottom - pr->top;
         }
      else
         {
         dx = (rNew.right -rNew.left) - (rOld.right -rOld.left);
         dy = (rNew.bottom -rNew.top) - (rOld.bottom -rOld.top);
         }

      if (abs(dx) & 1)
         awl[ii].cxDeltaCenter += (dx > 0) ? 1 : -1;
      if (abs(dy) & 1)
         awl[ii].cyDeltaCenter += (dy > 0) ? 1 : -1;

      dxc = dx + awl[ii].cxDeltaCenter/2;
      dyc = dy + awl[ii].cyDeltaCenter/2;

      awl[ii].cxDeltaCenter %= 2;
      awl[ii].cyDeltaCenter %= 2;

      if (dx != 0 || dy != 0)
         {
         HWND   hItem;
         size_t nItems = 0;

         for (hItem = GetWindow (hWnd, GW_CHILD);
              hItem != NULL;
              hItem = GetWindow (hItem, GW_HWNDNEXT))
            {
            nItems++;
            }

         if (nItems != 0)
            {
            DWORD ra;
            HDWP dwp = BeginDeferWindowPos (nItems);
            BOOL fRepaint = FALSE;

            for (hItem = GetWindow (hWnd, GW_CHILD);
                 hItem != NULL;
                 hItem = GetWindow (hItem, GW_HWNDNEXT))
               {
               RECT   rItem;

               GetRectInParent (hItem, &rItem);

               ra = rwFindAction (awd, (GetWindowLong (hItem, GWL_ID)));

               DeferWindowPos (dwp, hItem, NULL,
                               rItem.left + ((ra & raMoveX)  ? dx :
                                            (ra & raMoveXB) ? (0-dx) :
                                            (ra & raMoveXC) ? (dxc/2) : 0),
                               rItem.top  + ((ra & raMoveY)  ? dy :
                                            (ra & raMoveYB) ? (0-dy) :
                                            (ra & raMoveYC) ? (dyc/2) : 0),
                               rItem.right -rItem.left
                                         + ((ra & raSizeX)  ? dx :
                                            (ra & raSizeXB) ? (0-dx) :
	    (ra & raSizeXC) ? (dxc/2) : 0),
                               rItem.bottom -rItem.top
                                         + ((ra & raSizeY)  ? dy :
                                            (ra & raSizeYB) ? (0-dy) :
	    (ra & raSizeYC) ? (dyc/2) : 0),
                               SWP_NOACTIVATE | SWP_NOZORDER);
               }

            EndDeferWindowPos (dwp);

            for (hItem = GetWindow (hWnd, GW_CHILD);
                 hItem != NULL;
                 hItem = GetWindow (hItem, GW_HWNDNEXT))
               {
               ra = rwFindAction (awd, (GetWindowLong (hItem, GWL_ID)));

               if (ra & raRepaint)
                  {
                  RECT rItem;
                  GetRectInParent (hItem, &rItem);
                  InvalidateRect (hWnd, &rItem, TRUE);
                  fRepaint = TRUE;
                  }

               if (ra & raNotify)
                  {
                  RECT rClient;
                  GetClientRect (hItem, &rClient);
                  SendMessage (hItem, WM_SIZE, SIZE_RESTORED, MAKELPARAM( rClient.right-rClient.left, rClient.bottom-rClient.top ));
                  }
               }

            if (fRepaint)
               {
               UpdateWindow (hWnd);
               }
            }
         }
      }

            // Record this window's current position
            //
   awl[ii].rWnd = rNew;

lblDONE:
   fInHere = FALSE;
}


int rwFindOrAddWnd (HWND hWnd, rwWindowData *awd)
{
            // Is the window handle listed in awl[] already?
            //
   int ii;
   for (ii = 0; ii < (int)cwl; ii++)
      {
      if (awl[ii].hWnd == hWnd)
         {
         if (awd)
            awl[ii].awdLast = awd;
         return ii;
         }
      }

            // No?  Then add it.
            //
   for (ii = 0; ii < (int)cwl; ii++)
      {
      if (awl[ii].hWnd == NULL)
         break;
      }
   if (ii == (int)cwl)
      {
      if (!REALLOC (awl, cwl, ii+1, 1))
         return (DWORD)-1;
      }

   awl[ii].hWnd = hWnd;
   awl[ii].awdLast = awd;

   if (IsWindow (hWnd))
      {
      GetWindowRect (hWnd, &awl[ii].rWnd);
      Subclass_AddHook (hWnd, Resize_DialogProc);
      }
   else
      {
      awl[ii].rWnd.left = 0;
      awl[ii].rWnd.right = 0;
      awl[ii].rWnd.top = 0;
      awl[ii].rWnd.bottom = 0;
      }
   awl[ii].cxDeltaCenter = 0;
   awl[ii].cyDeltaCenter = 0;

   return ii;
}


void rwFindAndRemoveWnd (HWND hWnd)
{
   for (size_t ii = 0; ii < (int)cwl; ii++)
      {
      if (awl[ii].hWnd == hWnd)
         {
         Subclass_RemoveHook (awl[ii].hWnd, hWnd);
         awl[ii].hWnd = NULL;
         return;
         }
      }
}


DWORD rwFindAction (rwWindowData *awd, int id)
{
   DWORD raDefault = raLeaveAlone;

   if (awd != NULL)
      {
      for (int ii = 0; awd[ii].id != idENDLIST; ++ii)
         {
         if (awd[ii].id == id)
            return awd[ii].ra;
         if (awd[ii].id == idDEFAULT)
            raDefault = awd[ii].ra;
         }
      }

   return raDefault;
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


/*
 * SPLITTERS __________________________________________________________________
 *
 */

static BOOL fRegistered = FALSE;	// TRUE If registered class

static TCHAR cszSplitterClassX[] = TEXT("SplitterWindowClassX");
static TCHAR cszSplitterClassY[] = TEXT("SplitterWindowClassY");


HWND CreateSplitter (HWND hWnd, int id1, int id2, int id,
                     LONG *pcd, rwWindowData *awd, BOOL fMovedAlready)
{
   SplitterData *psd;
   RECT          rWnd;
   BOOL          fX;

   if (!WhereShouldSplitterGo (hWnd, id1, id2, &rWnd, &fX))
      return NULL;

   EnsureSplitterRegistered ();

   psd = (SplitterData *)Allocate (sizeof(SplitterData));
   if (psd == NULL)
      return NULL;

   psd->pcDelta = pcd;
   psd->awd = awd;
   psd->fX = fX;
   psd->idWnd1 = id1;
   psd->idWnd2 = id2;
   psd->fMovedBeforeCreate = fMovedAlready;

   return CreateWindow(
              (fX) ? cszSplitterClassX : cszSplitterClassY,
              TEXT(""),	// Title
              WS_CHILD | WS_VISIBLE,	// Window style
              rWnd.left,	// Default horizontal position
              rWnd.top,	// Default vertical position
              rWnd.right  -rWnd.left,	// Default width
              rWnd.bottom -rWnd.top,	// Default height
              hWnd,	// Parent window
              (HMENU)id,	// Use ID given us by caller
              THIS_HINST,	// This instance owns this window
              (void *)psd	// Pointer not needed
              );
}

void DeleteSplitter (HWND hWnd, int id1)
{
   HWND hSplit;

   if (hSplit = GetDlgItem (hWnd, id1))
      {
      DestroyWindow (hSplit);
      }
}

void EnsureSplitterRegistered (void)
{
   WNDCLASS  wc;

   if (fRegistered)
      return;
   fRegistered = TRUE;

   wc.style         = CS_HREDRAW | CS_VREDRAW;
   wc.lpfnWndProc   = (WNDPROC)SplitterWndProc;
   wc.cbClsExtra    = 0;
   wc.cbWndExtra    = sizeof(SplitterData *);
   wc.hInstance     = THIS_HINST;
   wc.hIcon         = NULL;
   wc.hbrBackground = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ));
   wc.lpszMenuName  = NULL;

            // Register the X-moving class:
            //
#ifdef IDC_HSPLIT
   wc.hCursor       = LoadCursor (THIS_HINST, MAKEINTRESOURCE( IDC_HSPLIT ));
#else
   wc.hCursor       = LoadCursor (NULL, IDC_SIZEWE);
#endif

   wc.lpszClassName = cszSplitterClassX;

   (void)RegisterClass (&wc);

            // Register the Y-moving class:
            //
#ifdef IDC_VSPLIT
   wc.hCursor       = LoadCursor (THIS_HINST, MAKEINTRESOURCE( IDC_VSPLIT ));
#else
   wc.hCursor       = LoadCursor (NULL, IDC_SIZENS);
#endif

   wc.lpszClassName = cszSplitterClassY;

   (void)RegisterClass (&wc);

}


BOOL WhereShouldSplitterGo (HWND hWnd, int id1, int id2, RECT *prWnd, BOOL *pfX)
{
   RECT  r1, r2;
   BOOL  rc = TRUE;

   GetRectInParent (GetDlgItem (hWnd, id1), &r1);
   GetRectInParent (GetDlgItem (hWnd, id2), &r2);

   if (r2.left > r1.right)	// R1 on left, R2 on right?
      {
      *pfX = TRUE;
      prWnd->top    = min (r1.top,    r2.top);
      prWnd->bottom = max (r1.bottom, r2.bottom);
      prWnd->left   = r1.right;
      prWnd->right  = r2.left;
      }
   else if (r2.right < r1.left)	// R2 on left, R1 on right?
      {
      *pfX = TRUE;
      prWnd->top    = min (r1.top,    r2.top);
      prWnd->bottom = max (r1.bottom, r2.bottom);
      prWnd->left   = r2.right;
      prWnd->right  = r1.left;
      }
   else if (r2.top > r1.bottom)	// R1 on top, R2 on bottom?
      {
      *pfX = FALSE;
      prWnd->left   = min (r1.left,   r2.left);
      prWnd->right  = max (r1.right,  r2.right);
      prWnd->top    = r1.bottom;
      prWnd->bottom = r2.top;
      }
   else if (r2.bottom < r1.top)	// R2 on top, R1 on bottom?
      {
      *pfX = FALSE;
      prWnd->left   = min (r1.left,   r2.left);
      prWnd->right  = max (r1.right,  r2.right);
      prWnd->top    = r2.bottom;
      prWnd->bottom = r1.top;
      }
   else	// Rectangles intersect!
      {	// Don't know where it should go.
      rc = FALSE;
      }

   return rc;
}


LONG APIENTRY SplitterWndProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   SplitterData *psd;
   static LONG cdAtStart;

   if (msg == WM_CREATE)
      {
      SetWindowLong (hWnd,GWL_USER,(LONG)((LPCREATESTRUCT)lp)->lpCreateParams);
      }

   if ((psd = (SplitterData *)GetWindowLong (hWnd, GWL_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_CREATE:
               if (!psd->fMovedBeforeCreate)
                  ResizeSplitter (GetParent (hWnd), psd, 0, psd->pcDelta);
               psd->fDragging = FALSE;
               break;

         case WM_LBUTTONDOWN:
               if (!psd->fDragging)
                  {
                  SetCapture (hWnd);
                  psd->fDragging = TRUE;
                  cdAtStart = *psd->pcDelta;

                  GetCursorPos (&psd->ptStart);
                  ScreenToClient (GetParent (hWnd), &psd->ptStart);
                  }
               break;

         case WM_MOUSEMOVE:
               if (psd->fDragging)
                  {
                  POINT  pt;
                  LONG   cx, cy;
                  LONG   cd;

                  GetCursorPos (&pt);
                  ScreenToClient (GetParent (hWnd), &pt);

                  cx = (LONG)pt.x - (LONG)psd->ptStart.x;
                  cy = (LONG)pt.y - (LONG)psd->ptStart.y;

                  if (psd->fX)
                     cd = cdAtStart + cx;
                  else // (!psd->fX)
                     cd = cdAtStart + cy;

                  if (cd != *(psd->pcDelta))
                     {
                     ResizeSplitter (GetParent(hWnd), psd, *psd->pcDelta, &cd);
                     *psd->pcDelta = cd;
                     }
                  }
               break;

         case WM_LBUTTONUP:
               if (psd->fDragging)
                  {
                  ReleaseCapture ();
                  psd->fDragging = FALSE;
                  }
               break;

         case WM_DESTROY:
               if (psd->fDragging)
                  {
                  ReleaseCapture ();
                  psd->fDragging = FALSE;

                  Free (psd);
                  psd = NULL;	// fault blatantly if you use {psd} now
                  SetWindowLong (hWnd, GWL_USER, 0);
                  }
               break;

#if 0 // Enable me to make the splitters draw in black
         case WM_PAINT:
               {
               PAINTSTRUCT ps;
               HDC hdc = BeginPaint (hWnd, &ps);
               FillRect (hdc, &ps.rcPaint, GetStockObject(BLACK_BRUSH));
               EndPaint (hWnd, &ps);
               return 0;
               }
               break;
#endif
         }
      }

   return DefWindowProc (hWnd, msg, wp, lp);
}


void ResizeSplitter (HWND hWnd, SplitterData *psd, LONG cOld, LONG *pcNew)
{
   LONG   dx, dy;
   HWND   hItem;
   size_t nItems = 0;
   LONG   cdMin, cdMax;

   if (psd == NULL || pcNew == NULL || hWnd == NULL)
      return;

   FindSplitterMinMax (hWnd, psd, cOld, &cdMin, &cdMax);
   *pcNew = limit (cdMin, *pcNew, cdMax);

   if (*pcNew == cOld)
      return;

   dx = (psd->fX) ? (*pcNew - cOld) : 0;
   dy = (psd->fX) ? 0 : (*pcNew - cOld);

   for (hItem = GetWindow (hWnd, GW_CHILD);
        hItem != NULL;
        hItem = GetWindow (hItem, GW_HWNDNEXT))
      {
      nItems++;
      }

   if (nItems != 0)
      {
      BOOL fRepaint = FALSE;
      HDWP dwp = BeginDeferWindowPos (nItems);

      for (hItem = GetWindow (hWnd, GW_CHILD);
           hItem != NULL;
           hItem = GetWindow (hItem, GW_HWNDNEXT))
         {
         RECT   rItem;
         DWORD  ra;

         GetRectInParent (hItem, &rItem);

         ra = rwFindAction (psd->awd, (GetWindowLong (hItem, GWL_ID)));

         DeferWindowPos (dwp, hItem, NULL,
                         rItem.left + ((ra & raMoveX)  ? dx :
                                      (ra & raMoveXB) ? (0-dx) : 0),
                         rItem.top  + ((ra & raMoveY)  ? dy :
                                      (ra & raMoveYB) ? (0-dy) : 0),
                         rItem.right -rItem.left
                                   + ((ra & raSizeX)  ? dx :
                                      (ra & raSizeXB) ? (0-dx) : 0),
                         rItem.bottom -rItem.top
                                   + ((ra & raSizeY)  ? dy :
                                      (ra & raSizeYB) ? (0-dy) : 0),
                         SWP_NOACTIVATE | SWP_NOZORDER);
         }

      EndDeferWindowPos (dwp);

      for (hItem = GetWindow (hWnd, GW_CHILD);
           hItem != NULL;
           hItem = GetWindow (hItem, GW_HWNDNEXT))
         {
         DWORD ra = rwFindAction (psd->awd, (GetWindowLong (hItem, GWL_ID)));

         if (ra & raRepaint)
            {
            RECT  rItem;

            GetRectInParent (hItem, &rItem);

            InvalidateRect (hWnd, &rItem, TRUE);
            fRepaint = TRUE;
            }
         }

      if (fRepaint)
         {
         UpdateWindow (hWnd);
         }
      }
}


void FindSplitterMinMax (HWND hWnd, SplitterData *psd, LONG cOld, LONG *pcdMin, LONG *pcdMax)
{
   *pcdMin = 0;
   *pcdMax = 0;

   for (int ii = 0; psd->awd[ ii ].id != idENDLIST; ++ii)
      {
      HWND hControl;
      if ((hControl = GetDlgItem (hWnd, psd->awd[ ii ].id)) != NULL)
         {
         RECT rControl;
         GetRectInParent (hControl, &rControl);

         if (psd->fX)
            {
            LONG cxMin = 0;
            LONG cxMax = 0;

            if (LOWORD(psd->awd[ ii ].cMinimum)) // X minimum?
               {
               if (psd->awd[ ii ].ra & raSizeX)
                  cxMin = cOld - cxRECT(rControl) + LOWORD(psd->awd[ ii ].cMinimum);
               else if (psd->awd[ ii ].ra & raSizeXB)
                  cxMax = cOld + cxRECT(rControl) - LOWORD(psd->awd[ ii ].cMinimum);
               else if (psd->awd[ ii ].ra & raSizeXC)
                  cxMin = cOld - (cxRECT(rControl) - LOWORD(psd->awd[ ii ].cMinimum))*2;
               }

            if (LOWORD(psd->awd[ ii ].cMaximum)) // X maximum?
               {
               if (psd->awd[ ii ].ra & raSizeX)
                  cxMax = cOld - cxRECT(rControl) + LOWORD(psd->awd[ ii ].cMaximum);
               else if (psd->awd[ ii ].ra & raSizeXB)
                  cxMin = cOld + cxRECT(rControl) - LOWORD(psd->awd[ ii ].cMaximum);
               else if (psd->awd[ ii ].ra & raSizeXC)
                  cxMin = cOld - (cxRECT(rControl) - LOWORD(psd->awd[ ii ].cMaximum))*2;
               }

            if (cxMin)  *pcdMin = (*pcdMin) ? max( *pcdMin, cxMin ) : cxMin;
            if (cxMax)  *pcdMax = (*pcdMax) ? min( *pcdMax, cxMax ) : cxMax;
            }
         else
            {
            LONG cyMin = 0;
            LONG cyMax = 0;

            if (HIWORD(psd->awd[ ii ].cMinimum)) // Y minimum?
               {
               if (psd->awd[ ii ].ra & raSizeY)
                  cyMin = cOld - cyRECT(rControl) + HIWORD(psd->awd[ ii ].cMinimum);
               else if (psd->awd[ ii ].ra & raSizeYB)
                  cyMax = cOld + cyRECT(rControl) - HIWORD(psd->awd[ ii ].cMinimum);
               else if (psd->awd[ ii ].ra & raSizeYC)
                  cyMin = cOld - (cyRECT(rControl) - HIWORD(psd->awd[ ii ].cMinimum))*2;
               }

            if (HIWORD(psd->awd[ ii ].cMaximum)) // Y maximum?
               {
               if (psd->awd[ ii ].ra & raSizeY)
                  cyMax = cOld - cyRECT(rControl) + HIWORD(psd->awd[ ii ].cMaximum);
               else if (psd->awd[ ii ].ra & raSizeYB)
                  cyMin = cOld + cyRECT(rControl) - HIWORD(psd->awd[ ii ].cMaximum);
               else if (psd->awd[ ii ].ra & raSizeYC)
                  cyMin = cOld - (cyRECT(rControl) - HIWORD(psd->awd[ ii ].cMaximum))*2;
               }

            if (cyMin)  *pcdMin = (*pcdMin) ? max( *pcdMin, cyMin ) : cyMin;
            if (cyMax)  *pcdMax = (*pcdMax) ? min( *pcdMax, cyMax ) : cyMax;
            }
         }
      }
}


void FindResizeLimits (HWND hWnd, LONG *pcxMin, LONG *pcxMax, LONG *pcyMin, LONG *pcyMax, rwWindowData *awd)
{
   *pcxMin = 0;
   *pcxMax = 0;
   *pcyMin = 0;
   *pcyMax = 0;

   if (awd == NULL)
      {
      int iwl;
      if ((iwl = rwFindOrAddWnd (hWnd)) == -1)
         return;

      if ((awd = awl[ iwl ].awdLast) == NULL)
         return;
      }

   RECT rNow;
   GetWindowRect (hWnd, &rNow);

   for (DWORD ii = 0; awd[ ii ].id != idENDLIST; ++ii)
      {
      HWND hControl;
      if ((hControl = GetDlgItem (hWnd, awd[ ii ].id)) != NULL)
         {
         RECT rControl;
         GetRectInParent (hControl, &rControl);

         LONG cxMin = 0;
         LONG cyMin = 0;
         LONG cxMax = 0;
         LONG cyMax = 0;

         if (LOWORD(awd[ ii ].cMinimum)) // X minimum?
            {
            if (awd[ ii ].ra & raSizeX)
               cxMin = cxRECT(rNow) - cxRECT(rControl) + LOWORD(awd[ ii ].cMinimum);
            else if (awd[ ii ].ra & raSizeXB)
               cxMax = cxRECT(rNow) + cxRECT(rControl) - LOWORD(awd[ ii ].cMinimum);
            else if (awd[ ii ].ra & raSizeXC)
               cxMin = cxRECT(rNow) - (cxRECT(rControl) - LOWORD(awd[ ii ].cMinimum))*2;
            }

         if (LOWORD(awd[ ii ].cMaximum)) // X maximum?
            {
            if (awd[ ii ].ra & raSizeX)
               cxMax = cxRECT(rNow) - cxRECT(rControl) + LOWORD(awd[ ii ].cMaximum);
            else if (awd[ ii ].ra & raSizeXB)
               cxMin = cxRECT(rNow) + cxRECT(rControl) - LOWORD(awd[ ii ].cMaximum);
            else if (awd[ ii ].ra & raSizeXC)
               cxMax = cxRECT(rNow) - (cxRECT(rControl) - LOWORD(awd[ ii ].cMaximum))*2;
            }

         if (HIWORD(awd[ ii ].cMinimum)) // Y minimum?
            {
            if (awd[ ii ].ra & raSizeY)
               cyMin = cyRECT(rNow) - cyRECT(rControl) + HIWORD(awd[ ii ].cMinimum);
            else if (awd[ ii ].ra & raSizeYB)
               cyMax = cyRECT(rNow) + cyRECT(rControl) - HIWORD(awd[ ii ].cMinimum);
            else if (awd[ ii ].ra & raSizeYC)
               cyMin = cyRECT(rNow) - (cyRECT(rControl) - HIWORD(awd[ ii ].cMinimum))*2;
            }

         if (HIWORD(awd[ ii ].cMaximum)) // Y maximum?
            {
            if (awd[ ii ].ra & raSizeY)
               cyMax = cyRECT(rNow) - cyRECT(rControl) + HIWORD(awd[ ii ].cMaximum);
            else if (awd[ ii ].ra & raSizeYB)
               cyMin = cyRECT(rNow) + cyRECT(rControl) - HIWORD(awd[ ii ].cMaximum);
            else if (awd[ ii ].ra & raSizeYC)
               cyMax = cyRECT(rNow) - (cyRECT(rControl) - HIWORD(awd[ ii ].cMaximum))*2;
            }

         if (cxMin)  *pcxMin = (*pcxMin) ? max( *pcxMin, cxMin ) : cxMin;
         if (cyMin)  *pcyMin = (*pcyMin) ? max( *pcyMin, cyMin ) : cyMin;
         if (cxMax)  *pcxMax = (*pcxMax) ? min( *pcxMax, cxMax ) : cxMax;
         if (cyMax)  *pcyMax = (*pcyMax) ? min( *pcyMax, cyMax ) : cyMax;
         }
      }
}


BOOL CALLBACK Resize_DialogProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID fnNext = Subclass_FindNextHook (hWnd, Resize_DialogProc);

   switch (msg)
      {
      case WM_GETMINMAXINFO:
         LONG cxMin;
         LONG cyMin;
         LONG cxMax;
         LONG cyMax;
         FindResizeLimits (hWnd, &cxMin, &cxMax, &cyMin, &cyMax);

         LPMINMAXINFO lpmmi;
         lpmmi = (LPMINMAXINFO)lp;

         if (cxMin)
            lpmmi->ptMinTrackSize.x = cxMin;
         if (cyMin)
            lpmmi->ptMinTrackSize.y = cyMin;
         if (cxMax)
            lpmmi->ptMaxTrackSize.x = cxMax;
         if (cyMax)
            lpmmi->ptMaxTrackSize.y = cyMax;
         return FALSE;

      case WM_DESTROY:
         rwFindAndRemoveWnd (hWnd);
         break;
      }

   return (fnNext) ? CallWindowProc ((WNDPROC)fnNext, hWnd, msg, wp, lp) : FALSE;
}

