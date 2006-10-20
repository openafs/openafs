/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <WINNT/regexp.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define markREPEAT       TEXT('\x01')
#define markCHARACTER    TEXT('\x02')
#define markANYCHAR      TEXT('\x04')
#define markCHARSET      TEXT('\x06')
#define markNONCHARSET   TEXT('\x08')
#define markREFERENCE    TEXT('\x0A')
#define markLPAREN       TEXT('\xFC')
#define markRPAREN       TEXT('\xFD')
#define markENDLINE      TEXT('\xFE')
#define markENDPATTERN   TEXT('\xFF')


/*
 * CLASS ROUTINES _____________________________________________________________
 *
 */

REGEXP::REGEXP (void)
{
   m_fMatchFromStart = FALSE;
   m_achCompiled[0] = TEXT('\0');
}

REGEXP::REGEXP (LPCTSTR pszExpr)
{
   m_fMatchFromStart = FALSE;
   m_achCompiled[0] = TEXT('\0');
   SetExpression (pszExpr);
}

REGEXP::~REGEXP (void)
{
   ; // nothing really to do here
}

BOOL REGEXP::SetExpression (LPCTSTR pszExpr)
{
   return Compile (pszExpr);
}

BOOL REGEXP::Matches (LPCTSTR pszExpr, LPCTSTR pszString)
{
   REGEXP Expr (pszExpr);
   return Expr.Matches (pszString);
}

BOOL REGEXP::fIsRegExp (void)
{
   if (m_fMatchFromStart) // started with "^"?
      return TRUE;        // it's a regexp.

   for (LPCTSTR pch = m_achCompiled; (*pch) && (*pch != markENDPATTERN); pch += 2)
      {
      if (*pch != markCHARACTER)
         return TRUE;
      }

   return FALSE; // just a string of characters
}

BOOL REGEXP::fIsRegExp (LPCTSTR pszExpr)
{
   REGEXP Expr (pszExpr);
   return Expr.fIsRegExp();
}


/*
 * REGEXP _____________________________________________________________________
 *
 */

