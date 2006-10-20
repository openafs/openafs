;/* Copyright 2000, International Business Machines Corporation and others.
; * All Rights Reserved.
; *
; * This software has been released under the terms of the IBM Public
; * License.  For details, see the LICENSE file in the top-level source
; * directory or online at http://www.openafs.org/dl/license10.html
; * event.mc --(mc)--> event.[h|rc] --(logevent.h + event.h)--> afsevent.h
; */
;
;#ifndef OPENAFS_AFSEVENT_H
;#define OPENAFS_AFSEVENT_H
;
;
;/* AFS event.mc format.
; *
; * AFS event messages are grouped by category.  The MessageId of the
; * first message in a given category specifies the starting identifier
; * range for that category; the second and later messages in a category
; * do NOT specify a MessageId value and thus receive the value of the
; * previous message plus one.
; *
; * To add a new message to an existing category, append it to the end of
; * that category.  To create a new category, provide an appropriate
; * comment line and specify a non-conflicting MessageId for the first
; * message in the new category.
; */
;


MessageIdTypedef=unsigned
LanguageNames=(German=1:MSG000001)
;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=German
Testnachricht für das AFS-Server-Ereignisprotokoll.
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=German
Testnachricht für das AFS-Server-Ereignisprotokoll (Zeichenfolge1: %1, Zeichenfolge2: %2).
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=German
Ein AFS-Server-Prozeß konnte kein Assert durchführen: Zeile %1 in Datei %2.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=German
%1 konnte das Installationsverzeichnis der AFS-Software nicht finden.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=German
%1 konnte die Windows Sockets-Bibliothek nicht initialisieren.
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=German
Der AFS BOS-Steuerungsservice wurde gestartet.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=German
Der AFS BOS-Steuerungsservice wurde beendet.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=German
Der AFS BOS-Steuerungsservice kann keine Daten zum System-SCM übertragen.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=German
Der AFS BOS-Steuerungsservice kann keine Ereignissteuerroutine registrieren.  Die AFS-Server-Software ist möglicherweise nicht einwandfrei konfiguriert.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=German
Der AFS BOS-Steuerungsservice kann die erforderlichen Systemressourcen nicht erhalten.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=German
Im AFS BOS-Steuerungsservice ist ein interner Fehler aufgetreten.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=German
Der AFS BOS-Steuerungsservice konnte das Installationsverzeichnis der AFS-Software nicht finden. Die AFS-Server-Software ist möglicherweise nicht einwandfrei konfiguriert.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=German
Der AFS BOS-Steuerungsservice konnte den AFS BOS-Server nicht oder nicht erneut starten.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=German
Der AFS BOS-Steuerungsservice konnte den AFS BOS-Server nicht beenden. Alle AFS-Server-Prozesse müssen manuell beendet werden (versuchen Sie, über den Befehl afskill dem AFS BOS-Server ein SIGQUIT-Signal zu senden).
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=German
Der AFS BOS-Steuerungsservice wartet nicht mehr länger auf das Beenden des AFS BOS-Servers. Überprüfen Sie vor dem Neustart des Service, ob alle AFS-Server-Prozesse beendet wurden.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=German
Der AFS BOS-Steuerungsservice startet den AFS BOS-Server neu.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=German
Der AFS BOS-Steuerungsservice hat festgestellt, daß der AFS BOS-Server ohne Anforderung eines Neustarts beendet wurde.
.



;
;#endif /* OPENAFS_AFSEVENT_H */
