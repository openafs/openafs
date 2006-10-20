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

#define NO_DEBUG_ALLOC // Turn off memory-allocation instrumentation for this

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <WINNT/fastlist.h>
#include <WINNT/subclass.h>
#include <WINNT/hashlist.h>
#include <WINNT/TaLocale.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define clrTRANSPARENT PALETTERGB(255,0,255)

typedef struct _FASTLISTITEM
   {
   LPARAM lpUser;
   int index;

   int iFirstImage;
   int iSecondImage;

   HLISTITEM hTreeParent;
   HLISTITEM hTreeChild;
   HLISTITEM hTreePrevious;
   HLISTITEM hTreeNext;
   HLISTITEM hListPrevious;
   HLISTITEM hListNext;
   HLISTITEM hSelectPrevious;
   HLISTITEM hSelectNext;

   BOOL fExpanded;
   BOOL fVisible;
   DWORD dwFlags;
   BOOL fSelected;
   BOOL fFocused;

   LPTSTR *apszText;
   size_t  cpszText;
   } FASTLISTITEM, *LPFASTLISTITEM, LISTITEM, *LPLISTITEM;


        // We over-allocate the aVisibleHeap[] array whenever it fills up,
        // in order to perform fewer allocations. The allocation size
        // defined by the macros below causes the following progression
        // of array sizes:
        //
        //    128, 256, 512, 1024, 2048, 4096, 8192, 16384, 24576, 32768, ...
        //
#define cREALLOC_VISIBLEHEAP_MIN   128
#define cREALLOC_VISIBLEHEAP_MAX  4096
#define cREALLOC_VISIBLEHEAP(pfl) min(max(pfl->cVisibleHeap, cREALLOC_VISIBLEHEAP_MIN),cREALLOC_VISIBLEHEAP_MAX)

#define dwSigFASTLIST  TEXT('FAST')

typedef struct FASTLIST
   {
   DWORD dwSig;
   HWND hList;
   HWND hHeader;
   HWND hScrollH;
   HWND hScrollV;
   DWORD dwStyle;
   LONG dxPixel;
   LONG dyPixel;
   CRITICAL_SECTION cs;
   HFONT hf;
   BOOL fSortBeforePaint;
   BOOL fSyncScrollBeforePaint;
   BOOL fSyncIndicesBeforePaint;
   BOOL fRepaintRequired;
   BOOL fSyncHeaderRequired;
   size_t nBegin;
   HIMAGELIST hilSmall;
   HIMAGELIST hilLarge;
   LPFASTLISTTEXTCALLBACK pfnText;
   DWORD dwCookieText;
   LPFASTLISTSORTFUNC pfnSort;
   int iColSort;
   BOOL fRevSort;
   HLISTITEM hEnsureVisible;

   LPHASHLIST lItems;
   LPHASHLISTKEY lkUserParam;
   HLISTITEM hTreeFirst;
   HLISTITEM hListFirst;
   HLISTITEM hSelectFirst;
   HLISTITEM hFocused;
   HLISTITEM hAnchor;

   BOOL fButtonDown;
   BOOL fRightDrag;
   BOOL fDragging;
   BOOL fTriedToDrag;
   POINT ptScreenDown;
   HLISTITEM hHitOnDown;
   BOOL fSelectedOnDown;

   HD_ITEM *aColumns;
   size_t cColumns;

   HLISTITEM *aVisibleHeap;
   size_t cVisibleHeap;
   size_t cVisible;
   } FASTLIST, *LPFASTLIST;


/*
 * GLOBAL TO FASTLIST _________________________________________________________
 *
 */

static struct FASTLIST_GLOBAL
   {
   HBITMAP bmp;
   LONG cxBmp;
   LONG cyBmp;
   HLISTITEM *aObjects;
   size_t cObjects;
   CRITICAL_SECTION cs;
   LPFASTLIST pfl;
   } fg;

#define cREALLOC_OBJECTHEAP   512


/*
 * DISPLAY CONSTANTS __________________________________________________________
 *
 */

#ifndef cxRECT
#define cxRECT(_r) ((_r).right - (_r).left)
#endif

#ifndef cyRECT
#define cyRECT(_r) ((_r).bottom - (_r).top)
#endif

#ifndef limit
#define limit(_a,_x,_b) min(max((_x),(_a)),(_b))
#endif

#ifndef DivRoundUp
#define DivRoundUp(_a,_b)  (((_a) + (_b) -1) / (_b))
#endif

#define cyABOVE_LARGE_ICON      2
#define cyBELOW_LARGE_ICON      1
#define cyABOVE_LARGE_TEXT      1
#define cyBELOW_LARGE_TEXT      1
#define cxBEFORE_LARGE_TEXT     1
#define cxAFTER_LARGE_TEXT      2

#define cyABOVE_SMALL_TEXT      1
#define cyBELOW_SMALL_TEXT      1
#define cxBEFORE_SMALL_ICON     2
#define cxAFTER_SMALL_ICON      2
#define cxBEFORE_SMALL_TEXT     1
#define cxAFTER_SMALL_TEXT      2

#define cxBEFORE_LIST_ICON      2
#define cxAFTER_LIST_ICON       2
#define cyABOVE_LIST_TEXT       1
#define cyBELOW_LIST_TEXT       1
#define cxBETWEEN_LIST_ICONS    2
#define cxBEFORE_LIST_TEXT      1
#define cxAFTER_LIST_TEXT       4
#define cxBEFORE_COLUMN_TEXT    5

#define cxSPACE_TREELINE        2
#define cySPACE_TREELINE        2
#define cxTREE_BOX              8
#define cyTREE_BOX              8

#define TREELINE_HORIZONTAL     0x00000001
#define TREELINE_BOX            0x00000002
#define TREELINE_BOXOPEN       (TREELINE_BOX)
#define TREELINE_BOXCLOSED     (0x00000004 | TREELINE_BOX)
#define TREELINE_UP             0x00000008
#define TREELINE_DOWN           0x00000010


/*
 * VARIABLES __________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

#define fIsShiftDown()   (GetKeyState(VK_SHIFT) < 0)
#define fIsControlDown() (GetKeyState(VK_CONTROL) < 0)

BOOL OpenGlobalBitmap (HDC hdc, RECT *prClient);
void CloseGlobalBitmap (void);

BOOL OpenGlobalArray (size_t cObjects);
void CloseGlobalArray (void);

BOOL CALLBACK FastList_ControlProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK FastList_ParentProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void FastList_OnCreate (HWND hList);
void FastList_OnDestroy (HWND hList);
void FastList_OnStyleChange (HWND hList);
void FastList_OnSize (HWND hList);
void FastList_OnPaint (HWND hList);
void FastList_OnPaintItem (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi);
BOOL FastList_OnPaintItem_DrawImage (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, HIMAGELIST hil, int iImage, LONG xImage, LONG yImage, BOOL fHLines);
void FastList_OnPaintItem_GetItemColors (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, COLORREF *pclrFore, COLORREF *pclrBack);
void FastList_OnPaintItem_TreeLines (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, DWORD dwLines, RECT *prLines);
void FastList_OnPaintItem_Large (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi);
void FastList_OnPaintItem_Small (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi);
void FastList_OnPaintItem_Tree (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, HLISTITEM hItem, RECT *prThisColumn);
void FastList_OnPaintItem_List (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi);
void FastList_OnScroll (HWND hList, UINT msg, WPARAM wp, LPARAM lp);
void FastList_OnRightButtonDown (HWND hList);
void FastList_OnLeftButtonDown (HWND hList);
void FastList_OnLeftButtonDouble (HWND hList);
void FastList_OnKeyPress (HWND hList, TCHAR ch);
BOOL FastList_OnKeyPress_EnsureSelection (LPFASTLIST pfl);
void FastList_OnKeyPress_ChangeSelection (LPFASTLIST pfl, HLISTITEM hSelect);

void FastList_OnCommand_Begin (HWND hList);
void FastList_OnCommand_End (HWND hList, BOOL fForce);
HLISTITEM FastList_OnCommand_AddItem (HWND hList, LPFASTLISTADDITEM pai);
void FastList_OnCommand_RemoveItem (HWND hList, HLISTITEM hItem);
LPCTSTR FastList_OnCommand_GetItemText (HWND hList, HLISTITEM hItem, int iColumn);
void FastList_OnCommand_SetItemText (HWND hList, LPFASTLISTITEMCOLUMN pflic, LPCTSTR pszText);
LPARAM FastList_OnCommand_GetItemParam (HWND hList, HLISTITEM hItem);
void FastList_OnCommand_SetItemParam (HWND hList, HLISTITEM hItem, LPARAM lpUser);
DWORD FastList_OnCommand_GetItemFlags (HWND hList, HLISTITEM hItem);
void FastList_OnCommand_SetItemFlags (HWND hList, HLISTITEM hItem, DWORD dwFlags);
int FastList_OnCommand_GetItemImage (HWND hList, HLISTITEM hItem, int iImage);
void FastList_OnCommand_SetItemImage (HWND hList, LPFASTLISTITEMIMAGE pflii, int iImage);
void FastList_OnCommand_GetImageLists (HWND hList, HIMAGELIST *phiSmall, HIMAGELIST *phiLarge);
void FastList_OnCommand_SetImageLists (HWND hList, HIMAGELIST hiSmall, HIMAGELIST hiLarge);
HIMAGELIST FastList_OnCommand_CreateDragImage (HWND hList, HLISTITEM hItem);
LPFASTLISTSORTFUNC FastList_OnCommand_GetSortFunc (HWND hList);
void FastList_OnCommand_SetSortFunc (HWND hList, LPFASTLISTSORTFUNC pfn);
void FastList_OnCommand_GetSortStyle (HWND hList, int *piColSort, BOOL *pfRevSort);
void FastList_OnCommand_SetSortStyle (HWND hList, int iColSort, BOOL fRevSort);
void FastList_OnCommand_Sort (HWND hList);
int FastList_OnCommand_GetColumnCount (HWND hList);
BOOL FastList_OnCommand_GetColumn (HWND hList, int iColumn, LPFASTLISTCOLUMN pcol);
void FastList_OnCommand_SetColumn (HWND hList, int iColumn, LPFASTLISTCOLUMN pcol);
BOOL FastList_OnCommand_IsSelected (HWND hList, HLISTITEM hItem);
void FastList_OnCommand_SelectItem (HWND hList, HLISTITEM hItem, BOOL fSelect);
HLISTITEM FastList_OnCommand_FindList (HWND hList, HLISTITEM hItem, DWORD dwCode);
HLISTITEM FastList_OnCommand_FindTree (HWND hList, HLISTITEM hItem, DWORD dwCode);
HLISTITEM FastList_OnCommand_FindSelected (HWND hList, HLISTITEM hItem);
HLISTITEM FastList_OnCommand_FindItem (HWND hList, LPENUM *ppEnum, LPARAM lpUser);
HLISTITEM FastList_OnCommand_FindNextItem (HWND hList, LPENUM *ppEnum);
HLISTITEM FastList_OnCommand_FindVisible (HWND hList, HLISTITEM hItem, DWORD dwCode);
void FastList_OnCommand_FindClose (HWND hList, LPENUM *ppEnum);
int FastList_OnCommand_GetItemCount (HWND hList, BOOL fVisibleOnly);
BOOL FastList_OnCommand_IsExpanded (HWND hList, HLISTITEM hItem);
void FastList_OnCommand_Expand (HWND hList, HLISTITEM hItem, BOOL fExpand);
BOOL FastList_OnCommand_ItemVisible (HWND hList, HLISTITEM hItem, BOOL fSet);
HLISTITEM FastList_OnCommand_ItemFocus (HWND hList, HLISTITEM hItem, BOOL fSet);
HLISTITEM FastList_OnCommand_ItemFromPoint (HWND hList, POINT *pptClient, BOOL fStrict);
void FastList_OnCommand_GetItemRegions (HWND hList, HLISTITEM hItem, LPFASTLISTITEMREGIONS pReg, POINT *pptTest = NULL, BOOL *pfHit = NULL);
LPFASTLISTTEXTCALLBACK FastList_OnCommand_GetTextCallback (HWND hList);
void FastList_OnCommand_SetTextCallback (HWND hList, LPFASTLISTTEXTCALLBACK pfn, DWORD dwCookie);

BOOL FastList_Notify_GetItemText (LPFASTLIST pfl, HLISTITEM hItem, int icol, LPTSTR pszText, size_t cchText, size_t *pcchRequired);
BOOL FastList_Notify_ItemChanged (LPFASTLIST pfl, HLISTITEM hItem);
BOOL FastList_Notify_AddItem (LPFASTLIST pfl, HLISTITEM hItem);
BOOL FastList_Notify_RemoveItem (LPFASTLIST pfl, HLISTITEM hItem);
BOOL FastList_Notify_ColumnClick (LPFASTLIST pfl, int icol, BOOL fDouble);
BOOL FastList_Notify_ColumnResize (LPFASTLIST pfl, int icol, LONG cxWidth);
BOOL FastList_Notify_ItemSelect (LPFASTLIST pfl);
BOOL FastList_Notify_ItemExpand (LPFASTLIST pfl, HLISTITEM hItem);
BOOL FastList_Notify_Generic (LPFASTLIST pfl, DWORD dwCode);
BOOL FastList_Notify_Drag (LPFASTLIST pfl, DWORD dwCode, LPFLN_DRAG_PARAMS pfln);

void FastList_Repaint (LPFASTLIST pfl);
HLISTITEM FastList_CreateItem (LPFASTLIST pfl);
void FastList_DeleteItem (LPFASTLIST pfl, HLISTITEM hItem);
int FastList_GetListHeight (LPFASTLIST pfl);
int FastList_GetListWidth (LPFASTLIST pfl);
void FastList_CalcItemRect (LPFASTLIST pfl, int index, RECT *prItem, BOOL fScrollH, BOOL fScrollV);
void FastList_CalcFieldSize (LPFASTLIST pfl, LONG *pcxField, LONG *pcyField, BOOL fScrollH, BOOL fScrollV);
void FastList_GetHeaderRect (LPFASTLIST pfl, RECT *prHeader);
LONG FastList_GetHeaderHeight (LPFASTLIST pfl);
void FastList_UpdateColumn (LPFASTLIST pfl, int iColumn);
void FastList_SyncHeader (LPFASTLIST pfl);
void FastList_SyncScroll (LPFASTLIST pfl);
void FastList_SyncScrollPos (LPFASTLIST pfl);
void FastList_CallPaintItem (LPFASTLIST pfl, HDC hdc, BOOL fDraw, BOOL fDragImage, int iItem, RECT *prItem, LPFASTLISTITEMREGIONS pReg, POINT *pptTest, BOOL *pfHit);
LPTSTR FastList_GetColumnText (LPFASTLIST pfl, HLISTITEM hItem, int icol, BOOL fAlternateBuffer = FALSE);
void FastList_PerformSort (LPFASTLIST pfl);
void FastList_PerformSortLevel (LPFASTLIST pfl, HLISTITEM hParent, BOOL fTreeSort);
int __cdecl FastList_SortFunction (const void *lp1, const void *lp2);
void FastList_RepairOneVisibilityFlag (LPFASTLIST pfl, HLISTITEM hItem);
void FastList_RepairVisibilityFlags (LPFASTLIST pfl, HLISTITEM hParent = NULL, BOOL fForceHidden = FALSE);
void FastList_RepairVisibilityIndices (LPFASTLIST pfl, HLISTITEM hParent = NULL);
void FastList_ScrollHeader (LPFASTLIST pfl);
void FastList_PerformSelectTest (LPFASTLIST pfl, HLISTITEM hItem);
void FastList_PerformSelectItem (LPFASTLIST pfl, HLISTITEM hItem, BOOL fSelect);
void FastList_PerformSelectRange (LPFASTLIST pfl, HLISTITEM hItem1, HLISTITEM hItem2);
void FastList_PerformEnsureVisible (LPFASTLIST pfl);
void FastList_ButtonDown (HWND hList, BOOL fRightButton, BOOL fActivate);
void FastList_MouseMove (HWND hList);
void FastList_ButtonUp (HWND hList, BOOL fRightButton, BOOL fActivate);

LPFASTLIST GetFastList (HWND hList);

BOOL CALLBACK FastList_KeyUserParam_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
HASHVALUE CALLBACK FastList_KeyUserParam_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
HASHVALUE CALLBACK FastList_KeyUserParam_HashData (LPHASHLISTKEY pKey, PVOID pData);


/*
 * REALLOC ____________________________________________________________________
 *
 */

#ifdef REALLOC
#undef REALLOC
#endif
#define REALLOC(_a,_c,_r,_i) FastList_ReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL FastList_ReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
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


/*
 * WINDOW CLASS SUPPORT _______________________________________________________
 *
 */

BOOL RegisterFastListClass (void)
{
   static BOOL fRegistered = FALSE;

   if (!fRegistered)
      {
      memset (&fg, 0x00, sizeof(fg));
      InitializeCriticalSection (&fg.cs);

      WNDCLASS wc;
      GetClassInfo (THIS_HINST, TEXT("LISTBOX"), &wc);
      wc.style = CS_GLOBALCLASS | CS_DBLCLKS;
      wc.cbWndExtra = 4;
      wc.lpfnWndProc = (WNDPROC)FastList_ControlProc;
      wc.hInstance = THIS_HINST;
      wc.hCursor = LoadCursor (NULL, MAKEINTRESOURCE (IDC_ARROW));
      wc.hbrBackground = NULL;
      wc.lpszClassName = WC_FASTLIST;

      if (RegisterClass (&wc))
         fRegistered = TRUE;
      }

   return fRegistered;
}


BOOL OpenGlobalBitmap (HDC hdc, RECT *prClient)
{
   EnterCriticalSection (&fg.cs);

   if ((!fg.bmp) || (fg.cxBmp < prClient->right) || (fg.cyBmp < prClient->bottom))
      {
      if (fg.bmp)
         DeleteObject (fg.bmp);

      fg.cxBmp = 128 * DivRoundUp (prClient->right, 128);
      fg.cyBmp =  64 * DivRoundUp (prClient->bottom, 64);
      fg.bmp = CreateCompatibleBitmap (hdc, fg.cxBmp, fg.cyBmp);
      }

   if (!fg.bmp)
      {
      LeaveCriticalSection (&fg.cs);
      return FALSE;
      }

   return TRUE;
}


void CloseGlobalBitmap (void)
{
   LeaveCriticalSection (&fg.cs);
}


BOOL OpenGlobalArray (size_t cObjects)
{
   EnterCriticalSection (&fg.cs);
   REALLOC (fg.aObjects, fg.cObjects, cObjects, cREALLOC_OBJECTHEAP);
   memset (fg.aObjects, 0x00, sizeof(HLISTITEM) * fg.cObjects);
   return TRUE;
}


void CloseGlobalArray (void)
{
   LeaveCriticalSection (&fg.cs);
}


