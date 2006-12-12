/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef FASTLIST_H
#define FASTLIST_H

#include <commctrl.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef EXPORTED
#define EXPORTED
#endif

#ifndef THIS_HINST
#define THIS_HINST   (HINSTANCE)GetModuleHandle(NULL)
#endif

#define WC_FASTLIST   TEXT("OpenAFS_FastList")

#define FLM_FIRST     0x1400

#define FLN_FIRST    (0U-880U)
#define FLN_LAST     (0U-885U)

#define IMAGE_NOIMAGE      ((int)(-1))
#define IMAGE_BLANKIMAGE   ((int)(-2))


/*
 * WINDOW STYLES ______________________________________________________________
 *
 */

        // The FLS_VIEW_* styles are used to indicate the format for
        // displaying items in the FastList; either by Large Icons,
        // Small Icons (equivalent to a ListView's "List" layout),
        // in a List (equivalent to ListView's "Report" layout),
        // in a Tree (equivalent to a TreeView), or as a tree/list hybrid--
        // showing the Tree hierarchy as well as a header bar and
        // details about each item.
        //
#define FLS_VIEW_LARGE      0x0001
#define FLS_VIEW_SMALL      0x0002
#define FLS_VIEW_LIST       0x0003
#define FLS_VIEW_TREE       0x0004
#define FLS_VIEW_TREELIST   (FLS_VIEW_TREE | FLS_VIEW_LIST)
#define FLS_VIEW_MASK       (FLS_VIEW_LARGE | FLS_VIEW_SMALL | FLS_VIEW_TREE | FLS_VIEW_LIST)

        // The FLS_NOSORTHEADER style causes the header bar (when visible)
        // to be drawn "flat"--the buttons cannot be depressed. This style
        // cannot be combined with FLS_AUTOSORTHEADER.
        //
#define FLS_NOSORTHEADER    0x0008

        // The FLS_SELECTION_MULTIPLE style indicates that more than one
        // item in the list may be selected at any time. When combined with
        // the FLS_SELECTION_SIBLING style, only direct siblings in the tree
        // hierarchy may be selected at any time. When instead combined with
        // the FLS_SELECTION_LEVEL style, only items at the same level from
        // the root may be selected at any time.  If these bits are not set,
        // only one item may be selected at any time.
        //
#define FLS_SELECTION_MULTIPLE  0x0010
#define FLS_SELECTION_SIBLING  (0x0020 | FLS_SELECTION_MULTIPLE)
#define FLS_SELECTION_LEVEL    (0x0040 | FLS_SELECTION_MULTIPLE)

        // The FLS_LINESATROOT style causes root-level items in the tree
        // to be prefixed by open/close boxes.
        //
#define FLS_LINESATROOT     0x0080

        // The FLS_LONGCOLUMNS style allows the right-most column containing
        // text, if left-justified, to be drawn to the extent of the window
        // regardless of column boundaries. It is particularly useful in
        // Tree/List mode, where some entries may have long descriptions
        // for the left-most column, and others may have several columns of
        // data.
        //
#define FLS_LONGCOLUMNS     0x0100

        // The FLS_HIT_TEXTONLY style allows the user to de-select all
        // items by left- or right-clicking on the whitespace between
        // columns of data in List mode. (It has no effect in other modes.)
        // If this bit is not set, clicking on this whitespace causes
        // that item to be selected.
        //
#define FLS_HIT_TEXTONLY    0x0200

        // The FLS_AUTOSORTHEADER style allows the user to sort on any
        // column by clicking on the column, or to reverse the sort on
        // a column by clicking it twice. This style cannot be combined
        // with the FLS_NOSORTHEADER style.
        //
#define FLS_AUTOSORTHEADER  0x0400

/*
 * STRUCTURES _________________________________________________________________
 *
 */

typedef struct _FASTLISTITEM *HLISTITEM;

typedef struct FASTLISTADDITEM
   {
   HLISTITEM hParent;	// parent item or NULL
   int iFirstImage;	// primary icon or imageNO_IMAGE
   int iSecondImage;	// secondary icon or imageNO_IMAGE
   LPTSTR pszText;	// text for column 0
   LPARAM lParam;	// user-supplied cookie
   DWORD dwFlags;	// combination of FLIF_* flags
   } FASTLISTADDITEM, *LPFASTLISTADDITEM;

#define FLIF_TREEVIEW_ONLY      0x0001	// hide item unless in tree view
#define FLIF_DROPHIGHLIGHT      0x0002	// draw item as if it were selected
#define FLIF_DISALLOW_COLLAPSE  0x0004	// don't allow this item to collapse
#define FLIF_DISALLOW_SELECT    0x0008	// don't allow this item to be selected

typedef struct FASTLISTITEMREGIONS
   {
   RECT rItem;	// OUT: overall rectangle for item
   RECT rImage;	// OUT: rect of item's images
   RECT rLabel;	// OUT: rect of item's label
   RECT rHighlight;	// OUT: rect that inverts on selection
   RECT rButton;	// OUT: rect of tree open/close button
   RECT rSelect;	// OUT: union of rHighlight+rImage
   } FASTLISTITEMREGIONS, *LPFASTLISTITEMREGIONS;

