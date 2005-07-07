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
#include "al_dynlink.h"
#include <WINNT/TaAfsAdmSvrClient.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define GWD_ANIMATIONFRAME  TEXT("afsapplib/al_misc.cpp - next animation frame")


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK AfsAppLib_TranslateErrorFunc (LPTSTR pszText, ULONG code, LANGID idLanguage);


/*
 * STARTUP ____________________________________________________________________
 *
 */

static HINSTANCE g_hInst = NULL;
static HINSTANCE g_hInstApp = NULL;

HINSTANCE AfsAppLib_GetInstance (void)
{
   return g_hInst;
}


EXPORTED HINSTANCE AfsAppLib_GetAppInstance (void)
{
   if (!g_hInstApp)
      g_hInstApp = GetModuleHandle(NULL);
   return g_hInstApp;
}


EXPORTED void AfsAppLib_SetAppInstance (HINSTANCE hInst)
{
   g_hInstApp = hInst;
}


extern "C" BOOLEAN _stdcall DllEntryPoint (HANDLE hInst, DWORD dwReason, PVOID pReserved)
{
   switch (dwReason)
      {
      case DLL_PROCESS_ATTACH:
         if (!g_hInst)
            {
            g_hInst = (HINSTANCE)hInst;
            TaLocale_LoadCorrespondingModule (g_hInst);
            SetErrorTranslationFunction ((LPERRORPROC)AfsAppLib_TranslateErrorFunc);

            InitCommonControls();
            RegisterSpinnerClass();
            RegisterElapsedClass();
            RegisterTimeClass();
            RegisterDateClass();
            RegisterSockAddrClass();
            RegisterCheckListClass();
            RegisterFastListClass();
            }
         break;
      }
   return TRUE;
}


/*
 * IMAGE LISTS ________________________________________________________________
 *
 */

HIMAGELIST AfsAppLib_CreateImageList (BOOL fLarge)
{
   HIMAGELIST hil;
   hil = ImageList_Create ((fLarge)?32:16, (fLarge)?32:16, TRUE, 4, 0);

   AfsAppLib_AddToImageList (hil, IDI_SERVER,           fLarge);
   AfsAppLib_AddToImageList (hil, IDI_SERVER_ALERT,     fLarge);
   AfsAppLib_AddToImageList (hil, IDI_SERVER_UNMON,     fLarge);
   AfsAppLib_AddToImageList (hil, IDI_SERVICE,          fLarge);
   AfsAppLib_AddToImageList (hil, IDI_SERVICE_ALERT,    fLarge);
   AfsAppLib_AddToImageList (hil, IDI_SERVICE_STOPPED,  fLarge);
   AfsAppLib_AddToImageList (hil, IDI_AGGREGATE,        fLarge);
   AfsAppLib_AddToImageList (hil, IDI_AGGREGATE_ALERT,  fLarge);
   AfsAppLib_AddToImageList (hil, IDI_FILESET,          fLarge);
   AfsAppLib_AddToImageList (hil, IDI_FILESET_ALERT,    fLarge);
   AfsAppLib_AddToImageList (hil, IDI_FILESET_LOCKED,   fLarge);
   AfsAppLib_AddToImageList (hil, IDI_BOSSERVICE,       fLarge);
   AfsAppLib_AddToImageList (hil, IDI_CELL,             fLarge);
   AfsAppLib_AddToImageList (hil, IDI_SERVER_KEY,       fLarge);
   AfsAppLib_AddToImageList (hil, IDI_USER,             fLarge);
   AfsAppLib_AddToImageList (hil, IDI_GROUP,            fLarge);

   return hil;
}


void AfsAppLib_AddToImageList (HIMAGELIST hil, int idi, BOOL fLarge)
{
   HICON hi = TaLocale_LoadIcon (idi);
   ImageList_AddIcon (hil, hi);
}


/*
 * ANIMATION __________________________________________________________________
 *
 */

