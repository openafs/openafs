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

#ifndef OPENAFS_WINNT_AFSD_CM_NLS_H
#define OPENAFS_WINNT_AFSD_CM_NLS_H 1

/* Character types

   There are three character types that we use as implementation
   types.  These should generally only be referenced by the
   nationalization code.

   - ::cm_unichar_t

   - ::cm_normchar_t

   - ::cm_utf8char_t

   The character types that are used by code are :

   - ::clientchar_t

   - ::normchar_t

   - ::fschar_t

 */

/*! \brief Unicode UTF-16 Character */
typedef wchar_t cm_unichar_t;

/*! \brief Unicode UTF-16 Normalized Character (NF-C) */
typedef wchar_t cm_normchar_t;

/*! \brief Unicode UTF-8 Character */
typedef unsigned char cm_utf8char_t;

/*! \brief Client name */
typedef cm_unichar_t  clientchar_t;

/*! \brief File Server name */
typedef cm_utf8char_t fschar_t;

/*! \brief Normalized name */
typedef cm_normchar_t normchar_t;

#define __paste(a,b) a ## b
#define _C(s) __paste(L,s)
#define _FS(s) s
#define _N(s) __paste(L,s)

#define cm_ClientStringToNormStringAlloc cm_NormalizeStringAlloc
#define cm_ClientStringToFsStringAlloc cm_Utf16ToUtf8Alloc
#define cm_ClientStringToUtf8Alloc cm_Utf16ToUtf8Alloc
#define cm_FsStringToClientStringAlloc cm_Utf8ToUtf16Alloc
#define cm_FsStringToNormStringAlloc cm_NormalizeUtf8StringToUtf16Alloc
#define cm_Utf8ToNormStringAlloc cm_NormalizeUtf8StringToUtf16Alloc
#define cm_Utf8ToClientStringAlloc cm_Utf8ToUtf16Alloc

#define cm_ClientStringToUtf16 cm_Utf16ToUtf16
#define cm_ClientStringToUtf8  cm_Utf16ToUtf8
#define cm_ClientStringToFsString cm_Utf16ToUtf8
#define cm_ClientStringToNormString cm_NormalizeString
#define cm_FsStringToClientString cm_Utf8ToUtf16
#define cm_FsStringToNormString cm_NormalizeUtf8StringToUtf16
#define cm_Utf8ToClientString cm_Utf8ToUtf16
#define cm_OemToClientString(s,cchs,d,cchd) MultiByteToWideChar(CP_OEMCP, 0, s, cchs, d, cchd)
#define cm_AnsiToClientString(s,cchs,d,cchd) MultiByteToWideChar(CP_ACP, 0, s, cchs, d, cchd)

#define cm_ClientStrCmp wcscmp
#define cm_ClientStrCmpI cm_stricmp_utf16
#define cm_ClientStrCmpIA cm_stricmp_utf16
#define cm_ClientStrCmpNI cm_strnicmp_utf16
#define cm_ClientStrCmpN wcsncmp
#define cm_ClientStrChr wcschr
#define cm_ClientStrRChr wcsrchr
#define cm_ClientStrCpy(d,cch,s) StringCchCopyW(d,cch,s)
#define cm_ClientStrCpyN(d,cch,s,n) StringCchCopyNW(d,cch,s,n)
#define cm_ClientStrDup wcsdup
#define cm_ClientStrCat(d,cch,s) StringCchCatW(d,cch,s)
#define cm_ClientStrCatN(d,cch,s,n) StringCchCatNW(d,cch,s,n)
#define cm_ClientStrPrintfN StringCchPrintfW
#define cm_ClientStrPrintfV StringCchVPrintfW
//#define cm_ClientStrPrintf  swprintf
#define cm_ClientStrLen wcslen
#define cm_ClientStrLwr cm_strlwr_utf16
#define cm_ClientStrUpr cm_strupr_utf16
#define cm_ClientStrSpn wcsspn
#define cm_ClientStrCSpn wcscspn
#define osi_LogSaveClientString osi_LogSaveStringW
#define cm_ClientCharThis char_this_utf16
#define cm_ClientCharNext char_next_utf16
#define cm_ClientCharPrev char_prev_utf16

#define cm_FsStrDup strdup
#define cm_FsStrLen strlen
#define cm_FsStrCat StringCchCatA
#define cm_FsStrPrintf StringCchPrintfA
#define cm_FsStrRChr strrchr
#define cm_FsStrChr  strchr
#define cm_FsStrCmpIA cm_stricmp_utf8
#define cm_FsStrCmpI cm_stricmp_utf8
#define cm_FsStrCmpA  strcmp
#define cm_FsStrCmp  strcmp
#define cm_FsStrCpy(d,cch,s) StringCchCopyA(d,cch,s)
#define osi_LogSaveFsString osi_LogSaveString
#define cm_FsStrCpyN(d,cch,s,n) StringCchCopyN(d,cch,s,n)

#define cm_NormStrDup wcsdup
#define cm_NormStrCmpI cm_stricmp_utf16
#define cm_NormStrCmp wcscmp
#define cm_NormCharUpr towupper

#define cm_IsValidClientString(s) cm_is_valid_utf16((s), -1)
#define cm_IsValidNormString(s) cm_is_valid_utf16((s), -1)

