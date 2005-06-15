/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
/* copyright (c) 2005
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any purpose,
 * so long as no fee is charged, and so long as the copyright notice
 * above, this grant of permission, and the disclaimer below appear
 * in all copies made; and so long as the name of the university of
 * michigan is not used in any advertising or publicity pertaining
 * to the use or distribution of this software without specific, written
 * prior authorization.
 * 
 * this software is provided as is, without representation from the
 * university of michigan as to its fitness for any purpose, and without
 * warranty by the university of michigan of any kind, either express
 * or implied, including without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  the regents
 * of the university of michigan shall not be liable for nay damages,
 * including special, indirect, incidental, or consequential damages,
 * with respect to ant claim arising out of or in connection with the
 * use of the software, even if it has been or is hereafter advised
 * of the possibility of such damages.
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
#include <rx/rxkad.h>
#include <afs/fs_utils.h>
}
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <WINNT/TaLocale.h>
#include <WINNT/afsreg.h>
#undef REALLOC
#include "drivemap.h"
#include <time.h>
#include <adssts.h>
#ifdef DEBUG
#define DEBUG_VERBOSE
#endif
#include <osilog.h>
#include <lanahelper.h>
#include <strsafe.h>

extern void Config_GetLanAdapter (ULONG *pnLanAdapter);

/*
 * REGISTRY ___________________________________________________________________
 *
 */


/*
 * PROFILE SECTIONS ___________________________________________________________
 *
 */

#define cREALLOC_SUBMOUNTS   4

static TCHAR cszSECTION_SUBMOUNTS[] = TEXT(AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts");
static TCHAR cszSECTION_MAPPINGS[]  = TEXT(AFSREG_CLT_OPENAFS_SUBKEY "\\Mappings");
static TCHAR cszSECTION_ACTIVE[]    = TEXT(AFSREG_CLT_OPENAFS_SUBKEY "\\Active Maps");

static TCHAR cszAUTOSUBMOUNT[] = TEXT("Auto");
static TCHAR cszLANMANDEVICE[] = TEXT("\\Device\\LanmanRedirector\\");


static BOOL 
WriteRegistryString(HKEY key, TCHAR * subkey, LPTSTR lhs, LPTSTR rhs)
{
    HKEY hkSub = NULL;
    RegCreateKeyEx( key,
                    subkey,
                    0,
                    NULL,
                    REG_OPTION_NON_VOLATILE,
                    KEY_WRITE,
                    NULL,
                    &hkSub,
                    NULL);

    DWORD status = RegSetValueEx( hkSub, lhs, 0, REG_SZ, (const BYTE *)rhs, strlen(rhs)+1 );

    if ( hkSub )
        RegCloseKey( hkSub );

    return (status == ERROR_SUCCESS);
}

static BOOL 
WriteExpandedRegistryString(HKEY key, TCHAR * subkey, LPTSTR lhs, LPTSTR rhs)
{
    HKEY hkSub = NULL;
    RegCreateKeyEx( key,
                    subkey,
                    0,
                    NULL,
                    REG_OPTION_NON_VOLATILE,
                    KEY_WRITE,
                    NULL,
                    &hkSub,
                    NULL);

    DWORD status = RegSetValueEx( hkSub, lhs, 0, REG_EXPAND_SZ, (const BYTE *)rhs, strlen(rhs)+1 );

    if ( hkSub )
        RegCloseKey( hkSub );

    return (status == ERROR_SUCCESS);
}

static BOOL 
ReadRegistryString(HKEY key, TCHAR * subkey, LPTSTR lhs, LPTSTR rhs, DWORD * size)
{
    HKEY hkSub = NULL;
    RegCreateKeyEx( key,
                    subkey,
                    0,
                    NULL,
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ,
                    NULL,
                    &hkSub,
                    NULL);

    DWORD dwType = 0;
    DWORD localSize = *size;

    DWORD status = RegQueryValueEx( hkSub, lhs, 0, &dwType, (LPBYTE)rhs, &localSize);
    if (status == 0 && dwType == REG_EXPAND_SZ) {
        TCHAR * buf = (TCHAR *)malloc((*size) * sizeof(TCHAR));
        memcpy(buf, rhs, (*size) * sizeof(TCHAR));
        localSize = ExpandEnvironmentStrings(buf, rhs, *size);
        free(buf);
        if ( localSize > *size )
            status = !ERROR_SUCCESS;
    }
    *size = localSize;

    if ( hkSub )
        RegCloseKey( hkSub );

    return (status == ERROR_SUCCESS);
}

static BOOL 
DeleteRegistryString(HKEY key, TCHAR * subkey, LPTSTR lhs)
{
    HKEY hkSub = NULL;
    RegCreateKeyEx( key,
                    subkey,
                    0,
                    NULL,
                    REG_OPTION_NON_VOLATILE,
                    KEY_WRITE,
                    NULL,
                    &hkSub,
                    NULL);

    DWORD status = RegDeleteValue( hkSub, lhs );

    if ( hkSub )
        RegCloseKey( hkSub );

    return (status == ERROR_SUCCESS);
}

/*
 * STRINGS ____________________________________________________________________
 *
 */

static LPTSTR AllocateStringMemory (size_t cch)
{
   LPTSTR psz = (LPTSTR)Allocate (sizeof(TCHAR) * (cch+1));
   memset (psz, 0x00, sizeof(TCHAR) * (cch+1));
   return psz;
}

static void FreeStringMemory (LPTSTR pszString)
{
   Free (pszString);
}

static int lstrncmpi (LPCTSTR pszA, LPCTSTR pszB, size_t cch)
{
   if (!pszA || !pszB)
      {
      return (!pszB) - (!pszA);   // A,!B:1, !A,B:-1, !A,!B:0
      }

   for ( ; cch > 0; cch--, pszA = CharNext(pszA), pszB = CharNext(pszB))
      {
      TCHAR chA = toupper( *pszA );
      TCHAR chB = toupper( *pszB );

      if (!chA || !chB)
         return (!chB) - (!chA);    // A,!B:1, !A,B:-1, !A,!B:0

      if (chA != chB)
         return (int)(chA) - (int)(chB);   // -1:A<B, 0:A==B, 1:A>B
      }

   return 0;  // no differences before told to stop comparing, so A==B
}


/*
 * REALLOC ____________________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) DriveMapReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL DriveMapReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
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
#endif


/*
 * WINDOWS NT STUFF ___________________________________________________________
 *
 */

static BOOL IsWindowsNT (void)
{
   static BOOL fChecked = FALSE;
   static BOOL fIsWinNT = FALSE;

   if (!fChecked)
      {
      fChecked = TRUE;

      OSVERSIONINFO Version;
      memset (&Version, 0x00, sizeof(Version));
      Version.dwOSVersionInfoSize = sizeof(Version);

      if (GetVersionEx (&Version))
         {
         if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT)
            fIsWinNT = TRUE;
         }
      }

   return fIsWinNT;
}

