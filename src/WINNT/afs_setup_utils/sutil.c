/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <WINNT/afsreg.h>

#include "sutil.h"

/* Some install/uninstall related utilities. */


#define NETWORK_PROVIDER_ORDER_KEY	"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\NetworkProvider\\Order"
#define PROVIDER_ORDER_VALUE_NAME	"ProviderOrder"

#define	ENVIRONMENT_KEY			"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"

#define AUTOEXEC_PATH			"c:\\autoexec.bat"
#define AUTOEXEC_TMP_PATH		"c:\\TaAfsAutoexec.tmp"


static BOOL ReadRegEnv(char **ppszEnvValue, char *pszRegValueName);
static BOOL WriteRegEnv(char *pszEnvValue, char *pszEnvName);
static BOOL ReadAutoExec(char **ppszEnvValue, char *pszEnvName);
static BOOL WriteAutoExec(char *pszEnvValue, char *pszEnvName);
static BOOL FindSubString(const char *s1, const char *s2);



/* ------------------ exported functions ---------------------- */

BOOL InNetworkProviderOrder(char *pszNetworkProvider, BOOL *pbIn)
{
    HKEY hKey;
    LONG bResult;
    DWORD dwType;
    char *pszProviderOrder = 0;
    DWORD dwSize;

	bResult = FALSE;	// Assume failure

    if (RegOpenKeyAlt(AFSREG_NULL_KEY, NETWORK_PROVIDER_ORDER_KEY, KEY_READ, FALSE, &hKey, 0) == ERROR_SUCCESS) {
        if (RegQueryValueAlt(hKey, PROVIDER_ORDER_VALUE_NAME, &dwType, &pszProviderOrder, &dwSize) == ERROR_SUCCESS) {
			*pbIn = strstr(pszProviderOrder, pszNetworkProvider) != 0;
			bResult = TRUE;
			free(pszProviderOrder);
		}
        RegCloseKey(hKey);
    }

	return bResult;
}


/*
 * AddToProviderOrder() -- add entry to network provider order
 */
BOOL AddToProviderOrder(char *pszWhatToAdd)
{
    HKEY hKey;
    DWORD dwType;
    LONG result;
    int nLen;
    char *pszValue = 0;
    char *pszNewValue;
    BOOL bAlreadyAdded = FALSE;

    /* Open the key, creating it if necessary (but should always be there). */
    result = RegOpenKeyAlt(AFSREG_NULL_KEY,
			   NETWORK_PROVIDER_ORDER_KEY,
			   KEY_SET_VALUE | KEY_ALL_ACCESS, TRUE, &hKey, 0);
    if (result != ERROR_SUCCESS)
	return FALSE;

    /* Get the old value */
    result = RegQueryValueAlt(hKey,
			      PROVIDER_ORDER_VALUE_NAME,
			      &dwType, &pszValue, &nLen);
    if (result != ERROR_SUCCESS) {
	nLen = 0;
    }

    pszNewValue = malloc(nLen + strlen(pszWhatToAdd) + 1);/* Add 1 for comma */
    *pszNewValue = 0;

    /* Add the new value */
    if (result == ERROR_SUCCESS) {
	if (strstr(pszValue, pszWhatToAdd) != 0)
	    bAlreadyAdded = TRUE;
	else {
	    if (pszValue && *pszValue) {
		strcpy(pszNewValue, pszValue);
		strcat(pszNewValue, ",");
	    }
	    strcat(pszNewValue, pszWhatToAdd);		
	}

    } else if (result == ERROR_FILE_NOT_FOUND)
	strcpy(pszNewValue, pszWhatToAdd);

    /* Set the new value in the registry */
    if (((result == ERROR_SUCCESS) ||
	 (result == ERROR_FILE_NOT_FOUND)) && !bAlreadyAdded)
	result = RegSetValueEx(hKey, PROVIDER_ORDER_VALUE_NAME, 0,
			       REG_SZ, pszNewValue, strlen(pszNewValue) + 1);
    free(pszNewValue);
    free(pszValue);

    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS);
}


