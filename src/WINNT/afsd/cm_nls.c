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

#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include <strsafe.h>
#include <stdio.h>
#include <errno.h>

#include "cm_nls.h"

#ifdef DEBUG_UNICODE
#include <assert.h>
#endif

/* This is part of the Microsoft Internationalized Domain Name
   Mitigation APIs. */
#include <normalization.h>

/* TODO: All the normalization and conversion code should NUL
   terminate destination strings. */

int
(WINAPI *pNormalizeString)( __in NORM_FORM NormForm,
                            __in_ecount(cwSrcLength) LPCWSTR lpSrcString,
                            __in int cwSrcLength,
                            __out_ecount(cwDstLength) LPWSTR lpDstString,
                            __in int cwDstLength ) = NULL;

BOOL
(WINAPI *pIsNormalizedString)( __in NORM_FORM NormForm,
                               __in_ecount(cwLength) LPCWSTR lpString,
                               __in int cwLength ) = NULL;


#define NLSDLLNAME "Normaliz.dll"
#define NLSMAXCCH  1024
#define NLSERRCCH  8

#define AFS_NORM_FORM NormalizationC

static LCID nls_lcid = LOCALE_INVARIANT;

static int nls_init = 0;

static BOOL
is_windows_2000 (void)
{
   static BOOL fChecked = FALSE;
   static BOOL fIsWin2K = FALSE;

   if (!fChecked)
   {
       OSVERSIONINFO Version;

       memset (&Version, 0x00, sizeof(Version));
       Version.dwOSVersionInfoSize = sizeof(Version);

       if (GetVersionEx (&Version))
       {
           if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT &&
                Version.dwMajorVersion >= 5)
               fIsWin2K = TRUE;
       }
       fChecked = TRUE;
   }

   return fIsWin2K;
}

long cm_InitNormalization(void)
{
    HMODULE h_Nls;

    if (pNormalizeString != NULL)
        return 0;

    h_Nls = LoadLibrary(NLSDLLNAME);
    if (h_Nls == INVALID_HANDLE_VALUE) {
        return 1;
    }

    pNormalizeString =
        (int (WINAPI *)( NORM_FORM, LPCWSTR,
                         int, LPWSTR, int))
        GetProcAddress(h_Nls, "NormalizeString");

    pIsNormalizedString =
        (BOOL
         (WINAPI *)( NORM_FORM, LPCWSTR, int ))
        GetProcAddress(h_Nls, "IsNormalizedString");

    if (is_windows_2000())
        nls_lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);

    nls_init = 1;

    return (pNormalizeString && pIsNormalizedString);
}

/* \brief Normalize a UTF-16 string.

   If the supplied destination buffer is insufficient or NULL, then a
   new buffer will be allocated to hold the normalized string.

   \param[in] src : Source UTF-16 string.  Length is specified in
       cch_src.

   \param[in] cch_src : The character count in cch_src is assumed to
       be tight and include the terminating NULL character if there is
       one.  If the NULL is absent, the resulting string will not be
       NULL terminated.

   \param[out] ext_dest : The destination buffer.  Can be NULL, in
       which case *pcch_dest MUST be 0.

   \param[in,out] pcch_dest : On entry *pcch_dest contains a count of
       characters in the destination buffer.  On exit, it will contain
       a count of characters that were copied to the destination
       buffer.

   Returns a pointer to the buffer containing the normalized string or
   NULL if the call was unsuccessful.  If the returned destination
   buffer is different from the supplied buffer and non-NULL, it
   should be freed using free().
*/
static wchar_t *
NormalizeUtf16String(const wchar_t * src, int cch_src, wchar_t * ext_dest, int *pcch_dest)
{
    if (!nls_init)
        cm_InitNormalization();

#ifdef DEBUG_UNICODE
    assert (pNormalizeString != NULL && pIsNormalizedString != NULL);
#endif

    if (cch_src == -1)
        cch_src = (int)wcslen(src) + 1;

    if ((pIsNormalizedString && (*pIsNormalizedString)(AFS_NORM_FORM, src, cch_src)) ||
        (!pNormalizeString)) {

        if (ext_dest == NULL || *pcch_dest < cch_src) {
            ext_dest = malloc(cch_src * sizeof(wchar_t));
            *pcch_dest = cch_src;
        }

        /* No need to or unable to normalize.  Just copy the string.
           Note that the string is not NUL terminated if the source
           string is not NUL terminated. */

        if (ext_dest) {
            memcpy(ext_dest, src, cch_src * sizeof(wchar_t));
            *pcch_dest = cch_src;
        } else {
            *pcch_dest = 0;
        }
        return ext_dest;

    } else {

        int rv;
        DWORD gle;
        int tries = 10;
        wchar_t * dest;
        int cch_dest = *pcch_dest;

        dest = ext_dest;

        while (tries-- > 0) {

            rv = (*pNormalizeString)(AFS_NORM_FORM, src, cch_src, dest, cch_dest);

            if (rv <= 0 && (gle = GetLastError()) != ERROR_SUCCESS) {
                if (gle == ERROR_INSUFFICIENT_BUFFER) {

                    /* The buffer wasn't big enough.  We are going to
                       try allocating one. */

                    cch_dest = (-rv) + NLSERRCCH;
                    goto cont;

                } else {
                    /* Something else is wrong */
                    break;
                }

            } else if (rv < 0) { /* rv < 0 && gle == ERROR_SUCCESS */

                /* Technically not one of the expected outcomes */
                break;

            } else {            /* rv > 0 || (rv == 0 && gle == ERROR_SUCCESS) */

                /* Possibly succeeded */

                if (rv == 0) { /* Succeeded and the return string is empty */
                    *pcch_dest = 0;
                    return dest;
                }

                if (cch_dest == 0) {
                    /* Nope.  We only calculated the required size of the buffer */

                    cch_dest = rv + NLSERRCCH;
                    goto cont;
                }

                *pcch_dest = rv;
                if (cch_dest > rv)
                    dest[rv] = 0;
                else {
                    /* Can't NUL terminate */
                    cch_dest = max(rv,cch_dest) + NLSERRCCH;
                    goto cont;
                }

                /* Success! */
                return dest;
            }

        cont:
            if (dest != ext_dest && dest)
                free(dest);
            dest = malloc(cch_dest * sizeof(wchar_t));
        }

        /* Failed */

        if (dest != ext_dest && dest)
            free(dest);

        *pcch_dest = 0;
        return NULL;
    }
}