BOOL CALLBACK FastList_ControlProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_CREATE:
         FastList_OnCreate (hList);
         if (GetParent (hList))
            Subclass_AddHook (GetParent (hList), FastList_ParentProc);
         break;

      case WM_DESTROY:
         if (GetParent (hList))
            Subclass_RemoveHook (GetParent (hList), FastList_ParentProc);
         FastList_OnDestroy (hList);
         break;

      case WM_GETDLGCODE:
         return DLGC_WANTARROWS;

      case WM_STYLECHANGED:
         FastList_OnStyleChange (hList);
         break;

      case WM_ENABLE:
         FastList_Repaint (GetFastList (hList));
         break;

      case WM_SIZE:
         FastList_OnSize (hList);
         break;

      case WM_PAINT:
         FastList_OnPaint (hList);
         break;

      case WM_LBUTTONDOWN:
         FastList_OnLeftButtonDown (hList);
         break;

      case WM_RBUTTONDOWN:
         FastList_OnRightButtonDown (hList);
         break;

      case WM_LBUTTONUP:
         FastList_ButtonUp (hList, FALSE, FALSE);
         break;

      case WM_RBUTTONUP:
         FastList_ButtonUp (hList, TRUE, FALSE);
         break;

      case WM_MOUSEMOVE:
         FastList_MouseMove (hList);
         break;

      case WM_LBUTTONDBLCLK:
         FastList_OnLeftButtonDouble (hList);
         break;

      case WM_KEYDOWN:
         FastList_OnKeyPress (hList, (TCHAR)wp);
         break;

      case WM_NOTIFY:
         switch (((NMHDR*)lp)->code)
            {
            case HDN_ITEMCLICK:
               FastList_Notify_ColumnClick (GetFastList (hList), ((HD_NOTIFY*)lp)->iItem, FALSE);
               break;

            case HDN_ITEMDBLCLICK:
               FastList_Notify_ColumnClick (GetFastList (hList), ((HD_NOTIFY*)lp)->iItem, TRUE);
               break;

            case HDN_ITEMCHANGED:
            case HDN_ENDTRACK:
            case HDN_TRACK:
               FastList_UpdateColumn (GetFastList (hList), ((HD_NOTIFY*)lp)->iItem);
               break;
            }
         break;

      case WM_HSCROLL:
      case WM_VSCROLL:
         FastList_OnScroll (hList, msg, wp, lp);
         break;

      case FLM_BEGIN:
         FastList_OnCommand_Begin (hList);
         return TRUE;

      case FLM_END:
         FastList_OnCommand_End (hList, (BOOL)wp);
         return TRUE;

      case FLM_ADDITEM:
         return (BOOL)FastList_OnCommand_AddItem (hList, (LPFASTLISTADDITEM)lp);

      case FLM_REMOVEITEM:
         FastList_OnCommand_RemoveItem (hList, (HLISTITEM)wp);
         return TRUE;

      case FLM_GETITEMTEXT:
         return (BOOL)FastList_OnCommand_GetItemText (hList, (HLISTITEM)wp, (int)lp);

      case FLM_SETITEMTEXT:
         FastList_OnCommand_SetItemText (hList, (LPFASTLISTITEMCOLUMN)wp, (LPCTSTR)lp);
         return TRUE;

      case FLM_GETITEMPARAM:
         return (BOOL)FastList_OnCommand_GetItemParam (hList, (HLISTITEM)wp);

      case FLM_SETITEMPARAM:
         FastList_OnCommand_SetItemParam (hList, (HLISTITEM)wp, lp);
         return TRUE;

      case FLM_GETITEMFLAGS:
         return (BOOL)FastList_OnCommand_GetItemFlags (hList, (HLISTITEM)wp);

      case FLM_SETITEMFLAGS:
         FastList_OnCommand_SetItemFlags (hList, (HLISTITEM)wp, (DWORD)lp);
         return TRUE;

      case FLM_GETITEMIMAGE:
         return (BOOL)FastList_OnCommand_GetItemImage (hList, (HLISTITEM)wp, (int)lp);

      case FLM_SETITEMIMAGE:
         FastList_OnCommand_SetItemImage (hList, (LPFASTLISTITEMIMAGE)wp, (int)lp);
         return TRUE;

      case FLM_ISEXPANDED:
         return (BOOL)FastList_OnCommand_IsExpanded (hList, (HLISTITEM)wp);

      case FLM_EXPAND:
         FastList_OnCommand_Expand (hList, (HLISTITEM)wp, (BOOL)lp);
         return TRUE;

      case FLM_ITEMVISIBLE:
         return (BOOL)FastList_OnCommand_ItemVisible (hList, (HLISTITEM)wp, (BOOL)lp);

      case FLM_ITEMFOCUS:
         return (BOOL)FastList_OnCommand_ItemFocus (hList, (HLISTITEM)wp, (BOOL)lp);

      case FLM_GETIMAGELISTS:
         FastList_OnCommand_GetImageLists (hList, (HIMAGELIST*)wp, (HIMAGELIST*)lp);
         return TRUE;

      case FLM_SETIMAGELISTS:
         FastList_OnCommand_SetImageLists (hList, (HIMAGELIST)wp, (HIMAGELIST)lp);
         return TRUE;

      case FLM_CREATEDRAGIMAGE:
         return (BOOL)FastList_OnCommand_CreateDragImage (hList, (HLISTITEM)wp);

      case FLM_GETSORTFUNC:
         return (BOOL)FastList_OnCommand_GetSortFunc (hList);

      case FLM_SETSORTFUNC:
         FastList_OnCommand_SetSortFunc (hList, (LPFASTLISTSORTFUNC)lp);
         return TRUE;

      case FLM_GETSORTSTYLE:
         FastList_OnCommand_GetSortStyle (hList, (int*)wp, (BOOL*)lp);
         return TRUE;

      case FLM_SETSORTSTYLE:
         FastList_OnCommand_SetSortStyle (hList, (int)wp, (BOOL)lp);
         return TRUE;

      case FLM_SORT:
         FastList_OnCommand_Sort (hList);
         return TRUE;

      case FLM_GETCOLUMNCOUNT:
         return (BOOL)FastList_OnCommand_GetColumnCount (hList);

      case FLM_GETCOLUMN:
         return FastList_OnCommand_GetColumn (hList, (int)wp, (LPFASTLISTCOLUMN)lp);

      case FLM_SETCOLUMN:
         FastList_OnCommand_SetColumn (hList, (int)wp, (LPFASTLISTCOLUMN)lp);
         return TRUE;

      case FLM_ISSELECTED:
         return (BOOL)FastList_OnCommand_IsSelected (hList, (HLISTITEM)wp);

      case FLM_SELECTITEM:
         FastList_OnCommand_SelectItem (hList, (HLISTITEM)wp, (BOOL)lp);
         return TRUE;

      case FLM_FINDLIST:
         return (BOOL)FastList_OnCommand_FindList (hList, (HLISTITEM)wp, (DWORD)lp);

      case FLM_FINDTREE:
         return (BOOL)FastList_OnCommand_FindTree (hList, (HLISTITEM)wp, (DWORD)lp);

      case FLM_FINDSELECTED:
         return (BOOL)FastList_OnCommand_FindSelected (hList, (HLISTITEM)wp);

      case FLM_FINDITEM:
         return (BOOL)FastList_OnCommand_FindItem (hList, (LPENUM*)wp, lp);

      case FLM_FINDNEXTITEM:
         return (BOOL)FastList_OnCommand_FindNextItem (hList, (LPENUM*)wp);

      case FLM_FINDCLOSE:
         FastList_OnCommand_FindClose (hList, (LPENUM*)wp);
         return TRUE;

      case FLM_FINDVISIBLE:
         return (BOOL)FastList_OnCommand_FindVisible (hList, (HLISTITEM)wp, (DWORD)lp);

      case FLM_GETITEMCOUNT:
         return (BOOL)FastList_OnCommand_GetItemCount (hList, (BOOL)wp);

      case FLM_ITEMFROMPOINT:
         return (BOOL)FastList_OnCommand_ItemFromPoint (hList, (POINT*)wp, (BOOL)lp);

      case FLM_GETITEMREGIONS:
         FastList_OnCommand_GetItemRegions (hList, (HLISTITEM)wp, (LPFASTLISTITEMREGIONS)lp);
         return TRUE;

      case FLM_GETTEXTCALLBACK:
         return (BOOL)FastList_OnCommand_GetTextCallback (hList);

      case FLM_SETTEXTCALLBACK:
         FastList_OnCommand_SetTextCallback (hList, (LPFASTLISTTEXTCALLBACK)wp, (DWORD)lp);
         return TRUE;
      }

   return DefWindowProc (hList, msg, wp, lp);
}


BOOL CALLBACK FastList_ParentProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hDlg, FastList_ParentProc);

   switch (msg)
      {
      case WM_DRAWITEM:
         LPFASTLIST pfl;
         if ((pfl = GetFastList (GetDlgItem (hDlg, wp))) != NULL)
            {
            FastList_OnPaintItem (pfl, (LPFASTLISTDRAWITEM)lp);
            return TRUE;
            }
         break;

      case WM_NOTIFY:
         if ((pfl = GetFastList (GetDlgItem (hDlg, wp))) != NULL)
            {
            switch (((NMHDR*)lp)->code)
               {
               case FLN_COLUMNCLICK:
                  if (pfl->dwStyle & FLS_AUTOSORTHEADER)
                     {
                     LPFLN_COLUMNCLICK_PARAMS pp = (LPFLN_COLUMNCLICK_PARAMS)lp;

                     if (pfl->iColSort == pp->icol)
                        FastList_SetSortStyle (pfl->hList, pp->icol, !pfl->fRevSort);
                     else
                        FastList_SetSortStyle (pfl->hList, pp->icol, FALSE);
                     }
                  break;
               }
            }
         break;
      }

   return CallWindowProc ((WNDPROC)oldProc, hDlg, msg, wp, lp);
}


/*
 * MESSAGE HANDLING ___________________________________________________________
 *
 */

void FastList_OnCreate (HWND hList)
{
   LPFASTLIST pfl = New (FASTLIST);
   memset (pfl, 0x00, sizeof(FASTLIST));
   SetWindowLong (hList, 0, (LONG)pfl);
   pfl->pfnSort = NULL;
   pfl->pfnText = NULL;
   pfl->dwCookieText = 0;

   InitializeCriticalSection (&pfl->cs);
   pfl->dwSig = dwSigFASTLIST;
   pfl->hList = hList;
   pfl->dwStyle = GetWindowLong (hList, GWL_STYLE);
   pfl->hf = (HFONT)GetStockObject (DEFAULT_GUI_FONT);

   pfl->lItems = New (HASHLIST);
   pfl->lItems->SetCriticalSection (&pfl->cs);
   pfl->lkUserParam = pfl->lItems->CreateKey ("User Param", FastList_KeyUserParam_Compare, FastList_KeyUserParam_HashObject, FastList_KeyUserParam_HashData);

   FastList_Begin (pfl->hList);

   if (!(pfl->dwStyle & WS_CLIPCHILDREN))
      SetWindowLong (pfl->hList, GWL_STYLE, pfl->dwStyle | WS_CLIPCHILDREN);

   FastList_SyncHeader (pfl);
   FastList_Repaint (pfl);
   FastList_End (pfl->hList);
}


void FastList_OnDestroy (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (pfl->hHeader)
         DestroyWindow (pfl->hHeader);
      if (pfl->hScrollH)
         DestroyWindow (pfl->hScrollH);
      if (pfl->hScrollV)
         DestroyWindow (pfl->hScrollV);
      if (pfl->aVisibleHeap)
         Free (pfl->aVisibleHeap);
      if (pfl->aColumns)
         {
         for (size_t iColumn = 0; iColumn < pfl->cColumns; ++iColumn)
            if (pfl->aColumns[ iColumn ].pszText)
               Free (pfl->aColumns[ iColumn ].pszText);
         Free (pfl->aColumns);
         }
      DeleteObject (pfl->hf);
      Delete (pfl->lItems);
      DeleteCriticalSection (&pfl->cs);
      SetWindowLong (hList, 0, 0);
      Delete (pfl);
      }
}


void FastList_OnStyleChange (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      BOOL fWasTree = ((pfl->dwStyle & FLS_VIEW_TREE) == FLS_VIEW_TREE) ? TRUE : FALSE;

      pfl->dwStyle = GetWindowLong (hList, GWL_STYLE);

      BOOL fIsTree = ((pfl->dwStyle & FLS_VIEW_TREE) == FLS_VIEW_TREE) ? TRUE : FALSE;
      if (fWasTree != fIsTree)
         {
         FastList_RepairVisibilityFlags (pfl);
         pfl->fSortBeforePaint = TRUE;
         }

      FastList_SyncHeader (pfl);
      FastList_Repaint (pfl);
      }
}


void FastList_OnSize (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      FastList_SyncHeader (pfl);
      FastList_Repaint (pfl);
      }
}


void FastList_OnPaint (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (pfl->nBegin)
         {
         pfl->fRepaintRequired = TRUE;
         }
      else // (!pfl->nBegin)
         {
         if (pfl->fSortBeforePaint)
            FastList_PerformSort (pfl);
         if (pfl->fSyncIndicesBeforePaint)
            FastList_RepairVisibilityIndices (pfl);
         if (pfl->fSyncScrollBeforePaint)
            FastList_SyncScroll (pfl);
         if (pfl->hEnsureVisible)
            FastList_PerformEnsureVisible (pfl);
         }

      PAINTSTRUCT ps;
      HDC hdc = BeginPaint (hList, &ps);

      // If we're showing both scrollbars, grey-wash the rectangle
      // in the bottom-right where they meet.
      //
      if ((pfl->hScrollH) && (pfl->hScrollV))
         {
         RECT rBox;
         GetClientRect (pfl->hList, &rBox);
         rBox.left = rBox.right - GetSystemMetrics(SM_CXVSCROLL);
         rBox.top = rBox.bottom - GetSystemMetrics(SM_CYHSCROLL);

         HBRUSH hbr = CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
         FillRect (hdc, &rBox, hbr);
         DeleteObject (hbr);
         }

      // Just what client area is left, after we skip the scrollbar and
      // header regions?
      //
      RECT rClient;
      GetClientRect (pfl->hList, &rClient);
      if (pfl->hScrollV)
         rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
      if (pfl->hScrollH)
         rClient.bottom -= GetSystemMetrics(SM_CYHSCROLL);
      if (pfl->hHeader)
         rClient.top += FastList_GetHeaderHeight (pfl);

      // If possible, we want to draw onto an off-screen bitmap then blit
      // the relevant sections onto the client area; this eliminates flicker
      // as items are erased and redrawn.
      //
      HDC hdcTarget = hdc;
      HBITMAP bmpTargetOld = NULL;

      BOOL fDoubleBuffer;
      if ((fDoubleBuffer = OpenGlobalBitmap (hdc, &rClient)) == TRUE)
         {
         hdcTarget = CreateCompatibleDC (hdc);
         bmpTargetOld = (HBITMAP)SelectObject (hdcTarget, fg.bmp);
         }

      HFONT hfOld = (HFONT)SelectObject (hdcTarget, pfl->hf);

      // Determine which objects we can display on the screen, and paint
      // each in turn. If there is no object at a particular location,
      // just white- or grey-fill the background.
      //
      RECT rTemplate;
      FastList_CalcItemRect (pfl, 0, &rTemplate, !!pfl->hScrollH, !!pfl->hScrollV);

      HBRUSH hbrBackground = CreateSolidBrush (GetSysColor( (IsWindowEnabled(pfl->hList)) ? COLOR_WINDOW : COLOR_BTNFACE ));
      FillRect (hdcTarget, &rClient, hbrBackground);

      if (!pfl->nBegin)
         {
         switch (pfl->dwStyle & FLS_VIEW_MASK)
            {
            case FLS_VIEW_LARGE:
               {
               // In Large mode, icons are stacked left-to-right, top-to-bottom.
               // Find the top index, and for each vertical index thereafter,
               // find and paint the relevant horizontal indices.
               //
               LONG cxItems = cxRECT(rClient) / cxRECT(rTemplate);

               LONG iyTopItem = pfl->dyPixel / cyRECT(rTemplate);
               LONG iyBottomItem = iyTopItem + DivRoundUp (cyRECT(rClient), cyRECT(rTemplate));

               for (LONG iyItem = iyTopItem; iyItem <= iyBottomItem; ++iyItem)
                  {
                  for (LONG ixItem = 0; ixItem < cxItems; ++ixItem)
                     {
                     int iItem = (int)( ixItem + cxItems * iyItem );
                     if (iItem < (int)pfl->cVisible)
                        {
                        RECT rItem;
                        rItem.top = rClient.top + iyItem * cyRECT(rTemplate) - pfl->dyPixel;
                        rItem.bottom = rItem.top + cyRECT(rTemplate);
                        rItem.left = rClient.left + ixItem * cxRECT(rTemplate) - pfl->dxPixel;
                        rItem.right = rItem.left + cxRECT(rTemplate);

                        FastList_CallPaintItem (pfl, hdcTarget, TRUE, FALSE, iItem, &rItem, NULL, NULL, NULL);
                        }
                     }
                  }
               break;
               }

            case FLS_VIEW_SMALL:
               {
               // In Small mode, icons are stacked top-to-bottom, left-to-right.
               // Find the left index, and for each horizontal index thereafter,
               // find and paint the relevant vertical indices.
               //
               LONG cyItems = cyRECT(rClient) / cyRECT(rTemplate);

               LONG ixLeftItem = pfl->dxPixel / cxRECT(rTemplate);
               LONG ixRightItem = ixLeftItem + DivRoundUp (cxRECT(rClient), cxRECT(rTemplate));

               for (LONG ixItem = ixLeftItem; ixItem <= ixRightItem; ++ixItem)
                  {
                  for (LONG iyItem = 0; iyItem < cyItems; ++iyItem)
                     {
                     int iItem = (int)( iyItem + cyItems * ixItem );
                     if (iItem < (int)pfl->cVisible)
                        {
                        RECT rItem;
                        rItem.top = rClient.top + iyItem * cyRECT(rTemplate) - pfl->dyPixel;
                        rItem.bottom = rItem.top + cyRECT(rTemplate);
                        rItem.left = rClient.left + ixItem * cxRECT(rTemplate) - pfl->dxPixel;
                        rItem.right = rItem.left + cxRECT(rTemplate);

                        FastList_CallPaintItem (pfl, hdcTarget, TRUE, FALSE, iItem, &rItem, NULL, NULL, NULL);
                        }
                     }
                  }
               break;
               }

            case FLS_VIEW_LIST:
            case FLS_VIEW_TREE:
            case FLS_VIEW_TREELIST:
               {
               // In these modes, icons are stacked top-to-bottom; each takes
               // the entire width we can give it. Find the top index, and
               // paint all indices which fit on the screen.
               //
               LONG iTopItem = pfl->dyPixel / cyRECT(rTemplate);
               LONG iBottomItem = iTopItem + DivRoundUp (cyRECT(rClient), cyRECT(rTemplate));

               for (LONG iItem = iTopItem; iItem <= iBottomItem; ++iItem)
                  {
                  if (iItem < (int)pfl->cVisible)
                     {
                     RECT rItem;
                     rItem.top = rClient.top + iItem * cyRECT(rTemplate) - pfl->dyPixel;
                     rItem.bottom = rItem.top + cyRECT(rTemplate);
                     rItem.left = rClient.left - pfl->dxPixel;
                     rItem.right = rItem.left + cxRECT(rTemplate);

                     FastList_CallPaintItem (pfl, hdcTarget, TRUE, FALSE, iItem, &rItem, NULL, NULL, NULL);
                     }
                  }
               break;
               }
            }
         }

      if (fDoubleBuffer)
         {
         BitBlt (hdc, rClient.left, rClient.top, cxRECT(rClient), cyRECT(rClient),
                 hdcTarget, rClient.left, rClient.top, SRCCOPY);
         SelectObject (hdcTarget, bmpTargetOld);
         DeleteDC (hdcTarget);
         CloseGlobalBitmap();
         }

      DeleteObject (hbrBackground);
      SelectObject (hdcTarget, hfOld);
      EndPaint (hList, &ps);
      }
}


