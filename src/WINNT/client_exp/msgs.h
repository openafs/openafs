/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _MSGS_H_
#define _MSGS_H_

#include "resource.h" 

UINT ShowMessageBox (UINT Id, UINT Button = MB_OK, UINT Help = 0, ...);
CString GetMessageString(UINT Id,...);
void LoadString (CString &Str, UINT id);

#endif    //_MSGS_H_

