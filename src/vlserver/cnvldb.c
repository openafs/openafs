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

#include <afs/stds.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/file.h>
#include <string.h>

#include "cnvldb.h"		/* CHANGEME! */
#include <netinet/in.h>
#include <afs/venus.h>
#include <afs/cmd.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>

#include "vlserver.h"

#define MAXSIZE 2048		/* most I'll get back from PIOCTL */
#define	BADSERVERID	255	/* XXX */


extern struct cmd_syndesc *cmd_CreateSyntax();
static char pn[] = "cnvldb";
static char tempname[] = "XXnewvldb";
static char space[MAXSIZE];
static int MaxServers[3] = { 30, 254, 254 };	/* max server # permitted in this version */

static afs_int32 Conv4to3();

static int convert_vlentry();
static int rewrite_header();

static char tspace[1024];	/* chdir can't handle anything bigger, anyway */
/* return a static pointer to a buffer */
static char *
Parent(apath)
     char *apath;
{
    register char *tp;
    strcpy(tspace, apath);
    tp = strrchr(tspace, '/');
    if (tp) {
	*tp = 0;
    } else
	strcpy(tspace, ".");
    return tspace;
}

int oldpos = 0;
int fromvers = 0, tovers = 0, showversion = 0;
afs_uint32 mhaddr;
afs_int32 dbsize;
char *pathname = NULL;
const char *dbPath;

static
handleit(as)
     struct cmd_syndesc *as;
{
    register struct cmd_item *ti;
    register afs_int32 code;
    int w, old, new, rc, dump = 0, fromv = 0;
    short uvers;
    char ubik[80];		/* space for some ubik header */
    union {
	struct vlheader_1 header1;
	struct vlheader_2 header2;
	struct vlheader_3 header3;
    } oldheader, oldheader1, newheader;	/* large enough for either */

    union {
	struct vlentry_1 entry1;
	struct vlentry_2 entry2;
	struct vlentry_3 entry3;
	char mhinfo_block[VL_ADDREXTBLK_SIZE];
    } xvlentry;

    pathname = (as->parms[2].items ? as->parms[2].items->data : dbPath);	/* -name */
    showversion = (as->parms[3].items ? 1 : 0);	/* -showversion */
    dump = (as->parms[4].items ? 1 : 0);	/* -dumpvldb */
    fromvers = (as->parms[1].items ? atoi(as->parms[1].items->data) : 0);	/* -fromversion */
    tovers = (as->parms[0].items ? atoi(as->parms[0].items->data) : 0);	/* -toversion */

    /* should stat() the old vldb, get its size, and see if there's */
    /* room for another.  It might be in AFS, so check the quota, too */
    old = open(pathname, O_RDONLY);
    if (old < 0) {
	perror(pn);
	exit(-1);
    }

    /* Read the version */
    lseek(old, 64, L_SET);
    read(old, &fromv, sizeof(int));
    fromv = ntohl(fromv);
    if ((fromv < 1) || (fromv > 4)) {
	fprintf(stderr, pn);
	fprintf(stderr, ": Unrecognized VLDB version %d.\n", fromv);
	exit(-1);
    }

    /* Sequentially read the database converting the entries as we go */
    lseek(old, 0, L_SET);
    read(old, ubik, 64);
    readheader(old, fromv, &oldheader);
    if (fromv == 1) {
	dbsize = ntohl(oldheader.header1.vital_header.eofPtr);
	fromv = ntohl(oldheader.header1.vital_header.vldbversion);
	mhaddr = 0;
    } else if (fromv == 2) {
	dbsize = ntohl(oldheader.header2.vital_header.eofPtr);
	fromv = ntohl(oldheader.header2.vital_header.vldbversion);
	mhaddr = 0;
    } else {
	int pos;

	dbsize = ntohl(oldheader.header3.vital_header.eofPtr);
	fromv = ntohl(oldheader.header3.vital_header.vldbversion);
	mhaddr = ntohl(oldheader.header3.SIT);

	/* Read the multihomed extent blocks in */
	pos = oldpos;
	read_mhentries(mhaddr, old);

	/* Position back to this after header */
	lseek(old, pos + 64, L_SET);
	oldpos = pos;
    }

    if (showversion || dump) {
	if (showversion)
	    fprintf(stdout, "%s has a version of %d\n", pathname, fromv);
	if (dump) {
	    while (oldpos < dbsize) {
		rc = readentry(old, fromv, &xvlentry);
		if ((rc == 0) || (rc == EOF))
		    break;
		printentry(fromv, &xvlentry);
	    }
	}
	exit(0);
    }