/* Check if the OS is Windows 2000 or higher.
*/
BOOL IsWindows2000 (void)
{
   static BOOL fChecked = FALSE;
   static BOOL fIsWin2K = FALSE;

   if (!fChecked)
      {
      fChecked = TRUE;

      OSVERSIONINFO Version;
      memset (&Version, 0x00, sizeof(Version));
      Version.dwOSVersionInfoSize = sizeof(Version);

      if (GetVersionEx (&Version))
         {
         if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT &&
             Version.dwMajorVersion >= 5)
             fIsWin2K = TRUE;
         }
      }

   return fIsWin2K;
}

/*
 * GENERAL ____________________________________________________________________
 *
 */

void GetClientNetbiosName (LPTSTR pszName)
{
    static TCHAR szNetbiosName[32] = "";

    if ( szNetbiosName[0] == 0 ) {
        lana_GetNetbiosName(szNetbiosName, LANA_NETBIOS_NAME_FULL);
    }
    _tcscpy(pszName, szNetbiosName);
}


BOOL SubmountToPath (PDRIVEMAPLIST pList, LPTSTR pszPath, LPTSTR pszSubmount, BOOL fMarkInUse)
{
   // We can't do this translation unless we're under Windows NT.
   //
   if (!IsWindowsNT())
      return FALSE;

   // \\computer-afs\all always maps to "/afs"
   //
   if (!lstrcmpi (pszSubmount, TEXT("all")))
      {
      lstrcpy (pszPath, cm_slash_mount_root);
      return TRUE;
      }

   // Otherwise, look up our list of submounts.
   //
#ifdef AFSIFS
  AdjustAfsPath (pszPath, pszSubmount, TRUE, TRUE);
#endif
   for (size_t ii = 0; ii < pList->cSubmounts; ++ii)
      {
#ifndef AFSIFS
      if (!lstrcmpi (pList->aSubmounts[ii].szSubmount, pszSubmount))
#else
      if (!lstrcmpi (pList->aSubmounts[ii].szMapping, pszPath))
#endif
         {
         if (fMarkInUse)
            pList->aSubmounts[ii].fInUse = TRUE;
         AdjustAfsPath (pszPath, pList->aSubmounts[ii].szMapping, TRUE, TRUE);
         return TRUE;
         }
      }

   return FALSE;
}


BOOL IsValidSubmountName (LPTSTR pszSubmount)
{
   if (!*pszSubmount)
      return FALSE;
   if (lstrlen (pszSubmount) > 12)
      return FALSE;

   for ( ; *pszSubmount; ++pszSubmount)
   {
       if (!isprint(*pszSubmount))
           return FALSE;
       if (*pszSubmount == TEXT(' '))
           return FALSE;
       if (*pszSubmount == TEXT('/'))
           return FALSE;
       if (*pszSubmount == TEXT('\\'))
           return FALSE;
       if (*pszSubmount == TEXT('\t'))
           return FALSE;
   }

   return TRUE;
}


/*
 * PIOCTL SUPPORT _____________________________________________________________
 *
 */

extern "C" {

#include "../afsd/fs_utils.h"

#define __CM_CONFIG_INTERFACES_ONLY__
#include "../afsd/cm_config.h"

#define __CM_IOCTL_INTERFACES_ONLY__
#include "../afsd/cm_ioctl.h"

} // extern "C"

#define PIOCTL_MAXSIZE     2048


BOOL fCanIssuePIOCTL (void)
{
   if (!IsWindowsNT())
      {
      TCHAR szGateway[ 256 ] = TEXT("");
      GetClientNetbiosName (szGateway);
      return (szGateway[0]) ? TRUE : FALSE;
      }

   SERVICE_STATUS Status;
   memset (&Status, 0x00, sizeof(Status));
   Status.dwCurrentState = SERVICE_STOPPED;

   SC_HANDLE hManager;
   if ((hManager = OpenSCManager (NULL, NULL, GENERIC_READ)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), GENERIC_READ)) != NULL)
         {
         QueryServiceStatus (hService, &Status);
         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   return (Status.dwCurrentState == SERVICE_RUNNING) ? TRUE : FALSE;
}


/*
 * QUERYDRIVEMAPLIST __________________________________________________________
 *
 */

void QueryDriveMapList_ReadSubmounts (PDRIVEMAPLIST pList)
{
    if (IsWindowsNT())
    {
        HKEY hkSubmounts;

        RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                        cszSECTION_SUBMOUNTS,
                        0, 
                        "AFS", 
                        REG_OPTION_NON_VOLATILE,
                        KEY_READ|KEY_QUERY_VALUE,
                        NULL, 
                        &hkSubmounts,
                        NULL );

        DWORD dwSubmounts;
        RegQueryInfoKey( hkSubmounts,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwSubmounts, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );

        for ( DWORD dwIndex = 0; dwIndex < dwSubmounts; dwIndex ++ ) {
            TCHAR submountPath[MAX_PATH] = "";
            DWORD submountPathLen = MAX_PATH;
            TCHAR submountName[MAX_PATH];
            DWORD submountNameLen = MAX_PATH;
            DWORD dwType = 0;

            RegEnumValue( hkSubmounts, dwIndex, submountName, &submountNameLen, NULL,
                          &dwType, (LPBYTE)submountPath, &submountPathLen);

            if (dwType == REG_EXPAND_SZ) {
                char buf[MAX_PATH];
                StringCbCopyA(buf, MAX_PATH, submountPath);
                submountPathLen = ExpandEnvironmentStrings(buf, submountPath, MAX_PATH);
                if (submountPathLen > MAX_PATH)
                    continue;
            }

            SUBMOUNT Submount;
            memset (&Submount, 0x00, sizeof(SUBMOUNT));
            lstrcpy (Submount.szSubmount, submountName);

            if ( submountPath[0] != TEXT('\0') ) {
                AdjustAfsPath (Submount.szMapping, submountPath, FALSE, TRUE);

                for (size_t ii = 0; ii < pList->cSubmounts; ++ii)
                {
                    if (!pList->aSubmounts[ii].szSubmount[0])
                        break;
                }
                if (REALLOC (pList->aSubmounts, pList->cSubmounts, 1+ii, cREALLOC_SUBMOUNTS))
                {
                    memcpy (&pList->aSubmounts[ii], &Submount, sizeof(SUBMOUNT));
                }
            }

        }
        RegCloseKey(hkSubmounts);
    }
}


