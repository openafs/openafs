/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef DEBUG
#define DEBUG
#endif
#ifdef NO_DEBUG_ALLOC
#undef NO_DEBUG_ALLOC
#endif

#include <windows.h>
#include <commctrl.h>
#include <WINNT/tal_alloc.h>


/*
 * PARAMETERS _________________________________________________________________
 *
 */

#define cDEFAULT_BUCKETS 128

#define dwSIG_AT_END     'Okay'

#define iINVALID         ((size_t)-1)

#define MIN_BUCKETS(_cObjects)     ((_cObjects) / 23)
#define TARGET_BUCKETS(_cObjects)  ((_cObjects) /  5)

#define TRACK_FREED

#define cmsecREFRESH   1000


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   PVOID aElements; // = EXPANDARRAYHEAP.aElements + 4;
   // Followed by allocated space for aElements
   } EXPANDARRAYHEAP, *LPEXPANDARRAYHEAP;

class ALLOCEXPANDARRAY
   {
   public:
      ALLOCEXPANDARRAY (size_t cbElement, size_t cElementsPerHeap = 0);
      ~ALLOCEXPANDARRAY (void);

      PVOID GetAt (size_t iElement);
      void SetAt (size_t iElement, PVOID pData);

   private:
      size_t m_cbElement;
      size_t m_cElementsPerHeap;

      size_t m_cHeaps;
      LPEXPANDARRAYHEAP *m_aHeaps;
   };

typedef struct
   {
   PVOID pData;
   size_t cbData;
   LPSTR pszExpr;
   LPSTR pszFile;
   DWORD dwLine;
   DWORD dwTick;
   DWORD dwEndSig;
   BOOL fCPP;
   size_t iNext;
   size_t iPrev;
   BOOL fFreed;
   BOOL fInList;
   BOOL fTared;
   } MEMCHUNK, *PMEMCHUNK;

typedef struct
   {
   size_t iFirst;
   } BUCKET, *PBUCKET;

typedef struct
   {
   size_t cAllocCpp;
   size_t cAllocDyna;
   size_t cAllocTotal;
   size_t cAllocCppTared;
   size_t cAllocDynaTared;
   size_t cAllocTotalTared;
   size_t cbAllocCpp;
   size_t cbAllocDyna;
   size_t cbAllocTotal;
   size_t cbAllocCppTared;
   size_t cbAllocDynaTared;
   size_t cbAllocTotalTared;
   } STATISTICS;

static struct l
   {
   CRITICAL_SECTION *pcs;
   HWND hManager;
   UINT_PTR idTimer;

   ALLOCEXPANDARRAY *pHeap;
   size_t cChunks;

   BUCKET *aBuckets;
   size_t cBuckets;

   STATISTICS Stats;
   } l;

static struct lr
   {
   RECT rManager;
   LONG acxColumns[6];
   int iColSort;
   BOOL fSortRev;
   } lr;

#define HASH(_dw,_cTable)  (((DWORD)(_dw) >> 4) % (_cTable))


/*
 * REALLOC ____________________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) Alloc_ReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL Alloc_ReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)GlobalAlloc (GMEM_FIXED, cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget == 0)
      memset (pNew, 0x00, cbElement * cNew);
   else
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      memset (&((char*)pNew)[ cbElement * (*pcTarget) ], 0x00, cbElement * (cNew - *pcTarget));
      GlobalFree ((HGLOBAL)*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}
#endif


/*
 * USER INTERFACE ROUTINES ____________________________________________________
 *
 */

#define IDC_INITIALIZE           100
#define IDC_BOX_ALLOC            101
#define IDC_LABEL_CPP            102
#define IDC_LABEL_OTHER          103
#define IDC_LABEL_TARED          104
#define IDC_LABEL_TOTAL          106
#define IDC_LABEL_COUNT          107
#define IDC_LABEL_SIZE           108
#define IDC_LABEL_AVERAGE        109
#define IDC_VALUE_CPP_COUNT      110
#define IDC_VALUE_OTHER_COUNT    111
#define IDC_VALUE_TARED_COUNT    112
#define IDC_VALUE_TOTAL_COUNT    113
#define IDC_VALUE_CPP_SIZE       114
#define IDC_VALUE_OTHER_SIZE     115
#define IDC_VALUE_TARED_SIZE     116
#define IDC_VALUE_TOTAL_SIZE     117
#define IDC_VALUE_CPP_AVERAGE    118
#define IDC_VALUE_OTHER_AVERAGE  119
#define IDC_VALUE_TARED_AVERAGE  120
#define IDC_VALUE_TOTAL_AVERAGE  121
#define IDC_BOX_DETAILS          122
#define IDC_LIST                 123
#define IDC_LABEL                124
#define IDC_HIDE                 125
#define IDC_TARE                 126
#define IDC_RESET                127
#define IDC_REFRESH              128
#define IDC_LIST_ADD             129
#define IDC_LIST_REMOVE          130
#define IDC_LIST_REMOVEADD       131

#define cszTITLE        TEXT("Memory Manager")

typedef struct
   {
   RECT rBoxAlloc;
   RECT rLabelCpp;
   RECT rLabelOther;
   RECT rLabelTared;
   RECT rLabelTotal;
   RECT rLabelCount;
   RECT rLabelSize;
   RECT rLabelAverage;
   RECT rValueCppCount;
   RECT rValueOtherCount;
   RECT rValueTaredCount;
   RECT rValueTotalCount;
   RECT rValueCppSize;
   RECT rValueOtherSize;
   RECT rValueTaredSize;
   RECT rValueTotalSize;
   RECT rValueCppAverage;
   RECT rValueOtherAverage;
   RECT rValueTaredAverage;
   RECT rValueTotalAverage;
   RECT rBoxDetails;
   RECT rList;
   RECT rLabel;
   RECT rHide;
   RECT rTare;
   RECT rReset;
   RECT rClose;
   } WINDOWSIZES;

#define cBORDER      7
#define cxBETWEEN    6
#define cyBETWEEN    6
#define cxBUTTONS   65
#define cyBUTTONS   22
#define cxLABELS    70
#define cxVALUES    70
#define cyLABELS    15


HWND MakeWindow (LPCTSTR pszClass, LPCTSTR pszTitle, DWORD dwStyle, RECT *prSource, 
                 HWND hParent, UINT idc, DWORD dwStyleEx = 0)
{
   RECT rr = { 0, 0, 16, 16 };
   if (prSource)
      rr = *prSource;
   HWND hWnd = CreateWindowEx (dwStyleEx, pszClass, pszTitle, dwStyle, 
                               rr.left, rr.top, rr.right - rr.left, rr.bottom - rr.top, 
                               hParent, (HMENU)UIntToPtr(idc), GetModuleHandle(0), 0);
   if (IsWindow (hWnd))
      SendMessage (hWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 1);
   return hWnd;
}

void SetWindowRect (HWND hWnd, RECT *pr)
{
   SetWindowPos (hWnd, 0, pr->left, pr->top, pr->right - pr->left, 
                 pr->bottom - pr->top, SWP_NOZORDER | SWP_NOACTIVATE);
}