/*! \brief Normalize a Unicode string into a newly allocated buffer

  The input string will be normalized using NFC.

  \param[in] s UTF-16 string to be normalized.

  \param[in] cch_src The number of characters in the input string.  If
      this is -1, then the input string is assumed to be NUL
      terminated.

  \param[out] pcch_dest Receives the number of characters copied to
      the output buffer.  Note that the character count is the number
      of wchar_t characters copied, and not the count of Unicode code
      points.  This includes the terminating NUL if cch_src was -1 or
      included the terminating NUL.

  \return A newly allocated buffer holding the normalized string or
      NULL if the call failed.
 */
cm_normchar_t * cm_NormalizeStringAlloc(const cm_unichar_t * s, int cch_src, int *pcch_dest)
{
    int cch_dest = 0;
    cm_normchar_t * r;

    if (!nls_init)
        cm_InitNormalization();

    if (s == NULL || cch_src == 0 || *s == L'\0') {
        if (pcch_dest)
            *pcch_dest = ((cch_src != 0)? 1: 0);
        return wcsdup(L"");
    }

    r = NormalizeUtf16String(s, cch_src, NULL, &cch_dest);

    if (pcch_dest)
        *pcch_dest = cch_dest;

    return r;
}

int cm_NormalizeString(const cm_unichar_t * s, int cch_src,
                       cm_normchar_t * dest, int cch_dest)
{
    int tcch = cch_dest;
    cm_normchar_t * r;

    if (!nls_init)
        cm_InitNormalization();

    r = NormalizeUtf16String(s, cch_src, dest, &tcch);

    if (r != dest) {
        /* The supplied buffer was insufficient */
        free(r);
        return 0;
    } else {
        return tcch;
    }
}

/*! \brief Convert a UTF-16 string to a UTF-8 string using a newly allocated buffer

  \param[in] s UTF-16 source string

  \param[in] cch_src Number of characters in \a s. This can be set to
      -1 if \a s is NUL terminated.

  \param[out] pcch_dest Receives a count of characters that were
      copied to the target buffer.

  \return A newly allocated buffer holding the UTF-8 string.

 */
cm_utf8char_t * cm_Utf16ToUtf8Alloc(const cm_unichar_t * s, int cch_src, int *pcch_dest)
{
    int cch_dest;
    cm_utf8char_t * dest;

    if (!nls_init)
        cm_InitNormalization();

    if (s == NULL || cch_src == 0 || *s == L'\0') {
        if (pcch_dest)
            *pcch_dest = ((cch_src != 0)?1:0);
        return strdup("");
    }

    cch_dest = WideCharToMultiByte(CP_UTF8, 0, s, cch_src, NULL, 0, NULL, FALSE);

    if (cch_dest == 0) {
        if (pcch_dest)
            *pcch_dest = cch_dest;
        return NULL;
    }

    dest = malloc((cch_dest + 1) * sizeof(cm_utf8char_t));

    WideCharToMultiByte(CP_UTF8, 0, s, cch_src, dest, cch_dest, NULL, FALSE);
    dest[cch_dest] = 0;

    if (pcch_dest)
        *pcch_dest = cch_dest;

    return dest;
}

int cm_Utf16ToUtf8(const cm_unichar_t * src, int cch_src,
                   cm_utf8char_t * dest, int cch_dest)
{
    if (!nls_init)
        cm_InitNormalization();

    return WideCharToMultiByte(CP_UTF8, 0, src, cch_src, dest, cch_dest, NULL, FALSE);
}

