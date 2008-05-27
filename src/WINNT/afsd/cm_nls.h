/*
 * Copyright (c) 2008 Secure Endpoints Inc.
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

#ifndef __CM_NLS_H_ENV__
#define __CM_NLS_H_ENV__

extern long cm_InitNormalization(void);

extern long cm_NormalizeUtf16StringToUtf8(const wchar_t * src, int cch_src,
                                          char * adest, int cch_adest);

extern long cm_NormalizeUtf8String(const char * src, int cch_src,
                                   char * adest, int cch_adest);

/* The cm_stricmp_utf8N function is identical to cm_stricmp_utf8
   except it is used in instances where one of the strings is always
   known to be ASCII. */
extern int cm_stricmp_utf8N(const char * str1, const char * str2);
#define cm_stricmp_utf8N cm_stricmp_utf8

extern int cm_stricmp_utf8(const char * str1, const char * str2);

/* The cm_strnicmp_utf8N function is identical to cm_strnicmp_utf8
   except it is used in instances where one of the strings is always
   known to be ASCII. */
extern int cm_strnicmp_utf8N(const char * str1, const char * str2, int n);
#define cm_strnicmp_utf8N cm_strnicmp_utf8

extern int cm_strnicmp_utf8(const char * str1, const char * str2, int n);

extern char * char_next_utf8(const char * c);

extern char * char_prev_utf8(const char * c);

extern char * strupr_utf8(char * str, size_t cbstr);

#endif
