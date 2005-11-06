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
#include <stdio.h>

#include <WINNT\afsreg.h>

int
main(int argc, char **argv) {
    HKEY hkSubmounts;
    HKEY hkParameters;
    char mountRoot[64]="/afs";
    char * mountstring;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "afsshare.exe <submount> [<afs mount path>]\n");
        exit(1);
    }

    if (RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                        AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                        0,
                        NULL,
                        REG_OPTION_NON_VOLATILE,
                        KEY_READ|KEY_WRITE,
                        NULL,
                        &hkSubmounts,
                        NULL) == ERROR_SUCCESS) 
    {
        if ( argc == 2 ) {
            if (RegDeleteValue(hkSubmounts, argv[1])) {
                fprintf(stderr,"Submount Deletion failure for [%s]: %lX",
                         argv[1], GetLastError());
                RegCloseKey(hkSubmounts);
                return 1;
            }
        } else {
            if (RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                                AFSREG_CLT_SVC_PARAM_SUBKEY,
                                0,
                                NULL,
                                REG_OPTION_NON_VOLATILE,
                                KEY_READ,
                                NULL,
                                &hkParameters,
                                NULL) == ERROR_SUCCESS) 
            {
                DWORD dwSize = sizeof(mountRoot);
                RegQueryValueEx (hkParameters, "MountRoot", NULL, NULL, (PBYTE)mountRoot, &dwSize);
                RegCloseKey(hkParameters);
            }


            if ( !strncmp(mountRoot, argv[2], strlen(mountRoot)) )
                mountstring = argv[2] + strlen(mountRoot);
            else
                mountstring = argv[2];

            if (RegSetValueEx(hkSubmounts, argv[1], 0, REG_EXPAND_SZ, mountstring, (DWORD)strlen(mountstring)+1)) {
                fprintf(stderr,"Submount Set failure for [%s]: %lX",
                         argv[1], GetLastError());
                RegCloseKey(hkSubmounts);
                return 2;
            }
        }
        RegCloseKey(hkSubmounts);
    } else {
        fprintf(stderr,"Submount access denied: %lX", GetLastError());
        return 3;
    }
    return 0;
}