int cm_Utf16ToUtf16(const cm_unichar_t * src, int cch_src,
                    cm_unichar_t * dest, int cch_dest)
{
    if (!nls_init)
        cm_InitNormalization();

    if (cch_src == -1) {
        StringCchCopyW(dest, cch_dest, src);
        return (int)wcslen(dest) + 1;
    } else {
        int cch_conv = min(cch_src, cch_dest);
        memcpy(dest, src, cch_conv * sizeof(cm_unichar_t));
        return cch_conv;
    }
}

/* \brief Normalize a UTF-16 string into a UTF-8 string.

   \param[in] src : Source string.

   \param[in] cch_src : Count of characters in src. If the count includes the
       NULL terminator, then the resulting string will be NULL
       terminated.  If it is -1, then src is assumed to be NULL
       terminated.

   \param[out] adest : Destination buffer.

   \param[in] cch_adest : Number of characters in the destination buffer.

   Returns the number of characters stored into cch_adest. This will
   include the terminating NULL if cch_src included the terminating
   NULL or was -1.  If this is 0, then the operation was unsuccessful.
 */
long cm_NormalizeUtf16StringToUtf8(const wchar_t * src, int cch_src,
                                   char * adest, int cch_adest)
{
    if (!nls_init)
        cm_InitNormalization();

    if (cch_src < 0) {
        size_t cch;

        if (FAILED(StringCchLengthW(src, NLSMAXCCH, &cch)))
            return E2BIG;

        cch_src = (int)cch+1;
    }

    {
        wchar_t nbuf[NLSMAXCCH];
        wchar_t * normalized;
        int cch_norm = NLSMAXCCH;

        normalized = NormalizeUtf16String(src, cch_src, nbuf, &cch_norm);
        if (normalized) {
            cch_adest = WideCharToMultiByte(CP_UTF8, 0, normalized, cch_norm,
                                            adest, cch_adest, NULL, 0);

            if (normalized != nbuf && normalized)
                free(normalized);

            return cch_adest;

        } else {

            return 0;

        }
    }
}

#define ESCVAL 0x1000
#define Esc(c) (ESCVAL + (short)(c))
#define IS_ESCAPED(c) (((c) & ESCVAL) == ESCVAL)

/* \brief Character sanitization map for CP-1252

   The following map indicates which characters should be escaped in
   the CP-1252 character map.  Characters that are documented as
   illegal characters in a file name are marked as escaped.  Escaped
   characters are marked using the ::Esc macro defined above.  The
   following exceptions apply:

   - Path delimeters '\\' and '/' are NOT escaped because the
     sanitization map applies to paths.  While those characters are
     illegal in filenames, they are legal in paths.

   - Wildcard characters '*' and '?' ARE escaped.  The document
     referred below does not specify these characters as invalid.
     Since no other escape mechanism exists, names containing
     wildcards are indistinguishable from actual wildcards used in SMB
     requests.

   - Reserved names are not and cannot be represented in this map.
     Reserved names are :

     CON, PRN, AUX, NUL, COM1, COM2, COM3, COM4, COM5, COM6, COM7,
     COM8, COM9, LPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, LPT9,
     CLOCK$

   - Characters 0x80, 0x81, 0x8d, 0x8e, 0x8f, 0x90, 0x9d, 0x9e, 0x9f
     are also escaped because they are unused in CP-1252 and hence
     cannot be convered to a Unicode string.

     Reserved names with extensions are also invalid. (i.e. NUL.txt)

   \note The only bit we are actually interested in from the following
     table is the ESCVAL bit.  However, the characters themselves are
     included for ease of maintenance.

   \see "Naming a File" topic in the Windows SDK.
 */
