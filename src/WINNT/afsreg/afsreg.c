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
#include <string.h>
#include <stdio.h>

#include "afsreg.h"


/* Extended (alternative) versions of registry access functions.
 * All functions return WIN32 style error codes.
 */


static long CopyKey(const char *sourceKey, const char *targetKey);
static long CopyValues(HKEY srcKey, HKEY dupKey);
static long CopySubkeys(const char *srcName, HKEY srcKey,
			const char *dupName, HKEY dupKey);

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
int IsWow64(void)
{
    static int init = TRUE;
    static int bIsWow64 = FALSE;

    if (init) {
        HMODULE hModule;
        LPFN_ISWOW64PROCESS fnIsWow64Process = NULL;

        hModule = GetModuleHandle(TEXT("kernel32"));
        if (hModule) {
            fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(hModule, "IsWow64Process");
  
            if (NULL != fnIsWow64Process)
            {
                if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
                {
                    // on error, assume FALSE.
                    // in other words, do nothing.
                }
            }       
            FreeLibrary(hModule);
        }
        init = FALSE;
    }
    return bIsWow64;
}


/* ----------------------- exported functions ----------------------- */


/* RegOpenKeyAlt() -- Open a key in the registry and return its handle.
 *     If create is set, the subkey (and all its parent keys on the path)
 *     is created if it does not exist.  In either case, the disposition of
 *     the key is indicated as REG_CREATED_NEW_KEY or REG_OPENED_EXISTING_KEY.
 *
 *     Note: if key is null (AFSREG_NULL_KEY) then the first component of
 *           subKeyName must be one of the predefined keys; resultKeyDispP
 *           can always be NULL.
 */

long
RegOpenKeyAlt(HKEY key,               /* [in] open key from which to start */
	      const char *subKeyName, /* [in] sub key path */
	      DWORD mode,             /* [in] desired access */
	      int create,             /* [in] if set, creates key(s) on path */
	      HKEY *resultKeyP,       /* [out] open key handle */
	      DWORD *resultKeyDispP)  /* [out] open key disposition */
{
    long status;
    DWORD keyDisp = REG_OPENED_EXISTING_KEY;

    if (key == AFSREG_NULL_KEY) {
	/* No starting key; first path component must be predefined key.
	 * NOTE: predefined keys are always open (i.e., don't need opening).
	 */
	const char *tokenP = subKeyName + strspn(subKeyName, "\\");
	size_t tokenSz = strcspn(tokenP, "\\");

	if (!strncmp(tokenP, "HKEY_LOCAL_MACHINE", tokenSz))
	    key = HKEY_LOCAL_MACHINE;
	else if (!strncmp(tokenP, "HKEY_CURRENT_USER", tokenSz))
	    key = HKEY_CURRENT_USER;
	else if (!strncmp(tokenP, "HKEY_CURRENT_CONFIG", tokenSz))
	    key = HKEY_CURRENT_CONFIG;
	else if (!strncmp(tokenP, "HKEY_USERS", tokenSz))
	    key = HKEY_USERS;
	else if (!strncmp(tokenP, "HKEY_CLASSES_ROOT", tokenSz))
	    key = HKEY_CLASSES_ROOT;
	else if (!strncmp(tokenP, "HKEY_PERFORMANCE_DATA", tokenSz))
	    key = HKEY_PERFORMANCE_DATA;
	else if (!strncmp(tokenP, "HKEY_DYN_DATA", tokenSz))
	    key = HKEY_DYN_DATA;
	else {
	    return ERROR_INVALID_PARAMETER;
	}

	subKeyName = tokenP + tokenSz + 1;
    }

    /* open (and possibly create) sub key */
    if (create) {
	status = RegCreateKeyEx(key, subKeyName,
				(DWORD)0, "AFS", REG_OPTION_NON_VOLATILE,
				(IsWow64()?KEY_WOW64_64KEY:0)|mode, NULL, resultKeyP, &keyDisp);
    } else {
	status = RegOpenKeyEx(key, subKeyName, (DWORD)0, (IsWow64()?KEY_WOW64_64KEY:0)|mode, resultKeyP);
    }

    if (resultKeyDispP) {
	*resultKeyDispP = keyDisp;
    }

    return status;
}