void FormatBytes (LPTSTR pszText, double lfValue)
{
   if (lfValue < 0)
      {
      lfValue = 0-lfValue;
      *pszText++ = TEXT('-');
      }

   if (lfValue >= 1048576)
      {
      LONG Value = (LONG)(lfValue / 1048576);
      lfValue -= Value * 1048576;
      LONG Frac = (LONG)(lfValue * 100 / 1048576);
      wsprintf (pszText, TEXT("%ld.%ld MB"), Value, Frac);
      }
   else if (lfValue >= 3072)
      {
      LONG Value = (LONG)(lfValue / 1024);
      lfValue -= Value * 1024;
      LONG Frac = (LONG)(lfValue * 100 / 1024);
      wsprintf (pszText, TEXT("%ld.%ld kb"), Value, Frac);
      }
   else // (lfValue < 3072)
      {
      wsprintf (pszText, TEXT("%ld b"), (LONG)lfValue);
      }
}

void FormatTime (LPTSTR pszText, DWORD dwTick)
{
   static FILETIME ftTickZero = { 0, 0 };
   if (!ftTickZero.dwHighDateTime && !ftTickZero.dwLowDateTime)
      {
      // We need to find out what FILETIME corresponds with a tick
      // count of zero. To do that, first find the FILETIME and tick
      // count that represent *now* (in local time).
      //
      SYSTEMTIME stNow;
      GetLocalTime (&stNow);
      SystemTimeToFileTime (&stNow, &ftTickZero);

      DWORD dwTickNow = GetTickCount();

      // A FILETIME is a count-of-100ns-intervals, and a tick count is
      // a count of 1ms-intervals.  So we need to subtract off a big
      // number times our tick count.
      //
      if ((dwTickNow * 10000) < (dwTickNow))
         ftTickZero.dwHighDateTime --;
      dwTickNow *= 10000;   // convert to 100ns intervals
      if (dwTickNow > ftTickZero.dwLowDateTime)
         ftTickZero.dwHighDateTime --;
      ftTickZero.dwLowDateTime -= dwTickNow; // unsigned, so it'll wrap OK
      }

   // Convert the given tick count into 100ns intervals, and add it
   // to our ftTickZero.
   //
   FILETIME ftTick = ftTickZero;

   if ((dwTick * 10000) < (dwTick))
      ftTick.dwHighDateTime ++;
   dwTick *= 10000;   // convert to 100ns intervals
   if (dwTick > (DWORD)(0-ftTick.dwLowDateTime)) // too big to add?
      ftTick.dwHighDateTime ++;
   ftTick.dwLowDateTime += dwTick; // unsigned, so it'll wrap OK

   // Convert to a SYSTEMTIME, and output the time component
   //
   SYSTEMTIME stTick;
   FileTimeToSystemTime (&ftTick, &stTick);

   wsprintf (pszText, TEXT("%ld:%02ld:%02ld.%02ld"),
              (DWORD)stTick.wHour,
              (DWORD)stTick.wMinute,
              (DWORD)stTick.wSecond,
              (DWORD)(stTick.wMilliseconds / 100));
}

void SetDlgItemBytes (HWND hDlg, int idc, double lfValue)
{
   TCHAR szText[ 256 ];
   FormatBytes (szText, lfValue);
   SetDlgItemText (hDlg, idc, szText);
}


void MemMgr_ShowWarning (PMEMCHUNK pChunk, LPSTR pszFile, DWORD dwLine, LPTSTR pszDesc)
{
   TCHAR szMessage[ 1024 ];
   wsprintf (szMessage, TEXT("%s\n\n   Address: 0x%08p (%s)\n   Allocated: %s line %ld\n   Freed: %s line %ld\n\nClick OK for memory details."), pszDesc, pChunk->pData, pChunk->pszExpr, pChunk->pszFile, pChunk->dwLine, pszFile, dwLine);
   if (MessageBox (NULL, szMessage, cszTITLE, MB_ICONHAND | MB_OKCANCEL | MB_DEFBUTTON2) == IDOK)
      {
      // TODO: Show Details
      }
}

void MIX (RECT *pr, RECT *pr1, RECT *pr2)
{
   pr->left = pr2->left;
   pr->top = pr1->top;
   pr->right = pr2->right;
   pr->bottom = pr1->bottom;
}

void MemMgr_GetWindowSizes (WINDOWSIZES *pSizes)
{
   RECT rClient;
   GetClientRect (l.hManager, &rClient);
   InflateRect (&rClient, 0-cBORDER, 0-cBORDER);

   pSizes->rLabelAverage.right = rClient.right -cxBETWEEN*2;
   pSizes->rLabelAverage.top = rClient.top +8 +cyBETWEEN;
   pSizes->rLabelAverage.left = pSizes->rLabelAverage.right -cxVALUES;
   pSizes->rLabelAverage.bottom = pSizes->rLabelAverage.top +cyLABELS;

   pSizes->rLabelSize = pSizes->rLabelAverage;
   pSizes->rLabelSize.left -= cxBETWEEN + cxVALUES;
   pSizes->rLabelSize.right -= cxBETWEEN + cxVALUES;

   pSizes->rLabelCount = pSizes->rLabelSize;
   pSizes->rLabelCount.left -= cxBETWEEN + cxVALUES;
   pSizes->rLabelCount.right -= cxBETWEEN + cxVALUES;

   pSizes->rLabelCpp.left = rClient.left +cxBETWEEN;
   pSizes->rLabelCpp.top = pSizes->rLabelCount.bottom +cyBETWEEN;
   pSizes->rLabelCpp.right = pSizes->rLabelCpp.left +cxLABELS;
   pSizes->rLabelCpp.bottom = pSizes->rLabelCpp.top +cyLABELS;

   pSizes->rLabelOther = pSizes->rLabelCpp;
   pSizes->rLabelOther.top += cyBETWEEN + cyLABELS;
   pSizes->rLabelOther.bottom += cyBETWEEN + cyLABELS;

   pSizes->rLabelTotal = pSizes->rLabelOther;
   pSizes->rLabelTotal.top += cyBETWEEN + cyLABELS;
   pSizes->rLabelTotal.bottom += cyBETWEEN + cyLABELS;

   pSizes->rLabelTared = pSizes->rLabelTotal;
   pSizes->rLabelTared.top += cyBETWEEN + cyLABELS;
   pSizes->rLabelTared.bottom += cyBETWEEN + cyLABELS;

   pSizes->rBoxAlloc = rClient;
   pSizes->rBoxAlloc.bottom = pSizes->rLabelTared.bottom +cyBETWEEN;

   MIX (&pSizes->rValueCppCount, &pSizes->rLabelCpp, &pSizes->rLabelCount);
   MIX (&pSizes->rValueOtherCount, &pSizes->rLabelOther, &pSizes->rLabelCount);
   MIX (&pSizes->rValueTaredCount, &pSizes->rLabelTared, &pSizes->rLabelCount);
   MIX (&pSizes->rValueTotalCount, &pSizes->rLabelTotal, &pSizes->rLabelCount);

   MIX (&pSizes->rValueCppSize, &pSizes->rLabelCpp, &pSizes->rLabelSize);
   MIX (&pSizes->rValueOtherSize, &pSizes->rLabelOther, &pSizes->rLabelSize);
   MIX (&pSizes->rValueTaredSize, &pSizes->rLabelTared, &pSizes->rLabelSize);
   MIX (&pSizes->rValueTotalSize, &pSizes->rLabelTotal, &pSizes->rLabelSize);

   MIX (&pSizes->rValueCppAverage, &pSizes->rLabelCpp, &pSizes->rLabelAverage);
   MIX (&pSizes->rValueOtherAverage, &pSizes->rLabelOther, &pSizes->rLabelAverage);
   MIX (&pSizes->rValueTaredAverage, &pSizes->rLabelTared, &pSizes->rLabelAverage);
   MIX (&pSizes->rValueTotalAverage, &pSizes->rLabelTotal, &pSizes->rLabelAverage);

   pSizes->rBoxDetails = rClient;
   pSizes->rBoxDetails.top = pSizes->rBoxAlloc.bottom +cyBETWEEN;
   pSizes->rBoxDetails.bottom -= cyBUTTONS +cyBETWEEN;

   pSizes->rList = pSizes->rBoxDetails;
   pSizes->rList.top += 8;
   InflateRect (&pSizes->rList, 0-cBORDER, 0-cBORDER);

   pSizes->rClose = rClient;
   pSizes->rClose.top = pSizes->rClose.bottom - cyBUTTONS;
   pSizes->rClose.left = pSizes->rClose.right - cxBUTTONS;

   pSizes->rReset = pSizes->rClose;
   pSizes->rReset.right = pSizes->rClose.left - cxBETWEEN;
   pSizes->rReset.left = pSizes->rReset.right - cxBUTTONS;

   pSizes->rTare = pSizes->rClose;
   pSizes->rTare.right = pSizes->rReset.left - cxBETWEEN;
   pSizes->rTare.left = pSizes->rTare.right - cxBUTTONS;

   pSizes->rLabel = pSizes->rTare;
   pSizes->rLabel.right = pSizes->rTare.left - cxBETWEEN;
   pSizes->rLabel.left = pSizes->rLabel.right - cxBUTTONS;

   pSizes->rHide = pSizes->rLabel;
   pSizes->rHide.right = pSizes->rLabel.left - cxBETWEEN;
   pSizes->rHide.left = pSizes->rHide.right - cxBUTTONS;
}

