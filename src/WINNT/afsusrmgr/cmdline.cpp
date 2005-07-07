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
#include "cmdline.h"

extern "C" {
#include <afs/afs_AdminErrors.h>
} // extern "C"


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef enum {
   swCELL,
   swREMOTE,
   swUSER,
   swPASSWORD
} SWITCH;

static struct {
   TCHAR szSwitch[ cchRESOURCE ];
   BOOL fGetsValue;
   BOOL fPresent;
   TCHAR szValue[ cchRESOURCE ];
} aSWITCHES[] = {
   { TEXT("cell"),      TRUE,	FALSE,	TEXT("") },
   { TEXT("remote"),    TRUE,	FALSE,	TEXT("") },
   { TEXT("user"),      TRUE,	FALSE,	TEXT("") },
   { TEXT("password"),  TRUE,	FALSE,	TEXT("") }
};

#define nSWITCHES (sizeof(aSWITCHES) / sizeof(aSWITCHES[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

#define iswhite(_ch) ( ((_ch)==TEXT(' ')) || ((_ch)==TEXT('\t')) )

void cdecl CommandLineHelp (int ids, LPTSTR pszFormat = NULL, ...);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

CMDLINEOP ParseCommandLine (LPTSTR pszCmdLine)
{
   size_t ii;
   for (ii = 0; ii < nSWITCHES; ++ii)
      aSWITCHES[ ii ].fPresent = FALSE;

   // Search through pszCmdLine for switches; each switch must be
   // preceeded by "/" or "-".
   //
   while (pszCmdLine && *pszCmdLine)
      {
      while (iswhite(*pszCmdLine))
         ++pszCmdLine;
      if (!*pszCmdLine)
         break;

      if ( (*pszCmdLine != '-') && (*pszCmdLine != '/') )
         {
         CommandLineHelp (IDS_CMDLINE_SYNTAX);
         return opCLOSEAPP;
         }

      ++pszCmdLine;

      // Okay, we've found what is probably the start of a switch.
      // See if it matches anything.
      //
      for (ii = 0; ii < nSWITCHES; ++ii)
         {
         size_t cch = lstrlen(aSWITCHES[ ii ].szSwitch);
         if (lstrncmpi (pszCmdLine, aSWITCHES[ ii ].szSwitch, cch))
            continue;

         // If the switch wants a value, it must be followed by ":"
         // or whitespace; if it doesn't, it must be followed by "/"
         // or whitespace.
         //
         LPTSTR pszAfter = &pszCmdLine[ cch ];
         if (iswhite (*pszAfter) || (!*pszAfter) ||
             ((*pszAfter == TEXT(':')) && (aSWITCHES[ ii ].fGetsValue)) ||
             ((*pszAfter == TEXT('/')) && (!aSWITCHES[ ii ].fGetsValue)) )
            {
            break; // found a switch!
            }
         }

      if (ii >= nSWITCHES)
         {
         TCHAR szCopy[ cchRESOURCE ];
         lstrcpy (szCopy, pszCmdLine);
         LPTSTR pch;
         for (pch = szCopy;
              *pch && !iswhite(*pch) && !(*pch == TEXT('/')) && !(*pch == TEXT(':'));
              ++pch)
            ;
         *pch = TEXT('\0');
         CommandLineHelp (IDS_CMDLINE_UNRECOGNIZED, TEXT("%s"), szCopy);
         return opCLOSEAPP;
         }
      if (aSWITCHES[ ii ].fPresent)
         {
         CommandLineHelp (IDS_CMDLINE_DUPLICATE, TEXT("%s"), aSWITCHES[ ii ].szSwitch);
         return opCLOSEAPP;
         }

      // Woo hoo!  Found what appears to be a valid switch.  Parse it now.
      //
      aSWITCHES[ ii ].fPresent = TRUE;
      pszCmdLine += lstrlen( aSWITCHES[ ii ].szSwitch );
      while (iswhite (*pszCmdLine))
         ++pszCmdLine;

      if (*pszCmdLine == TEXT(':'))
         {
         if (!aSWITCHES[ ii ].fGetsValue)
            {
            CommandLineHelp (IDS_CMDLINE_UNEXPECTVALUE, TEXT("%s"), aSWITCHES[ ii ].szSwitch);
            return opCLOSEAPP;
            }
         for (++pszCmdLine; iswhite (*pszCmdLine); ++pszCmdLine)
            ;
         }

      if (aSWITCHES[ ii ].fGetsValue)
         {
         if ( (*pszCmdLine == TEXT('/')) || (*pszCmdLine == TEXT('\0')) )
            {
            CommandLineHelp (IDS_CMDLINE_MISSINGVAL, TEXT("%s"), aSWITCHES[ ii ].szSwitch);
            return opCLOSEAPP;
            }
         BOOL fQuoted = FALSE;
         LPTSTR pszTarget;
         for (pszTarget = aSWITCHES[ ii ].szValue;
              *pszCmdLine && !(*pszCmdLine == TEXT('/') && !fQuoted)
                          && !(iswhite(*pszCmdLine) && !fQuoted); )
            {
            if (*pszCmdLine == TEXT('"'))
               {
               ++pszCmdLine;
               if (fQuoted)
                  break;
               fQuoted = TRUE;
               continue;
               }
            *pszTarget++ = *pszCmdLine++;
            }
         *pszTarget++ = TEXT('\0');
         }
      }

   // Now test the command-line for syntactical correctness.
   // First case: if the USER or PASSWORD switch is given, the other
   // is required.
   //
   if ( (aSWITCHES[ swUSER ].fPresent && !aSWITCHES[ swPASSWORD ].fPresent) ||
        (aSWITCHES[ swPASSWORD ].fPresent && !aSWITCHES[ swUSER ].fPresent) )
      {
      CommandLineHelp (IDS_CMDLINE_USERPASSWORD);
      return opCLOSEAPP;
      }

   // Connect to an administration server
   //
   if (aSWITCHES[ swREMOTE ].fPresent)
      {
      ULONG status;
      if (!AfsAppLib_OpenAdminServer (aSWITCHES[ swREMOTE ].szValue, &status))
         {
         if (status == ADMCLIENTCANTINITAFSLOCATION)
            ImmediateErrorDialog (status, IDS_ERROR_BAD_REMOTE_SERVER_INSTALL, aSWITCHES[ swREMOTE ].szValue);
         else
            ImmediateErrorDialog (status, IDS_ERROR_BAD_REMOTE_SERVER, TEXT("%s"), aSWITCHES[ swREMOTE ].szValue);
         return opCLOSEAPP;
         }
      }
   else // (!aSWITCHES[ swREMOTE ].fPresent)
      {
      ULONG status;
      if (!AfsAppLib_OpenAdminServer (NULL, &status))
         {
         if (status == ADMCLIENTCANTINITAFSLOCATION)
            ImmediateErrorDialog (status, IDS_ERROR_BAD_REMOTE_SERVER_INSTALL, aSWITCHES[ swREMOTE ].szValue);
         else
            ImmediateErrorDialog (status, IDS_ERROR_BAD_LOCAL_SERVER, TEXT("%s"), aSWITCHES[ swREMOTE ].szValue);
         return opCLOSEAPP;
         }
      }
   g.idClient = AfsAppLib_GetAdminServerClientID();

   // Obtain credentials
   //
   if (aSWITCHES[ swUSER ].fPresent)
      {
      LPTSTR pszCell = (aSWITCHES[ swCELL ].fPresent) ? (aSWITCHES[ swCELL ].szValue) : NULL;

      ULONG status;
      if (!AfsAppLib_SetCredentials (pszCell, aSWITCHES[ swUSER ].szValue, aSWITCHES[ swPASSWORD ].szValue, &status))
         {
         ImmediateErrorDialog (status, IDS_ERROR_BAD_CREDENTIALS);
         return opCLOSEAPP;
         }
      }

   // If the user asked us to, try to open the specified cell
   //
   if (aSWITCHES[ swCELL ].fPresent)
      {
      LPOPENCELL_PARAMS lpp = New (OPENCELL_PARAMS);
      memset (lpp, 0x00, sizeof(OPENCELL_PARAMS));
      lstrcpy (lpp->szCell, aSWITCHES[ swCELL ].szValue);
      lpp->fCloseAppOnFail = TRUE;
      StartTask (taskOPENCELL, NULL, lpp);
      return opNOCELLDIALOG;
      }

   // Okay--nothing sufficiently special took place to prevent us
   // from running the tool normally.
   //
   return opNORMAL;
}


void cdecl CommandLineHelp (int ids, LPTSTR pszFormat, ...)
{
   va_list   arg;
   va_start (arg, pszFormat);
   vMessage (MB_OK | MB_ICONHAND, IDS_CMDLINE_TITLE, ids, pszFormat, arg);
}

