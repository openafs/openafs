#ifndef SUBSET_H
#define SUBSET_H


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef struct
   {
   TCHAR szSubset[ cchNAME ];	// subset name ("" if unspecified)
   BOOL fModified;	// TRUE if subset contents changed
   LPTSTR pszMonitored;	// allocated multistring of servers
   LPTSTR pszUnmonitored;	// allocated multistring of servers
   } SUBSET, *LPSUBSET;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL     Subsets_fMonitorServer (LPSUBSET sub, LPIDENT lpiServer);
LPSUBSET Subsets_SetMonitor (LPSUBSET sub, LPIDENT lpiServer, BOOL fMonitor);

void ShowSubsetsDialog (void);

BOOL     Subsets_SaveIfDirty (LPSUBSET sub);

BOOL     Subsets_EnumSubsets (LPTSTR pszCell, size_t iIndex, LPTSTR pszSubset);
LPSUBSET Subsets_LoadSubset  (LPTSTR pszCell, LPTSTR pszSubset);
BOOL     Subsets_SaveSubset  (LPTSTR pszCell, LPTSTR pszSubset, LPSUBSET sub);
LPSUBSET Subsets_CopySubset  (LPSUBSET sub, BOOL fMakeIfNULL = FALSE);
void     Subsets_FreeSubset  (LPSUBSET sub);


#endif

