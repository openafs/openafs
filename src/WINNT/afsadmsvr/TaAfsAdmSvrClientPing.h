#ifndef TAAFSADMSVRCLIENTPING_H
#define TAAFSADMSVRCLIENTPING_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void StartPingThread (DWORD idClient);
void StopPingThread (DWORD idClient);

void StartCallbackThread (void);
void StopCallbackThread (void);


#endif // TAAFSADMSVRCLIENTPING_H

