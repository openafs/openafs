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

#include "forceremove.h"
#include "sutil.h"


/* Functions to forcibly remove AFS software without using InstallShield. */

#define CLIENT34_FME_VALUE  "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion\\File Manager\\AddOns\\AFS Client FME"

#define MS_UNINSTALL_KEY  "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall"


static DWORD
ClientSoftwareGet(DWORD *version, char *dir, DWORD dirSize);

static DWORD
ClientServiceDelete(void);

static DWORD
FileForceRemove(const char *filePath);

static DWORD
DirectoryForceRemove(const char *dir);

static DWORD
FolderLocateInTree(const char *dir, const char *folderName, char *buf);

static DWORD
Client34ZapUninstallKeys(void);



/* ------------------ exported functions ---------------------- */


/*
 * Client34Eradicate() -- remove AFS 3.4a client, if extant.
 *
 *     If keepConfig is TRUE then the following are not removed:
 *
 *         a) file %WINDIR%\afsdcell.ini - CellServDB
 *         b) file %WINDIR%\afsdsbmt.ini - submount settings
 *         c) file %WINDIR%\afsd.ini - client parameter settings
 */
DWORD Client34Eradicate(BOOL keepConfig)
{
    DWORD rc = ERROR_SUCCESS;
    DWORD status, version;
    BOOL installPathValid = FALSE;
    char installPath[MAX_PATH];
    char winPath[MAX_PATH];
    char sysPath[MAX_PATH];
    char filePath[MAX_PATH];

    /* check client version and fetch install directory */

    status = ClientSoftwareGet(&version, installPath, MAX_PATH);

    if (status == ERROR_SUCCESS) {
	if (version == 34) {
	    /* 3.4 client to eradicate */
	    installPathValid = TRUE;
	} else {
	    /* 3.5 or later client installed */
	    return ERROR_SUCCESS;
	}
    }

    /* If no client info found then assume partial unconfigure and keep
     * trying.  Save errors as proceed but keep going to maximize removal.
     */


    /* stop and delete client service */

    status = ClientServiceDelete();
    if (status != ERROR_SUCCESS) {
	rc = status;
    }

    /* remove client files */

    status = GetWindowsDirectory(winPath, MAX_PATH);
    if (status == 0 || status > MAX_PATH) {
	/* this should never happen */
	winPath[0] = '\0';
    }

    status = GetSystemDirectory(sysPath, MAX_PATH);
    if (status == 0 || status > MAX_PATH) {
	/* this should never happen */
	sysPath[0] = '\0';
    }

    if (installPathValid) {
	status = DirectoryForceRemove(installPath);
	if (status != ERROR_SUCCESS && status != ERROR_PATH_NOT_FOUND) {
	    rc = status;
	}
    }

    sprintf(filePath, "%s\\%s", winPath, "temp\\afsd.log");
    status = FileForceRemove(filePath);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }

    sprintf(filePath, "%s\\%s", winPath, "temp\\afsd_init.log");
    status = FileForceRemove(filePath);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }

    sprintf(filePath, "%s\\%s", sysPath, "afs_cpa.cpl");
    status = FileForceRemove(filePath);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }

    sprintf(filePath, "%s\\%s", sysPath, "afs_shl_ext.dll");
    status = FileForceRemove(filePath);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }

    if (!keepConfig) {
	sprintf(filePath, "%s\\%s", installPath, "Client\\afsdcell.ini");
	status = FileForceRemove(filePath);
	if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	    rc = status;
	}

	sprintf(filePath, "%s\\%s", installPath, "Client\\afsdsbmt.ini");
	status = FileForceRemove(filePath);
	if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	    rc = status;
	}

	sprintf(filePath, "%s\\%s", installPath, "Client\\afsd.ini");
	status = FileForceRemove(filePath);
	if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	    rc = status;
	}
    }

    do { /* locate all start menu entries (common or for a user) */
	status = FolderLocateInTree(winPath, "Transarc AFS Client", filePath);
	if (status == ERROR_SUCCESS) {
	    status = DirectoryForceRemove(filePath);
	}
    } while (status == ERROR_SUCCESS);
    if (status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }

    /* update relevant Microsoft registry entries */

    status = RegDeleteEntryAlt(CLIENT34_FME_VALUE, REGENTRY_VALUE);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }

    status = Client34ZapUninstallKeys();
    if (status != ERROR_SUCCESS) {
	rc = status;
    }

    if (!RemoveFromProviderOrder(AFSREG_CLT_SVC_NAME)) {
	/* function does not supply an error value; make one up */
	rc = ERROR_FILE_NOT_FOUND;
    }

    if (installPathValid) {
	status = GetShortPathName(installPath, filePath, MAX_PATH);
	if (status == 0 || status > MAX_PATH) {
	    strcpy(filePath, installPath);
	}
	strcat(filePath, "\\Program");
	if (!RemoveFromSystemPath(filePath)) {
	    /* function does not supply an error value; make one up */
	    rc = ERROR_FILE_NOT_FOUND;
	}
    }

    /* remove client registry entries */

    status = RegDeleteEntryAlt(AFSREG_CLT_SVC_KEY, REGENTRY_KEY);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }

    status = RegDeleteEntryAlt(AFSREG_CLT_SW_KEY, REGENTRY_KEY);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
	rc = status;
    }
    return rc;
}



