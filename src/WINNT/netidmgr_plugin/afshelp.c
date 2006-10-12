/*
 * Copyright (c) 2004 Massachusetts Institute of Technology
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

#define NOSTRSAFE

#include<afscred.h>
#include<shlwapi.h>
#include<htmlhelp.h>
#include<psapi.h>

#ifdef DEBUG
#include<assert.h>
#endif

#include<strsafe.h>

static wchar_t helpfile[MAX_PATH] = L"";

/* can only be called from the UI thread */
HWND
afs_html_help(HWND caller,
              wchar_t * postfix,
              UINT cmd,
              DWORD_PTR data) {

    wchar_t fullp[MAX_PATH + MAX_PATH];

    if (!helpfile[0]) {
        DWORD rv;

        rv = GetModuleFileNameEx(GetCurrentProcess(),
                                 hInstance,
                                 helpfile,
                                 ARRAYLENGTH(helpfile));
#ifdef DEBUG
        assert(rv != 0);
#endif
        PathRemoveFileSpec(helpfile);
        PathAppend(helpfile, AFS_HELPFILE);
    }

    StringCbCopy(fullp, sizeof(fullp), helpfile);
    if (postfix)
        StringCbCat(fullp, sizeof(fullp), postfix);

    return HtmlHelp(caller, fullp, cmd, data);
}