void FastList_OnPaintItem (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi)
{
   // Initialize the output FASTLISTITEMREGIONS structure.
   // We were given the object's overall rectangle, and will
   // calculate each of the other regions as we go along.
   //
   memset (&pdi->reg, 0x00, sizeof(pdi->reg));
   pdi->reg.rItem = pdi->rItem;
   pdi->fTextTestHit = FALSE;

   // The actual paint routine we'll use depends on the current view.
   //
   switch (pfl->dwStyle & FLS_VIEW_MASK)
      {
      case FLS_VIEW_LARGE:
         FastList_OnPaintItem_Large (pfl, pdi);
         break;

      case FLS_VIEW_SMALL:
         FastList_OnPaintItem_Small (pfl, pdi);
         break;

      case FLS_VIEW_LIST:
      case FLS_VIEW_TREE:
      case FLS_VIEW_TREELIST:
         FastList_OnPaintItem_List (pfl, pdi);
         break;
      }

   UnionRect (&pdi->reg.rSelect, &pdi->reg.rHighlight, &pdi->reg.rImage);
}


void FastList_OnPaintItem_TreeLines (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, DWORD dwLines, RECT *prThisColumn)
{
   if (prThisColumn->left >= prThisColumn->right)
      return;

   RECT rLines;
   rLines.left = prThisColumn->left;
   rLines.right = rLines.left +GetSystemMetrics(SM_CXSMICON) +cxAFTER_LIST_ICON;
   rLines.top = prThisColumn->top;
   rLines.bottom = prThisColumn->bottom;
   prThisColumn->left = rLines.right;

   LONG xBox = rLines.left + (GetSystemMetrics(SM_CXSMICON) - cxTREE_BOX)/2;
   LONG xLine = xBox + cxTREE_BOX/2;

   LONG yLine = rLines.top + cyRECT(rLines)/2;
   LONG yBox = yLine - cyTREE_BOX/2;

   if (dwLines & TREELINE_BOX)
      {
      pdi->reg.rButton.left = xBox;
      pdi->reg.rButton.right = xBox +cxTREE_BOX +1;
      pdi->reg.rButton.top = yBox;
      pdi->reg.rButton.bottom = yBox +cyTREE_BOX +1;
      }

   if (pdi->fDraw)
      {
      COLORREF clrBG = GetSysColor (COLOR_WINDOW);
      COLORREF clrFG = GetSysColor (COLOR_BTNSHADOW);
      if (!IsWindowEnabled (pfl->hList))
         clrBG = GetSysColor (COLOR_BTNFACE);

      if (dwLines & TREELINE_HORIZONTAL)
         {
         for (LONG xx = (dwLines & TREELINE_BOX) ? (xBox +cxTREE_BOX) : (dwLines & TREELINE_UP) ? xLine : (rLines.left+1); xx <= rLines.right+1; xx += cxSPACE_TREELINE)
            SetPixel (pdi->hdc, xx, yLine, clrFG);
         }

      if (dwLines & TREELINE_UP)
         {
         for (LONG yy = (dwLines & TREELINE_BOX) ? yBox : yLine; yy >= rLines.top; yy -= cySPACE_TREELINE)
            SetPixel (pdi->hdc, xLine, yy, clrFG);
         }

      if (dwLines & TREELINE_DOWN)
         {
         for (LONG yy = (dwLines & TREELINE_BOX) ? (yBox +cyTREE_BOX) : yLine; yy <= rLines.bottom; yy += cySPACE_TREELINE)
            SetPixel (pdi->hdc, xLine, yy, clrFG);
         }

      if (dwLines & TREELINE_BOX)
         {
         HPEN hpNew = CreatePen (PS_SOLID, 1, clrFG);
         HPEN hpOld = (HPEN)SelectObject (pdi->hdc, hpNew);

         MoveToEx (pdi->hdc, xBox, yBox, NULL);
         LineTo (pdi->hdc, xBox +cxTREE_BOX, yBox);
         LineTo (pdi->hdc, xBox +cxTREE_BOX, yBox +cyTREE_BOX);
         LineTo (pdi->hdc, xBox, yBox +cyTREE_BOX);
         LineTo (pdi->hdc, xBox, yBox);

         SelectObject (pdi->hdc, GetStockObject (BLACK_PEN));

         MoveToEx (pdi->hdc, xBox +2, yLine, NULL);
         LineTo (pdi->hdc, xBox +cxTREE_BOX -1, yLine);

         if ((dwLines & TREELINE_BOXCLOSED) == TREELINE_BOXCLOSED)
            {
            MoveToEx (pdi->hdc, xLine, yBox +2, NULL);
            LineTo (pdi->hdc, xLine, yBox +cyTREE_BOX -1);
            }

         SelectObject (pdi->hdc, hpOld);
         DeleteObject (hpNew);
         }
      }
}


BOOL FastList_OnPaintItem_DrawImage (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, HIMAGELIST hil, int iImage, LONG xImage, LONG yImage, BOOL fHLines)
{
   if (hil == NULL)
      return FALSE;
   if (iImage == IMAGE_NOIMAGE)
      return FALSE;

   RECT rImage;
   rImage.left = xImage;
   rImage.right = rImage.left + GetSystemMetrics ((hil == pfl->hilLarge) ? SM_CXICON : SM_CXSMICON);
   rImage.top = yImage;
   rImage.bottom = rImage.top + GetSystemMetrics ((hil == pfl->hilLarge) ? SM_CYICON : SM_CYSMICON);

   if (pdi->fDraw)
      {
      if (iImage == IMAGE_BLANKIMAGE)
         {
         if (fHLines && ((pfl->dwStyle & FLS_VIEW_TREE) == FLS_VIEW_TREE))
            {
            RECT rLine;
            rLine.left = xImage;
            rLine.right = rLine.left + GetSystemMetrics(SM_CXSMICON);
            rLine.top = yImage;
            rLine.bottom = rLine.top + GetSystemMetrics(SM_CYSMICON);
            FastList_OnPaintItem_TreeLines (pfl, pdi, TREELINE_HORIZONTAL, &rLine);
            }
         }
      else // An actual image was specified to be drawn.
         {
         if (!ImageList_Draw (hil, iImage, pdi->hdc, rImage.left, rImage.top, (pdi->fDragImage) ? ILD_TRANSPARENT : (!IsWindowEnabled(pfl->hList)) ? ILD_TRANSPARENT : (pdi->hItem->fSelected) ? ILD_SELECTED : (pdi->hItem->fFocused) ? ILD_FOCUS : ILD_TRANSPARENT))
            return FALSE;
         }
      }

   if (IsRectEmpty (&pdi->reg.rImage))
      pdi->reg.rImage = rImage;
   else
      UnionRect (&pdi->reg.rImage, &pdi->reg.rImage, &rImage);

   return TRUE;
}


void FastList_OnPaintItem_GetItemColors (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, COLORREF *pclrFore, COLORREF *pclrBack)
{
   if (pdi->fDragImage)
      {
      *pclrFore = GetSysColor (COLOR_WINDOWTEXT);
      *pclrBack = clrTRANSPARENT;
      }
   else if (!IsWindowEnabled (pfl->hList))
      {
      *pclrFore = GetSysColor (COLOR_GRAYTEXT);
      *pclrBack = GetSysColor (COLOR_BTNFACE);
      }
   else if ( (pdi->hItem->fSelected) || (pdi->hItem->dwFlags & FLIF_DROPHIGHLIGHT) )
      {
      *pclrFore = GetSysColor (COLOR_HIGHLIGHTTEXT);
      *pclrBack = GetSysColor (COLOR_HIGHLIGHT);
      }
   else // normal colors
      {
      *pclrFore = GetSysColor (COLOR_WINDOWTEXT);
      *pclrBack = GetSysColor (COLOR_WINDOW);
      }
}


void FastList_OnPaintItem_Large (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi)
{
   // If there is an image associated with this item, draw it. Remember
   // that if there are *two* images, draw the second one--it's the primary
   // image, or the user wouldn't have bothered adding it.
   //
   int iImage = pdi->hItem->iFirstImage;
   if (pdi->hItem->iSecondImage != IMAGE_NOIMAGE)
      iImage = pdi->hItem->iSecondImage;
   if (iImage != IMAGE_NOIMAGE)
      {
      LONG xImage = pdi->rItem.left + (cxRECT(pdi->rItem) - GetSystemMetrics(SM_CXICON))/2;
      LONG yImage = pdi->rItem.top + cyABOVE_LARGE_ICON;
      FastList_OnPaintItem_DrawImage (pfl, pdi, pfl->hilLarge, iImage, xImage, yImage, FALSE);
      }

   // If there is any column-0 text supplied for this item, draw that below
   // the item's icon.
   //
   LPTSTR pszColumnText = FastList_GetColumnText (pfl, pdi->hItem, 0);
   if (pszColumnText && *pszColumnText)
      {
      RECT rTextSize;
      SetRectEmpty (&rTextSize);
      rTextSize.right = cxRECT(pdi->rItem) - cxBEFORE_LARGE_TEXT - cxAFTER_LARGE_TEXT -2;

      if (!DrawTextEx (pdi->hdc, pszColumnText, lstrlen(pszColumnText), &rTextSize, DT_CENTER | DT_CALCRECT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX, 0))
         rTextSize.right = rTextSize.left;

      RECT rText = pdi->rItem;
      rText.left += (cxRECT(pdi->rItem)-cxRECT(rTextSize))/2;
      rText.right -= (cxRECT(pdi->rItem)-cxRECT(rTextSize))/2;
      rText.top += cyABOVE_LARGE_ICON + GetSystemMetrics(SM_CYICON) + cyBELOW_LARGE_ICON + cyABOVE_LARGE_TEXT;
      rText.bottom = min( pdi->rItem.bottom - cyBELOW_LARGE_TEXT, (rText.top + cyRECT(rTextSize)) );

      pdi->reg.rHighlight = rText;
      pdi->reg.rHighlight.left -= cxBEFORE_LARGE_TEXT;
      pdi->reg.rHighlight.right += cxAFTER_LARGE_TEXT;
      pdi->reg.rHighlight.top -= cyABOVE_LARGE_TEXT;
      pdi->reg.rHighlight.bottom += cyBELOW_LARGE_TEXT;

      pdi->reg.rLabel = pdi->reg.rHighlight;
      if (PtInRect (&rText, pdi->ptTextTest))
         pdi->fTextTestHit = TRUE;

      if (pdi->fDraw)
         {
         COLORREF clrFore;
         COLORREF clrBack;
         FastList_OnPaintItem_GetItemColors (pfl, pdi, &clrFore, &clrBack);

         if (pdi->hItem->fSelected)
            {
            HBRUSH hbr = CreateSolidBrush (clrBack);
            FillRect (pdi->hdc, &pdi->reg.rHighlight, hbr);
            DeleteObject (hbr);
            }

         COLORREF clrBackOld = SetBkColor (pdi->hdc, clrBack);
         COLORREF clrForeOld = SetTextColor (pdi->hdc, clrFore);

         DrawTextEx (pdi->hdc, pszColumnText, lstrlen(pszColumnText),
                     &rText, DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX, 0);

         SetTextColor (pdi->hdc, clrForeOld);
         SetBkColor (pdi->hdc, clrBackOld);

         if (pdi->hItem->fFocused && IsWindowEnabled (pfl->hList))
            DrawFocusRect (pdi->hdc, &pdi->reg.rHighlight);
         }
      }
}


void FastList_OnPaintItem_Small (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi)
{
   // If there is an image associated with this item, draw it. Remember
   // that if there are *two* images, draw the second one--it's the primary
   // image, or the user wouldn't have bothered adding it.
   //
   BOOL fImage = FALSE;

   int iImage = pdi->hItem->iFirstImage;
   if (pdi->hItem->iSecondImage != IMAGE_NOIMAGE)
      iImage = pdi->hItem->iSecondImage;
   if (iImage != IMAGE_NOIMAGE)
      {
      LONG xImage = pdi->rItem.left + cxBEFORE_SMALL_ICON;
      LONG yImage = pdi->rItem.top + (cyRECT(pdi->rItem) - GetSystemMetrics(SM_CYSMICON))/2;
      fImage = FastList_OnPaintItem_DrawImage (pfl, pdi, pfl->hilSmall, iImage, xImage, yImage, FALSE);
      }

   // If there is any column-0 text supplied for this item, draw that to the
   // right of the item's icon.
   //
   LPTSTR pszColumnText = FastList_GetColumnText (pfl, pdi->hItem, 0);
   if (pszColumnText && *pszColumnText)
      {
      RECT rTextSize = pdi->rItem;
      if (!DrawTextEx (pdi->hdc, pszColumnText, lstrlen(pszColumnText), &rTextSize, DT_CALCRECT | DT_END_ELLIPSIS | DT_NOPREFIX, 0))
         rTextSize.right = rTextSize.left;

      RECT rText = pdi->rItem;
      rText.left += cxBEFORE_SMALL_TEXT + fImage * (GetSystemMetrics(SM_CXSMICON) + cxBEFORE_SMALL_ICON + cxAFTER_SMALL_ICON);
      rText.right = min( pdi->rItem.right - cxAFTER_SMALL_TEXT, (rText.left + cxRECT(rTextSize)) );
      rText.top += cyABOVE_SMALL_TEXT;
      rText.bottom -= cyBELOW_SMALL_TEXT;

      pdi->reg.rHighlight = rText;
      pdi->reg.rHighlight.left -= cxBEFORE_SMALL_TEXT;
      pdi->reg.rHighlight.right += cxAFTER_SMALL_TEXT;
      pdi->reg.rHighlight.top -= cyABOVE_SMALL_TEXT;
      pdi->reg.rHighlight.bottom += cyBELOW_SMALL_TEXT;

      pdi->reg.rLabel = pdi->reg.rHighlight;
      if (PtInRect (&rText, pdi->ptTextTest))
         pdi->fTextTestHit = TRUE;

      if (pdi->fDraw)
         {
         COLORREF clrFore;
         COLORREF clrBack;
         FastList_OnPaintItem_GetItemColors (pfl, pdi, &clrFore, &clrBack);

         if (pdi->hItem->fSelected)
            {
            HBRUSH hbr = CreateSolidBrush (clrBack);
            FillRect (pdi->hdc, &pdi->reg.rHighlight, hbr);
            DeleteObject (hbr);
            }

         COLORREF clrBackOld = SetBkColor (pdi->hdc, clrBack);
         COLORREF clrForeOld = SetTextColor (pdi->hdc, clrFore);

         DrawTextEx (pdi->hdc, pszColumnText, lstrlen(pszColumnText),
                     &rText, DT_END_ELLIPSIS | DT_NOPREFIX, 0);

         SetTextColor (pdi->hdc, clrForeOld);
         SetBkColor (pdi->hdc, clrBackOld);

         if (pdi->hItem->fFocused && IsWindowEnabled (pfl->hList))
            DrawFocusRect (pdi->hdc, &pdi->reg.rHighlight);
         }
      }
}


void FastList_OnPaintItem_Tree (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi, HLISTITEM hItem, RECT *prThisColumn)
{
   // This routine is supposed to draw the tree structure off to the left
   // of an item. To do that, it calls itself recursively to draw the
   // icon areas of each of its parents.
   //
   if (hItem->hTreeParent != NULL)
      {
      FastList_OnPaintItem_Tree (pfl, pdi, hItem->hTreeParent, prThisColumn);
      }

   // For any given item, the first thing to determine is how many icons
   // that item has.
   //
   int iLeftImage = hItem->iFirstImage;
   int iRightImage = IMAGE_NOIMAGE;

   if (hItem->iSecondImage != IMAGE_NOIMAGE)
      {
      iLeftImage = hItem->iSecondImage;
      iRightImage = hItem->iFirstImage;
      }

   // What lines/boxes should we draw?
   //
   BOOL fHLines = FALSE;

   if ((hItem->hTreeParent) || (pfl->dwStyle & FLS_LINESATROOT))
      {
      fHLines = TRUE;

      DWORD dwLines = 0;
      if (hItem == pdi->hItem)
         dwLines |= TREELINE_HORIZONTAL;
      if ((hItem == pdi->hItem) && (hItem->hTreeChild))
         dwLines |= (hItem->fExpanded) ? TREELINE_BOXOPEN : TREELINE_BOXCLOSED;
      if (hItem->hTreeNext)
         dwLines |= TREELINE_DOWN;
      if ((dwLines != 0) && !((!hItem->hTreeParent && !hItem->hTreePrevious) && (hItem == pdi->hItem)))
         dwLines |= TREELINE_UP;

      FastList_OnPaintItem_TreeLines (pfl, pdi, dwLines, prThisColumn);
      }

   // The lines we (maybe) just drew took the place of an icon for this
   // item. Maybe draw blank space, to pad for the second icon
   //
   if ( (hItem != pdi->hItem) && (iLeftImage != IMAGE_NOIMAGE) && (iRightImage != IMAGE_NOIMAGE) )
      {
      FastList_OnPaintItem_TreeLines (pfl, pdi, 0, prThisColumn);
      }
}


