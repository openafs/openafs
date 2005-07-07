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
#include <stdio.h>
#include <process.h>
}

#include <WINNT/TaLocale.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   WORD wPriority;	// unused if MODULE_PRIORITY_REMOVED
   HINSTANCE hInstance;	// module handle
   } MODULE, *LPMODULE;

#define cREALLOC_MODULES  4


/*
 * VARIABLES __________________________________________________________________
 *
 */

static LANGID l_lang = LANG_USER_DEFAULT;
static CRITICAL_SECTION l_csModules;
static MODULE *l_aModules = NULL;
static size_t l_cModules = 0;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL IsValidStringTemplate (LPCSTRINGTEMPLATE pTable);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void TaLocale_Initialize (void)
{
    static BOOL fInitialized = FALSE;

    if (!fInitialized) {
        char mutexName[256];
        sprintf(mutexName, "TaLocale_Initialize pid=%d", getpid());
        HANDLE hMutex = CreateMutex(NULL, TRUE, mutexName);
        if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
            if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
                return;
            }
        }

        if (!fInitialized)
        {
            InitCommonControls();
            InitializeCriticalSection (&l_csModules);

            fInitialized = TRUE;
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);


            TaLocale_SpecifyModule (GetModuleHandle(NULL));

            LCID lcidUser = GetUserDefaultLCID();
            TaLocale_SetLanguage (LANGIDFROMLCID (lcidUser));

            LANGID LangOverride;
            if ((LangOverride = TaLocale_GetLanguageOverride()) != (LANGID)0)
                TaLocale_SetLanguage (LangOverride);
		}
    }
}


BOOL TaLocaleReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = Allocate (cbElement * cNew)) == NULL)
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


/*
 *** TaLocale_GuessBestLangID
 *
 * This routine picks the most common language/sublanguage pair based on
 * the current language and sublanguage. These values are based on the
 * most likely targets for our localization teams--e.g., we usually put
 * out a Simplified Chinese translation, so if we run under a Singapore
 * locale it's a good bet that we should load the Simplified Chinese
 * translation instead of just failing for lack of a Singapore binary.
 *
 */

LANGID TaLocale_GuessBestLangID (LANGID lang)
{
   switch (PRIMARYLANGID(lang))
      {
      case LANG_KOREAN:
         return MAKELANGID(LANG_KOREAN,SUBLANG_KOREAN);

      case LANG_JAPANESE:
         return MAKELANGID(LANG_JAPANESE,SUBLANG_DEFAULT);

      case LANG_ENGLISH:
         return MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US);

      case LANG_CHINESE:
         if (SUBLANGID(lang) != SUBLANG_CHINESE_TRADITIONAL)
            return MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_SIMPLIFIED);

      case LANG_GERMAN:
         return MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN);

      case LANG_SPANISH:
         return MAKELANGID(LANG_SPANISH,SUBLANG_SPANISH);

      case LANG_PORTUGUESE:
         return MAKELANGID(LANG_PORTUGUESE,SUBLANG_PORTUGUESE_BRAZILIAN);
      }

   return lang;
}


/*
 *** TaLocale_SpecifyModule
 *
 * Adds the given module handle to TaLocale's known list of possible sources
 * for localized resource data. By default, the list is initialized with
 * the current executable's module handle; if resources are to be extracted
 * from other .DLLs using the functions in the TaLocale library, this routine
 * should be called first to notify TaLocale about each .DLL.
 *
 */

