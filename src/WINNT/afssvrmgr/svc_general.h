#ifndef SVC_GENERAL_H
#define SVC_GENERAL_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

PVOID Services_LoadPreferences (LPIDENT lpiService);
BOOL Services_SavePreferences (LPIDENT lpiService);

void Services_GuessLogName (LPTSTR pszLogFile, LPTSTR pszServiceName);


#endif

