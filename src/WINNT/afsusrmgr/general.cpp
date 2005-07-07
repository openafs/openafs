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

#include "TaAfsUsrMgr.h"
#include "usr_col.h"
#include "grp_col.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL fIsValidDate (LPSYSTEMTIME pst)
{
   return (pst->wYear >= 1971) ? TRUE : FALSE;
}


void FormatElapsedSeconds (LPTSTR pszText, LONG csec)
{
   LPTSTR pszTarget = pszText;

   if (csec >= csec1WEEK)
      {
      if (pszTarget != pszText)
         *pszTarget++ = TEXT(' ');
      LPTSTR psz = FormatString (IDS_COUNT_WEEKS, TEXT("%lu"), csec / csec1WEEK);
      csec %= csec1WEEK;
      lstrcpy (pszTarget, psz);
      pszTarget += lstrlen(pszTarget);
      FreeString (psz);
      }

   if (csec >= csec1DAY)
      {
      if (pszTarget != pszText)
         *pszTarget++ = TEXT(' ');
      LPTSTR psz = FormatString (IDS_COUNT_DAYS, TEXT("%lu"), csec / csec1DAY);
      csec %= csec1DAY;
      lstrcpy (pszTarget, psz);
      pszTarget += lstrlen(pszTarget);
      FreeString (psz);
      }

   if (csec >= csec1HOUR)
      {
      if (pszTarget != pszText)
         *pszTarget++ = TEXT(' ');
      LPTSTR psz = FormatString (IDS_COUNT_HOURS, TEXT("%lu"), csec / csec1HOUR);
      csec %= csec1HOUR;
      lstrcpy (pszTarget, psz);
      pszTarget += lstrlen(pszTarget);
      FreeString (psz);
      }

   if (csec >= csec1MINUTE)
      {
      if (pszTarget != pszText)
         *pszTarget++ = TEXT(' ');
      LPTSTR psz = FormatString (IDS_COUNT_MINUTES, TEXT("%lu"), csec / csec1MINUTE);
      csec %= csec1MINUTE;
      lstrcpy (pszTarget, psz);
      pszTarget += lstrlen(pszTarget);
      FreeString (psz);
      }

   if (csec || (pszTarget == pszText))
      {
      if (pszTarget != pszText)
         *pszTarget++ = TEXT(' ');
      LPTSTR psz = FormatString (IDS_COUNT_SECONDS, TEXT("%lu"), csec);
      lstrcpy (pszTarget, psz);
      pszTarget += lstrlen(pszTarget);
      FreeString (psz);
      }

   *pszTarget = TEXT('\0');
}


LPTSTR CreateNameList (LPASIDLIST pAsidList, int idsHeader)
{
   LPTSTR pszOut;
   if (idsHeader)
      pszOut = FormatString (TEXT("%1"), TEXT("%m"), idsHeader);
   else
      pszOut = NULL;

   for (size_t ii = 0; ii < pAsidList->cEntries; ++ii)
      {
      ULONG status;
      TCHAR szName[ cchRESOURCE ];
      if (!asc_ObjectNameGet_Fast (g.idClient, g.idCell, pAsidList->aEntries[ ii ].idObject, szName, &status))
         continue;

      static TCHAR szSeparator[ cchRESOURCE ] = TEXT("");
      if (szSeparator[0] == TEXT('\0'))
         {
         if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SLIST, szSeparator, cchRESOURCE))
            lstrcpy (szSeparator, TEXT(","));
         lstrcat (szSeparator, TEXT(" "));
         }

      ASOBJPROP Properties;
      if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAsidList->aEntries[ ii ].idObject, &Properties, &status))
         {
         if (Properties.Type == TYPE_USER)
            AppendUID (szName, Properties.u.UserProperties.PTSINFO.uidName);
         else if (Properties.Type == TYPE_GROUP)
            AppendUID (szName, Properties.u.GroupProperties.uidName);
         }

      LPTSTR pszNext;
      if (!pszOut)
         pszNext = FormatString (TEXT("%1"), TEXT("%s"), szName);
      else if (ii == 0)
         pszNext = FormatString (TEXT("%1%2"), TEXT("%s%s"), pszOut, szName);
      else // (pszOut && ii)
         pszNext = FormatString (TEXT("%1%2%3"), TEXT("%s%s%s"), pszOut, szSeparator, szName);

      if (pszOut)
         FreeString (pszOut);
      pszOut = pszNext;
      }

   return pszOut;
}


void GetLocalSystemTime (LPSYSTEMTIME pst)
{
   SYSTEMTIME st_gmt;
   GetSystemTime (&st_gmt);

   FILETIME ft_gmt;
   SystemTimeToFileTime (&st_gmt, &ft_gmt);

   FILETIME ft_local;
   FileTimeToLocalFileTime (&ft_gmt, &ft_local);

   FileTimeToSystemTime (&ft_local, pst);
}


void FormatServerKey (LPTSTR psz, PBYTE pKey)
{
   size_t ii;
   for (ii = 0; ii < ENCRYPTIONKEYLENGTH; ++ii)
      {
      if (pKey[ii])
         break;
      }
   if (ii == ENCRYPTIONKEYLENGTH)
      {
      GetString (psz, IDS_USER_KEY_HIDDEN);
      return;
      }

   for (ii = 0; ii < ENCRYPTIONKEYLENGTH; ++ii)
      {
	  WORD ch1 = (WORD)(pKey[ii]) / 64;
	  WORD ch2 = ((WORD)(pKey[ii]) - (ch1 * 64)) / 8;
	  WORD ch3 = ((WORD)(pKey[ii]) - (ch1 * 64) - (ch2 * 8));
	  wsprintf (psz, TEXT("\\%0d%0d%0d"), ch1, ch2, ch3);
	  psz += 4;
      }

   *psz = TEXT('\0');
}