void TaLocale_SpecifyModule (HINSTANCE hInstance, WORD wSearchPriority)
{
   TaLocale_Initialize();
   EnterCriticalSection (&l_csModules);

   // First see if the specified module handle already exists; if
   // so, remove it (its priority may be changing, or we may actually
   // have been asked to remove it).
   //
   size_t iModule;
   for (iModule = 0; iModule < l_cModules; ++iModule)
      {
      if (l_aModules[ iModule ].wPriority == 0)  // end of list?
         break;
      if (l_aModules[ iModule ].hInstance == hInstance)  // found module?
         break;
      }
   if (iModule < l_cModules)
      {
      for ( ; iModule < l_cModules-1; ++iModule)
         memcpy (&l_aModules[ iModule ], &l_aModules[ iModule+1 ], sizeof(MODULE));
      memset (&l_aModules[ iModule ], 0x00, sizeof(MODULE));
      }

   // Next, if we have been asked to add this module, find a good place for it.
   // We want to leave {iModule} pointing to the slot where this module
   // should be inserted--anything currently at or after {iModule} will have
   // to be moved outward.
   //
   if (wSearchPriority != MODULE_PRIORITY_REMOVE)
      {
      for (iModule = 0; iModule < l_cModules; ++iModule)
         {
         if (l_aModules[ iModule ].wPriority == 0)  // end of list?
            break;
         if (l_aModules[ iModule ].wPriority > wSearchPriority)  // good place?
            break;
         }

      size_t iTarget;
      for (iTarget = iModule; iTarget < l_cModules; ++iTarget)
         {
         if (l_aModules[ iTarget ].wPriority == 0)  // end of list?
            break;
         }

      if (REALLOC (l_aModules, l_cModules, 1+iTarget, cREALLOC_MODULES))
         {
         for (size_t iSource = iTarget -1; (LONG)iSource >= (LONG)iModule; --iSource, --iTarget)
            memcpy (&l_aModules[ iTarget ], &l_aModules[ iSource ], sizeof(MODULE));

         l_aModules[ iModule ].wPriority = wSearchPriority;
         l_aModules[ iModule ].hInstance = hInstance;
         }
      }

   LeaveCriticalSection (&l_csModules);
}


/*
 *** FindAfsCommonPath
 *
 * Because our localized files are often placed in a single Common directory
 * for the AFS product, we need to know how to find that directory. This
 * routine, and its helper FindAfsCommonPathByComponent(), do that search.
 *
 */

BOOL FindAfsCommonPathByComponent (LPTSTR pszCommonPath, LPTSTR pszComponent)
{
   *pszCommonPath = 0;

   TCHAR szRegPath[ MAX_PATH ];
   wsprintf (szRegPath, TEXT("Software\\TransarcCorporation\\%s\\CurrentVersion"), pszComponent);

   HKEY hk;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, szRegPath, &hk) == 0)
      {
      DWORD dwType = REG_SZ;
      DWORD dwSize = MAX_PATH;

      if (RegQueryValueEx (hk, TEXT("PathName"), NULL, &dwType, (PBYTE)pszCommonPath, &dwSize) == 0)
         {
         *(LPTSTR)FindBaseFileName (pszCommonPath) = TEXT('\0');

         if (pszCommonPath[0] && (pszCommonPath[ lstrlen(pszCommonPath)-1 ] == TEXT('\\')))
            pszCommonPath[ lstrlen(pszCommonPath)-1 ] = TEXT('\0');

         if (pszCommonPath[0])
            lstrcat (pszCommonPath, TEXT("\\Common"));
         }

      RegCloseKey (hk);
      }

   return !!*pszCommonPath;
}


BOOL FindAfsCommonPath (LPTSTR pszCommonPath)
{
   if (FindAfsCommonPathByComponent (pszCommonPath, TEXT("AFS Client")))
      return TRUE;
   if (FindAfsCommonPathByComponent (pszCommonPath, TEXT("AFS Control Center")))
      return TRUE;
   if (FindAfsCommonPathByComponent (pszCommonPath, TEXT("AFS Server")))
      return TRUE;
   if (FindAfsCommonPathByComponent (pszCommonPath, TEXT("AFS Supplemental Documentation")))
      return TRUE;
   return FALSE;
}


/*
 *** TaLocale_LoadCorrespondingModule
 *
 * This routine looks for a .DLL named after the specified module, but
 * with a suffix reflecting the current locale--it loads that library
 * and calls TaLocale_SpecifyModule for the library.
 *
 */

