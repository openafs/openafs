#ifndef PREFS_H
#define PREFS_H

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void ErasePreferences (LPTSTR pszCell = NULL, LPTSTR pszServer = NULL);
BOOL RestorePreferences (LPIDENT, LPVOID, size_t);
BOOL StorePreferences (LPIDENT, LPVOID, size_t);

HKEY OpenSubsetsKey (LPTSTR pszCell, BOOL fCreate); // fCreate: 0=open, 1=create, 2=delete
HKEY OpenSubsetsSubKey (LPTSTR pszCell, LPTSTR pszSubset, BOOL fCreate);


#endif

