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

#include<windows.h>
#include<khdefs.h>
#include<kherror.h>
#include<dynimport.h>

DWORD     AfsAvailable = 0;

khm_int32 init_imports(void) {
    OSVERSIONINFO osvi;
    BOOL imp_rv = 1;

#define CKRV if(!imp_rv) goto _err_ret

    imp_rv = DelayLoadLibrary(SERVICE_DLL);
    CKRV;

    imp_rv = DelayLoadLibrary(SECUR32_DLL);
    CKRV;

    memset(&osvi, 0, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    if(osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        // Windows NT
        imp_rv = DelayLoadLibrary(PSAPIDLL);
        CKRV;
    }

    AfsAvailable = TRUE; //afscompat_init();

    return KHM_ERROR_SUCCESS;

 _err_ret:
    return KHM_ERROR_NOT_FOUND;
}

khm_int32 exit_imports(void) {
    //afscompat_close();

    return KHM_ERROR_SUCCESS;
}