static const short sanitized_escapes_1252[] = {
    Esc(0x00),Esc(0x01),Esc(0x02),Esc(0x03),Esc(0x04),Esc(0x05),Esc(0x06),Esc(0x07),
    Esc(0x08),Esc(0x09),Esc(0x0a),Esc(0x0b),Esc(0x0c),Esc(0x0d),Esc(0x0e),Esc(0x0f),
    Esc(0x10),Esc(0x11),Esc(0x12),Esc(0x13),Esc(0x14),Esc(0x15),Esc(0x16),Esc(0x17),
    Esc(0x18),Esc(0x19),Esc(0x1a),Esc(0x1b),Esc(0x1c),Esc(0x1d),Esc(0x1e),Esc(0x1f),
    ' ','!',Esc('"'),'#','$','%','&','\'','(',')',Esc('*'),'+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',Esc(':'),';',Esc('<'),'=',Esc('>'),Esc('?'),
    '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
    'P','Q','R','S','T','U','V','W','X','Y','Z','[','\\',']','^','_',
    '`','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
    'p','q','r','s','t','u','v','w','x','y','z','{',Esc('|'),'}','~',Esc(0x7f),
    Esc(0x80),Esc(0x81),0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,Esc(0x8d),Esc(0x8e),Esc(0x8f),
    Esc(0x90),0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,Esc(0x9d),Esc(0x9e),0x9f,
    0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
    0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
    0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
    0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
    0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
    0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

static int sanitize_bytestring(const char * src, int cch_src,
                               char * odest, int cch_dest)
{
    char * dest = odest;

    if (!nls_init)
        cm_InitNormalization();

    while (cch_src > 0 && *src && cch_dest > 0) {

        unsigned short rc;

        rc = sanitized_escapes_1252[*src];
        if (IS_ESCAPED(rc)) {
            static const char hex[] =
                {'0','1','2','3','4','5','6','7',
                 '8','9','a','b','c','d','e','f'};

            if (cch_dest < 3) {
                *dest++ = '\0';
                return 0;
            }

            *dest++ = '%';
            *dest++ = hex[(((int)*src) >> 4) & 0x0f];
            *dest++ = hex[(((int)*src) & 0x0f)];
            cch_dest -= 3;

        } else {
            *dest++ = *src;
            cch_dest--;
        }

        cch_src--;
        src++;
    }

    if (cch_src > 0 && cch_dest > 0) {
        *dest++ = '\0';
    }

    return (int)(dest - odest);
}

static int sanitize_utf16char(wchar_t c, wchar_t ** pdest, size_t * pcch)
{
    if (*pcch >= 6) {
        StringCchPrintfExW(*pdest, *pcch, pdest, pcch, 0, L"%%%04x", (int) c);
        return 1;
    } else {
        return 0;
    }
}

static int sanitize_utf16string(const wchar_t * src, size_t cch_src,
                                wchar_t * dest, size_t cch_dest)
{
    int cch_dest_o = cch_dest;

    if (dest == NULL) {
        /* only estimating */
        for (cch_dest = 0; cch_src > 0;) {
            if (*src >= 0xd800 && *src < 0xdc00) {
                if (cch_src <= 1 || src[1] < 0xdc00 || src[1] > 0xdfff) {
                    /* dangling surrogate */
                    src++;
                    cch_src --;
                    cch_dest += 5;
                } else {
                    /* surrogate pair */
                    src += 2;
                    cch_src -= 2;
                    cch_dest += 2;
                }
            } else if (*src >= 0xdc00 && *src <= 0xdfff) {
                /* dangling surrogate */
                src++;
                cch_src --;
                cch_dest += 5;
            } else {
                /* normal char */
                src++; cch_src --;
                cch_dest++;
            }
        }

        return cch_dest;
    }

    while (cch_src > 0 && cch_dest > 0) {
        if (*src >= 0xd800 && *src < 0xdc00) {
            if (cch_src <= 1 || src[1] < 0xdc00 || src[1] > 0xdfff) {
                if (!sanitize_utf16char(*src++, &dest, &cch_dest))
                    return 0;
                cch_src--;
            } else {
                /* found a surrogate pair */
                *dest++ = *src++;
                *dest++ = *src++;
                cch_dest -= 2; cch_src -= 2;
            }
        } else if (*src >= 0xdc00 && *src <= 0xdfff) {
            if (!sanitize_utf16char(*src++, &dest, &cch_dest))
                return 0;
            cch_src--;
        } else {
            *dest++ = *src++;
            cch_dest--; cch_src--;
        }
    }

    return (cch_src == 0) ? cch_dest_o - cch_dest : 0;
}

#undef Esc
#undef IS_ESCAPED
#undef ESCVAL

long cm_NormalizeUtf8StringToUtf16(const char * src, int cch_src,
                                   wchar_t * dest, int cch_dest)
{
    wchar_t wsrcbuf[NLSMAXCCH];
    wchar_t *wnorm;
    int cch;
    int cch_norm;

    if (!nls_init)
        cm_InitNormalization();

    /* Get some edge cases out first, so we don't have to worry about
       cch_src being 0 etc. */
    if (cch_src == 0) {
        return 0;
    } else if (*src == '\0') {
        if (cch_dest >= 1)
            *dest = L'\0';
        return 1;
    }

    if (dest && cch_dest > 0) {
        dest[0] = L'\0';
    }

    if (cch_src == -1) {
        cch_src = (int)strlen(src) + 1;
    }

    cch = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src,
                              cch_src * sizeof(char), wsrcbuf, NLSMAXCCH);

    if (cch != 0 && !cm_is_valid_utf16(wsrcbuf, cch)) {
        wchar_t wsanitized[NLSMAXCCH];

        /* We successfully converted, but the resulting UTF-16 string
           has dangling surrogates.  We should try and escape those
           next.  */
        cch = sanitize_utf16string(wsrcbuf, cch, wsanitized, NLSMAXCCH);
        if (cch != 0) {
            memcpy(wsrcbuf, wsanitized, cch * sizeof(wchar_t));
        }
    }

    if (cch == 0) {
        if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
            char sanitized[NLSMAXCCH];
            int cch_sanitized;

            /* If src doesn't have a unicode translation, then it
               wasn't valid UTF-8.  In this case, we assume that src
               is CP-1252 and then try to convert again.  But before
               that, we use a translation table to "sanitize" the
               input. */

            cch_sanitized = sanitize_bytestring(src, cch_src, sanitized,
                                                sizeof(sanitized)/sizeof(char));

            if (cch_sanitized == 0) {
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return 0;
            }

            cch = MultiByteToWideChar(1252, 0, sanitized,
                                      cch_sanitized * sizeof(char), wsrcbuf, NLSMAXCCH);
            if (cch == 0) {
                /* Well, that didn't work either.  Something is very wrong. */
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return 0;
            }
        } else {
            return 0;
        }
    }

    cch_norm = cch_dest;
    wnorm = NormalizeUtf16String(wsrcbuf, cch, dest, &cch_norm);
    if (wnorm == NULL) {
#ifdef DEBUG_UNICODE
        DebugBreak();
#endif
        return 0;
    }

    if (wnorm != dest) {
        /* The buffer was insufficient */
        if (dest != NULL && cch_dest > 1) {
            *dest = L'\0';
            cch_norm = 0;
        }

        free(wnorm);
    }

    return cch_norm;
}