typedef struct FASTLISTDRAWITEM
   {
   HDC hdc;	// IN: target or template HDC
   BOOL fDraw;	// IN: TRUE if should render to hdc
   BOOL fDragImage;	// IN: TRUE if want drag image only
   HWND hWnd;	// IN: handle of fastlist window
   HLISTITEM hItem;	// IN: item to be drawn
   LPARAM lParam;	// IN: user-supplied lparam for item
   RECT rItem;	// IN: overall rectangle for item
   POINT ptTextTest;	// IN: client coords of point to test
   BOOL fTextTestHit;	// OUT: TRUE if point is within text
   FASTLISTITEMREGIONS reg;	// OUT: information about item image
   } FASTLISTDRAWITEM, *LPFASTLISTDRAWITEM;

typedef struct FLN_GETITEMTEXT_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   struct
      {
      HLISTITEM hItem;	// IN: item for which to obtain text
      int icol;	// IN: 0-based column of text to get
      LPARAM lParam;	// IN: user-supplied lparam for item
      LPTSTR pszText;	// IN: buffer in which to place text
      size_t cchTextMax;	// IN: size of buffer; OUT: required
      } item;
   } FLN_GETITEMTEXT_PARAMS, *LPFLN_GETITEMTEXT_PARAMS;

typedef struct FLN_ITEMCHANGED_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   HLISTITEM hItem;	// IN: item whose data has changed
   } FLN_ITEMCHANGED_PARAMS, *LPFLN_ITEMCHANGED_PARAMS;

typedef struct FLN_ADDITEM_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   HLISTITEM hItem;	// IN: item which was added
   } FLN_ADDITEM_PARAMS, *LPFLN_ADDITEM_PARAMS;

typedef struct FLN_REMOVEITEM_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   HLISTITEM hItem;	// IN: item which was removed
   } FLN_REMOVEITEM_PARAMS, *LPFLN_REMOVEITEM_PARAMS;

typedef struct FLN_COLUMNCLICK_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   int icol;	// IN: 0-based column that was clicked
   BOOL fDouble;	// IN: TRUE if 'twas a double-click
   } FLN_COLUMNCLICK_PARAMS, *LPFLN_COLUMNCLICK_PARAMS;

typedef struct FLN_COLUMNRESIZE_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   int icol;	// IN: 0-based column that was resized
   LONG cxWidth;	// IN: new width of the column
   } FLN_COLUMNRESIZE_PARAMS, *LPFLN_COLUMNRESIZE_PARAMS;

typedef struct FLN_ITEMSELECT_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   HLISTITEM hItem;	// IN: first item which is now selected
   } FLN_ITEMSELECT_PARAMS, *LPFLN_ITEMSELECT_PARAMS;

typedef struct FLN_ITEMEXPAND_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   HLISTITEM hItem;	// IN: item which was expanded/collapsed
   BOOL fExpanded;	// IN: TRUE if item is now expanded
   } FLN_ITEMEXPAND_PARAMS, *LPFLN_ITEMEXPAND_PARAMS;

typedef struct FLN_DRAG_PARAMS
   {
   NMHDR hdr;	// IN: typical notification header
   HLISTITEM hFirst;	// IN: first (of perhaps many) selected
   POINT ptScreenFrom;	// IN: screen coordinates of button-down
   POINT ptScreenTo;	// IN: final screen coordinates
   BOOL fRightButton;	// IN: TRUE if right-button drag
   BOOL fShift;	// IN: TRUE if shift is down
   BOOL fControl;	// IN: TRUE if control is down
   } FLN_DRAG_PARAMS, *LPFLN_DRAG_PARAMS;

typedef struct FASTLISTCOLUMN
   {
   DWORD dwFlags;
   LONG cxWidth;
   TCHAR szText[ 256 ];
   } FASTLISTCOLUMN, *LPFASTLISTCOLUMN;

#define FLCF_JUSTIFY_LEFT    0x0001
#define FLCF_JUSTIFY_RIGHT   0x0002
#define FLCF_JUSTIFY_CENTER  0x0003
#define FLCF_JUSTIFY_MASK    0x0003

typedef struct FASTLISTITEMCOLUMN
   {
   HLISTITEM hItem;
   int icol;
   } FASTLISTITEMCOLUMN, *LPFASTLISTITEMCOLUMN;

typedef struct FASTLISTITEMIMAGE
   {
   HLISTITEM hItem;
   int iImage;
   } FASTLISTITEMIMAGE, *LPFASTLISTITEMIMAGE;

typedef int (CALLBACK * LPFASTLISTSORTFUNC)( HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2 );

typedef BOOL (CALLBACK * LPFASTLISTTEXTCALLBACK)( HWND hList, LPFLN_GETITEMTEXT_PARAMS pfln, DWORD dwCookie );


