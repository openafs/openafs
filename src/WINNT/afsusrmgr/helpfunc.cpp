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
#include "helpfunc.h"

#ifndef iswhite
#define iswhite(_ch) ( ((_ch) == TEXT(' ')) || ((_ch) == TEXT('\t')) )
#endif


/*
 * VARIABLES ________________________________________________________________
 *
 */

typedef enum {
   uuUNSPECIFIED,
   uuPTS,	// PTS commands
   uuKAS	// KAS commands
} UNIXUTIL;

// One entry per help context
static struct {
   UNIXUTIL uu;
   int ids;
   int hid;
} aCOMMANDS[] = {
   { uuPTS,  IDS_COMMAND_PTS_ADDUSER,            IDH_USRMGR_COMMAND_PTS_ADDUSER              },
   { uuPTS,  IDS_COMMAND_PTS_CHOWN,              IDH_USRMGR_COMMAND_PTS_CHOWN                },
   { uuPTS,  IDS_COMMAND_PTS_CREATEGROUP,        IDH_USRMGR_COMMAND_PTS_CREATEGROUP          },
   { uuPTS,  IDS_COMMAND_PTS_CREATEUSER,         IDH_USRMGR_COMMAND_PTS_CREATEUSER           },
   { uuPTS,  IDS_COMMAND_PTS_DELETE,             IDH_USRMGR_COMMAND_PTS_DELETE               },
   { uuPTS,  IDS_COMMAND_PTS_EXAMINE,            IDH_USRMGR_COMMAND_PTS_EXAMINE              },
   { uuPTS,  IDS_COMMAND_PTS_LISTMAX,            IDH_USRMGR_COMMAND_PTS_LISTMAX              },
   { uuPTS,  IDS_COMMAND_PTS_LISTOWNED,          IDH_USRMGR_COMMAND_PTS_LISTOWNED            },
   { uuPTS,  IDS_COMMAND_PTS_MEMBERSHIP,         IDH_USRMGR_COMMAND_PTS_MEMBERSHIP           },
   { uuPTS,  IDS_COMMAND_PTS_REMOVEUSER,         IDH_USRMGR_COMMAND_PTS_REMOVEUSER           },
   { uuPTS,  IDS_COMMAND_PTS_RENAME,             IDH_USRMGR_COMMAND_PTS_RENAME               },
   { uuPTS,  IDS_COMMAND_PTS_SETFIELDS,          IDH_USRMGR_COMMAND_PTS_SETFIELDS            },
   { uuPTS,  IDS_COMMAND_PTS_SETMAX,             IDH_USRMGR_COMMAND_PTS_SETMAX               },
   { uuKAS,  IDS_COMMAND_KAS_CREATE,             IDH_USRMGR_COMMAND_KAS_CREATE               },
   { uuKAS,  IDS_COMMAND_KAS_DELETE,             IDH_USRMGR_COMMAND_KAS_DELETE               },
   { uuKAS,  IDS_COMMAND_KAS_EXAMINE,            IDH_USRMGR_COMMAND_KAS_EXAMINE              },
   { uuKAS,  IDS_COMMAND_KAS_GETRANDOMKEY,       IDH_USRMGR_COMMAND_KAS_GETRANDOMKEY         },
   { uuKAS,  IDS_COMMAND_KAS_LIST,               IDH_USRMGR_COMMAND_KAS_LIST                 },
   { uuKAS,  IDS_COMMAND_KAS_SETFIELDS,          IDH_USRMGR_COMMAND_KAS_SETFIELDS            },
   { uuKAS,  IDS_COMMAND_KAS_SETKEY,             IDH_USRMGR_COMMAND_KAS_SETKEY               },
   { uuKAS,  IDS_COMMAND_KAS_SETPASSWORD,        IDH_USRMGR_COMMAND_KAS_SETPASSWORD          },
   { uuKAS,  IDS_COMMAND_KAS_STRINGTOKEY,        IDH_USRMGR_COMMAND_KAS_STRINGTOKEY          },
   { uuKAS,  IDS_COMMAND_KAS_UNLOCK,             IDH_USRMGR_COMMAND_KAS_UNLOCK               },
};

