/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef DIALOG_H
#define DIALOG_H

#include <commctrl.h>
#include <commdlg.h>
#include <WINNT/fastlist.h>

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef THIS_HINST
#define THIS_HINST  (HINSTANCE)GetModuleHandle(NULL)
#endif

#ifndef EXPORTED
#define EXPORTED
#endif

#ifndef IDINIT
#define IDINIT  20
#endif
#ifndef IDAPPLY
#define IDAPPLY 21
#endif
#ifndef IDHELP
#define IDHELP   9
#endif

#ifndef limit
#define limit(_a,_x,_b)  min( max( (_x), (_a) ), (_b) )
#endif

#ifndef inlimit
#define inlimit(_a,_x,_b)  ( (((_x)>=(_a)) && ((_x)<=(_b))) ? TRUE : FALSE )
#endif

#ifndef Set2State
#define Set2State(_hDlg,_idc)  SendDlgItemMessage (_hDlg,_idc,BM_SETSTYLE,BS_AUTOCHECKBOX,MAKELPARAM(TRUE,0))
#endif

#ifndef Set3State
#define Set3State(_hDlg,_idc)  SendDlgItemMessage (_hDlg,_idc,BM_SETSTYLE,BS_AUTO3STATE,MAKELPARAM(TRUE,0))
#endif

#define CheckMenu(_hm,_id,_f)  CheckMenuItem(_hm,_id,MF_BYCOMMAND|((_f)?MF_CHECKED:MF_UNCHECKED))
#define EnableMenu(_hm,_id,_f) EnableMenuItem(_hm,_id,MF_BYCOMMAND|((_f)?MF_ENABLED:MF_GRAYED))

#ifndef cxRECT
#define cxRECT(_r)  ((_r).right - (_r).left)
#endif

#ifndef cyRECT
#define cyRECT(_r)  ((_r).bottom - (_r).top)
#endif

#define WM_INITDIALOG_SHEET             0x03FE
#define WM_DESTROY_SHEET                0x03FF

#ifndef IDC_PROPSHEET_TABCTRL
#define IDC_PROPSHEET_TABCTRL  0x3020
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 *** PROPERTY SHEETS
 *
 */

typedef struct
   {
   PROPSHEETHEADER sh;
   struct {
      DLGPROC dlgproc;
      LPARAM lpUser;
      HWND hDlg;
   } *aTabs;
   size_t cTabs;
   BOOL fMadeCaption;
   LPARAM lpUser;
   } PROPSHEET, *LPPROPSHEET;

EXPORTED LPPROPSHEET PropSheet_Create (int idsTitle, BOOL fContextHelp, HWND hParent = NULL, LPARAM lp = 0);
EXPORTED LPPROPSHEET PropSheet_Create (LPTSTR pszTitle, BOOL fContextHelp, HWND hParent = NULL, LPARAM lp = 0);
EXPORTED BOOL PropSheet_AddTab (LPPROPSHEET psh, int idsTitle, int idd, DLGPROC dlgproc, LPARAM lpUser, BOOL fHelpButton, BOOL fStartPage = FALSE);
EXPORTED BOOL PropSheet_AddTab (LPPROPSHEET psh, LPTSTR pszTitle, int idd, DLGPROC dlgproc, LPARAM lpUser, BOOL fHelpButton, BOOL fStartPage = FALSE);
EXPORTED BOOL PropSheet_ShowModal (LPPROPSHEET psh, void (*fnPump)(MSG*lpm) = 0);
EXPORTED HWND PropSheet_ShowModeless (LPPROPSHEET psh, int nCmdShow = SW_SHOW);
EXPORTED LPARAM PropSheet_FindTabParam (HWND hTab);
EXPORTED void PropSheet_Free (LPPROPSHEET psh);
EXPORTED HWND PropSheet_FindTabWindow (LPPROPSHEET psh, DLGPROC dlgproc);

EXPORTED void PropSheetChanged (HWND hDlg);

EXPORTED void TabCtrl_SwitchToTab (HWND hTab, int iTab);


/*
 ** LIST, TREELIST, FASTLIST COLUMNS
 *
 */

#define nCOLUMNS_MAX    32  // maximum number of columns in a listview

typedef struct
   {
   LONG lvsView;
   size_t nColsAvail;
   size_t iSort;
   int cxColumns[ nCOLUMNS_MAX ];
   int idsColumns[ nCOLUMNS_MAX ];
   size_t nColsShown;
   size_t aColumns[ nCOLUMNS_MAX ];	// returns 0-based index into cxColumns
   } VIEWINFO, *LPVIEWINFO;

