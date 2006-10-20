/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef GENERAL_H
#define GENERAL_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   ctALPHABETIC,
   ctNUMERIC,
   ctDATE,
   ctELAPSED
   } COLUMNTYPE;

typedef BOOL (*GetColumnFunction)(ASID idObject, LONG iCol, LPTSTR pszText, LPSYSTEMTIME pstDate, LONG *pcsec, COLUMNTYPE *pcType);


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL fIsValidDate (LPSYSTEMTIME pst);

void FormatElapsedSeconds (LPTSTR pszText, LONG csec);

LPTSTR CreateNameList (LPASIDLIST pAsidList, int idsHeader = 0);

void GetLocalSystemTime (LPSYSTEMTIME pst);

void FormatServerKey (LPTSTR psz, PBYTE pKey);
BOOL ScanServerKey (PBYTE pKey, LPTSTR psz);

int CALLBACK General_ListSortFunction (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2);

void AppendUID (LPTSTR psz, int uid);

LPTSTR GetEditText (HWND hEdit);

BOOL fIsMachineAccount (ASID idAccount);
BOOL fIsMachineAccount (LPTSTR pszName);


#endif