void QueryDriveMapList_ReadMappings (PDRIVEMAPLIST pList)
{
    HKEY hkMappings;
    RegCreateKeyEx( HKEY_CURRENT_USER,
                    cszSECTION_MAPPINGS,
                    0, 
                    "AFS", 
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ|KEY_QUERY_VALUE,
                    NULL, 
                    &hkMappings,
                    NULL );

    DWORD dwMappings;
    RegQueryInfoKey( hkMappings,
                     NULL,  /* lpClass */
                     NULL,  /* lpcClass */
                     NULL,  /* lpReserved */
                     NULL,  /* lpcSubKeys */
                     NULL,  /* lpcMaxSubKeyLen */
                     NULL,  /* lpcMaxClassLen */
                     &dwMappings, /* lpcValues */
                     NULL,  /* lpcMaxValueNameLen */
                     NULL,  /* lpcMaxValueLen */
                     NULL,  /* lpcbSecurityDescriptor */
                     NULL   /* lpftLastWriteTime */
                     );

    for ( DWORD dwIndex = 0; dwIndex < dwMappings; dwIndex ++ ) {
        TCHAR mapping[MAX_PATH] = "";
        DWORD mappingLen = MAX_PATH;
        TCHAR drive[MAX_PATH];
        DWORD driveLen = MAX_PATH;
        DWORD dwType;

        RegEnumValue( hkMappings, dwIndex, drive, &driveLen, NULL,
                      &dwType, (LPBYTE)mapping, &mappingLen);
        if ( dwType == REG_EXPAND_SZ ) {
            TCHAR buf[MAX_PATH];
            DWORD dummyLen = ExpandEnvironmentStrings(mapping, buf, MAX_PATH);
            if (dummyLen > MAX_PATH)
                continue;
            _tcsncpy(mapping, buf, MAX_PATH);
        }

        DRIVEMAP DriveMap;
        memset (&DriveMap, 0x00, sizeof(DRIVEMAP));
        DriveMap.chDrive = toupper(*drive);
        DriveMap.fPersistent = TRUE;
        if ((DriveMap.chDrive < chDRIVE_A) || (DriveMap.chDrive > chDRIVE_Z))
            continue;

       if (mapping[0] != TEXT('\0'))
       {
           AdjustAfsPath (DriveMap.szMapping, mapping, TRUE, TRUE);
           if (DriveMap.szMapping[ lstrlen(DriveMap.szMapping)-1 ] == TEXT('*'))
           {
               DriveMap.fPersistent = FALSE;
               DriveMap.szMapping[ lstrlen(DriveMap.szMapping)-1 ] = TEXT('\0');
           }
           size_t iDrive = DriveMap.chDrive - chDRIVE_A;
           memcpy (&pList->aDriveMap[ iDrive ], &DriveMap, sizeof(DRIVEMAP));
       }
    }

    RegCloseKey(hkMappings);
}

BOOL ForceMapActive (TCHAR chDrive)
{
    TCHAR szDrive[2];
    TCHAR szActive[32];

    szDrive[0] = chDrive;
    szDrive[1] = 0;

    DWORD len = sizeof(szActive);
    ReadRegistryString( HKEY_CURRENT_USER, cszSECTION_ACTIVE, szDrive, szActive, &len);

    if ( !lstrcmp(szActive,"1") || !lstrcmpi(szActive,"true") || !lstrcmpi(szActive,"on") || !lstrcmpi(szActive,"yes") )
        return TRUE;
    return FALSE;
}


void WriteActiveMap (TCHAR chDrive, BOOL on)
{
    TCHAR szDrive[2];

    szDrive[0] = chDrive;
    szDrive[1] = 0;

    WriteRegistryString(HKEY_CURRENT_USER, cszSECTION_ACTIVE, szDrive, on ? "1" : "0");
}

void QueryDriveMapList_WriteMappings (PDRIVEMAPLIST pList)
{
    WriteDriveMappings (pList);
}


void WriteDriveMappings (PDRIVEMAPLIST pList)
{
    HKEY hkMappings;
    RegCreateKeyEx( HKEY_CURRENT_USER, 
                    cszSECTION_MAPPINGS,
                    0, 
                    "AFS", 
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ|KEY_QUERY_VALUE|KEY_WRITE,
                    NULL, 
                    &hkMappings,
                    NULL );

    DWORD dwMappings;
    RegQueryInfoKey( hkMappings,
                     NULL,  /* lpClass */
                     NULL,  /* lpcClass */
                     NULL,  /* lpReserved */
                     NULL,  /* lpcSubKeys */
                     NULL,  /* lpcMaxSubKeyLen */
                     NULL,  /* lpcMaxClassLen */
                     &dwMappings, /* lpcValues */
                     NULL,  /* lpcMaxValueNameLen */
                     NULL,  /* lpcMaxValueLen */
                     NULL,  /* lpcbSecurityDescriptor */
                     NULL   /* lpftLastWriteTime */
                     );

    if ( dwMappings > 0 ) {
        for ( long dwIndex = dwMappings - 1; dwIndex >= 0; dwIndex-- ) {
            TCHAR drive[MAX_PATH];
            DWORD driveLen = MAX_PATH;

            RegEnumValue( hkMappings, dwIndex, drive, &driveLen, NULL, NULL, NULL, NULL);
            RegDeleteValue( hkMappings, drive );
        }
    }

   for (size_t iDrive = 0; iDrive < 26; ++iDrive)
   {
       if (pList->aDriveMap[iDrive].szMapping[0] != TEXT('\0'))
       {
           TCHAR szLHS[] = TEXT("*");
           szLHS[0] = pList->aDriveMap[iDrive].chDrive;

           TCHAR szRHS[MAX_PATH];
           AdjustAfsPath (szRHS, pList->aDriveMap[iDrive].szMapping, TRUE, TRUE);
           if (!pList->aDriveMap[iDrive].fPersistent)
               lstrcat (szRHS, TEXT("*"));

           RegSetValueEx( hkMappings, szLHS, 0, REG_EXPAND_SZ, (const BYTE *)szRHS, lstrlen(szRHS) + 1);
       }
   }
   RegCloseKey( hkMappings );
}