#define COLUMN_JUSTMASK   0xF0000000    // cxColumns |=  to make right-just col
#define COLUMN_RIGHTJUST  0x80000000    // cxColumns |=  to make right-just col
#define COLUMN_CENTERJUST 0x40000000    // cxColumns |=  to make center-just col

#define COLUMN_SORTREV    0x80000000    // iSort |=  to sort descending

/*
 ** FASTLISTS
 *
 */

EXPORTED LPARAM FL_StartChange (HWND hList, BOOL fReset = TRUE);
EXPORTED void FL_EndChange (HWND hList, LPARAM lpToSelect = 0);

EXPORTED void FL_GetColumns (HWND hList, WORD nCols, WORD *acx);
EXPORTED void FL_ResizeColumns (HWND hList, WORD nCols, WORD *acx);
EXPORTED void cdecl FL_SetColumns (HWND hList, WORD nCols, WORD *acx, ...);

EXPORTED HLISTITEM cdecl FL_AddItem (HWND hList, LPVIEWINFO lpvi, LPARAM lp, int iImage1, ...);
EXPORTED HLISTITEM cdecl FL_AddItem (HWND hList, WORD nCols, LPARAM lp, int iImage1, ...);
EXPORTED void FL_GetItemText (HWND hList, HLISTITEM hItem, int iCol, LPTSTR pszText);
EXPORTED void FL_SetItemText (HWND hList, HLISTITEM hItem, int iCol, LPCTSTR pszText);

EXPORTED HLISTITEM FL_GetSelected (HWND hList, HLISTITEM hItemSearch = NULL);
EXPORTED void FL_SetSelected (HWND hList, HLISTITEM hItem);

EXPORTED LPARAM FL_GetSelectedData (HWND hList);
EXPORTED void FL_SetSelectedByData (HWND hList, LPARAM lp);
EXPORTED HLISTITEM FL_GetFocused (HWND hList);

EXPORTED LPARAM FL_GetData (HWND hList, HLISTITEM hItem);
EXPORTED LPARAM FL_GetFocusedData (HWND hList);
EXPORTED HLISTITEM FL_GetIndex (HWND hList, LPARAM lp);

EXPORTED BOOL FL_HitTestForHeaderBar (HWND hList, POINT ptClient);

EXPORTED void FL_RestoreView (HWND hList, LPVIEWINFO lpvi);
EXPORTED void FL_StoreView (HWND hList, LPVIEWINFO lpvi);
EXPORTED void FL_SortView (HWND hList, LPVIEWINFO lpvi);

/*
 ** LIST VIEWS
 *
 */

#define INDEX_SORT   (int)-1

EXPORTED LPARAM LV_StartChange (HWND hList, BOOL fReset = TRUE);
EXPORTED void LV_EndChange (HWND hList, LPARAM lpToSelect = (LPARAM)NULL);

EXPORTED void LV_GetColumns (HWND hList, WORD nCols, WORD *acx);
EXPORTED void LV_ResizeColumns (HWND hList, WORD nCols, WORD *acx);
EXPORTED void cdecl LV_SetColumns (HWND hList, WORD nCols, WORD *acx, ...);

EXPORTED void cdecl LV_AddItem (HWND hList, LPVIEWINFO lpvi, int idx, LPARAM lp, int iImage, ...);
EXPORTED void cdecl LV_AddItem (HWND hList, WORD nCols, int idx, LPARAM lp, int iImage, ...);
EXPORTED void cdecl LV_GetItemText (HWND hList, int inx, short col, LPTSTR pszText);
EXPORTED void cdecl LV_SetItemText (HWND hList, int idx, short col, LPCTSTR pszText);

EXPORTED int LV_GetSelected (HWND hList, int idx = -1);
EXPORTED void LV_SetSelected (HWND hList, int idx);

EXPORTED LPARAM LV_GetSelectedData (HWND hList);
EXPORTED void LV_SetSelectedByData (HWND hList, LPARAM lp);
EXPORTED int LV_GetFocused (HWND hList);

EXPORTED LPARAM LV_GetData (HWND hList, int idx);
EXPORTED LPARAM LV_GetFocusedData (HWND hList);
EXPORTED int LV_GetIndex (HWND hList, LPARAM lp);

