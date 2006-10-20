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

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=English
Mensagem de teste do log de eventos para o servidor de AFS.
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=English
Mensagem de teste do log de eventos para o servidor do AFS (str1: %1, str2: %2).
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=English
Um processo para o servidor do AFS falhou uma assertiva: linha %1 no arquivo %2.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=English
%1 não conseguiu localizar o diretório de instalação para o software de AFS.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=English
%1 não conseguiu inicializar a biblioteca de Sockets Windows.
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=English
O serviço de controle BOS do AFS foi iniciado.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=English
O serviço de controle BOS do AFS foi interrompido.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=English
O serviço de controle BOS do AFS não consegue se comunicar com o SCM do sistema.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=English
O serviço de controle BOS do AFS não consegue registrar um manipulador de eventos. O software do servidor do AFS pode estar configurado incorretamente.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=English
O serviço de controle BOS do AFS não consegue obter recursos necessários do sistema.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=English
O serviço de controle BOS do AFS sofreu um erro interno.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=English
O serviço de controle BOS do AFS não conseguiu localizar o diretório de instalação para o software do AFS. O software do servidor do AFS pode estar configurado incorretamente.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=English
O serviço de controle BOS do AFS não conseguiu iniciar ou reiniciar o AFS bosserver.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=English
O serviço de controle BOS do AFS não conseguiu interromper o AFS bosserver. Todos os processos para o servidor do AFS precisam ser interrompidos manualmente (tente enviar um sinal SIGQUIT ao AFS bosserver através do comando afskill).
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=English
O serviço de controle BOS do AFS desistiu de aguardar pela parada do AFS bosserver. Verifique que todos os processos para o servidor do AFS tenham parado antes de reiniciar o serviço.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=English
O serviço de controle BOS do AFS está reiniciando o AFS bosserver.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=English
O serviço de controle BOS do AFS detectou que o AFS bosserver saiu sem pedir um reinício.
.



;
;#endif /* OPENAFS_AFSEVENT_H */