void AfsAppLib_AnimateIcon (HWND hIcon, int *piFrameLast)
{
   static HICON hiStop;
   static HICON hiFrame[8];
   static BOOL fLoaded = FALSE;

   if (!fLoaded)
      {
      hiStop     = TaLocale_LoadIcon (IDI_SPINSTOP);
      hiFrame[0] = TaLocale_LoadIcon (IDI_SPIN1);
      hiFrame[1] = TaLocale_LoadIcon (IDI_SPIN2);
      hiFrame[2] = TaLocale_LoadIcon (IDI_SPIN3);
      hiFrame[3] = TaLocale_LoadIcon (IDI_SPIN4);
      hiFrame[4] = TaLocale_LoadIcon (IDI_SPIN5);
      hiFrame[5] = TaLocale_LoadIcon (IDI_SPIN6);
      hiFrame[6] = TaLocale_LoadIcon (IDI_SPIN7);
      hiFrame[7] = TaLocale_LoadIcon (IDI_SPIN8);
      fLoaded = TRUE;
      }

   if (piFrameLast)
      {
      *piFrameLast = (*piFrameLast == 7) ? 0 : (1+*piFrameLast);
      }

   SendMessage (hIcon, STM_SETICON, (WPARAM)((piFrameLast) ? hiFrame[ *piFrameLast ] : hiStop), 0);
}


BOOL CALLBACK AnimationHook (HWND hIcon, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hIcon, AnimationHook);

   switch (msg)
      {
      case WM_TIMER:
         int iFrame;
         iFrame = GetWindowData (hIcon, GWD_ANIMATIONFRAME);
         AfsAppLib_AnimateIcon (hIcon, &iFrame);
         SetWindowData (hIcon, GWD_ANIMATIONFRAME, iFrame);
         break;

      case WM_DESTROY:
         Subclass_RemoveHook (hIcon, AnimationHook);
         break;
      }

   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hIcon, msg, wp, lp);
   else
      return DefWindowProc (hIcon, msg, wp, lp);
}


void AfsAppLib_StartAnimation (HWND hIcon, int fps)
{
   Subclass_AddHook (hIcon, AnimationHook);
   SetTimer (hIcon, 0, 1000/((fps) ? fps : 8), NULL);
   AfsAppLib_AnimateIcon (hIcon);
}


void AfsAppLib_StopAnimation (HWND hIcon)
{
   KillTimer (hIcon, 0);
   AfsAppLib_AnimateIcon (hIcon);
   Subclass_RemoveHook (hIcon, AnimationHook);
}


/*
 * ERROR TRANSLATION __________________________________________________________
 *
 */

BOOL CALLBACK AfsAppLib_TranslateErrorFunc (LPTSTR pszText, ULONG code, LANGID idLanguage)
{
   DWORD idClient;
   if ((idClient = AfsAppLib_GetAdminServerClientID()) != 0)
      {
      ULONG status;
      return asc_ErrorCodeTranslate (idClient, code, idLanguage, pszText, &status);
      }
   else
       if (OpenUtilLibrary())
      {
      const char *pszTextA = NULL;
      afs_status_t status;
      if (util_AdminErrorCodeTranslate (code, idLanguage, &pszTextA, &status) && (pszTextA))
         {
         CopyAnsiToString (pszText, pszTextA);
         return TRUE;
         }
      CloseUtilLibrary();
      }

   return FALSE;
}


BOOL AfsAppLib_TranslateError (LPTSTR pszText, ULONG status, LANGID idLanguage)
{
   LANGID idLangOld = TaLocale_GetLanguage();
   TaLocale_SetLanguage (idLanguage);

   BOOL rc = FormatError (pszText, TEXT("%s"), status);

   TaLocale_SetLanguage (idLangOld);
   return rc;
}


/*
 * CELL LIST __________________________________________________________________
 *
 */

#define cREALLOC_CELLLIST   4