BOOL DriveIsGlobalAfsDrive(TCHAR chDrive)
{
   TCHAR szKeyName[128];
   TCHAR szValueName[128];
   TCHAR szValue[128];
   HKEY hKey;

   _stprintf(szKeyName, TEXT("%s\\GlobalAutoMapper"), AFSREG_CLT_SVC_PARAM_SUBKEY);

   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
      return FALSE;

   _stprintf(szValueName, TEXT("%c:"), chDrive);

   DWORD dwSize = sizeof(szValue);
   BOOL bIsGlobal = (RegQueryValueEx (hKey, szValueName, NULL, NULL, (PBYTE)szValue, &dwSize) == ERROR_SUCCESS);

   RegCloseKey (hKey);
   
   return bIsGlobal;
}


void QueryDriveMapList_FindNetworkDrives (PDRIVEMAPLIST pList, BOOL *pfFoundNew)
{
   for (TCHAR chDrive = chDRIVE_A; chDrive <= chDRIVE_Z; ++chDrive)
      {
      TCHAR szSubmount[ MAX_PATH ];
      if (!GetDriveSubmount (chDrive, szSubmount))
         continue;

      // We've got a mapping!  Drive {chDrive} is mapped to submount
      // {szSubmount}. See if that submount makes sense.
      //
      if (!IsWindowsNT())
         {
         size_t iDrive = chDrive - chDRIVE_A;
         if (pList->aDriveMap[ iDrive ].szMapping[0] != TEXT('\0'))
            {
            pList->aDriveMap[ iDrive ].fActive = TRUE;
            lstrcpy (pList->aDriveMap[ iDrive ].szSubmount, szSubmount);
            }
         continue;
         }
      else // (IsWindowsNT())
         {
         TCHAR szAfsPath[ MAX_PATH ];
         if (!SubmountToPath (pList, szAfsPath, szSubmount, TRUE))
            continue;

         // Okay, we know that drive {chDrive} is mapped to afs path {szAfsPath}.
         // If this drive is a global afs drive, then reject it.  Otherwise, look 
         // at pList->aDriveMap, to see if this drive mapping is already in our 
         // list. If not, add it and set pfFoundNew.
         //
         if (DriveIsGlobalAfsDrive(chDrive))
            continue;
         
         size_t iDrive = chDrive - chDRIVE_A;
         if (lstrcmpi (pList->aDriveMap[ iDrive ].szMapping, szAfsPath))
            {
            *pfFoundNew = TRUE;
            pList->aDriveMap[ iDrive ].fPersistent = TRUE;
            }
         pList->aDriveMap[ iDrive ].fActive = TRUE;
         pList->aDriveMap[ iDrive ].chDrive = chDrive;
         lstrcpy (pList->aDriveMap[ iDrive ].szSubmount, szSubmount);
         AdjustAfsPath (pList->aDriveMap[ iDrive ].szMapping, szAfsPath, TRUE, TRUE);
         }
      }
}


void QueryDriveMapList (PDRIVEMAPLIST pList)
{
   // Initialize the data structure
   //
   memset (pList, 0x00, sizeof(DRIVEMAPLIST));
   for (size_t ii = 0; ii < 26; ++ii)
      pList->aDriveMap[ii].chDrive = chDRIVE_A + ii;

   // Read the current lists of submounts and drive letter mappings
   //
   QueryDriveMapList_ReadSubmounts (pList);
   QueryDriveMapList_ReadMappings (pList);

   // Look through the current list of network drives, and see if
   // any are currently mapped to AFS. If we find any which are mapped
   // into AFS unexpectedly, we'll have to rewrite the mappings list.
   //
   BOOL fFoundNew = FALSE;
   QueryDriveMapList_FindNetworkDrives (pList, &fFoundNew);

   if (fFoundNew)
      {
      QueryDriveMapList_WriteMappings (pList);
      }
}


void FreeDriveMapList (PDRIVEMAPLIST pList)
{
   if (pList->aSubmounts)
      Free (pList->aSubmounts);
   memset (pList, 0x00, sizeof(DRIVEMAPLIST));
}


BOOL PathToSubmount (LPTSTR pszSubmount, LPTSTR pszMapping, LPTSTR pszSubmountReq, ULONG *pStatus)
{
   if (pszSubmountReq && !IsValidSubmountName (pszSubmountReq))
      pszSubmountReq = NULL;

   TCHAR szAfsPath[ MAX_PATH ];
   AdjustAfsPath (szAfsPath, pszMapping, TRUE, TRUE);

   // Try to ask AFSD for a new submount name.
   //
   if (!fCanIssuePIOCTL())
      return FALSE;

   BYTE InData[ PIOCTL_MAXSIZE ];
   memset (InData, 0x00, sizeof(InData));

   LPTSTR pszInData = (LPTSTR)InData;
   lstrcpy (pszInData, pszMapping);
   pszInData += 1+lstrlen(pszInData);
   if (pszSubmountReq)
      lstrcpy (pszInData, pszSubmountReq);

   BYTE OutData[ PIOCTL_MAXSIZE ];
   memset (OutData, 0x00, sizeof(OutData));

   struct ViceIoctl IOInfo;
   IOInfo.in = (char *)InData;
   IOInfo.in_size = PIOCTL_MAXSIZE;
   IOInfo.out = (char *)OutData;
   IOInfo.out_size = PIOCTL_MAXSIZE;

   ULONG status = pioctl (0, VIOC_MAKESUBMOUNT, &IOInfo, 1);
   if (pStatus)
       *pStatus = status;

   if (status)
      return FALSE;

   lstrcpy (pszSubmount, (LPCTSTR)OutData);
   return (pszSubmount[0] != TEXT('\0')) ? TRUE : FALSE;
}


BOOL ActivateDriveMap (TCHAR chDrive, LPTSTR pszMapping, LPTSTR pszSubmountReq, BOOL fPersistent, DWORD *pdwStatus)
{
   // We can only map drives to places in AFS using this function.
   //
   if ( (lstrncmpi (pszMapping, cm_slash_mount_root, lstrlen(cm_slash_mount_root))) &&
        (lstrncmpi (pszMapping, cm_back_slash_mount_root, lstrlen(cm_back_slash_mount_root))) )
      {
      if (pdwStatus)
         *pdwStatus = ERROR_BAD_NETPATH;
      return FALSE;
      }

   // First we have to translate {pszMapping} into a submount, and if there is
   // no current submount associated with this path, we'll have to make one.
   //
   ULONG status;
   TCHAR szSubmount[ MAX_PATH ];
   if (!PathToSubmount (szSubmount, pszMapping, pszSubmountReq, &status))
      {
      if (pdwStatus)
         *pdwStatus = status;
      return FALSE;
      }

   // We now have a submount name and drive letter--map the network drive.
#ifndef AFSIFS
   DWORD rc=MountDOSDrive(chDrive,szSubmount,fPersistent,NULL);
#else
   DWORD rc=MountDOSDrive(chDrive,/*szSubmount*/pszMapping,fPersistent,NULL);
#endif
   if (rc == NO_ERROR)
      return TRUE;

   if (pdwStatus)
      *pdwStatus = rc;
   return FALSE;
}