/*
 * NOTIFICATIONS ______________________________________________________________
 *
 * All notifications come via a WM_NOTIFY message to the FastList window's
 * parent window. The message handler should be of the form:
 *
 * switch (msg) {
 *    case WM_NOTIFY: {
 *       switch (((LPNMHDR)lp)->code) {
 *          case FLN_GETITEMTEXT: {
 *             LPFLN_GETITEMTEXT_PARAMS pParams = (LPFLN_GETITEMTEXT_PARAMS)lp;
 *             lstrcpy (pParams->pszText, ...);
 *             return TRUE;
 *          }
 *          case FLN_ITEMEXPAND: {
 *             ...
 *             return TRUE;
 *          }
 *       }
 *    }
 * }
 *
 * The LPARAM associated with a WM_NOTIFY message is always (for notifications
 * from any window, not just FastLists) a structure consisting of:
 *    struct {
 *       NMHDR hdr;
 *       // window- and message-specific elements ...
 *    } 
 * Thus, by casing the LPARAM value to LPNMHDR, information about the
 * notification can be obtained from any notification structure.
 * If a notification message is handled, the handler must return TRUE.
 *
 */

#define FLN_GETITEMTEXT  (FLN_FIRST-0)	// lp = LPFLN_GETITEMTEXT_PARAMS
#define FLN_ITEMCHANGED  (FLN_FIRST-1)	// lp = LPFLN_ITEMCHANGED_PARAMS
#define FLN_ADDITEM      (FLN_FIRST-2)	// lp = LPFLN_ADDITEM_PARAMS
#define FLN_REMOVEITEM   (FLN_FIRST-3)	// lp = LPFLN_REMOVEITEM_PARAMS
#define FLN_COLUMNCLICK  (FLN_FIRST-4)	// lp = LPFLN_COLUMNCLICK_PARAMS
#define FLN_COLUMNRESIZE (FLN_FIRST-5)	// lp = LPFLN_COLUMNRESIZE_PARAMS
#define FLN_ITEMSELECT   (FLN_FIRST-6)	// lp = LPFLN_ITEMSELECT_PARAMS
#define FLN_ITEMEXPAND   (FLN_FIRST-7)	// lp = LPFLN_ITEMEXPAND_PARAMS
#define FLN_LCLICK       (FLN_FIRST-8)	// lp = LPNMHDR
#define FLN_RCLICK       (FLN_FIRST-9)	// lp = LPNMHDR
#define FLN_LDBLCLICK    (FLN_FIRST-10)	// lp = LPNMHDR
#define FLN_BEGINDRAG    (FLN_FIRST-11)	// lp = LPFLN_DRAG_PARAMS, rc = fDrag
#define FLN_DRAG         (FLN_FIRST-12)	// lp = LPFLN_DRAG_PARAMS
#define FLN_ENDDRAG      (FLN_FIRST-13)	// lp = LPFLN_DRAG_PARAMS


/*
 * MESSAGES ___________________________________________________________________
 *
 */

/*
 * TASK GROUPING
 *
 * Calling FastList_Begin() suspends sorting, repainting and other display-
 * oriented operations until a matching call to FastList_End(). By surrounding
 * multiple operations with an initial call to _Begin() and a final call to
 * _End(), the operations can proceed (potentially much) more quickly, and
 * without flicker as items are added, updated or removed. Calls to _Begin()
 * and _End() nest: if _Begin() is called twice, two calls to _End() are
 * required to finally cause the display to be repainted and resume normal
 * operation. (A call to FastList_EndAll() ignores all nesting and enables
 * redraw as if the proper number of calls to _End() had been made.)
 *
 */

        // void FastList_Begin (HWND hList);
        //
#define FastList_Begin(_hList) \
           (void)SendMessage (_hList, FLM_BEGIN, 0, 0)
#define FLM_BEGIN               (FLM_FIRST + 0x0000)

        // void FastList_End (HWND hList);
        // void FastList_EndAll (HWND hList);
        //
#define FastList_End(_hList) \
           (void)SendMessage (_hList, FLM_END, 0, 0)
#define FastList_EndAll(_hList) \
           (void)SendMessage (_hList, FLM_END, 1, 0)
#define FLM_END                 (FLM_FIRST + 0x0001)

/*
 * ADD/REMOVE ITEMS
 *
 * These operations add and remove items to the list, and affect items'
 * properties.
 *
 */

        // HLISTITEM FastList_AddItem (HWND hList, LPFASTLISTADDITEM pai);
        //
#define FastList_AddItem(_hList,_pai) \
           (HLISTITEM)SendMessage (_hList, FLM_ADDITEM, 0, (LPARAM)(_pai))
#define FLM_ADDITEM             (FLM_FIRST + 0x0002)

        // void FastList_RemoveAll (HWND hList)
        // void FastList_RemoveItem (HWND hList, HLISTITEM hItem)
        //
