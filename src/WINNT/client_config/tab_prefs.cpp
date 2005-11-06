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

#include "afs_config.h"
#include "tab_prefs.h"
#include <hashlist.h>
#include <stdlib.h>


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct l
   {
   CRITICAL_SECTION cs;
   BOOL fThreadActive;
   HWND hList;
   } l;

#define cREALLOC_PREFS   32

#ifndef iswhite
#define iswhite(_ch) (((_ch)==TEXT(' ')) || ((_ch)==TEXT('\t')))
#endif
#ifndef iseol
#define iseol(_ch) (((_ch)==TEXT('\r')) || ((_ch)==TEXT('\n')))
#endif
#ifndef iswhiteeol
#define iswhiteeol(_ch) (iswhite(_ch) || iseol(_ch))
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void PrefsTab_OnInitDialog (HWND hDlg);
BOOL PrefsTab_OnApply (HWND hDlg);
void PrefsTab_OnRefresh (HWND hDlg);
void PrefsTab_OnFillList (HWND hDlg);
void PrefsTab_OnSelect (HWND hDlg);
void PrefsTab_OnUpDown (HWND hDlg, BOOL fDown);
void PrefsTab_OnAdd (HWND hDlg);
void PrefsTab_OnEdit (HWND hDlg);
void PrefsTab_OnImport (HWND hDlg);

void PrefsTab_MergeServerPrefs (PSERVERPREFS pGlobal, PSERVERPREFS pAdd);
void PrefsTab_AddItem (HWND hDlg, PSERVERPREF pPref, BOOL fSelect);
void PrefsTab_AddItem (HWND hDlg, LPCTSTR pszServer, int iRank);

DWORD WINAPI PrefsTab_RefreshThread (PVOID lp);
DWORD WINAPI PrefsTab_ThreadProc (PVOID lp);
void PrefsTab_ThreadProcFunc (PSERVERPREFS pPrefs, BOOL *pfStopFlag);

int CALLBACK PrefsTab_SortFunction (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2);

BOOL CALLBACK IPKey_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData);
HASHVALUE CALLBACK IPKey_HashObject (LPHASHLISTKEY pKey, PVOID pObject);
HASHVALUE CALLBACK IPKey_HashData (LPHASHLISTKEY pKey, PVOID pData);