    if (!fromvers) {		/* not set */
	fromvers = fromv;
    } else if (fromvers != fromv) {
	fprintf(stdout,
		"%s has a version of %d while the -fromversion specified was %d - aborting\n",
		pathname, fromv, fromvers);
	exit(0);
    }

    if ((fromvers < 1) || (fromvers > 4)) {
	fprintf(stderr, pn);
	fprintf(stderr, ": VLDB version %d is not supported.\n", fromvers);
	fprintf(stderr, pn);
	fprintf(stderr, ": Only versions 1-4 are currently supported.\n");
	exit(-1);
    }

    if (!tovers)
	tovers = fromvers + 1;

    if (tovers < 1 || tovers > 4) {
	fprintf(stderr, pn);
	fprintf(stderr, ": VLDB version %d is not supported.\n", tovers);
	fprintf(stderr, pn);
	fprintf(stderr, ": Only versions 1 - 4 are currently supported.\n");
	exit(-1);
    }

    if (mhaddr && (tovers < 3)) {
	fprintf(stderr, pn);
	fprintf(stderr, ": Cannot convert. VLDB contains multihome info.\n");
	exit(-1);
    }

    /* OK! let's get down to business... */

    if (chdir(Parent(pathname))) {
	perror(pn);
	exit(-1);
    }