#define FastList_RemoveAll(_hList) \
           FastList_RemoveItem(_hList,0)
#define FastList_RemoveItem(_hList,_hItem) \
           (void)SendMessage (_hList, FLM_REMOVEITEM, (WPARAM)(_hItem), 0)
#define FLM_REMOVEITEM          (FLM_FIRST + 0x0003)

        // LPCTSTR FastList_GetItemText (HWND hList, HLISTITEM hItem, int icol);
        // void FastList_SetItemText (HWND hList, HLISTITEM hItem, int icol, LPCTSTR pszText);
        //
#define FastList_GetItemText(_hList,_hItem,_iCol) \
           (LPCTSTR)SendMessage (_hList, FLM_GETITEMTEXT, (WPARAM)(_hItem), (LPARAM)(_iCol))
#define FastList_SetItemText(_hList,_hItem,_iCol,_psz) \
           do { FASTLISTITEMCOLUMN _flic; \
                _flic.hItem = _hItem; \
                _flic.icol = _iCol; \
                (void)SendMessage (_hList, FLM_SETITEMTEXT, (WPARAM)&_flic, (LPARAM)(_psz)); } while (0)
#define FLM_GETITEMTEXT         (FLM_FIRST + 0x0004)
#define FLM_SETITEMTEXT         (FLM_FIRST + 0x0005)

        // LPARAM FastList_GetItemParam (HWND hList, HLISTITEM hItem);
        // void FastList_SetItemParam (HWND hList, HLISTITEM hItem, LPARAM lParam);
        //
#define FastList_GetItemParam(_hList,_hItem) \
           (LPARAM)SendMessage (_hList, FLM_GETITEMPARAM, (WPARAM)(_hItem), 0)
#define FastList_SetItemParam(_hList,_hItem,_lp) \
           (void)SendMessage (_hList, FLM_SETITEMPARAM, (WPARAM)(_hItem), (LPARAM)(_lp))
#define FLM_GETITEMPARAM        (FLM_FIRST + 0x0006)
#define FLM_SETITEMPARAM        (FLM_FIRST + 0x0007)

        // DWORD FastList_GetItemFlags (HWND hList, HLISTITEM hItem);
        // void FastList_SetItemFlags (HWND hList, HLISTITEM hItem, DWORD dwFlags);
        //
#define FastList_GetItemFlags(_hList,_hItem) \
           (LPARAM)SendMessage (_hList, FLM_GETITEMFLAGS, (WPARAM)(_hItem), 0)
#define FastList_SetItemFlags(_hList,_hItem,_dw) \
           (void)SendMessage (_hList, FLM_SETITEMFLAGS, (WPARAM)(_hItem), (LPARAM)(_dw))
#define FLM_GETITEMFLAGS        (FLM_FIRST + 0x0008)
#define FLM_SETITEMFLAGS        (FLM_FIRST + 0x0009)

        // int FastList_GetItemFirstImage (HWND hList, HLISTITEM hItem);
        // int FastList_GetItemSecondImage (HWND hList, HLISTITEM hItem);
        // void FastList_SetItemFirstImage (HWND hList, HLISTITEM hItem, int image);
        // void FastList_SetItemSecondImage (HWND hList, HLISTITEM hItem, int image);
        //
#define FastList_GetItemFirstImage(_hList,_hItem) \
            FastList_GetItemImage(_hList,_hItem,0)
#define FastList_GetItemSecondImage(_hList,_hItem) \
            FastList_GetItemImage(_hList,_hItem,1)
#define FastList_SetItemFirstImage(_hList,_hItem,_image) \
            FastList_SetItemImage(_hList,_hItem,0,_image)
#define FastList_SetItemSecondImage(_hList,_hItem,_image) \
            FastList_SetItemImage(_hList,_hItem,1,_image)
#define FastList_GetItemImage(_hList,_hItem,_iImage) \
           (int)SendMessage (_hList, FLM_GETITEMIMAGE, (WPARAM)(_hItem), (LPARAM)(_iImage))
#define FastList_SetItemImage(_hList,_hItem,_iImage,_image) \
           do { FASTLISTITEMIMAGE _flii; \
                _flii.hItem = _hItem; \
                _flii.iImage = _iImage; \
                (void)SendMessage (_hList, FLM_SETITEMIMAGE, (WPARAM)&_flii, (LPARAM)(_image)); } while (0)
#define FLM_GETITEMIMAGE        (FLM_FIRST + 0x000A)
#define FLM_SETITEMIMAGE        (FLM_FIRST + 0x000B)

        // BOOL FastList_IsExpanded (HWND hList, HLISTITEM hItem);
        // void FastList_Expand (HWND hList, HLISTITEM hItem);
        // void FastList_Collapse (HWND hList, HLISTITEM hItem);
        // void FastList_SetExpanded (HWND hList, HLISTITEM hItem, BOOL fExpand);
        //
#define FastList_IsExpanded(_hList,_hItem) \
           (BOOL)SendMessage (_hList, FLM_ISEXPANDED, (WPARAM)(_hItem), 0)