BOOL CALLBACK PrefsEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void PrefsEdit_OnInitDialog (HWND hDlg);
void PrefsEdit_OnOK (HWND hDlg);
void PrefsEdit_Enable (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK PrefsTab_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         InitializeCriticalSection (&l.cs);
         PrefsTab_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               if (!PrefsTab_OnApply (hDlg))
                  SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
               break;

            case IDC_REFRESH:
               PrefsTab_OnRefresh (hDlg);
               break;

            case IDC_SHOW_FS:
            case IDC_SHOW_VLS:
               PrefsTab_OnFillList (hDlg);
               break;

            case IDC_ADD:
               PrefsTab_OnAdd (hDlg);
               break;

            case IDC_EDIT:
               PrefsTab_OnEdit (hDlg);
               break;

            case IDC_UP:
               PrefsTab_OnUpDown (hDlg, FALSE);
               break;

            case IDC_DOWN:
               PrefsTab_OnUpDown (hDlg, TRUE);
               break;

            case IDC_IMPORT:
               PrefsTab_OnImport (hDlg);
               break;

            case IDHELP:
               PrefsTab_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_PREFS_NT);
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               PrefsTab_OnSelect (hDlg);
               break;

            case FLN_LDBLCLICK:
               if (IsWindowEnabled (GetDlgItem (hDlg, IDC_EDIT)))
                  PrefsTab_OnEdit (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void PrefsTab_OnInitDialog (HWND hDlg)
{
   HICON hiUp = TaLocale_LoadIcon (IDI_UP);
   HICON hiDown = TaLocale_LoadIcon (IDI_DOWN);

   SendDlgItemMessage (hDlg, IDC_UP, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hiUp);
   SendDlgItemMessage (hDlg, IDC_DOWN, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hiDown);

   CheckDlgButton (hDlg, IDC_SHOW_FS, TRUE);
   CheckDlgButton (hDlg, IDC_SHOW_VLS, FALSE);

   l.hList = GetDlgItem (hDlg, IDC_LIST);

   FASTLISTCOLUMN Column;
   Column.dwFlags = FLCF_JUSTIFY_LEFT;
   Column.cxWidth = 200;
   GetString (Column.szText, IDS_PREFCOL_SERVER);
   FastList_SetColumn (l.hList, 0, &Column);

   Column.dwFlags = FLCF_JUSTIFY_RIGHT;
   Column.cxWidth = 40;
   GetString (Column.szText, IDS_PREFCOL_RANK);
   FastList_SetColumn (l.hList, 1, &Column);

   FastList_SetSortFunction (l.hList, PrefsTab_SortFunction);

   PrefsTab_OnFillList (hDlg);
   PrefsTab_OnRefresh (hDlg);
}


BOOL PrefsTab_CommitChanges (BOOL fForce)
{
   HWND hDlg;
   if ((hDlg = PropSheet_FindTabWindow (g.psh, (DLGPROC)PrefsTab_DlgProc)) == NULL)
      return TRUE;
   if (fForce)
      SetWindowLongPtr (hDlg, DWLP_MSGRESULT, FALSE); // Make sure we try to apply
   if (PrefsTab_OnApply (hDlg))
      return TRUE;
   SetWindowLongPtr (hDlg, DWLP_MSGRESULT, TRUE);
   return FALSE;
}


BOOL PrefsTab_OnApply (HWND hDlg)
{
   // Don't try to do anything if we've already failed the apply
   if (GetWindowLongPtr (hDlg, DWLP_MSGRESULT))
      return FALSE;

   if (g.Configuration.pFServers && g.Configuration.fChangedPrefs)
      {
      if (!Config_SetServerPrefs (g.Configuration.pFServers))
         return FALSE;
      }
   if (g.Configuration.pVLServers && g.Configuration.fChangedPrefs)
      {
      if (!Config_SetServerPrefs (g.Configuration.pVLServers))
         return FALSE;
      }
   g.Configuration.fChangedPrefs = FALSE;
   return TRUE;
}


void PrefsTab_OnRefresh (HWND hDlg)
{
   DWORD idThread;
   CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)PrefsTab_RefreshThread, (PVOID)hDlg, 0, &idThread);
}


void PrefsTab_OnFillList (HWND hDlg)
{
   EnterCriticalSection (&l.cs);
   BOOL fVLServers = IsDlgButtonChecked (hDlg, IDC_SHOW_VLS);

   // Empty the fastlist, and clear from the lists any mention of HLISTITEMs.
   //
   FastList_Begin (l.hList);
   FastList_RemoveAll (l.hList);

   if (g.Configuration.pVLServers)
      {
      for (size_t ii = 0; ii < g.Configuration.pVLServers->cPrefs; ++ii)
         g.Configuration.pVLServers->aPrefs[ ii ].hItem = NULL;
      }
   if (g.Configuration.pFServers)
      {
      for (size_t ii = 0; ii < g.Configuration.pFServers->cPrefs; ++ii)
         g.Configuration.pFServers->aPrefs[ ii ].hItem = NULL;
      }

   // Fill in the fastlist by adding entries from the appropriate prefslist.
   //
   PSERVERPREFS pPrefs = (fVLServers) ? g.Configuration.pVLServers : g.Configuration.pFServers;
   if (pPrefs)
      {
      for (size_t ii = 0; ii < pPrefs->cPrefs; ++ii)
         {
         if (!pPrefs->aPrefs[ ii ].ipServer)
            continue;

         TCHAR szItem[ cchRESOURCE ];
         if (!pPrefs->aPrefs[ ii ].szServer[0])
            {
            lstrcpy (szItem, inet_ntoa (*(struct in_addr *)&pPrefs->aPrefs[ ii ].ipServer));
            }
         else
            {
            wsprintf (szItem, TEXT("%s (%s)"),
                      pPrefs->aPrefs[ ii ].szServer,
                      inet_ntoa (*(struct in_addr *)&pPrefs->aPrefs[ ii ].ipServer));
            }

         FASTLISTADDITEM ai;
         memset (&ai, 0x00, sizeof(FASTLISTADDITEM));
         ai.iFirstImage = IMAGE_NOIMAGE;
         ai.iSecondImage = IMAGE_NOIMAGE;
         ai.pszText = szItem;
         ai.lParam = ii;
         pPrefs->aPrefs[ ii ].hItem = FastList_AddItem (l.hList, &ai);

         wsprintf (szItem, TEXT("%ld"), pPrefs->aPrefs[ ii ].iRank);
         FastList_SetItemText (l.hList, pPrefs->aPrefs[ ii ].hItem, 1, szItem);
         }
      }

   // Okay, we're done!
   //
   FastList_End (l.hList);
   LeaveCriticalSection (&l.cs);
   PrefsTab_OnSelect (hDlg);
}