    new = open(tempname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (new < 0) {
	perror(pn);
	exit(-1);
    }

    /* Write the UBIK data */
    w = write(new, ubik, 64);
    if (w != 64) {
	printf("Write of ubik header failed %d; error %u\n", w, errno);
	exit(1);
    }

    /* Because we know that all the vldb entries are the same size and type we 
     * can just read them sequentially, fiddle with the fields, and write
     * them out again.  If we invent a vldb format that has different
     * types of entries, then we're going to have to invent new logic for
     * converting the vldb-- we'll probably have to chase down the various
     * linked lists in turn, doing lseeks and the like.
     */

    convert_header(old, new, fromvers, tovers, &oldheader, &newheader);
    while (oldpos < dbsize) {
	rc = readentry(old, fromvers, &xvlentry);
	if ((rc == 0) || (rc == EOF))
	    break;
	convert_vlentry(new, fromvers, tovers, &oldheader, &newheader,
			&xvlentry);
    }

    /* We have now finished sequentially reading and writing the database.
     * Now randomly offset into database and update multihome entries.
     */
    convert_mhentries(old, new, &newheader, fromvers, tovers);
    rewrite_header(new, tovers, &newheader);

    close(old);
    if (fsync(new)) {
	perror(pn);
	exit(-1);
    }
    close(new);

    renamefile(tempname, pathname);
    sleep(5);
    exit(0);
}


readheader(fd, version, addr)
     int fd;
     int version;
     char *addr;
{
    int hdrsize, size = 0;

    oldpos = 0;
    if (version == 1)
	hdrsize = sizeof(struct vlheader_1);
    else
	hdrsize = sizeof(struct vlheader_2);

    size = read(fd, addr, hdrsize);
    if (size > 0)
	oldpos += size;

    return;
}

readentry(fd, version, addr)
     int fd;
     int version;
     char *addr;
{
    int rc, rc1;
    struct vlentry_3 *vl3p = (struct vlentry_3 *)addr;
    int toread;

    toread =
	((version ==
	  1) ? sizeof(struct vlentry_1) : sizeof(struct vlentry_2));
    rc = read(fd, addr, toread);
    if (rc != toread)
	printf("Partial read of vlentry at pos %u: %d\n", oldpos, rc);
    if (rc > 0)
	oldpos += rc;

    /* Read a mhblock entry if there is one */
    if ((rc > 0) && (vl3p->flags == VLCONTBLOCK)) {
	if (!mhaddr)		/* Remember first mh block */
	    mhaddr = oldpos - rc;

	rc1 = read(fd, &addr[rc], VL_ADDREXTBLK_SIZE - rc);
	if (rc1 != VL_ADDREXTBLK_SIZE - rc)
	    printf("Partial read of mhblock at pos %u: %d\n", oldpos + rc,
		   rc1);
	if (rc1 > 0) {
	    oldpos += rc1;
	    rc += rc1;
	}
    }

    return rc;
}

printentry(version, addr)
     int version;
     char *addr;
{
    struct vlentry_2 *vl2p = (struct vlentry_2 *)addr;
    struct vlentry_3 *vl3p = (struct vlentry_3 *)addr;
    int i;

    /* Don't print anything if the entry is a mh info block */
    if (vl3p->flags == VLCONTBLOCK) {
	return;
    }

    if (version == 1 || version == 2) {
	printf("%s\t%5d [%10d:%10d:%10d]%8X%8d\n", vl2p->name, vl2p->spares3,
	       vl2p->volumeId[0], vl2p->volumeId[1], vl2p->volumeId[2],
	       vl2p->flags, vl2p->LockAfsId);
	printf("\t%8d%8d%8d [%7d%7d%7d]%7d% [%4d%4d%4d%4d][%4d%4d%4d%4d]\n",
	       vl2p->LockTimestamp, vl2p->cloneId, vl2p->spares0,
	       vl2p->nextIdHash[0], vl2p->nextIdHash[1], vl2p->nextIdHash[2],
	       vl2p->nextNameHash, vl2p->serverNumber[0],
	       vl2p->serverNumber[1], vl2p->serverNumber[2],
	       vl2p->serverNumber[3], vl2p->serverPartition[0],
	       vl2p->serverPartition[1], vl2p->serverPartition[2],
	       vl2p->serverPartition[3]);
	printf("\t[%4d%4d%4d%4d]\n", vl2p->serverFlags[0],
	       vl2p->serverFlags[1], vl2p->serverFlags[2],
	       vl2p->serverFlags[3]);
    } else {			/* if (version >= 3) */

	if (vl3p->flags == VLFREE)
	    return;
	printf("%s\tPos=%d NextIdHash=[%d:%d:%d] NextNameHash=%d\n",
	       vl3p->name, (oldpos - sizeof(struct vlentry_3)),
	       vl3p->nextIdHash[0], vl3p->nextIdHash[1], vl3p->nextIdHash[2],
	       vl3p->nextNameHash);
	printf("\tRW=%u RO=%u BK=%u CL=%u flags=0x%X lockBy=%d lockTime=%u\n",
	       vl3p->volumeId[0], vl3p->volumeId[1], vl3p->volumeId[2],
	       vl3p->cloneId, vl3p->flags, vl3p->LockAfsId,
	       vl3p->LockTimestamp);
	for (i = 0; i < OMAXNSERVERS; i++) {
	    if ((vl3p->serverNumber[i] & 0xff) != 0xff) {
		printf("\tServer=%d Partition=%d flags=%X\n",
		       vl3p->serverNumber[i], vl3p->serverPartition[i],
		       vl3p->serverFlags[i]);
	    }
	}
    }
    return;
}

int readmhentries = 0;
struct extentaddr *base[VL_MAX_ADDREXTBLKS];

/* Read the multihome extent blocks in. Check if they are good by
 * verifying their address is not pass the EOF and the flags are good.
 * If it's not good, then don't read the block in.
 */
read_mhentries(mh_addr, oldfd)
     int oldfd;
     afs_uint32 mh_addr;
{
    afs_uint32 sit, a;
    afs_int32 code;
    int j;

    if (readmhentries)
	return;
    readmhentries = 1;

    /* Initialize base pointers */
    for (j = 0; j < VL_MAX_ADDREXTBLKS; j++)
	base[j] = 0;

    if (!mh_addr)
	return;

    /* Check if the first extent block is beyond eof. If 
     * it is, it's not real.
     */
    if (mh_addr > dbsize - VL_ADDREXTBLK_SIZE)
	return;

    /* Now read the first mh extent block */
    code = lseek(oldfd, mh_addr + 64, L_SET);
    if (code < 0) {
	perror("seek MH block");
	exit(1);
    }
    base[0] = (struct extentaddr *)malloc(VL_ADDREXTBLK_SIZE);
    if (!base[0]) {
	perror("malloc1");
	exit(1);
    }
    code = read(oldfd, (char *)base[0], VL_ADDREXTBLK_SIZE);
    if (code != VL_ADDREXTBLK_SIZE) {
	perror("read MH block");
	free(base[0]);
	base[0] = 0;
	exit(1);
    }

    /* Verify that this block is the right one */
    if (ntohl(base[0]->ex_flags) != VLCONTBLOCK) {	/* check if flag is correct */
	free(base[0]);
	base[0] = 0;
	return;
    }

    /* The first block contains pointers to the other extent blocks.
     * Check to see if the pointers are good and read them in if they are.
     */
    a = mh_addr;
    for (j = 1; j < VL_MAX_ADDREXTBLKS; j++) {
	if (!base[0]->ex_contaddrs[j])
	    continue;

	sit = ntohl(base[0]->ex_contaddrs[j]);

	/* Every time we allocate a new extent block, it is allocated after 
	 * the previous ones. But it must be before the EOF.
	 */
	if ((sit < (a + VL_ADDREXTBLK_SIZE))
	    || (sit > dbsize - VL_ADDREXTBLK_SIZE)) {
	    continue;
	}

	/* Read the extent block in */
	sit += 64;
	code = lseek(oldfd, sit, L_SET);
	if (code < 0) {
	    perror("seek MH block");
	    exit(1);
	}
	base[j] = (struct extentaddr *)malloc(VL_ADDREXTBLK_SIZE);
	if (!base[j]) {
	    perror("malloc1");
	    exit(1);
	}
	code = read(oldfd, (char *)base[j], VL_ADDREXTBLK_SIZE);
	if (code != VL_ADDREXTBLK_SIZE) {
	    perror("read MH block");
	    exit(1);
	}

	/* Verify that this block knows its an extent block */
	if (ntohl(base[j]->ex_flags) != VLCONTBLOCK) {
	    free(base[j]);
	    base[j] = 0;
	    continue;
	}

	/* The extent block passed our tests */
	a = ntohl(base[0]->ex_contaddrs[j]);
    }
}

/* Follow the SIT pointer in the header (mhaddr) to the multihomed
 * extent blocks and verify that the pointers are good. And fix.
 * Then convert the multihomed addresses to single address if we
 * are converting back from version 4.
 * 
 * Before this can be called, the routine read_mhentries must be called.
 */
convert_mhentries(oldfd, newfd, header, fromver, tover)
     int oldfd, newfd;
     struct vlheader_2 *header;
     int fromver, tover;
{
    afs_uint32 sit;
    afs_int32 code;
    int i, j, modified = 0, w;
    afs_uint32 raddr, addr;
    struct extentaddr *exp;
    int basei, index;

    /* Check if the header says the extent block exists. If
     * it does, then read_mhentries should have read it in.
     */
    if (mhaddr && !base[0]) {
	printf("Fix Bad base extent block pointer\n");
	header->SIT = mhaddr = 0;
    } else if (mhaddr && base[0]) {

	if ((ntohl(header->SIT) != mhaddr) && (tover == 4)) {
	    printf
		("Fix pointer to first base extent block. Was 0x%x, now 0x%x\n",
		 ntohl(header->SIT), mhaddr);
	    header->SIT = htonl(mhaddr);
	}

	/* Check if the first block points to itself. If not, then fix it */
	if (ntohl(base[0]->ex_contaddrs[0]) != mhaddr) {
	    printf("Fix bad pointer in base extent block: Base 0\n");
	    base[0]->ex_contaddrs[0] = htonl(mhaddr);
	    modified = 1;
	}

	/* The first block contains pointers to the other extent blocks.
	 * Check to see if the pointers are good.
	 */
	for (j = 1; j < VL_MAX_ADDREXTBLKS; j++) {
	    /* Check if the base extent block says the extent blocks exist.
	     * If it does, then read_mhentries should have read it in.
	     */
	    if (base[0]->ex_contaddrs[j] && !base[j]) {
		printf("Fix bad pointer in base extent block: Base %d\n", j);
		base[0]->ex_contaddrs[j] = 0;
		modified = 1;
	    }
	}

	/* Now write out the base extent blocks if it changed */
	if (modified) {
	    code = lseek(newfd, mhaddr + 64, L_SET);
	    if (code < 0) {
		perror("seek MH Block");
		exit(1);
	    }
	    w = write(newfd, (char *)base[0], VL_ADDREXTBLK_SIZE);
	    if (w != VL_ADDREXTBLK_SIZE) {
		perror("write MH Block");
		exit(1);
	    }
	}
    }

    /* If we are converting from version 4 to version 3, then 
     * translate any multihome ptrs in the IpMappedAddr array
     * to true IP addresses.
     */
    if ((fromver == 4) && (tover == 3)) {
	/* Step through the fileserver addresses in the VLDB header
	 * and convert the pointers back to IP addresses.
	 */
	for (i = 0; i < 254; i++) {
	    addr = ntohl(header->IpMappedAddr[i]);
	    if (addr && ((addr & 0xff000000) == 0xff000000)) {
		basei = (addr >> 16) & 0xff;
		index = addr & 0xffff;

		if ((basei >= VL_ADDREXTBLK_SIZE) || !base[basei]) {
		    fprintf(stderr,
			    "Warning: mh entry %d has no IP address; ignored!!\n",
			    i);
		    header->IpMappedAddr[i] = 0;
		    continue;
		}
		exp = &base[basei][index];

		/* For now return the first ip address back */
		for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		    if (exp->ex_addrs[j]) {
			raddr = ntohl(exp->ex_addrs[j]);
			break;
		    }
		}
		if (j >= VL_MAXIPADDRS_PERMH) {
		    fprintf(stderr,
			    "Warning: mh entry %d has no ip address; ignored!!\n",
			    i);
		    raddr = 0;
		} else {
		    printf
			("Multi-homed addr: converting to single ip address %d.%d.%d.%d\n",
			 (raddr >> 24 & 0xff), (raddr >> 16 & 0xff),
			 (raddr >> 8 & 0xff), (raddr & 0xff));
		}
		header->IpMappedAddr[i] = htonl(raddr);
	    }
	}
	header->SIT = mhaddr = 0;	/* mhinfo block has been removed */

	/* Now step through the hash tables in header updating them.
	 * Because we removed the mh info blocks and some entries they
	 * point to may have changed position.
	 * The VolnameHash
	 */
	for (i = 0; i < 8191; i++) {
	    header->VolnameHash[i] = Conv4to3(header->VolnameHash[i]);
	}
	/* The VolidHash */
	for (i = 0; i < 3; i++) {
	    for (j = 0; j < 8191; j++) {
		header->VolidHash[i][j] = Conv4to3(header->VolidHash[i][j]);
	    }
	}

	/* Update eofptr to take into account the removal of the mhinfo blocks */
	header->vital_header.eofPtr = htonl(Conv4to3(dbsize));
    }
}