HINSTANCE TaLocale_LoadCorrespondingModule (HINSTANCE hInstance, WORD wSearchPriority)
{
   // If the caller was sloppy and didn't supply an instance handle,
   // assume we should find the module corresponding with the current .EXE.
   //
   if (hInstance == NULL)
      hInstance = GetModuleHandle(NULL);

   TCHAR szFilename[ MAX_PATH ] = TEXT("");
   GetModuleFileName (hInstance, szFilename, MAX_PATH);

   return TaLocale_LoadCorrespondingModuleByName (hInstance, szFilename, wSearchPriority);
}

HINSTANCE TaLocale_LoadCorrespondingModuleByName (HINSTANCE hInstance, LPTSTR pszFilename, WORD wSearchPriority)
{
   HINSTANCE hDLL = NULL;

   TCHAR szFilename[ MAX_PATH ];
   if (lstrchr (pszFilename, TEXT('\\')) != NULL)
      lstrcpy (szFilename, pszFilename);
   else
      {
      GetModuleFileName (hInstance, szFilename, MAX_PATH);
      lstrcpy ((LPTSTR)FindBaseFileName (szFilename), pszFilename);
      }


   // If the caller was sloppy and didn't supply an instance handle,
   // assume we should find the module corresponding with the current .EXE.
   //
   if (hInstance == NULL)
      hInstance = GetModuleHandle(NULL);

   LPTSTR pchExtension;
   if ((pchExtension = (LPTSTR)FindExtension (szFilename)) != NULL)
      {

      // Find the filename associated with the specified module, remove its
      // extension, and append "_409.dll" (where the 409 is, naturally, the
      // current LANGID). Then try to load that library.
      //
      wsprintf (pchExtension, TEXT("_%lu.dll"), TaLocale_GetLanguage());
      if ((hDLL = LoadLibrary (szFilename)) == NULL)
         hDLL = LoadLibrary (FindBaseFileName (szFilename));

      // If we couldn't find the thing under that name, it's possible we
      // have a .DLL made for the proper language but not for the user's
      // specific sublanguage (say, a US English .DLL but we're running
      // in a Canadian English locale). Make an intelligent guess about
      // what the valid ID would be.
      //
      if (hDLL == NULL)
         {
         wsprintf (pchExtension, TEXT("_%lu.dll"), TaLocale_GuessBestLangID (TaLocale_GetLanguage()));
         if ((hDLL = LoadLibrary (szFilename)) == NULL)
            hDLL = LoadLibrary (FindBaseFileName (szFilename));
         }

      // If we STILL couldn't find a corresponding resource library,
      // we'll take anything we can find; this should cover the case
      // where a Setup program asked the user what language to use,
      // and just installed that matching library. Look in the
      // appropriate directory for any .DLL that fits the naming convention.
      //
      if (hDLL == NULL)
         {
         wsprintf (pchExtension, TEXT("_*.dll"));

         WIN32_FIND_DATA wfd;
         memset (&wfd, 0x00, sizeof(wfd));

         HANDLE hFind;
         if ((hFind = FindFirstFile (szFilename, &wfd)) != NULL)
            {
            if (wfd.cFileName[0])
               {
               wsprintf ((LPTSTR)FindBaseFileName (szFilename), wfd.cFileName);
               hDLL = LoadLibrary (szFilename);
               }
            FindClose (hFind);
            }
         }

      // If we EVEN NOW couldn't find a corresponding resource library,
      // we may have done our wildcard search in the wrong directory.
      // Try to find the Common subdirectory of our AFS installation,
      // and look for any matching DLL there.
      //
      if (hDLL == NULL)
         {
         wsprintf (pchExtension, TEXT("_*.dll"));

         TCHAR szCommonPath[ MAX_PATH ];
         if (FindAfsCommonPath (szCommonPath))
            {
            lstrcat (szCommonPath, TEXT("\\"));
            lstrcat (szCommonPath, FindBaseFileName (szFilename));

            WIN32_FIND_DATA wfd;
            memset (&wfd, 0x00, sizeof(wfd));

            HANDLE hFind;
            if ((hFind = FindFirstFile (szCommonPath, &wfd)) != NULL)
               {
               if (wfd.cFileName[0])
                  {
                  wsprintf ((LPTSTR)FindBaseFileName (szCommonPath), wfd.cFileName);
                  hDLL = LoadLibrary (szCommonPath);
                  }
               FindClose (hFind);
               }
            }
         }

      // If all else fails, we'll try to find the English library
      // somewhere on our path.
      //
      if (hDLL == NULL)
         {
         wsprintf (pchExtension, TEXT("_%lu.dll"), MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US));
         if ((hDLL = LoadLibrary (szFilename)) == NULL)
            hDLL = LoadLibrary (FindBaseFileName (szFilename));
         }

      // If we were successful in loading the resource library, add it
      // to our chain of modules-to-search
      //
      if (hDLL != NULL)
         {
         TaLocale_SpecifyModule (hDLL, wSearchPriority);
         }
      }

   return hDLL;
}


