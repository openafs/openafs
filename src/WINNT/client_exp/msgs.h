/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

#ifndef _MSGS_H_
#define _MSGS_H_

#include "resource.h" 

UINT ShowMessageBox (UINT Id, UINT Button = MB_OK, UINT Help = 0, ...);
CString GetMessageString(UINT Id,...);
void LoadString (CString &Str, UINT id);

#endif    //_MSGS_H_

