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


LanguageNames=(Japanese=1:MSG000001)
MessageIdTypedef=unsigned

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=Japanese
AFS サーバー・イベント・ログ・テスト・メッセージ。
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=Japanese
AFS サーバー・イベント・ログ・テスト・メッセージ (str1: %1, str2: %2)。
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=Japanese
AFS サーバー・プロセスが代入に失敗しました: 行 %1 ファイル %2。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=Japanese
%1 が AFS ソフトウェアのインストール・ディレクトリーを見つけられませんでした。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=Japanese
%1 が Windows Socket ライブラリーを初期化できませんでした。
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=Japanese
AFS BOS 制御サービスが始動しました。
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=Japanese
AFS BOS 制御サービスが停止しました。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=Japanese
AFS BOS 制御サービスがシステム SCM と通信できません。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=Japanese
AFS BOS 制御サービスがイベント・ハンドラーを登録できません。AFS サーバー・ソフトウェアが正しく構成されていない可能性があります。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=Japanese
AFS BOS 制御サービスが必要なシステム・リソースを取得できません。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=Japanese
AFS BOS 制御サービスが内部エラーを検出しました。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=Japanese
AFS BOS 制御サービスが AFS ソフトウェアのインストール・ディレクトリーを見つけられませんでした。AFS サーバー・ソフトウェアが正しく構成されていない可能性があります。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=Japanese
AFS BOS 制御サービスが AFS bosserver を始動または再始動できませんでした。
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=Japanese
AFS BOS 制御サービスが AFS bosserver を停止できませんでした。AFS サーバー・プロセスをすべて手動で停止する必要があります (AFS bosserver に afskill コマンドで SIGQUIT シグナルを送信してみてください)。
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=Japanese
AFS BOS 制御サービスが AFS bosserver の停止待ちを中止しました。サービスを再始動する前に、すべての AFS サーバー・プロセスが停止していることを確かめてください。
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=Japanese
AFS BOS 制御サービスが AFS bosserver を再始動しています。
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=Japanese
AFS BOS 制御サービスが、AFS bosserver が再始動要求なしで終了したことを検出しました。
.



;
;#endif /* OPENAFS_AFSEVENT_H */