EXPORTED BOOL LV_HitTestForHeaderBar (HWND hList, POINT ptClient);

EXPORTED void LV_RestoreView (HWND hList, LPVIEWINFO lpvi);
EXPORTED void LV_StoreView (HWND hList, LPVIEWINFO lpvi);
EXPORTED void LV_SortView (HWND hList, LPVIEWINFO lpvi);
EXPORTED int LV_PickInsertionPoint (HWND hList, LPTSTR pszNewItem, int (__stdcall *fnSort)(LPCTSTR, LPCTSTR) = 0);


/*
 *** COMBO BOXES
 *
 */

EXPORTED LPARAM CB_StartChange (HWND hCombo, BOOL fReset = TRUE);
EXPORTED void CB_EndChange (HWND hCombo, LPARAM lpToSelect = (LPARAM)NULL);

EXPORTED UINT CB_AddItem (HWND hCombo, int nsz, LPARAM lp);
EXPORTED UINT CB_AddItem (HWND hCombo, LPCTSTR psz, LPARAM lp);

EXPORTED UINT CB_GetSelected (HWND hCombo);
EXPORTED void CB_SetSelected (HWND hCombo, UINT index);

EXPORTED LPARAM CB_GetSelectedData (HWND hCombo);
EXPORTED void CB_SetSelectedByData (HWND hCombo, LPARAM lp);

EXPORTED LPARAM CB_GetData (HWND hCombo, UINT index);
EXPORTED UINT CB_GetIndex (HWND hCombo, LPARAM lp);


/*
 *** LIST BOXES
 *
 */

EXPORTED LPARAM LB_StartChange (HWND hList, BOOL fReset = TRUE);
EXPORTED void LB_EndChange (HWND hList, LPARAM lpToSelect = (LPARAM)NULL);

EXPORTED UINT LB_AddItem (HWND hList, int nsz, LPARAM lp);
EXPORTED UINT LB_AddItem (HWND hList, LPCTSTR psz, LPARAM lp);

EXPORTED void LB_EnsureVisible (HWND hList, UINT index);

EXPORTED UINT LB_GetSelected (HWND hList);
EXPORTED void LB_SetSelected (HWND hList, UINT index);

EXPORTED LPARAM LB_GetSelectedData (HWND hList);
EXPORTED void LB_SetSelectedByData (HWND hList, LPARAM lp);

EXPORTED LPARAM LB_GetData (HWND hList, UINT index);
EXPORTED UINT LB_GetIndex (HWND hList, LPARAM lp);

EXPORTED void LB_EnableHScroll (HWND hList);

/*
 *** TREEVIEWS
 *
 */

EXPORTED LPARAM TV_StartChange (HWND hTree, BOOL fReset);
EXPORTED void TV_EndChange (HWND hTree, LPARAM lpToSelect = (LPARAM)NULL);

EXPORTED LPARAM TV_GetData (HWND hTree, HTREEITEM hti);
EXPORTED LPARAM TV_GetSelectedData (HWND hTree);

EXPORTED HTREEITEM TV_RecursiveFind (HWND hTree, HTREEITEM htiRoot, LPARAM lpToFind);


/*
 *** COMMDLG      
 *
 */

EXPORTED BOOL Browse_Open (HWND hParent, LPTSTR pszFilename, LPTSTR pszLastDirectory, int idsFilter, int iddTemplate = 0, LPOFNHOOKPROC lpfnHook = NULL, DWORD lCustData = 0);
EXPORTED BOOL Browse_Save (HWND hParent, LPTSTR pszFilename, LPTSTR pszLastDirectory, int idsFilter, int iddTemplate = 0, LPOFNHOOKPROC lpfnHook = NULL, DWORD lCustData = 0);


/*
 *** MISCELLANEOUS
 *
 */

EXPORTED void StartHourGlass (void);
EXPORTED void StopHourGlass (void);

EXPORTED void DisplayContextMenu (HMENU hm, POINT ptScreen, HWND hParent);

EXPORTED size_t CountChildren (HWND hParent, LPTSTR pszClass);

EXPORTED WORD NextControlID (HWND hParent);

EXPORTED BOOL IsAncestor (HWND hParent, HWND hChild);

EXPORTED HWND GetTabChild (HWND hTab);

EXPORTED HWND GetLastDlgTabItem (HWND hDlg);

EXPORTED BOOL IsPropSheet (HWND hSheet);


#endif