/*
 *** TaLocale_EnumModule
 *
 * Enables enumeration of each of the product modules which will be seached
 * by TaLocale for resources. Use TaLocale_SpecifyModule to modify this list.
 * Modules will be returned in priority order, with the highest-priority
 * modules being searched first.
 *
 */

BOOL TaLocale_EnumModule (size_t iModule, HINSTANCE *phInstance, WORD *pwSearchPriority)
{
   BOOL rc = FALSE;
   TaLocale_Initialize();
   EnterCriticalSection (&l_csModules);

   if ( (iModule < l_cModules) && (l_aModules[ iModule ].wPriority != 0) )
      {
      rc = TRUE;
      if (phInstance)
         *phInstance = l_aModules[ iModule ].hInstance;
      if (pwSearchPriority)
         *pwSearchPriority = l_aModules[ iModule ].wPriority;
      }

   LeaveCriticalSection (&l_csModules);
   return rc;
}


/*
 *** TaLocale_GetLanguage
 *** TaLocale_SetLanguage
 *
 * Allows specification of a default language for resources extracted by
 * the functions exported by tal_string.h and tal_dialog.h. When a particular
 * string or dialog resource is required, the resource which matches this
 * specified language will be retrieved--if no localized resource is available,
 * the default resource will instead be used.
 *
 */

LANGID TaLocale_GetLanguage (void)
{
   TaLocale_Initialize();
   return l_lang;
}


void TaLocale_SetLanguage (LANGID lang)
{
   TaLocale_Initialize();
   l_lang = lang;
}


/*
 *** TaLocale_GetLanguageOverride
 *** TaLocale_SetLanguageOverride
 *
 * Allows specification of a persistent default language for resources
 * extracted by the functions exported by tal_string.h and tal_dialog.h.
 * If a language override (which is really just a registry entry) exists,
 * all TaLocale-based applications will default to that locale when first
 * run.
 *
 */

static HKEY REGSTR_BASE_OVERRIDE = HKEY_LOCAL_MACHINE;
static TCHAR REGSTR_PATH_OVERRIDE[] = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Nls");
static TCHAR REGSTR_VAL_OVERRIDE[] = TEXT("Default Language");

LANGID TaLocale_GetLanguageOverride (void)
{
   DWORD dwLang = 0;

   HKEY hk;
   if (RegOpenKey (REGSTR_BASE_OVERRIDE, REGSTR_PATH_OVERRIDE, &hk) == 0)
      {
      DWORD dwType = REG_DWORD;
      DWORD dwSize = sizeof(DWORD);
      if (RegQueryValueEx (hk, REGSTR_VAL_OVERRIDE, 0, &dwType, (PBYTE)&dwLang, &dwSize) != 0)
         dwLang = 0;
      RegCloseKey (hk);
      }

   return (LANGID)dwLang;
}