BOOL InactivateDriveMap (TCHAR chDrive, DWORD *pdwStatus)
{
    DWORD rc = DisMountDOSDrive(chDrive, FALSE);
    if (rc == NO_ERROR)
        return TRUE;

    if (pdwStatus)
        *pdwStatus = rc;
    return FALSE;
}


void AddSubMount (LPTSTR pszSubmount, LPTSTR pszMapping)
{
    TCHAR szRHS[ MAX_PATH ];
    AdjustAfsPath (szRHS, pszMapping, FALSE, TRUE);
    if (!szRHS[0])
        lstrcpy (szRHS, TEXT("/"));

    WriteExpandedRegistryString(HKEY_LOCAL_MACHINE, cszSECTION_SUBMOUNTS, pszSubmount, szRHS);
}


void RemoveSubMount (LPTSTR pszSubmount)
{
    DeleteRegistryString(HKEY_LOCAL_MACHINE, cszSECTION_SUBMOUNTS, pszSubmount);
}


void AdjustAfsPath (LPTSTR pszTarget, LPCTSTR pszSource, BOOL fWantAFS, BOOL fWantForwardSlashes)
{
    if (!*pszSource)
        lstrcpy (pszTarget, (fWantAFS) ? cm_slash_mount_root : TEXT(""));
    else if ((*pszSource != TEXT('/')) && (*pszSource != TEXT('\\')))
        wsprintf (pszTarget, TEXT("%s/%s"),cm_slash_mount_root, pszSource);
    // We don't want to strip afs off the start if it is part of something for example afscell.company.com
    else if (fWantAFS && (lstrncmpi (&pszSource[1], cm_mount_root, strlen(cm_mount_root))) || !((pszSource[strlen(cm_slash_mount_root)] == TEXT('/')) ||
                                                                                                 (pszSource[strlen(cm_slash_mount_root)] == TEXT('\\')) ||
                                                                                                 (lstrlen(pszSource) == strlen(cm_slash_mount_root))))
        wsprintf (pszTarget, TEXT("%s%s"),cm_slash_mount_root, pszSource);
    else if (!fWantAFS && (!lstrncmpi (&pszSource[1], cm_mount_root, strlen(cm_mount_root)) && ((pszSource[strlen(cm_slash_mount_root)] == TEXT('/')) ||
                                                                                                 (pszSource[strlen(cm_slash_mount_root)] == TEXT('\\')) ||
                                                                                                 (lstrlen(pszSource) == strlen(cm_slash_mount_root)))))
        lstrcpy (pszTarget, &pszSource[strlen(cm_slash_mount_root)]);
    else
        lstrcpy (pszTarget, pszSource);

   for (LPTSTR pch = pszTarget; *pch; ++pch)
      {
      if (fWantForwardSlashes)
         {
         *pch = (*pch == TEXT('\\')) ? TEXT('/') : (*pch);
         }
      else // (!fWantForwardSlashes)
         {
         *pch = (*pch == TEXT('/')) ? TEXT('\\') : (*pch);
         }
      }

   if (lstrlen(pszTarget) &&
       ((pszTarget[lstrlen(pszTarget)-1] == TEXT('/')) ||
        (pszTarget[lstrlen(pszTarget)-1] == TEXT('\\'))))
      {
      pszTarget[lstrlen(pszTarget)-1] = TEXT('\0');
      }
}

BOOL GetDriveSubmount (TCHAR chDrive, LPTSTR pszSubmountNow)
{
	BOOL isWinNT = IsWindowsNT();

	TCHAR szDrive[] = TEXT("*:");
    szDrive[0] = chDrive;

    TCHAR szMapping[ _MAX_PATH ] = TEXT("");

    if (isWinNT && !QueryDosDevice (szDrive, szMapping, MAX_PATH))
           return FALSE;

    LPTSTR pszSubmount = szMapping;
    
	TCHAR szNetBiosName[32];
    memset(szNetBiosName, '\0', sizeof(szNetBiosName));
    GetClientNetbiosName(szNetBiosName);
    _tcscat(szNetBiosName, TEXT("\\"));

   if (isWinNT)
   {
      // Now if this is an AFS network drive mapping, {szMapping} will be:
      //
      //   \Device\LanmanRedirector\<Drive>:\<netbiosname>\submount
      //
      // on Windows NT. On Windows 2000, it will be:
      //
      //   \Device\LanmanRedirector\;<Drive>:0\<netbiosname>\submount
      //
      // (This is presumably to support multiple drive mappings with
      // Terminal Server).
      //
      // on Windows XP and 2003, it will be :
      //   \Device\LanmanRedirector\;<Drive>:<AuthID>\<netbiosname>\submount
      //
      //   where : <Drive> : DOS drive letter
      //           <AuthID>: Authentication ID, 16 char hex.
      //           <netbiosname>: Netbios name of server
      //
#ifndef AFSIFS
      if (_tcsnicmp(szMapping, cszLANMANDEVICE, _tcslen(cszLANMANDEVICE)))
#else
   const TCHAR ker_sub_path[] = "\\Device\\afsrdr\\";
      if (_tcsnicmp(szMapping, ker_sub_path, _tcslen(ker_sub_path)))
#endif
         return FALSE;
#ifndef AFSIFS
      pszSubmount = &szMapping[ _tcslen(cszLANMANDEVICE) ];
#else
      pszSubmount = &szMapping[ _tcslen(ker_sub_path) ];
#endif

#ifdef AFSIFS
		if (*(pszSubmount) < '0' ||
			*(pszSubmount) > '9')
			return FALSE;
		++pszSubmount;
#else
      if (IsWindows2000())
	  {
          if (*(pszSubmount) != TEXT(';'))
             return FALSE;
	  } else 
		--pszSubmount;

      if (toupper(*(++pszSubmount)) != chDrive)
         return FALSE;

      if (*(++pszSubmount) != TEXT(':'))
         return FALSE;

#ifdef COMMENT
       // No longer a safe assumption on XP
      if (IsWindows2000())
          if (*(++pszSubmount) != TEXT('0'))
             return FALSE;
#endif

      // scan for next "\"
      while (*(++pszSubmount) != TEXT('\\'))
      {
	  if (*pszSubmount==0)
              return FALSE;
      }

       // note that szNetBiosName has a '\\' tagged in the end earlier
      for (++pszSubmount; *pszSubmount && (*pszSubmount != TEXT('\\')); ++pszSubmount)
         if (!_tcsncicmp(pszSubmount, szNetBiosName, _tcslen(szNetBiosName)))
            break;
      if ((!*pszSubmount) || (*pszSubmount == TEXT('\\')))
         return FALSE;

       pszSubmount += _tcslen(szNetBiosName);
#endif
      }
   else // (!IsWindowsNT())
      {
      DWORD dwSize = MAX_PATH;
      if (WNetGetConnection (szDrive, szMapping, &dwSize) != NO_ERROR)
         return FALSE;
      if (*(pszSubmount++) != TEXT('\\'))
         return FALSE;
      if (*(pszSubmount++) != TEXT('\\'))
         return FALSE;
      for ( ; *pszSubmount && (*pszSubmount != TEXT('\\')); ++pszSubmount)
         if (!lstrncmpi (pszSubmount, szNetBiosName, lstrlen(szNetBiosName)))
            break;
      if ((!*pszSubmount) || (*pszSubmount == TEXT('\\')))
         return FALSE;
      pszSubmount += lstrlen(szNetBiosName);
      }

   if (!pszSubmount || !*pszSubmount)
      return FALSE;

#ifndef AFSIFS
   lstrcpy (pszSubmountNow, pszSubmount);
#else
   lstrcpy (pszSubmountNow, "\\afs");
   lstrcat (pszSubmountNow, pszSubmount);
#endif
   return TRUE;
}

