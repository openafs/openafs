// This header contains needed definitions and declarations for afsd's
// event logging functions.

#ifndef __AFSD_EVENTLOG_H_
#define __AFSD_EVENTLOG_H_ 1

// Include the event log message definitions.
#include "afsd_eventmessages.h"

VOID LogEventMessage(WORD wEventType, DWORD dwEventID, DWORD dwMessageID);
VOID LogEvent(WORD wEventType, DWORD dwEventID, ...);
#endif /* __AFSD_EVENTLOG_H_ */
