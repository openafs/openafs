/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRCLIENTPING_H
#define TAAFSADMSVRCLIENTPING_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void StartPingThread (UINT_PTR idClient);
void StopPingThread (UINT_PTR idClient);

void StartCallbackThread (void);
void StopCallbackThread (void);


#endif // TAAFSADMSVRCLIENTPING_H

