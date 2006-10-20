/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef MULTISTRING_H
#define MULTISTRING_H


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

LPTSTR mstralloc (size_t cchMax);
void mstrfree (LPCTSTR msz);
BOOL mstrwalk (LPCTSTR msz, TCHAR chSep, LPTSTR *ppSegment, size_t *pchSegment);
size_t mstrlen (LPCTSTR msz, TCHAR chSep);
size_t mstrcount (LPCTSTR msz, TCHAR chSep);
BOOL mstrstr (LPCTSTR msz, TCHAR chSep, LPCTSTR pszTest);
BOOL mstrcat (LPTSTR *pmsz, TCHAR chSep, LPCTSTR pszAppend);
BOOL mstrdel (LPTSTR *pmsz, TCHAR chSep, LPCTSTR pszRemove);


#endif

