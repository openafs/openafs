/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <windows.h>
#include "multistring.h"


static int lstrncmpi (LPCTSTR pszA, LPCTSTR pszB, size_t cch)
{
   if (!pszA || !pszB)
      {
      return (!pszB) - (!pszA);   // A,!B:1, !A,B:-1, !A,!B:0
      }

   for ( ; cch > 0; cch--, pszA = CharNext(pszA), pszB = CharNext(pszB))
      {
      TCHAR chA = toupper( *pszA );
      TCHAR chB = toupper( *pszB );

      if (!chA || !chB)
         return (!chB) - (!chA);    // A,!B:1, !A,B:-1, !A,!B:0

      if (chA != chB)
         return (int)(chA) - (int)(chB);   // -1:A<B, 0:A==B, 1:A>B
      }

   return 0;  // no differences before told to stop comparing, so A==B
}


/*
 * mstralloc - Allocates space for a given-length multistring
 * mstrfree - Frees a multistring
 * mstrwalk - Allows iterative progression along a multistring
 * mstrlen - Returns the length of a given multistring, including all \0's
 * mstrcount - Returns the number of entries in a given multistring
 * mstrstr - Determines if a given substring exists in a multistring
 * mstrcat - Adds a substring to a multistring (doesn't do any checking first)
 * mstrdel - Removes a substring from a multistring
 *
 */

LPTSTR mstralloc (size_t cchMax)
{
   LPTSTR msz;
   if ((msz = (LPTSTR)GlobalAlloc (GMEM_FIXED, sizeof(TCHAR) * (1+cchMax))) != NULL)
      memset (msz, 0x00, sizeof(TCHAR) * (1+cchMax));
   return msz;
}


void mstrfree (LPCTSTR msz)
{
   if (msz)
      GlobalFree ((HGLOBAL)msz);
}


BOOL mstrwalk (LPCTSTR msz, TCHAR chSep, LPTSTR *ppSegment, size_t *pchSegment)
{
   // If the caller supplied {*ppSegment} as NULL, we should return the
   // first segment. Otherwise, advance {*pchSegment} characters and
   // return the next segment.
   //
   if (!*ppSegment)
      *ppSegment = (LPTSTR)msz;
   else if (*(*ppSegment += *pchSegment) == chSep)
      (*ppSegment) ++;
   else
      return FALSE;

   if (!*ppSegment || !*(*ppSegment))
      return FALSE;

   *pchSegment = 0;
   while ((*ppSegment)[*pchSegment] && ((*ppSegment)[*pchSegment] != chSep))
      (*pchSegment)++;

   return TRUE;
}


size_t mstrlen (LPCTSTR msz, TCHAR chSep)
{
   LPTSTR pSegment = NULL;
   size_t cchSegment = 0;

   size_t cchTotal = 0;
   while (mstrwalk (msz, chSep, &pSegment, &cchSegment))
      cchTotal += cchSegment+1;

   if (!chSep || !cchTotal)
      cchTotal ++;     // To terminate the string

   return cchTotal;
}


size_t mstrcount (LPCTSTR msz, TCHAR chSep)
{
   LPTSTR pSegment = NULL;
   size_t cchSegment = 0;

   size_t cSegments = 0;
   while (mstrwalk (msz, chSep, &pSegment, &cchSegment))
      cSegments ++;

   return cSegments;
}


BOOL mstrstr (LPCTSTR msz, TCHAR chSep, LPCTSTR pszTest)
{
   LPTSTR pSegment = NULL;
   size_t cchSegment = 0;

   size_t cchTotal = 0;
   while (mstrwalk (msz, chSep, &pSegment, &cchSegment))
      {
      if ( (cchSegment == (size_t)lstrlen(pszTest)) &&
           (!lstrncmpi (pSegment, pszTest, cchSegment)) )
         return TRUE;
      }

   return FALSE;
}


BOOL mstrcat (LPTSTR *pmsz, TCHAR chSep, LPCTSTR pszAppend)
{
   size_t cchOld = mstrlen(*pmsz,chSep);
   size_t cchAdd = (pszAppend) ? lstrlen(pszAppend) : 0;
   size_t cchRetain = (chSep && (cchOld!=1)) ? cchOld : (cchOld-1);

   LPTSTR mszNew;
   if ((mszNew = mstralloc (cchRetain + cchAdd + 2)) == NULL)
      return FALSE;

   if (cchRetain)
      memcpy (mszNew, *pmsz, sizeof(TCHAR) * cchRetain);

   if (cchRetain)
      mszNew[ cchRetain-1 ] = chSep;

   lstrcpy (&mszNew[ cchRetain ], pszAppend);

   if (!chSep)
      mszNew[ cchRetain + cchAdd +1 ] = 0;

   if (*pmsz)
      mstrfree (*pmsz);
   *pmsz = mszNew;
   return TRUE;
}


BOOL mstrdel (LPTSTR *pmsz, TCHAR chSep, LPCTSTR pszRemove)
{
   LPTSTR mszNew;
   if ((mszNew = mstralloc (mstrlen(*pmsz,chSep))) == NULL)
      return FALSE;

   LPTSTR pSegmentWrite = mszNew;
   LPTSTR pSegmentRead = NULL;
   size_t cchSegment = 0;

   size_t cchTotal = 0;
   while (mstrwalk (*pmsz, chSep, &pSegmentRead, &cchSegment))
      {
      if ( (cchSegment == (size_t)lstrlen(pszRemove)) &&
           (!lstrncmpi (pSegmentRead, pszRemove, cchSegment)) )
         continue;

      for (size_t ich = 0; ich < cchSegment; ++ich)
         *pSegmentWrite++ = pSegmentRead[ ich ];
      *pSegmentWrite++ = chSep;
      }
   if ((pSegmentWrite != mszNew) && (chSep)) // don't need trailing separator
      pSegmentWrite--;
   *pSegmentWrite = 0;

   if (*pmsz)
      mstrfree (*pmsz);
   *pmsz = mszNew;
   return TRUE;
}

