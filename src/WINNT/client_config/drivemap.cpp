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
#include <rx/rxkad.h>
}
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <WINNT/TaLocale.h>
#include "drivemap.h"
#include <time.h>
#include <adssts.h>
#include <osilog.h>

/*
 * REGISTRY ___________________________________________________________________
 *
 */

#undef AFSConfigKeyName
const TCHAR sAFSConfigKeyName[] = TEXT("SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters");


/*
 * PROFILE SECTIONS ___________________________________________________________
 *
 */

#define cREALLOC_SUBMOUNTS   4

static TCHAR cszINIFILE[] = TEXT("afsdsbmt.ini");
static TCHAR cszSECTION_SUBMOUNTS[] = TEXT("AFS Submounts");
static TCHAR cszSECTION_MAPPINGS[] = TEXT("AFS Mappings");

static TCHAR cszAUTOSUBMOUNT[] = TEXT("Auto");
static TCHAR cszLANMANDEVICE[] = TEXT("\\Device\\LanmanRedirector\\");


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
   *pszName = TEXT('\0');

   if (IsWindowsNT())
      {
      HKEY hk;
      if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Control\\ComputerName\\ComputerName"), &hk) == 0)
         {
         DWORD dwSize = MAX_PATH;
         DWORD dwType = REG_SZ;
         RegQueryValueEx (hk, TEXT("ComputerName"), NULL, &dwType, (PBYTE)pszName, &dwSize);
         }
      }
   else // (!IsWindowsNT())
      {
      HKEY hk;
      if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters"), &hk) == 0)
         {
         DWORD dwSize = MAX_PATH;
         DWORD dwType = REG_SZ;
         RegQueryValueEx (hk, TEXT("Gateway"), NULL, &dwType, (PBYTE)pszName, &dwSize);
         }
      }

   // Shorten the server name from its FQDN
   //
   for (LPTSTR pch = pszName; *pch; ++pch)
      {
      if (*pch == TEXT('.'))
         {
         *(LPTSTR)pch = TEXT('\0');
         break;
         }
      }

   // Form NetBIOS name from client's (possibly truncated) simple host name.
   if (*pszName != TEXT('\0')) {
       pszName[11] = TEXT('\0');
       lstrcat(pszName, TEXT("-AFS"));
   }
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
      lstrcpy (pszPath, TEXT("/afs"));
      return TRUE;
      }

   // Otherwise, look up our list of submounts.
   //
   for (size_t ii = 0; ii < pList->cSubmounts; ++ii)
      {
      if (!lstrcmpi (pList->aSubmounts[ii].szSubmount, pszSubmount))
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
      size_t cchLHS = 1024;
      LPTSTR mszLHS = AllocateStringMemory (cchLHS);

      for (int iRetry = 0; iRetry < 5; ++iRetry)
         {
         DWORD rc = GetPrivateProfileString (cszSECTION_SUBMOUNTS, NULL, TEXT(""), mszLHS, cchLHS, cszINIFILE);
         if ((rc != cchLHS-1) && (rc != cchLHS-2))
            break;

         FreeStringMemory (mszLHS);
         cchLHS *= 2;
         mszLHS = AllocateStringMemory (cchLHS);
         }

      for (LPTSTR psz = mszLHS; psz && *psz; psz += 1+lstrlen(psz))
         {
         SUBMOUNT Submount;
         memset (&Submount, 0x00, sizeof(SUBMOUNT));
         lstrcpy (Submount.szSubmount, psz);

         TCHAR szMapping[ MAX_PATH ] = TEXT("");
         GetPrivateProfileString (cszSECTION_SUBMOUNTS, Submount.szSubmount, TEXT(""), szMapping, MAX_PATH, cszINIFILE);
         if (szMapping[0] != TEXT('\0'))
            {
            AdjustAfsPath (Submount.szMapping, szMapping, FALSE, TRUE);

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

      FreeStringMemory (mszLHS);
      }
}


void QueryDriveMapList_ReadMappings (PDRIVEMAPLIST pList)
{
   size_t cchLHS = 1024;
   LPTSTR mszLHS = AllocateStringMemory (cchLHS);

   for (int iRetry = 0; iRetry < 5; ++iRetry)
      {
      DWORD rc = GetPrivateProfileString (cszSECTION_MAPPINGS, NULL, TEXT(""), mszLHS, cchLHS, cszINIFILE);
      if ((rc != cchLHS-1) && (rc != cchLHS-2))
         break;

      FreeStringMemory (mszLHS);
      cchLHS *= 2;
      mszLHS = AllocateStringMemory (cchLHS);
      }

   for (LPTSTR psz = mszLHS; psz && *psz; psz += 1+lstrlen(psz))
      {
      DRIVEMAP DriveMap;
      memset (&DriveMap, 0x00, sizeof(DRIVEMAP));
      DriveMap.chDrive = toupper(*psz);
      DriveMap.fPersistent = TRUE;
      if ((DriveMap.chDrive < chDRIVE_A) || (DriveMap.chDrive > chDRIVE_Z))
         continue;

      TCHAR szMapping[ MAX_PATH ] = TEXT("");
      GetPrivateProfileString (cszSECTION_MAPPINGS, psz, TEXT(""), szMapping, MAX_PATH, cszINIFILE);
      if (szMapping[0] != TEXT('\0'))
         {
         AdjustAfsPath (DriveMap.szMapping, szMapping, TRUE, TRUE);
         if (DriveMap.szMapping[ lstrlen(DriveMap.szMapping)-1 ] == TEXT('*'))
            {
            DriveMap.fPersistent = FALSE;
            DriveMap.szMapping[ lstrlen(DriveMap.szMapping)-1 ] = TEXT('\0');
            }
         size_t iDrive = DriveMap.chDrive - chDRIVE_A;
         memcpy (&pList->aDriveMap[ iDrive ], &DriveMap, sizeof(DRIVEMAP));
         }
      }

   FreeStringMemory (mszLHS);
}


void QueryDriveMapList_WriteMappings (PDRIVEMAPLIST pList)
{
   WriteDriveMappings (pList);
}


void WriteDriveMappings (PDRIVEMAPLIST pList)
{
   WritePrivateProfileString (cszSECTION_MAPPINGS, NULL, NULL, cszINIFILE);

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

         WritePrivateProfileString (cszSECTION_MAPPINGS, szLHS, szRHS, cszINIFILE);
         }
      }
}