/* Generate Random User name random acording to time*/
DWORD dwOldState=0;
TCHAR pUserName[MAXRANDOMNAMELEN]=TEXT("");
BOOL fUserName=FALSE;
#define AFSLogonOptionName TEXT(AFSREG_CLT_SVC_PROVIDER_SUBKEY)

void SetBitLogonOption(BOOL set,DWORD value)
{

   RWLogonOption(FALSE,((set)?value | RWLogonOption(TRUE,0):RWLogonOption(TRUE,0) & ~value) );	
}

DWORD RWLogonOption(BOOL read,DWORD value)
{
    // if read is true then if value==0 return registry value
    // if read and value!=0 then use value to test registry, return TRUE if value bits match value read
    HKEY hk;
    DWORD dwDisp;
    DWORD LSPtype, LSPsize;
    DWORD rval;
   
    if (read)
    {
        rval=0;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSLogonOptionName, 0, KEY_QUERY_VALUE, &hk)==ERROR_SUCCESS)
        {
            LSPsize=sizeof(rval);
            RegQueryValueEx(hk, "LogonOptions", NULL,
                             &LSPtype, (LPBYTE)&rval, &LSPsize);
            RegCloseKey (hk);
        }
        return (value==0)?rval:((rval & value)==value);
    } else {	//write
        if (RegCreateKeyEx (HKEY_LOCAL_MACHINE, AFSLogonOptionName, 0, NULL, 0, KEY_SET_VALUE, NULL, &hk, &dwDisp) == ERROR_SUCCESS)
        {
            RegSetValueEx(hk,TEXT("LogonOptions"),NULL,REG_DWORD,(LPBYTE)&value,sizeof(value));
            RegCloseKey (hk);
        }
        return TRUE;
    }    
}

void MapShareName(char *pszCmdLineA)
{
	fUserName = TRUE;
	TCHAR *p=pUserName;
	pszCmdLineA++;
	while (*pszCmdLineA && (*pszCmdLineA != ' '))
	{
	  *p++=*pszCmdLineA++;
	}
}

void GenRandomName(TCHAR *pname,int len)
{
	if (fUserName)
	{		//user name was passed through command line, use once
		fUserName=FALSE;
		return;
	}
	srand( (unsigned)time( NULL ) );
	for (int i=0;i<len;i++)
		pname[i]='a'+(rand() % 26);
	pname[len]=0;
	return;
}

/*
	Make a connection using users name
	if fUserName then force a connection
*/

BOOL TestAndDoMapShare(DWORD dwState)
{
    if ((dwState!=SERVICE_RUNNING) || (dwOldState!=SERVICE_START_PENDING))
	{
		dwOldState=dwState;
		return TRUE;
	}
	dwOldState=SERVICE_RUNNING;
	if (RWLogonOption(TRUE,LOGON_OPTION_HIGHSECURITY))
	    return (DoMapShare() && GlobalMountDrive());
	return GlobalMountDrive();
}

BOOL IsServiceActive()
{
   SC_HANDLE hManager;
   SERVICE_STATUS Status;
   if ((hManager = OpenSCManager (NULL, NULL, GENERIC_READ)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), GENERIC_READ)) != NULL)
         {
         QueryServiceStatus (hService, &Status);
         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   return (Status.dwCurrentState == SERVICE_RUNNING) ? TRUE : FALSE;
}

void TestAndDoUnMapShare()
{
	if (!RWLogonOption(TRUE,LOGON_OPTION_HIGHSECURITY))
		return;
	DoUnMapShare(FALSE);	
}

void DoUnMapShare(BOOL drivemap)	//disconnect drivemap 
{
	TCHAR szMachine[MAX_PATH],szPath[MAX_PATH];
	DWORD rc=28;
	HANDLE hEnum;
	LPNETRESOURCE lpnrLocal,lpnr=NULL;
	DWORD res;
	DWORD cbBuffer=16384;
	DWORD cEntries=-1;
	CHAR *pSubmount="";

    memset(szMachine, '\0', sizeof(szMachine));
    GetClientNetbiosName(szMachine);

   // Initialize the data structure
	if ((res=WNetOpenEnum(RESOURCE_CONNECTED,RESOURCETYPE_DISK,RESOURCEUSAGE_CONNECTABLE,lpnr,&hEnum))!=NO_ERROR)
		return;
	sprintf(szPath,"\\\\%s\\",szMachine);
	_strlwr(szPath);
	lpnrLocal=(LPNETRESOURCE) GlobalAlloc(GPTR,cbBuffer);
	do {
		memset(lpnrLocal,0,cbBuffer);
		if ((res = WNetEnumResource(hEnum,&cEntries,lpnrLocal,&cbBuffer))==NO_ERROR)
		{
			for (DWORD i=0;i<cEntries;i++)
			{
				if (strstr(_strlwr(lpnrLocal[i].lpRemoteName),szPath))
				{
					if ((lpnrLocal[i].lpLocalName) && (strlen(lpnrLocal[i].lpLocalName)>0))
					{
						if (drivemap) {
						    DisMountDOSDrive(*lpnrLocal[i].lpLocalName);
                            DEBUG_EVENT1("AFS DriveUnMap","UnMap-Local=%x",res);
                        }
					} else {
					    DisMountDOSDriveFull(lpnrLocal[i].lpRemoteName);
                        DEBUG_EVENT1("AFS DriveUnMap","UnMap-Remote=%x",res);
                    }
				}
			}
		}
	} while (res!=ERROR_NO_MORE_ITEMS);
	GlobalFree((HGLOBAL)lpnrLocal);
	WNetCloseEnum(hEnum);
}

