/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef INTERNAL_H
#define INTERNAL_H

#include "worker.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cREALLOC_SERVERS    16	// allocate space for 16 servers at once

#define cREALLOC_SERVICES    8	// allocate space for 8 svcs at once

#define cREALLOC_AGGREGATES 16	// allocate space for 16 aggs at once

#define cREALLOC_FILESETS   32	// allocate space for 32 sets at once


/*
 * VARIABLES __________________________________________________________________
 *
 */

extern size_t cRefreshAllReq;

extern BOOL   fLongServerNames;

extern DWORD  dwWant;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

LPCRITICAL_SECTION AfsClass_GetCriticalSection (void);

void AfsClass_ElapsedTimeToSeconds (ULONG *pcSeconds, LPSYSTEMTIME pet);

void AfsClass_ParseRecurringTime (BOOL *pfEver, LPSYSTEMTIME pst, LPTSTR pszTime);
void AfsClass_FormatRecurringTime (LPTSTR pszTarget, SYSTEMTIME *pst);

double AfsClass_FileTimeToDouble (FILETIME *pft);

void AfsClass_SplitFilename (LPSTR pszDirectoryA, LPSTR pszFilenameA, LPTSTR pszFullName);

void AfsClass_SystemTimeToRestartTime (bos_RestartTime_p prt, BOOL fEnable, LPSYSTEMTIME pst);
void AfsClass_RestartTimeToSystemTime (BOOL *pfEnable, LPSYSTEMTIME pst, bos_RestartTime_p prt);

void AfsClass_GenFullUserName (LPTSTR pszTarget, LPCTSTR pszPrincipal, LPCTSTR pszInstance);

#endif