void MemMgr_OnSize (void)
{
   WINDOWSIZES Sizes;
   MemMgr_GetWindowSizes (&Sizes);
   SetWindowRect (GetDlgItem (l.hManager, IDC_BOX_ALLOC), &Sizes.rBoxAlloc);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL_CPP), &Sizes.rLabelCpp);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL_OTHER), &Sizes.rLabelOther);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL_TARED), &Sizes.rLabelTared);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL_TOTAL), &Sizes.rLabelTotal);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL_COUNT), &Sizes.rLabelCount);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL_SIZE), &Sizes.rLabelSize);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL_AVERAGE), &Sizes.rLabelAverage);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_CPP_COUNT), &Sizes.rValueCppCount);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_OTHER_COUNT), &Sizes.rValueOtherCount);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_TARED_COUNT), &Sizes.rValueTaredCount);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_TOTAL_COUNT), &Sizes.rValueTotalCount);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_CPP_SIZE), &Sizes.rValueCppSize);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_OTHER_SIZE), &Sizes.rValueOtherSize);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_TARED_SIZE), &Sizes.rValueTaredSize);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_TOTAL_SIZE), &Sizes.rValueTotalSize);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_CPP_AVERAGE), &Sizes.rValueCppAverage);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_OTHER_AVERAGE), &Sizes.rValueOtherAverage);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_TARED_AVERAGE), &Sizes.rValueTaredAverage);
   SetWindowRect (GetDlgItem (l.hManager, IDC_VALUE_TOTAL_AVERAGE), &Sizes.rValueTotalAverage);
   SetWindowRect (GetDlgItem (l.hManager, IDC_BOX_DETAILS), &Sizes.rBoxDetails);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LIST), &Sizes.rList);
   SetWindowRect (GetDlgItem (l.hManager, IDC_LABEL), &Sizes.rLabel);
   SetWindowRect (GetDlgItem (l.hManager, IDC_HIDE), &Sizes.rHide);
   SetWindowRect (GetDlgItem (l.hManager, IDC_TARE), &Sizes.rTare);
   SetWindowRect (GetDlgItem (l.hManager, IDC_RESET), &Sizes.rReset);
   SetWindowRect (GetDlgItem (l.hManager, IDCANCEL), &Sizes.rClose);
}


void MemMgr_OnListRemove (PVOID pData)
{
   HWND hList = GetDlgItem (l.hManager, IDC_LIST);

   LV_FINDINFO Find;
   Find.flags = LVFI_PARAM;
   Find.lParam = (LPARAM)pData;

   int iItem;
   if ((iItem = ListView_FindItem (hList, 0, &Find)) == -1)
      {
      // The listview's find feature sucks; I've seen with my own little
      // eyes that it may miss an item if it's the only one in a list.
      // Look for ourselves.
      //
      int cItems = ListView_GetItemCount(hList);
      for (iItem = 0; iItem < cItems; ++iItem)
         {
         LV_ITEM Item;
         Item.iItem = iItem;
         Item.mask = LVIF_PARAM;
         ListView_GetItem (hList, &Item);
         if (Item.lParam == (LPARAM)pData)
            break;
         }
      }

   if (iItem != -1)
      {
      ListView_DeleteItem (hList, iItem);
      }
}


void MemMgr_RemoveFromList (PMEMCHUNK pChunk)
{
   PostMessage (l.hManager, WM_COMMAND, IDC_LIST_REMOVE, (LPARAM)(pChunk->pData));
   pChunk->fInList = FALSE;
}


LPTSTR MemMgr_GetItemKey (HWND hList, int iItem)
{
   static TCHAR szText[ 256 ];
   LPTSTR pszReturn = NULL;

   switch (lr.iColSort)
      {
      case 0:
      case 4:
      case 5:
         LV_ITEM Item;
         Item.iItem = iItem;
         Item.mask = LVIF_PARAM;
         ListView_GetItem (hList, &Item);

         EnterCriticalSection (l.pcs);

         size_t iBucket;
         iBucket = HASH(Item.lParam,l.cBuckets);

         PMEMCHUNK pChunk;
         pChunk = NULL;

         size_t iChunk;
         for (iChunk = l.aBuckets[iBucket].iFirst; iChunk != iINVALID; )
            {
            if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) == NULL)
               break;
            if (pChunk->pData == (PVOID)Item.lParam)
               break;
            if ((iChunk = pChunk->iNext) == iINVALID)
               {
               pChunk = NULL;
               break;
               }
            }

         if (pChunk)
            {
            if (lr.iColSort == 0)
               pszReturn = (LPTSTR)UlongToPtr(pChunk->dwTick);
            else if (lr.iColSort == 4)
               pszReturn = (LPTSTR)pChunk->cbData;
            else // (lr.iColSort == 5)
               pszReturn = (LPTSTR)pChunk->pData;
            }

         LeaveCriticalSection (l.pcs);
         break;

      default:
         ListView_GetItemText (hList, iItem, lr.iColSort, szText, 256);
         pszReturn = szText;
         break;
      }

   return pszReturn;
}


int MemMgr_CompareItems (LPTSTR pszKey1, LPTSTR pszKey2)
{
   int dw;

   switch (lr.iColSort)
      {
      case 0:
      case 4:
      case 5:
         dw = (int)( pszKey1 - pszKey2 );
         break;

      default:
         dw = lstrcmpi (pszKey1, pszKey2);
         break;
      }

   return (lr.fSortRev) ? (0-dw) : dw;
}


