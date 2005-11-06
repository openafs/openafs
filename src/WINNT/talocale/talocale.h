/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TALOCALE_H
#define TALOCALE_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#endif
#if defined(_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>

typedef struct
   {
   WORD cchString;
   WCHAR achString[1]; // contains {cchString} members
   } STRINGTEMPLATE, *LPSTRINGTEMPLATE;
typedef const STRINGTEMPLATE *LPCSTRINGTEMPLATE;

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

#include <WINNT/tal_string.h>
#include <WINNT/tal_dialog.h>
#include <WINNT/tal_alloc.h>

#ifndef EXPORTED
#define EXPORTED __declspec(dllexport)
#endif

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) TaLocaleReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
extern EXPORTED BOOL TaLocaleReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc);
#endif


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

#define MODULE_PRIORITY_HIGHEST   1	// Search this module first
#define MODULE_PRIORITY_BOOSTED  40	// Search this module fairly early
#define MODULE_PRIORITY_NORMAL   50	// Search this module any time
#define MODULE_PRIORITY_LOWEST  100	// Search this module last
#define MODULE_PRIORITY_REMOVE    0	// Never search this module again

extern EXPORTED void TaLocale_SpecifyModule (HINSTANCE hInstance = NULL, WORD wSearchPriority = MODULE_PRIORITY_NORMAL);


/*
 *** TaLocale_LoadCorrespondingModule
 *
 * This routine looks for a .DLL named after the specified module, but
 * with a suffix reflecting the current locale--it loads that library
 * and adds it to the active-resources chain (via TaLocale_SpecifyModule()).
 * The .DLL should ideally live in the same directory as the specified
 * module; if not, it should at least be on the path.
 *
 */

extern EXPORTED HINSTANCE TaLocale_LoadCorrespondingModule (HINSTANCE hInstance = NULL, WORD wSearchPriority = MODULE_PRIORITY_BOOSTED);

extern EXPORTED HINSTANCE TaLocale_LoadCorrespondingModuleByName (HINSTANCE hInstance, LPTSTR pszFilename, WORD wSearchPriority = MODULE_PRIORITY_BOOSTED);


/*
 *** TaLocale_EnumModule
 *
 * Enables enumeration of each of the product modules which will be seached
 * by TaLocale for resources. Use TaLocale_SpecifyModule to modify this list.
 * Modules will be returned in priority order, with the highest-priority
 * modules being searched first.
 *
 */

extern EXPORTED BOOL TaLocale_EnumModule (size_t iModule, HINSTANCE *phInstance = NULL, WORD *pwSearchPriority = NULL);


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

extern EXPORTED LANGID TaLocale_GetLanguage (void);
extern EXPORTED void TaLocale_SetLanguage (LANGID lang);


/*
 *** TaLocale_GetLanguageOverride
 *** TaLocale_SetLanguageOverride
 *** TaLocale_RemoveLanguageOverride
 *
 * Allows specification of a persistent default language for resources
 * extracted by the functions exported by tal_string.h and tal_dialog.h.
 * If a language override (which is really just a registry entry) exists,
 * all TaLocale-based applications will default to that locale when first
 * run.
 *
 */

extern EXPORTED LANGID TaLocale_GetLanguageOverride (void);
extern EXPORTED void TaLocale_SetLanguageOverride (LANGID lang);
extern EXPORTED void TaLocale_RemoveLanguageOverride (void);


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

extern EXPORTED LPCVOID TaLocale_GetResource (LPCTSTR pszType, LPCTSTR pszRes, LANGID lang = LANG_USER_DEFAULT, HINSTANCE *phInstFound = NULL);


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

extern EXPORTED LPCSTRINGTEMPLATE TaLocale_GetStringResource (int ids, HINSTANCE *phInstFound = NULL);
extern EXPORTED LPCDLGTEMPLATE TaLocale_GetDialogResource (int idd, HINSTANCE *phInstFound = NULL);


/*
 *** TaLocale_LoadMenu
 *** TaLocale_LoadImage
 *** TaLocale_LoadIcon
 *** TaLocale_LoadAccelerators
 *
 * Replacements for Win32 functions. By using these functions instead, the
 * caller can load the appropriate resources regardless of the module in
 * which they reside, or the language which is required.
 *
 */

extern EXPORTED HMENU TaLocale_LoadMenu (int idm);
extern EXPORTED HANDLE TaLocale_LoadImage (int idi, UINT imageType, int cx, int cy, UINT imageFlags);
extern EXPORTED HICON TaLocale_LoadIcon (int idi);
extern EXPORTED HACCEL TaLocale_LoadAccelerators (int ida);


#endif

