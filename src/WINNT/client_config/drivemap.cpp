
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <WINNT/TaLocale.h>
#include "drivemap.h"


/*
 * REGISTRY ___________________________________________________________________
 *
 */

static const TCHAR AFSConfigKeyName[] = TEXT("SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters");


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

   _stprintf(szKeyName, TEXT("%s\\GlobalAutoMapper"), AFSConfigKeyName);

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
   else if (fWantAFS && lstrncmpi (&pszSource[1], TEXT("afs"), 3))
      wsprintf (pszTarget, TEXT("/afs%s"), pszSource);
   else if (!fWantAFS && !lstrncmpi (&pszSource[1], TEXT("afs"), 3))
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
      if (lstrncmpi (szMapping, cszLANMANDEVICE, lstrlen(cszLANMANDEVICE)))
         return FALSE;
      pszSubmount = &szMapping[ lstrlen(cszLANMANDEVICE) ];
      if (toupper(*pszSubmount) != chDrive)
         return FALSE;
      if (*(++pszSubmount) != TEXT(':'))
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

