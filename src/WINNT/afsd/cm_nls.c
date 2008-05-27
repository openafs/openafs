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
#include <errno.h>

#define DEBUG_UNICODE

/* This is part of the Microsoft Internationalized Domain Name
   Mitigation APIs. */
#include <normalization.h>

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

long cm_InitNormalization(void)
{
    HMODULE h_Nls;

    if (pNormalizeString != NULL)
        return 0;

    h_Nls = LoadLibrary(NLSDLLNAME);
    if (h_Nls == INVALID_HANDLE_VALUE) {
        return 1;
    }

    pNormalizeString = GetProcAddress(h_Nls, "NormalizeString");
    pIsNormalizedString = GetProcAddress(h_Nls, "IsNormalizedString");

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
    if ((pIsNormalizedString && (*pIsNormalizedString)(AFS_NORM_FORM, src, cch_src)) ||
        (!pNormalizeString)) {

        if (ext_dest == NULL || *pcch_dest < cch_src) {
            ext_dest = malloc(cch_src * sizeof(wchar_t));
            *pcch_dest = cch_src;
        }

        /* No need to or unable to normalize.  Just copy the string.
           Note that the string is not necessarily NULL terminated. */

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
    if (cch_src < 0) {
        size_t cch;

        if (FAILED(StringCchLengthW(src, NLSMAXCCH, &cch)))
            return E2BIG;

        cch_src = cch+1;
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

#undef Esc
#undef IS_ESCAPED
#undef ESCVAL

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
        cch_src = strlen(src) + 1;
    }

    cch = MultiByteToWideChar(CP_UTF8, 0, src,
                              cch_src * sizeof(char), wsrcbuf, NLSMAXCCH);

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

    /* first check for NULL pointers */
    if (str1 == NULL) {
        if (str2 == NULL)
            return 0;
        else
            return -1;
    } else if (str2 == NULL) {
        return 1;
    }

    len1 = MultiByteToWideChar(CP_UTF8, 0, str1, n, wstr1, NLSMAXCCH);
    if (len1 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr1[0] = L'\0';
    }

    len2 = MultiByteToWideChar(CP_UTF8, 0, str2, n, wstr2, NLSMAXCCH);
    if (len2 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr2[0] = L'\0';
    }

    rv = CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, wstr1, len1, wstr2, len2);
    if (rv > 0)
        return (rv - 2);
    else {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }
}

int cm_stricmp_utf8(const char * str1, const char * str2)
{
    wchar_t wstr1[NLSMAXCCH];
    int len1;
    int len2;
    wchar_t wstr2[NLSMAXCCH];
    int rv;

    /* first check for NULL pointers */
    if (str1 == NULL) {
        if (str2 == NULL)
            return 0;
        else
            return -1;
    } else if (str2 == NULL) {
        return 1;
    }

    len1 = MultiByteToWideChar(CP_UTF8, 0, str1, -1, wstr1, NLSMAXCCH);
    if (len1 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr1[0] = L'\0';
    }

    len2 = MultiByteToWideChar(CP_UTF8, 0, str2, -1, wstr2, NLSMAXCCH);
    if (len2 == 0) {
#ifdef DEBUG
        DebugBreak();
#endif
        wstr2[0] = L'\0';
    }

    rv = CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, wstr1, len1, wstr2, len2);
    if (rv > 0)
        return (rv - 2);
    else {
#ifdef DEBUG
        DebugBreak();
#endif
        return 0;
    }
}

wchar_t * strupr_utf16(wchar_t * wstr, size_t cbstr)
{
    wchar_t wstrd[NLSMAXCCH];
    int len;

    len = cbstr / sizeof(wchar_t);
    len = LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, wstr, len, wstrd, NLSMAXCCH);
    StringCbCopyW(wstr, cbstr, wstrd);

    return wstr;
}

char * strupr_utf8(char * str, size_t cbstr)
{
    wchar_t wstr[NLSMAXCCH];
    wchar_t wstrd[NLSMAXCCH];
    int len;
    int r;

    len = MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, NLSMAXCCH);
    if (len == 0)
        return str;

    len = LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, wstr, len, wstrd, NLSMAXCCH);

    len = WideCharToMultiByte(CP_UTF8, 0, wstrd, -1, str, cbstr, NULL, FALSE);

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