int MemMgr_PickListInsertionPoint (HWND hList, LPTSTR pszKey)
{
   int iItemLow  = 0;
   int iItemHigh = ListView_GetItemCount (hList) -1;

   while (iItemLow <= iItemHigh)
      {
      int iItemTest = iItemLow + (iItemHigh - iItemLow) / 2;

      LPTSTR pszAlternate = MemMgr_GetItemKey (hList, iItemTest);

      int iCompare = MemMgr_CompareItems (pszKey, pszAlternate);
      if (iCompare <= 0)
         {
         if ((iItemHigh = iItemTest-1) < iItemLow)
            return iItemHigh +1;
         }
      if (iCompare >= 0)
         {
         if ((iItemLow = iItemTest+1) > iItemHigh)
            return iItemLow;
         }
      }

   return 0;
}


void MemMgr_OnListAdd (PMEMCHUNK pCopy)
{
   HWND hList = GetDlgItem (l.hManager, IDC_LIST);

   TCHAR szTime[256];
   FormatTime (szTime, pCopy->dwTick);

   TCHAR szFlags[256];
   LPTSTR pszFlags = szFlags;
   *pszFlags++ = (pCopy->fCPP) ? TEXT('C') : TEXT(' ');
   *pszFlags++ = TEXT(' ');
   *pszFlags++ = (pCopy->fFreed) ? TEXT('F') : TEXT(' ');
   *pszFlags++ = 0;

   TCHAR szExpr[256];
   lstrcpy (szExpr, (pCopy->pszExpr) ? pCopy->pszExpr : TEXT("unknown"));

   LPTSTR pszFile = pCopy->pszFile;
   for (LPTSTR psz = pCopy->pszFile; *psz; ++psz)
      {
      if ((*psz == TEXT(':')) || (*psz == TEXT('\\')))
         pszFile = &psz[1];
      }
   TCHAR szLocation[256];
   if (!pszFile || !pCopy->dwLine)
      lstrcpy (szLocation, TEXT("unknown"));
   else
      wsprintf (szLocation, TEXT("%s, %ld"), pszFile, pCopy->dwLine);

   TCHAR szBytes[256];
   FormatBytes (szBytes, (double)pCopy->cbData);

   TCHAR szAddress[256];
   wsprintf (szAddress, TEXT("0x%08p"), pCopy->pData);

   LPTSTR pszKey = NULL;
   switch (lr.iColSort)
      {
      case 0:  pszKey = (LPTSTR)UlongToPtr(pCopy->dwTick);  break;
      case 1:  pszKey = (LPTSTR)szFlags;        break;
      case 2:  pszKey = (LPTSTR)szExpr;         break;
      case 3:  pszKey = (LPTSTR)szLocation;     break;
      case 4:  pszKey = (LPTSTR)pCopy->cbData;  break;
      case 5:  pszKey = (LPTSTR)pCopy->pData;   break;
      }

   LV_ITEM Item;
   memset (&Item, 0x00, sizeof(Item));
   Item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE | LVIF_IMAGE;
   Item.iItem = MemMgr_PickListInsertionPoint (hList, pszKey);
   Item.iSubItem = 0;
   Item.cchTextMax = 256;
   Item.lParam = (LPARAM)pCopy->pData;

   Item.pszText = szTime;
   DWORD iItem = ListView_InsertItem (hList, &Item);
   ListView_SetItemText (hList, iItem, 1, szFlags);
   ListView_SetItemText (hList, iItem, 2, szExpr);
   ListView_SetItemText (hList, iItem, 3, szLocation);
   ListView_SetItemText (hList, iItem, 4, szBytes);
   ListView_SetItemText (hList, iItem, 5, szAddress);

   delete pCopy;
}


void MemMgr_AddToList (PMEMCHUNK pChunk)
{
   PMEMCHUNK pCopy = new MEMCHUNK;
   memcpy (pCopy, pChunk, sizeof(MEMCHUNK));

   if (pChunk->fInList)
      PostMessage (l.hManager, WM_COMMAND, IDC_LIST_REMOVEADD, (LPARAM)pCopy);
   else
      PostMessage (l.hManager, WM_COMMAND, IDC_LIST_ADD, (LPARAM)pCopy);
   pChunk->fInList = TRUE;
}


void MemMgr_RestoreSettings (void)
{
   lr.rManager.left = 0;
   lr.rManager.top = 0;
   lr.rManager.right = 510;
   lr.rManager.bottom = 440;
   lr.acxColumns[0] = 70;
   lr.acxColumns[1] = 40;
   lr.acxColumns[2] = 100;
   lr.acxColumns[3] = 110;
   lr.acxColumns[4] = 50;
   lr.acxColumns[5] = 80;
   lr.iColSort = 4;
   lr.fSortRev = TRUE;

   HKEY hk;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("Software\\Random\\MemMgr"), &hk) == 0)
      {
      DWORD dwType = REG_BINARY;
      DWORD dwSize = sizeof(lr);
      RegQueryValueEx (hk, TEXT("Settings"), 0, &dwType, (PBYTE)&lr, &dwSize);
      RegCloseKey (hk);
      }
}

void MemMgr_StoreSettings (void)
{
   if (IsWindow (l.hManager))
      {
      GetWindowRect (l.hManager, &lr.rManager);

      HWND hList = GetDlgItem (l.hManager, IDC_LIST);
      if (IsWindow (hList))
         {
         for (size_t ii = 0; ii < 6; ++ii)
            lr.acxColumns[ ii ] = ListView_GetColumnWidth (hList, ii);
         }
      }

   HKEY hk;
   if (RegCreateKey (HKEY_LOCAL_MACHINE, TEXT("Software\\Random\\MemMgr"), &hk) == 0)
      {
      RegSetValueEx (hk, TEXT("Settings"), 0, REG_BINARY, (PBYTE)&lr, sizeof(lr));
      RegCloseKey (hk);
      }
}

