#ifndef CREDS_H
#define CREDS_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL IsServiceRunning (void);
BOOL IsServicePersistent (void);
BOOL IsServiceConfigured (void);

int GetCurrentCredentials (void);

int DestroyCurrentCredentials (LPCTSTR pszCell);

int ObtainNewCredentials (LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword);

int GetDefaultCell (LPTSTR pszCell);

void GetGatewayName (LPTSTR pszGateway);

BOOL Creds_OpenLibraries (void);
void Creds_CloseLibraries (void);


#endif