void FastList_OnPaintItem_List (LPFASTLIST pfl, LPFASTLISTDRAWITEM pdi)
{
   // Painting a tree or list is done column by column. Remember
   // We may have a "long column"--that's a column that doesn't
   // necessarily respect the header column boundaries.
   //
   RECT rItemRemaining = pdi->rItem;
   pdi->reg.rHighlight = pdi->rItem;

   int icolLong = -1;
   if (pdi->fDragImage)
      {
      icolLong = 0;
      }
   else if ((pfl->dwStyle & FLS_VIEW_TREELIST) == FLS_VIEW_TREE)
      {
      icolLong = 0;
      }
   else if (pfl->dwStyle & FLS_LONGCOLUMNS)
      {
      for (size_t icol = 0; icol < pfl->cColumns; ++icol)
         {
         if (!pfl->aColumns[ icol ].pszText)
            break;

         LPTSTR pszColumnText = FastList_GetColumnText (pfl, pdi->hItem, icol);
         if (*pszColumnText)
            {
            icolLong = icol;
            if ((pfl->aColumns[ icol ].fmt & HDF_JUSTIFYMASK) != HDF_LEFT)
               icolLong = -1; // this can't be the long column
            }
         }
      }

   // Select appropriate text colors. Since FastLists always draw highlights
   // all the way across the item, we won't change colors from column to
   // column.
   //
   COLORREF clrFore;
   COLORREF clrBack;
   FastList_OnPaintItem_GetItemColors (pfl, pdi, &clrFore, &clrBack);

   COLORREF clrBackOld = SetBkColor (pdi->hdc, clrBack);
   COLORREF clrForeOld = SetTextColor (pdi->hdc, clrFore);

   // Now that we know where the long column is, we're ready to start
   // painting columns.
   //
   for (int icol = 0; (icolLong == -1) || (icol <= icolLong); ++icol)
      {
      if (icol && !pfl->cColumns)
         break;
      else if ((pfl->cColumns) && ((icol >= (int)pfl->cColumns) || (!pfl->aColumns[ icol ].pszText)))
         break;

      // The rules for the long column are slightly different than the
      // rules for all other columns.
      //
      LONG cxColumn = pdi->rItem.right - rItemRemaining.left;
      int fmtColumn = HDF_LEFT;

      if ((icol != icolLong) && (pfl->cColumns) && (icol < (int)pfl->cColumns))
         {
         cxColumn = pfl->aColumns[ icol ].cxy;
         fmtColumn = pfl->aColumns[ icol ].fmt & HDF_JUSTIFYMASK;
         }

      // Remember where this column ends and the next column begins.
      //
      RECT rThisColumn = rItemRemaining;
      rThisColumn.right = rThisColumn.left + cxColumn;

      // Get the text for this column, and figure out how big it will be.
      //
      LPTSTR pszColumnText = FastList_GetColumnText (pfl, pdi->hItem, icol);

      RECT rTextSize = rThisColumn;
      if (!DrawTextEx (pdi->hdc, pszColumnText, lstrlen(pszColumnText), &rTextSize, DT_CALCRECT | DT_END_ELLIPSIS | DT_NOPREFIX, 0))
         rTextSize.right = rTextSize.left;

      // Draw the icons and/or lines for the column. The behavior here
      // depends on whether we're drawing a tree or a list, or both.
      //
      BOOL fHLines = FALSE;
      if ((icol == 0) && ((pfl->dwStyle & FLS_VIEW_TREE) == FLS_VIEW_TREE) && (!pdi->fDragImage))
         {
         FastList_OnPaintItem_Tree (pfl, pdi, pdi->hItem, &rThisColumn);
         fHLines = TRUE;
         }

      if (icol == 0)
         {
         int iLeftImage = pdi->hItem->iFirstImage;
         int iRightImage = IMAGE_NOIMAGE;

         if ((pdi->hItem->iSecondImage != IMAGE_NOIMAGE) &&
             (rThisColumn.left < rThisColumn.right) &&
             (!pdi->fDragImage))
            {
            iLeftImage = pdi->hItem->iSecondImage;
            iRightImage = pdi->hItem->iFirstImage;
            }

         if ((iLeftImage != IMAGE_NOIMAGE) &&
             (rThisColumn.left < rThisColumn.right))
            {
            rThisColumn.left += cxBEFORE_LIST_ICON;
            pdi->reg.rImage = rThisColumn;

            LONG xImage = rThisColumn.left;
            LONG yImage = rThisColumn.top + (cyRECT(pdi->rItem) - GetSystemMetrics(SM_CYSMICON))/2;
            if (FastList_OnPaintItem_DrawImage (pfl, pdi, pfl->hilSmall, iLeftImage, xImage, yImage, fHLines))
               {
               rThisColumn.left += GetSystemMetrics (SM_CXSMICON);
               if (iRightImage != IMAGE_NOIMAGE)
                  {
                  rThisColumn.left += cxBETWEEN_LIST_ICONS;
                  xImage = rThisColumn.left;
                  yImage = rThisColumn.top + (cyRECT(pdi->rItem) - GetSystemMetrics(SM_CYSMICON))/2;
                  if (FastList_OnPaintItem_DrawImage (pfl, pdi, pfl->hilSmall, iRightImage, xImage, yImage, fHLines))
                     rThisColumn.left += GetSystemMetrics (SM_CXSMICON);
                  }
               }

            pdi->reg.rImage.right = rThisColumn.left;
            rThisColumn.left += cxAFTER_LIST_ICON;
            }
         }

      // Determine where the text will go in this column. We have to pass
      // that information back (for column 0) as reg.rLabel; we also have
      // to test the text's position against {pdi->ptTextTest} to see if the
      // user just clicked on the text.
      //
      RECT rText = rThisColumn;
      rText.left += cxBEFORE_LIST_TEXT;
      rText.right -= cxAFTER_LIST_TEXT;
      rText.top += cyABOVE_LIST_TEXT;
      rText.bottom -= cyBELOW_LIST_TEXT;

      if (icol != 0)
         rText.left += cxBEFORE_COLUMN_TEXT;

      RECT rTextJustified = rText;
      rTextJustified.right = min( rText.right, rText.left + cxRECT(rTextSize) );

      LONG dxJustify = 0;
      if (fmtColumn == HDF_CENTER)
         dxJustify = (cxRECT(rText) - cxRECT(rTextJustified))/2;
      else if (fmtColumn == HDF_RIGHT)
         dxJustify = cxRECT(rText) - cxRECT(rTextJustified);
      rTextJustified.left += dxJustify;
      rTextJustified.right += dxJustify;

      if (icol == 0)
         pdi->reg.rLabel = rTextJustified;

      if (PtInRect (&rTextJustified, pdi->ptTextTest))
         pdi->fTextTestHit = TRUE;

      // Having determined where the text will go, we should record
      // where the highlight will be drawn.
      //
      if (icol == 0)
         {
         pdi->reg.rHighlight.left = rThisColumn.left;
         }
      if (icol == icolLong)
         {
         pdi->reg.rHighlight.right = min( pdi->reg.rHighlight.right, rTextJustified.right + cxAFTER_LIST_TEXT );
         }

      // Okay, it's time to actually draw the text. Didn't think we'd
      // ever get here, did you?  :)
      //
      if (pdi->fDraw)
         {
         if ((!pdi->fDragImage) && ((pdi->hItem->fSelected) || (pdi->hItem->dwFlags & FLIF_DROPHIGHLIGHT)))
            {
            RECT rHighlightInColumn;
            IntersectRect (&rHighlightInColumn, &pdi->reg.rHighlight, &rThisColumn);

            HBRUSH hbr = CreateSolidBrush (clrBack);
            FillRect (pdi->hdc, &rHighlightInColumn, hbr);
            DeleteObject (hbr);
            }

         DrawTextEx (pdi->hdc, pszColumnText, lstrlen(pszColumnText),
                     &rTextJustified, DT_END_ELLIPSIS | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER, 0);
         }

      rItemRemaining.left = rThisColumn.right;
      }

   // Finally, draw the focus rectangle and restore the original text colors.
   //
   if (pdi->hItem->fFocused && pdi->fDraw && IsWindowEnabled (pfl->hList) && (!pdi->fDragImage))
      DrawFocusRect (pdi->hdc, &pdi->reg.rHighlight);

   SetTextColor (pdi->hdc, clrForeOld);
   SetBkColor (pdi->hdc, clrBackOld);
}


void FastList_OnScroll (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      HWND hScroll = (msg == WM_VSCROLL) ? pfl->hScrollV : pfl->hScrollH;

      SCROLLINFO si;
      memset (&si, 0x00, sizeof(si));
      si.cbSize = sizeof(si);
      si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;
      GetScrollInfo (hScroll, SB_CTL, &si);

      LONG posNew = si.nPos;
      BOOL fRepaint = TRUE;

      switch (LOWORD(wp))
         {
         case SB_TOP:
            posNew = 0;
            break;

         case SB_BOTTOM:
            posNew = si.nMax - si.nPage;
            break;

         case SB_PAGEUP:
            posNew -= si.nPage;
            break;

         case SB_PAGEDOWN:
            posNew += si.nPage;
            break;

         case SB_LINEUP:
            posNew -= FastList_GetListHeight (pfl);
            break;

         case SB_LINEDOWN:
            posNew += FastList_GetListHeight (pfl);
            break;

         case SB_THUMBTRACK:
            posNew = si.nTrackPos;
            fRepaint = FALSE;
            break;
         }

      posNew = limit( 0, posNew, (LONG)(si.nMax - si.nPage) );
      SetScrollPos (hScroll, SB_CTL, posNew, fRepaint);

      // If we just scrolled vertically, repaint.  If we just scrolled
      // horizontally, we'll have to sync the header first.
      //
      if (msg == WM_HSCROLL)
         pfl->dxPixel = posNew;
      else
         pfl->dyPixel = posNew;

      FastList_Repaint (pfl);
      if (msg == WM_HSCROLL)
         FastList_ScrollHeader (pfl);
      }
}


void FastList_OnRightButtonDown (HWND hList)
{
   FastList_ButtonDown (hList, TRUE, FALSE);
   FastList_Notify_Generic (GetFastList(hList), FLN_RCLICK);
}


void FastList_OnLeftButtonDown (HWND hList)
{
   FastList_ButtonDown (hList, FALSE, FALSE);
   FastList_Notify_Generic (GetFastList(hList), FLN_LCLICK);
}


void FastList_OnLeftButtonDouble (HWND hList)
{
   FastList_ButtonDown (hList, FALSE, TRUE);
   FastList_ButtonUp (hList, FALSE, TRUE);
   FastList_Notify_Generic (GetFastList(hList), FLN_LDBLCLICK);
}


void FastList_OnKeyPress (HWND hList, TCHAR ch)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      switch (ch)
         {
         case VK_LEFT:
            if (!FastList_OnKeyPress_EnsureSelection (pfl))
               break;
            if ((pfl->hFocused->hTreeChild) && (pfl->hFocused->fExpanded) && (!(pfl->hFocused->dwFlags & FLIF_DISALLOW_COLLAPSE)) && (pfl->dwStyle & FLS_VIEW_TREE))
               {
               FastList_Collapse (hList, pfl->hFocused);
               break;
               }
            if (pfl->hFocused->index != 0)
               FastList_OnKeyPress_ChangeSelection (pfl, pfl->aVisibleHeap[ pfl->hFocused->index -1 ]);
            break;

         case VK_UP:
            if (!FastList_OnKeyPress_EnsureSelection (pfl))
               break;
            if (pfl->hFocused->index != 0)
               FastList_OnKeyPress_ChangeSelection (pfl, pfl->aVisibleHeap[ pfl->hFocused->index -1 ]);
            break;

         case VK_RIGHT:
            if (!FastList_OnKeyPress_EnsureSelection (pfl))
               break;
            if ((pfl->hFocused->hTreeChild) && (!pfl->hFocused->fExpanded) && (pfl->dwStyle & FLS_VIEW_TREE))
               {
               FastList_Expand (hList, pfl->hFocused);
               FastList_Notify_ItemExpand (pfl, pfl->hFocused);
               break;
               }
            if (pfl->hFocused->index < (int)(pfl->cVisible-1))
               FastList_OnKeyPress_ChangeSelection (pfl, pfl->aVisibleHeap[ pfl->hFocused->index +1 ]);
            break;

         case VK_DOWN:
            if (!FastList_OnKeyPress_EnsureSelection (pfl))
               break;
            if (pfl->hFocused->index < (int)(pfl->cVisible-1))
               FastList_OnKeyPress_ChangeSelection (pfl, pfl->aVisibleHeap[ pfl->hFocused->index +1 ]);
            break;

         case VK_HOME:
            if (!FastList_OnKeyPress_EnsureSelection (pfl))
               break;
            if (pfl->cVisible)
               FastList_OnKeyPress_ChangeSelection (pfl, pfl->aVisibleHeap[0]);
            break;

         case VK_END:
            if (!FastList_OnKeyPress_EnsureSelection (pfl))
               break;
            if (pfl->cVisible)
               FastList_OnKeyPress_ChangeSelection (pfl, pfl->aVisibleHeap[ pfl->cVisible -1 ]);
            break;

         case TEXT(' '):
            if (!FastList_OnKeyPress_EnsureSelection (pfl))
               break;
            FastList_Begin (pfl->hList);
            if (pfl->hFocused->fSelected && fIsControlDown())
               FastList_SelectItem (pfl->hList, pfl->hFocused, FALSE);
            else if (!pfl->hFocused->fSelected && !fIsShiftDown())
               FastList_SelectItem (pfl->hList, pfl->hFocused, TRUE);
            else if (!pfl->hFocused->fSelected)
               FastList_OnKeyPress_ChangeSelection (pfl, pfl->hFocused);
            FastList_End (pfl->hList);
            break;
         }
      }
}


BOOL FastList_OnKeyPress_EnsureSelection (LPFASTLIST pfl)
{
   if (pfl->fSortBeforePaint)
      FastList_PerformSort (pfl);
   if (pfl->fSyncIndicesBeforePaint)
      FastList_RepairVisibilityIndices (pfl);
   if (pfl->hFocused && pfl->hFocused->fVisible)
      return TRUE;

   // Find the item at the top of the list.
   //
   RECT rClient;
   GetClientRect (pfl->hList, &rClient);
   if (pfl->hScrollV)
      rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
   if (pfl->hScrollH)
      rClient.bottom -= GetSystemMetrics(SM_CYHSCROLL);
   if (pfl->hHeader)
      rClient.top += FastList_GetHeaderHeight (pfl);

   RECT rTemplate;
   FastList_CalcItemRect (pfl, 0, &rTemplate, !!pfl->hScrollH, !!pfl->hScrollV);

   int indexTopLeft = -1;

   switch (pfl->dwStyle & FLS_VIEW_MASK)
      {
      case FLS_VIEW_LARGE:
         {
         LONG cxItems = cxRECT(rClient) / cxRECT(rTemplate);
         indexTopLeft = pfl->dyPixel / cyRECT(rTemplate);
         break;
         }

      case FLS_VIEW_SMALL:
         {
         LONG cyItems = cyRECT(rClient) / cyRECT(rTemplate);
         indexTopLeft = pfl->dxPixel / cxRECT(rTemplate);
         break;
         }

      case FLS_VIEW_LIST:
      case FLS_VIEW_TREE:
      case FLS_VIEW_TREELIST:
         indexTopLeft = pfl->dyPixel / cyRECT(rTemplate);
         break;
      }

   if ((indexTopLeft >= 0) && (indexTopLeft < (int)pfl->cVisible))
      {
      FastList_SetFocus (pfl->hList, pfl->aVisibleHeap[ indexTopLeft ]);
      }

   return FALSE;
}


void FastList_OnKeyPress_ChangeSelection (LPFASTLIST pfl, HLISTITEM hSelect)
{
   FastList_Begin (pfl->hList);
   FastList_SetFocus (pfl->hList, hSelect);
   FastList_EnsureVisible (pfl->hList, hSelect);

   // If Control is not down, begin by de-selecting all items
   // If Shift is not down, anchor and select the specified item
   // If Shift is down, select from the anchor to the specified item
   //
   HLISTITEM hAnchor = pfl->hAnchor;
   if (!fIsControlDown())
      {
      FastList_SelectNone (pfl->hList);
      }
   if ((!fIsShiftDown() || !hAnchor) && (!fIsControlDown()))
      {
      FastList_SelectItem (pfl->hList, hSelect, TRUE);
      pfl->hAnchor = hSelect;
      }
   if (fIsShiftDown() && hAnchor)
      {
      FastList_PerformSelectRange (pfl, hAnchor, hSelect);
      if (hAnchor->fSelected)
         pfl->hAnchor = hAnchor;
      }

   FastList_End (pfl->hList);
   FastList_Notify_ItemSelect (pfl);
}


/*
 * EXPORTED ROUTINES __________________________________________________________
 *
 */

void FastList_OnCommand_Begin (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      pfl->nBegin ++;
      }
}


void FastList_OnCommand_End (HWND hList, BOOL fForce)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (fForce)
         pfl->nBegin = 0;
      else if (pfl->nBegin)
         pfl->nBegin --;

      if (pfl->nBegin == 0)
         {
         if (pfl->fSyncHeaderRequired)
            FastList_SyncHeader (pfl);
         if (pfl->fSortBeforePaint)
            pfl->fRepaintRequired = TRUE;
         if (pfl->fSyncScrollBeforePaint)
            pfl->fRepaintRequired = TRUE;
         if (pfl->fRepaintRequired)
            FastList_Repaint (pfl);
         }
      }
}


HLISTITEM FastList_OnCommand_AddItem (HWND hList, LPFASTLISTADDITEM pai)
{
   HLISTITEM hItem = NULL;

   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      hItem = FastList_CreateItem (pfl);

      // Add this item to the hList chain
      //
      if ((hItem->hListNext = pfl->hListFirst) != NULL)
         hItem->hListNext->hListPrevious = hItem;
      pfl->hListFirst = hItem;

      // Add this item to the hTree chain
      //
      if ((hItem->hTreeParent = pai->hParent) != NULL)
         {
         if ((hItem->hTreeNext = hItem->hTreeParent->hTreeChild) != NULL)
            hItem->hTreeNext->hTreePrevious = hItem;
         hItem->hTreeParent->hTreeChild = hItem;
         }
      else // (hItem->hTreeParent == NULL)
         {
         if ((hItem->hTreeNext = pfl->hTreeFirst) != NULL)
            hItem->hTreeNext->hTreePrevious = hItem;
         pfl->hTreeFirst = hItem;
         }

      hItem->iFirstImage = pai->iFirstImage;
      hItem->iSecondImage = pai->iSecondImage;
      hItem->dwFlags = pai->dwFlags;
      hItem->lpUser = pai->lParam;

      if (pai->pszText)
         {
         REALLOC (hItem->apszText, hItem->cpszText, 1, max(1,pfl->cColumns));
         hItem->apszText[0] = (LPTSTR)Allocate (sizeof(TCHAR)*(1+lstrlen(pai->pszText)));
         lstrcpy (hItem->apszText[0], pai->pszText);
         }

      hItem->fVisible = FALSE;
      FastList_RepairOneVisibilityFlag (pfl, hItem);

      pfl->lItems->Add (hItem);
      pfl->fSortBeforePaint = TRUE;
      pfl->fSyncScrollBeforePaint = TRUE;
      FastList_Notify_AddItem (pfl, hItem);
      FastList_Repaint (pfl);
      }

   return hItem;
}


void FastList_OnCommand_RemoveItem (HWND hList, HLISTITEM hItem)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (hItem != NULL)
         {
         FastList_DeleteItem (pfl, hItem);
         }
      else
         {
         FastList_Begin (pfl->hList);

         while ((hItem = (HLISTITEM)(pfl->lItems->GetFirstObject())) != NULL)
            FastList_DeleteItem (pfl, hItem);

         FastList_End (pfl->hList);
         }

      FastList_Repaint (pfl);
      FastList_Notify_ItemSelect (pfl);
      }
}


LPCTSTR FastList_OnCommand_GetItemText (HWND hList, HLISTITEM hItem, int iColumn)
{
   if (hItem == NULL)
      return NULL;
   if ((iColumn < 0) || (iColumn >= (int)(hItem->cpszText)))
      return NULL;
   return hItem->apszText[ iColumn ];
}


void FastList_OnCommand_SetItemText (HWND hList, LPFASTLISTITEMCOLUMN pflic, LPCTSTR pszText)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if ((pflic) && (pflic->hItem) && (pflic->icol >= 0))
         {
         if (REALLOC (pflic->hItem->apszText, pflic->hItem->cpszText, pflic->icol+1, 1))
            {
            if (pflic->hItem->apszText[ pflic->icol ] != NULL)
               {
               Free (pflic->hItem->apszText[ pflic->icol ]);
               pflic->hItem->apszText[ pflic->icol ] = NULL;
               }
            if (pszText != NULL)
               {
               pflic->hItem->apszText[ pflic->icol ] = (LPTSTR)Allocate (sizeof(TCHAR)*(1+lstrlen(pszText)));
               lstrcpy (pflic->hItem->apszText[ pflic->icol ], pszText);
               }
            pfl->fSortBeforePaint = TRUE;
            FastList_Notify_ItemChanged (pfl, pflic->hItem);
            FastList_Repaint (pfl);
            }
         }
      }
}


