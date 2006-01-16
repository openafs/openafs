;//
;// afsd_eventmessages.mc
;//
;// This file contains the message definitions for the
;// TransarcAFSDaemon service to facilitate system event logging.
;//
;//
;
;#ifndef __AFSD_EVENTMESSAGES_H_
;#define __AFSD_EVENTMESSAGES_H_ 1

MessageIdTypedef=DWORD

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0:FACILITY_SYSTEM
               Runtime=0x2:FACILITY_RUNTIME
               Stubs=0x3:FACILITY_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
              )

LanguageNames=(English=0x409:MSG00409)

MessageId=0x1
Severity=Informational
Facility=System
SymbolicName=MSG_TIME_FLUSH_PER_VOLUME
Language=English
Elapsed time to flush AFS volume <%1> = %2 milliseconds.
.

MessageId=
Severity=Informational
Facility=System
SymbolicName=MSG_TIME_FLUSH_TOTAL
Language=English
Total elapsed time to flush %1 AFS volume(s) = %2 milliseconds.
.

MessageId=
Severity=Error
Facility=System
SymbolicName=MSG_FLUSH_NO_SHARE_NAME
Language=English
Cannot get AFS share name to flush volumes.
.

MessageId=
Severity=Error
Facility=System
SymbolicName=MSG_FLUSH_BAD_SHARE_NAME
Language=English
Invalid share name %1; cannot flush volumes.
.

MessageId=
Severity=Error
Facility=System
SymbolicName=MSG_FLUSH_NO_MEMORY
Language=English
Insufficient memory to flush volumes.
.

MessageId=
Severity=Error
Facility=System
SymbolicName=MSG_FLUSH_OPEN_ENUM_ERROR
Language=English
Cannot open enumeration of network resources: %1
.

MessageId=
Severity=Error
Facility=System
SymbolicName=MSG_FLUSH_ENUM_ERROR
Language=English
Cannot enumerate network resources: %1
.

MessageId=
Severity=Warning
Facility=System
SymbolicName=MSG_FLUSH_FAILED
Language=English
Failed to flush volume %1.
.

MessageId=
Severity=Error
Facility=System
SymbolicName=MSG_FLUSH_IMPERSONATE_ERROR
Language=English
Failed to impersonate logged-on user.
.

MessageId=
Severity=Warning
Facility=System
SymbolicName=MSG_FLUSH_UNEXPECTED_EVENT
Language=English
Flush volumes thread received unrecognized event.
.


MessageId=
Severity=Warning
Facility=System
SymbolicName=MSG_SMB_SEND_PACKET_FAILURE
Language=English
Unable to Send SMB Packet: %1.
.


MessageId=
Severity=Warning
Facility=System
SymbolicName=MSG_UNEXPECTED_SMB_SESSION_CLOSE
Language=English
Unexpected SMB Session Close: %1.
.


;#endif /* __AFSD_EVENTMESSAGES_H_ 1 */