// Precalculated hashing values for faster searching through help topics
static DWORD aSEARCHVALUES[] = {
   0x16765627, 0x02D416E6, 0xF657E647, 0x02143636,
   0x02146435, 0x37162736, 0x452716E6, 0x45865602,
   0xE6B30000, 0xE6967616, 0x02A45627, 0x86162746,
   0x02259636, 0xE6022697, 0x96474756, 0xE2007527,
   0xF6772797, 0xF62602D4, 0x26970224, 0x96563702,
   0x26271627, 0x5602C696, 0x16479667, 0x96374727,
   0x46D696E6, 0x00000014, 0xE64796C6, 0x96470257,
   0x02027516, 0x561647F3, 0x723702E6, 0x02F6E656,
   0x47869637, 0x96E6B602, 0x12004586, 0x56163756,
   0x022756C6, 0xE6568747, 0x47865602, 0x56E60000,
   0x22027786, 0xE616D656, 0x8696E656, 0xA3D61636,
   0xD6F64756, 0x22F22756, 0x45279702, 0xE647A302,
   0x12008496, 0x762716D6, 0x020727F6, 0x47869637,
   0x96E67602, 0x47162747, 0x00000037, 0xD2D2D200,
   0x00000000,
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
      if (!lstrcmpi (pszKeyword, TEXT("pts")))
         {
         fSkip = TRUE;
         *puu = uuPTS;
         }
      if (!lstrcmpi (pszKeyword, TEXT("kas")))
         {
         fSkip = TRUE;
         *puu = uuKAS;
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
   size_t iiDisplay = -1;

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
   AfsAppLib_TranslateError (szDesc, dwError);

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
UINT_PTR procAbout;

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
      GetWindowRect(GetDlgItem(GetParent(hDlg),0x07E5),&rr);
      DWORD dw = GetMessagePos();
      POINT pt = { LOWORD(dw), HIWORD(dw) };
      if (PtInRect (&rr, pt))
         {
         SetDlgItemText (GetParent(hDlg), 0x07E6, TEXT("\n\n\n\n\n\n\n"));
         SetTimer (GetParent(hDlg), 1000, 1000/8, NULL);
         }
      }
   return CallWindowProc ((WNDPROC)procAbout, hDlg, msg, wp, lp)?TRUE:FALSE;
}


void Help_About_OnInitDialog (HWND hDlg)
{
   HWND hAbout = GetDlgItem (hDlg, IDOK);
   procAbout = (UINT_PTR)GetWindowLongPtr (hAbout, GWLP_WNDPROC);
   SetWindowLongPtr (hAbout, GWLP_WNDPROC, (LONG_PTR)Help_About_Proc);

   LPTSTR pszText = FormatString (IDS_HELPABOUT_DESC1);
   SetDlgItemText (hDlg, IDC_HELPABOUT_DESC, pszText);
   FreeString (pszText);
}


void Help_About_OnSysCommand (HWND hDlg, int &cmd)
{
   DWORD dw;
   TCHAR szSys[cchRESOURCE];
   TCHAR szSys2[cchRESOURCE];
   LPTSTR psz;
   GetDlgItemText (hDlg, 0x07E6, szSys, cchRESOURCE);
   if ((dw = NextSearch (cmd)) != 0)
      {
      for (psz = &szSys[ lstrlen(szSys)-1 ]; *(psz-1) != TEXT('\n'); --psz);
      lstrcpy (szSys2, psz);
      wsprintf (psz, TEXT("%c%s"), (TCHAR)dw, szSys2);
      SetDlgItemText (hDlg, 0x07E6, szSys);
      }
   else // (dw == 0)
      {
      for (psz = szSys; *psz && (*psz != TEXT('\n')); ++psz);
      wsprintf (szSys2, TEXT("%s\n"), 1+psz);
      SetDlgItemText (hDlg, 0x07E6, szSys2);
      }
}