/* ------------------ utility functions ---------------------- */


/*
 * ClientSoftwareGet() -- fetch client software installation information.
 */
static DWORD
ClientSoftwareGet(DWORD *version, char *dir, DWORD dirSize)
{
    HKEY key;
    DWORD rc = ERROR_SUCCESS;

    /* get client version and install directory */

    rc = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_CLT_SW_VERSION_KEY,
		       KEY_READ, 0, &key, NULL);
    if (rc == ERROR_SUCCESS) {
	DWORD major, minor, dataSize;

	dataSize = sizeof(DWORD);
	rc = RegQueryValueEx(key, AFSREG_CLT_SW_VERSION_MAJOR_VALUE,
			     NULL, NULL, (void*)&major, &dataSize);
	if (rc == ERROR_SUCCESS) {
	    dataSize = sizeof(DWORD);
	    rc = RegQueryValueEx(key, AFSREG_CLT_SW_VERSION_MINOR_VALUE,
				 NULL, NULL, (void*)&minor, &dataSize);
	    if (rc == ERROR_SUCCESS) {
		dataSize = dirSize;
		rc = RegQueryValueEx(key, AFSREG_CLT_SW_VERSION_DIR_VALUE,
				     NULL, NULL, dir, &dataSize);
	    }
	}
	(void)RegCloseKey(key);

	if (rc == ERROR_SUCCESS) {
	    *version = (major * 10) + minor;
	}
    }
    return rc;
}


/*
 * ClientServiceDelete() -- stop and delete the client service.
 */
static DWORD
ClientServiceDelete(void)
{
    SC_HANDLE scmHandle, svcHandle;
    DWORD rc = ERROR_SUCCESS;

    if ((scmHandle = OpenSCManager(NULL,
				   NULL, SC_MANAGER_ALL_ACCESS)) == NULL ||
	(svcHandle = OpenService(scmHandle,
				 AFSREG_CLT_SVC_NAME,
				 SERVICE_ALL_ACCESS)) == NULL) {
	/* can't connect to SCM or can't open service */
	DWORD status = GetLastError();

	if (status != ERROR_SERVICE_DOES_NOT_EXIST) {
	    rc = status;
	}

	if (scmHandle != NULL) {
	    CloseServiceHandle(scmHandle);
	}

    } else {
	SERVICE_STATUS svcStatus;

	if (!ControlService(svcHandle, SERVICE_CONTROL_STOP, &svcStatus)) {
	    /* service stop failed */
	    DWORD status = GetLastError();

	    if (status != ERROR_SERVICE_NOT_ACTIVE) {
		rc = status;
	    }
	}

	if (rc == ERROR_SUCCESS) {
	    if (!DeleteService(svcHandle)) {
		/* service delete failed */
		DWORD status = GetLastError();

		if (status != ERROR_SERVICE_MARKED_FOR_DELETE) {
		    rc = status;
		}
	    }
	}

	CloseServiceHandle(svcHandle);
	CloseServiceHandle(scmHandle);

	if (rc == ERROR_SUCCESS) {
	    /* let client state settle; not mandatory so don't do query */
	    Sleep(2000);
	}
    }
    return rc;
}