convert_header(ofd, fd, fromv, tov, fromaddr, toaddr)
     int ofd, fd, fromv, tov;
     char *fromaddr, *toaddr;
{
    struct vlheader_1 *tvp1;
    struct vlheader_2 *tvp2;
    int i, j, diff, w;

    if (fromv == 1) {
	if (tov == 1) {
	    memcpy(toaddr, fromaddr, sizeof(struct vlheader_1));
	    tvp1 = (struct vlheader_1 *)toaddr;

	    w = write(fd, tvp1, sizeof(struct vlheader_1));
	    if (w != sizeof(struct vlheader_1)) {
		printf("Write of header failed %d; error %u\n", w, errno);
		exit(1);
	    }

	    /* for garbage-collecting... */
	    for (i = 0; i < 31; i++)
		tvp1->IpMappedAddr[i] = 0;

	} else if (tov == 2 || tov == 3) {
	    tvp1 = (struct vlheader_1 *)fromaddr;
	    tvp2 = (struct vlheader_2 *)toaddr;
	    memset(tvp2, 0, sizeof(struct vlheader_2));
	    tvp2->vital_header.vldbversion = htonl(tov);
	    tvp2->vital_header.headersize = htonl(sizeof(struct vlheader_2));
	    diff =
		ntohl(tvp2->vital_header.headersize) -
		ntohl(tvp1->vital_header.headersize);
	    if (ntohl(tvp1->vital_header.freePtr))
		tvp2->vital_header.freePtr =
		    htonl(ntohl(tvp1->vital_header.freePtr) + diff);
	    if (ntohl(tvp1->vital_header.eofPtr))
		tvp2->vital_header.eofPtr =
		    htonl(ntohl(tvp1->vital_header.eofPtr) + diff);
	    tvp2->vital_header.allocs = tvp1->vital_header.allocs;
	    tvp2->vital_header.frees = tvp1->vital_header.frees;
	    tvp2->vital_header.MaxVolumeId = tvp1->vital_header.MaxVolumeId;
	    for (i = 0; i < 3; i++)
		tvp2->vital_header.totalEntries[i] =
		    tvp1->vital_header.totalEntries[i];

	    for (i = 0; i < 31; i++)
		tvp2->IpMappedAddr[i] = tvp1->IpMappedAddr[i];

	    for (i = 0; i < 8191; i++) {
		if (ntohl(tvp1->VolnameHash[i]))
		    tvp2->VolnameHash[i] =
			htonl(ntohl(tvp1->VolnameHash[i]) + diff);
	    }

	    for (i = 0; i < 3; i++) {
		for (j = 0; j < 8191; j++) {
		    if (ntohl(tvp1->VolidHash[i][j]))
			tvp2->VolidHash[i][j] =
			    htonl(ntohl(tvp1->VolidHash[i][j]) + diff);
		}
	    }

	    w = write(fd, tvp2, sizeof(struct vlheader_2));
	    if (w != sizeof(struct vlheader_2)) {
		printf("Write of header failed %d; error %u\n", w, errno);
		exit(1);
	    }

	    /* for garbage-collecting... */
	    for (i = 0; i < 31; i++)
		tvp2->IpMappedAddr[i] = 0;
	} else
	    return EINVAL;
    } else if (fromv == 2 || fromv == 3 || fromv == 4) {
	if (tov == 2 || tov == 3 || tov == 4) {
	    memcpy(toaddr, fromaddr, sizeof(struct vlheader_2));
	    tvp2 = (struct vlheader_2 *)toaddr;
	    tvp2->vital_header.vldbversion = htonl(tov);
	    w = write(fd, tvp2, sizeof(struct vlheader_2));
	    if (w != sizeof(struct vlheader_2)) {
		printf("Write of header failed %d; error %u\n", w, errno);
		exit(1);
	    }

	} else if (tov == 1) {
	    tvp2 = (struct vlheader_2 *)fromaddr;
	    tvp1 = (struct vlheader_1 *)toaddr;
	    memset(tvp1, 0, sizeof(struct vlheader_1));
	    tvp1->vital_header.vldbversion = htonl(1);
	    tvp1->vital_header.headersize = htonl(sizeof(struct vlheader_1));
	    diff =
		ntohl(tvp1->vital_header.headersize) -
		ntohl(tvp2->vital_header.headersize);
	    if (ntohl(tvp2->vital_header.freePtr))
		tvp1->vital_header.freePtr =
		    htonl(ntohl(tvp2->vital_header.freePtr) + diff);
	    if (ntohl(tvp2->vital_header.eofPtr))
		tvp1->vital_header.eofPtr =
		    htonl(ntohl(tvp2->vital_header.eofPtr) + diff);
	    tvp1->vital_header.allocs = tvp2->vital_header.allocs;
	    tvp1->vital_header.frees = tvp2->vital_header.frees;
	    tvp1->vital_header.MaxVolumeId = tvp2->vital_header.MaxVolumeId;
	    for (i = 0; i < 3; i++)
		tvp1->vital_header.totalEntries[i] =
		    tvp2->vital_header.totalEntries[i];

	    for (i = 0; i < 31; i++)
		tvp1->IpMappedAddr[i] = tvp2->IpMappedAddr[i];

	    for (i = 0; i < 8191; i++) {
		if (ntohl(tvp2->VolnameHash[i]))
		    tvp1->VolnameHash[i] =
			htonl(ntohl(tvp2->VolnameHash[i]) + diff);
	    }

	    for (i = 0; i < 3; i++) {
		for (j = 0; j < 8191; j++) {
		    if (ntohl(tvp2->VolidHash[i][j]))
			tvp1->VolidHash[i][j] =
			    htonl(ntohl(tvp2->VolidHash[i][j]) + diff);
		}
	    }

	    w = write(fd, tvp1, sizeof(struct vlheader_1));
	    if (w != sizeof(struct vlheader_2)) {
		printf("Write of header failed %d; error %u\n", w, errno);
		exit(1);
	    }

	    /* for garbage-collecting... */
	    for (i = 0; i < 31; i++)
		tvp1->IpMappedAddr[i] = 0;
	} else
	    return EINVAL;
    } else
	return EINVAL;
    return 0;
}