void PrefsTab_OnSelect (HWND hDlg)
{
   if (IsWindowEnabled (l.hList))
      {
      HLISTITEM hItem = FastList_FindFirstSelected (l.hList);
      HLISTITEM hItemFirst = FastList_FindFirst (l.hList);
      HLISTITEM hItemNext = FastList_FindNext (l.hList, hItem);

      EnableWindow (GetDlgItem (hDlg, IDC_UP), (hItem && (hItem != hItemFirst)));
      EnableWindow (GetDlgItem (hDlg, IDC_DOWN), (hItem && hItemNext));
      EnableWindow (GetDlgItem (hDlg, IDC_ADD), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_IMPORT), TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_EDIT), !!hItem);
      }
}


void PrefsTab_OnUpDown (HWND hDlg, BOOL fDown)
{
   BOOL fVLServers = IsDlgButtonChecked (hDlg, IDC_SHOW_VLS);
   PSERVERPREFS pPrefs = (fVLServers) ? g.Configuration.pVLServers : g.Configuration.pFServers;

   HLISTITEM hItem;
   if ((hItem = FastList_FindFirstSelected (l.hList)) == NULL)
      return;

   HLISTITEM hOther;
   hOther = (fDown) ? FastList_FindNext(l.hList,hItem) : FastList_FindPrevious(l.hList,hItem);
   if (hOther == NULL)
      return;

   size_t iItem = (size_t)FastList_GetItemParam (l.hList, hItem);
   size_t iOther = (size_t)FastList_GetItemParam (l.hList, hOther);

   if (!pPrefs || (pPrefs->cPrefs <= iItem) || (pPrefs->cPrefs <= iOther))
      return;

   FastList_Begin (l.hList);

   PSERVERPREF pPref1 = &pPrefs->aPrefs[ iItem ];
   PSERVERPREF pPref2 = &pPrefs->aPrefs[ iOther ];

   if (pPref1->iRank == pPref2->iRank)
      {
      if (fDown && (pPref1->iRank < 65534))
         pPref1->iRank ++;
      else if ((!fDown) && (pPref1->iRank > 1))
         pPref1->iRank --;
      pPref1->fChanged = TRUE;
      }
   else // (pPref1->iRating != pPref2->iRating)
      {
      pPref1->iRank ^= pPref2->iRank;
      pPref2->iRank ^= pPref1->iRank;
      pPref1->iRank ^= pPref2->iRank;
      pPref1->fChanged = TRUE;
      pPref2->fChanged = TRUE;
      }

   TCHAR szText[ cchRESOURCE ];
   wsprintf (szText, TEXT("%ld"), pPref1->iRank);
   FastList_SetItemText (l.hList, pPref1->hItem, 1, szText);

   wsprintf (szText, TEXT("%ld"), pPref2->iRank);
   FastList_SetItemText (l.hList, pPref2->hItem, 1, szText);

   FastList_EnsureVisible (l.hList, hItem);
   FastList_End (l.hList);
   PrefsTab_OnSelect (hDlg);

   g.Configuration.fChangedPrefs = TRUE;
}