/*
 * DirectoryForceRemove() -- forcibly, and recursively, remove a directory
 *     and its contents; this may require moving in-use files to a temp
 *     directory and marking them for delete on reboot.
 */
static DWORD
DirectoryForceRemove(const char *dir)
{
    DWORD rc = ERROR_SUCCESS;
    HANDLE enumHandle;
    WIN32_FIND_DATA enumResult;
    char filePath[MAX_PATH];

    /* enumerate directory and delete contents */

    sprintf(filePath, "%s\\*.*", dir);

    enumHandle = FindFirstFile(filePath, &enumResult);

    if (enumHandle == INVALID_HANDLE_VALUE) {
	DWORD status = GetLastError();

	if (status != ERROR_NO_MORE_FILES) {
	    /* failure other than contents already deleted */
	    rc = status;
	}

    } else {
	while (1) {
	    DWORD status;

	    sprintf(filePath, "%s\\%s", dir, enumResult.cFileName);

	    if (enumResult.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		if (strcmp(enumResult.cFileName, ".") == 0 ||
		    strcmp(enumResult.cFileName, "..") == 0) {
		    /* ignore these special directories */
		    status = ERROR_SUCCESS;
		} else {
		    status = DirectoryForceRemove(filePath);
		}
	    } else {
		status = FileForceRemove(filePath);
	    }

	    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
		/* save error but keep on truckin' */
		rc = status;
	    }

	    if (!FindNextFile(enumHandle, &enumResult)) {
		status = GetLastError();

		if (status != ERROR_NO_MORE_FILES) {
		    rc = status;
		}
		break;
	    }
	}
	FindClose(enumHandle);
    }

    if (rc == ERROR_SUCCESS) {
	if (!RemoveDirectory(dir)) {
	    rc = GetLastError();
	}
    }
    return rc;
}


/*
 * FileForceRemove() -- forcibly remove a file; if the file is in-use then
 *     move the file to a temp directory and mark it for delete on reboot.
 */
static DWORD
FileForceRemove(const char *filePath)
{
    DWORD rc = ERROR_SUCCESS;

    if (!DeleteFile(filePath)) {
	rc = GetLastError();

	if (rc != ERROR_FILE_NOT_FOUND) {
	    /* couldn't just delete; probably in use; try to move */
	    char filePathFull[MAX_PATH];
	    char *dummy;
	    DWORD status;

	    rc = ERROR_SUCCESS;

	    status = GetFullPathName(filePath, MAX_PATH, filePathFull, &dummy);
	    if (status == 0 || status > MAX_PATH) {
		if (status == 0) {
		    rc = GetLastError();
		} else {
		    rc = ERROR_INVALID_PARAMETER;
		}
	    }

	    if (rc == ERROR_SUCCESS) {
		char tempDir[MAX_PATH];
		char tempPath[MAX_PATH];

		status = GetTempPath(MAX_PATH, tempDir);
		if ((status == 0 || status > MAX_PATH) ||
		    (_strnicmp(tempDir, filePathFull, 3))) {
		    /* failed getting temp dir, or temp dir is on different
		     * drive than file (so can't do a true move to there).
		     */
		    sprintf(tempDir, "%c:\\", filePathFull[0]);
		}

		if (!GetTempFileName(tempDir, "AFS", 0, tempPath)) {
		    rc = GetLastError();
		} else {
		    if (MoveFileEx(filePathFull, tempPath,
				   MOVEFILE_REPLACE_EXISTING)) {
			(void)SetFileAttributes(tempPath,
						FILE_ATTRIBUTE_NORMAL);
			(void)MoveFileEx(tempPath, NULL,
					 MOVEFILE_DELAY_UNTIL_REBOOT);
		    } else {
			rc = GetLastError();
		    }
		}
	    }
	}
    }
    return rc;
}