void TaLocale_SetLanguageOverride (LANGID lang)
{
   HKEY hk;
   if (RegCreateKey (REGSTR_BASE_OVERRIDE, REGSTR_PATH_OVERRIDE, &hk) == 0)
      {
      DWORD dwLang = (DWORD)lang;
      RegSetValueEx (hk, REGSTR_VAL_OVERRIDE, 0, REG_DWORD, (PBYTE)&dwLang, sizeof(DWORD));
      RegCloseKey (hk);
      }
}


void TaLocale_RemoveLanguageOverride (void)
{
   HKEY hk;
   if (RegOpenKey (REGSTR_BASE_OVERRIDE, REGSTR_PATH_OVERRIDE, &hk) == 0)
      {
      RegDeleteValue (hk, REGSTR_VAL_OVERRIDE);
      RegCloseKey (hk);
      }
}


/*
 *** TaLocale_GetResource
 *
 * Returns a pointer an in-memory image of a language-specific resource.
 * The resource is found by searching all specified module handles
 * (as determined automatically, or as specified by previous calls to
 * TaLocale_SpecifyModule()) for an equivalent resource identifier matching
 * the requested resource type and localized into the requested language.
 * In the event that a matching localized resource cannot be found, the
 * search is repeated using LANG_USER_DEFAULT.
 *
 * The pointer returned should be treated as read-only, and should not be freed.
 *
 */

LPCVOID TaLocale_GetResourceEx (LPCTSTR pszType, LPCTSTR pszRes, LANGID lang, HINSTANCE *phInstFound, BOOL fSearchDefaultLanguageToo)
{
   PVOID pr = NULL;

   HINSTANCE hInstance;
   for (size_t iModule = 0; TaLocale_EnumModule (iModule, &hInstance); ++iModule)
      {
      HRSRC hr;
      if ((hr = FindResourceEx (hInstance, pszType, pszRes, lang)) == NULL)
         {
         // Our translation teams don't usually change the language
         // constants within .RC files, so we should look for English
         // language translations too.
         //
         if ((hr = FindResourceEx (hInstance, pszType, pszRes, MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US))) == NULL)
            {
            // If we still can't find it, we'll take anything...
            //
            if ((hr = FindResource (hInstance, pszType, pszRes)) == NULL)
               continue;
            }
         }

      // Once we get here, we'll only fail for weird out-of-memory problems.
      //
      HGLOBAL hg;
      if ((hg = LoadResource (hInstance, hr)) != NULL)
         {
         if ((pr = (PVOID)LockResource (hg)) != NULL)
            {
            if (phInstFound)
               *phInstFound = hInstance;
            break;
            }
         }
      }

   return pr;
}


LPCVOID TaLocale_GetResource (LPCTSTR pszType, LPCTSTR pszRes, LANGID lang, HINSTANCE *phInstFound)
{
   return TaLocale_GetResourceEx (pszType, pszRes, lang, phInstFound, TRUE);
}



/*
 *** TaLocale_GetStringResource
 *** TaLocale_GetDialogResource
 *
 * Convenience wrappers around TaLocale_GetResource for obtaining resources
 * of a particular type by numeric identifier; these routines specify the
 * default language (from TaLocale_SetLanguage()) when calling
 * TaLocale_GetResource().
 *
 */

LPCDLGTEMPLATE TaLocale_GetDialogResource (int idd, HINSTANCE *phInstFound)
{
   return (LPCDLGTEMPLATE)TaLocale_GetResource (RT_DIALOG, MAKEINTRESOURCE( idd ), TaLocale_GetLanguage(), phInstFound);
}