BOOL DoMapShareChange()
{
	DRIVEMAPLIST List;
	TCHAR szMachine[ MAX_PATH],szPath[MAX_PATH];
	DWORD rc=28;
	HANDLE hEnum;
	LPNETRESOURCE lpnrLocal,lpnr=NULL;
	DWORD res;
	DWORD cEntries=-1;
    DWORD cbBuffer=16384;

    memset(szMachine, '\0', sizeof(szMachine));
    GetClientNetbiosName(szMachine);

    // Initialize the data structure
	if (!IsServiceActive())
		return TRUE;
	memset (&List, 0x00, sizeof(DRIVEMAPLIST));
	for (size_t ii = 0; ii < 26; ++ii)
		List.aDriveMap[ii].chDrive = chDRIVE_A + ii;
	QueryDriveMapList_ReadSubmounts (&List);
	if ((res=WNetOpenEnum(RESOURCE_CONNECTED,RESOURCETYPE_DISK,RESOURCEUSAGE_CONNECTABLE,lpnr,&hEnum))!=NO_ERROR)
		return FALSE;
	lpnrLocal=(LPNETRESOURCE) GlobalAlloc(GPTR,cbBuffer);
	sprintf(szPath,"\\\\%s\\",szMachine);
	_strlwr(szPath);
	do {
		memset(lpnrLocal,0,cbBuffer);
		if ((res = WNetEnumResource(hEnum,&cEntries,lpnrLocal,&cbBuffer))==NO_ERROR)
		{
			for (DWORD i=0;i<cEntries;i++)
			{
				if (strstr(_strlwr(lpnrLocal[i].lpRemoteName),szPath)==NULL)
					continue;	//only look at real afs mappings
				CHAR * pSubmount=strrchr(lpnrLocal[i].lpRemoteName,'\\')+1;
				if (lstrcmpi(pSubmount,"all")==0) 
					continue;				// do not remove 'all'
				for (DWORD j=0;j<List.cSubmounts;j++)
				{
					if (
						(List.aSubmounts[j].szSubmount[0]) &&
						(lstrcmpi(List.aSubmounts[j].szSubmount,pSubmount)==0)
						) 
					{
						List.aSubmounts[j].fInUse=TRUE; 
						goto nextname;
					}
				}
				// wasn't on list so lets remove
				DisMountDOSDrive(pSubmount);
				nextname:;
			}
		}
	} while (res!=ERROR_NO_MORE_ITEMS);
	GlobalFree((HGLOBAL)lpnrLocal);
	WNetCloseEnum(hEnum);
	sprintf(szPath,"\\\\%s\\all",szMachine);

	// Lets connect all submounts that weren't connectd
    DWORD cbUser=MAXRANDOMNAMELEN-1;
	CHAR szUser[MAXRANDOMNAMELEN];
    CHAR * pUser = NULL;
	if (WNetGetUser(szPath,(LPSTR)szUser,&cbUser)!=NO_ERROR) {
        if (RWLogonOption(TRUE,LOGON_OPTION_HIGHSECURITY)) {
            if (!pUserName[0]) {
                GenRandomName(szUser,MAXRANDOMNAMELEN-1);
                pUser = szUser;
            } else {
                pUser = pUserName;
            }
        }
    } else {
		if ((pUser=strchr(szUser,'\\'))!=NULL)
            pUser++;
	}

    for (DWORD j=0;j<List.cSubmounts;j++)
	{
		if (List.aSubmounts[j].fInUse)
			continue;
		DWORD res=MountDOSDrive(0,List.aSubmounts[j].szSubmount,FALSE,pUser);
	}
	return TRUE;
}

BOOL DoMapShare()
{
	DRIVEMAPLIST List;
	DWORD rc=28;
	BOOL bMappedAll=FALSE;

   // Initialize the data structure
	DEBUG_EVENT0("AFS DoMapShare");
	QueryDriveMapList (&List);
	DoUnMapShare(TRUE);
	// All connections have been removed
	// Lets restore them after making the connection from the random name

	TCHAR szMachine[ MAX_PATH],szPath[MAX_PATH];
    memset(szMachine, '\0', sizeof(szMachine));
    GetClientNetbiosName(szMachine);
    sprintf(szPath,"\\\\%s\\all",szMachine);

    // Lets connect all submounts that weren't connectd
    DWORD cbUser=MAXRANDOMNAMELEN-1;
	CHAR szUser[MAXRANDOMNAMELEN];
    CHAR * pUser = NULL;
	if (WNetGetUser(szPath,(LPSTR)szUser,&cbUser)!=NO_ERROR) {
        if (RWLogonOption(TRUE,LOGON_OPTION_HIGHSECURITY)) {
            if (!pUserName[0]) {
                GenRandomName(szUser,MAXRANDOMNAMELEN-1);
                pUser = szUser;
            } else {
                pUser = pUserName;
            }
        }
    } else {
		if ((pUser=strchr(szUser,'\\'))!=NULL)
            pUser++;
	}

	for (DWORD i=0;i<List.cSubmounts;i++)
	{
		if (List.aSubmounts[i].szSubmount[0])
		{
			DWORD res=MountDOSDrive(0,List.aSubmounts[i].szSubmount,FALSE,pUser);
			if (lstrcmpi("all",List.aSubmounts[i].szSubmount)==0)
				bMappedAll=TRUE;
		}
	}
	if (!bMappedAll)	//make sure all is mapped also
	{
        DWORD res=MountDOSDrive(0,"all",FALSE,pUser);
        if (res==ERROR_SESSION_CREDENTIAL_CONFLICT)
        {
            DisMountDOSDrive("all");
            MountDOSDrive(0,"all",FALSE,pUser);
        }
	}
	for (TCHAR chDrive = chDRIVE_A; chDrive <= chDRIVE_Z; ++chDrive)
	{
		if (List.aDriveMap[chDrive-chDRIVE_A].fActive ||
            ForceMapActive(chDrive))
		{
            TCHAR szSubmount[ MAX_PATH ];
            if (List.aDriveMap[chDrive-chDRIVE_A].szSubmount[0])
                lstrcpy(szSubmount,List.aDriveMap[chDrive-chDRIVE_A].szSubmount);
            else if (!PathToSubmount (szSubmount, List.aDriveMap[chDrive-chDRIVE_A].szMapping, NULL, NULL))
                continue;

            BOOL fPersistent = List.aDriveMap[chDrive-chDRIVE_A].fPersistent;
            if (RWLogonOption(TRUE,LOGON_OPTION_HIGHSECURITY))
                fPersistent = FALSE;
		    DWORD res=MountDOSDrive(chDrive
					    ,szSubmount
					    ,fPersistent,pUser);
		}
	}
	return TRUE;
}

