/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef REGEXP_H
#define REGEXP_H

#ifndef EXPORTED
#define EXPORTED
#endif

/*
 * This class (hopefully) makes it easy to use regular-expression pattern
 * matching. As an example, you can do the following:
 *
 *    LPREGEXP pExpr = new REGEXP (TEXT("sl[ia]p-h.ppy$"));
 *    if (pExpr->Matches (TEXT("slap-happy")))
 *       ...
 *    else if (pExpr->Matches (TEXT("slip-hoppy")))
 *       ...
 *    else if (!pExpr->Matches (TEXT("slug-hoppy")))
 *       ...
 *    delete pExpr;
 *
 * As a convenience, you can also use a simpler, more limited interface...
 *
 *    if (REGEXP::Matches (TEXT("sl[ia]p-h.ppy$"), TEXT("slug-hoppy")))
 *       ...
 *
 * You can also test a string to see if it looks like a regular expression:
 *
 *    LPREGEXP pExpr = new REGEXP (TEXT("ab[cC]d"));
 *    if (pExpr->fIsRegExp())
 *       ...
 *    if (!REGEXP::fIsRegExp (TEXT("testing")))
 *       ...
 *
 */

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cchCOMPILED_BUFFER_MAX     512
#define nCOMPILED_PARENS_MAX         9    // "\1" through "\9"

typedef class EXPORTED REGEXP REGEXP, *LPREGEXP;

class EXPORTED REGEXP
   {
   public:
      REGEXP (void);
      REGEXP (LPCTSTR pszExpr);
      ~REGEXP (void);

      BOOL SetExpression (LPCTSTR pszExpr);

      BOOL Matches (LPCTSTR pszString);
      static BOOL Matches (LPCTSTR pszExpr, LPCTSTR pszString);

      BOOL fIsRegExp (void);
      static BOOL fIsRegExp (LPCTSTR pszExpr);

   private:
      BOOL Compile (LPCTSTR pszExpr);
      BOOL MatchSubset (LPCTSTR pszString, LPCTSTR pchCompiled, LPCTSTR *aParenStarts, LPCTSTR *aParenEnds);
      BOOL CompareParen (int ii, LPCTSTR pszString, LPCTSTR *aParenStarts, LPCTSTR *aParenEnds);
      BOOL fIsInCharSet (LPCTSTR pszCharSet, TCHAR chTest, int fInclusive);

      BOOL m_fMatchFromStart;
      TCHAR m_achCompiled[ cchCOMPILED_BUFFER_MAX ];
   };


#endif