/* Convert an address pointer to a vlentry from version 4 to version 3.
 * This involves checking if the address is after any of the four
 * MH block and if it is, subtract the size of the MH block. 
 *
 * In going from version 4 to 3, the mh blocks go away and all entries
 * move up in their place. The adresses then need to be updated.
 *
 * Before this can be called, the routine read_mhentries must be called.
 */
static afs_int32
Conv4to3(addr)
     afs_int32 addr;
{
    afs_int32 raddr;
    int i;

    if (!base[0] || !addr)
	return (addr);

    raddr = addr;
    for (i = 0; i < VL_MAX_ADDREXTBLKS; i++) {
	if (base[i] && base[0]->ex_contaddrs[i]
	    && (addr > base[0]->ex_contaddrs[i]))
	    raddr -= VL_ADDREXTBLK_SIZE;
    }

    return (raddr);
}

/* this only works because the vlheader struct is essentially the same 
 * from version 1 to version 2 -- that is, the first bunch of fields
 * aren't any more or any larger, so they match up pretty well.
*/

static int
convert_vlentry(new, fromvers, tovers, oldheader, newheader, vlentryp)
     int new, fromvers, tovers;
     struct vlheader_1 *oldheader, *newheader;	/* close enough */
     struct vlentry_1 *vlentryp;	/* 1 and 2 are identical */
{
    int diff, i, s, w;
    struct vlentry_3 *vl3p = (struct vlentry_3 *)vlentryp;