void MemMgr_OnInit (void)
{
   DWORD dwFlags = WS_CHILD | WS_TABSTOP | WS_VISIBLE;
   MakeWindow (TEXT("Button"), TEXT("Allocation Statistics"), dwFlags | BS_GROUPBOX, NULL, l.hManager, IDC_BOX_ALLOC);
   MakeWindow (TEXT("Static"), TEXT("C++ Objects:"), dwFlags, NULL, l.hManager, IDC_LABEL_CPP);
   MakeWindow (TEXT("Static"), TEXT("Dynamic:"), dwFlags, NULL, l.hManager, IDC_LABEL_OTHER);
   MakeWindow (TEXT("Static"), TEXT("Combined:"), dwFlags, NULL, l.hManager, IDC_LABEL_TOTAL);
   MakeWindow (TEXT("Static"), TEXT("Tared:"), dwFlags, NULL, l.hManager, IDC_LABEL_TARED);
   MakeWindow (TEXT("Static"), TEXT("Count"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_LABEL_COUNT);
   MakeWindow (TEXT("Static"), TEXT("Size"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_LABEL_SIZE);
   MakeWindow (TEXT("Static"), TEXT("Average"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_LABEL_AVERAGE);
   MakeWindow (TEXT("Static"), TEXT("0"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_CPP_COUNT);
   MakeWindow (TEXT("Static"), TEXT("1"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_OTHER_COUNT);
   MakeWindow (TEXT("Static"), TEXT("2"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_TARED_COUNT);
   MakeWindow (TEXT("Static"), TEXT("3"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_TOTAL_COUNT);
   MakeWindow (TEXT("Static"), TEXT("4"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_CPP_SIZE);
   MakeWindow (TEXT("Static"), TEXT("5"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_OTHER_SIZE);
   MakeWindow (TEXT("Static"), TEXT("6"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_TARED_SIZE);
   MakeWindow (TEXT("Static"), TEXT("7"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_TOTAL_SIZE);
   MakeWindow (TEXT("Static"), TEXT("8"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_CPP_AVERAGE);
   MakeWindow (TEXT("Static"), TEXT("9"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_OTHER_AVERAGE);
   MakeWindow (TEXT("Static"), TEXT("10"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_TARED_AVERAGE);
   MakeWindow (TEXT("Static"), TEXT("11"), dwFlags | SS_CENTER, NULL, l.hManager, IDC_VALUE_TOTAL_AVERAGE);
   MakeWindow (TEXT("Button"), TEXT("Details"), dwFlags | BS_GROUPBOX, NULL, l.hManager, IDC_BOX_DETAILS);
   MakeWindow (WC_LISTVIEW, TEXT(""), dwFlags | LVS_REPORT | LVS_SINGLESEL, NULL, l.hManager, IDC_LIST, WS_EX_CLIENTEDGE);
// MakeWindow (TEXT("Button"), TEXT("Label"), dwFlags, NULL, l.hManager, IDC_LABEL);
// MakeWindow (TEXT("Button"), TEXT("Hidden"), dwFlags, NULL, l.hManager, IDC_HIDE);
   MakeWindow (TEXT("Button"), TEXT("Tare"), dwFlags, NULL, l.hManager, IDC_TARE);
   MakeWindow (TEXT("Button"), TEXT("Restore"), dwFlags, NULL, l.hManager, IDC_RESET);
   MakeWindow (TEXT("Button"), TEXT("Close"), dwFlags, NULL, l.hManager, IDCANCEL);
   MemMgr_OnSize();

   HWND hList = GetDlgItem (l.hManager, IDC_LIST);

   LV_COLUMN Col;
   Col.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_FMT | LVCF_SUBITEM;
   Col.fmt = LVCFMT_LEFT;
   Col.iSubItem = 0;
   Col.cx = lr.acxColumns[0];
   Col.pszText = TEXT("Time");
   ListView_InsertColumn (hList, Col.iSubItem, &Col);

   Col.cx = lr.acxColumns[1];
   Col.pszText = TEXT("Flags");
   Col.iSubItem++;
   ListView_InsertColumn (hList, Col.iSubItem, &Col);

   Col.cx = lr.acxColumns[2];
   Col.pszText = TEXT("Expression");
   Col.iSubItem++;
   ListView_InsertColumn (hList, Col.iSubItem, &Col);

   Col.cx = lr.acxColumns[3];
   Col.pszText = TEXT("Location");
   Col.iSubItem++;
   ListView_InsertColumn (hList, Col.iSubItem, &Col);

   Col.fmt = LVCFMT_RIGHT;
   Col.cx = lr.acxColumns[4];
   Col.pszText = TEXT("Size");
   Col.iSubItem++;
   ListView_InsertColumn (hList, Col.iSubItem, &Col);

   Col.cx = lr.acxColumns[5];
   Col.pszText = TEXT("Address");
   Col.iSubItem++;
   ListView_InsertColumn (hList, Col.iSubItem, &Col);

   EnterCriticalSection (l.pcs);
   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, FALSE, 0);

   for (size_t iChunk = 0; iChunk < l.cChunks; ++iChunk)
      {
      PMEMCHUNK pChunk;
      if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) == NULL)
         continue;

      pChunk->fInList = FALSE;
      if (!pChunk->fTared && !pChunk->fFreed)
         MemMgr_AddToList (pChunk);
      }

   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, TRUE, 0);
   LeaveCriticalSection (l.pcs);

   l.idTimer = SetTimer (l.hManager, 0, cmsecREFRESH, NULL);
}

void MemMgr_OnRefresh (void)
{
   // Fill in the statistics at the top of the manager dialog
   //
   SetDlgItemInt (l.hManager, IDC_VALUE_CPP_COUNT, (INT)l.Stats.cAllocCpp, TRUE);
   SetDlgItemInt (l.hManager, IDC_VALUE_OTHER_COUNT, (INT)l.Stats.cAllocDyna, TRUE);
   SetDlgItemInt (l.hManager, IDC_VALUE_TARED_COUNT, (INT)l.Stats.cAllocTotalTared, TRUE);
   SetDlgItemInt (l.hManager, IDC_VALUE_TOTAL_COUNT, (INT)l.Stats.cAllocTotal, TRUE);

   SetDlgItemBytes (l.hManager, IDC_VALUE_CPP_SIZE, (double)l.Stats.cbAllocCpp);
   SetDlgItemBytes (l.hManager, IDC_VALUE_OTHER_SIZE, (double)l.Stats.cbAllocDyna);
   SetDlgItemBytes (l.hManager, IDC_VALUE_TARED_SIZE, (double)l.Stats.cbAllocTotalTared);
   SetDlgItemBytes (l.hManager, IDC_VALUE_TOTAL_SIZE, (double)l.Stats.cbAllocTotal);

   SetDlgItemBytes (l.hManager, IDC_VALUE_CPP_AVERAGE, (l.Stats.cAllocCpp) ? ((double)l.Stats.cbAllocCpp / l.Stats.cAllocCpp) : 0);
   SetDlgItemBytes (l.hManager, IDC_VALUE_OTHER_AVERAGE, (l.Stats.cAllocDyna) ? ((double)l.Stats.cbAllocDyna / l.Stats.cAllocDyna) : 0);
   SetDlgItemBytes (l.hManager, IDC_VALUE_TARED_AVERAGE, (l.Stats.cAllocTotalTared) ? ((double)l.Stats.cbAllocTotalTared / l.Stats.cAllocTotalTared) : 0);
   SetDlgItemBytes (l.hManager, IDC_VALUE_TOTAL_AVERAGE, (l.Stats.cAllocTotal) ? ((double)l.Stats.cbAllocTotal / l.Stats.cAllocTotal) : 0);
}

void MemMgr_OnLabel (void)
{
   // TODO
}

void MemMgr_OnHide (void)
{
   // TODO
}

void MemMgr_OnReset (void)
{
   l.Stats.cAllocCpp += l.Stats.cAllocCppTared;
   l.Stats.cAllocDyna += l.Stats.cAllocDynaTared;
   l.Stats.cAllocTotal += l.Stats.cAllocTotalTared;
   l.Stats.cbAllocCpp += l.Stats.cbAllocCppTared;
   l.Stats.cbAllocDyna += l.Stats.cbAllocDynaTared;
   l.Stats.cbAllocTotal += l.Stats.cbAllocTotalTared;

   l.Stats.cAllocCppTared = 0;
   l.Stats.cAllocDynaTared = 0;
   l.Stats.cAllocTotalTared = 0;
   l.Stats.cbAllocCppTared = 0;
   l.Stats.cbAllocDynaTared = 0;
   l.Stats.cbAllocTotalTared = 0;

   EnterCriticalSection (l.pcs);
   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, FALSE, 0);

   ListView_DeleteAllItems (GetDlgItem (l.hManager, IDC_LIST));

   for (size_t iChunk = 0; iChunk < l.cChunks; ++iChunk)
      {
      PMEMCHUNK pChunk;
      if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) == NULL)
         continue;

      pChunk->fInList = FALSE;
      pChunk->fTared = FALSE;
      if (!pChunk->fFreed)
         MemMgr_AddToList (pChunk);
      }

   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, TRUE, 0);
   LeaveCriticalSection (l.pcs);

   MemMgr_OnRefresh();
}

void MemMgr_OnTare (void)
{
   l.Stats.cAllocCppTared += l.Stats.cAllocCpp;
   l.Stats.cAllocDynaTared += l.Stats.cAllocDyna;
   l.Stats.cAllocTotalTared += l.Stats.cAllocTotal;
   l.Stats.cbAllocCppTared += l.Stats.cbAllocCpp;
   l.Stats.cbAllocDynaTared += l.Stats.cbAllocDyna;
   l.Stats.cbAllocTotalTared += l.Stats.cbAllocTotal;

   l.Stats.cAllocCpp = 0;
   l.Stats.cAllocDyna = 0;
   l.Stats.cAllocTotal = 0;
   l.Stats.cbAllocCpp = 0;
   l.Stats.cbAllocDyna = 0;
   l.Stats.cbAllocTotal = 0;

   EnterCriticalSection (l.pcs);
   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, FALSE, 0);

   ListView_DeleteAllItems (GetDlgItem (l.hManager, IDC_LIST));

   for (size_t iChunk = 0; iChunk < l.cChunks; ++iChunk)
      {
      PMEMCHUNK pChunk;
      if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) == NULL)
         continue;

      pChunk->fInList = FALSE;
      pChunk->fTared = TRUE;
      }

   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, TRUE, 0);
   LeaveCriticalSection (l.pcs);

   MemMgr_OnRefresh();
}


void MemMgr_OnSort (void)
{
   EnterCriticalSection (l.pcs);
   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, FALSE, 0);

   ListView_DeleteAllItems (GetDlgItem (l.hManager, IDC_LIST));

   for (size_t iChunk = 0; iChunk < l.cChunks; ++iChunk)
      {
      PMEMCHUNK pChunk;
      if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) == NULL)
         continue;

      pChunk->fInList = FALSE;
      if (!pChunk->fTared && !pChunk->fFreed)
         MemMgr_AddToList (pChunk);
      }

   SendMessage (GetDlgItem (l.hManager, IDC_LIST), WM_SETREDRAW, TRUE, 0);
   LeaveCriticalSection (l.pcs);
}