BOOL DriveIsGlobalAfsDrive(TCHAR chDrive)
{
   TCHAR szKeyName[128];
   TCHAR szValueName[128];
   TCHAR szValue[128];
   HKEY hKey;

   _stprintf(szKeyName, TEXT("%s\\GlobalAutoMapper"), sAFSConfigKeyName);

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

   ULONG status;
   if ((status = pioctl (0, VIOC_MAKESUBMOUNT, &IOInfo, 1)) != 0)
      return FALSE;

   lstrcpy (pszSubmount, (LPCTSTR)OutData);
   return (pszSubmount[0] != TEXT('\0')) ? TRUE : FALSE;
}


BOOL ActivateDriveMap (TCHAR chDrive, LPTSTR pszMapping, LPTSTR pszSubmountReq, BOOL fPersistent, DWORD *pdwStatus)
{
   // We can only map drives to places in AFS using this function.
   //
   if ( (lstrncmpi (pszMapping, TEXT("/afs"), lstrlen(TEXT("/afs")))) &&
        (lstrncmpi (pszMapping, TEXT("\\afs"), lstrlen(TEXT("\\afs")))) )
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
   //
   TCHAR szClient[ MAX_PATH ];
   GetClientNetbiosName (szClient);

   TCHAR szLocal[ MAX_PATH ] = TEXT("*:");
   szLocal[0] = chDrive;

   TCHAR szRemote[ MAX_PATH ];
   wsprintf (szRemote, TEXT("\\\\%s\\%s"), szClient, szSubmount);

   NETRESOURCE Resource;
   memset (&Resource, 0x00, sizeof(NETRESOURCE));
   Resource.dwScope = RESOURCE_GLOBALNET;
   Resource.dwType = RESOURCETYPE_DISK;
   Resource.dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
   Resource.dwUsage = RESOURCEUSAGE_CONNECTABLE;
   Resource.lpLocalName = szLocal;
   Resource.lpRemoteName = szRemote;

   DWORD rc = WNetAddConnection2 (&Resource, NULL, NULL, ((fPersistent) ? CONNECT_UPDATE_PROFILE : 0));
   if (rc == NO_ERROR)
      return TRUE;

   if (pdwStatus)
      *pdwStatus = rc;
   return FALSE;
}


