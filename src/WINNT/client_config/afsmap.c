/* 
 * Copyright 2004.  Secure Endpoints Inc.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
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

/* 
 * 
 */

void usage(char * program)
{
    fprintf(stderr, "Usage: %s <drive> <afs-path> [ <submount> ] [ /persistent ]\n", program);
    fprintf(stderr, "       %s <drive> <unc-path> [ /persistent ]\n", program);
    fprintf(stderr, "       %s <drive> /delete\n", program);
    fprintf(stderr, "       %s /list\n", program);
    fprintf(stderr, "       %s /help\n", program);
    exit(1);
}

int main(int argc, char * argv) 
{
    int retval = 0;
    char drive = 0;
    char mountRoot[16];
    DWORD mountRootLen = 0;
    char buf[200];
	HKEY parmKey;
	DWORD dummyLen;
    DWORD code;
    char  netbiosName[17];
    DWORD netbiosNameLen;
    BOOL  persistent;
    char  submount[32] = "";

    if ( argc < 2 || argc > 5 )
        usage(program);

    if ( stricmp("/list", argv[1]) ) {
        /* print list of afs drive mappings */

        return 0;
    }

    if ( stricmp("/help", argv[1]) ) {
        usage(program);
    }
    
    if ( strlen(argv[1]) == 2 && argv[1][1] == ':' ) {
        drive = toupper(argv[1][0]);
        if ( drive < 'A' || drive > 'Z' )
            usage(program);
    }

    if ( stricmp("/delete", argv[2]) ) {
        if ( !DriveIsGlobalAfsDrive(drive) ) {
            fprintf(stderr, "%s: Drive %c is not mapped to AFS\n", argv[0], drive);
            return 6;
        }

        /* remove drive map */
        WriteActiveMap( drive, FALSE );
        code = DisMountDOSDrive( drive, TRUE );
        if ( code ) {
            fprintf(stderr, "%s: Unable to delete drive %c:, error = %lX\n", argv[0], code);
            return 2;
        }
        return 0;
    }

    /* need to determine if argv[3] contains a /mountroot or \\netbiosname path.  
     * do not use hard code constants; instead use the registry strings
     */
    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSConfigKeyName,
                         0, KEY_QUERY_VALUE, &parmKey);
    dummyLen = sizeof(cm_mountRoot);
	code = RegQueryValueEx(parmKey, "MountRoot", NULL, NULL,
                           mountRoot, &mountRootLen);
	if (code == ERROR_SUCCESS) {
		cm_mountRootLen = strlen(cm_mountRoot);
	} else {
		strncpy(mountRoot, sizeof(cm_mountRoot), "/afs");
		cm_mountRootLen = 4;
	}

    code = lana_GetNetbiosName(netbiosName, LANA_NETBIOS_NAME_FULL);
    if ( code ) {
        fprintf(stderr, "%s: Unable to determine AFS SMB server name\n",argv[0]);
        return 3;
    }
    netbiosNameLen = strlen(netbiosName);

    if ( argv[3][0] == '\\' && argv[3][1] == '\\' && 
         _strnicmp(netbiosName, &argv[3][2], netbiosNameLen) == 0 &&
         argv[3][netbiosNameLen+3] == '\\') {
        /* we have a UNC style path */

        if ( argc == 4 ) {
            persistent = FALSE;
        } else if (argc > 5) {
            usage(argv[0]);
        } else {
            if ( stricmp("/persistent", argv[4]) )
                usage(argv[0]);
            persistent = TRUE;
        }

        code = MountDOSDrive(drive, &argv[3][netbiosNameLen+4], persistent, NULL);
        if ( code ) {
            fprintf(stderr, "%s: Unable to map %c: to %s, error = %lX\n", argv[0], drive, argv[3], code);
            return 4;
        }
        WriteActiveMap( drive, persistent );
        return 0;
    }

    if ( argv[3][0] == '/' && 
         _strnicmp(mountRoot, &argv[1], mountRootLen) &&
         argv[3][mountRootLen+2] == '/') {
        /* we have an afs path */
        /* check to see if we have a submount, if not generate a random one */
        /* check to see if we are persistent or not */

        if ( argc == 4 ) {
            /* we have neither persistence nor a submount name */
            persistent = FALSE;
        } else if ( argc > 6 ) {
            usage(argv[0]);
        } else if ( argc == 5 ) {
            /* we have either persistence or a submount */
            if ( _stricmp("/persistent", argv[4]) == 0 ) {
                persistent = TRUE;
            } else {
                /* we have a submount name */
                if ( !IsValidSubmountName(argv[4]) ) {
                    fprintf(stderr, "%s: invalid submount name: %s\n", argv[4]);
                    return 5;
                }
                strcpy(submount, argv[4]);
                persistent = FALSE;
            }
        } else {
            /* we have both persistent and a submount */
            if ( _stricmp("/persistent", argv[5]) == 0 ) {
                persistent = TRUE;
            } else {
                usage(argv[0]);
            }

            if ( !IsValidSubmountName(argv[4]) ) {
                fprintf(stderr, "%s: invalid submount name: %s\n", argv[4]);
                return 5;
            }
            strcpy(submount, argv[4]);
        }

        WriteActiveMap( drive, persistent );
    }
    return 0;
}