#define FastList_Expand(_hList,_hItem) \
           FastList_SetExpanded(_hList,_hItem,TRUE)
#define FastList_Collapse(_hList,_hItem) \
           FastList_SetExpanded(_hList,_hItem,FALSE)
#define FastList_SetExpanded(_hList,_hItem,_fExpand) \
           (BOOL)SendMessage (_hList, FLM_EXPAND, (WPARAM)(_hItem), (LPARAM)(_fExpand))
#define FLM_ISEXPANDED          (FLM_FIRST + 0x000C)
#define FLM_EXPAND              (FLM_FIRST + 0x000D)

        // BOOL FastList_IsVisible (HWND hList, HLISTITEM hItem);
        // void FastList_EnsureVisible (HWND hList, HLISTITEM hItem);
        //
#define FastList_IsVisible(_hList,_hItem) \
           (BOOL)SendMessage (_hList, FLM_ITEMVISIBLE, (WPARAM)(_hItem), 0)
#define FastList_EnsureVisible(_hList,_hItem) \
           (BOOL)SendMessage (_hList, FLM_ITEMVISIBLE, (WPARAM)(_hItem), 1)
#define FLM_ITEMVISIBLE         (FLM_FIRST + 0x000E)

        // HLISTITEM FastList_GetFocus (HWND hList);
        // BOOL FastList_IsFocused (HWND hList, HLISTITEM hItem);
        // void FastList_SetFocus (HWND hList, HLISTITEM hItem);
        //
#define FastList_GetFocus(_hList) \
           (HLISTITEM)SendMessage (_hList, FLM_ITEMFOCUS, 0, 0)
#define FastList_IsFocused(_hList,_hItem) \
           (BOOL)SendMessage (_hList, FLM_ITEMFOCUS, (WPARAM)(_hItem), 0)
#define FastList_SetFocus(_hList,_hItem) \
           (void)SendMessage (_hList, FLM_ITEMFOCUS, (WPARAM)(_hItem), 1)
#define FLM_ITEMFOCUS           (FLM_FIRST + 0x000F)


/*
 * IMAGE LIST SUPPORT
 *
 * A FastList may have two separate IMAGELISTs associated with it--one for
 * 32x32 icons (used only in Large mode), and one for 16x16 icons (used in
 * all other display modes). Use COMMCTRL.H's ImageList_Create() to create
 * imagelists for use with a FastList.
 *
 */

        // void FastList_GetImageLists (HWND hList, HIMAGELIST *phiSmall, HIMAGELIST *phiLarge)
        //
#define FastList_GetImageLists(_hList,_phiSmall,_phiLarge) \
           (void)SendMessage (_hList, FLM_GETIMAGELISTS, (WPARAM)(_phiSmall), (LPARAM)(_phiLarge))
#define FLM_GETIMAGELISTS       (FLM_FIRST + 0x0010)

        // void FastList_SetImageLists (HWND hList, HIMAGELIST hiSmall, HIMAGELIST hiLarge)
        //
#define FastList_SetImageLists(_hList,_hiSmall,_hiLarge) \
           (void)SendMessage (_hList, FLM_SETIMAGELISTS, (WPARAM)(_hiSmall), (LPARAM)(_hiLarge))
#define FLM_SETIMAGELISTS       (FLM_FIRST + 0x0011)

        // HIMAGELIST FastList_CreateDragImage (HWND hList, HLISTITEM hItem)
        //
#define FastList_CreateDragImage(_hList,_hItem)  \
           (HIMAGELIST)SendMessage (_hList, FLM_CREATEDRAGIMAGE, (WPARAM)(_hItem), 0)
#define FLM_CREATEDRAGIMAGE     (FLM_FIRST + 0x0012)

/*
 * SORTING
 *
 * The contents of a FastList are always kept sorted; they will never be
 * displayed out-of-sort. The function used to sort items may be replaced
 * by calling the FastList_SetSortFunction() method, and parameters
 * regarding the sort may be changed by calling the FastList_SetSortStyle()
 * function. The default sorting parameters are:
 *
 *    Function: FastList_SortFunc_AlphaNumeric()  [declared way down below]
 *    Column:   Sorted on column 0
 *    Reverse:  No; items ascending-sorted
 *
 * The FastList_Sort() function may be called to initiate a re-sort;
 * however, doing so is unnecessary (as the list is automatically re-sorted
 * when necessary). When displaying items in a tree, sorting is performed
 * between sibling items only.
 *
 */

        // LPFASTLISTSORTFUNC FastList_GetSortFunction (HWND hList)
        //
#define FastList_GetSortFunction(_hList,_pfn) \
           (LPFASTLISTSORTFUNC)SendMessage (_hList, FLM_GETSORTFUNC, 0, 0)
#define FLM_GETSORTFUNC         (FLM_FIRST + 0x0013)

        // void FastList_SetSortFunction (HWND hList, LPFASTLISTSORTFUNC pfn)
        //
