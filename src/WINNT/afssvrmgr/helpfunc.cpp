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

#include "svrmgr.h"
#include "helpfunc.h"
#include <stdlib.h>

#ifndef iswhite
#define iswhite(_ch) ( ((_ch) == TEXT(' ')) || ((_ch) == TEXT('\t')) )
#endif


/*
 * VARIABLES ________________________________________________________________
 *
 */

typedef enum {
   uuUNSPECIFIED,
   uuVOS,	// VOS commands
   uuBOS,	// BOS commands
   uuKAS,	// KAS commands
   uuFS	// FS commands
} UNIXUTIL;

// One entry per help context
static struct {
   UNIXUTIL uu;
   int ids;
   int hid;
} aCOMMANDS[] = {
   { uuVOS, IDS_COMMAND_VOS_ADDSITE,         IDH_SVRMGR_COMMAND_VOS_ADDSITE },
   { uuVOS, IDS_COMMAND_VOS_BACKUP,          IDH_SVRMGR_COMMAND_VOS_BACKUP },
   { uuVOS, IDS_COMMAND_VOS_BACKUPSYS,       IDH_SVRMGR_COMMAND_VOS_BACKUPSYS },
   { uuVOS, IDS_COMMAND_VOS_CREATE,          IDH_SVRMGR_COMMAND_VOS_CREATE },
   { uuVOS, IDS_COMMAND_VOS_DELENTRY,        IDH_SVRMGR_COMMAND_VOS_DELENTRY },
   { uuVOS, IDS_COMMAND_VOS_DUMP,            IDH_SVRMGR_COMMAND_VOS_DUMP },
   { uuVOS, IDS_COMMAND_VOS_EXAMINE,         IDH_SVRMGR_COMMAND_VOS_EXAMINE },
   { uuVOS, IDS_COMMAND_VOS_LISTPART,        IDH_SVRMGR_COMMAND_VOS_LISTPART },
   { uuVOS, IDS_COMMAND_VOS_LISTVLDB,        IDH_SVRMGR_COMMAND_VOS_LISTVLDB },
   { uuVOS, IDS_COMMAND_VOS_LISTVOL,         IDH_SVRMGR_COMMAND_VOS_LISTVOL },
   { uuVOS, IDS_COMMAND_VOS_LOCK,            IDH_SVRMGR_COMMAND_VOS_LOCK },
   { uuVOS, IDS_COMMAND_VOS_MOVE,            IDH_SVRMGR_COMMAND_VOS_MOVE },
   { uuVOS, IDS_COMMAND_VOS_PARTINFO,        IDH_SVRMGR_COMMAND_VOS_PARTINFO },
   { uuVOS, IDS_COMMAND_VOS_RELEASE,         IDH_SVRMGR_COMMAND_VOS_RELEASE },
   { uuVOS, IDS_COMMAND_VOS_REMOVE,          IDH_SVRMGR_COMMAND_VOS_REMOVE },
   { uuVOS, IDS_COMMAND_VOS_REMSITE,         IDH_SVRMGR_COMMAND_VOS_REMSITE },
   { uuVOS, IDS_COMMAND_VOS_RENAME,          IDH_SVRMGR_COMMAND_VOS_RENAME },
   { uuVOS, IDS_COMMAND_VOS_RESTORE,         IDH_SVRMGR_COMMAND_VOS_RESTORE },
   { uuVOS, IDS_COMMAND_VOS_SYNCVLDB,        IDH_SVRMGR_COMMAND_VOS_SYNCVLDB },
   { uuVOS, IDS_COMMAND_VOS_UNLOCK,          IDH_SVRMGR_COMMAND_VOS_UNLOCK },
   { uuVOS, IDS_COMMAND_VOS_UNLOCKVLDB,      IDH_SVRMGR_COMMAND_VOS_UNLOCKVLDB },
   { uuVOS, IDS_COMMAND_VOS_ZAP,             IDH_SVRMGR_COMMAND_VOS_ZAP },
   { uuBOS, IDS_COMMAND_BOS_ADDHOST,         IDH_SVRMGR_COMMAND_BOS_ADDHOST },
   { uuBOS, IDS_COMMAND_BOS_ADDKEY,          IDH_SVRMGR_COMMAND_BOS_ADDKEY },
   { uuBOS, IDS_COMMAND_BOS_ADDUSER,         IDH_SVRMGR_COMMAND_BOS_ADDUSER },
   { uuBOS, IDS_COMMAND_BOS_CREATE,          IDH_SVRMGR_COMMAND_BOS_CREATE },
   { uuBOS, IDS_COMMAND_BOS_DELETE,          IDH_SVRMGR_COMMAND_BOS_DELETE },
   { uuBOS, IDS_COMMAND_BOS_EXEC,            IDH_SVRMGR_COMMAND_BOS_EXEC },
   { uuBOS, IDS_COMMAND_BOS_GETDATE,         IDH_SVRMGR_COMMAND_BOS_GETDATE },
   { uuBOS, IDS_COMMAND_BOS_GETLOG,          IDH_SVRMGR_COMMAND_BOS_GETLOG },
   { uuBOS, IDS_COMMAND_BOS_GETRESTART,      IDH_SVRMGR_COMMAND_BOS_GETRESTART },
   { uuBOS, IDS_COMMAND_BOS_INSTALL,         IDH_SVRMGR_COMMAND_BOS_INSTALL },
   { uuBOS, IDS_COMMAND_BOS_LISTHOSTS,       IDH_SVRMGR_COMMAND_BOS_LISTHOSTS },
   { uuBOS, IDS_COMMAND_BOS_LISTKEYS,        IDH_SVRMGR_COMMAND_BOS_LISTKEYS },
   { uuBOS, IDS_COMMAND_BOS_LISTUSERS,       IDH_SVRMGR_COMMAND_BOS_LISTUSERS },
   { uuBOS, IDS_COMMAND_BOS_PRUNE,           IDH_SVRMGR_COMMAND_BOS_PRUNE },
   { uuBOS, IDS_COMMAND_BOS_REMOVEHOST,      IDH_SVRMGR_COMMAND_BOS_REMOVEHOST },
   { uuBOS, IDS_COMMAND_BOS_REMOVEKEY,       IDH_SVRMGR_COMMAND_BOS_REMOVEKEY },
   { uuBOS, IDS_COMMAND_BOS_REMOVEUSER,      IDH_SVRMGR_COMMAND_BOS_REMOVEUSER },
   { uuBOS, IDS_COMMAND_BOS_RESTART,         IDH_SVRMGR_COMMAND_BOS_RESTART },
   { uuBOS, IDS_COMMAND_BOS_SALVAGE,         IDH_SVRMGR_COMMAND_BOS_SALVAGE },
   { uuBOS, IDS_COMMAND_BOS_SETAUTH,         IDH_SVRMGR_COMMAND_BOS_SETAUTH },
   { uuBOS, IDS_COMMAND_BOS_SETRESTART,      IDH_SVRMGR_COMMAND_BOS_SETRESTART },
   { uuBOS, IDS_COMMAND_BOS_SHUTDOWN,        IDH_SVRMGR_COMMAND_BOS_SHUTDOWN },
   { uuBOS, IDS_COMMAND_BOS_START,           IDH_SVRMGR_COMMAND_BOS_START },
   { uuBOS, IDS_COMMAND_BOS_STARTUP,         IDH_SVRMGR_COMMAND_BOS_STARTUP },
   { uuBOS, IDS_COMMAND_BOS_STATUS,          IDH_SVRMGR_COMMAND_BOS_STATUS },
   { uuBOS, IDS_COMMAND_BOS_STOP,            IDH_SVRMGR_COMMAND_BOS_STOP },
   { uuBOS, IDS_COMMAND_BOS_UNINSTALL,       IDH_SVRMGR_COMMAND_BOS_UNINSTALL },
   { uuKAS, IDS_COMMAND_KAS_GETRANDOMKEY,    IDH_SVRMGR_COMMAND_KAS_GETRANDOMKEY },
   { uuFS,  IDS_COMMAND_FS_LISTQUOTA,        IDH_SVRMGR_COMMAND_FS_LISTQUOTA },
   { uuFS,  IDS_COMMAND_FS_QUOTA,            IDH_SVRMGR_COMMAND_FS_QUOTA },
   { uuFS,  IDS_COMMAND_FS_SETQUOTA,         IDH_SVRMGR_COMMAND_FS_SETQUOTA },
};