/*
 * CONTEXT HELP _____________________________________________________________
 *
 */

static DWORD IDD_OPENCELL_HELP[] = {
   IDC_OPENCELL_CELL,                IDH_USRMGR_OPENCELL_CELL,
   IDC_OPENCELL_OLDCREDS,            IDH_USRMGR_OPENCELL_ID_OLD,
   IDC_OPENCELL_ID,                  IDH_USRMGR_OPENCELL_ID_NEW,
   IDC_OPENCELL_PASSWORD,            IDH_USRMGR_OPENCELL_PASSWORD,
   0, 0
};

static DWORD IDD_COLUMNS_HELP[] = {
   IDC_COLUMNS,                      IDH_USRMGR_COLUMNS_WHICH,
   IDC_COL_AVAIL,                    IDH_USRMGR_COLUMNS_AVAIL,
   IDC_COL_SHOWN,                    IDH_USRMGR_COLUMNS_SHOWN,
   IDC_COL_INSERT,                   IDH_USRMGR_COLUMNS_INSERT,
   IDC_COL_DELETE,                   IDH_USRMGR_COLUMNS_DELETE,
   IDC_COL_UP,                       IDH_USRMGR_COLUMNS_MOVEUP,
   IDC_COL_DOWN,                     IDH_USRMGR_COLUMNS_MOVEDOWN,
   0, 0
};

static DWORD IDD_USER_GENERAL_HELP[] = {
   IDC_USER_CPW_NOW,                 IDH_USRMGR_USER_GENERAL_CPW_NOW,
   IDC_USER_CPW,                     IDH_USRMGR_USER_GENERAL_CPW,
   IDC_USER_RPW,                     IDH_USRMGR_USER_GENERAL_RPW,
   IDC_USER_PWEXPIRES,               IDH_USRMGR_USER_GENERAL_PWEXPIRES,
   IDC_USER_PWEXPIRATION,            IDH_USRMGR_USER_GENERAL_PWEXPIRATION,
   IDC_USER_FAILLOCK,                IDH_USRMGR_USER_GENERAL_FAILLOCK,
   IDC_USER_FAILLOCK_COUNT,          IDH_USRMGR_USER_GENERAL_FAILLOCK_COUNT,
   IDC_USER_FAILLOCK_INFINITE,       IDH_USRMGR_USER_GENERAL_FAILLOCK_INFINITE,
   IDC_USER_FAILLOCK_FINITE,         IDH_USRMGR_USER_GENERAL_FAILLOCK_FINITE,
   IDC_USER_FAILLOCK_DURATION,       IDH_USRMGR_USER_GENERAL_FAILLOCK_DURATION,
   IDC_USER_UNLOCK,                  IDH_USRMGR_USER_GENERAL_UNLOCK,
   IDC_USER_EXPIRES,                 IDH_USRMGR_USER_GENERAL_EXPIRES,
   IDC_USER_EXPIRE_DATE,             IDH_USRMGR_USER_GENERAL_EXPIRE_DATE,
   IDC_USER_EXPIRE_TIME,             IDH_USRMGR_USER_GENERAL_EXPIRE_TIME,
   0, 0
};

