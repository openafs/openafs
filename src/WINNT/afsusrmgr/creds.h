#ifndef CREDS_H
#define CREDS_H

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

int OpenCellDialog (void);
BOOL NewCredsDialog (void);
void CheckForExpiredCredentials (void);
BOOL CheckCredentials (BOOL fComplain);
void ShowCurrentCredentials (void);

#endif

