#ifndef SHORTCUT_H
#define SHORTCUT_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Shortcut_Init (void);
void Shortcut_Exit (void);
BOOL Shortcut_Create (LPTSTR pszTarget, LPCTSTR pszSource, LPTSTR pszDesc = NULL);
void Shortcut_FixStartup (LPCTSTR pszLinkName, BOOL fAutoStart);


#endif

