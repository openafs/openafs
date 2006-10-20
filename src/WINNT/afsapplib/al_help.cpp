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

#include <WINNT/afsapplib.h>


/*
 * DEFINITIONS ______________________________________________________________
 *
 */

typedef struct 
   {
   BOOL fInUse;
   int idd;
   DWORD *adwContext;
   int idhOverview;
   } DIALOGHELP, *LPDIALOGHELP;


/*
 * VARIABLES ________________________________________________________________
 *
 */

static TCHAR g_szHelpfile[ MAX_PATH ] = TEXT("");

static DIALOGHELP *g_adh = NULL;
static size_t g_cdh = 0;


/*
 * ROUTINES _________________________________________________________________
 *
 */

void AfsAppLib_RegisterHelpFile (LPTSTR pszFilename)
{
   if (pszFilename)
      lstrcpy (g_szHelpfile, pszFilename);
   else
      g_szHelpfile[0] = TEXT('\0');
}


void AfsAppLib_RegisterHelp (int idd, DWORD *adwContext, int idhOverview)
{
   size_t ih;
   for (ih = 0; ih < g_cdh; ++ih)
      {
      if (!g_adh[ ih ].fInUse)
         continue;
      if (g_adh[ ih ].idd == idd)
         {
         g_adh[ ih ].adwContext = adwContext;
         g_adh[ ih ].idhOverview = idhOverview;
         return;
         }
      }
   for (ih = 0; ih < g_cdh; ++ih)
      {
      if (!g_adh[ ih ].fInUse)
         break;
      }
   if (ih == g_cdh)
      {
      (void)REALLOC (g_adh, g_cdh, 1+ih, 16);
      }
   if (ih < g_cdh)
      {
      g_adh[ ih ].fInUse = TRUE;
      g_adh[ ih ].idd = idd;
      g_adh[ ih ].adwContext = adwContext;
      g_adh[ ih ].idhOverview = idhOverview;
      }
}


BOOL AfsAppLib_HandleHelp (int idd, HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   BOOL rc = FALSE;
   size_t ih;

   switch (msg)
      {
      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDHELP:
               for (ih = 0; ih < g_cdh; ++ih)
                  {
                  if (idd == g_adh[ ih ].idd)
                     break;
                  }
               if (ih < g_cdh)
                  {
                  if (g_szHelpfile)
                     WinHelp (hDlg, g_szHelpfile, HELP_CONTEXT, g_adh[ ih ].idhOverview);
                  rc = TRUE;
                  }
               break;
            }
         break;

      case WM_HELP:
         LPHELPINFO lphi;
         for (ih = 0; ih < g_cdh; ++ih)
            {
            if (idd == g_adh[ ih ].idd)
               break;
            }

         if ( (ih == g_cdh) ||
              ((lphi = (LPHELPINFO)lp) == NULL) ||
              (lphi->hItemHandle == NULL) ||
              (lphi->hItemHandle == hDlg) )
            {
            PostMessage (hDlg, WM_COMMAND, (WPARAM)MAKELONG(IDHELP,BN_CLICKED), (LPARAM)GetDlgItem (hDlg, IDHELP));
            }
         else
            {
            if (g_szHelpfile)
               WinHelp ((HWND)(lphi->hItemHandle), g_szHelpfile, HELP_WM_HELP, (DWORD)g_adh[ ih ].adwContext);
            }

         rc = TRUE;
         break;
      }

   return rc;
}

