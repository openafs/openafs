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
LanguageNames=(Korean=1:MSG000001)

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=Korean
AFS 서버 이벤트 로그 검사 메시지.
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=Korean
AFS 서버 이벤트 로그 검사 메시지(str1: %1, str2: %2).
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=Korean
AFS 서버 프로세스가 승인에 실패했습니다: %2 파일의 %1 행.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=Korean
%1이(가) AFS 소프트웨어 설치 디렉토리를 찾지 못했습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=Korean
%1이(가) Windows 소켓 라이브러리를 초기화하지 못했습니다.
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=Korean
AFS BOS 제어 서비스가 시작되었습니다.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=Korean
AFS BOS 제어 서비스가 정지되었습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=Korean
AFS BOS 제어 서비스가 SCM 시스템과 통신할 수 없습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=Korean
AFS BOS 제어 서비스가 이벤트 처리기를 등록할 수 없습니다.
AFS 서버 소프트웨어가 제대로 구성되지 않은 것일 수도 있습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=Korean
AFS BOS 제어 서비스가 필요한 시스템 자원을 얻을 수 없습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=Korean
AFS BOS 제어 서비스에 내부 오류가 발생했습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=Korean
AFS BOS 제어 서비스가 AFS 소프트웨어 설치 디렉토리를 찾지 못했습니다.
AFS 서버 소프트웨어가 제대로 구성되지 않은 것일 수도 있습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=Korean
AFS BOS 제어 서비스가 AFS bosserver를 시작 또는 재시작하지 못했습니다.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=Korean
AFS BOS 제어 서비스가 AFS bosserver를 정지하지 못했습니다.
모든 AFS 서버 프로세스를 수동으로 정지시켜야 합니다(afskill 명령을 통해 SIGQUIT 신호를 AFS bosserver로 보내 보십시오).
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=Korean
AFS BOS 제어 서비스가 AFS bosserver가 중지될 때까지 기다리는 것을 포기했습니다.
모든 AFS 서버 프로세스가 서비스를 재시작하기 전에 정지되었는지 확인하십시오.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=Korean
AFS BOS 제어 서비스가 AFS bosserver를 재시작하고 있습니다.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=Korean
AFS BOS 제어 서비스가 AFS bosserver가 재시작을 요청하지 않고 종료되었음을 감지했습니다.
.



;
;#endif /* OPENAFS_AFSEVENT_H */