BOOL InactivateDriveMap (TCHAR chDrive, DWORD *pdwStatus)
{
   TCHAR szLocal[ MAX_PATH ] = TEXT("*:");
   szLocal[0] = chDrive;

   DWORD rc = WNetCancelConnection (szLocal, FALSE);
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
   WritePrivateProfileString (cszSECTION_SUBMOUNTS, pszSubmount, szRHS, cszINIFILE);
}


void RemoveSubMount (LPTSTR pszSubmount)
{
   WritePrivateProfileString (cszSECTION_SUBMOUNTS, pszSubmount, NULL, cszINIFILE);
}


void AdjustAfsPath (LPTSTR pszTarget, LPCTSTR pszSource, BOOL fWantAFS, BOOL fWantForwardSlashes)
{
   if (!*pszSource)
      lstrcpy (pszTarget, (fWantAFS) ? TEXT("/afs") : TEXT(""));
   else if ((*pszSource != TEXT('/')) && (*pszSource != TEXT('\\')))
      wsprintf (pszTarget, TEXT("/afs/%s"), pszSource);
   // We don't want to strip afs off the start if it is part of something for example afscell.company.com
   else if (fWantAFS && (lstrncmpi (&pszSource[1], TEXT("afs"), 3)) || !((pszSource[4] == TEXT('/')) ||
                                                                         (pszSource[4] == TEXT('\\')) ||
                                                                         (lstrlen(pszSource) == 4)))
      wsprintf (pszTarget, TEXT("/afs%s"), pszSource);
   else if (!fWantAFS && (!lstrncmpi (&pszSource[1], TEXT("afs"), 3) && ((pszSource[4] == TEXT('/')) ||
                                                                        (pszSource[4] == TEXT('\\')) ||
                                                                        (lstrlen(pszSource) == 4))))
      lstrcpy (pszTarget, &pszSource[4]);
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
   TCHAR szDrive[] = TEXT("*:");
   szDrive[0] = chDrive;

   TCHAR szMapping[ MAX_PATH ] = TEXT("");
   LPTSTR pszSubmount = szMapping;

   if (IsWindowsNT())
      {
      QueryDosDevice (szDrive, szMapping, MAX_PATH);

      // Now if this is an AFS network drive mapping, {szMapping} will be:
      //
      //   \Device\LanmanRedirector\Q:\machine-afs\submount
      //
      // on Windows NT. On Windows 2000, it will be:
      //
      //   \Device\LanmanRedirector\;Q:0\machine-afs\submount
      //
      // (This is presumably to support multiple drive mappings with
      // Terminal Server).
      //
      if (lstrncmpi (szMapping, cszLANMANDEVICE, lstrlen(cszLANMANDEVICE)))
         return FALSE;
      pszSubmount = &szMapping[ lstrlen(cszLANMANDEVICE) ];

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

      if (IsWindows2000())
          if (*(++pszSubmount) != TEXT('0'))
             return FALSE;

      if (*(++pszSubmount) != TEXT('\\'))
         return FALSE;
      for (++pszSubmount; *pszSubmount && (*pszSubmount != TEXT('\\')); ++pszSubmount)
         if (!lstrncmpi (pszSubmount, TEXT("-afs\\"), lstrlen(TEXT("-afs\\"))))
            break;
      if ((!*pszSubmount) || (*pszSubmount == TEXT('\\')))
         return FALSE;
      pszSubmount += lstrlen("-afs\\");
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
         if (!lstrncmpi (pszSubmount, TEXT("-afs\\"), lstrlen(TEXT("-afs\\"))))
            break;
      if ((!*pszSubmount) || (*pszSubmount == TEXT('\\')))
         return FALSE;
      pszSubmount += lstrlen("-afs\\");
      }

   if (!pszSubmount || !*pszSubmount)
      return FALSE;

   lstrcpy (pszSubmountNow, pszSubmount);
   return TRUE;
}