LPARAM FastList_OnCommand_GetItemParam (HWND hList, HLISTITEM hItem)
{
   return (hItem == NULL) ? 0 : (hItem->lpUser);
}


void FastList_OnCommand_SetItemParam (HWND hList, HLISTITEM hItem, LPARAM lpUser)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (hItem != NULL)
         {
         hItem->lpUser = lpUser;
         pfl->lItems->Update (hItem);

         pfl->fSortBeforePaint = TRUE;
         FastList_Notify_ItemChanged (pfl, hItem);
         FastList_Repaint (pfl);
         }
      }
}


DWORD FastList_OnCommand_GetItemFlags (HWND hList, HLISTITEM hItem)
{
   return (hItem == NULL) ? 0 : (hItem->dwFlags);
}


void FastList_OnCommand_SetItemFlags (HWND hList, HLISTITEM hItem, DWORD dwFlagsNew)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (hItem != NULL)
         {
         FastList_Begin (hList);

         DWORD dwFlagsOld = hItem->dwFlags;
         hItem->dwFlags = dwFlagsNew;

         if ( (dwFlagsOld & FLIF_TREEVIEW_ONLY) != (dwFlagsNew & FLIF_TREEVIEW_ONLY) )
            {
            FastList_RepairOneVisibilityFlag (pfl, hItem);
            FastList_Repaint (pfl);
            }

         if ( (dwFlagsNew & FLIF_DISALLOW_SELECT) && (hItem->fSelected) )
            {
            FastList_PerformSelectItem (pfl, hItem, FALSE);
            FastList_Repaint (pfl);
            }

         if ( (dwFlagsNew & FLIF_DISALLOW_COLLAPSE) && !(hItem->fExpanded) )
            {
            FastList_OnCommand_Expand (hList, hItem, TRUE);
            FastList_Repaint (pfl);
            }

         if ( (dwFlagsOld & FLIF_DROPHIGHLIGHT) != (dwFlagsNew & FLIF_DROPHIGHLIGHT) )
            FastList_Repaint (pfl);

         FastList_Notify_ItemChanged (pfl, hItem);
         FastList_End (hList);
         }
      }
}


int FastList_OnCommand_GetItemImage (HWND hList, HLISTITEM hItem, int iImage)
{
   if (hItem && (iImage==0 || iImage==1))
      return (iImage == 0) ? (hItem->iFirstImage) : (hItem->iSecondImage);

   return IMAGE_NOIMAGE;
}


void FastList_OnCommand_SetItemImage (HWND hList, LPFASTLISTITEMIMAGE pflii, int iImage)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (pflii && pflii->hItem && (pflii->iImage==0 || pflii->iImage==1))
         {
         if (pflii->iImage == 0)
            pflii->hItem->iFirstImage = iImage;
         else // (pflii->iImage == 1)
            pflii->hItem->iSecondImage = iImage;

         FastList_Notify_ItemChanged (pfl, pflii->hItem);
         FastList_Repaint (pfl);
         }
      }
}


BOOL FastList_OnCommand_IsExpanded (HWND hList, HLISTITEM hItem)
{
   return (hItem) ? (hItem->fExpanded) : FALSE;
}


void FastList_OnCommand_Expand (HWND hList, HLISTITEM hItem, BOOL fExpand)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (hItem && (hItem->fExpanded != fExpand) && (fExpand || !(hItem->dwFlags & FLIF_DISALLOW_COLLAPSE)))
         {
         BOOL fTreeMode = (pfl->dwStyle & FLS_VIEW_TREE) ? TRUE : FALSE;

         hItem->fExpanded = fExpand;
         pfl->fSyncScrollBeforePaint = TRUE;
         pfl->fSyncIndicesBeforePaint = TRUE;
         FastList_RepairVisibilityFlags (pfl, hItem, (!hItem->fExpanded || !hItem->fVisible) && (fTreeMode));
         FastList_Notify_ItemChanged (pfl, hItem);
         FastList_Repaint (pfl);
         }
      }
}


BOOL FastList_OnCommand_ItemVisible (HWND hList, HLISTITEM hItem, BOOL fSet)
{
   if (!fSet)
      return (hItem) ? (hItem->fVisible) : FALSE;

   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (hItem && hItem->fVisible)
         {
         pfl->hEnsureVisible = hItem;
         FastList_Repaint (pfl);
         }
      }
   return TRUE;
}


HLISTITEM FastList_OnCommand_ItemFocus (HWND hList, HLISTITEM hItem, BOOL fSet)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (!fSet)
         return (hItem) ? (HLISTITEM)(hItem->fFocused) : pfl->hFocused;

      if (pfl->hFocused != hItem)
         {
         if (pfl->hFocused != NULL)
            {
            pfl->hFocused->fFocused = FALSE;
            FastList_Notify_ItemChanged (pfl, pfl->hFocused);
            }
         if ((pfl->hFocused = hItem) != NULL)
            {
            pfl->hFocused->fFocused = TRUE;
            FastList_Notify_ItemChanged (pfl, pfl->hFocused);
            }
         FastList_Repaint (pfl);
         }
      }

   return NULL;
}


void FastList_OnCommand_GetImageLists (HWND hList, HIMAGELIST *phiSmall, HIMAGELIST *phiLarge)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (phiSmall)
         *phiSmall = pfl->hilSmall;
      if (phiLarge)
         *phiLarge = pfl->hilLarge;
      }
}


void FastList_OnCommand_SetImageLists (HWND hList, HIMAGELIST hiSmall, HIMAGELIST hiLarge)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      pfl->hilSmall = hiSmall;
      pfl->hilLarge = hiLarge;
      FastList_Repaint (pfl);
      }
}


HIMAGELIST FastList_OnCommand_CreateDragImage (HWND hList, HLISTITEM hItem)
{
   HIMAGELIST hil = NULL;

   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (hItem)
         {

         // First find out how big this item is. Note that we'll ask
         // for the item regions, while specifying the fDragImage flag
         // in the paintitem request--that will make it draw only the
         // dragimage stuff.
         //
         HDC hdc = CreateCompatibleDC (NULL);
         HFONT hfOld = (HFONT)SelectObject (hdc, pfl->hf);

         RECT rItem;
         FastList_CalcItemRect (pfl, 0, &rItem, !!pfl->hScrollH, !!pfl->hScrollV);

         FASTLISTITEMREGIONS reg;
         FastList_CallPaintItem (pfl, hdc, FALSE, TRUE, hItem->index, &rItem, &reg, NULL, NULL);

         SelectObject (hdc, hfOld);
         DeleteDC (hdc);

         RECT rDragImage = reg.rSelect;

         rDragImage.left -= reg.rSelect.left;
         rDragImage.right -= reg.rSelect.left;
         rDragImage.top -= reg.rSelect.top;
         rDragImage.bottom -= reg.rSelect.top;

         rItem.left -= reg.rSelect.left;
         rItem.right -= reg.rSelect.left;
         rItem.top -= reg.rSelect.top;
         rItem.bottom -= reg.rSelect.top;

         // Okay, now we know how big the item will be. Create a bitmap,
         // draw onto it, and stuff it into an imagelist.
         //
         hdc = GetDC (NULL);
         HDC hdcBitmap = CreateCompatibleDC (hdc);
         HBITMAP bmp = CreateCompatibleBitmap (hdc, cxRECT(rDragImage), cyRECT(rDragImage));
         HBITMAP bmpOld = (HBITMAP)SelectObject (hdcBitmap, bmp);
         hfOld = (HFONT)SelectObject (hdcBitmap, pfl->hf);

         HBRUSH hbr = CreateSolidBrush (clrTRANSPARENT);
         FillRect (hdcBitmap, &rDragImage, hbr);
         DeleteObject (hbr);

         FastList_CallPaintItem (pfl, hdcBitmap, TRUE, TRUE, hItem->index, &rItem, NULL, NULL, NULL);

         SelectObject (hdcBitmap, hfOld);
         SelectObject (hdcBitmap, bmpOld);
         DeleteDC (hdcBitmap);
         ReleaseDC (NULL, hdc);

         if ((hil = ImageList_Create (cxRECT(rDragImage), cyRECT(rDragImage), ILC_MASK, 1, 1)) != NULL)
            {
            ImageList_AddMasked (hil, bmp, clrTRANSPARENT);
            }

         DeleteObject (bmp);
         }
      }

   return hil;
}


LPFASTLISTSORTFUNC FastList_OnCommand_GetSortFunc (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      return pfl->pfnSort;
      }

   return NULL;
}


void FastList_OnCommand_SetSortFunc (HWND hList, LPFASTLISTSORTFUNC pfn)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      pfl->pfnSort = pfn;
      pfl->fSortBeforePaint = TRUE;
      }
}


void FastList_OnCommand_GetSortStyle (HWND hList, int *piColSort, BOOL *pfRevSort)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (piColSort)
         *piColSort = pfl->iColSort;
      if (pfRevSort)
         *pfRevSort = pfl->fRevSort;
      }
}


void FastList_OnCommand_SetSortStyle (HWND hList, int iColSort, BOOL fRevSort)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      pfl->iColSort = iColSort;
      pfl->fRevSort = fRevSort;
      pfl->fSortBeforePaint = TRUE;
      FastList_Repaint (pfl);
      }
}


void FastList_OnCommand_Sort (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      pfl->fSortBeforePaint = FALSE;
      FastList_Repaint (pfl);
      }
}


int FastList_OnCommand_GetColumnCount (HWND hList)
{
   int cColumns = 0;

   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      for (cColumns = 0; cColumns < (int)pfl->cColumns; ++cColumns)
         {
         if (pfl->aColumns[ cColumns ].pszText == NULL)
            break;
         }
      }

   return cColumns;
}


BOOL FastList_OnCommand_GetColumn (HWND hList, int iColumn, LPFASTLISTCOLUMN pcol)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) == NULL)
      return FALSE;

   if ((iColumn < 0) || (iColumn >= (int)pfl->cColumns) || (!pfl->aColumns[ iColumn ].pszText))
      return FALSE;

   if (pcol)
      {
      pcol->dwFlags = FLCF_JUSTIFY_LEFT;
      if ((pfl->aColumns[ iColumn ].fmt & HDF_JUSTIFYMASK) == HDF_RIGHT)
         pcol->dwFlags = FLCF_JUSTIFY_RIGHT;
      else if ((pfl->aColumns[ iColumn ].fmt & HDF_JUSTIFYMASK) == HDF_CENTER)
         pcol->dwFlags = FLCF_JUSTIFY_CENTER;
      pcol->cxWidth = pfl->aColumns[ iColumn ].cxy;
      lstrcpy (pcol->szText, pfl->aColumns[ iColumn ].pszText);
      }

   return TRUE;
}


void FastList_OnCommand_SetColumn (HWND hList, int iColumn, LPFASTLISTCOLUMN pcol)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      iColumn = limit (0, iColumn, (int)pfl->cColumns);

      if (REALLOC (pfl->aColumns, pfl->cColumns, 1+iColumn, 1))
         {
         if (pfl->aColumns[ iColumn ].pszText)
            Free (pfl->aColumns[ iColumn ].pszText);
         memset (&pfl->aColumns[ iColumn ], 0x00, sizeof(HD_ITEM));

         if (pcol)
            {
            if ((pcol->dwFlags & FLCF_JUSTIFY_MASK) == FLCF_JUSTIFY_RIGHT)
               pfl->aColumns[ iColumn ].fmt |= HDF_RIGHT;
            else if ((pcol->dwFlags & FLCF_JUSTIFY_MASK) == FLCF_JUSTIFY_CENTER)
               pfl->aColumns[ iColumn ].fmt |= HDF_CENTER;
            else
               pfl->aColumns[ iColumn ].fmt |= HDF_LEFT;

            pfl->aColumns[ iColumn ].cxy = pcol->cxWidth;
            pfl->aColumns[ iColumn ].pszText = (LPTSTR)Allocate (sizeof(TCHAR)*(1+lstrlen(pcol->szText)));
            lstrcpy (pfl->aColumns[ iColumn ].pszText, pcol->szText);

            pfl->aColumns[ iColumn ].mask = HDI_WIDTH | HDI_TEXT | HDI_FORMAT;
            }

         FastList_SyncHeader (pfl);
         }
      }
}


BOOL FastList_OnCommand_IsSelected (HWND hList, HLISTITEM hItem)
{
   return hItem->fSelected;
}


void FastList_OnCommand_SelectItem (HWND hList, HLISTITEM hItem, BOOL fSelect)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (hItem != NULL)
         {
         if (!fSelect || !(hItem->dwFlags & FLIF_DISALLOW_SELECT))
            {
            if (fSelect)
               FastList_PerformSelectTest (pfl, hItem);
            FastList_PerformSelectItem (pfl, hItem, fSelect);
            }
         }
      else
         {
         BOOL fTreeMode = (pfl->dwStyle & FLS_VIEW_TREE);
         BOOL fMultiple = (pfl->dwStyle & FLS_SELECTION_MULTIPLE);
         BOOL fLevel = (pfl->dwStyle & (FLS_SELECTION_LEVEL & ~FLS_SELECTION_MULTIPLE));
         BOOL fSibling = (pfl->dwStyle & (FLS_SELECTION_SIBLING & ~FLS_SELECTION_MULTIPLE));

         if ( (!fSelect) ||
              (fMultiple && (!fTreeMode || (!fLevel && !fSibling))) )
            {
            FastList_Begin (hList);

            for (LPENUM pEnum = pfl->lItems->FindFirst(); pEnum; pEnum = pEnum->FindNext())
               {
               hItem = (HLISTITEM)(pEnum->GetObject());
               if (!fSelect || !(hItem->dwFlags & FLIF_DISALLOW_SELECT))
                  FastList_PerformSelectItem (pfl, hItem, fSelect);
               }

            FastList_End (hList);
            }
         }
      }
}


HLISTITEM FastList_OnCommand_FindList (HWND hList, HLISTITEM hItem, DWORD dwCode)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      switch (dwCode)
         {
         case FLM_FINDLIST_FIRST:
            return pfl->hListFirst;

         case FLM_FINDLIST_PREVIOUS:
            return (hItem) ? (hItem->hListPrevious) : NULL;

         case FLM_FINDLIST_NEXT:
            return (hItem) ? (hItem->hListNext) : (pfl->hListFirst);
         }
      }

   return NULL;
}


HLISTITEM FastList_OnCommand_FindTree (HWND hList, HLISTITEM hItem, DWORD dwCode)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      switch (dwCode)
         {
         case FLM_FINDTREE_PARENT:
            return (hItem) ? (hItem->hTreeParent) : NULL;

         case FLM_FINDTREE_CHILD:
            return (hItem) ? (hItem->hTreeChild) : (pfl->hTreeFirst);

         case FLM_FINDTREE_PREVIOUS:
            return (hItem) ? (hItem->hTreePrevious) : NULL;

         case FLM_FINDTREE_NEXT:
            return (hItem) ? (hItem->hTreeNext) : NULL;
         }
      }

   return NULL;
}

HLISTITEM FastList_OnCommand_FindSelected (HWND hList, HLISTITEM hItem)
{
   if (hItem != NULL)
      {
      return hItem->hSelectNext;
      }

   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      return pfl->hSelectFirst;
      }

   return NULL;
}


HLISTITEM FastList_OnCommand_FindItem (HWND hList, LPENUM *ppEnum, LPARAM lpUser)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (!ppEnum)
         return (HLISTITEM)(pfl->lkUserParam->GetFirstObject (&lpUser));
      else if (ppEnum && (((*ppEnum) = pfl->lkUserParam->FindFirst (&lpUser)) != NULL))
         return (HLISTITEM)((*ppEnum)->GetObject());
      }
   return NULL;
}


HLISTITEM FastList_OnCommand_FindNextItem (HWND hList, LPENUM *ppEnum)
{
   if (ppEnum && (((*ppEnum) = (*ppEnum)->FindNext()) != NULL))
      return (HLISTITEM)((*ppEnum)->GetObject());
   return NULL;
}


void FastList_OnCommand_FindClose (HWND hList, LPENUM *ppEnum)
{
   if (ppEnum && (*ppEnum))
      {
      Delete (*ppEnum);
      *ppEnum = NULL;
      }
}


HLISTITEM FastList_OnCommand_FindVisible (HWND hList, HLISTITEM hItem, DWORD dwCode)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (pfl->fSortBeforePaint)
         FastList_PerformSort (pfl);
      if (pfl->fSyncIndicesBeforePaint)
         FastList_RepairVisibilityIndices (pfl);

      switch (dwCode)
         {
         case FLM_FINDVISIBLE_FIRST:
            return (pfl->cVisible) ? pfl->aVisibleHeap[0] : NULL;

         case FLM_FINDVISIBLE_NEXT:
            if (!hItem->fVisible)
               return NULL;
            if (hItem->index == (int)(pfl->cVisible-1))
               return NULL;
            return pfl->aVisibleHeap[ hItem->index +1 ];
         }
      }

   return NULL;
}


int FastList_OnCommand_GetItemCount (HWND hList, BOOL fVisibleOnly)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) == NULL)
      return 0;

   return (fVisibleOnly) ? pfl->cVisible : pfl->lItems->GetCount();
}


HLISTITEM FastList_OnCommand_ItemFromPoint (HWND hList, POINT *pptClient, BOOL fStrict)
{
   HLISTITEM hItem = NULL;

   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (pfl->fSortBeforePaint)
         FastList_PerformSort (pfl);
      if (pfl->fSyncIndicesBeforePaint)
         FastList_RepairVisibilityIndices (pfl);

      // Adjust the client coordinates we were given based on the current
      // position of the scrollbars.
      //
      LONG xField = pptClient->x + pfl->dxPixel;
      LONG yField = pptClient->y + pfl->dyPixel;

      // Every item's overall RECT is symmetrical, regardless of the
      // current view mode. Find out how big those RECTs are.
      //
      RECT rTemplate;
      FastList_CalcItemRect (pfl, 0, &rTemplate, !!pfl->hScrollH, !!pfl->hScrollV);

      // How large is the current client area?
      //
      RECT rClient;
      GetClientRect (pfl->hList, &rClient);
      if (pfl->hScrollV)
         rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
      if (pfl->hScrollH)
         rClient.bottom -= GetSystemMetrics(SM_CYHSCROLL);
      if (pfl->hHeader)
         rClient.top += FastList_GetHeaderHeight (pfl);

      // The specified point exists in exactly one item (or none at all).
      // Just which item that is depends on what the current view mode is.
      //
      int index = -1;

      switch (pfl->dwStyle & FLS_VIEW_MASK)
         {
         case FLS_VIEW_LARGE:
            {
            LONG ixIndex = xField / cxRECT(rTemplate);
            LONG iyIndex = yField / cyRECT(rTemplate);

            LONG cxArray = cxRECT(rClient) / cxRECT(rTemplate);
            cxArray = max( cxArray, 1 );

            index = ixIndex + iyIndex * cxArray;
            break;
            }

         case FLS_VIEW_SMALL:
            {
            LONG ixIndex = xField / cxRECT(rTemplate);
            LONG iyIndex = yField / cyRECT(rTemplate);

            LONG cyArray = cyRECT(rClient) / cyRECT(rTemplate);
            cyArray = max( cyArray, 1 );

            index = iyIndex + ixIndex * cyArray;
            break;
            }

         case FLS_VIEW_LIST:
         case FLS_VIEW_TREE:
         case FLS_VIEW_TREELIST:
            index = (yField - rClient.top) / cyRECT(rTemplate);
            break;
         }

      if ((index >= 0) && (index < (int)pfl->cVisible))
         hItem = pfl->aVisibleHeap[ index ];

      // If there is indeed an item underneath that point, test the item's
      // regions--and our window styles--to see if a click on the specified
      // point actually *changes* that item.
      //
      if (hItem && fStrict)
         {
         BOOL fHit = FALSE;

         FASTLISTITEMREGIONS reg;
         FastList_OnCommand_GetItemRegions (hList, hItem, &reg, pptClient, &fHit);

         if (!PtInRect (&reg.rSelect, *pptClient))
            hItem = NULL;
         else if (PtInRect (&reg.rHighlight, *pptClient) && !fHit && (pfl->dwStyle & FLS_HIT_TEXTONLY))
            hItem = NULL;
         }
      }

   return hItem;
}


