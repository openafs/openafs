#ifndef TAAFSADMSVRCLIENTBIND_H
#define TAAFSADMSVRCLIENTBIND_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL ADMINAPI BindToAdminServer (LPCTSTR pszAddress, BOOL fWait, DWORD *pidClient, ULONG *pStatus);
BOOL ADMINAPI UnbindFromAdminServer (DWORD idClient, ULONG *pStatus);
BOOL ADMINAPI ForkNewAdminServer (ULONG *pStatus);

LPCTSTR ADMINAPI ResolveAddress (LPCTSTR pszAddress);


#endif // TAAFSADMSVRCLIENTBIND_H

