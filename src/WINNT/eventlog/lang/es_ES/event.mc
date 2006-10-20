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
Mensaje de prueba de registro cronol=gico de eventos de servidor de AFS.
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=English
Mensaje de prueba de registro cronol=gico de eventos de servidor de AFS (str1: %1, str2: %2).
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=English
Ha resultado an=mala una afirmaci=n en un proceso de servidor de AFS: lfnea %1 en el archivo %2.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=English
%1 no ha podido localizar el directorio de instalaci=n de software de AFS.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=English
%1 no ha podido inicializar la biblioteca de Windows Sockets.
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=English
Se ha iniciado el servicio de control de BOS de AFS.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=English
Se ha detenido el servicio de control de BOS de AFS.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=English
El servicio de control de BOS de AFS no ha podido comunicarse con el SCM de sistema.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=English
El servicio de control de BOS de AFS no ha podido registrar un manejador de eventos. Es posible que el software de servidor de AFS se haya configurado indebidamente.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=English
El servicio de control de BOS de AFS no ha podido obtener los recursos de sistema necesarios.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=English
El servicio de control de BOS de AFS ha sufrido un error interno.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=English
El servicio de control de BOS de AFS no ha podido localizar el
directorio de instalaci=n de software de AFS. Es posible que el
software de servidor de AFS se haya configurado indebidamente.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=English
El servicio de control de BOS de AFS no ha podido iniciar o reiniciar el bosserver de AFS.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=English
El servicio de control de BOS de AFS no ha podido
detener el bosserver de AFS. Todos los procesos de servidor de AFS
deben detenerse manualmente (pruebe a enviar al bosserver
de AFS una se±al de SIGQUIT por medio del mandato afskill).
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=English
El servicio de control de BOS de AFS ha dejado de esperar
la detenci=n del bosserver de AFS. Compruebe que se han detenido
todos los procesos de servidor de AFS antes de reiniciar el servicio.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=English
El servicio de control de BOS de AFS estﬂ reiniciando el bosserver de AFS.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=English
El servicio de control de BOS de AFS ha detectado que el
bosserver de AFS ha salido sin solicitar un reinicio.
.



;
;#endif /* OPENAFS_AFSEVENT_H */