void PrefsTab_OnAdd (HWND hDlg)
{
   BOOL fVLServers = IsDlgButtonChecked (hDlg, IDC_SHOW_VLS);
   PSERVERPREFS pPrefs = (fVLServers) ? g.Configuration.pVLServers : g.Configuration.pFServers;

   SERVERPREF Pref;
   memset (&Pref, 0x00, sizeof(SERVERPREF));
   Pref.iRank = 30000;

   if (ModalDialogParam (IDD_PREFS_EDIT, GetParent(hDlg), (DLGPROC)PrefsEdit_DlgProc, (LPARAM)&Pref) == IDOK)
      {
      PrefsTab_AddItem (hDlg, &Pref, TRUE);
      PrefsTab_OnSelect (hDlg);
      g.Configuration.fChangedPrefs = TRUE;
      }
}


void PrefsTab_OnEdit (HWND hDlg)
{
   BOOL fVLServers = IsDlgButtonChecked (hDlg, IDC_SHOW_VLS);
   PSERVERPREFS pPrefs = (fVLServers) ? g.Configuration.pVLServers : g.Configuration.pFServers;

   HLISTITEM hItem;
   if ((hItem = FastList_FindFirstSelected (l.hList)) == NULL)
      return;

   PSERVERPREF pPref = &pPrefs->aPrefs[ FastList_GetItemParam (l.hList, hItem) ];

   if (ModalDialogParam (IDD_PREFS_EDIT, GetParent(hDlg), (DLGPROC)PrefsEdit_DlgProc, (LPARAM)pPref) == IDOK)
      {
      FastList_Begin (l.hList);

      TCHAR szText[ cchRESOURCE ];
      wsprintf (szText, TEXT("%ld"), pPref->iRank);
      FastList_SetItemText (l.hList, pPref->hItem, 1, szText);
      pPref->fChanged = TRUE;

      FastList_EnsureVisible (l.hList, hItem);
      FastList_End (l.hList);
      PrefsTab_OnSelect (hDlg);
      g.Configuration.fChangedPrefs = TRUE;
      }
}


