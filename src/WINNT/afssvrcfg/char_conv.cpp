/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES _________________________________________________________________
 *
 */
#include <WINNT\afsapplib.h>
#include <char_conv.h>



/*
 *  Class to convert a TCHAR string to an ANSI string.
 */
S2A::S2A(LPCTSTR pszUnicode)
{
    m_pszAnsi = StringToAnsi(pszUnicode);
}

S2A::~S2A()
{
// Only need to free the string if a conversion was performed
#ifdef UNICODE
    if (m_pszAnsi)
        FreeString(m_pszAnsi);
#endif
}


/*
 *  Class to convert an ANSI string to a TCHAR string.  If UNICODE is defined,
 *  then the TCHAR string will be a UNICODE string, otherwise it will be an
 *  ANSI string.
 */
A2S::A2S(const char *pszAnsi)
{
    m_pszUnicode = AnsiToString(pszAnsi);
}

A2S::~A2S()
{
// Only need to free the string if a conversion was performed
#ifdef UNICODE
    if (m_pszUnicode)
        FreeString(m_pszUnicode);
#endif
}

