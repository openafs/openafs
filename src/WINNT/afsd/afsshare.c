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

int
main(int argc, char **argv) {
    BOOL res;
    HKEY hkSubmounts;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Incorrect arguments\n");
        exit(1);
    }

    if (RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                        "SOFTWARE\\OpenAFS\\Client\\Submounts",
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
                return 1;
            }
        } else {
            if (RegSetValueEx(hkSubmounts, argv[1], 0, REG_SZ, argv[2], strlen(argv[2])+1)) {
                fprintf(stderr,"Submount Set failure for [%s]: %lX",
                         argv[1], GetLastError());
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