cm_normchar_t *cm_NormalizeUtf8StringToUtf16Alloc(const cm_utf8char_t * src, int cch_src,
                                                  int *pcch_dest)
{
    wchar_t wsrcbuf[NLSMAXCCH];
    wchar_t *wnorm;
    int cch;
    int cch_norm;

    if (!nls_init)
        cm_InitNormalization();

    /* Get some edge cases out first, so we don't have to worry about
       cch_src being 0 etc. */
    if (cch_src == 0 || src == NULL || *src == '\0') {
        if (pcch_dest)
            *pcch_dest = ((cch_src != 0)? 1 : 0);
        return wcsdup(L"");
    }

    if (cch_src == -1) {
        cch_src = (int)strlen(src) + 1;
    }

    cch = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src,
                              cch_src * sizeof(char), wsrcbuf, NLSMAXCCH);

    if (cch != 0 && !cm_is_valid_utf16(wsrcbuf, cch)) {
        wchar_t wsanitized[NLSMAXCCH];

        /* We successfully converted, but the resulting UTF-16 string
           has dangling surrogates.  We should try and escape those
           next.  */
        cch = sanitize_utf16string(wsrcbuf, cch, wsanitized, NLSMAXCCH);
        if (cch != 0) {
            memcpy(wsrcbuf, wsanitized, cch * sizeof(wchar_t));
        }
    }

    if (cch == 0) {
        if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
            char sanitized[NLSMAXCCH];
            int cch_sanitized;

            /* If src doesn't have a unicode translation, then it
               wasn't valid UTF-8.  In this case, we assume that src
               is CP-1252 and then try to convert again.  But before
               that, we use a translation table to "sanitize" the
               input. */

            cch_sanitized = sanitize_bytestring(src, cch_src, sanitized,
                                                sizeof(sanitized)/sizeof(char));

            if (cch_sanitized == 0) {
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return NULL;
            }

            cch = MultiByteToWideChar(1252, 0, sanitized,
                                      cch_sanitized * sizeof(char), wsrcbuf, NLSMAXCCH);
            if (cch == 0) {
                /* Well, that didn't work either.  Something is very wrong. */
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return NULL;
            }
        } else {
            return NULL;
        }
    }

    cch_norm = 0;
    wnorm = NormalizeUtf16String(wsrcbuf, cch, NULL, &cch_norm);
    if (wnorm == NULL) {
#ifdef DEBUG_UNICODE
        DebugBreak();
#endif
        return NULL;
    }

    if (pcch_dest)
        *pcch_dest = cch_norm;

    return wnorm;
}

int cm_Utf8ToUtf16(const cm_utf8char_t * src, int cch_src,
                   cm_unichar_t * dest, int cch_dest)
{
    int cch;

    if (cch_dest >= 1 && dest != NULL) {
        dest[0] = L'\0';
    }

    if (!nls_init)
        cm_InitNormalization();

    if (cch_src == -1) {
        cch_src = (int)strlen(src) + 1;
    }

    cch = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src,
                              cch_src * sizeof(char), dest, cch_dest);

    if (cch != 0 && !cm_is_valid_utf16(dest, cch)) {
        wchar_t wsanitized[NLSMAXCCH];

        cch = sanitize_utf16string(dest, cch, wsanitized, NLSMAXCCH);
        if (cch != 0) {
            memcpy(dest, wsanitized, cch * sizeof(wchar_t));
        }
    }

    if (cch == 0) {
        if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
            char sanitized[NLSMAXCCH];
            int cch_sanitized;

            /* If src doesn't have a unicode translation, then it
               wasn't valid UTF-8.  In this case, we assume that src
               is CP-1252 and then try to convert again.  But before
               that, we use a translation table to "sanitize" the
               input. */

            cch_sanitized = sanitize_bytestring(src, cch_src, sanitized,
                                                sizeof(sanitized)/sizeof(char));

            if (cch_sanitized == 0) {
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return 0;
            }

            cch = MultiByteToWideChar(1252, 0, sanitized,
                                      cch_sanitized * sizeof(char), dest, cch_dest);
            if (cch == 0) {
                /* Well, that didn't work either.  Something is very wrong. */
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return 0;
            } else {
                return cch;
            }

        } else {
            return 0;
        }
    } else {
        return cch;
    }
}