/*
 * RemoveFromProviderOrder() -- remove entry from network provider order
 */
BOOL RemoveFromProviderOrder(char *pszWhatToDel)
{
    HKEY hKey;
    DWORD dwType;
    LONG result;
    int nLen;
    char *pszValue = 0;
    char *pszNewValue;
    BOOL bAlreadyRemoved = FALSE;

    /* Open the key, creating if necessary (but should always be there). */
    result = RegOpenKeyAlt(AFSREG_NULL_KEY, NETWORK_PROVIDER_ORDER_KEY,
			   KEY_SET_VALUE | KEY_ALL_ACCESS, TRUE, &hKey, 0);
    if (result != ERROR_SUCCESS)
	return FALSE;

    /* Get the old value */
    result = RegQueryValueAlt(hKey, PROVIDER_ORDER_VALUE_NAME,
			      &dwType, &pszValue, &nLen);

    if (result == ERROR_SUCCESS) {
	pszNewValue = malloc(nLen); /* bigger than we need, but that's ok */
	*pszNewValue = 0;

	if (strstr(pszValue, pszWhatToDel) == 0)
	    bAlreadyRemoved = TRUE;
	else {
	    char *pszCur;

	    pszCur = strtok(pszValue, ",");
	    while (pszCur) {
		if (strcmp(pszCur, pszWhatToDel) != 0) {
		    if (*pszNewValue)
			strcat(pszNewValue, ",");
		    strcat(pszNewValue, pszCur);
		}
		pszCur = strtok(0, ",");
	    }
	}

	/* Set the new value in the registry */
	if (!bAlreadyRemoved)
	    result = RegSetValueEx(hKey, PROVIDER_ORDER_VALUE_NAME, 0, REG_SZ,
				   pszNewValue, strlen(pszNewValue) + 1);
	free(pszNewValue);
	free(pszValue);
    }
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS);
}


/*
 * ReadSystemEnv() -- read system environment variable
 */
BOOL ReadSystemEnv(char **ppszEnvValue, char *pszEnvName)
{
    if (IsWinNT())
	return ReadRegEnv(ppszEnvValue, pszEnvName);
    else
	return ReadAutoExec(ppszEnvValue, pszEnvName);
}


/*
 * WriteSystemEnv() -- write system environment variable
 */
BOOL WriteSystemEnv(char *pszEnvValue, char *pszEnvName)
{
    if (IsWinNT())
	return WriteRegEnv(pszEnvValue, pszEnvName);
    else
	return WriteAutoExec(pszEnvValue, pszEnvName);
}


/*
 * AddToSystemPath() -- add specified entry to system PATH variable.
 */
BOOL AddToSystemPath(char *pszPath)
{
    char *pszCurPath = 0;
    char *pszNewPath = 0;
    BOOL bStatus = TRUE;

    if (!ReadSystemEnv(&pszCurPath, "Path"))
	return FALSE;

    /* Do we need to add it? */
    if (!pszCurPath || !FindSubString(pszCurPath, pszPath)) {
		
	/* Old path + a semicolon + the new path entry + a null */
	pszNewPath = malloc((pszCurPath ? strlen(pszCurPath) + 1 : 0) +
			    strlen(pszPath) + 1);
	if (pszNewPath == 0) {
	    if (pszCurPath)
		free(pszCurPath);
	    return FALSE;
	}

	pszNewPath[0] = 0;

	if (pszCurPath) {
	    strcpy(pszNewPath, pszCurPath);
	    strcat(pszNewPath, ";");
	}

	strcat(pszNewPath, pszPath);
	bStatus = WriteSystemEnv(pszNewPath, "Path");

	free(pszNewPath);
    }

    if (pszCurPath)
	free(pszCurPath);

    return bStatus;
}