void FastList_OnCommand_GetItemRegions (HWND hList, HLISTITEM hItem, LPFASTLISTITEMREGIONS pReg, POINT *pptTest, BOOL *pfHit)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      if (pfl->fSortBeforePaint)
         FastList_PerformSort (pfl);
      if (pfl->fSyncIndicesBeforePaint)
         FastList_RepairVisibilityIndices (pfl);

      if (hItem && !hItem->fVisible && pReg)
         {
         memset (pReg, 0x00, sizeof(FASTLISTITEMREGIONS));
         }
      else if (hItem && hItem->fVisible)
         {
         HDC hdc = CreateCompatibleDC (NULL);
         HFONT hfOld = (HFONT)SelectObject (hdc, pfl->hf);

         RECT rItem;
         FastList_CalcItemRect (pfl, hItem->index, &rItem, !!pfl->hScrollH, !!pfl->hScrollV);
         rItem.left -= pfl->dxPixel;
         rItem.right -= pfl->dxPixel;
         rItem.top -= pfl->dyPixel;
         rItem.bottom -= pfl->dyPixel;

         FastList_CallPaintItem (pfl, hdc, FALSE, FALSE, hItem->index, &rItem, pReg, pptTest, pfHit);

         SelectObject (hdc, hfOld);
         DeleteDC (hdc);
         }
      }
}


LPFASTLISTTEXTCALLBACK FastList_OnCommand_GetTextCallback (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      return pfl->pfnText;

   return NULL;
}


void FastList_OnCommand_SetTextCallback (HWND hList, LPFASTLISTTEXTCALLBACK pfn, DWORD dwCookie)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      pfl->pfnText = pfn;
      pfl->dwCookieText = dwCookie;
      }
}


/*
 * NOTIFICATION ROUTINES ______________________________________________________
 *
 */

BOOL FastList_Notify_GetItemText (LPFASTLIST pfl, HLISTITEM hItem, int icol, LPTSTR pszText, size_t cchText, size_t *pcchRequired)
{
   FLN_GETITEMTEXT_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_GETITEMTEXT;
   fln.item.hItem = hItem;
   fln.item.icol = icol;
   fln.item.lParam = hItem->lpUser;
   fln.item.pszText = pszText;
   fln.item.cchTextMax = cchText;

   if (pfl->pfnText)
      {
      if (!(*pfl->pfnText)( pfl->hList, &fln, pfl->dwCookieText ))
         return FALSE;
      }
   else
      {
      if (!GetParent (pfl->hList))
         return FALSE;
      if (!SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln))
         return FALSE;
      }

   *pcchRequired = fln.item.cchTextMax;
   return TRUE;
}


BOOL FastList_Notify_ItemChanged (LPFASTLIST pfl, HLISTITEM hItem)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   FLN_ITEMCHANGED_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_ITEMCHANGED;
   fln.hItem = hItem;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
}


BOOL FastList_Notify_AddItem (LPFASTLIST pfl, HLISTITEM hItem)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   FLN_ADDITEM_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_ADDITEM;
   fln.hItem = hItem;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
}


BOOL FastList_Notify_RemoveItem (LPFASTLIST pfl, HLISTITEM hItem)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   FLN_REMOVEITEM_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_REMOVEITEM;
   fln.hItem = hItem;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
}


BOOL FastList_Notify_ColumnClick (LPFASTLIST pfl, int icol, BOOL fDouble)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   FLN_COLUMNCLICK_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_COLUMNCLICK;
   fln.icol = icol;
   fln.fDouble = fDouble;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
}


BOOL FastList_Notify_ColumnResize (LPFASTLIST pfl, int icol, LONG cxWidth)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   FLN_COLUMNRESIZE_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_COLUMNRESIZE;
   fln.icol = icol;
   fln.cxWidth = cxWidth;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
}


BOOL FastList_Notify_ItemSelect (LPFASTLIST pfl)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   FLN_ITEMSELECT_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_ITEMSELECT;
   fln.hItem = pfl->hSelectFirst;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
}


BOOL FastList_Notify_ItemExpand (LPFASTLIST pfl, HLISTITEM hItem)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   FLN_ITEMEXPAND_PARAMS fln;
   fln.hdr.hwndFrom = pfl->hList;
   fln.hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   fln.hdr.code = FLN_ITEMEXPAND;
   fln.hItem = hItem;
   fln.fExpanded = hItem->fExpanded;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
}


BOOL FastList_Notify_Generic (LPFASTLIST pfl, DWORD dwCode)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   NMHDR hdr;
   hdr.hwndFrom = pfl->hList;
   hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   hdr.code = dwCode;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
}


BOOL FastList_Notify_Drag (LPFASTLIST pfl, DWORD dwCode, LPFLN_DRAG_PARAMS pfln)
{
   if (!GetParent (pfl->hList))
      return FALSE;

   pfln->hdr.hwndFrom = pfl->hList;
   pfln->hdr.idFrom = GetWindowLong (pfl->hList, GWL_ID);
   pfln->hdr.code = dwCode;
   return (BOOL)SendMessage (GetParent (pfl->hList), WM_NOTIFY, (WPARAM)pfln->hdr.idFrom, (LPARAM)pfln);
}


/*
 * SUPPORT ROUTINES ___________________________________________________________
 *
 */

void FastList_Repaint (LPFASTLIST pfl)
{
   if ((pfl->fRepaintRequired = (pfl->nBegin) ? TRUE : FALSE) == FALSE)
      {
      RECT rClient;
      GetClientRect (pfl->hList, &rClient);
      InvalidateRect (pfl->hList, &rClient, TRUE);
      UpdateWindow (pfl->hList);
      }
}


HLISTITEM FastList_CreateItem (LPFASTLIST pfl)
{
   HLISTITEM hItem = New (LISTITEM);
   memset (hItem, 0x00, sizeof(LISTITEM));
   hItem->fExpanded = TRUE;
   return hItem;
}


void FastList_DeleteItem (LPFASTLIST pfl, HLISTITEM hItem)
{
   if (hItem)
      {
      // Remove any children of this item, even if we're not in
      // treeview mode now.
      //
      while (hItem->hTreeChild)
         {
         FastList_DeleteItem (pfl, hItem->hTreeChild);
         }

      FastList_Notify_RemoveItem (pfl, hItem); // notify *before* delete

       if (pfl->hFocused == hItem)
          pfl->hFocused = FALSE;

      // Unlink this item from the List chain
      //
      if (pfl->hListFirst == hItem)
         pfl->hListFirst = hItem->hListNext;
      if (hItem->hListPrevious)
         hItem->hListPrevious->hListNext = hItem->hListNext;
      if (hItem->hListNext)
         hItem->hListNext->hListPrevious = hItem->hListPrevious;

      // Unlink this item from the Tree chain
      //
      if (pfl->hTreeFirst == hItem)
         pfl->hTreeFirst = hItem->hTreeNext;
      if (hItem->hTreePrevious)
         hItem->hTreePrevious->hTreeNext = hItem->hTreeNext;
      if (hItem->hTreeNext)
         hItem->hTreeNext->hTreePrevious = hItem->hTreePrevious;
      if (hItem->hTreeParent && (hItem->hTreeParent->hTreeChild == hItem))
         hItem->hTreeParent->hTreeChild = hItem->hTreeNext;

      // Unlink this item from the Selection chain
      //
      if (hItem->hSelectPrevious)
         hItem->hSelectPrevious->hSelectNext = hItem->hSelectNext;
      if (hItem->hSelectNext)
         hItem->hSelectNext->hSelectPrevious = hItem->hSelectPrevious;
      if (pfl->hSelectFirst == hItem)
         pfl->hSelectFirst = hItem->hSelectNext;
      if (pfl->hAnchor == hItem)
         pfl->hAnchor = NULL;
      hItem->hSelectPrevious = NULL;
      hItem->hSelectNext = NULL;

      // If we're holding onto this item as the next guy who we should
      // ensure is visible, forget about it there too.
      //
      if (pfl->hEnsureVisible == hItem)
         pfl->hEnsureVisible = NULL;

      // Remove this item from the hashlist, and deallocate any memory
      // associated with it.
      //
      pfl->lItems->Remove (hItem);

      if (hItem->apszText)
         {
         for (size_t iCol = 0; iCol < hItem->cpszText; ++iCol)
            {
            if (hItem->apszText[ iCol ] != NULL)
               Free (hItem->apszText[ iCol ]);
            }
         Free (hItem->apszText);
         }

#ifdef DEBUG
      memset (hItem, 0xFE, sizeof(_FASTLISTITEM));
#endif
      Delete (hItem);

      pfl->fSyncScrollBeforePaint = TRUE;
      pfl->fSyncIndicesBeforePaint = TRUE;
      }
}


int FastList_GetListHeight (LPFASTLIST pfl)
{
   static HFONT hfLast = NULL;
   static int cyLast = 0;

   if (!cyLast || (pfl->hf != hfLast))
      {
      hfLast = pfl->hf;

      TEXTMETRIC tm;
      HDC hdc = CreateCompatibleDC (NULL);
      HFONT hfOld = (HFONT)SelectObject (hdc, pfl->hf);
      GetTextMetrics (hdc, &tm);
      SelectObject (hdc, hfOld);
      DeleteDC (hdc);

      cyLast = tm.tmHeight + cyABOVE_LIST_TEXT + cyBELOW_LIST_TEXT;
      cyLast = max( cyLast, GetSystemMetrics(SM_CYSMICON) );
      if (cyLast & 1)
         cyLast ++;  // make the height even (for vertical dotted lines)
      }

   return cyLast;
}


int FastList_GetListWidth (LPFASTLIST pfl)
{
   LONG cxWidth = 0;

   for (size_t iColumn = 0; iColumn < pfl->cColumns; ++iColumn)
      if (pfl->aColumns[ iColumn ].pszText)
         cxWidth += pfl->aColumns[ iColumn ].cxy;

   if (cxWidth == 0)  // no columns specified? pretend there's just one.
      {
      RECT rClient;
      GetClientRect (pfl->hList, &rClient);
      if (pfl->hScrollV)
         rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
      cxWidth = cxRECT(rClient);
      }

   return cxWidth;
}


LONG FastList_GetHeaderHeight (LPFASTLIST pfl)
{
   TEXTMETRIC tm;
   HDC hdc = CreateCompatibleDC (NULL);
   HFONT hfOld = (HFONT)SelectObject (hdc, pfl->hf);
   GetTextMetrics (hdc, &tm);
   SelectObject (hdc, hfOld);
   DeleteDC (hdc);
   return 4*GetSystemMetrics(SM_CYBORDER) + tm.tmHeight;
}


void FastList_GetHeaderRect (LPFASTLIST pfl, RECT *prHeader)
{
   GetClientRect (pfl->hList, prHeader);
   prHeader->bottom = prHeader->top + FastList_GetHeaderHeight (pfl);
}


void FastList_UpdateColumn (LPFASTLIST pfl, int iColumn)
{
   if ((iColumn >= 0) && (iColumn < (int)pfl->cColumns) && (pfl->aColumns[ iColumn ].pszText))
      {
      HD_ITEM col;
      memset (&col, 0x00, sizeof(col));
      col.mask |= HDI_WIDTH;
      Header_GetItem (pfl->hHeader, iColumn, &col);

      if (pfl->aColumns[ iColumn ].cxy != col.cxy)
         {
         pfl->aColumns[ iColumn ].cxy = col.cxy;
         pfl->fSyncScrollBeforePaint = TRUE;
         FastList_Notify_ColumnResize (pfl, iColumn, col.cxy);
         FastList_Repaint (pfl);
         }
      }
}


void FastList_SyncHeader (LPFASTLIST pfl)
{
   if ((pfl->fSyncHeaderRequired = (pfl->nBegin) ? TRUE : FALSE) == FALSE)
      {
      BOOL fNeedsHeader = ((pfl->dwStyle & FLS_VIEW_LIST) == FLS_VIEW_LIST);
      BOOL fNoSortHeader = (pfl->dwStyle & FLS_NOSORTHEADER);

      if (pfl->cColumns == 0)
         fNeedsHeader = FALSE;

      RECT rHeader;
      if (fNeedsHeader)
         FastList_GetHeaderRect (pfl, &rHeader);

      if (fNeedsHeader && !pfl->hHeader)
         {
         pfl->hHeader = CreateWindow (WC_HEADER, TEXT(""),
                                      0x80 | WS_CHILD | WS_VISIBLE | ((fNoSortHeader) ? 0 : HDS_BUTTONS),
                                      rHeader.left, rHeader.top,
                                      cxRECT(rHeader), cyRECT(rHeader),
                                      pfl->hList,
                                      (HMENU)0,
                                      THIS_HINST,
                                      (LPVOID)0);

         SendMessage (pfl->hHeader, WM_SETFONT, (WPARAM)(pfl->hf), 0);
         }
      else if (fNeedsHeader && pfl->hHeader)
         {
         DWORD dwStyle = GetWindowLong (pfl->hHeader, GWL_STYLE);
         if (fNoSortHeader)
            dwStyle &= ~HDS_BUTTONS;
         else
            dwStyle |= HDS_BUTTONS;
         SetWindowLong (pfl->hHeader, GWL_STYLE, dwStyle);

         SetWindowPos (pfl->hHeader, 0,
                       rHeader.left, rHeader.top, cxRECT(rHeader), cyRECT(rHeader),
                       SWP_NOACTIVATE | SWP_NOZORDER);
         }
      else if (!fNeedsHeader && pfl->hHeader)
         {
         DestroyWindow (pfl->hHeader);
         pfl->hHeader = NULL;
         }

      // If we ended up with a header window, update its columns.
      //
      if (pfl->hHeader)
         {
         while (Header_DeleteItem (pfl->hHeader, 0))
            ;
         for (size_t iCol = 0; iCol < pfl->cColumns; ++iCol)
            Header_InsertItem (pfl->hHeader, iCol, &pfl->aColumns[ iCol ]);
         }

      pfl->fSyncScrollBeforePaint = TRUE;
      }
}


void FastList_SyncScroll (LPFASTLIST pfl)
{
   pfl->fSyncScrollBeforePaint = FALSE;

   RECT rClient;
   GetClientRect (pfl->hList, &rClient);
   if (pfl->hHeader)
      rClient.top += FastList_GetHeaderHeight (pfl);

   LONG cxField = 0;
   LONG cyField = 0;

   // First find out how much space we'd need, horizontally and vertically,
   // to display all the items we can. Remember that the number of items
   // we can display changes depending on whether the scrollbars appear or
   // not.
   //
   BOOL fAllowHScroll = ((pfl->dwStyle & FLS_VIEW_MASK) != FLS_VIEW_LARGE);
   BOOL fAllowVScroll = ((pfl->dwStyle & FLS_VIEW_MASK) != FLS_VIEW_SMALL);

   BOOL fNeedHScroll = FALSE;
   BOOL fNeedVScroll = FALSE;

   FastList_CalcFieldSize (pfl, &cxField, &cyField, fNeedHScroll, fNeedVScroll);

   if ( (cxField > cxRECT(rClient)) && (fAllowHScroll) )
      fNeedHScroll = TRUE;
   if ( (cyField > cyRECT(rClient)) && (fAllowVScroll) )
      fNeedVScroll = TRUE;

   FastList_CalcFieldSize (pfl, &cxField, &cyField, fNeedHScroll, fNeedVScroll);

   RECT rTest = rClient;
   if (pfl->hScrollV)
      rTest.right -= GetSystemMetrics(SM_CXVSCROLL);
   if (pfl->hScrollH)
      rTest.bottom -= GetSystemMetrics(SM_CYHSCROLL);

   if ( (cxField > cxRECT(rTest)) && (fAllowHScroll) )
      fNeedHScroll = TRUE;
   if ( (cyField > cyRECT(rTest)) && (fAllowVScroll) )
      fNeedVScroll = TRUE;

   // Now that we know the field size, create, remove or position
   // the scroll bars.
   //
   if (pfl->hScrollV && !fNeedVScroll)
      {
      DestroyWindow (pfl->hScrollV);
      pfl->hScrollV = NULL;
      pfl->dyPixel = 0;
      }
   else if (fNeedVScroll)
      {
      RECT rScroll = rClient;
      rScroll.left = rScroll.right - GetSystemMetrics (SM_CXVSCROLL);
      if (fNeedHScroll)
         rScroll.bottom -= GetSystemMetrics (SM_CYHSCROLL);

      if (!pfl->hScrollV)
         {
         pfl->hScrollV = CreateWindow ("ScrollBar", TEXT(""),
                                       WS_CHILD | WS_VISIBLE | SBS_VERT,
                                       rScroll.left, rScroll.top,
                                       cxRECT(rScroll), cyRECT(rScroll),
                                       pfl->hList,
                                       (HMENU)0,
                                       THIS_HINST,
                                       (LPVOID)0);
         }
      else
         {
         SetWindowPos (pfl->hScrollV, 0, 
                       rScroll.left, rScroll.top, cxRECT(rScroll), cyRECT(rScroll),
                       SWP_NOZORDER | SWP_NOACTIVATE);
         }
      }
   else // (!fNeedVScroll)
      {
      pfl->dyPixel = 0;
      }

   if (pfl->hScrollH && !fNeedHScroll)
      {
      DestroyWindow (pfl->hScrollH);
      pfl->hScrollH = NULL;
      pfl->dxPixel = 0;
      }
   else if (fNeedHScroll)
      {
      RECT rScroll = rClient;
      rScroll.top = rScroll.bottom - GetSystemMetrics (SM_CYHSCROLL);
      if (fNeedVScroll)
         rScroll.right -= GetSystemMetrics (SM_CXVSCROLL);

      if (!pfl->hScrollH)
         {
         pfl->hScrollH = CreateWindow ("ScrollBar", TEXT(""),
                                       WS_CHILD | WS_VISIBLE | SBS_HORZ,
                                       rScroll.left, rScroll.top,
                                       cxRECT(rScroll), cyRECT(rScroll),
                                       pfl->hList,
                                       (HMENU)0,
                                       THIS_HINST,
                                       (LPVOID)0);
         }
      else
         {
         SetWindowPos (pfl->hScrollH, 0, 
                       rScroll.left, rScroll.top, cxRECT(rScroll), cyRECT(rScroll),
                       SWP_NOZORDER | SWP_NOACTIVATE);
         }
      }
   else // (!fNeedHScroll)
      {
      pfl->dxPixel = 0;
      }

   FastList_SyncScrollPos (pfl);
}