/*
 * RegQueryValueAlt() -- Read data associated with a key value.
 *     If a buffer is supplied (i.e., *dataPP is not NULL) then
 *     the data is placed in that buffer; in this case *dataSizeP
 *     is the buffer size on input and the data size on output.
 *     Otherwise, if *dataPP is NULL, a data buffer is allocated
 *     and *dataPP is set to point to that buffer.
 *
 *     NOTE: dataTypeP can always be NULL, and dataSizeP can be NULL
 *           only if *dataPP is NULL.
 */

long
RegQueryValueAlt(HKEY key,               /* [in] open key handle */
		 const char *valueName,  /* [in] value name */
		 DWORD *dataTypeP,   /* [out] type of value's data */
		 void **dataPP,      /* [in/out] buffer for value's data */
		 DWORD *dataSizeP)   /* [in/out] size of data (buffer) */
{
    long status;

    if (*dataPP) {
	/* Use user-supplied data buffer; no real work */
	status = RegQueryValueEx(key, valueName, NULL,
				 dataTypeP, *dataPP, dataSizeP);
    } else {
	DWORD bufType, bufSize, dwordData;
	char *buf;

	/* get size of value's data; optimize read for common REG_DWORD */
	bufSize = sizeof(DWORD);

	status = RegQueryValueEx(key, valueName, NULL,
				 &bufType, (void*)&dwordData, &bufSize);

	if (status == ERROR_SUCCESS || status == ERROR_MORE_DATA) {
	    /* allocate buffer for value's data */
	    buf = malloc(bufSize);

	    if (!buf) {
		status = ERROR_NOT_ENOUGH_MEMORY;
	    } else {
		if (status == ERROR_SUCCESS) {
		    /* data fit in DWORD buffer; don't read registry again */
		    memcpy(buf, &dwordData, bufSize);
		} else {
		    /* data did not fit in DWORD buffer; read registry again */
		    status = RegQueryValueEx(key, valueName, NULL,
					     &bufType, buf, &bufSize);
		}

		if (status == ERROR_SUCCESS) {
		    /* return requested results */
		    *dataPP = buf;
		    if (dataTypeP) *dataTypeP = bufType;
		    if (dataSizeP) *dataSizeP = bufSize;
		} else {
		    /* registry read failed */
		    free(buf);
		}
	    }
	}
    }

    return status;
}



/*
 * RegEnumKeyAlt() -- Enumerate subkey names of specified key.
 *     Subkey names are returned in a single multistring (REG_MULTI_SZ).
 *     If the key has no subkeys, then *subkeyNames is set to NULL.
 *
 *     Note:  A multistring is a char buffer containing individual strings
 *     separated by a '\0' char with the last string being followed by
 *     two '\0' chars.
 */

long
RegEnumKeyAlt(HKEY key,            /* [in] open key handle */
	      char **subkeyNames)  /* [out] subkey name buf (multistring) */
{
    long status;
    DWORD skCount, skNameLenMax;
    char *skNameBuf = NULL;

    status = RegQueryInfoKey(key,
			     NULL, NULL, NULL,
			     &skCount, &skNameLenMax,
			     NULL, NULL, NULL, NULL, NULL, NULL);

    if (status == ERROR_SUCCESS && skCount > 0) {
	skNameBuf = malloc((skCount * (skNameLenMax + 1)) + 1);

	if (!skNameBuf) {
	    status = ERROR_NOT_ENOUGH_MEMORY;
	} else {
	    DWORD i, skFillLen;
	    char *skBufP = skNameBuf;

	    for (i = 0; i < skCount && status == ERROR_SUCCESS; i++) {
		skFillLen = skNameLenMax + 1;
		status = RegEnumKeyEx(key, i, skBufP, &skFillLen,
				      NULL, NULL, NULL, NULL);
		if (status == ERROR_SUCCESS) {
		    skBufP += skFillLen + 1;
		}
	    }
	    *skBufP = '\0';

	    if (status != ERROR_SUCCESS) {
		free(skNameBuf);
		skNameBuf = NULL;
	    }
	}
    }

    *subkeyNames = skNameBuf;
    return status;
}