/*
 * FolderLocateInTree() -- find an instance of named directory in specified
 *     tree; folderName is presumed to be a directory name only (i.e., not
 *     a path); buf is presumed to be at least MAX_PATH characters.
 */
static DWORD
FolderLocateInTree(const char *dir, const char *folderName, char *buf)
{
    DWORD rc = ERROR_SUCCESS;
    HANDLE enumHandle;
    WIN32_FIND_DATA enumResult;
    char filePath[MAX_PATH];

    /* enumerate directory recursively looking for folder */

    sprintf(filePath, "%s\\*.*", dir);

    enumHandle = FindFirstFile(filePath, &enumResult);

    if (enumHandle == INVALID_HANDLE_VALUE) {
	DWORD status = GetLastError();

	if (status == ERROR_NO_MORE_FILES) {
	    rc = ERROR_FILE_NOT_FOUND;
	} else {
	    rc = status;
	}

    } else {
	while (1) {
	    DWORD status = ERROR_FILE_NOT_FOUND;

	    if (enumResult.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		if (_stricmp(enumResult.cFileName, folderName) == 0) {
		    /* is folder that we're looking for */
		    sprintf(buf, "%s\\%s", dir, enumResult.cFileName);
		    status = ERROR_SUCCESS;

		} else if (strcmp(enumResult.cFileName, ".") != 0 &&
			   strcmp(enumResult.cFileName, "..") != 0) {
		    /* is not folder that we're looking for; search it */
		    sprintf(filePath, "%s\\%s", dir, enumResult.cFileName);
		    status = FolderLocateInTree(filePath, folderName, buf);
		}
	    }

	    if (status != ERROR_FILE_NOT_FOUND) {
		/* found folder or encountered an error; quit */
		rc = status;
		break;
	    } else {
		/* folder not found; keep looking */
		if (!FindNextFile(enumHandle, &enumResult)) {
		    status = GetLastError();

		    if (status == ERROR_NO_MORE_FILES) {
			rc = ERROR_FILE_NOT_FOUND;
		    } else {
			rc = status;
		    }
		    break;
		}
	    }
	}
	FindClose(enumHandle);
    }
    return rc;
}


/*
 * Client34ZapUninstallKeys() -- delete all of the client uninstall keys
 */
static DWORD
Client34ZapUninstallKeys(void)
{
    DWORD rc = ERROR_SUCCESS;
    HKEY key;

    /* enumerate all uninstall registry keys looking for client's */

    rc = RegOpenKeyAlt(AFSREG_NULL_KEY,
		       MS_UNINSTALL_KEY, KEY_ALL_ACCESS, 0, &key, NULL);
    if (rc == ERROR_SUCCESS) {
	char *keyEnum;

	rc = RegEnumKeyAlt(key, &keyEnum);
	if (rc == ERROR_SUCCESS && keyEnum != NULL) {
	    char *keyEnumName;

	    for (keyEnumName = keyEnum;
		 *keyEnumName != '\0';
		 keyEnumName += strlen(keyEnumName) + 1) {
		if (_stricmp(keyEnumName, "AFSDeinstKey") == 0 ||
		    _strnicmp(keyEnumName, "AFSV34", 6) == 0) {
		    /* found an AFS uninstall key */
		    DWORD status = RegDeleteKeyAlt(key, keyEnumName);
		    if (status != ERROR_SUCCESS) {
			rc = status;
		    }
		}
	    }
	    free(keyEnum);
	}
	(void) RegCloseKey(key);
    }
    return rc;
}