void FastList_SyncScrollPos (LPFASTLIST pfl)
{
   RECT rClient;
   GetClientRect (pfl->hList, &rClient);
   if (pfl->hScrollV)
      rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
   if (pfl->hScrollH)
      rClient.bottom -= GetSystemMetrics(SM_CYHSCROLL);
   if (pfl->hHeader)
      rClient.top += FastList_GetHeaderHeight (pfl);

   LONG cxField;
   LONG cyField;
   FastList_CalcFieldSize (pfl, &cxField, &cyField, !!pfl->hScrollH, !!pfl->hScrollV);

   if (pfl->hScrollH)
      pfl->dxPixel = limit( 0, pfl->dxPixel, (cxField-cxRECT(rClient)) );
   if (pfl->hScrollV)
      pfl->dyPixel = limit( 0, pfl->dyPixel, (cyField-cyRECT(rClient)) );

   SCROLLINFO si;
   if (pfl->hScrollH)
      {
      memset (&si, 0x00, sizeof(SCROLLINFO));
      si.cbSize = sizeof(si);
      si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
      si.nMin = 0;
      si.nMax = cxField;
      si.nPage = cxRECT(rClient);
      si.nPos = pfl->dxPixel;
      SetScrollInfo (pfl->hScrollH, SB_CTL, &si, TRUE);
      }

   if (pfl->hScrollV)
      {
      memset (&si, 0x00, sizeof(SCROLLINFO));
      si.cbSize = sizeof(si);
      si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
      si.nMin = 0;
      si.nMax = cyField;
      si.nPage = cyRECT(rClient);
      si.nPos = pfl->dyPixel;
      SetScrollInfo (pfl->hScrollV, SB_CTL, &si, TRUE);
      }

   FastList_ScrollHeader (pfl);
}


void FastList_CalcFieldSize (LPFASTLIST pfl, LONG *pcxField, LONG *pcyField, BOOL fScrollH, BOOL fScrollV)
{
   RECT rClient;
   GetClientRect (pfl->hList, &rClient);
   if (fScrollV)
      rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
   if (fScrollH)
      rClient.bottom -= GetSystemMetrics(SM_CYHSCROLL);
   if (pfl->hHeader)
      rClient.top += FastList_GetHeaderHeight (pfl);

   *pcxField = 0;
   *pcyField = 0;

   RECT rItem;
   FastList_CalcItemRect (pfl, 0, &rItem, fScrollH, fScrollV);

   LONG cxArray = 0;
   LONG cyArray = 0;

   switch (pfl->dwStyle & FLS_VIEW_MASK)
      {
      case FLS_VIEW_LARGE:
         cxArray = cxRECT(rClient) / cxRECT(rItem);
         cxArray = max (cxArray, 1);
         if (cxArray)
            cyArray = DivRoundUp (pfl->cVisible, cxArray);
         break;

      case FLS_VIEW_SMALL:
         cyArray = cyRECT(rClient) / cyRECT(rItem);
         cyArray = max (cyArray, 1);
         if (cyArray)
            cxArray = DivRoundUp (pfl->cVisible, cyArray);
         break;

      case FLS_VIEW_LIST:
      case FLS_VIEW_TREE:
      case FLS_VIEW_TREELIST:
         cxArray = 1;
         cyArray = pfl->cVisible;
         break;
      }

   *pcxField = cxArray * cxRECT(rItem);
   *pcyField = cyArray * cyRECT(rItem);
}


void FastList_CalcItemRect (LPFASTLIST pfl, int index, RECT *prItem, BOOL fScrollH, BOOL fScrollV)
{
   SetRectEmpty (prItem);

   RECT rClient;
   GetClientRect (pfl->hList, &rClient);
   if (fScrollV)
      rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
   if (fScrollH)
      rClient.bottom -= GetSystemMetrics(SM_CYHSCROLL);
   if (pfl->hHeader)
      rClient.top += FastList_GetHeaderHeight (pfl);

   switch (pfl->dwStyle & FLS_VIEW_MASK)
      {
      case FLS_VIEW_LARGE:
         {
         // Large layout shows a two-dimensional list of 32x32 icons,
         // each followed beneath by text. The layout of the grid is
         // determined by well-known system metrics. There will be no
         // horizontal scrollbar; just a vertical one (if necessary).
         // Slots are filled in left-to-right, top-to-bottom.
         //
         LONG cxRect = GetSystemMetrics (SM_CXICONSPACING);
         LONG cyRect = GetSystemMetrics (SM_CYICONSPACING);
         int cxLayout = cxRECT(rClient) / cxRect;
         cxLayout = max (cxLayout, 1);

         int vIndex = index / cxLayout;  // 0 = top row
         int hIndex = index % cxLayout;  // 0 = left row

         prItem->left = hIndex * cxRect;
         prItem->right = prItem->left + cxRect;
         prItem->top = vIndex * cyRect;
         prItem->bottom = prItem->top + cyRect;
         break;
         }

      case FLS_VIEW_SMALL:
         {
         // Small layout is similar: it shows a two-dimensional list of
         // 16x16 icons, each followed to the right by text. The vertical
         // layout of the grid is the same as the list default vertical
         // size; the horizontal layout is twice the default system metric.
         // Another difference: Small mode uses only a horizontal scrollbar,
         // not a vertical one. Slots are filled in top-to-bottom,
         // left-to-right.
         //
         LONG cxRect = GetSystemMetrics (SM_CXICONSPACING) * 2;
         LONG cyRect = FastList_GetListHeight (pfl);
         int cyLayout = cyRECT(rClient) / cyRect;
         cyLayout = max (cyLayout, 1);

         int hIndex = index / cyLayout;  // 0 = left row
         int vIndex = index % cyLayout;  // 0 = top row

         prItem->left = hIndex * cxRect;
         prItem->right = prItem->left + cxRect;
         prItem->top = vIndex * cyRect;
         prItem->bottom = prItem->top + cyRect;
         break;
         }

      case FLS_VIEW_LIST:
      case FLS_VIEW_TREE:
      case FLS_VIEW_TREELIST:
         {
         // List and Tree layouts are fairly straight-forward: they each show
         // a one-dimensional array of items, each of which runs horizontally
         // from edge to edge. The vertical layout of the grid is the default
         // vertical size (see FastList_GetListHeight()). A vertical scrollbar
         // is applied if necessary; a horizontal scrollbar is added for the
         // header.
         //
         LONG cyRect = FastList_GetListHeight (pfl);

         prItem->left = 0;
         prItem->right = FastList_GetListWidth (pfl);
         prItem->top = rClient.top + index * cyRect;
         prItem->bottom = prItem->top + cyRect;
         break;
         }
      }
}


void FastList_CallPaintItem (LPFASTLIST pfl, HDC hdc, BOOL fDraw, BOOL fDragImage, int iItem, RECT *prItem, LPFASTLISTITEMREGIONS pReg, POINT *pptTest, BOOL *pfHit)
{
   FASTLISTDRAWITEM di;
   memset (&di, 0x00, sizeof(di));
   di.hdc = hdc;
   di.fDraw = fDraw;
   di.fDragImage = fDragImage;
   di.hWnd = pfl->hList;
   di.hItem = pfl->aVisibleHeap[ iItem ];
   di.lParam = di.hItem->lpUser;
   di.rItem = *prItem;

   if (pptTest)
      di.ptTextTest = *pptTest;
   if (pfHit)
      *pfHit = FALSE;

   FastList_OnPaintItem (pfl, &di);
// if (GetParent (pfl->hList))
//    SendMessage (GetParent (pfl->hList), WM_DRAWITEM, (WPARAM)GetWindowLong (pfl->hList, GWL_ID), (LPARAM)&di);
// else
//    FastList_OnPaintItem (pfl, &di);

   if (pfHit)
      *pfHit = di.fTextTestHit;
   if (pReg)
      memcpy (pReg, &di.reg, sizeof(FASTLISTITEMREGIONS));
}


LPTSTR FastList_GetColumnText (LPFASTLIST pfl, HLISTITEM hItem, int icol, BOOL fAlternateBuffer)
{
   if ((icol < (int)hItem->cpszText) && (hItem->apszText[ icol ] != NULL))
      return hItem->apszText[ icol ];

   if (!GetParent (pfl->hList))
      return TEXT("");

   // We'll have to send a message to the parent window, and hope the user
   // catches it to tell us what the text should be. We have two dynamically-
   // allocated buffers we use for storing text.
   //
   static LPTSTR pszTextA = NULL;
   static size_t cchTextA = 0;

   static LPTSTR pszTextB = NULL;
   static size_t cchTextB = 0;

   LPTSTR *ppszText = (fAlternateBuffer) ? &pszTextB : &pszTextA;
   size_t *pcchText = (fAlternateBuffer) ? &cchTextB : &cchTextA;

   for (size_t cchRequired = 256; ; )
      {
      if (!*pcchText || (*pcchText < cchRequired))
         {
         if (*ppszText)
            Free (*ppszText);
         if ((*ppszText = (LPTSTR)Allocate (sizeof(TCHAR)*(1+cchRequired))) == NULL)
            *pcchText = 0;
         else
            *pcchText = cchRequired;
         }
      if (!*pcchText)
         break;

      *(*ppszText) = TEXT('\0');
      if (!FastList_Notify_GetItemText (pfl, hItem, icol, *ppszText, *pcchText, &cchRequired))
         break;
      if (cchRequired <= *pcchText)
         break;
      }

   return (*ppszText) ? (*ppszText) : TEXT("");
}


void FastList_PerformSort (LPFASTLIST pfl)
{
   pfl->fSortBeforePaint = FALSE;

   // Sorting would be easy, except that we have to handle the tree case.
   // For a tree, "sorting" means ordering *sibling* items--we make no
   // attempt to order cousins, but all the children under a given parent
   // must be sorted. We don't sort the Visible heap directly--instead,
   // we sort the hashlist of items, and correct the Visible heap to match.
   // That way, the user can later expand/collapse items in the tree
   // without our having to re-sort.
   //
   // In sorting the hashlist items, we're not so much interested in their
   // order of enumeration as we're interested in pointing each object's
   // {hPrevious} and {hNext} pointers to the right objects. (When sorting
   // a tree, this is even more clear; hPrevious and hNext point only to
   // siblings.)  To perform a sort on these items, we must generate an
   // array of objects, qsort the array, then update the items' pointers.
   // That array of objects is in {fg}, the fastlist global structure.
   //
   // The flow of sorting a list is:
   //    1- fill an array with HLISTITEM pointers for all items in the list
   //    2- qsort the array, using the {pfl->fnSort} comparison function
   //    3- update the HLISTITEMs' {hPrevious}, {hNext}, and {index} members
   //    4- call FastList_Update to fix the hash on the revised {index} members
   //
   // The flow for sorting a tree is more complex:
   //    1- set indexNext = 0
   //    2- call SortLevel(NULL), to specify that we're sorting root items)
   //    SortLevel(hParent):
   //       3- fill an array with HLISTITEM pointers for this item and siblings
   //       4- qsort the array, using the {pfl->fnSort} comparison function
   //       5- update the HLISTITEMs' {hPrevious}, {hNext} members
   //       6- use {indexNext++} to assign index values to all visible items
   //       7- if hParent->hTreeChild, call SortLevel(hParent->hTreeChild)
   //    8- call FastList_Update to fix the hash on the revised {index} members
   //
   FastList_Begin (pfl->hList);

   size_t cItems;
   if ((cItems = pfl->lItems->GetCount()) != 0)
      {
      if (OpenGlobalArray (cItems))
         {
         fg.pfl = pfl;
         FastList_PerformSortLevel (pfl, NULL, ((pfl->dwStyle & FLS_VIEW_TREE) == FLS_VIEW_TREE));

         CloseGlobalArray();
         }
      }

   pfl->fSyncIndicesBeforePaint = TRUE;
   FastList_End (pfl->hList);
}


void FastList_PerformSortLevel (LPFASTLIST pfl, HLISTITEM hParent, BOOL fTreeSort)
{
   // Find the first item in the chain that we're going to sort.
   // Then walk that chain, filling in the global array of HLISTITEMs
   // If we're sorting the thing as a tree, only add {hItem}'s siblings
   // to the fg.aObjects[] array; if we're sorting as a list, add
   // all items. Add items regardless of whether they're invisible.
   //
   size_t cObjectsToSort = 0;

   if (fTreeSort)
      {
      for (HLISTITEM hItem = (hParent) ? hParent->hTreeChild : pfl->hTreeFirst; hItem; hItem = hItem->hTreeNext)
         fg.aObjects[ cObjectsToSort++ ] = hItem;
      }
   else // Viewing by list, so sort items as if there were no tree structure
      {
      for (HLISTITEM hItem = pfl->hListFirst; hItem; hItem = hItem->hListNext)
         fg.aObjects[ cObjectsToSort++ ] = hItem;
      }

   if (cObjectsToSort != 0)
      {
      // Perform a qsort on the array
      //
      FastList_SortFunction (0, 0);
      qsort (fg.aObjects, cObjectsToSort, sizeof(HLISTITEM), FastList_SortFunction);

      // Walk the array, adjusting objects' next/previous pointers.
      //
      if (fTreeSort)
         {
         HLISTITEM hTreePrevious = NULL;
         for (size_t iObject = 0; iObject < cObjectsToSort; ++iObject)
            {
            fg.aObjects[ iObject ]->hTreePrevious = hTreePrevious;
            fg.aObjects[ iObject ]->hTreeNext = (iObject < cObjectsToSort-1) ? fg.aObjects[ iObject+1 ] : NULL;
            hTreePrevious = fg.aObjects[ iObject ];
            }
         if (hParent == NULL)
            pfl->hTreeFirst = fg.aObjects[0];
         else
            hParent->hTreeChild = fg.aObjects[0];
         }
      else // (!(pfl->dwStyle & FLS_VIEW_TREE))
         {
         HLISTITEM hListPrevious = NULL;
         for (size_t iObject = 0; iObject < cObjectsToSort; ++iObject)
            {
            fg.aObjects[ iObject ]->hListPrevious = hListPrevious;
            fg.aObjects[ iObject ]->hListNext = (iObject < cObjectsToSort-1) ? fg.aObjects[ iObject+1 ] : NULL;
            hListPrevious = fg.aObjects[ iObject ];
            }
         pfl->hListFirst = fg.aObjects[0];
         }

      // If this is to be a treesort, walk the chain ascending children
      // recursively as we come to them.
      //
      if (fTreeSort)
         {
         for (HLISTITEM hWalk = fg.aObjects[0]; hWalk; hWalk = hWalk->hTreeNext)
            {
            if (hWalk && hWalk->hTreeChild)
               FastList_PerformSortLevel (pfl, hWalk, fTreeSort);
            }
         }
      }
}


int __cdecl FastList_SortFunction (const void *lp1, const void *lp2)
{
   if (fg.pfl->pfnSort)
      return (*(fg.pfl->pfnSort))(fg.pfl->hList, ((lp1) ? (*(HLISTITEM*)lp1) : 0), ((lp1) ? (*(HLISTITEM*)lp1)->lpUser : 0), ((lp2) ? (*(HLISTITEM*)lp2) : 0), ((lp2) ? (*(HLISTITEM*)lp2)->lpUser : 0));
   else
      return FastList_SortFunc_AlphaNumeric (fg.pfl->hList, ((lp1) ? (*(HLISTITEM*)lp1) : 0), ((lp1) ? (*(HLISTITEM*)lp1)->lpUser : 0), ((lp2) ? (*(HLISTITEM*)lp2) : 0), ((lp2) ? (*(HLISTITEM*)lp2)->lpUser : 0));
}


void FastList_RepairOneVisibilityFlag (LPFASTLIST pfl, HLISTITEM hItem)
{
   // There are two reasons to hide an item: because we're in tree mode
   // and one of its parent is collapsed, or because we're not in tree mode
   // and it has the only-show-me-in-tree-mode flag set.
   //
   BOOL fTreeMode = (pfl->dwStyle & FLS_VIEW_TREE) ? TRUE : FALSE;

   hItem->fVisible = TRUE;

   if ((hItem->dwFlags & FLIF_TREEVIEW_ONLY) && (!fTreeMode))
      {
      hItem->fVisible = FALSE;
      }
   else if (fTreeMode)
      {
      for (HLISTITEM hParent = hItem->hTreeParent; hParent; hParent = hParent->hTreeParent)
         {
         if ((!hParent->fVisible) || (!hParent->fExpanded))
            {
            hItem->fVisible = FALSE;
            break;
            }
         }
      }
}


void FastList_RepairVisibilityFlags (LPFASTLIST pfl, HLISTITEM hParent, BOOL fForceHidden)
{
   BOOL fTreeMode = (pfl->dwStyle & FLS_VIEW_TREE) ? TRUE : FALSE;

   // This routine fixes the {fVisible} flags for all items, based on
   // their FLIF_TREEVIEW_ONLY flags and whether their parent items are
   // visible.
   //
   // Technically we could acheive this by a normal FindFirst()/FindNext()
   // enumeration of the items in the hashlist; on each item, we'd check
   // its parents and flags. But we can be a smidgen faster by walking
   // the list in tree order--that way, when we hit a collapsed tree, we
   // can mark all its children as hidden without any additional work.
   //
   // Naturally, since we're a FASTlist, we'll take any little smidgen of
   // speed we can get.  Find the first item in the chain that we're going
   // to fix.
   //
   for (HLISTITEM hItem = (hParent) ? hParent->hTreeChild : pfl->hTreeFirst; hItem; hItem = hItem->hTreeNext)
      {
      // If we were passed {fForceHidden==TRUE}, we know that we're in tree
      // mode and one of our parents is collapsed--that's great, because it
      // means we can set fVisible=FALSE without thinking. If we weren't passed
      // that flag, we know that we're either not in treeview mode or none
      // of our parents are collapsed--so we don't have to walk up the
      // list of parents to check that.
      //
      if (fForceHidden)
         hItem->fVisible = FALSE;
      else
         hItem->fVisible = fTreeMode || !(hItem->dwFlags & FLIF_TREEVIEW_ONLY);

      // We've fixed this item. If it has any children, fix them too.
      // Remember that we may be able to blindly tell children to be hidden.
      //
      if (hItem->hTreeChild)
         {
         BOOL fChildrenHidden = fForceHidden;
         if (fTreeMode && (!hItem->fExpanded || !hItem->fVisible))
            fChildrenHidden = TRUE;

         FastList_RepairVisibilityFlags (pfl, hItem, fChildrenHidden);
         }
      }
}


void FastList_RepairVisibilityIndices (LPFASTLIST pfl, HLISTITEM hParent)
{
   BOOL fTreeMode = (pfl->dwStyle & FLS_VIEW_TREE) ? TRUE : FALSE;

   if (hParent == NULL)  // Starting a new repair?  Initialize pfl->cVisible.
      {
      pfl->cVisible = 0;
      pfl->fSyncIndicesBeforePaint = FALSE;
      }

   // This routine fixes the {index} settings for all items, by walking
   // the list/tree in (presumably already-sorted) order and making
   // sure all items with {fVisible} set have smoothly increasing indices.
   // It rebuilds the Visible heap as it goes along.
   //
   for (HLISTITEM hItem = (!fTreeMode) ? (pfl->hListFirst) : (hParent) ? (hParent->hTreeChild) : (pfl->hTreeFirst);
        hItem != NULL;
        hItem = (!fTreeMode) ? (hItem->hListNext) : (hItem->hTreeNext))
      {
      if (hItem->fVisible)
         {
         hItem->index = pfl->cVisible;

         // Update the aVisibleHeap to match the new item.
         //
         REALLOC (pfl->aVisibleHeap, pfl->cVisibleHeap, 1+ hItem->index, cREALLOC_VISIBLEHEAP(pfl));
         pfl->aVisibleHeap[ hItem->index ] = hItem;
         pfl->cVisible ++;
         }

      if (fTreeMode && hItem->hTreeChild)
         {
         FastList_RepairVisibilityIndices (pfl, hItem);
         }
      }
}