/* Generate Random User name random acording to time*/
DWORD dwOldState=0;
TCHAR pUserName[MAXRANDOMNAMELEN];
BOOL fUserName=FALSE;
#define AFSLogonOptionName TEXT("System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\NetworkProvider")

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
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSLogonOptionName,0, KEY_QUERY_VALUE, &hk)==ERROR_SUCCESS)
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
	if ((dwState!=SERVICE_RUNNING) || (dwOldState!=SERVICE_START_PENDING) 
		|| (!RWLogonOption(TRUE,LOGON_OPTION_HIGHSECURITY)))
	{
		dwOldState=dwState;
		return TRUE;
	}
	dwOldState=SERVICE_RUNNING;
	return DoMapShare();
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
	TCHAR szMachine[ MAX_PATH],szPath[MAX_PATH];
	DWORD rc=28;
	HANDLE hEnum;
	LPNETRESOURCE lpnrLocal,lpnr=NULL;
	DWORD res;
	DWORD cbBuffer=16384;
	DWORD cEntries=-1;
	GetComputerName(szMachine,&rc);
	CHAR *pSubmount="";
   // Initialize the data structure
	if ((res=WNetOpenEnum(RESOURCE_CONNECTED,RESOURCETYPE_DISK,RESOURCEUSAGE_CONNECTABLE,lpnr,&hEnum))!=NO_ERROR)
		return;
	sprintf(szPath,"\\\\%s-afs\\",szMachine);
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
						if (drivemap)
							WNetCancelConnection(lpnrLocal[i].lpLocalName,TRUE);
					} else
						WNetCancelConnection(lpnrLocal[i].lpRemoteName,TRUE);
					DEBUG_EVENT1("AFS DriveUnMap","UnMap-Remote=%x",res);
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
	DWORD cbBuffer=16384;
	DWORD cEntries=-1;
	GetComputerName(szMachine,&rc);
	CHAR szUser[MAXRANDOMNAMELEN];
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
	sprintf(szPath,"\\\\%s-afs\\",szMachine);
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
				if (strcmpi(pSubmount,"all")==0) 
					continue;				// do not remove 'all'
				for (DWORD j=0;j<List.cSubmounts;j++)
				{
					if (
						(List.aSubmounts[j].szSubmount[0]) &&
						(strcmpi(List.aSubmounts[j].szSubmount,pSubmount)==0)
						) 
					{
						List.aSubmounts[j].fInUse=TRUE; 
						goto nextname;
					}
				}
				// wasn't on list so lets remove
				sprintf(szPath,"\\\\%s-afs\\%s",szMachine,pSubmount);
				WNetCancelConnection(szPath,TRUE);
				nextname:;
			}
		}
	} while (res!=ERROR_NO_MORE_ITEMS);
	GlobalFree((HGLOBAL)lpnrLocal);
	WNetCloseEnum(hEnum);
	sprintf(szPath,"\\\\%s-afs\\all",szMachine);
	cbBuffer=MAXRANDOMNAMELEN-1;
	// Lets connect all submounts that weren't connectd
	CHAR * pUser=szUser;
	if (WNetGetUser(szPath,(LPSTR)szUser,&cbBuffer)!=NO_ERROR)
		GenRandomName(szUser,MAXRANDOMNAMELEN-1);
	else {
		if ((pUser=strchr(szUser,'\\'))==NULL)
			return FALSE;
		pUser++;
	}
	for (DWORD j=0;j<List.cSubmounts;j++)
	{
		if (List.aSubmounts[j].fInUse)
			continue;
		sprintf(szPath,"\\\\%s-afs\\%s",szMachine,List.aSubmounts[j].szSubmount);
		NETRESOURCE nr;
		memset (&nr, 0x00, sizeof(NETRESOURCE));
		nr.dwType=RESOURCETYPE_DISK;
		nr.lpLocalName="";
		nr.lpRemoteName=szPath;
		DWORD res=WNetAddConnection2(&nr,NULL,pUser,0);

	}
	return TRUE;
}