// Precalculated hashing values for faster searching through help topics
static DWORD aSEARCHVALUES[] = {
   0x16765627, 0x02D416E6, 0x27675627, 0x35023556,
   0x36021464, 0xE6371627, 0x02452716, 0x00458656,
   0x16E6B300, 0x27E69676, 0x4602A456, 0x36861627,
   0x97022596, 0x56E60226, 0x27964747, 0x97E20075,
   0xD4F67727, 0x24F62602, 0x02269702, 0x27965637,
   0x96262716, 0x675602C6, 0x27164796, 0xE6963747,
   0x1446D696, 0x97000000, 0x96C69647, 0x37025747,
   0x02478696, 0x0266F627, 0x66163656, 0xE6475627,
   0x56270296, 0x56025737, 0xA3004586, 0x7796E676,
   0xF6C6C6F6, 0x86560266, 0x26970247, 0x77564602,
   0x56679656, 0xE6460227, 0x56460216, 0x379676E6,
   0x37024656, 0x00007716, 0x74271697, 0x16E65602,
   0xE65602A4, 0x560014E6, 0xB4565666, 0xE602F472,
   0x24279616, 0x3716C600, 0x86D65627, 0xE6023536,
   0x24279716, 0x4756B600, 0x02A416E6, 0x9616E656,
   0x56C60044, 0x02642756, 0xB6005446, 0x26573756,
   0x374702F4, 0x5427E656, 0xC696E600, 0x02645627,
   0x96369616, 0x006456C6, 0xD4573796, 0x27567602,
   0x27460074, 0x96368616, 0x47860225, 0x27564696,
   0x2700D456, 0x34F6D656, 0x1656C602, 0xD4963686,
   0x77169700, 0xC4162716, 0x5696C602, 0x16E600E4,
   0x27E69676, 0x4602A456, 0x36861627, 0x27002596,
   0xC6B6E656, 0x02641657, 0x0025F6E6, 0x56765627,
   0xE6024527, 0x3716C697, 0x160025F6, 0xB4F647C6,
   0x67163702, 0x2796E696, 0xE6960035, 0x24969716,
   0x2716D602, 0x006596B6, 0x00000000
};

#define nCOMMANDS (sizeof(aCOMMANDS)/sizeof(aCOMMANDS[0]))
#define nSEARCHVALUES (sizeof(aSEARCHVALUES)/sizeof(aSEARCHVALUES[0]))


/*
 * ROUTINES _________________________________________________________________
 *
 */

LPCTSTR lstrstr (LPCTSTR pszBuffer, LPCTSTR pszFind)
{
   if (!pszBuffer || !pszFind || !*pszFind)
      return pszBuffer;

   for ( ; *pszBuffer; ++pszBuffer)
      {
      if (*pszBuffer == *pszFind)
         {
         if (!lstrncmpi (pszBuffer, pszFind, lstrlen(pszFind)))
            return pszBuffer;
         }
      }

   return NULL;
}


/*
 * FIND COMMAND _____________________________________________________________
 *
 */

BOOL CALLBACK Help_FindCommand_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Help_FindCommand_OnInitDialog (HWND hDlg);
BOOL Help_FindCommand_OnOK (HWND hDlg);


void Help_FindCommand (void)
{
   ModalDialog (IDD_HELP_FIND, g.hMain, (DLGPROC)Help_FindCommand_DlgProc);
}


BOOL CALLBACK Help_FindCommand_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Help_FindCommand_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               if (Help_FindCommand_OnOK (hDlg))
                  EndDialog (hDlg, LOWORD(wp));
               break;

            case IDCANCEL:
               EndDialog (hDlg, LOWORD(wp));
               break;
            }
         break;
      }

   return FALSE;
}


void Help_FindCommand_OnInitDialog (HWND hDlg)
{
   HWND hCombo = GetDlgItem (hDlg, IDC_FIND_COMMAND);
   CB_StartChange (hCombo, TRUE);

   for (size_t ii = 0; ii < nCOMMANDS; ++ii)
      {
      CB_AddItem (hCombo, aCOMMANDS[ii].ids, 0);
      }

   CB_EndChange (hCombo, -1);
}


LPTSTR Help_FindCommand_Search (UNIXUTIL *puu, LPTSTR pszKeyword)
{
   // search for a usable keyword--skip "vos" or "bos" (etc).
   //
   while (*pszKeyword)
      {
      // strip any initial whitespace
      while (iswhite(*pszKeyword))
         ++pszKeyword;

      // find the end of this word
      LPTSTR pszNext;
      for (pszNext = pszKeyword; *pszNext && !iswhite(*pszNext); )
         ++pszNext;
      if (!*pszNext)  // last word?  Gotta use it.
         break;
      *pszNext = TEXT('\0');

      BOOL fSkip = FALSE;
      if (!lstrcmpi (pszKeyword, TEXT("vos")))
         {
         fSkip = TRUE;
         *puu = uuVOS;
         }
      if (!lstrcmpi (pszKeyword, TEXT("bos")))
         {
         fSkip = TRUE;
         *puu = uuBOS;
         }
      if (!lstrcmpi (pszKeyword, TEXT("kas")))
         {
         fSkip = TRUE;
         *puu = uuKAS;
         }
      if (!lstrcmpi (pszKeyword, TEXT("fs")))
         {
         fSkip = TRUE;
         *puu = uuFS;
         }

      if (fSkip)
         pszKeyword = 1+pszNext;
      else
         break;
      }

   return pszKeyword;
}


DWORD NextSearch (int &ii)
{
   ii = ((ii>>2) == nSEARCHVALUES) ? 1 : ii+1;
   return ( (DWORD)(((aSEARCHVALUES[(ii-1)>>2]>>(((ii-1)%4)<<3))>>4)&15) |
            (DWORD)(((aSEARCHVALUES[(ii-1)>>2]>>(((ii-1)%4)<<3))<<4)&240) );
}


BOOL Help_FindCommand_OnOK (HWND hDlg)
{
   HWND hCombo = GetDlgItem (hDlg, IDC_FIND_COMMAND);
   int iiDisplay = -1;

   UNIXUTIL uu = uuUNSPECIFIED;
   TCHAR szText[ cchRESOURCE ];
   GetWindowText (hCombo, szText, cchRESOURCE);

   if (!szText[0])
      {
      Message (MB_ICONASTERISK | MB_OK, IDS_FIND_NOTHING_TITLE, IDS_FIND_NOTHING_DESC);
      return FALSE;
      }

   LPTSTR pszKeyword = Help_FindCommand_Search (&uu, szText);

   for (size_t ii = 0; (iiDisplay == -1) && ii < nCOMMANDS; ++ii)
      {
      TCHAR szCommand[ cchRESOURCE ];
      GetString (szCommand, aCOMMANDS[ ii ].ids);

      if (lstrstr (szCommand, pszKeyword) != NULL)
         {
         if ((uu == uuUNSPECIFIED) || (uu == aCOMMANDS[ ii ].uu))
            iiDisplay = ii;
         }
      }

   if (iiDisplay == -1)
      {
      Message (MB_ICONASTERISK | MB_OK, IDS_FIND_UNKNOWN_TITLE, IDS_FIND_UNKNOWN_DESC, TEXT("%s"), pszKeyword);
      return FALSE;
      }

   WinHelp (g.hMain, cszHELPFILENAME, HELP_CONTEXT, aCOMMANDS[ iiDisplay ].hid);
   return TRUE;
}


