/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_H_CHAR_CONVERSION_H_
#define _H_CHAR_CONVERSION_H_


/*
 *  Class to convert a TCHAR string to an ANSI string.
 */
class S2A
{
    char *m_pszAnsi;

public:
    S2A(LPCTSTR pszUnicode);
    ~S2A();

    // Allow casts to char *
    operator char *() const     { return m_pszAnsi; }
    operator const char *() const     { return m_pszAnsi; }
};



/*
 *  Class to convert an ANSI string to a TCHAR string.  If UNICODE is defined,
 *  then the TCHAR string will be a UNICODE string, otherwise it will be an
 *  ANSI string.
 */
class A2S
{
    LPTSTR m_pszUnicode;

public:
    A2S(const char *pszAnsi);
    ~A2S();

    // Allow casts to LPTSTR
    operator LPTSTR() const     { return m_pszUnicode; }
    operator LPCTSTR() const     { return m_pszUnicode; }
};

#endif  // _H_CHAR_CONVERSION_H_