cm_unichar_t  * cm_Utf8ToUtf16Alloc(const cm_utf8char_t * src, int cch_src, int *pcch_dest)
{
    cm_unichar_t * ustr = NULL;
    int cch;

    if (!nls_init)
        cm_InitNormalization();

    if (cch_src == 0 || src == NULL || *src == '\0') {
        if (pcch_dest)
            *pcch_dest = ((cch_src != 0)? 1 : 0);
        return wcsdup(L"");
    }

    if (cch_src == -1) {
        cch_src = (int)strlen(src) + 1;
    }

    cch = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src,
                              cch_src * sizeof(char), NULL, 0);

    if (cch == 0) {
        if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
            char sanitized[NLSMAXCCH];
            int cch_sanitized;

            /* If src doesn't have a unicode translation, then it
               wasn't valid UTF-8.  In this case, we assume that src
               is CP-1252 and then try to convert again.  But before
               that, we use a translation table to "sanitize" the
               input. */

            cch_sanitized = sanitize_bytestring(src, cch_src, sanitized,
                                                sizeof(sanitized)/sizeof(char));

            if (cch_sanitized == 0) {
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return NULL;
            }

            cch = MultiByteToWideChar(1252, 0, sanitized,
                                      cch_sanitized * sizeof(char), NULL, 0);
            if (cch == 0) {
                /* Well, that didn't work either.  Something is very wrong. */
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return NULL;
            }

            ustr = malloc((cch + 1) * sizeof(wchar_t));

            cch = MultiByteToWideChar(1252, 0, sanitized,
                                      cch_sanitized * sizeof(char), ustr, cch);
            ustr[cch] = 0;
        } else {
            return NULL;
        }
    } else {

        ustr = malloc((cch + 1) * sizeof(wchar_t));

        cch = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src,
                                  cch_src * sizeof(char), ustr, cch);
        ustr[cch] = 0;

        if (!cm_is_valid_utf16(ustr, cch)) {
            cm_unichar_t * us = NULL;
            int cch_s;

            cch_s = sanitize_utf16string(ustr, cch, NULL, 0);
            if (cch_s != 0) {
                us = malloc(cch_s * sizeof(wchar_t));
                cch_s = sanitize_utf16string(ustr, cch, us, cch_s);
            }

            if (cch_s != 0) {
                free(ustr);
                ustr = us;
                us = NULL;
            } else {
                if (us)
                    free(us);
                free(ustr);
                ustr = NULL;
            }
        }
    }

    if (pcch_dest)
        *pcch_dest = cch;

    return ustr;
}



/* \brief Normalize a UTF-8 string.

   \param[in] src String to normalize.

   \param[in] cch_src : Count of characters in src.  If this value is
       -1, then src is assumed to be NULL terminated.  The translated
       string will be NULL terminated only if this is -1 or the count
       includes the terminating NULL.

   \param[out] adest : Destination string.  Only considered valid if
       \a cch_adest is non-zero.

   \param[in] cch_adest : Number of characters in the destination
       string.  If this is zero, then the return value is the number
       of bytes required.

   \return If \a cch_adest is non-zero, then the return value is the
       number of bytes stored into adest.  If \a cch_adest is zero,
       then the return value is the number of bytes required.  In both
       cases, the return value is 0 if the call was unsuccessful.
 */