BOOL GlobalMountDrive()
{
    char szDriveToMapTo[5];
    DWORD dwResult;
    char szKeyName[256];
    HKEY hKey;
    DWORD dwIndex = 0;
    DWORD dwDriveSize;
    DWORD dwSubMountSize;
    char unsigned szSubMount[256];
    char cm_HostName[200];
    DWORD dwType=sizeof(cm_HostName);
    if (!IsServiceActive())
	return TRUE;
    if (!GetComputerName(cm_HostName, &dwType))
        return TRUE;
    sprintf(szKeyName, "%s\\GlobalAutoMapper", AFSREG_CLT_SVC_PARAM_SUBKEY);
    
    dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_QUERY_VALUE,
			    &hKey);
    if (dwResult != ERROR_SUCCESS)
	return TRUE;
    
    while (1) {
        dwDriveSize = sizeof(szDriveToMapTo);
        dwSubMountSize = sizeof(szSubMount);
        dwResult = RegEnumValue(hKey, dwIndex++, szDriveToMapTo, &dwDriveSize, 
				0, &dwType, szSubMount, &dwSubMountSize);
        if (dwResult != ERROR_MORE_DATA) {
	    if (dwResult != ERROR_SUCCESS) {
		if (dwResult != ERROR_NO_MORE_ITEMS)
		{
		    DEBUG_EVENT1("AFS DriveMap","Failed to read GlobalAutoMapper values: %d",dwResult);
		}
		break;
	    }
	}
	dwResult=MountDOSDrive(*szDriveToMapTo,(const char *)szSubMount,FALSE,NULL);
    }
    RegCloseKey(hKey);
    return TRUE;
}

DWORD MountDOSDrive(char chDrive,const char *szSubmount,BOOL bPersistent,const char * pUsername)
{
    DWORD err;
	BOOL succ;
	TCHAR szPath[MAX_PATH], szTokens[MAX_PATH], *tok;
    TCHAR szClient[MAX_PATH];
    TCHAR szDrive[3] = TEXT("?:");

#ifdef AFSIFS
	int pathCount, currPos, lastPos, x;
    
	pathCount = 0;

	pathCount = 0;
	strcpy(szTokens, szSubmount);
	tok = strtok(szTokens, "/\\");
	strcpy(szPath, "");
	while (tok)
		{
		if (pathCount || stricmp(tok, "afs"))
			{
			strcat(szPath, "\\");
			strcat(szPath, tok);
			pathCount++;
			}
		tok = strtok(NULL, "/\\");
		}

	sprintf(szDrive,"%c:",chDrive);
	strcpy(szTokens, szPath);
	sprintf(szPath,"\\Device\\afsrdr\\%d%s",pathCount,szTokens);
	//succ = DefineDosDevice(DDD_RAW_TARGET_PATH, "J:", "\\Device\\afsrdr\\2\\ericjw\\test");
	succ = DefineDosDevice(DDD_RAW_TARGET_PATH, szDrive, szPath);
	err = GetLastError();

	return succ ? NO_ERROR : ERROR_DEVICE_IN_USE;

#else

    sprintf(szDrive,"%c:",chDrive);
    GetClientNetbiosName (szClient);
    sprintf(szPath,"\\\\%s\\%s",szClient,szSubmount);
    NETRESOURCE nr;
    memset (&nr, 0x00, sizeof(NETRESOURCE));
    nr.dwType=RESOURCETYPE_DISK;
    nr.lpLocalName=szDrive;
    nr.lpRemoteName=szPath;
    nr.dwDisplayType = RESOURCEDISPLAYTYPE_SHARE; /* ignored parameter */
    DWORD res=WNetAddConnection2(&nr,NULL,pUsername,(bPersistent)?CONNECT_UPDATE_PROFILE:0);
    DEBUG_EVENT5("AFS DriveMap","Mount %s Local[%s] Remote[%s] User[%s]=%x",
                  (bPersistent)?"Persistant" : "NonPresistant",
                  szDrive,szPath,pUsername?pUsername:"NULL",res);
    return res;
#endif
}

DWORD DisMountDOSDriveFull(const char *szPath,BOOL bForce)
{
#ifndef AFSIFS
    DWORD res=WNetCancelConnection(szPath,bForce);
#else
    DWORD res;
	res = ERROR_DEVICE_IN_USE;
	// must handle drive letters and afs paths
	//DDD_REMOVE_DEFINITION
#endif
    DEBUG_EVENT3("AFS DriveMap","%sDismount Remote[%s]=%x",
                  bForce ? "Forced " : "",szPath,res);
    return (res==ERROR_NOT_CONNECTED)?NO_ERROR:res;
}

DWORD DisMountDOSDrive(const char *pSubmount,BOOL bForce)
{
    TCHAR szPath[MAX_PATH];
    TCHAR szClient[MAX_PATH];
    GetClientNetbiosName (szClient);
    sprintf(szPath,"\\\\%s\\%s",szClient,pSubmount);
    return DisMountDOSDriveFull(szPath,bForce);
}


DWORD DisMountDOSDrive(const char chDrive,BOOL bForce)
{
    TCHAR szPath[MAX_PATH];
	DWORD succ;

	sprintf(szPath,"%c:",chDrive);
#ifdef AFSIFS
	succ = DefineDosDevice(DDD_REMOVE_DEFINITION, szPath, NULL);
	return (!succ) ? GetLastError() : 0;
#else
    return DisMountDOSDriveFull(szPath,bForce);
#endif
}