    /* For mh information blocks,
     * If going to version 4 or greater, keep the mh info block.
     * Otherwise, don't keep it (version 3 and earlier don't have them).
     */
    if (vl3p->flags == VLCONTBLOCK) {
	if (tovers >= 4) {
	    w = write(new, vlentryp, VL_ADDREXTBLK_SIZE);
	    if (w != VL_ADDREXTBLK_SIZE) {
		printf("Write of mh info block failed %d; error %u\n", w,
		       errno);
		exit(1);
	    }
	}
	return 0;
    }

    if (fromvers == 2 && tovers == 3) {
	struct vlentry_3 vl;

	vl.volumeId[0] = vlentryp->volumeId[0];
	vl.volumeId[1] = vlentryp->volumeId[1];
	vl.volumeId[2] = vlentryp->volumeId[2];
	vl.flags = vlentryp->flags;
	vl.LockAfsId = vlentryp->LockAfsId;
	vl.LockTimestamp = vlentryp->LockTimestamp;
	vl.cloneId = vlentryp->cloneId;
	vl.nextIdHash[0] = vlentryp->nextIdHash[0];
	vl.nextIdHash[1] = vlentryp->nextIdHash[1];
	vl.nextIdHash[2] = vlentryp->nextIdHash[2];
	vl.nextNameHash = vlentryp->nextNameHash;
	memcpy(vl.name, vlentryp->name, 65);
	for (i = 0; i < 8; i++) {
	    vl.serverNumber[i] = vlentryp->serverNumber[i];
	    vl.serverPartition[i] = vlentryp->serverPartition[i];
	    vl.serverFlags[i] = vlentryp->serverFlags[i];
	}
	for (; i < 13; i++)
	    vl.serverNumber[i] = vl.serverPartition[i] = vl.serverFlags[i] =
		BADSERVERID;
	w = write(new, &vl, sizeof(struct vlentry_3));
	if (w != sizeof(struct vlentry_3)) {
	    printf("Write of entry failed %d; error %u\n", w, errno);
	    exit(1);
	}

	return 0;
    } else if (fromvers == 3 && tovers == 2) {
	struct vlentry_2 vl;
	struct vlentry_3 *xnvlentry = (struct vlentry_3 *)vlentryp;

	memset((char *)&vl, 0, sizeof(struct vlentry_2));
	vl.volumeId[0] = xnvlentry->volumeId[0];
	vl.volumeId[1] = xnvlentry->volumeId[1];
	vl.volumeId[2] = xnvlentry->volumeId[2];
	vl.flags = xnvlentry->flags;
	vl.LockAfsId = xnvlentry->LockAfsId;
	vl.LockTimestamp = xnvlentry->LockTimestamp;
	vl.cloneId = xnvlentry->cloneId;
	for (i = 0; i < 3; i++) {
	    if (ntohl(xnvlentry->nextIdHash[i]))
		vl.nextIdHash[i] = xnvlentry->nextIdHash[i];
	}
	if (ntohl(xnvlentry->nextNameHash))
	    vl.nextNameHash = xnvlentry->nextNameHash;
	memcpy(vl.name, xnvlentry->name, 65);
	for (i = 0; i < 8; i++) {
	    vl.serverNumber[i] = xnvlentry->serverNumber[i];
	    vl.serverPartition[i] = xnvlentry->serverPartition[i];
	    vl.serverFlags[i] = xnvlentry->serverFlags[i];
	}
	w = write(new, &vl, sizeof(struct vlentry_2));
	if (w != sizeof(struct vlentry_2)) {
	    printf("Write of entry failed %d; error %u\n", w, errno);
	    exit(1);
	}
	return 0;
    } else if (fromvers == 3 && tovers == 1) {
	struct vlentry_1 vl;
	struct vlentry_3 *xnvlentry = (struct vlentry_3 *)vlentryp;

	diff =
	    (tovers ==
	     1 ? sizeof(struct vlheader_1) : sizeof(struct vlheader_2))
	    - (fromvers ==
	       1 ? sizeof(struct vlheader_1) : sizeof(struct vlheader_2));
	memset((char *)&vl, 0, sizeof(struct vlentry_1));
	vl.volumeId[0] = xnvlentry->volumeId[0];
	vl.volumeId[1] = xnvlentry->volumeId[1];
	vl.volumeId[2] = xnvlentry->volumeId[2];
	vl.flags = xnvlentry->flags;
	vl.LockAfsId = xnvlentry->LockAfsId;
	vl.LockTimestamp = xnvlentry->LockTimestamp;
	vl.cloneId = xnvlentry->cloneId;
	for (i = 0; i < 3; i++) {
	    if (ntohl(xnvlentry->nextIdHash[i]))
		vl.nextIdHash[i] =
		    htonl(ntohl(xnvlentry->nextIdHash[i]) + diff);
	}
	if (ntohl(xnvlentry->nextNameHash))
	    vl.nextNameHash = htonl(ntohl(xnvlentry->nextNameHash) + diff);

	memcpy(vl.name, xnvlentry->name, 65);
	for (i = 0; i < 8; i++) {
	    vl.serverNumber[i] = xnvlentry->serverNumber[i];
	    vl.serverPartition[i] = xnvlentry->serverPartition[i];
	    vl.serverFlags[i] = xnvlentry->serverFlags[i];
	}
	for (i = 0; i < 8; i++) {
	    s = xnvlentry->serverNumber[i];
	    if (s != 255) {
		if (s > MaxServers[tovers - 1]) {
		    fprintf(stderr,
			    "%s: Too Many Servers (%d) for this version!\n",
			    pn, s + 1);
		    exit(-1);
		} else
		    newheader->IpMappedAddr[s] = oldheader->IpMappedAddr[s];
	    }
	}
	w = write(new, &vl, sizeof(struct vlentry_1));
	if (w != sizeof(struct vlentry_1)) {
	    printf("Write of entry failed %d; error %u\n", w, errno);
	    exit(1);
	}
	return 0;
    } else if (fromvers == 4 && tovers == 3) {
	struct vlentry_3 vl;
	/* We are converting from version 4 to 3. In this conversion, mh info
	 * blocks go away and all vlentries after them move up in the vldb file.
	 * When this happens, the linked list pointers need to be updated.
	 */
	memcpy(&vl, vlentryp, sizeof(vl));
	for (i = 0; i < 3; i++) {
	    vl.nextIdHash[i] = Conv4to3(vl.nextIdHash[i]);
	}
	vl.nextNameHash = Conv4to3(vl.nextNameHash);

	w = write(new, &vl, sizeof(vl));
	if (w != sizeof(vl)) {
	    printf("Write of entry failed %d; error %u\n", w, errno);
	    exit(1);
	}
	return 0;
    }