static DWORD IDD_USER_ADVANCED_HELP[] = {
   IDC_USER_NAME,                    IDH_USRMGR_USER_ADVANCED_NAME,
   IDC_USER_NOSEAL,                  IDH_USRMGR_USER_ADVANCED_NOSEAL,
   IDC_USER_ADMIN,                   IDH_USRMGR_USER_ADVANCED_ADMIN,
   IDC_USER_TGS,                     IDH_USRMGR_USER_ADVANCED_TGS,
   IDC_USER_LIFETIME,                IDH_USRMGR_USER_ADVANCED_LIFETIME,
   IDC_USER_GROUP_HASQUOTA,          IDH_USRMGR_USER_ADVANCED_GROUP_HASQUOTA,
   IDC_USER_GROUP_QUOTA,             IDH_USRMGR_USER_ADVANCED_GROUP_QUOTA,
   IDC_USER_PERM_STATUS,             IDH_USRMGR_USER_ADVANCED_PERM_STATUS,
   IDC_USER_PERM_OWNED,              IDH_USRMGR_USER_ADVANCED_PERM_OWNED,
   IDC_USER_PERM_MEMBER,             IDH_USRMGR_USER_ADVANCED_PERM_MEMBER,
   IDC_USER_CREATE_KAS,              IDH_USRMGR_USER_ADVANCED_CREATE_KAS,
   IDC_USER_CREATE_PTS,              IDH_USRMGR_USER_ADVANCED_CREATE_PTS,
   IDC_USER_KEY,                     IDH_USRMGR_USER_ADVANCED_KEY,
   0, 0
};

static DWORD IDD_USER_MEMBER_HELP[] = {
   IDC_GROUPS_LIST,                  IDH_USRMGR_USER_MEMBER_LIST,
   IDC_MEMBER_REMOVE,                IDH_USRMGR_USER_MEMBER_REMOVE,
   IDC_MEMBER_ADD,                   IDH_USRMGR_USER_MEMBER_ADD,
   IDC_USER_SHOW_MEMBER,             IDH_USRMGR_USER_MEMBER_SHOWMEMBER,
   IDC_USER_SHOW_OWNER,              IDH_USRMGR_USER_MEMBER_SHOWOWNER,
   0, 0
};

static DWORD IDD_GROUP_GENERAL_HELP[] = {
   IDC_GROUP_NAME,                   IDH_USRMGR_GROUP_GENERAL_NAME,
   IDC_GROUP_PERM_STATUS,            IDH_USRMGR_GROUP_GENERAL_PERM_STATUS,
   IDC_GROUP_PERM_GROUPS,            IDH_USRMGR_GROUP_GENERAL_PERM_GROUPS,
   IDC_GROUP_PERM_MEMBERS,           IDH_USRMGR_GROUP_GENERAL_PERM_MEMBERS,
   IDC_GROUP_PERM_ADD,               IDH_USRMGR_GROUP_GENERAL_PERM_ADD,
   IDC_GROUP_PERM_REMOVE,            IDH_USRMGR_GROUP_GENERAL_PERM_REMOVE,
   IDC_GROUP_OWNER,                  IDH_USRMGR_GROUP_GENERAL_OWNER,
   IDC_GROUP_CREATOR,                IDH_USRMGR_GROUP_GENERAL_CREATOR,
   IDC_GROUP_CHANGEOWNER,            IDH_USRMGR_GROUP_GENERAL_CHANGEOWNER,
   0, 0
};

static DWORD IDD_GROUP_MEMBER_HELP[] = {
   IDC_USERS_LIST,                   IDH_USRMGR_GROUP_MEMBER_LIST,
   IDC_MEMBER_REMOVE,                IDH_USRMGR_GROUP_MEMBER_REMOVE,
   IDC_MEMBER_ADD,                   IDH_USRMGR_GROUP_MEMBER_ADD,
   IDC_GROUP_SHOW_MEMBER,            IDH_USRMGR_GROUP_MEMBER_SHOWMEMBER,
   IDC_GROUP_SHOW_OWNER,             IDH_USRMGR_GROUP_MEMBER_SHOWOWNER,
   0, 0
};

static DWORD IDD_MACHINE_ADVANCED_HELP[] = {
   IDC_USER_NAME,                    IDH_USRMGR_MACHINE_ADVANCED_NAME,
   IDC_USER_GROUP_HASQUOTA,          IDH_USRMGR_MACHINE_ADVANCED_GROUP_HASQUOTA,
   IDC_USER_GROUP_QUOTA,             IDH_USRMGR_MACHINE_ADVANCED_GROUP_QUOTA,
   IDC_USER_PERM_STATUS,             IDH_USRMGR_MACHINE_ADVANCED_PERM_STATUS,
   IDC_USER_PERM_OWNED,              IDH_USRMGR_MACHINE_ADVANCED_PERM_OWNED,
   IDC_USER_PERM_MEMBER,             IDH_USRMGR_MACHINE_ADVANCED_PERM_MEMBER,
   0, 0
};

