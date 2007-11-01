/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/pthread_glock.h>
#include <afs/afsutil.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <rx/rxkad.h>
#include "cellconfig.h"
#include "keys.h"

/* write ThisCell and CellServDB containing exactly one cell's info specified
    by acellInfo parm.   Useful only on the server (which describes only one cell).
*/

static int
VerifyEntries(register struct afsconf_cell *aci)
{
    register int i;
    register struct hostent *th;

    for (i = 0; i < aci->numServers; i++) {
	if (aci->hostAddr[i].sin_addr.s_addr == 0) {
	    /* no address spec'd */
	    if (*(aci->hostName[i]) != 0) {
		th = gethostbyname(aci->hostName[i]);
		if (!th) {
		    printf("Host %s not found in host database...\n",
			   aci->hostName[i]);
		    return AFSCONF_FAILURE;
		}
		memcpy(&aci->hostAddr[i].sin_addr, th->h_addr,
		       sizeof(afs_int32));
	    }
	    /* otherwise we're deleting this entry */
	} else {
	    /* address spec'd, perhaps no name known */
	    if (aci->hostName[i][0] != 0)
		continue;	/* name known too */
	    /* figure out name, if possible */
	    th = gethostbyaddr((char *)(&aci->hostAddr[i].sin_addr), 4,
			       AF_INET);
	    if (!th) {
		strcpy(aci->hostName[i], "UNKNOWNHOST");
	    } else {
		strcpy(aci->hostName[i], th->h_name);
	    }
	}
    }
    return 0;
}

/* Changed the interface to accept the afsconf_dir datastructure.
   This is a handle to the internal cache that is maintained by the bosserver.
   */

int
afsconf_SetCellInfo(struct afsconf_dir *adir, const char *apath, 
		    struct afsconf_cell *acellInfo)
{
    afs_int32 code;

    code = afsconf_SetExtendedCellInfo(adir, apath, acellInfo, NULL);
    return code;
}

int
afsconf_SetExtendedCellInfo(struct afsconf_dir *adir, 
			    const char *apath, 
			    struct afsconf_cell *acellInfo, char clones[])
{
    register afs_int32 code;
    register int fd;
    char tbuffer[1024];
    register FILE *tf;
    register afs_int32 i;

    LOCK_GLOBAL_MUTEX;
    /* write ThisCell file */
    strcompose(tbuffer, 1024, apath, "/", AFSDIR_THISCELL_FILE, NULL);

    fd = open(tbuffer, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
	UNLOCK_GLOBAL_MUTEX;
	return errno;
    }
    i = strlen(acellInfo->name);
    code = write(fd, acellInfo->name, i);
    if (code != i) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    if (close(fd) < 0) {
	UNLOCK_GLOBAL_MUTEX;
	return errno;
    }

    /* make sure we have both name and address for each host, looking up other
     * if need be */
    code = VerifyEntries(acellInfo);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    /* write CellServDB */
    strcompose(tbuffer, 1024, apath, "/", AFSDIR_CELLSERVDB_FILE, NULL);
    tf = fopen(tbuffer, "w");
    if (!tf) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_NOTFOUND;
    }
    fprintf(tf, ">%s	#Cell name\n", acellInfo->name);
    for (i = 0; i < acellInfo->numServers; i++) {
	code = acellInfo->hostAddr[i].sin_addr.s_addr;	/* net order */
	if (code == 0)
	    continue;		/* delete request */
	code = ntohl(code);	/* convert to host order */
	if (clones && clones[i])
	    fprintf(tf, "[%d.%d.%d.%d]  #%s\n", (code >> 24) & 0xff,
		    (code >> 16) & 0xff, (code >> 8) & 0xff, code & 0xff,
		    acellInfo->hostName[i]);
	else
	    fprintf(tf, "%d.%d.%d.%d    #%s\n", (code >> 24) & 0xff,
		    (code >> 16) & 0xff, (code >> 8) & 0xff, code & 0xff,
		    acellInfo->hostName[i]);
    }
    if (ferror(tf)) {
	fclose(tf);
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    code = fclose(tf);

    /* Reset the timestamp in the cache, so that
     * the CellServDB is read into the cache next time.
     * Resolves the lost update problem due to an inconsistent cache
     */
    if (adir)
	adir->timeRead = 0;

    UNLOCK_GLOBAL_MUTEX;
    if (code == EOF)
	return AFSCONF_FAILURE;
    return 0;
}