void MemMgr_OnPaint (void)
{
   PAINTSTRUCT ps;
   HDC hdc = BeginPaint (l.hManager, &ps);

   static HBRUSH hbr = (HBRUSH)CreateSolidBrush (GetSysColor (COLOR_BTNFACE));
   FillRect (hdc, &ps.rcPaint, hbr);

   EndPaint (l.hManager, &ps);
}


void MemMgr_OnTimer (void)
{
   if (GetWindowLongPtr (l.hManager, GWLP_USERDATA))
      {
      SetWindowLongPtr (l.hManager, GWLP_USERDATA, 0);
      MemMgr_OnRefresh();
      }
}


void MemMgr_OnDelayedRefresh (void)
{
   SetWindowLongPtr (l.hManager, GWLP_USERDATA, 1);
}


BOOL CALLBACK MemMgr_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_DESTROY:
         MemMgr_StoreSettings();
         l.hManager = NULL;
         KillTimer (hDlg, l.idTimer);
         l.idTimer = -1;
         break;

      case WM_SIZE:
         MemMgr_OnSize();
         MemMgr_StoreSettings();
         break;

      case WM_MOVE:
         MemMgr_StoreSettings();
         break;

      case WM_GETMINMAXINFO:
         LPMINMAXINFO lpmmi;
         lpmmi = (LPMINMAXINFO)lp;
         lpmmi->ptMinTrackSize.x = cxBUTTONS*5 + cxBETWEEN*4 + cBORDER*2 + 8;
         lpmmi->ptMinTrackSize.y = 270;
         return FALSE;

      case WM_PAINT:
         MemMgr_OnPaint();
         break;

      case WM_NOTIFY:
         switch (((NMHDR *)lp)->code)
            {
            case LVN_COLUMNCLICK:
               if (((NM_LISTVIEW*)lp)->iSubItem == lr.iColSort)
                  lr.fSortRev = !lr.fSortRev;
               else
                  {
                  lr.iColSort = ((NM_LISTVIEW*)lp)->iSubItem;
                  lr.fSortRev = FALSE;
                  }
               MemMgr_OnSort();
               MemMgr_StoreSettings();
               break;
            }
         break;

      case WM_TIMER:
         MemMgr_OnTimer();
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;

            case IDC_INITIALIZE:
               MemMgr_OnInit();
               MemMgr_OnRefresh();
               break;

            case IDC_REFRESH:
               MemMgr_OnDelayedRefresh();
               break;

            case IDC_LABEL:
               MemMgr_OnLabel();
               break;

            case IDC_HIDE:
               MemMgr_OnHide();
               break;

            case IDC_RESET:
               MemMgr_OnReset();
               break;

            case IDC_TARE:
               MemMgr_OnTare();
               break;

            case IDC_LIST_REMOVEADD:
               MemMgr_OnListRemove (((PMEMCHUNK)lp)->pData);
               MemMgr_OnListAdd ((PMEMCHUNK)lp);
               break;

            case IDC_LIST_ADD:
               MemMgr_OnListAdd ((PMEMCHUNK)lp);
               break;

            case IDC_LIST_REMOVE:
               MemMgr_OnListRemove ((PVOID)lp);
               break;
            }
         break;
      }

   return (BOOL) DefWindowProc (hDlg, msg, wp, lp);
}


/*
 * MANAGEMENT ROUTINES ________________________________________________________
 *
 */

BOOL MemMgr_Initialize (void)
{
   if (!l.pHeap)
      {
      l.pcs = new CRITICAL_SECTION;
      InitializeCriticalSection (l.pcs);

      l.pHeap = new ALLOCEXPANDARRAY (sizeof(MEMCHUNK));

      REALLOC (l.aBuckets, l.cBuckets, cDEFAULT_BUCKETS, 1);
      if (l.aBuckets)
         {
         for (size_t ii = 0; ii < l.cBuckets; ++ii)
            l.aBuckets[ii].iFirst = iINVALID;
         }
      }

   return (l.pHeap) && (l.aBuckets);
}