long cm_NormalizeUtf8String(const char * src, int cch_src,
                            char * adest, int cch_adest)
{
    wchar_t wsrcbuf[NLSMAXCCH];
    wchar_t *wnorm;
    int cch;
    int cch_norm;

    if (!nls_init)
        cm_InitNormalization();

    /* Get some edge cases out first, so we don't have to worry about
       cch_src being 0 etc. */
    if (cch_src == 0) {
        return 0;
    } else if (*src == '\0') {
        if (cch_adest >= 1)
            *adest = '\0';
        return 1;
    }

    if (cch_src == -1) {
        cch_src = (int)strlen(src) + 1;
    }

    cch = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src,
                              cch_src * sizeof(char), wsrcbuf, NLSMAXCCH);

    if (cch != 0 && !cm_is_valid_utf16(wsrcbuf, cch)) {
        wchar_t wsanitized[NLSMAXCCH];

        cch = sanitize_utf16string(wsrcbuf, cch, wsanitized, NLSMAXCCH);
        if (cch != 0) {
            memcpy(wsrcbuf, wsanitized, cch * sizeof(wchar_t));
        }
    }

    if (cch == 0) {
        if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
            char sanitized[NLSMAXCCH];
            int cch_sanitized;

            /* If src doesn't have a unicode translation, then it
               wasn't valid UTF-8.  In this case, we assume that src
               is CP-1252 and then try to convert again.  But before
               that, we use a translation table to "sanitize" the
               input. */

            cch_sanitized = sanitize_bytestring(src, cch_src, sanitized,
                                                sizeof(sanitized)/sizeof(char));

            if (cch_sanitized == 0) {
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return 0;
            }

            cch = MultiByteToWideChar(1252, 0, sanitized,
                                      cch_sanitized * sizeof(char), wsrcbuf, NLSMAXCCH);
            if (cch == 0) {
                /* Well, that didn't work either.  Something is very wrong. */
#ifdef DEBUG_UNICODE
                DebugBreak();
#endif
                return 0;
            }
        } else {
            return 0;
        }
    }

    cch_norm = 0;
    wnorm = NormalizeUtf16String(wsrcbuf, cch, NULL, &cch_norm);
    if (wnorm == NULL) {
#ifdef DEBUG_UNICODE
        DebugBreak();
#endif
        return 0;
    }

    cch = WideCharToMultiByte(CP_UTF8, 0, wnorm,
                              cch_norm, adest, cch_adest * sizeof(char),
                              NULL, FALSE);

    if (wnorm)
        free(wnorm);

    return cch;
}

/*! \brief Case insensitive comparison with specific length

  \param[in] str1 First string to compare.  Assumed to be encoded in UTF-8.

  \param[in] str2 Second string to compare.  Assumed to be encoded in UTF-8.

  \param[in] n Max byte count.

 */
int cm_strnicmp_utf8(const char * str1, const char * str2, int n)
{
    wchar_t wstr1[NLSMAXCCH];
    int len1;
    int len2;
    wchar_t wstr2[NLSMAXCCH];
    int rv;

    if (!nls_init)
        cm_InitNormalization();

    if (n == 0)
        return 0;

    /* first check for NULL pointers (assume NULL < "") */
    if (str1 == NULL) {
        if (str2 == NULL)
            return 0;
        else
            return -1;
    } else if (str2 == NULL) {
        return 1;
    }

    len1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str1, n, wstr1, NLSMAXCCH);
    if (len1 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr1[0] = L'\0';
    }

    len2 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str2, n, wstr2, NLSMAXCCH);
    if (len2 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr2[0] = L'\0';
    }

    rv = CompareStringW(nls_lcid, NORM_IGNORECASE, wstr1, len1, wstr2, len2);
    if (rv > 0)
        return (rv - 2);
    else {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }
}

int cm_strnicmp_utf16(const cm_unichar_t * str1, const cm_unichar_t * str2, int len)
{
    int rv;
    size_t cch1;
    size_t cch2;

    if (!nls_init)
        cm_InitNormalization();

    if (len == 0)
        return 0;

    /* first check for NULL pointers */
    if (str1 == NULL) {
        if (str2 == NULL)
            return 0;
        else
            return -1;
    } else if (str2 == NULL) {
        return 1;
    }

    if (FAILED(StringCchLengthW(str1, len, &cch1)))
        cch1 = len;

    if (FAILED(StringCchLengthW(str2, len, &cch2)))
        cch2 = len;

    rv = CompareStringW(nls_lcid, NORM_IGNORECASE, str1, (int)cch1, str2, (int)cch2);
    if (rv > 0)
        return (rv - 2);
    else {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }
}

int cm_stricmp_utf16(const cm_unichar_t * str1, const cm_unichar_t * str2)
{
    int rv;

    if (!nls_init)
        cm_InitNormalization();

    /* first check for NULL pointers */
    if (str1 == NULL) {
        if (str2 == NULL)
            return 0;
        else
            return -1;
    } else if (str2 == NULL) {
        return 1;
    }

    rv = CompareStringW(nls_lcid, NORM_IGNORECASE, str1, -1, str2, -1);
    if (rv > 0)
        return (rv - 2);
    else {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }
}

cm_unichar_t *cm_strlwr_utf16(cm_unichar_t * str)
{
    int rv;
    int len;

    if (!nls_init)
        cm_InitNormalization();

    len = (int)wcslen(str) + 1;
    rv = LCMapStringW(nls_lcid, LCMAP_LOWERCASE, str, len, str, len);
#ifdef DEBUG
    if (rv == 0) {
        DebugBreak();
    }
#endif

    return str;
}

cm_unichar_t *cm_strupr_utf16(cm_unichar_t * str)
{
    int rv;
    int len;

    if (!nls_init)
        cm_InitNormalization();

    len = (int)wcslen(str) + 1;
    rv = LCMapStringW(nls_lcid, LCMAP_UPPERCASE, str, len, str, len);
#ifdef DEBUG
    if (rv == 0) {
        DebugBreak();
    }
#endif

    return str;
}