void PrefsTab_OnImport (HWND hDlg)
{
   BOOL fVLServers = IsDlgButtonChecked (hDlg, IDC_SHOW_VLS);
   PSERVERPREFS pPrefs = (fVLServers) ? g.Configuration.pVLServers : g.Configuration.pFServers;

   TCHAR szFilename[ MAX_PATH ] = TEXT("");
   if (Browse_Open (hDlg, szFilename, NULL, IDS_FILTER_TXT, 0, NULL, 0))
      {
      FastList_Begin (l.hList);

      // Open the file and read it into memory.
      //
      HANDLE fh;
      if ((fh = CreateFile (szFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
         {
         DWORD cbLength = GetFileSize (fh, NULL);
         LPTSTR pszBuffer = (LPTSTR)Allocate (sizeof(TCHAR) * (cbLength +2));

         DWORD cbRead;
         if (ReadFile (fh, pszBuffer, cbLength, &cbRead, NULL))
            {
            pszBuffer[ cbRead ] = TEXT('\0');
            pszBuffer[ cbRead+1 ] = TEXT('\0');

            // Scan the file line-by-line...
            //
            LPTSTR pszStart = pszBuffer;
            while (pszStart && *pszStart)
               {
               while (iswhiteeol(*pszStart))
                  ++pszStart;

               LPTSTR pszEnd = pszStart;
               while (*pszEnd && !iseol(*pszEnd))
                  ++pszEnd;
               *pszEnd++ = TEXT('\0');

               // Okay, {pszStart} points to a 0-terminated line in this file.
               // If the line starts with '#', ';' or '//', skip it.
               //
               if ( (pszStart[0] != TEXT('#')) &&
                    (pszStart[0] != TEXT(';')) &&
                   ((pszStart[0] != TEXT('/')) || (pszStart[1] != TEXT('/'))) )
                  {
                  // Break the line up into two sections: the machine name,
                  // and the ranking.
                  //
                  TCHAR szServer[ MAX_PATH ];
                  LPTSTR pszOut;
                  for (pszOut = szServer; *pszStart && !iswhite(*pszStart); )
                     *pszOut++ = *pszStart++;
                  *pszOut = TEXT('\0');

                  while (iswhite(*pszStart))
                     ++pszStart;

                  TCHAR szRank[ MAX_PATH ];
                  for (pszOut = szRank; *pszStart && !iswhite(*pszStart); )
                     *pszOut++ = *pszStart++;
                  *pszOut = TEXT('\0');

                  PrefsTab_AddItem (hDlg, szServer, atoi(szRank));
                  }

               // Process the next line in the file.
               //
               pszStart = pszEnd;
               }
            }

         Free (pszBuffer);
         CloseHandle (fh);
         }

      // Restart the background thread, to resolve unknown IP address
      //
      PrefsTab_OnRefresh (hDlg);
      FastList_End (l.hList);
      g.Configuration.fChangedPrefs = TRUE;
      }
}


void PrefsTab_MergeServerPrefs (PSERVERPREFS pGlobal, PSERVERPREFS pAdd)
{
   LPHASHLIST pList = New (HASHLIST);
   LPHASHLISTKEY pKey = pList->CreateKey (TEXT("IP Address"), IPKey_Compare, IPKey_HashObject, IPKey_HashData);

   size_t ii;
   for (ii = 0; ii < pGlobal->cPrefs; ++ii)
      {
      if (!pGlobal->aPrefs[ ii ].ipServer)
         continue;
      pList->Add (&pGlobal->aPrefs[ ii ]);
      }

   size_t iOut = 0;
   for (ii = 0; ii < pAdd->cPrefs; ++ii)
      {
      if (!pAdd->aPrefs[ ii ].ipServer)
         continue;

      // The whole point of using a hashlist here is to allow this next call--
      // on a hashlist, lookup and add are both constant-time, turning this
      // merge into O(N) instead of (O(N^2))
      //
      if (pKey->GetFirstObject (&pAdd->aPrefs[ ii ].ipServer))
         continue;

      for ( ; iOut < pGlobal->cPrefs; ++iOut)
         {
         if (!pGlobal->aPrefs[ iOut ].ipServer)
            break;
         }

      if (REALLOC (pGlobal->aPrefs, pGlobal->cPrefs, 1+iOut, cREALLOC_PREFS))
         {
         memcpy (&pGlobal->aPrefs[ iOut ], &pAdd->aPrefs[ ii ], sizeof(SERVERPREFS));
         iOut++;
         }
      }

   Delete (pList);
}


void PrefsTab_AddItem (HWND hDlg, PSERVERPREF pPref, BOOL fSelect)
{
   BOOL fVLServers = IsDlgButtonChecked (hDlg, IDC_SHOW_VLS);
   PSERVERPREFS pPrefs = (fVLServers) ? g.Configuration.pVLServers : g.Configuration.pFServers;

   size_t ii;
   for (ii = 0; ii < pPrefs->cPrefs; ++ii)
      {
      if (pPrefs->aPrefs[ ii ].ipServer == pPref->ipServer)
         break;
      }
   if (ii == pPrefs->cPrefs)
      {
      for (ii = 0; ii < pPrefs->cPrefs; ++ii)
         {
         if (!pPrefs->aPrefs[ ii ].ipServer)
            break;
         }
      if (!REALLOC (pPrefs->aPrefs, pPrefs->cPrefs, 1+ii, cREALLOC_PREFS))
         return;
      memcpy (&pPrefs->aPrefs[ ii ], pPref, sizeof(SERVERPREF));
      }

   FastList_Begin (l.hList);

   if (!pPrefs->aPrefs[ ii ].hItem)
      {
      TCHAR szItem[ cchRESOURCE ];
      if (!pPrefs->aPrefs[ ii ].szServer[0])
         {
         lstrcpy (szItem, inet_ntoa (*(struct in_addr *)&pPrefs->aPrefs[ ii ].ipServer));
         }
      else
         {
         wsprintf (szItem, TEXT("%s (%s)"),
                   pPrefs->aPrefs[ ii ].szServer,
                   inet_ntoa (*(struct in_addr *)&pPrefs->aPrefs[ ii ].ipServer));
         }

      FASTLISTADDITEM ai;
      memset (&ai, 0x00, sizeof(FASTLISTADDITEM));
      ai.iFirstImage = IMAGE_NOIMAGE;
      ai.iSecondImage = IMAGE_NOIMAGE;
      ai.pszText = szItem;
      ai.lParam = ii;
      pPrefs->aPrefs[ ii ].hItem = FastList_AddItem (l.hList, &ai);
      }

   TCHAR szText[ cchRESOURCE ];
   wsprintf (szText, TEXT("%ld"), pPrefs->aPrefs[ ii ].iRank);
   FastList_SetItemText (l.hList, pPrefs->aPrefs[ ii ].hItem, 1, szText);
   pPrefs->aPrefs[ ii ].fChanged = TRUE;

   FastList_End (l.hList);

   if (fSelect)
      {
      FastList_SelectItem (l.hList, pPrefs->aPrefs[ ii ].hItem, TRUE);
      FastList_SetFocus (l.hList, pPrefs->aPrefs[ ii ].hItem);
      FastList_EnsureVisible (l.hList, pPrefs->aPrefs[ ii ].hItem);
      PrefsTab_OnSelect (hDlg);
      }
}


void PrefsTab_AddItem (HWND hDlg, LPCTSTR pszServer, int iRank)
{
   if ((iRank < 1) || (iRank > 65534))
      return;

   SERVERPREF Pref;
   memset (&Pref, 0x00, sizeof(SERVERPREF));
   Pref.iRank = iRank;

   // If the server's name is an IP address, we'll translate it later
   // when we do them en masse.
   //
   if (isdigit (pszServer[0]))
      {
      if ((Pref.ipServer = inet_addr (pszServer)) == INADDR_NONE)
         return;
      }
   else // (!isdigit (pszServer[0]))
      {
      HOSTENT *pEntry;
      if ((pEntry = gethostbyname (pszServer)) != NULL)
         {
         lstrcpy (Pref.szServer, pEntry->h_name);
         Pref.ipServer = *(int *)pEntry->h_addr;
         }
      }

   PrefsTab_AddItem (hDlg, &Pref, FALSE);
}


DWORD WINAPI PrefsTab_RefreshThread (PVOID lp)
{
   HWND hDlg = (HWND)lp;
   static BOOL *pfStopFlag = NULL;

   // We may have a background thread or two working on resolving IP addresses.
   // Flag them to stop.
   //
   EnterCriticalSection (&l.cs);
   if (l.fThreadActive)
      {
      if (pfStopFlag)
         *pfStopFlag = FALSE;
      }
   pfStopFlag = NULL; // Thread will free this when it terminates

   // Retrieve PSERVERPREFS structures, and merge them into our globals
   //
   PSERVERPREFS pVLServers = Config_GetServerPrefs (TRUE);
   PSERVERPREFS pFServers = Config_GetServerPrefs (FALSE);

   if (!g.Configuration.pVLServers)
      g.Configuration.pVLServers = pVLServers;
   else if (g.Configuration.pVLServers && pVLServers)
      PrefsTab_MergeServerPrefs (g.Configuration.pVLServers, pVLServers);

   if (!g.Configuration.pFServers)
      g.Configuration.pFServers = pFServers;
   else if (g.Configuration.pFServers && pFServers)
      PrefsTab_MergeServerPrefs (g.Configuration.pFServers, pFServers);

   // Add entries to the fastlist
   //
   PrefsTab_OnFillList (hDlg);

   // Fire up a background thread to resolve IP addresses into server names
   //
   pfStopFlag = New (BOOL);
   *pfStopFlag = FALSE;

   DWORD idThread;
   CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)PrefsTab_ThreadProc, (PVOID)pfStopFlag, 0, &idThread);
   l.fThreadActive = TRUE;

   // Enable or disable controls based on whether the service is running
   //
   BOOL fRunning = (Config_GetServiceState() == SERVICE_RUNNING) ? TRUE : FALSE;

   EnableWindow (GetDlgItem (hDlg, IDC_SHOW_FS), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_SHOW_VLS), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_LIST), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_UP), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_DOWN), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_IMPORT), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_ADD), fRunning);
   EnableWindow (GetDlgItem (hDlg, IDC_EDIT), fRunning);
   PrefsTab_OnSelect (hDlg);

   TCHAR szText[ cchRESOURCE ];
   GetString (szText, (fRunning) ? IDS_TIP_PREFS : IDS_WARN_STOPPED);
   SetDlgItemText (hDlg, IDC_WARN, szText);

   // We're done!
   //
   LeaveCriticalSection (&l.cs);
   return 0;
}