/*
 * RemoveFromSystemPath() -- remove specified entry from system PATH variable.
 */
BOOL RemoveFromSystemPath(char *pszPath)
{
    char *pszCurNls = 0;
    char *pszNewNls = 0;
    char *pSemi = 0;
    char *pCurPath = 0;
    BOOL bStatus = TRUE;

    if (!ReadSystemEnv(&pszCurNls, "Path"))
	return FALSE;

    /* Is it already not in the path? */
    if (!pszCurNls || !FindSubString(pszCurNls, pszPath)) {
	if (pszCurNls)
	    free(pszCurNls);
	return TRUE;
    }

    pszNewNls = (char *)malloc(strlen(pszCurNls) + 1);
    if (pszNewNls == 0) {
	free(pszCurNls);
	return FALSE;
    }

    pszNewNls[0] = 0;

    pCurPath = pszCurNls;

    while (1) {
	pSemi = strchr(pCurPath, ';');
	if (pSemi)
	    *pSemi = 0;

	if (_stricmp(pCurPath, pszPath) != 0) {
	    if (pszNewNls[0] != 0)
		strcat(pszNewNls, ";");
	    strcat(pszNewNls, pCurPath);
	}

	if (!pSemi)
	    break;

	pSemi++;
	pCurPath = pSemi;
    }

    bStatus = WriteSystemEnv(pszNewNls, "Path");

    free(pszNewNls);
    free(pszCurNls);

    return bStatus;
}


/*
 * IsWinNT() -- determine if system is NT or other (95/98).
 */
BOOL IsWinNT()
{
    DWORD dwVersion = GetVersion();

    return (dwVersion < 0x80000000);
}






/* ------------------ utility functions ---------------------- */


/*
 * ReadRegEnv() -- read system enviornment variable from registry (NT only).
 */
static BOOL ReadRegEnv(char **ppszEnvValue, char *pszRegValueName)
{
    HKEY hKey;
    DWORD dwType;
    LONG result;
    int nLen = 512;

    result = RegOpenKeyAlt(AFSREG_NULL_KEY, ENVIRONMENT_KEY,
			   KEY_SET_VALUE | KEY_ALL_ACCESS, FALSE, &hKey, 0);
    if (result != ERROR_SUCCESS)
	return FALSE;

    *ppszEnvValue = 0;

    do {
	if (*ppszEnvValue)
	    free(*ppszEnvValue);

	*ppszEnvValue = (char *)malloc(nLen);
	if (*ppszEnvValue == 0) {
	    RegCloseKey(hKey);
	    return FALSE;
	}

	/* If function fails to open the value and the error code says that
	 * the value doesn't exist, then we will attempt to make it.
	 */
	result = RegQueryValueEx(hKey, pszRegValueName, 0,
				 &dwType, *ppszEnvValue, &nLen);

	if (result == ERROR_FILE_NOT_FOUND) {
	    result = RegSetValueEx(hKey, pszRegValueName, 0,
				   REG_EXPAND_SZ, "", 0);
	    **ppszEnvValue = '\0';  /* zero length string "read" */
	}
    } while (result == ERROR_MORE_DATA);

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS || strlen(*ppszEnvValue) == 0) {
	/* Don't return empty strings; instead set buffer pointer to 0. */
	free(*ppszEnvValue);
	*ppszEnvValue = 0;
    }
    return (result == ERROR_SUCCESS);
}


/*
 * WriteRegEnv() -- write system environment variable to registry (NT only).
 */
static BOOL WriteRegEnv(char *pszEnvValue, char *pszEnvName)
{
    LONG result;
    HKEY hKey;

    result = RegOpenKeyAlt(AFSREG_NULL_KEY, ENVIRONMENT_KEY,
			   KEY_ALL_ACCESS, FALSE, &hKey, 0);
    if (result != ERROR_SUCCESS)
	return FALSE;

    result = RegSetValueEx(hKey, pszEnvName, 0, REG_EXPAND_SZ,
			   pszEnvValue, strlen(pszEnvValue) + 1);
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS);
}


