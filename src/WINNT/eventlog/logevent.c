/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stddef.h>
#include <stdarg.h>
#include <WINNT/afsreg.h>

#include "logevent.h"

/* AFS NT alternative event logging functions.
 *
 * All event logging functions take a variable number of arguments,
 * with the trailing (variable) arguments being a set of message text
 * insertion strings terminated by a 0 (NULL) argument.
 * Status values are logged as raw data if the status is not zero.
 *
 * All functions return 0 if message is logged successfully and -1 otherwise.
 *
 * Example: ReportErrorEventAlt(eventId, status, filenameString, 0);
 */

static int
ReportEventAlt(WORD eventType,
	       DWORD eventId,
	       WORD insertStrCount,
	       char **insertStr,
	       int status)
{
    HANDLE eventLog = NULL;
    BOOL result;

    /* open write handle to the event log */
    eventLog = RegisterEventSource(NULL /* local machine */,
				   AFSREG_SVR_APPLOG_SUBKEY);
    if (eventLog == NULL) {
	return -1;
    }

    /* log the event */
    result = ReportEvent(eventLog, eventType, 0 /* category */, eventId,
			 NULL /* SID */,
			 insertStrCount, (status ? sizeof(status) : 0),
			 insertStr, &status);

    (void) DeregisterEventSource(eventLog);

    return (result ? 0 : -1);
}


int
ReportErrorEventAlt(unsigned int eventId, int status, char *str, ...)
{
    va_list strArgs;
    char *iStrings[AFSEVT_MAXARGS];
    int i;

    /* construct iStrings (insertion strings) array */
    va_start(strArgs, str);
    for (i = 0; str && (i < AFSEVT_MAXARGS); i++) {
        iStrings[i] = str;
	str = va_arg(strArgs, char *);
    }
    va_end(strArgs);

    if (str) {
	/* too many args */
	return -1;
    }

    /* report error event */
    return ReportEventAlt(EVENTLOG_ERROR_TYPE,
			  (DWORD) eventId, (WORD) i, iStrings, status);
}


int
ReportWarningEventAlt(unsigned int eventId, int status, char *str, ...)
{
    va_list strArgs;
    char *iStrings[AFSEVT_MAXARGS];
    int i;

    /* construct iStrings (insertion strings) array */
    va_start(strArgs, str);
    for(i = 0; str && (i < AFSEVT_MAXARGS); i++) {
        iStrings[i] = str;
	str = va_arg(strArgs, char *);
    }
    va_end(strArgs);

    if (str) {
	/* too many args */
	return -1;
    }

    /* report warning event */
    return ReportEventAlt(EVENTLOG_WARNING_TYPE,
			  (DWORD) eventId, (WORD) i, iStrings, status);
}


int
ReportInformationEventAlt(unsigned int eventId, char *str, ...)
{
    va_list strArgs;
    char *iStrings[AFSEVT_MAXARGS];
    int i;

    /* construct iStrings (insertion strings) array */
    va_start(strArgs, str);
    for(i = 0; str && (i < AFSEVT_MAXARGS); i++) {
        iStrings[i] = str;
	str = va_arg(strArgs, char *);
    }
    va_end(strArgs);

    if (str) {
	/* too many args */
	return -1;
    }

    /* report information event */
    return ReportEventAlt(EVENTLOG_INFORMATION_TYPE,
			  (DWORD) eventId, (WORD) i, iStrings, 0);
}