static DWORD IDD_MACHINE_MEMBER_HELP[] = {
   IDC_GROUPS_LIST,                  IDH_USRMGR_MACHINE_MEMBER_LIST,
   IDC_MEMBER_REMOVE,                IDH_USRMGR_MACHINE_MEMBER_REMOVE,
   IDC_MEMBER_ADD,                   IDH_USRMGR_MACHINE_MEMBER_ADD,
   IDC_USER_SHOW_MEMBER,             IDH_USRMGR_MACHINE_MEMBER_SHOWMEMBER,
   IDC_USER_SHOW_OWNER,              IDH_USRMGR_MACHINE_MEMBER_SHOWOWNER,
   0, 0
};

static DWORD IDD_CREDENTIALS_HELP[] = {
   IDC_CREDS_CELL,                   IDH_USRMGR_CREDS_CELL,
   IDC_CREDS_CURRENTID,              IDH_USRMGR_CREDS_CURRENTID,
   IDC_CREDS_EXPDATE,                IDH_USRMGR_CREDS_EXPDATE,
   IDC_CREDS_LOGIN,                  IDH_USRMGR_CREDS_LOGIN,
   IDC_CREDS_ID,                     IDH_USRMGR_CREDS_AFS_ID,
   IDC_CREDS_PASSWORD,               IDH_USRMGR_CREDS_AFS_PASSWORD,
   0, 0
};

static DWORD IDD_BADCREDS_HELP[] = {
   IDC_BADCREDS_SHUTUP,              IDH_USRMGR_BADCREDS_SHUTUP,
   IDOK,                             IDH_USRMGR_BADCREDS_YES,
   IDCANCEL,                         IDH_USRMGR_BADCREDS_NO,
   0, 0
};

static DWORD IDD_BROWSE_JOIN_HELP[] = {
   IDC_BROWSE_NAMED,                 IDH_USRMGR_BROWSE_JOIN_NAMES,
   IDC_BROWSE_CHECK,                 IDH_USRMGR_BROWSE_JOIN_LIMIT,
   IDC_BROWSE_CELL,                  IDH_USRMGR_BROWSE_JOIN_CELL,
   IDC_BROWSE_LIST,                  IDH_USRMGR_BROWSE_JOIN_LIST,
   IDC_BROWSE_SELECT,                IDH_USRMGR_BROWSE_JOIN_OK,
   IDCANCEL,                         IDH_USRMGR_BROWSE_JOIN_CANCEL,
   0, 0
};

static DWORD IDD_BROWSE_OWN_HELP[] = {
   IDC_BROWSE_NAMED,                 IDH_USRMGR_BROWSE_OWN_NAMES,
   IDC_BROWSE_CHECK,                 IDH_USRMGR_BROWSE_OWN_LIMIT,
   IDC_BROWSE_CELL,                  IDH_USRMGR_BROWSE_OWN_CELL,
   IDC_BROWSE_LIST,                  IDH_USRMGR_BROWSE_OWN_LIST,
   IDC_BROWSE_SELECT,                IDH_USRMGR_BROWSE_OWN_OK,
   IDCANCEL,                         IDH_USRMGR_BROWSE_OWN_CANCEL,
   0, 0
};

