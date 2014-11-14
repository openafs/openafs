/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*  check_sysid.c     - Verify and display the sysid file.               */
/*  Date: 10/21/97                                                       */
/*                                                                       */
/* ********************************************************************* */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/vldbint.h>
#include <opr/uuid.h>

#define SYSIDMAGIC      0x88aabbcc
#define SYSIDVERSION    1

struct versionStamp {		/* Stolen from <afs/volume.h> */
    int magic;
    int version;
};

int
main(int argc, char **argv)
{
    int fd, size, i;

    struct versionStamp vs;
    int code;
    opr_uuid_t uuid;
    int nentries;
    int addr;
    char *buffer = NULL;

    if ((argc != 2) || (strcmp(argv[1], "-h") == 0)) {
	printf("Usage: check_sysid <sysid_file>\n");
	exit((argc != 2) ? 1 : 0);
    }

    fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0) {
	printf("Unable to open file '%s'. Errno = %d\n", argv[1], errno);
	exit(2);
    }

    /* Read the Version Stamp. The magic and version fields are stored
     * in host byte order. */
    size = read(fd, (char *)&vs, sizeof(vs));
    if (size != sizeof(vs)) {
	printf("Unable to read versionStamp. Size = %d. Errno = %d\n", size,
	       errno);
    }
    printf("versionStamp.magic   = 0x%x %s\n", vs.magic,
	   (vs.magic == SYSIDMAGIC) ? "" : "(should be 0x88aabbc)");
    printf("versionStamp.version = 0x%x %s\n", vs.version,
	   (vs.version == SYSIDVERSION) ? "" : "(should be 0x1)");

    /* Read the uuid. Portions of the uuid are stored in network
     * byte order. */
    size = read(fd, (char *)&uuid, sizeof(uuid));
    if (size != sizeof(uuid)) {
	printf("Unable to read afsUUID. Size = %d. Errno = %d\n", size,
	       errno);
	exit(3);
    }

    code = opr_uuid_toString(&uuid, &buffer);
    if (code != 0) {
        printf("Unable to format uuid string. code=%d\n", code);
        exit(6);
    }
    printf("UUID                 = %s\n", buffer);
    opr_uuid_freeString(buffer);

    /* Read the number of addresses recorded in the sysid.
     * nentries is stored in host byte order. */
    size = read(fd, (char *)&nentries, sizeof(int));
    if (size != sizeof(int)) {
	printf("Unable to read nentries. Size = %d. Errno = %d\n", size,
	       errno);
	exit(4);
    }
    printf("Number of addreses   = %d (0x%x)\n", nentries, nentries);

    /* Now read in each of the addresses.
     * Each address is stored in network byte order. */
    for (i = 0; i < nentries; i++) {
	size = read(fd, (char *)&addr, sizeof(int));
	if (size != sizeof(int)) {
	    printf("Unable to read IP Address %d. Size = %d. Errno = %d\n",
		   i + 1, size, errno);
	    exit(5);
	}
	addr = ntohl(addr);
	printf("Address              = %d.%d.%d.%d (0x%x)\n",
	       (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
	       (addr) & 0xff, addr);
    }

    close(fd);
    return 0;
}
