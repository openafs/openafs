/*det
 * Configuration Read/Modify Functions ________________________________________
 *
 * Temporarily these just modify the local Registry.
 * In the near future, they will modify the Registry on the
 * gateway, if a gateway is being used.
 *
 */

#include <windows.h>
#include <tchar.h>
//#include <ctype.h>
#include <stdlib.h>
#include <drivemap.h>
#include <WINNT\afsreg.h>

BOOL Config_ReadString (LPCTSTR pszLHS, LPTSTR pszRHS, size_t cchMax);

/*
 * REGISTRY ___________________________________________________________________
 *
 */

void Config_GetGlobalDriveList (DRIVEMAPLIST *pDriveList)
{
   // Read the GlobalAutoMapper registry key
   TCHAR szDriveToMapTo[5];
   DWORD dwResult;
   TCHAR szKeyName[256];
   HKEY hKey;
   DWORD dwIndex = 0;
   DWORD dwDriveSize;
   DWORD dwSubMountSize;
   TCHAR szSubMount[256];
   DWORD dwType;

   if (!pDriveList)
      return;

   memset(pDriveList, 0, sizeof(DRIVEMAPLIST));

   lstrcpy(szKeyName, TEXT(AFSREG_CLT_SVC_PARAM_SUBKEY));
   lstrcat(szKeyName, TEXT("\\GlobalAutoMapper"));

   dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_QUERY_VALUE, &hKey);
   if (dwResult != ERROR_SUCCESS)
      return;

   // Get the drive map list so we can lookup the paths that go with our submounts
	DRIVEMAPLIST DriveMapList;
   memset(&DriveMapList, 0, sizeof(DRIVEMAPLIST));
   QueryDriveMapList (&DriveMapList);

   while (1) {
      dwDriveSize = sizeof(szDriveToMapTo);
      dwSubMountSize = sizeof(szSubMount);
      
      dwResult = RegEnumValue(hKey, dwIndex++, szDriveToMapTo, &dwDriveSize, 0, &dwType, (BYTE*)szSubMount, &dwSubMountSize);
      if (dwResult != ERROR_SUCCESS)
         break;
      
      szDriveToMapTo[0] = _totupper(szDriveToMapTo[0]);
        
      int nCurDrive = szDriveToMapTo[0] - TEXT('A');
   	
      pDriveList->aDriveMap[nCurDrive].chDrive = szDriveToMapTo[0];
      lstrcpy(pDriveList->aDriveMap[nCurDrive].szSubmount, szSubMount);

      // Find the path that goes with this submount
      SubmountToPath (&DriveMapList, pDriveList->aDriveMap[nCurDrive].szMapping, szSubMount, FALSE);
   }        

   FreeDriveMapList(&DriveMapList);

   RegCloseKey(hKey);
}

BOOL Config_ReadGlobalNum (LPCTSTR pszLHS, DWORD *pdwRHS)
{
   HKEY hk;
   if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_SVC_PARAM_SUBKEY), 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
      return FALSE;

   DWORD dwSize = sizeof(*pdwRHS);
   if (RegQueryValueEx (hk, pszLHS, NULL, NULL, (PBYTE)pdwRHS, &dwSize) != ERROR_SUCCESS)
      {
      RegCloseKey (hk);
      return FALSE;
      }

   RegCloseKey (hk);
   return TRUE;
}


BOOL Config_ReadGlobalString (LPCTSTR pszLHS, LPTSTR pszRHS, size_t cchMax)
{
   HKEY hk;
   if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_SVC_PARAM_SUBKEY), 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
      return FALSE;

   DWORD dwSize = sizeof(TCHAR) * cchMax;
   if (RegQueryValueEx (hk, pszLHS, NULL, NULL, (PBYTE)pszRHS, &dwSize) != ERROR_SUCCESS)
      {
      RegCloseKey (hk);
      return FALSE;
      }

   RegCloseKey (hk);
   return TRUE;
}