static DWORD IDD_BROWSE_MEMBER_HELP[] = {
   IDC_BROWSE_NAMED,                 IDH_USRMGR_BROWSE_MEMBER_NAMES,
   IDC_BROWSE_CHECK,                 IDH_USRMGR_BROWSE_MEMBER_LIMIT,
   IDC_BROWSE_CELL,                  IDH_USRMGR_BROWSE_MEMBER_CELL,
   IDC_BROWSE_LIST,                  IDH_USRMGR_BROWSE_MEMBER_LIST,
   IDC_BROWSE_SELECT,                IDH_USRMGR_BROWSE_MEMBER_OK,
   IDC_BROWSE_COMBO,                 IDH_USRMGR_BROWSE_MEMBER_COMBO,
   IDCANCEL,                         IDH_USRMGR_BROWSE_MEMBER_CANCEL,
   0, 0
};

static DWORD IDD_USER_PASSWORD_HELP[] = {
   IDC_CPW_VERSION_AUTO,             IDH_USRMGR_USER_PASSWORD_VERSION_AUTO,
   IDC_CPW_VERSION_MANUAL,           IDH_USRMGR_USER_PASSWORD_VERSION_MANUAL,
   IDC_CPW_VERSION,                  IDH_USRMGR_USER_PASSWORD_VERSION,
   IDC_CPW_BYSTRING,                 IDH_USRMGR_USER_PASSWORD_BYSTRING,
   IDC_CPW_STRING,                   IDH_USRMGR_USER_PASSWORD_STRING,
   IDC_CPW_BYDATA,                   IDH_USRMGR_USER_PASSWORD_BYDATA,
   IDC_CPW_DATA,                     IDH_USRMGR_USER_PASSWORD_DATA,
   IDC_CPW_RANDOM,                   IDH_USRMGR_USER_PASSWORD_RANDOM,
   0, 0
};

static DWORD IDD_GROUP_RENAME_HELP[] = {
   IDC_RENAME_OLDNAME,               IDH_USRMGR_GROUP_RENAME_OLDNAME,
   IDC_RENAME_NEWNAME,               IDH_USRMGR_GROUP_RENAME_NEWNAME,
   IDC_RENAME_OWNER,                 IDH_USRMGR_GROUP_RENAME_OWNER,
   IDC_RENAME_CHOWN,                 IDH_USRMGR_GROUP_RENAME_CHOWN,
   0, 0
};

static DWORD IDD_BROWSE_OWNER_HELP[] = {
   IDC_BROWSE_NAMED,                 IDH_USRMGR_BROWSE_OWNER_NAMES,
   IDC_BROWSE_CELL,                  IDH_USRMGR_BROWSE_OWNER_CELL,
   IDC_BROWSE_LIST,                  IDH_USRMGR_BROWSE_OWNER_LIST,
   IDC_BROWSE_SELECT,                IDH_USRMGR_BROWSE_OWNER_OK,
   IDC_BROWSE_COMBO,                 IDH_USRMGR_BROWSE_OWNER_COMBO,
   IDCANCEL,                         IDH_USRMGR_BROWSE_OWNER_CANCEL,
   0, 0
};

static DWORD IDD_BROWSE_OWNED_HELP[] = {
   IDC_BROWSE_NAMED,                 IDH_USRMGR_BROWSE_OWNED_NAMES,
   IDC_BROWSE_CHECK,                 IDH_USRMGR_BROWSE_OWNED_LIMIT,
   IDC_BROWSE_CELL,                  IDH_USRMGR_BROWSE_OWNED_CELL,
   IDC_BROWSE_LIST,                  IDH_USRMGR_BROWSE_OWNED_LIST,
   IDC_BROWSE_SELECT,                IDH_USRMGR_BROWSE_OWNED_OK,
   IDCANCEL,                         IDH_USRMGR_BROWSE_OWNED_CANCEL,
   0, 0
};

