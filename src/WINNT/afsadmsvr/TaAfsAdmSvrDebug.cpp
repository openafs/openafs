extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsAdmSvrInternal.h"


/*
 * VARIABLES __________________________________________________________________
 *
 */

static DWORD PrintDetailLevel = dlDEFAULT;


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void cdecl vPrint (DWORD level, LPTSTR pszLine, va_list arg)
{
   static LPCRITICAL_SECTION pcs = NULL;
   if (!pcs)
      {
      pcs = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcs);
      }

   EnterCriticalSection (pcs);

   if ((!level) || (PrintDetailLevel & level))
      {
      TCHAR szOut[ 1024 ];
      wvsprintf (szOut, pszLine, arg);
      printf ("AdmSvr: ");
      if (!level)
         printf (" * ");
      if (level & dlINDENT1)
         printf ("   ");
      if (level & dlINDENT2)
         printf ("      ");
      printf ("%s\n", szOut);
      }

   LeaveCriticalSection (pcs);
}


void cdecl Print (LPTSTR pszLine, ...)
{
   va_list arg;
   va_start (arg, pszLine);
   vPrint (dlSTANDARD, pszLine, arg);
}


void cdecl Print (DWORD level, LPTSTR pszLine, ...)
{
   va_list arg;
   va_start (arg, pszLine);
   vPrint (level, pszLine, arg);
}


DWORD GetPrintDetailLevel (void)
{
   return PrintDetailLevel;
}


void SetPrintDetailLevel (DWORD level)
{
   PrintDetailLevel = level;
}