#define cm_Utf16ToClientString cm_Utf16ToUtf16

extern long cm_InitNormalization(void);

/* Functions annotated in accordance with sal.h */

#ifndef __in_z

#define __out_ecount_full_z(x)
#define __out_ecount_full_z_opt(x)
#define __in_z
#define __out_z
#define __inout_z

#endif

extern __out_ecount_full_z(*pcch_dest) __checkReturn __success(return != NULL) cm_normchar_t *
    cm_NormalizeStringAlloc
    (__in_ecount(cch_src) const cm_unichar_t * s,
     int cch_src,
     __out_ecount_full_opt(1) int *pcch_dest);

extern __success(return != 0) int
    cm_NormalizeString
    (__in_ecount(cch_src) const cm_unichar_t * s,
     int cch_src,
     __out_ecount_full_z_opt(cch_dest) cm_normchar_t * dest,
     int cch_dest);

extern __out_ecount_full_z(*pcch_dest) __checkReturn __success(return != NULL) cm_utf8char_t *
    cm_Utf16ToUtf8Alloc
    (__in_ecount(cch_src) const cm_unichar_t * s,
     int cch_src,
     __out_ecount_full_opt(1) int *pcch_dest);

extern __out_ecount_full_z(*pcch_dest) __checkReturn __success(return != NULL) cm_unichar_t *
    cm_Utf8ToUtf16Alloc
    (__in_ecount(cch_src) const cm_utf8char_t * src,
     int cch_src,
     __out_ecount_full_opt(1) int *pcch_dest);

extern __success(return != 0) long
    cm_NormalizeUtf8StringToUtf16
    (__in_ecount(cch_src) const char * src,
     int cch_src,
     __out_ecount_full_z_opt(cch_dest) cm_normchar_t * dest,
     int cch_dest);

extern __out_ecount_full_z(*pcch_dest) __checkReturn __success(return != NULL) cm_normchar_t *
    cm_NormalizeUtf8StringToUtf16Alloc
    (__in_ecount(cch_src) const cm_utf8char_t * src,
     int cch_src,
     __out_ecount_full_opt(1) int *pcch_dest);

extern __success(return != 0) int
    cm_Utf8ToUtf16
    (__in_ecount(cch_src) const cm_utf8char_t * src,
     int cch_src,
     __out_ecount_full_z_opt(cch_dest) cm_unichar_t * dest,
     int cch_dest);

extern __success(return != 0) int
    cm_Utf16ToUtf8
    (__in_ecount(cch_src) const cm_unichar_t * src,
     int cch_src,
     __out_ecount_full_z_opt(cch_dest) cm_utf8char_t * dest,
     int cch_dest);

extern __success(return != 0) int
    cm_Utf16ToUtf16
    (__in_ecount(cch_src) const cm_unichar_t * src,
     int cch_src,
     __out_ecount_full_z_opt(cch_dest) cm_unichar_t * dest,
     int cch_dest);

extern int
    cm_strnicmp_utf16
    (__in_z const cm_unichar_t * str1,
     __in_z const cm_unichar_t * str2,
     int len);

extern int
    cm_stricmp_utf16
    (__in_z const cm_unichar_t * str1,
     __in_z const cm_unichar_t * str2);

/* The cm_stricmp_utf8N function is identical to cm_stricmp_utf8
   except it is used in instances where one of the strings is always
   known to be ASCII. */
extern int
    cm_stricmp_utf8N
    (__in_z const char * str1,
     __in_z const char * str2);
#define cm_stricmp_utf8N cm_stricmp_utf8

extern int
    cm_stricmp_utf8
    (__in_z const char * str1,
     __in_z const char * str2);

/* The cm_strnicmp_utf8N function is identical to cm_strnicmp_utf8
   except it is used in instances where one of the strings is always
   known to be ASCII. */
extern int
    cm_strnicmp_utf8N
    (__in_z const char * str1,
     __in_z const char * str2, int n);
#define cm_strnicmp_utf8N cm_strnicmp_utf8

extern int
    cm_strnicmp_utf8
    (__in_z const char * str1,
     __in_z const char * str2, int n);

extern __out_z wchar_t *
char_next_utf16
(__in_z const wchar_t * c);

extern __out_z wchar_t *
char_prev_utf16
(__in_z const wchar_t * c);

extern __out_z wchar_t *
char_this_utf16
(__in_z const wchar_t * c);

extern __out_z cm_unichar_t *
cm_strlwr_utf16(__inout_z cm_unichar_t * str);

extern __out_z cm_unichar_t *
cm_strupr_utf16(__inout_z cm_unichar_t * str);

extern int
cm_is_valid_utf16(__in_z const wchar_t * c, int cch);

#ifdef DEBUG
wchar_t * cm_GetRawCharsAlloc(const wchar_t * c, int len);
#endif

#if 0

extern long cm_NormalizeUtf16StringToUtf8(const wchar_t * src, int cch_src,
                                          char * adest, int cch_adest);

extern char * char_next_utf8(const char * c);

extern char * char_prev_utf8(const char * c);

extern char * strupr_utf8(char * str, size_t cbstr);

#endif

#define lengthof(a) (sizeof(a)/sizeof(a[0]))
#endif