LPCELLLIST AfsAppLib_GetCellList (HKEY hkBase, LPTSTR pszRegPath)
{
   LPCELLLIST lpcl = New (CELLLIST);
   memset (lpcl, 0x00, sizeof(CELLLIST));

   if (hkBase && pszRegPath)
      {
      lpcl->hkBase = hkBase;
      lstrcpy (lpcl->szRegPath, pszRegPath);

      HKEY hk;
      if (RegOpenKey (hkBase, pszRegPath, &hk) == 0)
         {
         TCHAR szCell[ cchNAME ];
         for (size_t ii = 0; RegEnumKey (hk, ii, szCell, cchNAME) == 0; ++ii)
            {
            if (REALLOC (lpcl->aCells, lpcl->nCells, 1+ii, cREALLOC_CELLLIST))
               {
               lpcl->aCells[ ii ] = CloneString (szCell);
               }
            }
         RegCloseKey (hk);
         }
      }
   else // Get cell list from AFS
      {
      // TODO
      }

   TCHAR szDefCell[ cchNAME ];
   if (AfsAppLib_GetLocalCell (szDefCell))
      {
      size_t iclDef;
      for (iclDef = 0; iclDef < lpcl->nCells; ++iclDef)
         {
         if (lpcl->aCells[ iclDef ] == NULL)
            continue;
         if (!lstrcmpi (lpcl->aCells[ iclDef ], szDefCell))
            break;
         }
      if (iclDef == lpcl->nCells) // default cell not currently in list?
         {
         for (iclDef = 0; iclDef < lpcl->nCells; ++iclDef)
            {
            if (lpcl->aCells[ iclDef ] == NULL)
               break;
            }
         if (REALLOC (lpcl->aCells, lpcl->nCells, 1+iclDef, cREALLOC_CELLLIST))
            {
            lpcl->aCells[iclDef] = CloneString (szDefCell);
            }
         }
      if ((iclDef > 0) && (iclDef < lpcl->nCells) && (lpcl->aCells[ iclDef ]))
         {
         LPTSTR pszZero = lpcl->aCells[0];
         lpcl->aCells[0] = lpcl->aCells[iclDef];
         lpcl->aCells[iclDef] = pszZero;
         }
      }

   for ( ; (lpcl->nCells != 0); (lpcl->nCells)--)
      {
      if (lpcl->aCells[ lpcl->nCells-1 ] != NULL)
         break;
      }

   return lpcl;
}


LPCELLLIST AfsAppLib_GetCellList (LPCELLLIST lpclCopy)
{
   LPCELLLIST lpcl = New (CELLLIST);
   memset (lpcl, 0x00, sizeof(CELLLIST));

   if (lpclCopy)
      {
      if (REALLOC (lpcl->aCells, lpcl->nCells, lpclCopy->nCells, cREALLOC_CELLLIST))
         {
         for (size_t icl = 0; icl < lpcl->nCells; ++icl)
            {
            if (lpclCopy->aCells[ icl ])
               {
               lpcl->aCells[ icl ] = CloneString (lpclCopy->aCells[ icl ]);
               }
            }
         }
      }

   return lpcl;
}


void AfsAppLib_AddToCellList (LPCELLLIST lpcl, LPTSTR pszCell)
{
   if (lpcl && lpcl->hkBase && lpcl->szRegPath[0])
      {
      TCHAR szPath[ MAX_PATH ];
      wsprintf (szPath, TEXT("%s\\%s"), lpcl->szRegPath, pszCell);

      HKEY hk;
      if (RegCreateKey (lpcl->hkBase, szPath, &hk) == 0)
         {
         RegCloseKey (hk);
         }
      }
}


void AfsAppLib_FreeCellList (LPCELLLIST lpcl)
{
   if (lpcl)
      {
      if (lpcl->aCells)
         {
         for (size_t icl = 0; icl < lpcl->nCells; ++icl)
            {
            if (lpcl->aCells[icl] != NULL)
               FreeString (lpcl->aCells[icl]);
            }
         Free (lpcl->aCells);
         }
      Delete (lpcl);
      }
}


/*
 * TRULY MISCELLANEOUS ________________________________________________________
 *
 */

BOOL AfsAppLib_IsTimeInFuture (LPSYSTEMTIME pstTest)
{
   SYSTEMTIME stNow;
   GetSystemTime (&stNow);

   FILETIME ftNowGMT;
   SystemTimeToFileTime (&stNow, &ftNowGMT);

   FILETIME ftTest;
   SystemTimeToFileTime (pstTest, &ftTest);

   if (CompareFileTime (&ftTest, &ftNowGMT) >= 0)
      return TRUE;

   return FALSE;
}