BOOL DoMapShare()
{
	DRIVEMAPLIST List;
	TCHAR szMachine[ MAX_PATH ];
	TCHAR szPath[ MAX_PATH ];
	DWORD rc=28;
	BOOL bMappedAll=FALSE;
	GetComputerName(szMachine,&rc);
   // Initialize the data structure
	DEBUG_EVENT0("AFS DoMapShare");
	QueryDriveMapList (&List);
	DoUnMapShare(TRUE);
	// All connections have been removed
	// Lets restore them after making the connection from the random name

	GenRandomName(pUserName,MAXRANDOMNAMELEN-1);
	for (DWORD i=0;i<List.cSubmounts;i++)
	{
		if (List.aSubmounts[i].szSubmount[0])
		{
			sprintf(szPath,"\\\\%s-afs\\%s",szMachine,List.aSubmounts[i].szSubmount);
			NETRESOURCE nr;
			memset (&nr, 0x00, sizeof(NETRESOURCE));
			nr.dwType=RESOURCETYPE_DISK;
			nr.lpLocalName="";
			nr.lpRemoteName=szPath;
			DWORD res=WNetAddConnection2(&nr,NULL,pUserName,0);
			DEBUG_EVENT2("AFS DriveMap","Remote[%s]=%x",szPath,res);
			if (strcmpi("all",List.aSubmounts[i].szSubmount)==0)
				bMappedAll=TRUE;
		}
	}
	if (!bMappedAll)	//make sure all is mapped also
	{
			sprintf(szPath,"\\\\%s-afs\\all",szMachine);
			NETRESOURCE nr;
			memset (&nr, 0x00, sizeof(NETRESOURCE));
			nr.dwType=RESOURCETYPE_DISK;
			nr.lpLocalName="";
			nr.lpRemoteName=szPath;
			DWORD res=WNetAddConnection2(&nr,NULL,pUserName,0);
			DEBUG_EVENT2("AFS DriveMap","Remote[%s]=%x",szPath,res);
			if (res==ERROR_SESSION_CREDENTIAL_CONFLICT)
			{
				WNetCancelConnection(szPath,TRUE);
				WNetAddConnection2(&nr,NULL,pUserName,0);
			}
	}
	for (TCHAR chDrive = chDRIVE_A; chDrive <= chDRIVE_Z; ++chDrive)
	{
		TCHAR szRemote[3];
		if (List.aDriveMap[chDrive-chDRIVE_A].fActive)
		{
			sprintf(szRemote,"%c:",chDrive);
			sprintf(szPath,"\\\\%s-afs\\%s",szMachine,List.aDriveMap[chDrive-chDRIVE_A].szSubmount);
			NETRESOURCE nr;
			memset (&nr, 0x00, sizeof(NETRESOURCE));
			nr.dwType=RESOURCETYPE_DISK;
			nr.lpLocalName=szRemote;
			nr.lpRemoteName=szPath;
			DWORD res=WNetAddConnection2(&nr,NULL,NULL,(List.aDriveMap[chDrive-chDRIVE_A].fPersistent)?CONNECT_UPDATE_PROFILE:0);
			DEBUG_EVENT3("AFS DriveMap","Persistant[%d] Remote[%s]=%x",List.aDriveMap[chDrive-chDRIVE_A].fPersistent,szPath,res);
		}
	}
	return TRUE;
}