BOOL REGEXP::Compile (LPCTSTR pszExpr)
{
   BYTE aParens[ nCOMPILED_PARENS_MAX ];
   PBYTE pParen = &aParens[0];
   LPTSTR pchLastEx = NULL;
   int nParens = 0;

   // Erase any previous compiled expression
   //
   LPTSTR pchCompiled = m_achCompiled;
   *pchCompiled = TEXT('\0');
   m_fMatchFromStart = FALSE;

   if (!pszExpr || !*pszExpr)
      {
      SetLastError (ERROR_INVALID_PARAMETER);
      return FALSE;
      }

   // See if the expression starts with a "^"
   //
   if ((m_fMatchFromStart = (*pszExpr == TEXT('^'))) == TRUE)
      ++pszExpr;

   // Start stripping characters from the expression
   //
   BOOL rc;
   for (rc = TRUE; rc; )
      {
      TCHAR ch;

      if ((sizeof(TCHAR)*(pchCompiled - m_achCompiled)) > sizeof(m_achCompiled))
         {
         SetLastError (ERROR_META_EXPANSION_TOO_LONG);
         rc = FALSE;
         break;
         }

      if ((ch = *pszExpr++) == TEXT('\0'))
         {
         // We finally hit the end of this expression.
         //
         if (pParen != &aParens[0])
            {
            SetLastError (ERROR_BAD_FORMAT); // unmatched "\("
            rc = FALSE;
            }
         break;
         }

      if (ch != TEXT('*'))
         pchLastEx = pchCompiled;

      switch (ch)
         {
         case TEXT('.'):
         case TEXT('?'):
            *pchCompiled++ = markANYCHAR;
            break;

         case TEXT('*'):
            if ((pchLastEx == NULL) || (*pchLastEx == markLPAREN) || (*pchLastEx == markRPAREN))
               {
               *pchCompiled++ = markCHARACTER;
               *pchCompiled++ = ch;
               }
            else // record that we can repeat the last expression
               {
               *pchLastEx |= markREPEAT;
               }
            break;

         case TEXT('$'):
            if (*pszExpr != TEXT('\0'))
               {
               *pchCompiled++ = markCHARACTER;
               *pchCompiled++ = ch;
               }
            else // record that we should match end-of-line
               {
               *pchCompiled++ = markENDLINE;
               }
            break;

         case TEXT('['):
            if ((ch = *pszExpr++) == '^')
               {
               *pchCompiled++ = markNONCHARSET;
               ch = *pszExpr++;
               }
            else
               {
               *pchCompiled++ = markCHARSET;
               }

            *pchCompiled++ = 1;            // length; this is pchLastEx[1]

            do {
               if (ch == TEXT('\0'))
                  {
                  SetLastError (ERROR_BAD_FORMAT); // unmatched "\("
                  rc = FALSE;
                  break;
                  }

               if ((ch == TEXT('-')) && (*pchCompiled != pchLastEx[2]))
                  {
                  if ((ch = *pszExpr++) == TEXT(']'))
                     {
                     *pchCompiled++ = TEXT('-');
                     pchLastEx[1]++;
                     break;
                     }
                  while ((BYTE)pchCompiled[-1] < (BYTE)ch)
                     {
                     *pchCompiled = pchCompiled[-1] + 1;
                     pchCompiled++;
                     pchLastEx[1]++;
                     if ((sizeof(TCHAR)*(pchCompiled - m_achCompiled)) > sizeof(m_achCompiled))
                        {
                        SetLastError (ERROR_META_EXPANSION_TOO_LONG);
                        rc = FALSE;
                        break;
                        }
                     }
                  }
               else
                  {
                  *pchCompiled++ = ch;
                  pchLastEx[1]++;

                  if ((sizeof(TCHAR)*(pchCompiled - m_achCompiled)) > sizeof(m_achCompiled))
                     {
                     SetLastError (ERROR_META_EXPANSION_TOO_LONG);
                     rc = FALSE;
                     break;
                     }
                  }

               } while ((ch = *pszExpr++) != TEXT(']'));
            break;

         case TEXT('\\'):
            if ((ch = *pszExpr++) == TEXT('('))
               {
               if (nParens >= nCOMPILED_PARENS_MAX)
                  {
                  SetLastError (ERROR_META_EXPANSION_TOO_LONG);
                  rc = FALSE;
                  break;
                  }
               *pParen++ = nParens;
               *pchCompiled++ = markLPAREN;
               *pchCompiled++ = nParens++;
               }
            else if (ch == TEXT(')'))
               {
               if (pParen == &aParens[0])
                  {
                  SetLastError (ERROR_BAD_FORMAT);
                  rc = FALSE;
                  break;
                  }
               *pchCompiled++ = markRPAREN;
               *pchCompiled++ = *--pParen;
               }
            else if ((ch >= TEXT('1')) && (ch < (TEXT('1') + nCOMPILED_PARENS_MAX)))
               {
               *pchCompiled++ = markREFERENCE;
               *pchCompiled++ = ch - '1';
               }
            else
               {
               *pchCompiled++ = markCHARACTER;
               *pchCompiled++ = ch;
               }
            break;

         default:
            *pchCompiled++ = markCHARACTER;
            *pchCompiled++ = ch;
            break;
         }
      }

   *pchCompiled++ = markENDPATTERN;
   *pchCompiled++ = 0;
   return rc;
}


BOOL REGEXP::Matches (LPCTSTR pszString)
{
   if (!pszString)
      return FALSE;

   // Prepare a place to store information about \( and \) pairs
   //
   LPCTSTR aParenStarts[ nCOMPILED_PARENS_MAX ];
   LPCTSTR aParenEnds[ nCOMPILED_PARENS_MAX ];

   for (size_t ii = 0; ii < nCOMPILED_PARENS_MAX; ii++)
      {
      aParenStarts[ii] = NULL;
      aParenEnds[ii] = NULL;
      }

   // If the expression starts with "^", we can do a quick pattern-match...
   //
   if (m_fMatchFromStart)
      {
      return MatchSubset (pszString, m_achCompiled, aParenStarts, aParenEnds);
      }

   // Otherwise, we have to work a little harder. If the expression
   // at least starts with a recognized character, we can scan for that
   // as the start of a pattern...
   //
   LPTSTR pchCompiled = m_achCompiled;
   if (*pchCompiled == markCHARACTER)
      {
      TCHAR chStart = pchCompiled[1];
      do {
         if (*pszString != chStart)
            continue;
         if (MatchSubset (pszString, pchCompiled, aParenStarts, aParenEnds))
            return TRUE;
         } while (*pszString++);

      return FALSE;
      }

   // If the expression starts with something weird, we'll have to test
   // against every character in the string.
   //
   do {
      if (MatchSubset (pszString, pchCompiled, aParenStarts, aParenEnds))
         return TRUE;
      } while (*pszString++);

   return FALSE;
}


