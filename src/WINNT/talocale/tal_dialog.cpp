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

#include <WINNT/talocale.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

int cdecl vMessage (UINT, LONG, LONG, LPCTSTR, va_list);
DWORD WINAPI Message_ThreadProc (PVOID lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

HWND ModelessDialog (int idd, HWND hWndParent, DLGPROC lpDialogFunc)
{
   return ModelessDialogParam (idd, hWndParent, lpDialogFunc, 0);
}


HWND ModelessDialogParam (int idd, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
   HINSTANCE hInstFound;
   LPCDLGTEMPLATE pTemplate;
   if ((pTemplate = TaLocale_GetDialogResource (idd, &hInstFound)) == NULL)
      return NULL;

   return CreateDialogIndirectParam (hInstFound, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}


int ModalDialog (int idd, HWND hWndParent, DLGPROC lpDialogFunc)
{
   return ModalDialogParam (idd, hWndParent, lpDialogFunc, 0);
}


int ModalDialogParam (int idd, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
   HINSTANCE hInstFound;
   LPCDLGTEMPLATE pTemplate;
   if ((pTemplate = TaLocale_GetDialogResource (idd, &hInstFound)) == NULL)
      return NULL;

   return DialogBoxIndirectParam (hInstFound, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}


/*** Message() - generic-text dialog box
 *
 */

int cdecl Message (UINT type, LPCTSTR title, LPCTSTR text, LPCTSTR fmt, ...)
{
   va_list  arg;
   // if (fmt != NULL)
      va_start (arg, fmt);
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


int cdecl Message (UINT type, LPCTSTR title, int text, LPCTSTR fmt, ...)
{
   va_list  arg;
   // if (fmt != NULL)
      va_start (arg, fmt);
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


int cdecl Message (UINT type, int title, LPCTSTR text, LPCTSTR fmt, ...)
{
   va_list  arg;
   // if (fmt != NULL)
      va_start (arg, fmt);
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


int cdecl Message (UINT type, int title, int text, LPCTSTR fmt, ...)
{
   va_list  arg;
   // if (fmt != NULL)
      va_start (arg, fmt);
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


int cdecl vMessage (UINT type, LPCTSTR title, LPCTSTR text, LPCTSTR fmt, va_list arg)
{
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


int cdecl vMessage (UINT type, LPCTSTR title, int text, LPCTSTR fmt, va_list arg)
{
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


int cdecl vMessage (UINT type, int title, LPCTSTR text, LPCTSTR fmt, va_list arg)
{
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


int cdecl vMessage (UINT type, int title, int text, LPCTSTR fmt, va_list arg)
{
   return vMessage (type, (LONG)title, (LONG)text, fmt, arg);
}


typedef struct
   {
   UINT dwType;
   LPTSTR pszTitle;
   LPTSTR pszText;
   } MESSAGE_PARAMS, *LPMESSAGE_PARAMS;

int cdecl vMessage (UINT type, LONG title, LONG text, LPCTSTR fmt, va_list arg)
{
   LPMESSAGE_PARAMS pmp = New(MESSAGE_PARAMS);

   pmp->dwType = type;

   if ((pmp->pszTitle = FormatString (title, fmt, arg)) == NULL)
      {
      Delete(pmp);
      return IDCANCEL;
      }

   if ((pmp->pszText = vFormatString (text, fmt, arg)) == NULL)
      {
      FreeString (pmp->pszTitle);
      Delete(pmp);
      return IDCANCEL;
      }

   if (!( pmp->dwType & 0xF0 )) // no icon requested?  pick one.
      {
      pmp->dwType |= ((pmp->dwType & 0x0F) ? MB_ICONQUESTION : MB_ICONASTERISK);
      }

   if (pmp->dwType & MB_MODELESS)
      {
      pmp->dwType &= ~MB_MODELESS;

      HANDLE hThread;
      if ((hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)Message_ThreadProc, pmp, 0, NULL)) != NULL)
         SetThreadPriority (hThread, THREAD_PRIORITY_BELOW_NORMAL);

      return -1; // threaded--who knows what button was hit.
      }

   return Message_ThreadProc (pmp);
}


DWORD WINAPI Message_ThreadProc (PVOID lp)
{
   LPMESSAGE_PARAMS pmp = (LPMESSAGE_PARAMS)lp;

   DWORD rc = MessageBox (NULL, pmp->pszText, pmp->pszTitle, pmp->dwType);

   FreeString (pmp->pszText);
   FreeString (pmp->pszTitle);
   Delete(pmp);

   return rc;
}