/*
 * FIND ERROR _______________________________________________________________
 *
 */

BOOL CALLBACK Help_FindError_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Help_FindError_OnInitDialog (HWND hDlg);
void Help_FindError_OnTranslate (HWND hDlg);
void Help_FindError_Shrink (HWND hDlg, BOOL fShrink);

void Help_FindError (void)
{
   ModalDialog (IDD_HELP_ERROR, g.hMain, (DLGPROC)Help_FindError_DlgProc);
}


BOOL CALLBACK Help_FindError_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Help_FindError_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_ERROR_TRANSLATE:
               Help_FindError_OnTranslate (hDlg);
               break;

            case IDOK:
            case IDCANCEL:
               EndDialog (hDlg, LOWORD(wp));
               break;
            }
         break;
      }

   return FALSE;
}


void Help_FindError_OnInitDialog (HWND hDlg)
{
   Help_FindError_Shrink (hDlg, TRUE);

   SetDlgItemText (hDlg, IDC_ERROR_NUMBER, TEXT(""));
}


void Help_FindError_OnTranslate (HWND hDlg)
{
   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_ERROR_NUMBER, szText, cchRESOURCE);

   LPSTR pszTextA = StringToAnsi (szText);
   DWORD dwError = strtoul (pszTextA, NULL, 0);
   FreeString (pszTextA, szText);

   TCHAR szDesc[ cchRESOURCE ];
   FormatError (szDesc, TEXT("%s"), dwError);

   // The output string either looks like this (if successful):
   //    successful completion (0x00000000)
   // Or like this:
   //    0x00000000
   // Since we list the error code elsewhere, remove it from the
   // former case. In the latter case, empty the string entirely
   // so we'll know there was no translation.
   //
   LPTSTR pszTruncate;
   if ((pszTruncate = (LPTSTR)lstrrchr (szDesc, TEXT('('))) == NULL)
      pszTruncate = szDesc;
   else if ((pszTruncate > szDesc) && (*(pszTruncate-1) == TEXT(' ')))
      --pszTruncate;
   *pszTruncate = TEXT('\0');

   LPTSTR pszText;
   if (szDesc[0] == TEXT('\0'))
      pszText = FormatString (IDS_ERROR_NOTTRANSLATED, TEXT("%08lX%lu"), dwError, dwError);
   else
      pszText = FormatString (IDS_ERROR_TRANSLATED, TEXT("%08lX%lu%s"), dwError, dwError, szDesc);
   SetDlgItemText (hDlg, IDC_ERROR_DESC, pszText);
   FreeString (pszText);

   Help_FindError_Shrink (hDlg, FALSE);
}