/*
 * RegDeleteKeyAlt() -- Delete named subkey and all its subkeys and values.
 *
 *    This is just a recursive version of RegDeleteKey(), which does not
 *    recurse on NT (though it does on Win95/98).
 */
long
RegDeleteKeyAlt(HKEY key,
		const char *subKeyName)
{
    long status;

    status = RegDeleteKey(key, subKeyName);

    if (status != ERROR_SUCCESS) {
	/* determine if delete failed due to subkeys */
	HKEY subKey;

	status = RegOpenKeyEx(key, subKeyName, 0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_ALL_ACCESS, &subKey);
	if (status == ERROR_SUCCESS) {
	    char *keyEnum;

	    status = RegEnumKeyAlt(subKey, &keyEnum);
	    if (status == ERROR_SUCCESS && keyEnum != NULL) {
		/* subkeys found; try to delete each; ignore errors */
		char *keyEnumName;

		for (keyEnumName = keyEnum;
		     *keyEnumName != '\0';
		     keyEnumName += strlen(keyEnumName) + 1) {
		    (void) RegDeleteKeyAlt(subKey, keyEnumName);
		}
		free(keyEnum);
	    }
	    (void) RegCloseKey(subKey);
	}

	/* try delete again */
	status = RegDeleteKey(key, subKeyName);
    }
    return status;
}


/*
 * RegDeleteEntryAlt() -- delete named key or value; if key, then all subkeys
 *     and values are removed; entryName must be in canonical path format.
 */
long
RegDeleteEntryAlt(const char *entryName, regentry_t entryType)
{
    long status = ERROR_SUCCESS;
    char *entryBuf = _strdup(entryName);

    if (entryBuf == NULL) {
	status = ERROR_NOT_ENOUGH_MEMORY;
    } else {
	char *keyPath = entryBuf;
	char *entryName = strrchr(entryBuf, '\\');

	if (entryName == NULL) {
	    status = ERROR_INVALID_PARAMETER;
	} else {
	    HKEY key;

	    *entryName = '\0';
	    entryName++;

	    status = RegOpenKeyAlt(AFSREG_NULL_KEY,
				   keyPath, KEY_ALL_ACCESS, 0, &key, NULL);
	    if (status == ERROR_SUCCESS) {
		if (entryType == REGENTRY_KEY) {
		    status = RegDeleteKeyAlt(key, entryName);
		} else if (entryType == REGENTRY_VALUE) {
		    status = RegDeleteValue(key, entryName);
		} else {
		    status = ERROR_INVALID_PARAMETER;
		}
		(void) RegCloseKey(key);
	    }
	}
	free(entryBuf);
    }
    return status;
}


/*
 * RegDupKeyAlt() -- duplicate sourceKey as targetKey; both sourceKey and
 *     targetKey must be in canonical path format.
 *
 *     NOTE: if targetKey already exists it will be replaced.
 */
long
RegDupKeyAlt(const char *sourceKey, const char *targetKey)
{
    long status;

    /* delete target key if extant */
    status = RegDeleteEntryAlt(targetKey, REGENTRY_KEY);

    if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) {
	status = CopyKey(sourceKey, targetKey);

	if (status != ERROR_SUCCESS) {
	    /* clean-up partial duplication */
	    (void) RegDeleteEntryAlt(targetKey, REGENTRY_KEY);
	}
    }
    return status;
}



/* ----------------------- local functions ----------------------- */


/*
 * CopyKey() -- worker function implementing RegDupKeyAlt().
 *
 *     Note: - assumes target does not exist (i.e., deleted by RegDupKeyAlt())
 *           - no cleanup on failure (i.e., assumes done by RegDupKeyAlt())
 */
