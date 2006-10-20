/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SETTINGS_H
#define SETTINGS_H

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef EXPORTED
#define EXPORTED
#endif

#ifndef HKCR
#define HKCR            HKEY_CLASSES_ROOT
#endif

#ifndef HKCU
#define HKCU            HKEY_CURRENT_USER
#endif

#ifndef HKLM
#define HKLM            HKEY_LOCAL_MACHINE
#endif

#ifndef HIBYTE
#define HIBYTE(_w)      (BYTE)(((_w) >> 8) & 0x00FF)
#endif

#ifndef LOBYTE
#define LOBYTE(_w)      (BYTE)((_w) & 0x00FF)
#endif

#ifndef MAKEVERSION
#define MAKEVERSION(_h,_l)  ( ((((WORD)(_h)) << 8) & 0xFF00) | (((WORD)(_l)) & 0xFF) )
#endif



/*
 * PROTOTYPES _________________________________________________________________
 *
 * Summary:
 *
 *    Store/RestoreSettings() are pretty simple--they're just convenience
 *    wrappers for providing version-controlled storage of a structure in
 *    the registry. You provide the structure and supply a version number;
 *    when your program loads, call RestoreSettings() to see if you've saved
 *    anything previously.  When your program exits, call StoreSettings()
 *    so you don't lose anything--effectively, you'll have global variables
 *    that won't change from one run of your program to the next.  Again,
 *    this is really pretty elementary stuff, but I find I write it a lot
 *    so I stuck it in a common source file.
 *
 *
 * About version numbers:
 *
 *    Don't just pick them at random--there's actually an algorithm here.
 *    A typical version number is composed of two parts--a major and a
 *    minor component.  Start with a version of 1.0, just for grins.
 *    Whenever you *add* something to the structure, increment the minor
 *    version--and make sure you add to the end of the structure.  If you
 *    have to do anything else to the structure--get rid of an element,
 *    reorder things, change the size of an element--increment the major
 *    version number.
 *
 *    Why?  Because it provides backward compatibility.  StoreSettings()
 *    always writes out the version number that you provide it... but
 *    RestoreSettings() is picky about what versions it will load:
 *
 *       (1) Restore fails if the stored major version != the expected version
 *       (2) Restore fails if the stored minor version < the expected version
 *
 *    Another way of looking at that is,
 *
 *       (1) Restore succeeds only if the major versions are identical
 *       (2) Restore succeeds only if the stored version is higher or equal
 *
 *    So if you run a 1.4 program and it tries to read a 1.7 level stored
 *    structure, it will succeed!!  Why?  Because a 1.4 structure is just
 *    like a 1.7 structure--only missing some stuff at the end, which
 *    RestoreSettings will just ignore (the 1.4 program didn't know about
 *    it anyway, and obviously did just fine without it).
 *
 *
 * Examples:
 *
 *    struct {
 *       RECT rWindow;
 *       TCHAR szLastDirectory[ MAX_PATH ];
 *       TCHAR szUserName[ MAX_PATH ];
 *    } globals_restored;
 *
 *    #define wVerGLOBALS MAKEVERSION(1,0)  // major version 1, minor version 0
 *
 *    #define cszPathGLOBALS  TEXT("Software\\MyCompany\\MyProgram")
 *    #define cszValueGLOBALS TEXT("Settings")
 *
 *    ...
 *
 *    if (!RestoreSettings (HKLM, cszPathGLOBALS, cszValueGLOBALS,
 *                          &globals_restored, sizeof(globals_restored),
 *                          wVerGLOBALS))
 *       {
 *       memset (&globals_restored, 0x00, sizeof(globals_restored));
 *       lstrcpy (globals_restored.szUserName, TEXT("unknown"));
 *       // set any other default, first-time-run values here
 *       }
 *
 *    ...
 *
 *    StoreSettings (HKLM, cszPathGLOBALS, cszValueGLOBALS,
 *                   &globals_restored, sizeof(globals_restored),
 *                   wVerGLOBALS);
 *
 */

EXPORTED void EraseSettings (HKEY hkParent,
                             LPCTSTR pszBase,
                             LPCTSTR pszValue);

EXPORTED BOOL RestoreSettings (HKEY hkParent,
                               LPCTSTR pszBase,
                               LPCTSTR pszValue,
                               PVOID pStructure,
                               size_t cbStructure,
                               WORD wVerExpected);

EXPORTED BOOL StoreSettings   (HKEY hkParent,
                               LPCTSTR pszBase,
                               LPCTSTR pszValue,
                               PVOID pStructure,
                               size_t cbStructure,
                               WORD wVersion);

EXPORTED size_t GetRegValueSize (HKEY hk,
                                 LPCTSTR pszBase,
                                 LPCTSTR pszValue);

EXPORTED BOOL GetBinaryRegValue (HKEY hk,
                                 LPCTSTR pszBase,
                                 LPCTSTR pszValue,
                                 PVOID pData,
                                 size_t cbData);

EXPORTED BOOL SetBinaryRegValue (HKEY hk,
                                 LPCTSTR pszBase,
                                 LPCTSTR pszValue,
                                 PVOID pData,
                                 size_t cbData);

EXPORTED BOOL RegDeltreeKey (HKEY hk, LPTSTR pszKey);


#endif

