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

