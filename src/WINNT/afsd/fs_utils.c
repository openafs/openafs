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
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <winioctl.h>
#include <winsock2.h>
#include <nb30.h>

#include <osi.h>
#include "afsd.h"
#include "smb.h"
#include "cmd.h"
#include <fs_utils.h>
#include <WINNT\afsreg.h>

long fs_ExtractDriveLetter(char *inPathp, char *outPathp)
{
	if (inPathp[0] != 0 && inPathp[1] == ':') {
		/* there is a drive letter */
                *outPathp++ = *inPathp++;
                *outPathp++ = *inPathp++;
                *outPathp++ = 0;
        }
	else *outPathp = 0;

        return 0;
}

/* strip the drive letter from a component */
long fs_StripDriveLetter(char *inPathp, char *outPathp, long outSize)
{
	char tempBuffer[1000];
        strcpy(tempBuffer, inPathp);
        if (tempBuffer[0] != 0 && tempBuffer[1] == ':') {
		/* drive letter present */
                strcpy(outPathp, tempBuffer+2);
        }
        else {
        	/* no drive letter present */
        	strcpy(outPathp, tempBuffer);
	}
        return 0;
}

/* take a path with a drive letter, possibly relative, and return a full path
 * without the drive letter.  This is the full path relative to the working
 * dir for that drive letter.  The input and output paths can be the same.
 */
long fs_GetFullPath(char *pathp, char *outPathp, long outSize)
{
	char tpath[1000];
	char origPath[1000];
    char *firstp;
    long code;
    int pathHasDrive;
    int doSwitch;
    char newPath[3];

	if (pathp[0] != 0 && pathp[1] == ':') {
		/* there's a drive letter there */
        firstp = pathp+2;
        pathHasDrive = 1;
    }
    else {
        firstp = pathp;
		pathHasDrive = 0;
	}   
        
    if (*firstp == '\\' || *firstp == '/') {
		/* already an absolute pathname, just copy it back */
        strcpy(outPathp, firstp);
        return 0;
    }
        
    GetCurrentDirectory(sizeof(origPath), origPath);
        
	doSwitch = 0;
    if (pathHasDrive && (*pathp & ~0x20) != (origPath[0] & ~0x20)) {
		/* a drive has been specified and it isn't our current drive.
         * to get path, switch to it first.  Must case-fold drive letters
         * for user convenience.
         */
		doSwitch = 1;
        newPath[0] = *pathp;
        newPath[1] = ':';
        newPath[2] = 0;
        if (!SetCurrentDirectory(newPath)) {
			code = GetLastError();
            return code;
        }
    }
        
    /* now get the absolute path to the current wdir in this drive */
    GetCurrentDirectory(sizeof(tpath), tpath);
    strcpy(outPathp, tpath+2);	/* skip drive letter */
	/* if there is a non-null name after the drive, append it */
	if (*firstp != 0) {
        strcat(outPathp, "\\");
        strcat(outPathp, firstp);
	}

	/* finally, if necessary, switch back to our home drive letter */
    if (doSwitch) {
		SetCurrentDirectory(origPath);
    }
        
    return 0;
}

#ifdef COMMENT
struct hostent *hostutil_GetHostByName(char *namep)
{
	struct hostent *thp;
        
        thp = gethostbyname(namep);
        return thp;
}

/* get hostname or addr, given addr in network byte order */
char *hostutil_GetNameByINet(afs_uint32 addr)
{
	static char hostNameBuffer[256];
        struct hostent *thp;
        
        thp = gethostbyaddr((char *) &addr, sizeof(afs_uint32), AF_INET);
        if (thp)
        	strcpy(hostNameBuffer, thp->h_name);
	else {
		sprintf(hostNameBuffer, "%d.%d.%d.%d",
                	addr & 0xff,
                        (addr >> 8) & 0xff,
                        (addr >> 16) & 0xff,
                        (addr >> 24) & 0xff);
        }

	/* return static buffer */
        return hostNameBuffer;
}
#endif

/* is this a digit or a digit-like thing? */
static int ismeta(ac, abase)
register int abase;
register int ac; {
/*    if (ac == '-' || ac == 'x' || ac == 'X') return 1; */
    if (ac >= '0' && ac <= '7') return 1;
    if (abase <= 8) return 0;
    if (ac >= '8' && ac <= '9') return 1;
    if (abase <= 10) return 0;
    if (ac >= 'a' && ac <= 'f') return 1;
    if (ac >= 'A' && ac <= 'F') return 1;
    return 0;
}

/* given that this is a digit or a digit-like thing, compute its value */
static int getmeta(ac)
register int ac; {
    if (ac >= '0' && ac <= '9') return ac - '0';
    if (ac >= 'a' && ac <= 'f') return ac - 'a' + 10;
    if (ac >= 'A' && ac <= 'F') return ac - 'A' + 10;
    return 0;
}

long util_GetInt32 (as, aval)
register char *as;
long *aval;
{
    register long total;
    register int tc;
    int base;
    int negative;

    total = 0;	/* initialize things */
    negative = 0;

    /* skip over leading spaces */
    while (tc = *as) {
	if (tc != ' ' && tc != '\t') break;
    }

    /* compute sign */
    if (*as == '-') {
	negative = 1;
	as++;	/* skip over character */
    }

    /* compute the base */
    if (*as == '0') {
	as++;
	if (*as == 'x' || *as == 'X') {
	    base = 16;
	    as++;
	}
	else base = 8;
    }
    else base = 10;

    /* compute the # itself */
    while(tc = *as) {
	if (!ismeta(tc, base)) return -1;
	total *= base;
	total += getmeta(tc);
	as++;
    }

    if (negative) *aval = -total;
    else *aval = total;
    return 0;
}

char *cm_mount_root="afs"; 
char *cm_slash_mount_root="/afs";
char *cm_back_slash_mount_root="\\afs";

void fs_utils_InitMountRoot()
{
    HKEY parmKey;
    char mountRoot[MAX_PATH+1];
    char *pmount=mountRoot;
    DWORD len=sizeof(mountRoot)-1;
    printf("int mountroot \n");
    if ((RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY, 0, KEY_QUERY_VALUE, &parmKey)!= ERROR_SUCCESS) 
         || (RegQueryValueEx(parmKey, "Mountroot", NULL, NULL,(LPBYTE)(mountRoot), &len)!= ERROR_SUCCESS)
         || (len==sizeof(mountRoot)-1)
         ) 
        strcpy(mountRoot, "\\afs"); 
    RegCloseKey(parmKey);
    mountRoot[len]=0;       /*safety see ms-help://MS.MSDNQTR.2002OCT.1033/sysinfo/base/regqueryvalueex.htm*/
    cm_mount_root=malloc(len+1);
    cm_slash_mount_root=malloc(len+2);
    cm_back_slash_mount_root=malloc(len+2);
    if ((*pmount=='/') || (*pmount='\\'))
        pmount++;
    strcpy(cm_mount_root,pmount);
    strcpy(cm_slash_mount_root+1,pmount);
    cm_slash_mount_root[0]='/';
    strcpy(cm_back_slash_mount_root+1,pmount);
    cm_back_slash_mount_root[0]='\\';
}

