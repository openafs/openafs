/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
#if !defined(SHARE_H__CF7462A3_F999_11D3_A374_00105A6BCA62__INCLUDED_)
#define SHARE_H__CF7462A3_F999_11D3_A374_00105A6BCA62__INCLUDED_
#define WM_MY_TRAY_NOTIFICATION WM_APP+1
#define WM_DIRTIMER WM_USER+2		//MUST BE DIFFERENT FROM CTRAYICON 
#define WM_AUTTIMER WM_USER+3
#define WM_PROGRESSTIMER WM_USER+4
#define WM_PROGRESSPARM WM_USER+5	//SHARED BETWEEN PROGBAR 
#define WM_LOG WM_USER+6			//Message Log
#define WSA_EVENT WM_USER+7			//Socket messages
#define WM_UIONPARM WM_USER+8
#define WM_UIPING WM_USER+9			//INDication from UIThread that there is a socket connection to server
#define WM_UICONNECT WM_USER+10		//UIThread do a connect to socket
#define WM_UIDISCONNECT WM_USER+11	//UIthread do a disconnect
#define WM_PINGRETURN WM_USER+12	//return message from a ping
#define WM_CONNECTRETURN WM_USER+13	//return message from connect
#define WM_RESUMEDELAY WM_USER+14	//used to delay after power resume
#define WM_ERRORMSG WM_USER+15		//Error Display Message
#define ProgFShow 100		//since modes are 1-7 skip thoes numbers
#define ProgFHide 102
#define ProgFOpt 103
#define ProgFNext 104
#define ProgFSetTitle 105

#define SOCKETIO 2000		//maximum IO time for sockets in milli-seconds

#endif
