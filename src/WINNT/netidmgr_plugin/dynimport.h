/*
 * Copyright (c) 2005 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* $Id$ */

#ifndef __KHIMAIRA_DYNIMPORT_H
#define __KHIMAIRA_DYNIMPORT_H

/* Dynamic imports */
#include<khdefs.h>
#include<tlhelp32.h>
#include<delayload_library.h>
#include<krbcompat_delayload.h>

#if defined(_WIN32_WINNT)
#  if (_WIN32_WINNT < 0x0501)
#    define AFS_WIN32_WINNT _WIN32_WINNT
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0501
#  endif
#else
#  define _WIN32_WINNT 0x0501
#endif

#include<ntsecapi.h>
#if defined(AFS_WIN32_WINNT)
#undef _WIN32_WINNT
#define _WIN32_WINNT AFS_WIN32_WINNT
#undef AFS_WIN32_WINNT
#endif

#ifndef FAR
#define FAR
#endif

///////////////////////////////////////////////////////////////////////////////

#define SERVICE_DLL   "advapi32.dll"
#define SECUR32_DLL   "secur32.dll"
#define PSAPIDLL      "psapi.dll"

//////////////////////////////////////////////////////////////////////////////

extern  DWORD AfsAvailable;

khm_int32 init_imports(void);
khm_int32 exit_imports(void);

#endif