    if (tovers == 1) {
	w = write(new, vlentryp, sizeof(struct vlentry_1));
	if (w != sizeof(struct vlentry_1)) {
	    printf("Write of entry failed %d; error %u\n", w, errno);
	    exit(1);
	}
    } else if (tovers == 2) {
	w = write(new, vlentryp, sizeof(struct vlentry_2));
	if (w != sizeof(struct vlentry_2)) {
	    printf("Write of entry failed %d; error %u\n", w, errno);
	    exit(1);
	}
    } else if (tovers == 3 || tovers == 4) {
	w = write(new, vlentryp, sizeof(struct vlentry_3));
	if (w != sizeof(struct vlentry_3)) {
	    printf("Write of entry failed %d; error %u\n", w, errno);
	    exit(1);
	}
    } else {
	perror(pn);
	fprintf(stderr,
		"Skipping vlentry write - db corrupted - bad toversion %d\n",
		tovers);
    }

    return;
}

static int
rewrite_header(new, tovers, newheader)
     int new, tovers;
     char *newheader;
{
    int pos, w, towrite;

    pos = lseek(new, 64, L_SET);	/* leave room for ubik */
    if (pos == -1) {
	perror(pn);
	fprintf(stderr, "%s: no garbage colection\n", pn);
	return;
    } else if (pos != 64) {
	fprintf(stderr, "%s: Can't rewind: no garbage collection\n", pn);
	return;
    }

    towrite =
	((tovers ==
	  1) ? sizeof(struct vlheader_1) : sizeof(struct vlheader_2));
    w = write(new, newheader, towrite);
    if (w != towrite) {
	printf("Write of entry failed %d; error %u\n", w, errno);
	exit(1);
    }

    return;
}


#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register struct cmd_syndesc *ts;
    afs_int32 code;

    ts = cmd_CreateSyntax("initcmd", handleit, 0, "optional");
    cmd_AddParm(ts, "-to", CMD_SINGLE, CMD_OPTIONAL, "goal version");
    cmd_AddParm(ts, "-from", CMD_SINGLE, CMD_OPTIONAL, "current version");
    cmd_AddParm(ts, "-path", CMD_SINGLE, CMD_OPTIONAL, "pathname");
    cmd_AddParm(ts, "-showversion", CMD_FLAG, CMD_OPTIONAL,
		"Just display version of current vldb");
    cmd_AddParm(ts, "-dumpvldb", CMD_FLAG, CMD_OPTIONAL,
		"display all vldb entries");

#ifdef DEBUG
    cmd_AddParm(ts, "-noGC", CMD_FLAG, CMD_OPTIONAL,
		"Don't do garbage collection");
#endif /* DEBUG */

    dbPath = AFSDIR_SERVER_VLDB_FILEPATH;

    code = cmd_Dispatch(argc, argv);
    exit(code);
}