LPCSTRINGTEMPLATE TaLocale_GetStringResource (int ids, HINSTANCE *phInstFound)
{
    // Strings are organized into heaps of String Tables, each table
    // holding 16 strings (regardless of their length). The first table's
    // first string index is for string #1. When searching for a string,
    // the string's table is the index given to FindResource.
    //
    LPCSTRINGTEMPLATE pst = NULL;
    LANGID lang = TaLocale_GetLanguage();

    int iTable = (ids / 16) + 1;           // 1 = first string table
    int iIndex = ids - ((iTable-1) * 16);  // 0 = first string in the table

    HINSTANCE hInstance;
    for (size_t iModule = 0; !pst && TaLocale_EnumModule (iModule, &hInstance); ++iModule)
    {
        HRSRC hr;
        if ((hr = FindResourceEx (hInstance, RT_STRING, MAKEINTRESOURCE( iTable ), lang)) == NULL)
        {
            // Our translation teams don't usually change the language
            // constants within .RC files, so we should look for English
            // language translations too.
            //
            if ((hr = FindResourceEx (hInstance, RT_STRING, MAKEINTRESOURCE( iTable ), MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US))) == NULL)
            {
                // If we still can't find it, we'll take anything...
                //
                if ((hr = FindResource (hInstance, RT_STRING, MAKEINTRESOURCE( iTable ))) == NULL)
                    continue;
            }
        }

        HGLOBAL hg;
        if ((hg = LoadResource (hInstance, hr)) != NULL)
        {
            const WORD *pTable;
            if     ((pTable = (WORD*)LockResource (hg)) != NULL)
            {
                try {
                    // Skip words in the string table until we reach the string
                    // index we're looking for.
                    //
                    for (int iIndexWalk = iIndex; iIndexWalk && ((LPCSTRINGTEMPLATE)pTable)->cchString; --iIndexWalk) {
                        pTable += 1 + ((LPCSTRINGTEMPLATE)pTable)->cchString;
                    }

                    if (IsValidStringTemplate ((LPCSTRINGTEMPLATE)pTable))
                    {
                        pst = (LPCSTRINGTEMPLATE)pTable;
                        if (phInstFound)
                            *phInstFound = hInstance;
                    } else {
                        UnlockResource(pTable);
                        FreeResource(hg);
                    }
                }
                catch(...)
                {
                    UnlockResource(pTable);
                    FreeResource(hg);
                    // If we walked off the end of the table, then the
                    // string we want just wasn't there.
                }
            }
        }
    }

    return pst;
}


BOOL IsValidStringTemplate (LPCSTRINGTEMPLATE pTable)
{
    if (!pTable->cchString)
        return FALSE;

    for (size_t ii = 0; ii < pTable->cchString; ++ii)
    {
        if (!pTable->achString[ii])
            return FALSE;
    }

    return TRUE;
}


/*
 *** TaLocale_LoadMenu
 *** TaLocale_LoadImage
 *
 * Replacements for Win32 functions. By using these functions instead, the
 * caller can load the appropriate resources regardless of the module in
 * which they reside, or the language which is required.
 *
 */

HMENU TaLocale_LoadMenu (int idm)
{
   const MENUTEMPLATE *pTemplate;
   if ((pTemplate = (const MENUTEMPLATE *)TaLocale_GetResource (RT_MENU, MAKEINTRESOURCE( idm ), TaLocale_GetLanguage())) == NULL)
      return NULL;
   return LoadMenuIndirect (pTemplate);
}


HANDLE TaLocale_LoadImage (int idi, UINT imageType, int cx, int cy, UINT imageFlags)
{
   HINSTANCE hInstance;
   for (size_t iModule = 0; TaLocale_EnumModule (iModule, &hInstance); ++iModule)
      {
      HANDLE hImage;
      if (imageType == IMAGE_ICON)
         hImage = (HANDLE)LoadIcon (hInstance, MAKEINTRESOURCE(idi));
      else
         hImage = LoadImage (hInstance, MAKEINTRESOURCE(idi), imageType, cx, cy, imageFlags);

      if (hImage != NULL)
         return hImage;
      }

   return NULL;
}


HICON TaLocale_LoadIcon (int idi)
{
   return (HICON)TaLocale_LoadImage (idi, IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
}


HACCEL TaLocale_LoadAccelerators (int ida)
{
   HINSTANCE hInstance;
   for (size_t iModule = 0; TaLocale_EnumModule (iModule, &hInstance); ++iModule)
      {
      HACCEL hAccel;
      if ((hAccel = LoadAccelerators (hInstance, MAKEINTRESOURCE(ida))) != NULL)
         return hAccel;
      }

   return NULL;
}