int cm_stricmp_utf8(const char * str1, const char * str2)
{
    wchar_t wstr1[NLSMAXCCH];
    int len1;
    int len2;
    wchar_t wstr2[NLSMAXCCH];
    int rv;

    if (!nls_init)
        cm_InitNormalization();

    /* first check for NULL pointers */
    if (str1 == NULL) {
        if (str2 == NULL)
            return 0;
        else
            return -1;
    } else if (str2 == NULL) {
        return 1;
    }

    len1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str1, -1, wstr1, NLSMAXCCH);
    if (len1 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr1[0] = L'\0';
    }

    len2 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str2, -1, wstr2, NLSMAXCCH);
    if (len2 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr2[0] = L'\0';
    }

    rv = CompareStringW(nls_lcid, NORM_IGNORECASE, wstr1, len1, wstr2, len2);
    if (rv > 0)
        return (rv - 2);
    else {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }
}

#if 0
wchar_t * strupr_utf16(wchar_t * wstr, size_t cbstr)
{
    wchar_t wstrd[NLSMAXCCH];
    int len;

    if (!nls_init)
        cm_InitNormalization();

    len = cbstr / sizeof(wchar_t);
    len = LCMapStringW(nls_lcid, LCMAP_UPPERCASE, wstr, len, wstrd, NLSMAXCCH);
    StringCbCopyW(wstr, cbstr, wstrd);

    return wstr;
}
#endif

char * strupr_utf8(char * str, size_t cbstr)
{
    wchar_t wstr[NLSMAXCCH];
    wchar_t wstrd[NLSMAXCCH];
    int len;

    if (!nls_init)
        cm_InitNormalization();

    len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1, wstr, NLSMAXCCH);
    if (len == 0)
        return str;

    len = LCMapStringW(nls_lcid, LCMAP_UPPERCASE, wstr, len, wstrd, NLSMAXCCH);

    len = WideCharToMultiByte(CP_UTF8, 0, wstrd, -1, str, (int)cbstr, NULL, FALSE);

    return str;
}

char * char_next_utf8(const char * c)
{
#define CH (*((const unsigned char *)c))

    if ((CH & 0x80) == 0)
        return (char *) c+1;
    else {
        switch (CH & 0xf0) {
        case 0xc0:
        case 0xd0:
            return (char *) c+2;

        case 0xe0:
            return (char *) c+3;

        case 0xf0:
            return (char *) c+4;

        default:
            return (char *) c+1;
        }
    }
#undef CH
}


char * char_prev_utf8(const char * c)
{
#define CH (*((const unsigned char *)c))

    c--;

    if ((CH & 0x80) == 0)
        return (char *) c;
    else
        while ((CH & 0xc0) == 0x80)
            (char *) c--;
    return (char *) c;

#undef CH
}

wchar_t * char_next_utf16(const wchar_t * c)
{
    unsigned short sc = (unsigned short) *c;

    if (sc >= 0xd800 && sc <= 0xdbff)
        return (wchar_t *) c+2;
    return (wchar_t *) c+1;
}

wchar_t * char_prev_utf16(const wchar_t * c)
{
    unsigned short sc = (unsigned short) *(--c);

    if (sc >= 0xdc00 && sc <= 0xdfff)
        return (wchar_t *) --c;
    return (wchar_t *) c;
}

wchar_t * char_this_utf16(const wchar_t * c)
{
    unsigned short sc = (unsigned short) *c;

    if (sc >= 0xdc00 && sc <= 0xdfff)
        return (wchar_t *) --c;
    return (wchar_t *) c;
}

int cm_is_valid_utf16(const wchar_t * c, int cch)
{
    if (cch < 0)
        cch = wcslen(c) + 1;

    for (; cch > 0; c++, cch--) {
        if (*c >= 0xd800 && *c < 0xdc00) {
            c++; cch--;
            if (cch == 0 || *c < 0xdc00 || *c > 0xdfff)
                return 0;
        } else if (*c >= 0xdc00 && *c <= 0xdfff) {
            return 0;
        }
    }

    return 1;
}

#ifdef DEBUG
wchar_t * cm_GetRawCharsAlloc(const wchar_t * c, int len)
{
    wchar_t * ret;
    wchar_t * current;
    size_t cb;

    if (len == -1)
        len = wcslen(c);

    if (len == 0)
        return wcsdup(L"(empty)");

    cb = len * 5 * sizeof(wchar_t);
    current = ret = malloc(cb);
    if (ret == NULL)
        return NULL;

    for (; len > 0; ++c, --len) {
        StringCbPrintfExW(current, cb, &current, &cb, 0,
                         L"%04x", (int) *c);
        if (len > 1)
            StringCbCatExW(current, cb, L",", &current, &cb, 0);
    }

    return ret;
}
#endif