void MemMgr_TrackAllocation (PVOID pData, size_t cb, LPSTR pszExpr, LPSTR pszFile, DWORD dwLine, BOOL fSig)
{
   if (!pData)
      return;
   if (!MemMgr_Initialize())
      return;

   EnterCriticalSection (l.pcs);

   // Ensure our hash table will be large enough to handle the new
   // MEMCHUNK we're about to assign.
   //
   if (l.cBuckets < MIN_BUCKETS(l.cChunks+1))
      {
      // Too small! Rebuild the hash table.
      //
      REALLOC (l.aBuckets, l.cBuckets, TARGET_BUCKETS(l.cChunks+1), 1);
      if (l.aBuckets)
         {
         for (size_t ii = 0; ii < l.cBuckets; ++ii)
            l.aBuckets[ii].iFirst = iINVALID;

         for (size_t iChunk = 0; iChunk < l.cChunks; ++iChunk)
            {
            PMEMCHUNK pChunk;
            if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) == NULL)
               continue;

            size_t iBucket = HASH(PtrToUlong(pChunk->pData),l.cBuckets);
            if ((pChunk->iNext = l.aBuckets[iBucket].iFirst) != iINVALID)
               {
               PMEMCHUNK pNext;
               if ((pNext = (PMEMCHUNK)l.pHeap->GetAt(pChunk->iNext)) != NULL)
                  pNext->iPrev = iChunk;
               }
            l.aBuckets[iBucket].iFirst = iChunk;
            pChunk->iPrev = iINVALID;
            }
         }
      }

   // If space was allocated for a trailing signature, write one
   //
   if (fSig)
      *(DWORD *)( (PBYTE)pData + cb ) = dwSIG_AT_END;

   // Prepare a MEMCHUNK entry and shove it in our array
   //
   size_t iChunk = l.cChunks;
   size_t iBucket = HASH(PtrToUlong(pData),l.cBuckets);
   BOOL fLinkIn = TRUE;

   MEMCHUNK Chunk;
   Chunk.pData = pData;
   Chunk.fFreed = FALSE;
   Chunk.cbData = cb;
   Chunk.pszExpr = pszExpr;
   Chunk.pszFile = pszFile;
   Chunk.fCPP = !fSig;
   Chunk.dwLine = dwLine;
   Chunk.dwTick = GetTickCount();
   Chunk.dwEndSig = (fSig) ? dwSIG_AT_END : 0;
   Chunk.fInList = FALSE;
   Chunk.fTared = FALSE;

#ifdef TRACK_FREED
   // Find the memchunk associated with this pData. That's what our
   // hash table is for.
   //
   for (iChunk = l.aBuckets[iBucket].iFirst; iChunk != iINVALID; )
      {
      PMEMCHUNK pChunk;
      if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) != NULL)
         {
         if (pChunk->pData == pData)
            {
            Chunk.iNext = pChunk->iNext;
            Chunk.iPrev = pChunk->iPrev;
            fLinkIn = FALSE;
            break;
            }
         }
      iChunk = (pChunk) ? pChunk->iNext : iINVALID;
      }
   if (iChunk == iINVALID)
      iChunk = l.cChunks;
#endif

   l.cChunks = max (l.cChunks, 1+iChunk);

   if (fLinkIn)
      {
      if ((Chunk.iNext = l.aBuckets[iBucket].iFirst) != iINVALID)
         {
         PMEMCHUNK pNext;
         if ((pNext = (PMEMCHUNK)l.pHeap->GetAt(Chunk.iNext)) != NULL)
            pNext->iPrev = iChunk;
         }
      l.aBuckets[iBucket].iFirst = iChunk;
      Chunk.iPrev = iINVALID;
      }

   if (IsWindow (l.hManager))
      MemMgr_AddToList (&Chunk);

   l.pHeap->SetAt (iChunk, &Chunk);

   if (Chunk.fCPP)
      l.Stats.cAllocCpp ++;
   else // (!Chunk.fCPP)
      l.Stats.cAllocDyna ++;

   if (Chunk.fCPP)
      l.Stats.cbAllocCpp += Chunk.cbData;
   else // (!Chunk.fCPP)
      l.Stats.cbAllocDyna += Chunk.cbData;

   l.Stats.cAllocTotal ++;
   l.Stats.cbAllocTotal += Chunk.cbData;

   LeaveCriticalSection (l.pcs);

   if (IsWindow (l.hManager))
      PostMessage (l.hManager, WM_COMMAND, IDC_REFRESH, 0);
}


BOOL MemMgr_TrackDestruction (PVOID pData, LPSTR pszFile, DWORD dwLine)
{
   if (MemMgr_Initialize())
      {
      EnterCriticalSection (l.pcs);

      // Find the memchunk associated with this pData. That's what our
      // hash table is for.
      //
      size_t iBucket = HASH(PtrToUlong(pData),l.cBuckets);

      PMEMCHUNK pChunk = NULL;
      for (size_t iChunk = l.aBuckets[iBucket].iFirst; iChunk != iINVALID; )
         {
         if ((pChunk = (PMEMCHUNK)l.pHeap->GetAt(iChunk)) == NULL)
            break;
         if (pChunk->pData == pData)
            break;
         if ((iChunk = pChunk->iNext) == iINVALID)
            {
            pChunk = NULL;
            break;
            }
         }

      if (!pChunk)
         {
         MEMCHUNK Sim;
         memset (&Sim, 0x00, sizeof(MEMCHUNK));
         Sim.pData = pData;
         Sim.pszExpr = "(unknown)";
         Sim.pszFile = "(unknown)";
         MemMgr_ShowWarning (&Sim, pszFile, dwLine, TEXT("An invalid memory address was freed."));
         }
      else if (pChunk->dwEndSig && (*(DWORD*)((PBYTE)pData + pChunk->cbData) != pChunk->dwEndSig))
         {
         MemMgr_ShowWarning (pChunk, pszFile, dwLine, TEXT("The trailing signature on a block of memory was overwritten."));
         }
      else if (pChunk->fFreed)
         {
         MemMgr_ShowWarning (pChunk, pszFile, dwLine, TEXT("A block of memory was freed more than once."));
         }

      if (pChunk)
         {
         pChunk->fFreed = TRUE;

         if (IsWindow (l.hManager))
            {
            if (pChunk->fInList)
               MemMgr_RemoveFromList (pChunk);
            }

         if (pChunk->fCPP)
            l.Stats.cAllocCpp --;
         else // (!pChunk->fCPP)
            l.Stats.cAllocDyna --;

         if (pChunk->fCPP)
            l.Stats.cbAllocCpp -= pChunk->cbData;
         else // (!pChunk->fCPP)
            l.Stats.cbAllocDyna -= pChunk->cbData;

         l.Stats.cAllocTotal --;
         l.Stats.cbAllocTotal -= pChunk->cbData;

#ifndef TRACK_FREED

         // Unlink this chunk from the hash table
         //
         if (pChunk->iNext != iINVALID)
            {
            PMEMCHUNK pNext;
            if ((pNext = (PMEMCHUNK)l.pHeap->GetAt(pChunk->iNext)) != NULL)
               pNext->iPrev = pChunk->iPrev;
            }
         if (pChunk->iPrev != iINVALID)
            {
            PMEMCHUNK pPrev;
            if ((pPrev = (PMEMCHUNK)l.pHeap->GetAt(pChunk->iPrev)) != NULL)
               pPrev->iNext = pChunk->iNext;
            }
         if (l.aBuckets[iBucket].iFirst == iChunk)
            l.aBuckets[iBucket].iFirst = pChunk->iNext;

         // Empty this element in the expandarray. That means we need to
         // take the last element and shove it in here--so we'll have do
         // rework the hash table a little bit more.
         //
         if (iChunk != l.cChunks-1)
            {
            PMEMCHUNK pMove;
            if ((pMove = (PMEMCHUNK)l.pHeap->GetAt(l.cChunks-1)) != NULL)
               {
               size_t iBucket = HASH(pMove->pData,l.cBuckets);

               if (pMove->iNext != iINVALID)
                  {
                  PMEMCHUNK pNext;
                  if ((pNext = (PMEMCHUNK)l.pHeap->GetAt(pMove->iNext)) != NULL)
                     pNext->iPrev = iChunk;
                  }
               if (pMove->iPrev != iINVALID)
                  {
                  PMEMCHUNK pPrev;
                  if ((pPrev = (PMEMCHUNK)l.pHeap->GetAt(pMove->iPrev)) != NULL)
                     pPrev->iNext = iChunk;
                  }
               if (l.aBuckets[iBucket].iFirst == (l.cChunks-1))
                  l.aBuckets[iBucket].iFirst = iChunk;

               l.pHeap->SetAt (iChunk, pMove);
               memset (pMove, 0x00, sizeof(MEMCHUNK));
               }
            }

         l.cChunks --;

#endif // TRACK_FREED
         }

      LeaveCriticalSection (l.pcs);
      }

   if (IsWindow (l.hManager))
      PostMessage (l.hManager, WM_COMMAND, IDC_REFRESH, 0);
   return TRUE;
}


