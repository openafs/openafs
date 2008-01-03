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

RCSID
    ("$Header$");

#include <fcntl.h>
#include <errno.h>
#include <afs/vldbint.h>

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
    afsUUID uuid;
    int nentries;
    int addr;

    if ((argc != 2) || (strcmp(argv[1], "-h") == 0)) {
	printf("Usage: check_sysid <sysid_file>\n");
	exit((argc != 2) ? 1 : 0);
    }

    fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0) {
	printf("Unable to open file '%s'. Errno = %d\n", argv[1], errno);
	exit(2);
    }

    /* Read the Version Stamp */
    size = read(fd, (char *)&vs, sizeof(vs));
    if (size != sizeof(vs)) {
	printf("Unable to read versionStamp. Size = %d. Errno = %d\n", size,
	       errno);
    }
    printf("versionStamp.magic   = 0x%x %s\n", vs.magic,
	   (vs.magic == SYSIDMAGIC) ? "" : "(should be 0x88aabbc)");
    printf("versionStamp.version = 0x%x %s\n", vs.version,
	   (vs.version == SYSIDVERSION) ? "" : "(should be 0x1)");

    /* Read the uuid.
     * Look at util/uuid.c afs_uuid_create() to see how it is created.
     */
    size = read(fd, (char *)&uuid, sizeof(uuid));
    if (size != sizeof(uuid)) {
	printf("Unable to read afsUUID. Size = %d. Errno = %d\n", size,
	       errno);
	exit(3);
    }
    printf("UUID.time(hi.mid.low)= 0x%03x.%04x.%08x\n",
	   uuid.time_hi_and_version & 0x0fff, uuid.time_mid & 0xffff,
	   uuid.time_low);
    printf("UUID.version         = %d (0x%01x)\n",
	   (uuid.time_hi_and_version >> 12) & 0xf,
	   (uuid.time_hi_and_version >> 12) & 0xf);
    printf("UUID.clock(hi.low)   = 0x%02x.%02x\n",
	   uuid.clock_seq_hi_and_reserved & 0x3f, uuid.clock_seq_low & 0xff);
    printf("UUID.reserved        = %d (0x%02x)\n",
	   (uuid.clock_seq_hi_and_reserved >> 6) & 0x3,
	   (uuid.clock_seq_hi_and_reserved >> 6) & 0x3);
    printf("UUID.node            = %02x.%02x.%02x.%02x.%02x.%02x\n",
	   uuid.node[0] & 0xff, uuid.node[1] & 0xff, uuid.node[2] & 0xff,
	   uuid.node[3] & 0xff, uuid.node[4] & 0xff, uuid.node[5] & 0xff);

    /* Read the number of addresses recorded in the sysid */
    size = read(fd, (char *)&nentries, sizeof(int));
    if (size != sizeof(int)) {
	printf("Unable to read nentries. Size = %d. Errno = %d\n", size,
	       errno);
	exit(4);
    }
    printf("Number of addreses   = %d (0x%x)\n", nentries, nentries);

    /* Now read in each of the addresses */
    for (i = 0; i < nentries; i++) {
	size = read(fd, (char *)&addr, sizeof(int));
	if (size != sizeof(int)) {
	    printf("Unable to read IP Address %d. Size = %d. Errno = %d\n",
		   i + 1, size, errno);
	    exit(5);
	}
	printf("Address              = %d.%d.%d.%d (0x%x)\n",
	       (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
	       (addr) & 0xff, addr);
    }

    close(fd);
    return 0;
}