void Config_WriteGlobalNum (LPCTSTR pszLHS, DWORD dwRHS)
{
   HKEY hk;
   DWORD dwDisp;
   if (RegCreateKeyEx (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_SVC_PARAM_SUBKEY), 0, TEXT("container"), 0, KEY_SET_VALUE, NULL, &hk, &dwDisp) == ERROR_SUCCESS)
      {
      RegSetValueEx (hk, pszLHS, NULL, REG_DWORD, (PBYTE)&dwRHS, sizeof(dwRHS));
      RegCloseKey (hk);
      }
}


void Config_WriteGlobalString (LPCTSTR pszLHS, LPCTSTR pszRHS)
{
   HKEY hk;
   DWORD dwDisp;
   if (RegCreateKeyEx (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_SVC_PARAM_SUBKEY), 0, TEXT("container"), 0, KEY_SET_VALUE, NULL, &hk, &dwDisp) == ERROR_SUCCESS)
      {
      RegSetValueEx (hk, pszLHS, NULL, REG_SZ, (PBYTE)pszRHS, sizeof(TCHAR) * (1+lstrlen(pszRHS)));
      RegCloseKey (hk);
      }
}


BOOL Config_ReadUserNum (LPCTSTR pszLHS, DWORD *pdwRHS)
{
   HKEY hk;
    if (RegOpenKeyEx (HKEY_CURRENT_USER, TEXT(AFSREG_USER_OPENAFS_SUBKEY), 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
        if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_OPENAFS_SUBKEY), 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
            return FALSE;

   DWORD dwSize = sizeof(*pdwRHS);
   if (RegQueryValueEx (hk, pszLHS, NULL, NULL, (PBYTE)pdwRHS, &dwSize) != ERROR_SUCCESS)
      {
      RegCloseKey (hk);
      return FALSE;
      }

   RegCloseKey (hk);
   return TRUE;
}


BOOL Config_ReadUserString (LPCTSTR pszLHS, LPTSTR pszRHS, size_t cchMax)
{
   HKEY hk;
    if (RegOpenKeyEx (HKEY_CURRENT_USER, TEXT(AFSREG_USER_OPENAFS_SUBKEY), 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
        if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_OPENAFS_SUBKEY), 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
            return FALSE;

   DWORD dwSize = sizeof(TCHAR) * cchMax;
   if (RegQueryValueEx (hk, pszLHS, NULL, NULL, (PBYTE)pszRHS, &dwSize) != ERROR_SUCCESS)
      {
      RegCloseKey (hk);
      return FALSE;
      }

   RegCloseKey (hk);
   return TRUE;
}


void Config_WriteUserNum (LPCTSTR pszLHS, DWORD dwRHS)
{
   HKEY hk;
   DWORD dwDisp;
   if (RegCreateKeyEx (HKEY_CURRENT_USER, TEXT(AFSREG_USER_OPENAFS_SUBKEY), 0, TEXT("container"), 0, KEY_SET_VALUE, NULL, &hk, &dwDisp) == ERROR_SUCCESS)
      {
      RegSetValueEx (hk, pszLHS, NULL, REG_DWORD, (PBYTE)&dwRHS, sizeof(dwRHS));
      RegCloseKey (hk);
      }
}


void Config_WriteUserString (LPCTSTR pszLHS, LPCTSTR pszRHS)
{
   HKEY hk;
   DWORD dwDisp;
   if (RegCreateKeyEx (HKEY_CURRENT_USER, TEXT(AFSREG_USER_OPENAFS_SUBKEY), 0, TEXT("container"), 0, KEY_SET_VALUE, NULL, &hk, &dwDisp) == ERROR_SUCCESS)
      {
      RegSetValueEx (hk, pszLHS, NULL, REG_SZ, (PBYTE)pszRHS, sizeof(TCHAR) * (1+lstrlen(pszRHS)));
      RegCloseKey (hk);
      }
}