/*
 * PUBLIC ROUTINES ____________________________________________________________
 *
 */

void MEMMGR_CALLCONV ShowMemoryManager (void)
{
   if (MemMgr_Initialize())
      {
      if (!IsWindow (l.hManager))
         {
         MemMgr_RestoreSettings();
         l.hManager = MakeWindow (TEXT("Static"), cszTITLE, WS_OVERLAPPED | WS_THICKFRAME | WS_SYSMENU, &lr.rManager, 0, 0);
         SetWindowLongPtr (l.hManager, GWLP_WNDPROC, (LONG)PtrToUlong(MemMgr_DlgProc));
         PostMessage (l.hManager, WM_COMMAND, IDC_INITIALIZE, 0);
         ShowWindow (l.hManager, SW_SHOW);
         }
      }
}


void MEMMGR_CALLCONV WhileMemoryManagerShowing (void)
{
   if (MemMgr_Initialize())
      {
      while (IsWindow (l.hManager))
         {
         MSG msg;
         if (!GetMessage (&msg, NULL, 0, 0))
            break;

         if (!IsDialogMessage (l.hManager, &msg))
            {
            TranslateMessage (&msg);
            DispatchMessage (&msg);
            }
         }
      }
}


BOOL MEMMGR_CALLCONV IsMemoryManagerMessage (MSG *pMsg)
{
   if (IsWindow (l.hManager))
      {
      if (pMsg->hwnd == l.hManager)
         {
//       if (IsDialogMessage (l.hManager, pMsg))
//          return TRUE;
         }
      }
   else if ((pMsg->message == WM_KEYDOWN) && (pMsg->wParam == VK_F8))
      {
      ShowMemoryManager();
      return TRUE;
      }
   return FALSE;
}


PVOID MEMMGR_CALLCONV MemMgr_AllocateMemory (size_t cb, LPSTR pszExpr, LPSTR pszFile, DWORD dwLine)
{
   PVOID pData = GlobalAlloc (GMEM_FIXED, cb + sizeof(DWORD));
   MemMgr_TrackAllocation (pData, cb, pszExpr, pszFile, dwLine, TRUE);
   return pData;
}


void MEMMGR_CALLCONV MemMgr_FreeMemory (PVOID pData, LPSTR pszFile, DWORD dwLine)
{
   if (MemMgr_TrackDestruction (pData, pszFile, dwLine))
      {
      GlobalFree ((HGLOBAL)pData);
      }
}


PVOID MEMMGR_CALLCONV MemMgr_TrackNew (PVOID pData, size_t cb, LPSTR pszExpr, LPSTR pszFile, DWORD dwLine)
{
   MemMgr_TrackAllocation (pData, cb, pszExpr, pszFile, dwLine, FALSE);
   return pData;
}


void MEMMGR_CALLCONV MemMgr_TrackDelete (PVOID pData, LPSTR pszFile, DWORD dwLine)
{
   MemMgr_TrackDestruction (pData, pszFile, dwLine);
}


/*
 * EXPANDARRAY ________________________________________________________________
 *
 * We'll use an EXPANDARRAY to manage our heap of allocation information.
 * Technically I would prefer to use a full HASHLIST here, but I don't want
 * this source file to be dependent upon yet another file. So we'll kinda
 * write a special-purpose hash system.
 *
 */

#define cEXPANDARRAYHEAPELEMENTS  1024
#define cREALLOC_EXPANDARRAYHEAPS   16

ALLOCEXPANDARRAY::ALLOCEXPANDARRAY (size_t cbElement, size_t cElementsPerHeap)
{
   if ((m_cbElement = cbElement) == 0)
      m_cbElement = sizeof(DWORD);
   if ((m_cElementsPerHeap = cElementsPerHeap) == 0)
      m_cElementsPerHeap = cEXPANDARRAYHEAPELEMENTS;

   m_cHeaps = 0;
   m_aHeaps = 0;
}

ALLOCEXPANDARRAY::~ALLOCEXPANDARRAY (void)
{
   if (m_aHeaps)
      {
      for (size_t ii = 0; ii < m_cHeaps; ++ii)
         GlobalFree ((HGLOBAL)(m_aHeaps[ ii ]));
      GlobalFree ((HGLOBAL)m_aHeaps);
      }
}

PVOID ALLOCEXPANDARRAY::GetAt (size_t iElement)
{
   size_t iHeap = iElement / m_cElementsPerHeap;
   size_t iIndex = iElement % m_cElementsPerHeap;
   if ((iHeap >= m_cHeaps) || (!m_aHeaps[iHeap]))
      return NULL;
   return (PVOID)&((PBYTE)m_aHeaps[ iHeap ]->aElements)[ iIndex * m_cbElement ];
}

void ALLOCEXPANDARRAY::SetAt (size_t iElement, PVOID pData)
{
   size_t iHeap = iElement / m_cElementsPerHeap;
   size_t iIndex = iElement % m_cElementsPerHeap;

   if (!REALLOC (m_aHeaps, m_cHeaps, 1+iHeap, cREALLOC_EXPANDARRAYHEAPS))
      return;

   if (!m_aHeaps[ iHeap ])
      {
      size_t cbHeap = sizeof(EXPANDARRAYHEAP) + (m_cElementsPerHeap * m_cbElement);
      if ((m_aHeaps[ iHeap ] = (LPEXPANDARRAYHEAP)GlobalAlloc (GMEM_FIXED, cbHeap)) == NULL)
         return;
      memset (m_aHeaps[ iHeap ], 0x00, cbHeap);
      m_aHeaps[ iHeap ]->aElements = ((PBYTE)m_aHeaps[ iHeap ]) + sizeof(EXPANDARRAYHEAP);
      }

   if (!pData)
      memset (&((PBYTE)m_aHeaps[ iHeap ]->aElements)[ iIndex * m_cbElement ], 0x00, m_cbElement);
   else
      memcpy (&((PBYTE)m_aHeaps[ iHeap ]->aElements)[ iIndex * m_cbElement ], pData, m_cbElement);
}