DWORD WINAPI PrefsTab_ThreadProc (PVOID lp)
{
   BOOL *pfStopFlag = (BOOL*)lp;
   if (pfStopFlag)
      {
      PrefsTab_ThreadProcFunc (g.Configuration.pFServers, pfStopFlag);
      PrefsTab_ThreadProcFunc (g.Configuration.pVLServers, pfStopFlag);

      l.fThreadActive = FALSE;
      Delete (pfStopFlag);
      }
   return 0;
}


void PrefsTab_ThreadProcFunc (PSERVERPREFS pPrefs, BOOL *pfStopFlag)
{
   for (size_t ii = 0; ; ++ii)
      {
      // Find the next IP address to translate
      //
      EnterCriticalSection (&l.cs);

      if ( (*pfStopFlag) || (ii >= pPrefs->cPrefs) )
         {
         LeaveCriticalSection (&l.cs);
         break;
         }

      int ipServer;
      if ( ((ipServer = pPrefs->aPrefs[ ii ].ipServer) == 0) ||
           (pPrefs->aPrefs[ ii ].szServer[0] != TEXT('\0')) )
         {
         LeaveCriticalSection (&l.cs);
         continue;
         }

      LeaveCriticalSection (&l.cs);

      // Translate this IP address into a name
      //
      HOSTENT *pEntry;
      if ((pEntry = gethostbyaddr ((char*)&ipServer, sizeof(ipServer), AF_INET)) == NULL)
         continue;

      // Update the SERVERPREFS list, and if necessary, update the display
      // to show the server's name
      //
      EnterCriticalSection (&l.cs);

      if (!*pfStopFlag)
         {
         if ((ii < pPrefs->cPrefs) && (ipServer == pPrefs->aPrefs[ ii ].ipServer))
            {
            lstrcpy (pPrefs->aPrefs[ ii ].szServer, pEntry->h_name);
            if (pPrefs->aPrefs[ ii ].szServer[0])
               {
               if (pPrefs->aPrefs[ ii ].hItem)
                  {
                  TCHAR szItem[ cchRESOURCE ];
                  wsprintf (szItem, TEXT("%s (%s)"),
                            pPrefs->aPrefs[ ii ].szServer,
                            inet_ntoa (*(struct in_addr *)&pPrefs->aPrefs[ ii ].ipServer));

                  FastList_SetItemText (l.hList, pPrefs->aPrefs[ ii ].hItem, 0, szItem);
                  }
               }
            }
         }

      LeaveCriticalSection (&l.cs);
      }
}


