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
LanguageNames=(Chinese_Traditional=1:MSG000001)

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=Chinese_Traditional
AFS 伺服器事件日誌測試訊息。
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=Chinese_Traditional
AFS 伺服器事件日誌測試訊息 (str1: %1, str2: %2)。
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=Chinese_Traditional
AFS 伺服器處理失效確認：在檔案 %2 的第 %1 行。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=Chinese_Traditional
%1 無法找到 AFS 軟體安裝目錄。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=Chinese_Traditional
%1 無法起始設定 Windows Sockets 程式庫。
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=Chinese_Traditional
已啟動 AFS BOS 控制服務。
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=Chinese_Traditional
已停止 AFS BOS 控制服務。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=Chinese_Traditional
AFS BOS 控制服務無法與系統 SCM 通信。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=Chinese_Traditional
AFS BOS 控制服務無法登錄事件處理常式。AFS 伺服器軟體的架構可能不正確。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=Chinese_Traditional
AFS BOS 控制服務無法取得必要的系統資源。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=Chinese_Traditional
AFS BOS 控制服務發生內部錯誤。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=Chinese_Traditional
AFS BOS 控制服務無法找到 AFS 軟體安裝目錄。AFS 伺服器軟體的架構可能不正確。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=Chinese_Traditional
AFS BOS 控制服務無法啟動或重新啟動 AFS 主伺服器。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=Chinese_Traditional
AFS BOS 控制服務無法停止 AFS 主伺服器。所有的 AFS 伺服器處理都必須以手動方式停止
（試著透過 fskill 指令傳送 SIGQUIT 訊號到 AFS 主伺服器）。
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=Chinese_Traditional
AFS BOS 控制服務放棄等候 AFS 主伺服器停止。請檢查所有 AFS 伺服器處理都已在重新啟動服務之前停止。
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=Chinese_Traditional
AFS BOS 控制服務正在重新啟動 AFS 主伺服器。
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=Chinese_Traditional
AFS BOS 控制服務偵測到 AFS 主伺服器結束，並且未要求重新啟動。
.



;
;#endif /* OPENAFS_AFSEVENT_H */