/*
 * ReadAutoExec() -- read environment variable from autoexec.bat (95/98).
 */
static BOOL ReadAutoExec(char **ppszEnvValue, char *pszEnvName)
{
    char szSetCmd[256];
    char szLine[2048];
    FILE *fp;

    *ppszEnvValue = 0;

    fp = fopen(AUTOEXEC_PATH, "rt");
    if (fp == 0)
	return FALSE;

    /* Create the string we are looking for */
    sprintf(szSetCmd, "SET %s", pszEnvName);

    /* Now read each line and look for our SetCmd string */
    while (1) {
	int nLineLen;
	fgets(szLine, sizeof(szLine), fp);

	if (feof(fp))
	    break;

	/* Strip off the trailing newline */
	nLineLen = strlen(szLine);
	if (szLine[nLineLen - 1] == '\n') {
	    nLineLen--;
	    szLine[nLineLen] = 0;
	}

	if (_strnicmp(szSetCmd, szLine, strlen(szSetCmd)) == 0) {
	    char *value = strchr(szLine, '=');
	    if (value)
		*ppszEnvValue = _strdup(++value);
	    break;
	}
    }

    /* Don't return empty strings; instead set buffer to 0. */
    if (*ppszEnvValue && (strlen(*ppszEnvValue) == 0)) {
	free(*ppszEnvValue);
	*ppszEnvValue = 0;
    }

    fclose(fp);

    return TRUE;
}


/*
 * WriteAutoExec() -- write environment variable to autoexec.bat (95/98).
 */
static BOOL WriteAutoExec(char *pszEnvValue, char *pszEnvName)
{
    char szSetCmd[256];
    char szLine[512];
    BOOL bValueWritten = FALSE;
    FILE *fpIn, *fpOut;
    BOOL bResult;

    fpOut = fopen(AUTOEXEC_TMP_PATH, "wt");
    if (fpOut == 0)
	return FALSE;

    sprintf(szSetCmd, "SET %s", pszEnvName);

    fpIn = fopen(AUTOEXEC_PATH, "rt");
    if (fpIn != 0) {
	/* Now read each line and look for our SetCmd string */
	while (1) {
	    fgets(szLine, sizeof(szLine), fpIn);
	    if (feof(fpIn))
		break;

	    if (!bValueWritten &&
		(_strnicmp(szSetCmd, szLine, strlen(szSetCmd)) == 0)) {
		fprintf(fpOut, "%s=%s\n", szSetCmd, pszEnvValue);
		bValueWritten = TRUE;
	    } else
		fputs(szLine, fpOut);
	}

	fclose(fpIn);
    }

    /* If the value didn't previously exist, then add it to the end */
    if (!bValueWritten)
	fprintf(fpOut, "%s=%s\n", szSetCmd, pszEnvValue);

    fclose(fpOut);

    bResult = CopyFile(AUTOEXEC_TMP_PATH, AUTOEXEC_PATH, FALSE);

    /* Try to delete this even if the copy fails.  Tie the return code
     * to the copy and not the delete.
     */
    DeleteFile(AUTOEXEC_TMP_PATH);

    return bResult;
}


/*
 * FindSubString() -- basically a case-insensitive strstr().
 */
static BOOL FindSubString(const char *s1, const char *s2)
{
    char *ls1, *ls2;
    BOOL bFound = FALSE;

    ls1 = _strdup(s1);
    if (!ls1)
	return FALSE;

    ls2 = _strdup(s2);
    if (!ls2) {
	free(ls1);
	return FALSE;
    }

    bFound = strstr(_strlwr(ls1), _strlwr(ls2)) != 0;

    free(ls2);
    free(ls1);
    return bFound;
}
