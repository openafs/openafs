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

#include<windows.h>
#include<msiquery.h>
#include<tchar.h>

static
const TCHAR * const dword_props[] = {
    _TEXT("OPENAFSVERSIONMAJOR"),
    _TEXT("OPENAFSVERSIONMINOR"),
    _TEXT("KFWVERSIONMAJOR")
};

static void strip_decoration(TCHAR * str, int cchlen) {
    int i;

    if (str[0] != _T('#') || cchlen < 1)
        return;

    for (i=1; i < cchlen && str[i]; i++) {
        str[i-1] = str[i];
    }

    str[i-1] = _T('\0');
}

UINT __stdcall StripRegDecoration(MSIHANDLE hInstall) {
    TCHAR propbuffer[16];      /* we are looking for string
                                  representations of DOWRDs.  They
                                  can't be longer than this. */
    DWORD cch_buffer;
    UINT rv;
    int i;

    for (i=0; i < sizeof(dword_props)/sizeof(dword_props[0]); i++) {
        cch_buffer = sizeof(propbuffer)/sizeof(propbuffer[0]);
        rv = MsiGetProperty(hInstall, dword_props[i], propbuffer, &cch_buffer);
        if (rv == ERROR_SUCCESS) {
            strip_decoration(propbuffer, cch_buffer);
            MsiSetProperty(hInstall, dword_props[i], propbuffer);
        }
    }

    return ERROR_SUCCESS;
}