void AfsAppLib_UnixTimeToSystemTime (LPSYSTEMTIME pst, ULONG ut, BOOL fElapsed)
{
   // A Unix time is the number of seconds since 1/1/1970.
   // The first step in this conversion is to change that count-of-seconds
   // into a count-of-100ns-intervals...
   //
   LARGE_INTEGER ldw;
   ldw.QuadPart = (LONGLONG)10000000 * (LONGLONG)ut;

   // Then adjust the count to be a count-of-100ns-intervals since
   // 1/1/1601, instead of 1/1/1970. That means adding a *big* number...
   //
   ldw.QuadPart += (LONGLONG)0x019db1ded53e8000;

   // Now the count is effectively a FILETIME, which we can convert
   // to a SYSTEMTIME with a Win32 API.
   //
   FILETIME ft;
   ft.dwHighDateTime = (DWORD)ldw.HighPart;
   ft.dwLowDateTime = (DWORD)ldw.LowPart;
   FileTimeToSystemTime (&ft, pst);

   if (fElapsed)
      {
      pst->wYear -= 1970;
      pst->wMonth -= 1;
      pst->wDayOfWeek -= 1;
      pst->wDay -= 1;
      }
}


void AfsAppLib_SplitCredentials (LPTSTR pszCreds, LPTSTR pszCell, LPTSTR pszID)
{
   LPTSTR pszSlash = (LPTSTR)lstrrchr (pszCreds, TEXT('/'));
   if (pszSlash == NULL)
      {
      if (pszCell)
         AfsAppLib_GetLocalCell (pszCell);
      if (pszID)
         lstrcpy (pszID, pszCreds);
      }
   else // a cell was specified
      {
      if (pszCell)
         lstrzcpy (pszCell, pszCreds, pszSlash -pszCreds);
      if (pszID)
         lstrcpy (pszID, 1+pszSlash);
      }
}


BOOL AfsAppLib_GetLocalCell (LPTSTR pszCell, ULONG *pStatus)
{
   static TCHAR szCell[ cchRESOURCE ] = TEXT("");

   BOOL rc = TRUE;
   ULONG status = 0;	

   if (szCell[0] == TEXT('\0'))
      {
      DWORD idClient;
      if ((idClient = AfsAppLib_GetAdminServerClientID()) != 0)
         {
         rc = asc_LocalCellGet (idClient, szCell, &status);
         }
      else 
          if (OpenClientLibrary())
         {
         char szCellNameA[ MAX_PATH ];
         if ((rc = afsclient_LocalCellGet (szCellNameA, (afs_status_p)&status)) == TRUE)
            {
            CopyAnsiToString (szCell, szCellNameA);
            }
         CloseClientLibrary();
         }
      }

   if (rc)
      lstrcpy (pszCell, szCell);
   else if (pStatus)
      *pStatus = status;
   return rc;
}


BOOL AfsAppLib_ReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)Allocate (cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget != 0)
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      Free (*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}


HFONT AfsAppLib_CreateFont (int idsFont)
{
   TCHAR szFont[ cchRESOURCE ];
   GetString (szFont, idsFont);

   HFONT hf = (HFONT)GetStockObject (DEFAULT_GUI_FONT);

   LOGFONT lf;
   GetObject (hf, sizeof(lf), &lf);
   lf.lfWeight = FW_NORMAL;

   LPTSTR pszSection = szFont;
   LPTSTR pszNextSection;
   if ((pszNextSection = (LPTSTR)lstrchr (pszSection, TEXT(','))) != NULL)
      *pszNextSection++ = TEXT('\0');
   if (!pszSection || !*pszSection)
      return NULL;

   lstrcpy (lf.lfFaceName, pszSection);

   pszSection = pszNextSection;
   if ((pszNextSection = (LPTSTR)lstrchr (pszSection, TEXT(','))) != NULL)
      *pszNextSection++ = TEXT('\0');
   if (!pszSection || !*pszSection)
      return NULL;

   HDC hdc = GetDC (NULL);
   lf.lfHeight = -MulDiv (_ttol(pszSection), GetDeviceCaps (hdc, LOGPIXELSY), 72);
   ReleaseDC (NULL, hdc);

   pszSection = pszNextSection;
   if ((pszNextSection = (LPTSTR)lstrchr (pszSection, TEXT(','))) != NULL)
      *pszNextSection++ = TEXT('\0');
   if (pszSection && *pszSection)
      {
      if (lstrchr (pszSection, TEXT('b')) || lstrchr (pszSection, TEXT('B')))
         lf.lfWeight = FW_BOLD;
      if (lstrchr (pszSection, TEXT('i')) || lstrchr (pszSection, TEXT('I')))
         lf.lfItalic = TRUE;
      if (lstrchr (pszSection, TEXT('u')) || lstrchr (pszSection, TEXT('U')))
         lf.lfUnderline = TRUE;
      }

   return CreateFontIndirect(&lf);
}