static long
CopyKey(const char *sourceKey, const char *targetKey)
{
    long status;
    HKEY srcKey, dupKey;

    /* open source key */
    status = RegOpenKeyAlt(AFSREG_NULL_KEY, sourceKey,
			   KEY_READ, 0, &srcKey, NULL);
    if (status == ERROR_SUCCESS) {
	/* create target key */
	status = RegOpenKeyAlt(AFSREG_NULL_KEY, targetKey,
			       KEY_ALL_ACCESS, 1 /* create */, &dupKey, NULL);
	if (status == ERROR_SUCCESS) {
	    /* copy values and their data from source to target */
	    status = CopyValues(srcKey, dupKey);

	    if (status == ERROR_SUCCESS) {
		/* copy subkeys from source to target */
		status = CopySubkeys(sourceKey, srcKey, targetKey, dupKey);
	    }
	    (void) RegCloseKey(dupKey);
	}
	(void) RegCloseKey(srcKey);
    }
    return status;
}


/*
 * CopyValues() -- copy values and their data from srcKey to dupKey
 */
static long
CopyValues(HKEY srcKey, HKEY dupKey)
{
    long status;
    DWORD valCount, valNameLenMax, valDataLenMax;

    status = RegQueryInfoKey(srcKey,
			     NULL, NULL, NULL, NULL, NULL, NULL,
			     &valCount, &valNameLenMax, &valDataLenMax,
			     NULL, NULL);

    if (status == ERROR_SUCCESS && valCount > 0) {
	char *valBuffer = malloc(valNameLenMax + 1 + valDataLenMax);

	if (valBuffer == NULL) {
	    status = ERROR_NOT_ENOUGH_MEMORY;
	} else {
	    DWORD i;
	    char *valName = valBuffer;
	    char *valData = valBuffer + (valNameLenMax + 1);

	    for (i = 0; i < valCount && status == ERROR_SUCCESS; i++) {
		DWORD valNameFillLen = valNameLenMax + 1;
		DWORD valDataFillLen = valDataLenMax;
		DWORD valDataType;

		status = RegEnumValue(srcKey, i,
				      valName, &valNameFillLen,
				      NULL,
				      &valDataType,
				      valData, &valDataFillLen);

		if (status == ERROR_SUCCESS) {
		    status = RegSetValueEx(dupKey,
					   valName, 0,
					   valDataType,
					   valData, valDataFillLen);
		}
	    }
	    free(valBuffer);
	}
    }
    return status;
}


/*
 * CopySubkeys() -- copy subkeys from srcKey to dupKey
 *
 *     Note - srcName and dupName are the canonical path names of the
 *            open keys srcKey and dupKey, respectively.
 */
static long
CopySubkeys(const char *srcName, HKEY srcKey, const char *dupName, HKEY dupKey)
{
    long status;
    char *skEnum;

    status = RegEnumKeyAlt(srcKey, &skEnum);

    if (status == ERROR_SUCCESS && skEnum != NULL) {
	char *skEnumName, *skNameBuf;
	size_t skSrcNameMax, skDupNameMax, skNameMax;

	skNameMax = 0;
	skEnumName = skEnum;
	while (*skEnumName != '\0') {
	    size_t skNameLen = strlen(skEnumName);
	    skNameMax = max(skNameMax, skNameLen);

	    skEnumName += skNameLen + 1;
	}

	skSrcNameMax = strlen(srcName) + 1 + skNameMax + 1;
	skDupNameMax = strlen(dupName) + 1 + skNameMax + 1;

	skNameBuf = malloc(skSrcNameMax + skDupNameMax);
	if (skNameBuf == NULL) {
	    status = ERROR_NOT_ENOUGH_MEMORY;
	} else {
	    char *skSrcName = skNameBuf;
	    char *skDupName = skNameBuf + skSrcNameMax;

	    for (skEnumName = skEnum;
		 *skEnumName != '\0' && status == ERROR_SUCCESS;
		 skEnumName += strlen(skEnumName) + 1) {
		sprintf(skSrcName, "%s\\%s", srcName, skEnumName);
		sprintf(skDupName, "%s\\%s", dupName, skEnumName);

		status = CopyKey(skSrcName, skDupName);
	    }
	    free(skNameBuf);
	}
	free(skEnum);
    }
    return status;
}