int CALLBACK PrefsTab_SortFunction (HWND hList, HLISTITEM hItem1, LPARAM lpItem1, HLISTITEM hItem2, LPARAM lpItem2)
{
   static PSERVERPREFS pPrefs = NULL;
   if (!hItem1 || !hItem2)
      {
      BOOL fVLServers = IsDlgButtonChecked (GetParent(hList), IDC_SHOW_VLS);
      pPrefs = (fVLServers) ? g.Configuration.pVLServers : g.Configuration.pFServers;
      return 0;
      }

   if (!pPrefs || (pPrefs->cPrefs <= (size_t)lpItem1) || (pPrefs->cPrefs <= (size_t)lpItem2))
      return 0;

   PSERVERPREF pPref1 = &pPrefs->aPrefs[ lpItem1 ];
   PSERVERPREF pPref2 = &pPrefs->aPrefs[ lpItem2 ];

   if (pPref1->iRank != pPref2->iRank)
      return pPref1->iRank - pPref2->iRank;

   ULONG ip1 = (ULONG)htonl (pPref1->ipServer);
   ULONG ip2 = (ULONG)htonl (pPref2->ipServer);
   return (ip1 < ip2) ? -1 : 1;
}


BOOL CALLBACK IPKey_Compare (LPHASHLISTKEY pKey, PVOID pObject, PVOID pData)
{
   return (((PSERVERPREF)pObject)->ipServer == *(int*)pData);
}