#define FastList_SetSortFunction(_hList,_pfn) \
           (void)SendMessage (_hList, FLM_SETSORTFUNC, 0, (LPARAM)(_pfn))
#define FLM_SETSORTFUNC         (FLM_FIRST + 0x0014)

        // void FastList_GetSortStyle (HWND hList, int *piColumn, BOOL *pfReverse)
        //
#define FastList_GetSortStyle(_hList,_piCol,_pfRev) \
           (void)SendMessage (_hList, FLM_GETSORTSTYLE, (WPARAM)(_piCol), (LPARAM)(_pfRev))
#define FLM_GETSORTSTYLE        (FLM_FIRST + 0x0015)

        // void FastList_SetSortStyle (HWND hList, int iColumn, BOOL fReverse)
        //
#define FastList_SetSortStyle(_hList,_iCol,_fRev) \
           (void)SendMessage (_hList, FLM_SETSORTSTYLE, (WPARAM)(_iCol), (LPARAM)(_fRev))
#define FLM_SETSORTSTYLE        (FLM_FIRST + 0x0016)

        // void FastList_Sort (HWND hList)
        //
#define FastList_Sort(_hList) \
           (void)SendMessage (_hList, FLM_SORT, 0, 0)
#define FLM_SORT                (FLM_FIRST + 0x0017)

/*
 * COLUMNS
 *
 * In List and TreeList mode, a FastList can be caused to display a column
 * header and the associated multiple columns of information for each item.
 * FastList_AddItem() supplies only a place for the item's column-0 text
 * to be specified; other columns may be specified by FastList_SetItemText(),
 * but if not, the FastList will call back to its parent window via the
 * WM_NOTIFY/FLN_GETITEMTEXT notification to obtain text for the items'
 * unspecified columns.
 *
 */

        // int FastList_GetColumnCount (HWND hList)
        //
#define FastList_GetColumnCount(_hList) \
           (int)SendMessage (_hList, FLM_GETCOLUMNCOUNT, 0, 0)
#define FLM_GETCOLUMNCOUNT      (FLM_FIRST + 0x0018)

        // BOOL FastList_GetColumn (HWND hList, int iColumn, LPFASTLISTCOLUMN pcol)
        //
#define FastList_GetColumn(_hList,_iCol,_pCol) \
           (BOOL)SendMessage (_hList, FLM_GETCOLUMN, (WPARAM)(_iCol), (LPARAM)(_pCol))
#define FLM_GETCOLUMN           (FLM_FIRST + 0x0019)

        // void FastList_SetColumn (HWND hList, int iColumn, LPFASTLISTCOLUMN pcol)
        // void FastList_RemoveColumn (HWND hList, int iColumn)
        //
#define FastList_SetColumn(_hList,_iCol,_pCol) \
           (void)SendMessage (_hList, FLM_SETCOLUMN, (WPARAM)(_iCol), (LPARAM)(_pCol))
#define FastList_RemoveColumn(_hList,_iCol) \
           FastList_SetColumn (_hList, _iCol, NULL)
#define FLM_SETCOLUMN           (FLM_FIRST + 0x001A)

/*
 * SELECTION
 *
 * A FastList supports several types of selection.  Single Selection causes
 * any current selection to be cleared before a new item can be selected,
 * either programmatically or by user action. Multiple Selection
 * indiscriminately allows any items to be selected or deselected at
 * the same time.  Multiple/Sibling Selection requires that, if an item
 * is selected which is not a sibling of any currently-selected items,
 * the current selection is cleared first.  Multiple/Level Selection requires
 * that, if an item is selected which is at a different level from the root
 * than any currently-selected item, the current selection is cleared first.
 *
 */

        // BOOL FastList_IsSelected (HWND hList, HLISTITEM hItem)
        //
#define FastList_IsSelected(_hList,_hItem) \
           (BOOL)SendMessage (_hList, FLM_ISSELECTED, (WPARAM)(_hItem), 0)
#define FLM_ISSELECTED          (FLM_FIRST + 0x001B)

        // void FastList_SelectAll (HWND hList)
        // void FastList_SelectNone (HWND hList)
        // void FastList_SelectItem (HWND hList, HLISTITEM hItem, BOOL fSelect)
        //
#define FastList_SelectAll(_hList) \
           FastList_SelectItem(_hList,0,TRUE)
#define FastList_SelectNone(_hList) \
           FastList_SelectItem(_hList,0,FALSE)
#define FastList_SelectItem(_hList,_hItem,_fSel) \
           (void)SendMessage (_hList, FLM_SELECTITEM, (WPARAM)(_hItem), (LPARAM)(_fSel))
#define FLM_SELECTITEM          (FLM_FIRST + 0x001C)