static DWORD IDD_NEWUSER_HELP[] = {
   IDC_NEWUSER_NAME,                 IDH_USRMGR_NEWUSER_NAME,
   IDC_NEWUSER_PW1,                  IDH_USRMGR_NEWUSER_PW1,
   IDC_NEWUSER_PW2,                  IDH_USRMGR_NEWUSER_PW2,
   IDC_NEWUSER_ID_AUTO,              IDH_USRMGR_NEWUSER_ID_AUTO,
   IDC_NEWUSER_ID_MANUAL,            IDH_USRMGR_NEWUSER_ID_MANUAL,
   IDC_NEWUSER_ID,                   IDH_USRMGR_NEWUSER_ID,
   IDC_ADVANCED,                     IDH_USRMGR_NEWUSER_ADVANCED,
   0, 0
};

static DWORD IDD_NEWGROUP_HELP[] = {
   IDC_NEWGROUP_NAME,                IDH_USRMGR_NEWGROUP_NAME,
   IDC_NEWGROUP_ID_AUTO,             IDH_USRMGR_NEWGROUP_ID_AUTO,
   IDC_NEWGROUP_ID_MANUAL,           IDH_USRMGR_NEWGROUP_ID_MANUAL,
   IDC_NEWGROUP_ID,                  IDH_USRMGR_NEWGROUP_ID,
   IDC_ADVANCED,                     IDH_USRMGR_NEWGROUP_ADVANCED,
   0, 0
};

static DWORD IDD_NEWMACHINE_HELP[] = {
   IDC_NEWUSER_NAME,                 IDH_USRMGR_NEWMACHINE_NAME,
   IDC_NEWUSER_ID_AUTO,              IDH_USRMGR_NEWMACHINE_ID_AUTO,
   IDC_NEWUSER_ID_MANUAL,            IDH_USRMGR_NEWMACHINE_ID_MANUAL,
   IDC_NEWUSER_ID,                   IDH_USRMGR_NEWMACHINE_ID,
   IDC_ADVANCED,                     IDH_USRMGR_NEWMACHINE_ADVANCED,
   0, 0
};

static DWORD IDD_USER_DELETE_HELP[] = {
   IDOK,                             IDH_USRMGR_USER_DELETE_OK,
   IDCANCEL,                         IDH_USRMGR_USER_DELETE_CANCEL,
   IDC_DELETE_KAS,                   IDH_USRMGR_USER_DELETE_KAS,
   IDC_DELETE_PTS,                   IDH_USRMGR_USER_DELETE_PTS,
   0, 0
};

static DWORD IDD_GROUP_DELETE_HELP[] = {
   IDOK,                             IDH_USRMGR_GROUP_DELETE_OK,
   IDCANCEL,                         IDH_USRMGR_GROUP_DELETE_CANCEL,
   0, 0
};

static DWORD IDD_MACHINE_DELETE_HELP[] = {
   IDOK,                             IDH_USRMGR_MACHINE_DELETE_OK,
   IDCANCEL,                         IDH_USRMGR_MACHINE_DELETE_CANCEL,
   0, 0
};

static DWORD IDD_CELL_GENERAL_HELP[] = {
   IDC_CELL_USERMAX,                 IDH_USRMGR_CELL_GENERAL_USERMAX,
   IDC_CELL_GROUPMAX,                IDH_USRMGR_CELL_GENERAL_GROUPMAX,
   0, 0
};

static DWORD IDD_OPTIONS_HELP[] = {
   IDC_REGEXP_UNIX,                  IDH_USRMGR_OPTIONS_REGEXP_UNIX,
   IDC_REGEXP_WINDOWS,               IDH_USRMGR_OPTIONS_REGEXP_WINDOWS,
   IDC_REFRESH,                      IDH_USRMGR_OPTIONS_REFRESH,
   IDC_REFRESH_RATE,                 IDH_USRMGR_OPTIONS_REFRESH_RATE,
   0, 0
};

static DWORD IDD_SEARCH_USERS_HELP[] = {
   IDC_SEARCH_ALL,                   IDH_USRMGR_SEARCH_USERS_SHOWALL,
   IDC_SEARCH_EXPIRE,                IDH_USRMGR_SEARCH_USERS_SHOWEXPIRE,
   IDC_SEARCH_EXPIRE_DATE,           IDH_USRMGR_SEARCH_USERS_EXPIREDATE,
   IDC_SEARCH_PWEXPIRE,              IDH_USRMGR_SEARCH_USERS_SHOWPWEXPIRE,
   IDC_SEARCH_PWEXPIRE_DATE,         IDH_USRMGR_SEARCH_USERS_PWEXPIREDATE,
   0, 0
};