void Help_FindError_Shrink (HWND hDlg, BOOL fShrink)
{
   static BOOL fShrunk = FALSE;
   static LONG cyShrunk = 0;

   if (fShrink)
      {
      fShrunk = TRUE;

      // shrink the window--move the IDCANCEL button up so that its
      // top edge is where the IDC_ADVANCED_BOX line's top edge is, and
      // hide IDC_ADVANCED_BOX and IDC_ERROR_DESC.
      //
      RECT rAdvanced;
      GetRectInParent (GetDlgItem (hDlg, IDC_ADVANCED_BOX), &rAdvanced);

      RECT rClose;
      GetRectInParent (GetDlgItem (hDlg, IDCANCEL), &rClose);

      cyShrunk = rClose.top - rAdvanced.top - 9;
      
      ShowWindow (GetDlgItem (hDlg, IDC_ERROR_DESC), SW_HIDE);

      SetWindowPos (GetDlgItem (hDlg, IDCANCEL), NULL,
                    rClose.left, rClose.top -cyShrunk, 0, 0,
                    SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

      RECT rDialog;
      GetWindowRect (hDlg, &rDialog);
      SetWindowPos (hDlg, NULL,
                    0, 0, cxRECT(rDialog), cyRECT(rDialog) -cyShrunk,
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
      }
   else if (fShrunk)
      {
      fShrunk = FALSE;

      // expand the window--move the IDCANCEL button down, and
      // show IDC_ADVANCED_BOX and IDC_ERROR_DESC.
      //
      RECT rClose;
      GetRectInParent (GetDlgItem (hDlg, IDCANCEL), &rClose);

      RECT rDialog;
      GetWindowRect (hDlg, &rDialog);
      SetWindowPos (hDlg, NULL,
                    0, 0, cxRECT(rDialog), cyRECT(rDialog) +cyShrunk,
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

      SetWindowPos (GetDlgItem (hDlg, IDCANCEL), NULL,
                    rClose.left, rClose.top +cyShrunk, 0, 0,
                    SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

      ShowWindow (GetDlgItem (hDlg, IDC_ERROR_DESC), SW_SHOW);
      }
}


/*
 * HELP ABOUT _______________________________________________________________
 *
 */


BOOL CALLBACK Help_About_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Help_About_OnInitDialog (HWND hDlg);
void Help_About_OnSysCommand (HWND hDlg, int &cmd);
LONG procAbout;

void Help_About (void)
{
   ModalDialog (IDD_HELP_ABOUT, g.hMain, (DLGPROC)Help_About_DlgProc);
}


BOOL CALLBACK Help_About_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static int cmd;
   switch (msg)
      {
      case WM_INITDIALOG:
         cmd = 0;
         Help_About_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               EndDialog (hDlg, LOWORD(wp));
               break;
            }
         break;

      case WM_SYSCOMMAND+1:
         Help_About_OnSysCommand (hDlg, cmd);
         break;
      }

   return FALSE;
}


BOOL CALLBACK Help_About_Proc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (msg == WM_DESTROY)
      {
      KillTimer (GetParent(hDlg), 1000);
      }
   else if (msg == WM_DESTROY+0x200)
      {
      RECT rr;
      GetWindowRect(GetDlgItem(GetParent(hDlg),0x051E),&rr);
      DWORD dw = GetMessagePos();
      POINT pt = { LOWORD(dw), HIWORD(dw) };
      if (PtInRect (&rr, pt))
         {
         SetDlgItemText (GetParent(hDlg), 0x051F, TEXT("\n\n\n\n\n\n\n"));
         SetTimer (GetParent(hDlg), 1000, 1000/8, NULL);
         }
      }
   return CallWindowProc ((WNDPROC)procAbout, hDlg, msg, wp, lp);
}


void Help_About_OnInitDialog (HWND hDlg)
{
   HWND hAbout = GetDlgItem (hDlg, IDOK);
   procAbout = (LONG)GetWindowLong (hAbout, GWL_WNDPROC);
   SetWindowLong (hAbout, GWL_WNDPROC, (LONG)Help_About_Proc);

   LPTSTR pszText = FormatString (IDS_HELPABOUT_DESC1);
   SetDlgItemText (hDlg, IDC_HELPABOUT_DESC, pszText);
   FreeString (pszText);
}


void Help_About_OnSysCommand (HWND hDlg, int &cmd)
{
   DWORD dw;
   TCHAR szSys[cchRESOURCE];
   TCHAR szSys2[cchRESOURCE];
   GetDlgItemText (hDlg, 0x051F, szSys, cchRESOURCE);
   if ((dw = NextSearch (cmd)) != 0)
      {
      LPTSTR psz;
      for (psz = &szSys[ lstrlen(szSys)-1 ]; *(psz-1) != TEXT('\n'); --psz);
      lstrcpy (szSys2, psz);
      wsprintf (psz, TEXT("%c%s"), (TCHAR)dw, szSys2);
      SetDlgItemText (hDlg, 0x051F, szSys);
      }
   else // (dw == 0)
      {
      LPTSTR psz;
      for (psz = szSys; *psz && (*psz != TEXT('\n')); ++psz);
      wsprintf (szSys2, TEXT("%s\n"), 1+psz);
      SetDlgItemText (hDlg, 0x051F, szSys2);
      }
}



/*
 * CONTEXT HELP _____________________________________________________________
 *
 */

static DWORD IDD_SVR_LISTS_HELP[] = {
   IDC_LIST_LIST,                    IDH_SVRMGR_LIST_LIST,
   IDC_LIST_ADD,                     IDH_SVRMGR_LIST_ADD,
   IDC_LIST_REMOVE,                  IDH_SVRMGR_LIST_REMOVE,
   IDC_LIST_NAME,                    IDH_SVRMGR_LIST_NAME,
   0, 0
};

static DWORD IDD_AGG_GENERAL_HELP[] = {
   IDC_AGG_NAME,                     IDH_SVRMGR_AGGPROP_NAME,
   IDC_AGG_ID,                       IDH_SVRMGR_AGGPROP_ID,
   IDC_AGG_DEVICE,                   IDH_SVRMGR_AGGPROP_DEVICE,
   IDC_AGG_USAGE,                    IDH_SVRMGR_AGGPROP_USAGE,
   IDC_AGG_USAGEBAR,                 IDH_SVRMGR_AGGPROP_USAGEBAR,
   IDC_AGG_WARN,                     IDH_SVRMGR_AGGPROP_WARN,
   IDC_AGG_WARN_AGGFULL_DEF,         IDH_SVRMGR_AGGPROP_WARN_AGGFULL_DEF,
   IDC_AGG_WARN_AGGFULL,             IDH_SVRMGR_AGGPROP_WARN_AGGFULL,
   IDC_AGG_WARN_AGGFULL_PERCENT,     IDH_SVRMGR_AGGPROP_WARN_AGGFULL_PERCENT,
   IDC_AGG_FILESETS,                 IDH_SVRMGR_AGGPROP_NUMFILESETS,
   IDC_AGG_WARNALLOC,                IDH_SVRMGR_AGGPROP_WARNALLOC,
   0, 0
};

static DWORD IDD_SVR_GENERAL_HELP[] = {
   IDC_SVR_NAME,                     IDH_SVRMGR_SVRPROP_NAME,
   IDC_SVR_ADDRESSES,                IDH_SVRMGR_SVRPROP_ADDRESSES,
   IDC_SVR_NUMAGGREGATES,            IDH_SVRMGR_SVRPROP_NUMAGGREGATES,
   IDC_SVR_AUTH_YES,                 IDH_SVRMGR_SVRPROP_AUTH_YES,
   IDC_SVR_AUTH_NO,                  IDH_SVRMGR_SVRPROP_AUTH_NO,
   IDC_SVR_CAPACITY,                 IDH_SVRMGR_SVRPROP_CAPACITY,
   IDC_SVR_ALLOCATION,               IDH_SVRMGR_SVRPROP_ALLOCATION,
   IDC_SVR_CHANGEADDR,               IDH_SVRMGR_SVRPROP_CHANGEADDR,
   0, 0
};

static DWORD IDD_SVR_SCOUT_HELP[] = {
   IDC_SVR_WARN_AGGFULL,             IDH_SVRMGR_SVRPROP_WARN_AGGFULL,
   IDC_SVR_WARN_AGGFULL_PERCENT,     IDH_SVRMGR_SVRPROP_WARN_AGGFULL_PERCENT,
   IDC_SVR_WARN_SETFULL,             IDH_SVRMGR_SVRPROP_WARN_SETFULL,
   IDC_SVR_WARN_SETFULL_PERCENT,     IDH_SVRMGR_SVRPROP_WARN_SETFULL_PERCENT,
   IDC_SVR_WARN_SVCSTOP,             IDH_SVRMGR_SVRPROP_WARN_SVCSTOP,
   IDC_SVR_WARN_SETNOVLDB,           IDH_SVRMGR_SVRPROP_WARN_SETNOVLDB,
   IDC_SVR_WARN_AGGNOSERV,           IDH_SVRMGR_SVRPROP_WARN_AGGNOSERV,
   IDC_SVR_WARN_SETNOSERV,           IDH_SVRMGR_SVRPROP_WARN_SETNOSERV,
   IDC_SVR_AUTOREFRESH,              IDH_SVRMGR_SVRPROP_AUTOREFRESH,
   IDC_SVR_AUTOREFRESH_MINUTES,      IDH_SVRMGR_SVRPROP_AUTOREFRESH_MINUTES,
   IDC_SVR_WARN_AGGALLOC,            IDH_SVRMGR_SVRPROP_WARNALLOC,
   0, 0
};

static DWORD IDD_SVC_GENERAL_HELP[] = {
   IDC_SVC_NAME,                     IDH_SVRMGR_SVCPROP_NAME,
   IDC_SVC_TYPE,                     IDH_SVRMGR_SVCPROP_TYPE,
   IDC_SVC_PARAMS,                   IDH_SVRMGR_SVCPROP_PARAMS,
   IDC_SVC_STARTDATE,                IDH_SVRMGR_SVCPROP_STARTDATE,
   IDC_SVC_STOPDATE,                 IDH_SVRMGR_SVCPROP_STOPDATE,
   IDC_SVC_LASTERROR,                IDH_SVRMGR_SVCPROP_LASTERROR,
   IDC_SVC_STATUS,                   IDH_SVRMGR_SVCPROP_STATUS,
   IDC_SVC_NOTIFIER,                 IDH_SVRMGR_SVCPROP_NOTIFIER,
   IDC_SVC_WARNSTOP,                 IDH_SVRMGR_SVCPROP_WARNSTOP,
   IDC_SVC_VIEWLOG,                  IDH_SVRMGR_SVCPROP_VIEWLOG,
   IDC_SVC_START,                    IDH_SVRMGR_SVCPROP_START,
   IDC_SVC_STOP,                     IDH_SVRMGR_SVCPROP_STOP,
   0, 0
};

static DWORD IDD_SET_GENERAL_HELP[] = {
   IDC_SET_NAME,                     IDH_SVRMGR_SETPROP_NAME,
   IDC_SET_ID,                       IDH_SVRMGR_SETPROP_ID,
   IDC_SET_CREATEDATE,               IDH_SVRMGR_SETPROP_CREATEDATE,
   IDC_SET_UPDATEDATE,               IDH_SVRMGR_SETPROP_UPDATEDATE,
   IDC_SET_ACCESSDATE,               IDH_SVRMGR_SETPROP_ACCESSDATE,
   IDC_SET_BACKUPDATE,               IDH_SVRMGR_SETPROP_BACKUPDATE,
   IDC_SET_STATUS,                   IDH_SVRMGR_SETPROP_STATUS,
   IDC_SET_LOCK,                     IDH_SVRMGR_SETPROP_LOCK,
   IDC_SET_UNLOCK,                   IDH_SVRMGR_SETPROP_UNLOCK,
   IDC_SET_USAGE,                    IDH_SVRMGR_SETPROP_USAGE,
   IDC_SET_QUOTA,                    IDH_SVRMGR_SETPROP_QUOTA,
   IDC_SET_USAGEBAR,                 IDH_SVRMGR_SETPROP_USAGEBAR,
   IDC_SET_WARN,                     IDH_SVRMGR_SETPROP_WARN,
   IDC_SET_WARN_SETFULL_DEF,         IDH_SVRMGR_SETPROP_WARN_SETFULL_DEF,
   IDC_SET_WARN_SETFULL,             IDH_SVRMGR_SETPROP_WARN_SETFULL,
   IDC_SET_WARN_SETFULL_PERCENT,     IDH_SVRMGR_SETPROP_WARN_SETFULL_PERCENT,
   IDC_SET_FILES,                    IDH_SVRMGR_SETPROP_FILES,
   0, 0
};

static DWORD IDD_SVC_CREATE_HELP[] = {
   IDC_SVC_SERVER,                   IDH_SVRMGR_SVCCREATE_SERVER,
   IDC_SVC_NAME,                     IDH_SVRMGR_SVCCREATE_NAME,
   IDC_SVC_COMMAND,                  IDH_SVRMGR_SVCCREATE_COMMAND,
   IDC_SVC_PARAMS,                   IDH_SVRMGR_SVCCREATE_PARAMS,
   IDC_SVC_NOTIFIER,                 IDH_SVRMGR_SVCCREATE_NOTIFIER,
   IDC_SVC_LOGFILE,                  IDH_SVRMGR_SVCCREATE_LOGFILE,
   IDC_SVC_TYPE_SIMPLE,              IDH_SVRMGR_SVCCREATE_SIMPLE,
   IDC_SVC_RUNNOW,                   IDH_SVRMGR_SVCCREATE_SIMPLE_RUNNOW,
   IDC_SVC_TYPE_FS,                  IDH_SVRMGR_SVCCREATE_FS,
   IDC_SVC_TYPE_CRON,                IDH_SVRMGR_SVCCREATE_CRON,
   IDC_SVC_RUNDAY,                   IDH_SVRMGR_SVCCREATE_CRON_RUNDAY,
   IDC_SVC_RUNTIME,                  IDH_SVRMGR_SVCCREATE_CRON_RUNTIME,
   0, 0
};

static DWORD IDD_SET_RELEASE_HELP[] = {
   IDOK,                             IDH_SVRMGR_SETRELEASE_OK,
   IDCANCEL,                         IDH_SVRMGR_SETRELEASE_CANCEL,
   IDC_RELSET_NORMAL,                IDH_SVRMGR_SETRELEASE_NORMAL,
   IDC_RELSET_FORCE,                 IDH_SVRMGR_SETRELEASE_FORCE,
   0, 0
};

static DWORD IDD_SET_REPSITES_HELP[] = {
   IDC_SET_NAME,                     IDH_SVRMGR_SETREPSITES_NAME,
   IDC_SET_SERVER,                   IDH_SVRMGR_SETREPSITES_RW_SERVER,
   IDC_SET_AGGREGATE,                IDH_SVRMGR_SETREPSITES_RW_AGGREGATE,
   IDC_SET_REP_LIST,                 IDH_SVRMGR_SETREPSITES_REPSITES,
   IDC_SET_REPSITE_ADD,              IDH_SVRMGR_SETREPSITES_REPSITE_ADD,
   IDC_SET_REPSITE_DELETE,           IDH_SVRMGR_SETREPSITES_REPSITE_DELETE,
   IDC_SET_RELEASE,                  IDH_SVRMGR_SETREPSITES_REPSITE_RELEASE,
   0, 0
};

static DWORD IDD_DCE_NEWCELL_HELP[] = {
   IDC_OPENCELL_CELL,                IDH_SVRMGR_NEWCELL_CELL,
   IDC_OPENCELL_OLDCREDS,            IDH_SVRMGR_NEWCELL_CURRENTID,
   IDC_OPENCELL_ID,                  IDH_SVRMGR_NEWCELL_ID,
   IDC_OPENCELL_PASSWORD,            IDH_SVRMGR_NEWCELL_PASSWORD,
   IDC_ADVANCED,                     IDH_SVRMGR_NEWCELL_ADVANCED,
   IDC_MON_ALL,                      IDH_SVRMGR_NEWCELL_MONALL,
   IDC_MON_ONE,                      IDH_SVRMGR_NEWCELL_MONONE,
   IDC_MON_SERVER,                   IDH_SVRMGR_NEWCELL_MONSERVER,
   IDC_MON_SOME,                     IDH_SVRMGR_NEWCELL_MONSOME,
   IDC_MON_SUBSET,                   IDH_SVRMGR_NEWCELL_MONSUBSET,
   0, 0
};

static DWORD IDD_DCE_NEWCREDS_HELP[] = {
   IDC_CREDS_CELL,                   IDH_SVRMGR_NEWCREDS_CELL,
   IDC_CREDS_CURRENTID,              IDH_SVRMGR_NEWCREDS_CURRENTID,
   IDC_CREDS_EXPDATE,                IDH_SVRMGR_NEWCREDS_EXPDATE,
   IDC_CREDS_LOGIN,                  IDH_SVRMGR_NEWCREDS_LOGIN,
   IDC_CREDS_ID,                     IDH_SVRMGR_NEWCREDS_ID,
   IDC_CREDS_PASSWORD,               IDH_SVRMGR_NEWCREDS_PASSWORD,
   (DWORD)IDC_STATIC,                0,
   0, 0
};

static DWORD IDD_COLUMNS_HELP[] = {
   IDC_COLUMNS,                      IDH_SVRMGR_COLUMNS_WHICH,
   IDC_COL_AVAIL,                    IDH_SVRMGR_COLUMNS_AVAIL,
   IDC_COL_SHOWN,                    IDH_SVRMGR_COLUMNS_SHOWN,
   IDC_COL_INSERT,                   IDH_SVRMGR_COLUMNS_INSERT,
   IDC_COL_DELETE,                   IDH_SVRMGR_COLUMNS_DELETE,
   IDC_COL_UP,                       IDH_SVRMGR_COLUMNS_MOVEUP,
   IDC_COL_DOWN,                     IDH_SVRMGR_COLUMNS_MOVEDOWN,
   0, 0
};

static DWORD IDD_SET_CREATE_HELP[] = {
   IDC_SET_NAME,                     IDH_SVRMGR_SETCREATE_NAME,
   IDC_SET_QUOTA,                    IDH_SVRMGR_SETCREATE_QUOTA,
   IDC_SET_QUOTA_UNITS,              IDH_SVRMGR_SETCREATE_QUOTA_UNITS,
   IDC_SET_CLONE,                    IDH_SVRMGR_SETCREATE_CLONE,
   IDC_SET_SERVER,                   IDH_SVRMGR_SETCREATE_SERVER,
   IDC_AGG_LIST,                     IDH_SVRMGR_SETCREATE_AGGLIST,
   0, 0
};

static DWORD IDD_SET_DELETE_HELP[] = {
   IDOK,                             IDH_SVRMGR_SETDELETE_OK,
   IDCANCEL,                         IDH_SVRMGR_SETDELETE_CANCEL,
   IDC_DELSET_SERVER,                IDH_SVRMGR_SETDELETE_DELFROM_SERVER,
   IDC_DELSET_VLDB,                  IDH_SVRMGR_SETDELETE_DELFROM_VLDB,
   0, 0
};

static DWORD IDD_SET_DELREP_HELP[] = {
   IDOK,                             IDH_SVRMGR_SETDELREP_OK,
   IDCANCEL,                         IDH_SVRMGR_SETDELREP_CANCEL,
   0, 0
};

static DWORD IDD_SET_DELCLONE_HELP[] = {
   IDOK,                             IDH_SVRMGR_SETDELCLONE_OK,
   IDCANCEL,                         IDH_SVRMGR_SETDELCLONE_CANCEL,
   0, 0
};

static DWORD IDD_SET_CLONE_HELP[] = {
   IDOK,                             IDH_SVRMGR_SETCLONE_OK,
   IDCANCEL,                         IDH_SVRMGR_SETCLONE_CANCEL,
   0, 0
};

static DWORD IDD_PROBLEMS_HELP[] = {
   IDC_PROBLEM_TEXT,                 IDH_SVRMGR_PROBLEMS_TEXT,
   IDC_PROBLEM_REMEDY,               IDH_SVRMGR_PROBLEMS_REMEDY,
   IDC_PROBLEM_SCROLL,               IDH_SVRMGR_PROBLEMS_SCROLL,
   0, 0
};

static DWORD IDD_SET_MOVETO_HELP[] = {
   IDC_AGG_LIST,                     IDH_SVRMGR_SETMOVE_AGGLIST,
   IDC_MOVESET_SERVER,               IDH_SVRMGR_SETMOVE_SERVER,
   0, 0
};

static DWORD IDD_SVC_LOGNAME_HELP[] = {
   IDC_VIEWLOG_SERVER,               IDH_SVRMGR_LOGNAME_SERVER,
   IDC_VIEWLOG_FILENAME,             IDH_SVRMGR_LOGNAME_FILENAME,
   IDOK,                             IDH_SVRMGR_LOGNAME_OK,
   IDCANCEL,                         IDH_SVRMGR_LOGNAME_CANCEL,
   0, 0
};

static DWORD IDD_SVC_VIEWLOG_HELP[] = {
   IDC_SVC_VIEWLOG_FILENAME,         IDH_SVRMGR_VIEWLOG_FILENAME,
   IDC_VIEWLOG_TEXT,                 IDH_SVRMGR_VIEWLOG_TEXT,
   IDC_VIEWLOG_SAVEAS,               IDH_SVRMGR_VIEWLOG_SAVEAS,
   0, 0
};

static DWORD IDD_SET_SETQUOTA_HELP[] = {
   IDC_SET_NAME,                     IDH_SVRMGR_SETQUOTA_NAME,
   IDC_SET_AGGREGATE,                IDH_SVRMGR_SETQUOTA_AGGREGATE,
   IDC_AGG_PROPERTIES,               IDH_SVRMGR_SETQUOTA_AGGPROPERTIES,
   IDC_SET_USAGE,                    IDH_SVRMGR_SETQUOTA_USAGE,
   IDC_SET_USAGEBAR,                 IDH_SVRMGR_SETQUOTA_USAGEBAR,
   IDC_SET_QUOTA,                    IDH_SVRMGR_SETQUOTA_VALUE,
   IDC_SET_QUOTA_UNITS,              IDH_SVRMGR_SETQUOTA_UNITS,
   IDOK,                             IDH_SVRMGR_SETQUOTA_OK,
   IDCANCEL,                         IDH_SVRMGR_SETQUOTA_CANCEL,
   0, 0
};

static DWORD IDD_SVR_SYNCVLDB_HELP[] = {
   IDOK,                             IDH_SVRMGR_SYNCVLDB_OK,
   IDCANCEL,                         IDH_SVRMGR_SYNCVLDB_CANCEL,
   0, 0
};

static DWORD IDD_SET_CREATEREP_HELP[] = {
   IDC_SET_NAME,                     IDH_SVRMGR_SETCREATEREP_NAME,
   IDC_AGG_LIST,                     IDH_SVRMGR_SETCREATEREP_AGGLIST,
   IDC_SET_SERVER,                   IDH_SVRMGR_SETCREATEREP_SERVER,
   0, 0
};

static DWORD IDD_SVR_INSTALL_HELP[] = {
   IDC_FILENAME,                     IDH_SVRMGR_INSTALL_SOURCE,
   IDC_BROWSE,                       IDH_SVRMGR_INSTALL_BROWSE,
   IDC_SERVER,                       IDH_SVRMGR_INSTALL_SERVER,
   IDC_DIRECTORY,                    IDH_SVRMGR_INSTALL_TARGET,
   0, 0
};

static DWORD IDD_SVR_UNINSTALL_HELP[] = {
   IDC_SERVER,                       IDH_SVRMGR_UNINSTALL_SERVER,
   IDC_FILENAME,                     IDH_SVRMGR_UNINSTALL_FILENAME,
   0, 0
};

static DWORD IDD_SVR_PRUNE_HELP[] = {
   IDC_SERVER,                       IDH_SVRMGR_PRUNE_SERVER,
   IDC_OP_DELETE_CORE,               IDH_SVRMGR_PRUNE_OP_DELETE_CORE,
   IDC_OP_DELETE_BAK,                IDH_SVRMGR_PRUNE_OP_DELETE_BAK,
   IDC_OP_DELETE_OLD,                IDH_SVRMGR_PRUNE_OP_DELETE_OLD,
   0, 0
};

static DWORD IDD_SET_RENAME_HELP[] = {
   IDC_RENSET_OLD,                   IDH_SVRMGR_RENAMESET_OLDNAME,
   IDC_RENSET_NEW,                   IDH_SVRMGR_RENAMESET_NEWNAME,
   0, 0
};

static DWORD IDD_SVC_DELETE_HELP[] = {
   IDCANCEL,                         IDH_SVRMGR_DELETESERVICE_CANCEL,
   IDOK,                             IDH_SVRMGR_DELETESERVICE_OK,
   0, 0
};

static DWORD IDD_SVR_GETDATES_HELP[] = {
   IDC_SERVER,                       IDH_SVRMGR_GETDATES_SERVER,
   IDC_FILENAME,                     IDH_SVRMGR_GETDATES_FILENAME,
   0, 0
};

static DWORD IDD_SVR_GETDATES_RESULTS_HELP[] = {
   IDC_SERVER,                       IDH_SVRMGR_GETDATES_SERVER,
   IDC_FILENAME,                     IDH_SVRMGR_GETDATES_FILENAME,
   IDC_DATE_FILE,                    IDH_SVRMGR_GETDATES_DATE_FILE,
   IDC_DATE_BAK,                     IDH_SVRMGR_GETDATES_DATE_BAK,
   IDC_DATE_OLD,                     IDH_SVRMGR_GETDATES_DATE_OLD,
   0, 0
};

static DWORD IDD_SET_DUMP_HELP[] = {
   IDC_DUMP_FILENAME,                IDH_SVRMGR_SETDUMP_FILENAME,
   IDC_DUMP_FULL,                    IDH_SVRMGR_SETDUMP_FULL,
   IDC_DUMP_LIMIT_TIME,              IDH_SVRMGR_SETDUMP_BYTIME,
   IDC_DUMP_TIME,                    IDH_SVRMGR_SETDUMP_BYTIME_TIME,
   IDC_DUMP_DATE,                    IDH_SVRMGR_SETDUMP_BYTIME_DATE,
   0, 0
};

static DWORD IDD_SET_RESTORE_HELP[] = {
   IDC_RESTORE_FILENAME,             IDH_SVRMGR_SETRESTORE_FILENAME,
   IDC_RESTORE_BROWSE,               IDH_SVRMGR_SETRESTORE_BROWSE,
   IDC_RESTORE_SETNAME,              IDH_SVRMGR_SETRESTORE_SETNAME,
   IDC_RESTORE_SERVER,               IDH_SVRMGR_SETRESTORE_SERVER,
   IDC_AGG_LIST,                     IDH_SVRMGR_SETRESTORE_AGGLIST,
   IDC_RESTORE_INCREMENTAL,          IDH_SVRMGR_SETRESTORE_INCREMENTAL,
   0, 0
};

static DWORD IDD_SVC_BOS_HELP[] = {
   IDC_SVC_NAME,                     IDH_SVRMGR_SVCPROP_BOS_NAME,
   IDC_BOS_GENRES,                   IDH_SVRMGR_SVCPROP_BOS_GENRES,
   IDC_BOS_GENRES_DATE,              IDH_SVRMGR_SVCPROP_BOS_GENRES_DATE,
   IDC_BOS_GENRES_TIME,              IDH_SVRMGR_SVCPROP_BOS_GENRES_TIME,
   IDC_BOS_BINRES,                   IDH_SVRMGR_SVCPROP_BOS_BINRES,
   IDC_BOS_BINRES_DATE,              IDH_SVRMGR_SVCPROP_BOS_BINRES_DATE,
   IDC_BOS_BINRES_TIME,              IDH_SVRMGR_SVCPROP_BOS_BINRES_TIME,
   0, 0
};

static DWORD IDD_SET_CLONESYS_HELP[] = {
   IDC_CLONE_ALL,                    IDH_SVRMGR_CLONESYS_ALL,
   IDC_CLONE_SOME,                   IDH_SVRMGR_CLONESYS_SOME,
   IDC_CLONE_SVR_LIMIT,              IDH_SVRMGR_CLONESYS_BYSERVER,
   IDC_CLONE_SVR,                    IDH_SVRMGR_CLONESYS_BYSERVER_SERVER,
   IDC_CLONE_AGG_LIMIT,              IDH_SVRMGR_CLONESYS_BYAGG,
   IDC_CLONE_AGG,                    IDH_SVRMGR_CLONESYS_BYAGG_AGGREGATE,
   IDC_CLONE_PREFIX_LIMIT,           IDH_SVRMGR_CLONESYS_BYPREFIX,
   IDC_CLONE_PREFIX,                 IDH_SVRMGR_CLONESYS_BYPREFIX_PREFIX,
   0, 0
};

static DWORD IDD_SUBSETS_HELP[] = {
   IDC_SUBSET_NAME,                  IDH_SVRMGR_SUBSET_NAME,
   IDC_SUBSET_LOAD,                  IDH_SVRMGR_SUBSET_LOAD,
   IDC_SUBSET_SAVE,                  IDH_SVRMGR_SUBSET_SAVE,
   IDC_SUBSET_LIST,                  IDH_SVRMGR_SUBSET_SERVERLIST,
   IDC_SUBSET_ALL,                   IDH_SVRMGR_SUBSET_MONITORALL,
   IDC_SUBSET_NONE,                  IDH_SVRMGR_SUBSET_MONITORNONE,
   0, 0
};

static DWORD IDD_SUBSET_LOADSAVE_HELP[] = {
   IDC_SUBSET_NAME,                  IDH_SVRMGR_SUBSET_NAME,
   IDC_SUBSET_LIST,                  IDH_SVRMGR_SUBSET_SUBSETLIST,
   IDC_SUBSET_DELETE,                IDH_SVRMGR_SUBSET_DELETE,
   IDC_SUBSET_RENAME,                IDH_SVRMGR_SUBSET_RENAME,
   0, 0
};

static DWORD IDD_OPTIONS_GENERAL_HELP[] = {
   IDC_OPT_SVR_LONGNAMES,            IDH_SVRMGR_OPT_SVR_LONGNAMES,
   IDC_OPT_SVR_DBL_PROP,             IDH_SVRMGR_OPT_SVR_DBL_PROP,
   IDC_OPT_SVR_DBL_DEPENDS,          IDH_SVRMGR_OPT_SVR_DBL_DEPENDS,
   IDC_OPT_SVR_DBL_OPEN,             IDH_SVRMGR_OPT_SVR_DBL_OPEN,
   IDC_OPT_SVR_OPENMON,              IDH_SVRMGR_OPT_SVR_OPENMON,
   IDC_OPT_SVR_CLOSEUNMON,           IDH_SVRMGR_OPT_SVR_CLOSEUNMON,
   IDC_OPT_WARN_BADCREDS,            IDH_SVRMGR_OPT_WARN_BADCREDS,
   0, 0
};

static DWORD IDD_BADCREDS_HELP[] = {
   IDC_BADCREDS_SHUTUP,              IDH_SVRMGR_BADCREDS_SHUTUP,
   IDOK,                             IDH_SVRMGR_BADCREDS_YES,
   IDCANCEL,                         IDH_SVRMGR_BADCREDS_NO,
   0, 0
};

static DWORD IDD_SVR_KEYS_HELP[] = {
   IDC_KEY_LIST,                     IDH_SVRMGR_KEY_LIST,
   IDC_KEY_ADD,                      IDH_SVRMGR_KEY_ADD,
   IDC_KEY_REMOVE,                   IDH_SVRMGR_KEY_REMOVE,
   IDC_KEY_NAME,                     IDH_SVRMGR_KEY_NAME,
   0, 0
};

static DWORD IDD_SVC_START_HELP[] = {
   IDC_STARTSTOP_TEMPORARY,          IDH_SVRMGR_STARTSERVICE_TEMPORARY,
   IDC_STARTSTOP_PERMANENT,          IDH_SVRMGR_STARTSERVICE_PERMANENT,
   0, 0
};

static DWORD IDD_SVC_STOP_HELP[] = {
   IDC_STARTSTOP_TEMPORARY,          IDH_SVRMGR_STOPSERVICE_TEMPORARY,
   IDC_STARTSTOP_PERMANENT,          IDH_SVRMGR_STOPSERVICE_PERMANENT,
   0, 0
};

static DWORD IDD_SVR_EXECUTE_HELP[] = {
   IDC_SERVER,                       IDH_SVRMGR_EXECUTECOMMAND_SERVER,
   IDC_COMMAND,                      IDH_SVRMGR_EXECUTECOMMAND_COMMAND,
   0, 0
};

static DWORD IDD_SVR_SALVAGE_HELP[] = {
   IDC_SERVER,                       IDH_SVRMGR_SALVAGE_SERVER,
   IDC_AGGREGATE,                    IDH_SVRMGR_SALVAGE_AGGREGATE,
   IDC_AGGREGATE_ALL,                IDH_SVRMGR_SALVAGE_AGGREGATE_ALL,
   IDC_FILESET,                      IDH_SVRMGR_SALVAGE_FILESET,
   IDC_FILESET_ALL,                  IDH_SVRMGR_SALVAGE_FILESET_ALL,
   IDC_ADVANCED,                     IDH_SVRMGR_SALVAGE_ADVANCED,
   IDC_SALVAGE_TEMPDIR,              IDH_SVRMGR_SALVAGE_TEMPDIR,
   IDC_SALVAGE_SIMUL,                IDH_SVRMGR_SALVAGE_SIMUL,
   IDC_SALVAGE_NUM,                  IDH_SVRMGR_SALVAGE_NUM,
   IDC_SALVAGE_READONLY,             IDH_SVRMGR_SALVAGE_READONLY,
   IDC_SALVAGE_BLOCK,                IDH_SVRMGR_SALVAGE_BLOCK,
   IDC_SALVAGE_FORCE,                IDH_SVRMGR_SALVAGE_FORCE,
   IDC_SALVAGE_FIXDIRS,              IDH_SVRMGR_SALVAGE_FIXDIRS,
   IDC_SALVAGE_LOG_FILE,             IDH_SVRMGR_SALVAGE_LOG_FILE,
   IDC_SALVAGE_LOG_INODES,           IDH_SVRMGR_SALVAGE_LOG_INODES,
   IDC_SALVAGE_LOG_ROOT,             IDH_SVRMGR_SALVAGE_LOG_ROOT,
   0, 0
};

static DWORD IDD_SVR_SALVAGE_RESULTS_HELP[] = {
   IDC_SALVAGE_DETAILS,              IDH_SVRMGR_SALVAGE_DETAILS,
   0, 0
};

static DWORD IDD_SVR_HOSTS_HELP[] = {
   IDC_HOST_LIST,                    IDH_SVRMGR_HOST_LIST,
   IDC_HOST_ADD,                     IDH_SVRMGR_HOST_ADD,
   IDC_HOST_REMOVE,                  IDH_SVRMGR_HOST_REMOVE,
   IDC_HOST_TITLE,                   IDH_SVRMGR_HOST_TITLE,
   0, 0
};

static DWORD IDD_SVR_ADDHOST_HELP[] = {
   IDC_ADDHOST_HOST,                 IDH_SVRMGR_ADDHOST_HOST,
   IDOK,                             IDH_SVRMGR_ADDHOST_OK,
   0, 0
};

static DWORD IDD_SVR_ADDRESS_HELP[] = {
   IDC_SVR_ADDRESSES,                IDH_SVRMGR_ADDRESS_LIST,
   IDC_ADDR_CHANGE,                  IDH_SVRMGR_ADDRESS_CHANGE,
   IDC_ADDR_REMOVE,                  IDH_SVRMGR_ADDRESS_REMOVE,
   0, 0
};

static DWORD IDD_SVR_NEWADDR_HELP[] = {
   IDC_ADDRESS,                      IDH_SVRMGR_CHANGEADDR_ADDRESS,
   0, 0
};


void Main_ConfigureHelp (void)
{
   AfsAppLib_RegisterHelpFile (cszHELPFILENAME);

   AfsAppLib_RegisterHelp (IDD_SVR_LISTS, IDD_SVR_LISTS_HELP, IDH_SVRMGR_ADMINLIST_EDIT_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_AGG_GENERAL, IDD_AGG_GENERAL_HELP, IDH_SVRMGR_PROP_AGGREGATE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_GENERAL, IDD_SVR_GENERAL_HELP, IDH_SVRMGR_PROP_SERVER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_SCOUT, IDD_SVR_SCOUT_HELP, IDH_SVRMGR_PROP_SERVER_WARNINGS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVC_GENERAL, IDD_SVC_GENERAL_HELP, IDH_SVRMGR_PROP_SERVICE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_GENERAL, IDD_SET_GENERAL_HELP, IDH_SVRMGR_PROP_FILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVC_CREATE, IDD_SVC_CREATE_HELP, IDH_SVRMGR_CREATESERVICE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_RELEASE, IDD_SET_RELEASE_HELP, IDH_SVRMGR_RELEASEFILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_REPSITES, IDD_SET_REPSITES_HELP, IDH_SVRMGR_PROP_REPSITES_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_OPENCELL, IDD_DCE_NEWCELL_HELP, IDH_SVRMGR_NEWCELL_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_APPLIB_CREDENTIALS, IDD_DCE_NEWCREDS_HELP, IDH_SVRMGR_NEWCREDS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_COLUMNS, IDD_COLUMNS_HELP, IDH_SVRMGR_COLUMNS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_CREATE, IDD_SET_CREATE_HELP, IDH_SVRMGR_CREATEFILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_DELETE, IDD_SET_DELETE_HELP, IDH_SVRMGR_DELETEFILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_DELREP, IDD_SET_DELREP_HELP, IDH_SVRMGR_DELETEREPLICA_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_DELCLONE, IDD_SET_DELCLONE_HELP, IDH_SVRMGR_DELETECLONE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_CLONE, IDD_SET_CLONE_HELP, IDH_SVRMGR_CLONE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_PROBLEMS, IDD_PROBLEMS_HELP, IDH_SVRMGR_PROBLEMS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_MOVETO, IDD_SET_MOVETO_HELP, IDH_SVRMGR_PROP_MOVEFILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVC_LOGNAME, IDD_SVC_LOGNAME_HELP, 0);
   AfsAppLib_RegisterHelp (IDD_SVC_VIEWLOG, IDD_SVC_VIEWLOG_HELP, 0);
   AfsAppLib_RegisterHelp (IDD_SET_SETQUOTA, IDD_SET_SETQUOTA_HELP, IDH_SVRMGR_SETFILESETQUOTA_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_SYNCVLDB, IDD_SVR_SYNCVLDB_HELP, IDH_SVRMGR_SYNCVLDB_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_CREATEREP, IDD_SET_CREATEREP_HELP, IDH_SVRMGR_CREATEREPLICA_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_INSTALL, IDD_SVR_INSTALL_HELP, IDH_SVRMGR_INSTALLFILE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_UNINSTALL, IDD_SVR_UNINSTALL_HELP, IDH_SVRMGR_UNINSTALLFILE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_PRUNE, IDD_SVR_PRUNE_HELP, IDH_SVRMGR_PRUNEFILES_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_RENAME, IDD_SET_RENAME_HELP, IDH_SVRMGR_RENAMEFILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVC_DELETE, IDD_SVC_DELETE_HELP, IDH_SVRMGR_DELETESERVICE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_GETDATES, IDD_SVR_GETDATES_HELP, IDH_SVRMGR_GETDATES_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_GETDATES_RESULTS, IDD_SVR_GETDATES_RESULTS_HELP, IDH_SVRMGR_GETDATES_RESULTS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_DUMP, IDD_SET_DUMP_HELP, IDH_SVRMGR_DUMPFILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_RESTORE, IDD_SET_RESTORE_HELP, IDH_SVRMGR_RESTOREFILESET_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVC_BOS, IDD_SVC_BOS_HELP, IDH_SVRMGR_PROP_SERVICE_BOS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SET_CLONESYS, IDD_SET_CLONESYS_HELP, IDH_SVRMGR_CLONESYS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SUBSETS, IDD_SUBSETS_HELP, IDH_SVRMGR_SUBSETS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SUBSET_LOADSAVE, IDD_SUBSET_LOADSAVE_HELP, 0);
   AfsAppLib_RegisterHelp (IDD_OPTIONS_GENERAL, IDD_OPTIONS_GENERAL_HELP, IDH_SVRMGR_OPTIONS_GENERAL_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_APPLIB_BADCREDS, IDD_BADCREDS_HELP, IDH_SVRMGR_BADCREDS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_KEYS, IDD_SVR_KEYS_HELP, IDH_SVRMGR_SERVERKEY_EDIT_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVC_START, IDD_SVC_START_HELP, IDH_SVRMGR_STARTSERVICE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVC_STOP, IDD_SVC_STOP_HELP, IDH_SVRMGR_STOPSERVICE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_EXECUTE, IDD_SVR_EXECUTE_HELP, IDH_SVRMGR_EXECUTECOMMAND_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_SALVAGE, IDD_SVR_SALVAGE_HELP, IDH_SVRMGR_SALVAGE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_SALVAGE_RESULTS, IDD_SVR_SALVAGE_RESULTS_HELP, IDH_SVRMGR_SALVAGE_RESULTS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_HOSTS, IDD_SVR_HOSTS_HELP, IDH_SVRMGR_HOSTS_EDIT_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_ADDHOST, IDD_SVR_ADDHOST_HELP, IDH_SVRMGR_HOSTS_ADD_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_ADDRESS, IDD_SVR_ADDRESS_HELP, IDH_SVRMGR_SERVERADDRESSES_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SVR_NEWADDR, IDD_SVR_NEWADDR_HELP, IDH_SVRMGR_CHANGEADDRESS_OVERVIEW);
}