/*
 * ENUMERATION
 *
 * A FastList provides multiple methods for enumeration. Items in the list
 * may be enumerated as members of a hierarchy, or as members of a list.
 * Additionally, the items in a FastList are hashed across their lParam
 * values, so the FastList item matching a given lParam value can quickly
 * be found. All enumeration functions are either constant time, or (as
 * in the case of lParam-to-HLISTITEM lookups) extremely close to it.
 *
 * A typical enumeration will look like this:
 *    for (HLISTITEM hItem = FastList_FindFirst (hList);
 *         hItem != NULL;
 *         hItem = FastList_FindNext (hList, hItem))
 *       {
 *       // ...
 *       }
 *
 * Or, equivalently:
 *    HLISTITEM hItem = NULL;
 *    while ((hItem = FastList_FindNext (hList, hItem)) != NULL)
 *       {
 *       // ...
 *       }
 *
 * If an enumeration involving an LPENUM pointer is terminated without
 * having enumerated all items in the list, the loop must be followed
 * by a FastList_FindClose() statement. (This statement is optional
 * otherwise.) If the state of the list or tree is updated, the enumeration
 * should be stopped.
 *
 */

        // HLISTITEM FastList_FindFirst (HWND hList)
        // HLISTITEM FastList_FindNext (HWND hList, HLISTITEM hItem)
        // HLISTITEM FastList_FindPrevious (HWND hList, HLISTITEM hItem)
        //
#define FastList_FindFirst(_hList)  \
           FastList_FindList(_hList,0,FLM_FINDLIST_FIRST) 
#define FastList_FindNext(_hList,_hItem)  \
           FastList_FindList(_hList,_hItem,FLM_FINDLIST_NEXT) 
#define FastList_FindPrevious(_hList,_hItem)  \
           FastList_FindList(_hList,_hItem,FLM_FINDLIST_PREVIOUS) 
#define FastList_FindList(_hList,_hItem,_dwCode) \
           (HLISTITEM)SendMessage (_hList, FLM_FINDLIST, (WPARAM)(_hItem), (LPARAM)(_dwCode))
#define FLM_FINDLIST            (FLM_FIRST + 0x001D)
#define FLM_FINDLIST_FIRST      0
#define FLM_FINDLIST_PREVIOUS   1
#define FLM_FINDLIST_NEXT       2

        // HLISTITEM FastList_FindFirstInRoot (HWND hList)
        // HLISTITEM FastList_FindParent (HWND hList, HLISTITEM hItem)
        // HLISTITEM FastList_FindFirstChild (HWND hList, HLISTITEM hItem)
        // HLISTITEM FastList_FindPreviousSibling (HWND hList, HLISTITEM hItem)
        // HLISTITEM FastList_FindNextSibling (HWND hList, HLISTITEM hItem)
        //
#define FastList_FindFirstInRoot(_hList)  \
           FastList_FindTree(_hList,0,FLM_FINDTREE_CHILD) 
#define FastList_FindParent(_hList,_hItem)  \
           FastList_FindTree(_hList,_hItem,FLM_FINDTREE_PARENT) 
#define FastList_FindFirstChild(_hList,_hItem)  \
           FastList_FindTree(_hList,_hItem,FLM_FINDTREE_CHILD) 
#define FastList_FindPreviousSibling(_hList,_hItem)  \
           FastList_FindTree(_hList,_hItem,FLM_FINDTREE_PREVIOUS) 
#define FastList_FindNextSibling(_hList,_hItem)  \
           FastList_FindTree(_hList,_hItem,FLM_FINDTREE_NEXT) 
#define FastList_FindTree(_hList,_hItem,_dwCode) \
           (HLISTITEM)SendMessage (_hList, FLM_FINDTREE, (WPARAM)(_hItem), (LPARAM)(_dwCode))
#define FLM_FINDTREE            (FLM_FIRST + 0x001E)
#define FLM_FINDTREE_PARENT     0
#define FLM_FINDTREE_CHILD      1
#define FLM_FINDTREE_PREVIOUS   2
#define FLM_FINDTREE_NEXT       3

        // HLISTITEM FastList_FindFirstSelected (HWND hList)
        // HLISTITEM FastList_FindNextSelected (HWND hList, HLISTITEM hItemPrevious)
        //
#define FastList_FindFirstSelected(_hList)  \
           FastList_FindNextSelected(_hList,0) 
#define FastList_FindNextSelected(_hList,_hItem) \
           (HLISTITEM)SendMessage (_hList, FLM_FINDSELECTED, (WPARAM)(_hItem), 0)
#define FLM_FINDSELECTED        (FLM_FIRST + 0x001F)

        // HLISTITEM FastList_FindItem (HWND hList, LPARAM lParam)
        // HLISTITEM FastList_FindFirstItem (HWND hList, LPENUM *ppEnum, LPARAM lParam)
        // HLISTITEM FastList_FindNextItem (HWND hList, LPENUM *ppEnum)
        // void FastList_FindClose (HWND hList, LPENUM *ppEnum)
        //
#define FastList_FindItem(_hList,_lp)  \
           (HLISTITEM)SendMessage (_hList, FLM_FINDITEM, 0, (LPARAM)(_lp))