HASHVALUE CALLBACK IPKey_HashObject (LPHASHLISTKEY pKey, PVOID pObject)
{
   return IPKey_HashData (pKey, &((PSERVERPREF)pObject)->ipServer);
}

HASHVALUE CALLBACK IPKey_HashData (LPHASHLISTKEY pKey, PVOID pData)
{
   return *(int*)pData;
}


BOOL CALLBACK PrefsEdit_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         PrefsEdit_OnInitDialog (hDlg);
         PrefsEdit_Enable (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_SERVER:
               PrefsEdit_Enable (hDlg);
               break;

            case IDOK:
               PrefsEdit_OnOK (hDlg);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDHELP:
               PrefsEdit_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_PREFS_NT_ADDEDIT);
         break;
      }

   return FALSE;
}


void PrefsEdit_OnInitDialog (HWND hDlg)
{
   PSERVERPREF pPref = (PSERVERPREF)GetWindowLongPtr (hDlg, DWLP_USER);

   if (pPref->ipServer)
      EnableWindow (GetDlgItem (hDlg, IDC_SERVER), FALSE);

   if (pPref->szServer[0])
      {
      SetDlgItemText (hDlg, IDC_SERVER, pPref->szServer);
      }
   else if (pPref->ipServer)
      {
      SetDlgItemText (hDlg, IDC_SERVER, inet_ntoa (*(struct in_addr *)&pPref->ipServer));
      }

   CreateSpinner (GetDlgItem (hDlg, IDC_RANK), 10, FALSE, 1, pPref->iRank, 65534);
}


void PrefsEdit_OnOK (HWND hDlg)
{
   PSERVERPREF pPref = (PSERVERPREF)GetWindowLongPtr (hDlg, DWLP_USER);
   pPref->iRank = SP_GetPos (GetDlgItem (hDlg, IDC_RANK));

   if (IsWindowEnabled (GetDlgItem (hDlg, IDC_SERVER)))
      {
      BOOL rc = TRUE;
      ULONG status = 0;

      TCHAR szServer[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_SERVER, szServer, cchRESOURCE);
      if (isdigit (szServer[0]))
         {
         if ((pPref->ipServer = inet_addr (szServer)) == INADDR_NONE)
            {
            rc = FALSE;
            status = WSAGetLastError();
            }
         else
            {
            HOSTENT *pEntry;
            if ((pEntry = gethostbyaddr ((char*)&pPref->ipServer, sizeof(pPref->ipServer), AF_INET)) != NULL)
               lstrcpy (pPref->szServer, pEntry->h_name);
            }
         }
      else // (!isdigit(pData->szServer[0]))
         {
         HOSTENT *pEntry;
         if ((pEntry = gethostbyname (szServer)) == NULL)
            {
            rc = FALSE;
            status = WSAGetLastError();
            }
         else
            {
            lstrcpy (pPref->szServer, pEntry->h_name);
            pPref->ipServer = *(int *)pEntry->h_addr;
            }
         }

      if (!rc)
         {
         Message (MB_ICONHAND | MB_OK, GetErrorTitle(), IDS_PREFERROR_RESOLVE, TEXT("%s%08lX"), szServer, status);
         return;
         }
      }

   EndDialog (hDlg, IDOK);
}


void PrefsEdit_Enable (HWND hDlg)
{
   TCHAR szServer[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_SERVER, szServer, cchRESOURCE);

   EnableWindow (GetDlgItem (hDlg, IDOK), !!szServer[0]);
}