BOOL REGEXP::MatchSubset (LPCTSTR pszString, LPCTSTR pchCompiled, LPCTSTR *aParenStarts, LPCTSTR *aParenEnds)
{
   LPCTSTR pchStartOfEx;
   int ii;
   int cchPattern;

   while (1)
   switch (*pchCompiled++)
      {
      case markCHARACTER:
         if (*pchCompiled++ == *pszString++)
            continue;
         return FALSE;

      case markANYCHAR:
         if (*pszString++)
            continue;
         return FALSE;

      case markENDLINE:
         if (*pszString == TEXT('\0'))
            continue;
         return FALSE;

      case markENDPATTERN:
         return TRUE;

      case markCHARSET:
         if (fIsInCharSet (pchCompiled, *pszString++, TRUE))
            {
            pchCompiled += *pchCompiled;
            continue;
            }
         return FALSE;

      case markNONCHARSET:
         if (fIsInCharSet (pchCompiled, *pszString++, FALSE))
            {
            pchCompiled += *pchCompiled;
            continue;
            }
         return FALSE;

      case markLPAREN:
         aParenStarts[*pchCompiled++] = pszString;
         continue;

      case markRPAREN:
         aParenEnds[*pchCompiled++] = pszString;
         continue;

      case markREFERENCE:
         if (aParenEnds[ii = *pchCompiled++] == 0)
            return FALSE; // reference to invalid \(\) pair
         if (CompareParen (ii, pszString, aParenStarts, aParenEnds))
            {
            pszString += aParenEnds[ii] - aParenStarts[ii];
            continue;
            }
         return FALSE;

      case markREFERENCE|markREPEAT:
         if (aParenEnds[ii = *pchCompiled++] == 0)
            return FALSE; // reference to invalid \(\) pair
         pchStartOfEx = pszString;
         cchPattern = aParenEnds[ii] - aParenStarts[ii];
         while (CompareParen (ii, pszString, aParenStarts, aParenEnds))
            pszString += cchPattern;
         while (pszString >= pchStartOfEx)
            {
            if (MatchSubset (pszString, pchCompiled, aParenStarts, aParenEnds))
               return TRUE;
            pszString -= cchPattern;
            }
         continue;

      case markANYCHAR|markREPEAT:
         pchStartOfEx = pszString;
         while (*pszString++)
            ;
         goto star;

      case markCHARACTER|markREPEAT:
         pchStartOfEx = pszString;
         while (*pszString++ == *pchCompiled)
            ;
         pchCompiled++;
         goto star;

      case markCHARSET|markREPEAT:
      case markNONCHARSET|markREPEAT:
         pchStartOfEx = pszString;
         while (fIsInCharSet (pchCompiled, *pszString++, (pchCompiled[-1] == (markCHARSET|markREPEAT))))
            ;
         pchCompiled += *pchCompiled;
         goto star;

      star:
         do {
            pszString--;
            if (MatchSubset (pszString, pchCompiled, aParenStarts, aParenEnds))
               return TRUE;
            } while (pszString > pchStartOfEx);
         return FALSE;

      default:
         return FALSE; // damaged compiled string
      }
}


BOOL REGEXP::CompareParen (int ii, LPCTSTR pszString, LPCTSTR *aParenStarts, LPCTSTR *aParenEnds)
{
   LPCTSTR pchInParen = aParenStarts[ii];
   while (*pchInParen++ == *pszString++)
      if (pchInParen >= aParenEnds[ii])
         return TRUE;
   return FALSE;
}


BOOL REGEXP::fIsInCharSet (LPCTSTR pszCharSet, TCHAR chTest, int fInclusive)
{
   if (chTest == 0)
      return FALSE;
   for (int n = (int)(*pszCharSet++); --n; )
      {
      if (*pszCharSet++ == chTest)
         return fInclusive;
      }
   return !fInclusive;
}