#define FastList_FindFirstItem(_hList,_ppEnum,_lp)  \
           (HLISTITEM)SendMessage (_hList, FLM_FINDITEM, (WPARAM)(_ppEnum), (LPARAM)(_lp))
#define FastList_FindNextItem(_hList,_ppEnum)  \
           (HLISTITEM)SendMessage (_hList, FLM_FINDNEXTITEM, (WPARAM)(_ppEnum), 0)
#define FastList_FindClose(_hList,_ppEnum) \
           (void)SendMessage (_hList, FLM_FINDCLOSE, (WPARAM)(_ppEnum), 0) 
#define FLM_FINDITEM            (FLM_FIRST + 0x0020)
#define FLM_FINDNEXTITEM        (FLM_FIRST + 0x0021)
#define FLM_FINDCLOSE           (FLM_FIRST + 0x0022)

        // int FastList_GetItemCount (HWND hList)
        // int FastList_GetVisibleItemCount (HWND hList)
        //
#define FastList_GetItemCount(_hList) \
           (int)SendMessage (_hList, FLM_GETITEMCOUNT, 0, 0)
#define FastList_GetVisibleItemCount(_hList) \
           (int)SendMessage (_hList, FLM_GETITEMCOUNT, 1, 0)
#define FLM_GETITEMCOUNT        (FLM_FIRST + 0x0023)

        // HLISTITEM FastList_FindFirstVisible (HWND hList)
        // HLISTITEM FastList_FindNextVisible (HWND hList, HLISTITEM hItem)
        //
#define FastList_FindFirstVisible(_hList)  \
           FastList_FindVisible(_hList,0,FLM_FINDVISIBLE_FIRST) 
#define FastList_FindNextVisible(_hList,_hItem)  \
           FastList_FindVisible(_hList,_hItem,FLM_FINDVISIBLE_NEXT) 
#define FastList_FindVisible(_hList,_hItem,_dwCode) \
           (HLISTITEM)SendMessage (_hList, FLM_FINDVISIBLE, (WPARAM)(_hItem), (LPARAM)(_dwCode))
#define FLM_FINDVISIBLE         (FLM_FIRST + 0x0024)
#define FLM_FINDVISIBLE_FIRST   0
#define FLM_FINDVISIBLE_NEXT    1

/*
 * ITEM DISPLAY
 *
 * Like any other control, a FastList can tell you which item is underneath
 * a particular point, and can tell you what RECTs an item uses for what
 * purposes on the display.
 *
 * There are style bits which can be set to affect hit-testing on
 * a FastList. Notably, the FLS_HIT_TEXTONLY bit prevents left- or right-
 * clicking in whitespace to select an item (in list mode).
 *
 */

        // HLISTITEM FastList_ItemFromPoint (HWND hList, POINT *pptClient, BOOL fStrict)
        //
#define FastList_ItemFromPoint(_hList,_pptClient,_fStrict)  \
           (HLISTITEM)SendMessage (_hList, FLM_ITEMFROMPOINT, (WPARAM)(_pptClient), (LPARAM)(_fStrict))
#define FLM_ITEMFROMPOINT       (FLM_FIRST + 0x0025)

        // void FastList_GetItemRegions (HWND hList, HLISTITEM hItem, LPFASTLISTITEMREGIONS pReg)
        //
#define FastList_GetItemRegions(_hList,_hItem,_pReg)  \
           (HLISTITEM)SendMessage (_hList, FLM_GETITEMREGIONS, (WPARAM)(_hItem), (LPARAM)(_pReg))
#define FLM_GETITEMREGIONS      (FLM_FIRST + 0x0026)

        // LPFASTLISTTEXTCALLBACK FastList_GetTextCallback (HWND hList)
        // void FastList_SetTextCallback (HWND hList, LPFASTLISTTEXTCALLBACK pfn, DWORD dwCookie)
        //
#define FastList_GetTextCallback(_hList)  \
           (LPFASTLISTTEXTCALLBACK)SendMessage (_hList, FLM_GETTEXTCALLBACK, 0, 0)
#define FastList_SetTextCallback(_hList,_pfn,_dwCookie)  \
           (void)SendMessage (_hList, FLM_SETTEXTCALLBACK, (WPARAM)(_pfn), (LPARAM)(_dwCookie))
#define FLM_GETTEXTCALLBACK     (FLM_FIRST + 0x0027)
#define FLM_SETTEXTCALLBACK     (FLM_FIRST + 0x0028)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

EXPORTED BOOL RegisterFastListClass (void);
EXPORTED BOOL fIsFastList (HWND hList);

EXPORTED void FastList_Enter (HWND hList);
EXPORTED void FastList_Leave (HWND hList);

EXPORTED int CALLBACK FastList_SortFunc_Alphabetic (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2);
EXPORTED int CALLBACK FastList_SortFunc_Numeric (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2);
EXPORTED int CALLBACK FastList_SortFunc_AlphaNumeric (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2);


#endif