void Main_ConfigureHelp (void)
{
   AfsAppLib_RegisterHelpFile (cszHELPFILENAME);

   AfsAppLib_RegisterHelp (IDD_APPLIB_OPENCELL, IDD_OPENCELL_HELP, IDH_USRMGR_OPENCELL_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_COLUMNS, IDD_COLUMNS_HELP, IDH_USRMGR_COLUMNS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_USER_GENERAL, IDD_USER_GENERAL_HELP, IDH_USRMGR_PROP_USER_GENERAL_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_USER_ADVANCED, IDD_USER_ADVANCED_HELP, IDH_USRMGR_PROP_USER_ADVANCED_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_USER_MEMBER, IDD_USER_MEMBER_HELP, IDH_USRMGR_PROP_USER_MEMBER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_GROUP_GENERAL, IDD_GROUP_GENERAL_HELP, IDH_USRMGR_PROP_GROUP_GENERAL_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_GROUP_MEMBER, IDD_GROUP_MEMBER_HELP, IDH_USRMGR_PROP_GROUP_MEMBER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_MACHINE_ADVANCED, IDD_MACHINE_ADVANCED_HELP, IDH_USRMGR_PROP_MACHINE_ADVANCED_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_MACHINE_MEMBER, IDD_MACHINE_MEMBER_HELP, IDH_USRMGR_PROP_MACHINE_MEMBER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_APPLIB_BADCREDS, IDD_BADCREDS_HELP, IDH_USRMGR_BADCREDS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_APPLIB_CREDENTIALS, IDD_CREDENTIALS_HELP, IDH_USRMGR_CREDENTIALS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_BROWSE_JOIN, IDD_BROWSE_JOIN_HELP, IDH_USRMGR_BROWSE_JOIN_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_BROWSE_OWN, IDD_BROWSE_OWN_HELP, IDH_USRMGR_BROWSE_OWN_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_BROWSE_MEMBER, IDD_BROWSE_MEMBER_HELP, IDH_USRMGR_BROWSE_MEMBER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_USER_PASSWORD, IDD_USER_PASSWORD_HELP, IDH_USRMGR_USER_PASSWORD_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_GROUP_RENAME, IDD_GROUP_RENAME_HELP, IDH_USRMGR_GROUP_RENAME_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_BROWSE_OWNER, IDD_BROWSE_OWNER_HELP, IDH_USRMGR_BROWSE_OWNER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_NEWUSER, IDD_NEWUSER_HELP, IDH_USRMGR_CREATE_USER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_NEWGROUP, IDD_NEWGROUP_HELP, IDH_USRMGR_CREATE_GROUP_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_NEWMACHINE, IDD_NEWMACHINE_HELP, IDH_USRMGR_CREATE_MACHINE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_USER_DELETE, IDD_USER_DELETE_HELP, IDH_USRMGR_DELETE_USER_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_GROUP_DELETE, IDD_GROUP_DELETE_HELP, IDH_USRMGR_DELETE_GROUP_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_MACHINE_DELETE, IDD_MACHINE_DELETE_HELP, IDH_USRMGR_DELETE_MACHINE_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_CELL_GENERAL, IDD_CELL_GENERAL_HELP, IDH_USRMGR_PROP_CELL_GENERAL_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_OPTIONS, IDD_OPTIONS_HELP, IDH_USRMGR_OPTIONS_OVERVIEW);
   AfsAppLib_RegisterHelp (IDD_SEARCH_USERS, IDD_SEARCH_USERS_HELP, IDH_USRMGR_SEARCH_USERS_OVERVIEW);
}