void FastList_ScrollHeader (LPFASTLIST pfl)
{
   if (pfl->hHeader)
      {
      RECT rHeader;
      GetWindowRect (pfl->hHeader, &rHeader);

      LONG cx = rHeader.right + pfl->dxPixel;

      SetWindowPos (pfl->hHeader, 0,
                    0 - pfl->dxPixel, 0, cx, cyRECT(rHeader),
                    SWP_NOZORDER | SWP_NOACTIVATE);
      }
}


void FastList_PerformSelectTest (LPFASTLIST pfl, HLISTITEM hItem)
{
   if (pfl->hSelectFirst && !hItem->fSelected)
      {
      BOOL fMultiple = (pfl->dwStyle & FLS_SELECTION_MULTIPLE);
      BOOL fTreeMode = (pfl->dwStyle & FLS_VIEW_TREE);
      BOOL fLevel = (pfl->dwStyle & (FLS_SELECTION_LEVEL & ~FLS_SELECTION_MULTIPLE));
      BOOL fSibling = (pfl->dwStyle & (FLS_SELECTION_SIBLING & ~FLS_SELECTION_MULTIPLE));

      BOOL fClearSelection = FALSE;

      if (!fMultiple)
         {
         fClearSelection = TRUE;
         }
      else if (fTreeMode)
         {
         if (fLevel)
            {
            size_t iLevelItem = 0;
            HLISTITEM hParent;
            for (hParent = hItem->hTreeParent; hParent; hParent = hParent->hTreeParent)
               ++iLevelItem;

            size_t iLevelSelected = 0;
            for (hParent = pfl->hSelectFirst->hTreeParent; hParent; hParent = hParent->hTreeParent)
               ++iLevelSelected;

            if (iLevelItem != iLevelSelected)
               fClearSelection = TRUE;
            }

         if (fSibling)
            {
            BOOL fFound = FALSE;
            HLISTITEM hSearch;
            for (hSearch = hItem->hTreePrevious; !fFound && hSearch; hSearch = hSearch->hTreePrevious)
               {
               if (hSearch == pfl->hSelectFirst)
                  fFound = TRUE;
               }
            for (hSearch = hItem->hTreeNext; !fFound && hSearch; hSearch = hSearch->hTreeNext)
               {
               if (hSearch == pfl->hSelectFirst)
                  fFound = TRUE;
               }
            if (!fFound)
               fClearSelection = TRUE;
            }
         }

      if (fClearSelection)
         {
         FastList_OnCommand_SelectItem (pfl->hList, NULL, FALSE);
         }
      }
}


void FastList_PerformSelectItem (LPFASTLIST pfl, HLISTITEM hItem, BOOL fSelect)
{
   if (hItem->fSelected != fSelect)
      {
      if ((hItem->fSelected = fSelect) == FALSE)
         {
         if (hItem->hSelectPrevious)
            hItem->hSelectPrevious->hSelectNext = hItem->hSelectNext;
         if (hItem->hSelectNext)
            hItem->hSelectNext->hSelectPrevious = hItem->hSelectPrevious;
         if (pfl->hSelectFirst == hItem)
            pfl->hSelectFirst = hItem->hSelectNext;
         if (pfl->hAnchor == hItem)
            pfl->hAnchor = NULL;
         hItem->hSelectPrevious = NULL;
         hItem->hSelectNext = NULL;
         }
      else // (hItem->fSelected == TRUE)
         {
         if ((hItem->hSelectNext = pfl->hSelectFirst) != NULL)
            hItem->hSelectNext->hSelectPrevious = hItem;
         pfl->hSelectFirst = hItem;
         if (hItem->hSelectNext == NULL)
            pfl->hAnchor = hItem;
         }

      FastList_Notify_ItemChanged (pfl, hItem);
      FastList_Repaint (pfl);
      }
}


void FastList_PerformSelectRange (LPFASTLIST pfl, HLISTITEM hItem1, HLISTITEM hItem2)
{
   if (hItem1->fVisible && hItem2->fVisible)
      {
      FastList_Begin (pfl->hList);

      int iIndex1 = min (hItem1->index, hItem2->index);
      int iIndex2 = max (hItem1->index, hItem2->index);
      for (int iIndex = iIndex1; iIndex <= iIndex2; ++iIndex)
         FastList_SelectItem (pfl->hList, pfl->aVisibleHeap[ iIndex ], TRUE);

      FastList_Repaint (pfl);
      FastList_End (pfl->hList);
      }
}


void FastList_PerformEnsureVisible (LPFASTLIST pfl)
{
   if (pfl->hEnsureVisible && pfl->lItems->fIsInList (pfl->hEnsureVisible))
      {
      RECT rClient;
      GetClientRect (pfl->hList, &rClient);
      if (pfl->hScrollV)
         rClient.right -= GetSystemMetrics(SM_CXVSCROLL);
      if (pfl->hScrollH)
         rClient.bottom -= GetSystemMetrics(SM_CYHSCROLL);
      if (pfl->hHeader)
         rClient.top += FastList_GetHeaderHeight (pfl);

      RECT rItem;
      FastList_CalcItemRect (pfl, pfl->hEnsureVisible->index, &rItem, !!pfl->hScrollH, !!pfl->hScrollV);

      if (rItem.right > (cxRECT(rClient) + pfl->dxPixel))
         pfl->dxPixel = rItem.right - cxRECT(rClient);
      if (rItem.left < pfl->dxPixel)
         pfl->dxPixel = rItem.left;
      if (rItem.bottom > (pfl->dyPixel + rClient.bottom))
         pfl->dyPixel = rItem.bottom - rClient.bottom;
      if (rItem.top < (pfl->dyPixel + rClient.top))
         pfl->dyPixel = rItem.top - rClient.top;

      pfl->hEnsureVisible = NULL;
      FastList_SyncScrollPos (pfl);
      }
}


void FastList_ButtonDown (HWND hList, BOOL fRightButton, BOOL fActivate)
{
   if (GetParent (hList))
      PostMessage (GetParent(hList), WM_NEXTDLGCTL, (WPARAM)hList, TRUE);
   else
      SetFocus (hList);

   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {

      // The rules for selection are somewhat complex; they follow the
      // conventions used by the Windows shell:
      //
      // * if the user button-downs on the open/close box of a treeview item,
      //   expand or collapse that item, and perform no further processing.
      //
      // * if the user button-downs on an item,
      //   and if that item is not selected, then:
      //   - if the Control is not down, deselect all items
      //   - if the Shift key is down, select all items between the Anchor
      //     and the clicked-on item
      //   - if the Shift key is not down, select (and anchor on) the item
      //
      // * if the user button-downs and subsequently moves the mouse enough to
      //   indicate a drag operation prior to the next button-up,
      //   notify the parent window.
      //
      // * if the user double-clicked, treat it as a button-down followed
      //   immediately thereafter by a button-up
      //
      // * if the user button-ups without having triggered a drag operation,
      //   - if the user had button-downed on an already-selected item,
      //     de-select that item
      //
      // * if the user button-ups after having triggered a drag operation,
      //   notify the parent window.
      //
      // Here, the user has just button-downed, either as part of a single-click
      // or a double-click. We therefore will attempt the initial selection
      // process, store the screen coordinates at which the user clicked, and
      // set capture onto this list window that we may look for drag operations.

      // First find out where the user button-downed.
      //
      DWORD dwPos = GetMessagePos();

      POINT ptScreen;
      ptScreen.x = LOWORD(dwPos);
      ptScreen.y = HIWORD(dwPos);

      POINT ptClient = ptScreen;
      ScreenToClient (hList, &ptClient);

      // Determine what item is underneath the given point; also determine
      // the region of the item which was clicked.
      //
      HLISTITEM hStrict = NULL;
      HLISTITEM hNonStrict = NULL;
      FASTLISTITEMREGIONS regNonStrict;
      if ((hStrict = FastList_ItemFromPoint (hList, &ptClient, TRUE)) == NULL)
         {
         if ((hNonStrict = FastList_ItemFromPoint (hList, &ptClient, FALSE)) != NULL)
            FastList_GetItemRegions (hList, hNonStrict, &regNonStrict);
         }

      // If the user has clicked on the open/close button of an item, expand
      // or collapse that item.
      //
      FastList_Begin (hList);

      if ( (!hStrict) && (hNonStrict) && (PtInRect (&regNonStrict.rButton, ptClient)) )
         {
         FastList_OnCommand_Expand (hList, hNonStrict, !hNonStrict->fExpanded);
         FastList_Notify_ItemExpand (pfl, hNonStrict);
         FastList_End (hList);
         return;
         }

      // If the user failed to click on any item at all, and if the control
      // key is not down, de-select all items. Regardless, return thereafter.
      //
      if (!hStrict)
         {
         if (!fIsControlDown())
            FastList_SelectNone (hList);
         FastList_SetFocus (hList,NULL);
         FastList_End (hList);
         FastList_Notify_ItemSelect (pfl);
         return;
         }

      // If the user has double-clicked on an item, and that item is a parent
      // of other item, and if the tree is showing, expand or collapse that item.
      //
      if (fActivate && hStrict->hTreeChild && (pfl->dwStyle & FLS_VIEW_TREE))
         {
         FastList_OnCommand_Expand (hList, hStrict, !hStrict->fExpanded);
         FastList_End (hList);
         return;
         }

      // The user has clicked on an item.  Record what we know about what
      // the user has done, and apply message capturing to the list.
      //
      pfl->fButtonDown = TRUE;
      pfl->fRightDrag = fRightButton;
      pfl->fDragging = FALSE;
      pfl->fTriedToDrag = FALSE;
      pfl->ptScreenDown = ptScreen;
      pfl->hHitOnDown = hStrict;
      pfl->fSelectedOnDown = hStrict->fSelected;
      SetCapture (hList);

      // There are eight states about which we need to concern ourselves:
      //
      // Control:  Down (C) or Not Down (c)
      // Shift:    Down (S) or Not Down (s)
      // Item:     Selected (I) or Not Selected (i)
      //
      // csi: de-select all items, anchor and select the specified item
      // csI: de-select all items, anchor and select the specified item
      // cSi: de-select all items, select from the anchor to the specified item
      // cSI: de-select all items, select from the anchor to the specified item
      // Csi: anchor and select the specified item
      // CsI: anchor and select the specified item
      // CSi: select from the anchor to the specified item
      // CSI: select from the anchor to the specified item
      //
      // From this we can deduce the following rules:
      //
      // if Control is not down, begin by de-selecting all items
      // if Shift is not down, anchor and select the specified item
      // if Shift is down, select from the anchor to the specified item
      //
      HLISTITEM hAnchor = pfl->hAnchor;
      if (!fIsControlDown())
         {
         FastList_SelectNone (hList);
         }
      if (!fIsShiftDown() || !hAnchor)
         {
         FastList_SelectItem (hList, hStrict, TRUE);
         pfl->hAnchor = hStrict;
         }
      if (fIsShiftDown() && hAnchor)
         {
         FastList_PerformSelectRange (pfl, hAnchor, hStrict);
         if (hAnchor->fSelected)
            pfl->hAnchor = hAnchor;
         }

      // Finally, place the focus on the clicked-on item, and return.
      // We will complete the selection/dragging operation on mouseup.
      //
      FastList_SetFocus (hList, hStrict);
      FastList_End (hList);
      FastList_Notify_ItemSelect (pfl);
      }
}


void FastList_MouseMove (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      // If there is no possibility that the user may be dragging something,
      // ignore the message.
      //
      if (!pfl->fButtonDown)
         return;

      // If the user has triggered a drag operation, notify the parent.
      //
      if ((!pfl->fTriedToDrag) || (pfl->fDragging))
         {
         DWORD dwCode = (pfl->fDragging) ? FLN_DRAG : FLN_BEGINDRAG;
         DWORD dwPos = GetMessagePos();

         POINT ptScreenTo;
         ptScreenTo.x = LOWORD(dwPos);
         ptScreenTo.y = HIWORD(dwPos);

         if (!pfl->fTriedToDrag)
            {
            if ( (abs(ptScreenTo.x - pfl->ptScreenDown.x) >= GetSystemMetrics(SM_CXDRAG)) ||
                 (abs(ptScreenTo.y - pfl->ptScreenDown.y) >= GetSystemMetrics(SM_CYDRAG)) )
               {
               pfl->fTriedToDrag = TRUE;
               }
            }
         if (pfl->fTriedToDrag)
            {
            FLN_DRAG_PARAMS fln;
            fln.hFirst = pfl->hSelectFirst;
            fln.ptScreenFrom = pfl->ptScreenDown;
            fln.ptScreenTo = ptScreenTo;
            fln.fRightButton = pfl->fRightDrag;
            fln.fShift = fIsShiftDown();
            fln.fControl = fIsControlDown();
            if ( (FastList_Notify_Drag (pfl, dwCode, &fln)) && (dwCode == FLN_BEGINDRAG) )
               pfl->fDragging = TRUE;
            }
         }
      }
}


void FastList_ButtonUp (HWND hList, BOOL fRightButton, BOOL fActivate)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      // First, our caller may have just notified us that a double-click
      // took place; if so, we may have already handled it (in the ButtonDown)
      // routine. Or, moreover, if we never got a button-down at all,
      // there's nothing for us to do here.
      //
      if (!pfl->fButtonDown)
         return;
      if (pfl->fRightDrag != fRightButton)
         return;

      // Release capture; we no longer need it.
      //
      pfl->fButtonDown = FALSE;
      ReleaseCapture();

      // If the user right-button-ups without having triggered a drag operation,
      // send a WM_CONTEXTMENU message.
      //
      if ( (!pfl->fTriedToDrag) && (fRightButton) )
         {
         if (GetParent (hList))
            {
            PostMessage (GetParent (hList), WM_CONTEXTMENU, (WPARAM)hList, (LPARAM)GetMessagePos());
            }
         }

      // If the user button-ups without having triggered a drag operation,
      // and if the user had button-downed on an already-selected item,
      // de-select that item.
      //
      if ( (!pfl->fTriedToDrag) && (pfl->hHitOnDown) && (pfl->fSelectedOnDown) && (fIsControlDown()) )
         {
         FastList_SelectItem (hList, pfl->hHitOnDown, FALSE);
         FastList_Notify_ItemSelect (pfl);
         return;
         }

      // If the user button-ups after triggering a drag operation, notify
      // our parent.
      //
      if (pfl->fDragging)
         {
         DWORD dwPos = GetMessagePos();

         POINT ptScreenTo;
         ptScreenTo.x = LOWORD(dwPos);
         ptScreenTo.y = HIWORD(dwPos);

         FLN_DRAG_PARAMS fln;
         fln.hFirst = pfl->hSelectFirst;
         fln.ptScreenFrom = pfl->ptScreenDown;
         fln.ptScreenTo = ptScreenTo;
         fln.fRightButton = pfl->fRightDrag;
         fln.fShift = fIsShiftDown();
         fln.fControl = fIsControlDown();
         FastList_Notify_Drag (pfl, FLN_ENDDRAG, &fln);
         }
      }
}


/*
 * UTILITY FUNCTIONS __________________________________________________________
 *
 */

LPFASTLIST GetFastList (HWND hList)
{
   LPFASTLIST pfl;
   try {
      if (!GetWindowLong (hList, 0))
	  pfl = NULL; else
      if ((pfl = (LPFASTLIST)GetWindowLong (hList, 0))->dwSig != dwSigFASTLIST)
         pfl = NULL;
      else if (pfl->hList != hList)
         pfl = NULL;
   } catch(...) {
      pfl = NULL;
   }
   return pfl;
}


BOOL fIsFastList (HWND hList)
{
   return (GetFastList (hList)) ? TRUE : FALSE;
}


void FastList_Enter (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      EnterCriticalSection (&pfl->cs);
      }
}


void FastList_Leave (HWND hList)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) != NULL)
      {
      LeaveCriticalSection (&pfl->cs);
      }
}


int CALLBACK FastList_SortFunc_AlphaNumeric (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) == NULL)
      return 0;

   if (!hItem1 || !hItem2)
      return 0;

   LPTSTR pszText1 = FastList_GetColumnText (pfl, hItem1, pfl->iColSort, FALSE);
   LPTSTR pszText2 = FastList_GetColumnText (pfl, hItem2, pfl->iColSort, TRUE);

   if ( (pfl->iColSort < (int)pfl->cColumns) &&
        (pfl->aColumns[ pfl->iColSort ].pszText != NULL) &&
        ((pfl->aColumns[ pfl->iColSort ].fmt & HDF_JUSTIFYMASK) == HDF_RIGHT) )
      {
      double dItem1 = atof (pszText1);
      double dItem2 = atof (pszText2);

      if (pfl->fRevSort)
         return (dItem1 < dItem2) ? 1 : (dItem1 > dItem2) ? -1 : 0;
      else
         return (dItem1 < dItem2) ? -1 : (dItem1 > dItem2) ? 1 : 0;
      }
   else // left- or center- justified; use an alphabetic sort
      {
      if (pfl->fRevSort)
         return lstrcmpi (pszText2, pszText1);
      else
         return lstrcmpi (pszText1, pszText2);
      }
}


int CALLBACK FastList_SortFunc_Alphabetic (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) == NULL)
      return 0;

   if (!hItem1 || !hItem2)
      return 0;

   LPTSTR pszText1 = FastList_GetColumnText (pfl, hItem1, pfl->iColSort, FALSE);
   LPTSTR pszText2 = FastList_GetColumnText (pfl, hItem2, pfl->iColSort, TRUE);

   if (pfl->fRevSort)
      return lstrcmpi (pszText2, pszText1);
   else
      return lstrcmpi (pszText1, pszText2);
}


int CALLBACK FastList_SortFunc_Numeric (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2)
{
   LPFASTLIST pfl;
   if ((pfl = GetFastList (hList)) == NULL)
      return 0;

   if (!hItem1 || !hItem2)
      return 0;

   LPTSTR pszText1 = FastList_GetColumnText (pfl, hItem1, pfl->iColSort, FALSE);
   LPTSTR pszText2 = FastList_GetColumnText (pfl, hItem2, pfl->iColSort, TRUE);

   double dItem1 = atof (pszText1);
   double dItem2 = atof (pszText2);

   if (pfl->fRevSort)
      return (dItem1 < dItem2) ? 1 : (dItem1 > dItem2) ? -1 : 0;
   else
      return (dItem1 < dItem2) ? -1 : (dItem1 > dItem2) ? 1 : 0;
}


/*
 * HASHLIST KEYS ______________________________________________________________
 *
 */

BOOL CALLBACK FastList_KeyUserParam_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return (((HLISTITEM)pObject)->lpUser == *(LPARAM*)pData) ? TRUE : FALSE;
}

HASHVALUE CALLBACK FastList_KeyUserParam_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return FastList_KeyUserParam_HashData (pKey, &((HLISTITEM)pObject)->lpUser);
}

HASHVALUE CALLBACK FastList_KeyUserParam_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return (HASHVALUE)*(LPARAM*)pData;
}