BOOL ScanServerKey (PBYTE pKey, LPTSTR psz)
{
   size_t ich;
   for (ich = 0; psz && *psz; )
      {
      if (ich == ENCRYPTIONKEYLENGTH)
         return FALSE;

      if (*psz != '\\')
         {
         pKey[ ich++ ] = (BYTE)*psz++;
         continue;
         }

      if ((lstrlen(psz) < 4) || (!isdigit(*(1+psz))))
         return FALSE;

      ++psz; // skip the backslash
      WORD ch1 = (WORD)((*psz++) - TEXT('0'));
      WORD ch2 = (WORD)((*psz++) - TEXT('0'));
      WORD ch3 = (WORD)((*psz++) - TEXT('0'));
      pKey[ ich++ ] = (BYTE)( ch1 * 64 + ch2 * 8 + ch3 );
      }

   return (ich == ENCRYPTIONKEYLENGTH) ? TRUE : FALSE;
}


int CALLBACK General_ListSortFunction (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2)
{
   // Find out what kind of sort we're performing. To speed things up,
   // we'll only gather this information when we start a sort
   //
   static TABTYPE tt;
   static int iCol;
   static BOOL fReverse;
   static COLUMNTYPE ct;
   static GetColumnFunction fnGetColumn = NULL;
   if (!hItem1 || !hItem2)
      {
      switch (GetWindowLong (hList, GWL_ID))
         {
         case IDC_USERS_LIST:
            tt = ttUSERS;
            break;
         case IDC_GROUPS_LIST:
            tt = ttGROUPS;
            break;
         default:
            return 0;
         }

      size_t iColumn;
      FastList_GetSortStyle (hList, &iColumn, &fReverse);

      switch (tt)
         {
         case ttUSERS:
            iCol = gr.viewUsr.aColumns[ iColumn ];
            fnGetColumn = (GetColumnFunction)User_GetColumn;
            break;
         case ttGROUPS:
            iCol = gr.viewGrp.aColumns[ iColumn ];
            fnGetColumn = (GetColumnFunction)Group_GetColumn;
            break;
         }

      if (fnGetColumn)
         (*fnGetColumn)(g.idCell, iCol, NULL, NULL, NULL, &ct);
      return 0;
      }

   if (!fnGetColumn)
      return 0;

   // Perform the sort
   //
   TCHAR szTextA[ 1024 ];
   TCHAR szTextB[ 1024 ];
   FILETIME ftTextA = { 0, 0 };
   FILETIME ftTextB = { 0, 0 };

   ASID idItem1 = (ASID)( (fReverse) ? lpItem2 : lpItem1 );
   ASID idItem2 = (ASID)( (fReverse) ? lpItem1 : lpItem2 );

   switch (ct)
      {
      case ctALPHABETIC:
         (*fnGetColumn)(idItem1, iCol, szTextA, NULL, NULL, NULL);
         (*fnGetColumn)(idItem2, iCol, szTextB, NULL, NULL, NULL);
         return lstrcmpi (szTextA, szTextB);

      case ctNUMERIC:
         (*fnGetColumn)(idItem1, iCol, szTextA, NULL, NULL, NULL);
         (*fnGetColumn)(idItem2, iCol, szTextB, NULL, NULL, NULL);
         return atol(szTextA) - atol(szTextB);

      case ctDATE:
         SYSTEMTIME stTextA;
         SYSTEMTIME stTextB;
         (*fnGetColumn)(idItem1, iCol, NULL, &stTextA, NULL, NULL);
         (*fnGetColumn)(idItem2, iCol, NULL, &stTextB, NULL, NULL);
         SystemTimeToFileTime (&stTextA, &ftTextA);
         SystemTimeToFileTime (&stTextB, &ftTextB);
         return CompareFileTime (&ftTextA, &ftTextB);

      case ctELAPSED:
         LONG csecTextA;
         LONG csecTextB;
         (*fnGetColumn)(idItem1, iCol, NULL, NULL, &csecTextA, NULL);
         (*fnGetColumn)(idItem2, iCol, NULL, NULL, &csecTextB, NULL);
         return csecTextA - csecTextB;
      }

   return 0;
}


void AppendUID (LPTSTR psz, int uid)
{
   if (uid != 0)
      wsprintf (&psz[ lstrlen(psz) ], TEXT(" (%ld)"), uid);
}


LPTSTR GetEditText (HWND hEdit)
{
   size_t cch = 1 + SendMessage (hEdit, EM_LINELENGTH, 0, 0);
   LPTSTR psz = AllocateString (cch);
   cch = GetWindowText (hEdit, psz, cch);
   psz[cch] = TEXT('\0');
   return psz;
}


BOOL fIsMachineAccount (ASID idAccount)
{
   TCHAR szName[ cchNAME ];
   if (!asc_ObjectNameGet_Fast (g.idClient, g.idCell, idAccount, szName))
      return FALSE;
   return fIsMachineAccount (szName);
}


BOOL fIsMachineAccount (LPTSTR pszName)
{
   for ( ; pszName && *pszName; ++pszName)
      {
      if (!( (*pszName == TEXT('.')) || ((*pszName >= TEXT('0')) && (*pszName <= TEXT('9'))) ))
         return FALSE;
      }

   return TRUE;
}

