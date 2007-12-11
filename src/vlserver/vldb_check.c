/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Read a VLDB file and verify it for correctness */

#define VL  0x001		/* good volume entry */
#define FR  0x002		/* free volume entry */
#define MH  0x004		/* multi-homed entry */

#define RWH 0x010		/* on rw hash chain */
#define ROH 0x020		/* on ro hash chain */
#define BKH 0x040		/* on bk hash chain */
#define NH  0x080		/* on name hash chain */

#define MHC 0x100		/* on multihomed chain */
#define FRC 0x200		/* on free chain */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#include "vlserver.h"
#include "vldbint.h"
#include <ubik.h>
#include <afs/afsutil.h>
#include <afs/cmd.h>

int fd;
int listentries, listservers, listheader, listuheader, verbose;

struct er {
    long addr;
    int type;
} *record;
int serveraddrs[MAXSERVERID + 2];


#define HDRSIZE 64
int
readUbikHeader()
{
    int offset, r;
    struct ubik_hdr uheader;

    offset = lseek(fd, 0, 0);
    if (offset != 0) {
	printf("error: lseek to 0 failed: %d %d\n", offset, errno);
	return (-1);
    }

    /* now read the info */
    r = read(fd, &uheader, sizeof(uheader));
    if (r != sizeof(uheader)) {
	printf("error: read of %d bytes failed: %d %d\n", sizeof(uheader), r,
	       errno);
	return (-1);
    }

    uheader.magic = ntohl(uheader.magic);
    uheader.size = ntohl(uheader.size);
    uheader.version.epoch = ntohl(uheader.version.epoch);
    uheader.version.counter = ntohl(uheader.version.counter);

    if (listuheader) {
	printf("Ubik Header\n");
	printf("   Magic           = 0x%x\n", uheader.magic);
	printf("   Size            = %u\n", uheader.size);
	printf("   Version.epoch   = %u\n", uheader.version.epoch);
	printf("   Version.counter = %u\n", uheader.version.counter);
    }

    if (uheader.size != HDRSIZE)
	printf("Ubik header size is %u (should be %u)\n", uheader.size,
	       HDRSIZE);
    if (uheader.magic != UBIK_MAGIC)
	printf("Ubik header magic is 0x%x (should be 0x%x)\n", uheader.magic,
	       UBIK_MAGIC);

    return (0);
}

int
vldbread(position, buffer, size)
     int position;
     char *buffer;
     int size;
{
    int offset, r, p;

    /* seek to the correct spot. skip ubik stuff */
    p = position + HDRSIZE;
    offset = lseek(fd, p, 0);
    if (offset != p) {
	printf("error: lseek to %d failed: %d %d\n", p, offset, errno);
	return (-1);
    }

    /* now read the info */
    r = read(fd, buffer, size);
    if (r != size) {
	printf("error: read of %d bytes failed: %d %d\n", size, r, errno);
	return (-1);
    }
    return (0);
}

char *
vtype(type)
     int type;
{
    static char Type[3];

    if (type == 0)
	strcpy(Type, "rw");
    else if (type == 1)
	strcpy(Type, "ro");
    else if (type == 2)
	strcpy(Type, "bk");
    else
	strcpy(Type, "??");
    return (Type);
}

afs_int32
NameHash(volname)
     char *volname;
{
    unsigned int hash;
    char *vchar;

    hash = 0;
    for (vchar = volname + strlen(volname) - 1; vchar >= volname; vchar--)
	hash = (hash * 63) + (*((unsigned char *)vchar) - 63);
    return (hash % HASHSIZE);
}

afs_int32
IdHash(volid)
     afs_int32 volid;
{
    return ((abs(volid)) % HASHSIZE);
}

#define LEGALCHARS ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
int
InvalidVolname(volname)
     char *volname;
{
    char *map;
    size_t slen;

    map = LEGALCHARS;
    slen = strlen(volname);
    if (slen >= VL_MAXNAMELEN)
	return 1;
    return (slen != strspn(volname, map));
}

readheader(headerp)
     struct vlheader *headerp;
{
    int i, j;

    vldbread(0, headerp, sizeof(*headerp));

    headerp->vital_header.vldbversion =
	ntohl(headerp->vital_header.vldbversion);
    headerp->vital_header.headersize =
	ntohl(headerp->vital_header.headersize);
    headerp->vital_header.freePtr = ntohl(headerp->vital_header.freePtr);
    headerp->vital_header.eofPtr = ntohl(headerp->vital_header.eofPtr);
    headerp->vital_header.allocs = ntohl(headerp->vital_header.allocs);
    headerp->vital_header.frees = ntohl(headerp->vital_header.frees);
    headerp->vital_header.MaxVolumeId =
	ntohl(headerp->vital_header.MaxVolumeId);
    headerp->vital_header.totalEntries[0] =
	ntohl(headerp->vital_header.totalEntries[0]);
    for (i = 0; i < MAXTYPES; i++)
	headerp->vital_header.totalEntries[i] =
	    ntohl(headerp->vital_header.totalEntries[1]);

    headerp->SIT = ntohl(headerp->SIT);
    for (i = 0; i < MAXSERVERID; i++)
	headerp->IpMappedAddr[i] = ntohl(headerp->IpMappedAddr[i]);
    for (i = 0; i < HASHSIZE; i++)
	headerp->VolnameHash[i] = ntohl(headerp->VolnameHash[i]);
    for (i = 0; i < MAXTYPES; i++)
	for (j = 0; j < HASHSIZE; j++)
	    headerp->VolidHash[i][j] = ntohl(headerp->VolidHash[i][j]);

    if (listheader) {
	printf("vldb header\n");
	printf("   vldbversion      = %u\n",
	       headerp->vital_header.vldbversion);
	printf("   headersize       = %u [actual=%u]\n",
	       headerp->vital_header.headersize, sizeof(*headerp));
	printf("   freePtr          = 0x%x\n", headerp->vital_header.freePtr);
	printf("   eofPtr           = %u\n", headerp->vital_header.eofPtr);
	printf("   allocblock calls = %10u\n", headerp->vital_header.allocs);
	printf("   freeblock  calls = %10u\n", headerp->vital_header.frees);
	printf("   MaxVolumeId      = %u\n",
	       headerp->vital_header.MaxVolumeId);
	printf("   rw vol entries   = %u\n",
	       headerp->vital_header.totalEntries[0]);
	printf("   ro vol entries   = %u\n",
	       headerp->vital_header.totalEntries[1]);
	printf("   bk vol entries   = %u\n",
	       headerp->vital_header.totalEntries[2]);
	printf("   multihome info   = 0x%x (%u)\n", headerp->SIT,
	       headerp->SIT);
	printf("   server ip addr   table: size = %d entries\n",
	       MAXSERVERID + 1);
	printf("   volume name hash table: size = %d buckets\n", HASHSIZE);
	printf("   volume id   hash table: %d tables with %d buckets each\n",
	       MAXTYPES, HASHSIZE);
    }

    /* Check the header size */
    if (headerp->vital_header.headersize != sizeof(*headerp))
	printf("Header reports its size as %d (should be %d)\n",
	       headerp->vital_header.headersize, sizeof(*headerp));
    return;
}

readMH(addr, mhblockP)
     afs_int32 addr;
     struct extentaddr *mhblockP;
{
    int i, j;
    struct extentaddr *e;

    vldbread(addr, mhblockP, VL_ADDREXTBLK_SIZE);

    mhblockP->ex_count = ntohl(mhblockP->ex_count);
    mhblockP->ex_flags = ntohl(mhblockP->ex_flags);
    for (i = 0; i < VL_MAX_ADDREXTBLKS; i++)
	mhblockP->ex_contaddrs[i] = ntohl(mhblockP->ex_contaddrs[i]);

    for (i = 1; i < VL_MHSRV_PERBLK; i++) {
	e = &(mhblockP[i]);

	/* won't convert hostuuid */
	e->ex_uniquifier = ntohl(e->ex_uniquifier);
	for (j = 0; j < VL_MAXIPADDRS_PERMH; j++)
	    e->ex_addrs[j] = ntohl(e->ex_addrs[j]);
    }
    return;
}

readentry(addr, vlentryp, type)
     afs_int32 addr;
     struct nvlentry *vlentryp;
     afs_int32 *type;
{
    int i;

    vldbread(addr, vlentryp, sizeof(*vlentryp));

    for (i = 0; i < MAXTYPES; i++)
	vlentryp->volumeId[i] = ntohl(vlentryp->volumeId[i]);
    vlentryp->flags = ntohl(vlentryp->flags);
    vlentryp->LockAfsId = ntohl(vlentryp->LockAfsId);
    vlentryp->LockTimestamp = ntohl(vlentryp->LockTimestamp);
    vlentryp->cloneId = ntohl(vlentryp->cloneId);
    for (i = 0; i < MAXTYPES; i++)
	vlentryp->nextIdHash[i] = ntohl(vlentryp->nextIdHash[i]);
    vlentryp->nextNameHash = ntohl(vlentryp->nextNameHash);
    for (i = 0; i < NMAXNSERVERS; i++) {
	vlentryp->serverNumber[i] = ntohl(vlentryp->serverNumber[i]);
	vlentryp->serverPartition[i] = ntohl(vlentryp->serverPartition[i]);
	vlentryp->serverFlags[i] = ntohl(vlentryp->serverFlags[i]);
    }

    if (vlentryp->flags == VLCONTBLOCK) {
	*type = MH;
    } else if (vlentryp->flags == VLFREE) {
	*type = FR;
    } else {
	*type = VL;
    }

    if (listentries) {
	printf("address %u: ", addr);
	if (vlentryp->flags == VLCONTBLOCK) {
	    printf("mh extension block\n");
	} else if (vlentryp->flags == VLFREE) {
	    printf("free vlentry\n");
	} else {
	    printf("vlentry %s\n", vlentryp->name);
	    printf("   rw id = %u ; ro id = %u ; bk id = %u\n",
		   vlentryp->volumeId[0], vlentryp->volumeId[1],
		   vlentryp->volumeId[2]);
	    printf("   flags         =");
	    if (vlentryp->flags & VLF_RWEXISTS)
		printf(" rw");
	    if (vlentryp->flags & VLF_ROEXISTS)
		printf(" ro");
	    if (vlentryp->flags & VLF_BACKEXISTS)
		printf(" bk");
	    if (vlentryp->flags & 0xffff8fff)
		printf(" errorflag(0x%x)", vlentryp->flags);
	    printf("\n");
	    printf("   LockAfsId     = %d\n", vlentryp->LockAfsId);
	    printf("   LockTimestamp = %d\n", vlentryp->LockTimestamp);
	    printf("   cloneId       = %u\n", vlentryp->cloneId);
	    printf
		("   next hash for rw = %u ; ro = %u ; bk = %u ; name = %u\n",
		 vlentryp->nextIdHash[0], vlentryp->nextIdHash[1],
		 vlentryp->nextIdHash[2], vlentryp->nextNameHash);
	    for (i = 0; i < NMAXNSERVERS; i++) {
		if (vlentryp->serverNumber[i] != 255) {
		    printf("   server %d ; partition %d ; flags =",
			   vlentryp->serverNumber[i],
			   vlentryp->serverPartition[i]);
		    if (vlentryp->serverFlags[i] & VLSF_RWVOL)
			printf(" rw");
		    if (vlentryp->serverFlags[i] & VLSF_ROVOL)
			printf(" ro");
		    if (vlentryp->serverFlags[i] & VLSF_BACKVOL)
			printf(" bk");
		    if (vlentryp->serverFlags[i] & VLSF_NEWREPSITE)
			printf(" newro");
		    printf("\n");
		}
	    }
	}
    }
    return;
}

void
readSIT(base, addr)
     int base;
     int addr;
{
    int i, j, a;
    char sitbuf[VL_ADDREXTBLK_SIZE];
    struct extentaddr *extent;

    if (!addr)
	return;
    vldbread(addr, sitbuf, VL_ADDREXTBLK_SIZE);
    extent = (struct extentaddr *)sitbuf;

    printf("multihome info block: base %d\n", base);
    if (base == 0) {
	printf("   count = %u\n", ntohl(extent->ex_count));
	printf("   flags = %u\n", ntohl(extent->ex_flags));
	for (i = 0; i < VL_MAX_ADDREXTBLKS; i++) {
	    printf("   contaddrs[%d] = %u\n", i,
		   ntohl(extent->ex_contaddrs[i]));
	}
    }
    for (i = 1; i < VL_MHSRV_PERBLK; i++) {
	/* should we skip this entry */
	for (j = 0; j < VL_MAX_ADDREXTBLKS; j++) {
	    if (extent[i].ex_addrs[j])
		break;
	}
	if (j >= VL_MAX_ADDREXTBLKS)
	    continue;

	printf("   base %d index %d:\n", base, i);

	printf("       afsuuid    = (%x %x %x /%d/%d/ /%x/%x/%x/%x/%x/%x/)\n",
	       ntohl(extent[i].ex_hostuuid.time_low),
	       ntohl(extent[i].ex_hostuuid.time_mid),
	       ntohl(extent[i].ex_hostuuid.time_hi_and_version),
	       ntohl(extent[i].ex_hostuuid.clock_seq_hi_and_reserved),
	       ntohl(extent[i].ex_hostuuid.clock_seq_low),
	       ntohl(extent[i].ex_hostuuid.node[0]),
	       ntohl(extent[i].ex_hostuuid.node[1]),
	       ntohl(extent[i].ex_hostuuid.node[2]),
	       ntohl(extent[i].ex_hostuuid.node[3]),
	       ntohl(extent[i].ex_hostuuid.node[4]),
	       ntohl(extent[i].ex_hostuuid.node[5]));
	printf("       uniquifier = %u\n", ntohl(extent[i].ex_uniquifier));
	for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
	    a = ntohl(extent[i].ex_addrs[j]);
	    if (a) {
		printf("       %d.%d.%d.%d\n", (a >> 24) & 0xff,
		       (a >> 16) & 0xff, (a >> 8) & 0xff, (a) & 0xff);
	    }
	}
    }
}

/*
 * Read each entry in the database:
 * Record what type of entry it is and its address in the record array.
 * Remember what the maximum volume id we found is and check against the header.
 */
void
ReadAllEntries(header)
     struct vlheader *header;
{
    afs_int32 type, rindex, i, j, e;
    int freecount = 0, mhcount = 0, vlcount = 0;
    int rwcount = 0, rocount = 0, bkcount = 0;
    struct nvlentry vlentry;
    afs_uint32 addr, entrysize, maxvolid = 0;

    if (verbose)
	printf("Read each entry in the database\n");
    for (addr = header->vital_header.headersize;
	 addr < header->vital_header.eofPtr; addr += entrysize) {

	/* Remember the highest volume id */
	readentry(addr, &vlentry, &type);
	if (type == VL) {
	    if (!(vlentry.flags & VLF_RWEXISTS))
		printf("WARNING: VLDB entry '%s' has no RW volume\n",
		       vlentry.name);

	    for (i = 0; i < MAXTYPES; i++)
		if (maxvolid < vlentry.volumeId[i])
		    maxvolid = vlentry.volumeId[i];

	    e = 1;
	    for (j = 0; j < NMAXNSERVERS; j++) {
		if (vlentry.serverNumber[j] == 255)
		    continue;
		if (vlentry.serverFlags[j] & (VLSF_ROVOL | VLSF_NEWREPSITE)) {
		    rocount++;
		    continue;
		}
		if (vlentry.serverFlags[j] & VLSF_RWVOL) {
		    rwcount++;
		    if (vlentry.flags & VLF_BACKEXISTS)
			bkcount++;
		    continue;
		}
		if (e) {
		    printf
			("VLDB entry '%s' contains an unknown RW/RO index serverFlag\n",
			 vlentry.name);
		    e = 0;
		}
		printf
		    ("   index %d : serverNumber %d : serverPartition %d : serverFlag %d\n",
		     j, vlentry.serverNumber[j], vlentry.serverPartition[j],
		     vlentry.serverFlags[j]);
	    }
	}

	rindex = addr / sizeof(vlentry);
	if (record[rindex].type) {
	    printf("INTERNAL ERROR: record holder %d already in use\n",
		   rindex);
	    return;
	}
	record[rindex].addr = addr;
	record[rindex].type = type;

	/* Determine entrysize and keep count */
	if (type == VL) {
	    entrysize = sizeof(vlentry);
	    vlcount++;
	} else if (type == FR) {
	    entrysize = sizeof(vlentry);
	    freecount++;
	} else if (type == MH) {
	    entrysize = VL_ADDREXTBLK_SIZE;
	    mhcount++;
	} else {
	    printf("Unknown entry at %u\n", addr);
	}
    }
    if (verbose) {
	printf("Found %d entries, %d free entries, %d multihomed blocks\n",
	       vlcount, freecount, mhcount);
	printf("Found %d RW volumes, %d BK volumes, %d RO volumes\n", rwcount,
	       bkcount, rocount);
    }

    /* Check the maxmimum volume id in the header */
    if (maxvolid != header->vital_header.MaxVolumeId - 1)
	printf
	    ("Header's maximum volume id is %u and largest id found in VLDB is %u\n",
	     header->vital_header.MaxVolumeId, maxvolid);
}


/*
 * Follow each Name hash bucket marking it as read in the record array.
 * Record we found it in the name hash within the record array.
 * Check that the name is hashed correctly.
 */
FollowNameHash(header)
     struct vlheader *header;
{
    int count = 0, longest = 0, shortest = -1, chainlength;
    struct nvlentry vlentry;
    afs_uint32 addr;
    afs_int32 i, type, rindex;

    /* Now follow the Name Hash Table */
    if (verbose)
	printf("Check Volume Name Hash\n");
    for (i = 0; i < HASHSIZE; i++) {
	chainlength = 0;
	for (addr = header->VolnameHash[i]; addr; addr = vlentry.nextNameHash) {
	    readentry(addr, &vlentry, &type);
	    if (type != VL) {
		printf("Name Hash %d: Bad entry at %u: Not a valid vlentry\n",
		       i, addr);
		continue;
	    }

	    rindex = addr / sizeof(vlentry);

	    if (record[rindex].addr != addr && record[rindex].addr) {
		printf
		    ("INTERNAL ERROR: addresses %u and %u use same record slot %d\n",
		     record[rindex].addr, addr, rindex);
	    }
	    if (record[rindex].type & NH) {
		printf
		    ("Name Hash %d: Bad entry '%s': Already in the name hash\n",
		     i, vlentry.name);
		break;
	    }
	    record[rindex].type |= NH;

	    chainlength++;
	    count++;

	    /* Hash the name and check if in correct hash table */
	    if (NameHash(vlentry.name) != i) {
		printf
		    ("Name Hash %d: Bad entry '%s': Incorrect name hash chain (should be in %d)\n",
		     i, vlentry.name, NameHash(vlentry.name));
	    }
	}
	if (chainlength > longest)
	    longest = chainlength;
	if ((shortest == -1) || (chainlength < shortest))
	    shortest = chainlength;
    }
    if (verbose) {
	printf
	    ("%d entries in name hash, longest is %d, shortest is %d, average length is %f\n",
	     count, longest, shortest, ((float)count / (float)HASHSIZE));
    }
    return;
}

/*
 * Follow the ID hash chains for the RW, RO, and BK hash tables.
 * Record we found it in the id hash within the record array.
 * Check that the ID is hashed correctly.
 */
FollowIdHash(header)
     struct vlheader *header;
{
    int count = 0, longest = 0, shortest = -1, chainlength;
    struct nvlentry vlentry;
    afs_uint32 addr;
    afs_int32 i, j, hash, type, rindex;

    /* Now follow the RW, RO, and BK Hash Tables */
    if (verbose)
	printf("Check RW, RO, and BK id Hashes\n");
    for (i = 0; i < MAXTYPES; i++) {
	hash = ((i == 0) ? RWH : ((i == 1) ? ROH : BKH));
	count = longest = 0;
	shortest = -1;

	for (j = 0; j < HASHSIZE; j++) {
	    chainlength = 0;
	    for (addr = header->VolidHash[i][j]; addr;
		 addr = vlentry.nextIdHash[i]) {
		readentry(addr, &vlentry, &type);
		if (type != VL) {
		    printf
			("%s Id Hash %d: Bad entry at %u: Not a valid vlentry\n",
			 vtype(i), j, addr);
		    continue;
		}

		rindex = addr / sizeof(vlentry);
		if (record[rindex].addr != addr && record[rindex].addr) {
		    printf
			("INTERNAL ERROR: addresses %u and %u use same record slot %d\n",
			 record[rindex].addr, addr, rindex);
		}
		if (record[rindex].type & hash) {
		    printf
			("%s Id Hash %d: Bad entry '%s': Already in the the hash table\n",
			 vtype(i), j, vlentry.name);
		    break;
		}
		record[rindex].type |= hash;

		chainlength++;
		count++;

		/* Hash the id and check if in correct hash table */
		if (IdHash(vlentry.volumeId[i]) != j) {
		    printf
			("%s Id Hash %d: Bad entry '%s': Incorrect Id hash chain (should be in %d)\n",
			 vtype(i), j, vlentry.name,
			 IdHash(vlentry.volumeId[i]));
		}
	    }

	    if (chainlength > longest)
		longest = chainlength;
	    if ((shortest == -1) || (chainlength < shortest))
		shortest = chainlength;
	}
	if (verbose) {
	    printf
		("%d entries in %s hash, longest is %d, shortest is %d, average length is %f\n",
		 count, vtype(i), longest, shortest,
		 ((float)count / (float)HASHSIZE));
	}
    }
    return;
}

/*
 * Follow the free chain.
 * Record we found it in the free chain within the record array.
 */
FollowFreeChain(header)
     struct vlheader *header;
{
    afs_int32 count = 0;
    struct nvlentry vlentry;
    afs_uint32 addr;
    afs_int32 type, rindex;

    /* Now follow the Free Chain */
    if (verbose)
	printf("Check Volume Free Chain\n");
    for (addr = header->vital_header.freePtr; addr;
	 addr = vlentry.nextIdHash[0]) {
	readentry(addr, &vlentry, &type);
	if (type != FR) {
	    printf
		("Free Chain %d: Bad entry at %u: Not a valid free vlentry (0x%x)\n",
		 count, addr, type);
	    continue;
	}

	rindex = addr / sizeof(vlentry);
	if (record[rindex].addr != addr && record[rindex].addr) {
	    printf
		("INTERNAL ERROR: addresses %u and %u use same record slot %d\n",
		 record[rindex].addr, addr, rindex);
	}
	if (record[rindex].type & FRC) {
	    printf("Free Chain: Bad entry at %u: Already in the free chain\n",
		   addr);
	    break;
	}
	record[rindex].type |= FRC;

	count++;
    }
    if (verbose)
	printf("%d entries on free chain\n", count);
    return;
}

/*
 * Read each multihomed block and mark it as found in the record.
 * Read each entry in each multihomed block and mark the serveraddrs
 * array with the number of ip addresses found for this entry.
 * 
 * Then read the IpMappedAddr array in the header.
 * Verify that multihomed entries base and index are valid and points to
 * a good multhomed entry.
 * Mark the serveraddrs array with 1 ip address for regular entries.
 * 
 * By the end, the severaddrs array will have a 0 if the entry has no 
 * IP addresses in it or the count of the number of IP addresses.
 *
 * The code does not verify if there are duplicate IP addresses in the 
 * list. The vlserver does this when a fileserver registeres itself.
 */
CheckIpAddrs(header)
     struct vlheader *header;
{
    int mhblocks = 0;
    afs_int32 i, j, m, rindex;
    afs_int32 mhentries, regentries;
    afs_int32 caddrs[VL_MAX_ADDREXTBLKS];
    char mhblock[VL_ADDREXTBLK_SIZE];
    struct extentaddr *MHblock = (struct extentaddr *)mhblock;
    struct extentaddr *e;
    int ipindex, ipaddrs;
    afsUUID nulluuid;

    memset(&nulluuid, 0, sizeof(nulluuid));

    if (verbose)
	printf("Check Multihomed blocks\n");

    if (header->SIT) {
	/* Read the first MH block and from it, gather the 
	 * addresses of all the mh blocks.
	 */
	readMH(header->SIT, MHblock);
	if (MHblock->ex_flags != VLCONTBLOCK) {
	    printf
		("Multihomed Block 0: Bad entry at %u: Not a valid multihomed block\n",
		 header->SIT);
	}

	for (i = 0; i < VL_MAX_ADDREXTBLKS; i++) {
	    caddrs[i] = MHblock->ex_contaddrs[i];
	}

	if (header->SIT != caddrs[0]) {
	    printf
		("MH block does not point to self %u in header, %u in block\n",
		 header->SIT, caddrs[0]);
	}

	/* Now read each MH block and record it in the record array */
	for (i = 0; i < VL_MAX_ADDREXTBLKS; i++) {
	    if (!caddrs[i])
		continue;

	    readMH(caddrs[i], MHblock);
	    if (MHblock->ex_flags != VLCONTBLOCK) {
		printf
		    ("Multihomed Block 0: Bad entry at %u: Not a valid multihomed block\n",
		     header->SIT);
	    }

	    rindex = caddrs[i] / sizeof(vlentry);
	    if (record[rindex].addr != caddrs[i] && record[rindex].addr) {
		printf
		    ("INTERNAL ERROR: addresses %u and %u use same record slot %d\n",
		     record[rindex].addr, caddrs[i], rindex);
	    }
	    if (record[rindex].type & FRC) {
		printf
		    ("MH Blocks Chain %d: Bad entry at %u: Already a MH block\n",
		     i, record[rindex].addr);
		break;
	    }
	    record[rindex].type |= MHC;

	    mhblocks++;

	    /* Read each entry in a multihomed block. 
	     * Find the pointer to the entry in the IpMappedAddr array and
	     * verify that the entry is good (has IP addresses in it).
	     */
	    mhentries = 0;
	    for (j = 1; j < VL_MHSRV_PERBLK; j++) {
		e = (struct extentaddr *)&(MHblock[j]);

		/* Search the IpMappedAddr array for the reference to this entry */
		for (ipindex = 0; ipindex < MAXSERVERID; ipindex++) {
		    if (((header->IpMappedAddr[ipindex] & 0xff000000) ==
			 0xff000000)
			&&
			(((header->
			   IpMappedAddr[ipindex] & 0x00ff0000) >> 16) == i)
			&& ((header->IpMappedAddr[ipindex] & 0x0000ffff) ==
			    j)) {
			break;
		    }
		}
		if (ipindex >= MAXSERVERID)
		    ipindex = -1;
		else
		    serveraddrs[ipindex] = -1;

		if (memcmp(&e->ex_hostuuid, &nulluuid, sizeof(afsUUID)) == 0) {
		    if (ipindex != -1) {
			printf
			    ("Server Addrs index %d references null MH block %d, index %d\n",
			     ipindex, i, j);
			serveraddrs[ipindex] = 0;	/* avoids printing 2nd error below */
		    }
		    continue;
		}

		/* Step through each ip address and count the good addresses */
		ipaddrs = 0;
		for (m = 0; m < VL_MAXIPADDRS_PERMH; m++) {
		    if (e->ex_addrs[m])
			ipaddrs++;
		}

		/* If we found any good ip addresses, mark it in the serveraddrs record */
		if (ipaddrs) {
		    mhentries++;
		    if (ipindex == -1) {
			printf
			    ("MH block %d, index %d: Not referenced by server addrs\n",
			     i, j);
		    } else {
			serveraddrs[ipindex] = ipaddrs;	/* It is good */
		    }
		}

		if (listservers && ipaddrs) {
		    printf("MH block %d, index %d:", i, j);
		    for (m = 0; m < VL_MAXIPADDRS_PERMH; m++) {
			if (!e->ex_addrs[m])
			    continue;
			printf(" %d.%d.%d.%d",
			       (e->ex_addrs[m] & 0xff000000) >> 24,
			       (e->ex_addrs[m] & 0x00ff0000) >> 16,
			       (e->ex_addrs[m] & 0x0000ff00) >> 8,
			       (e->ex_addrs[m] & 0x000000ff));
		    }
		    printf("\n");
		}
	    }
/*
 *      if (mhentries != MHblock->ex_count) {
 *	   printf("MH blocks says it has %d entries (found %d)\n",
 *		  MHblock->ex_count, mhentries);
 *	}
 */
	}
    }
    if (verbose)
	printf("%d multihomed blocks\n", mhblocks);

    /* Check the server addresses */
    if (verbose)
	printf("Check server addresses\n");
    mhentries = regentries = 0;
    for (i = 0; i <= MAXSERVERID; i++) {
	if (header->IpMappedAddr[i]) {
	    if ((header->IpMappedAddr[i] & 0xff000000) == 0xff000000) {
		mhentries++;
		if (((header->IpMappedAddr[i] & 0x00ff0000) >> 16) >
		    VL_MAX_ADDREXTBLKS)
		    printf
			("IP Addr for entry %d: Multihome block is bad (%d)\n",
			 i, ((header->IpMappedAddr[i] & 0x00ff0000) >> 16));
		if (((header->IpMappedAddr[i] & 0x0000ffff) > VL_MHSRV_PERBLK)
		    || ((header->IpMappedAddr[i] & 0x0000ffff) < 1))
		    printf
			("IP Addr for entry %d: Multihome index is bad (%d)\n",
			 i, (header->IpMappedAddr[i] & 0x0000ffff));
		if (serveraddrs[i] == -1) {
		    printf
			("warning: IP Addr for entry %d: Multihome entry has no ip addresses\n",
			 i);
		    serveraddrs[i] = 0;
		}
		if (listservers) {
		    printf("   Server ip addr %d = MH block %d, index %d\n",
			   i, (header->IpMappedAddr[i] & 0x00ff0000) >> 16,
			   (header->IpMappedAddr[i] & 0x0000ffff));
		}
	    } else {
		regentries++;
		serveraddrs[i] = 1;	/* It is good */
		if (listservers) {
		    printf("   Server ip addr %d = %d.%d.%d.%d\n", i,
			   (header->IpMappedAddr[i] & 0xff000000) >> 24,
			   (header->IpMappedAddr[i] & 0x00ff0000) >> 16,
			   (header->IpMappedAddr[i] & 0x0000ff00) >> 8,
			   (header->IpMappedAddr[i] & 0x000000ff));
		}
	    }
	}
    }
    if (verbose) {
	printf("%d simple entries, %d multihomed entries, Total = %d\n",
	       regentries, mhentries, mhentries + regentries);
    }
    return;
}

int
WorkerBee(as, arock)
     struct cmd_syndesc *as;
     void *arock;
{
    char *dbfile;
    afs_int32 maxentries, type;
    struct vlheader header;
    struct nvlentry vlentry;
    int i, j, help = 0;

    dbfile = as->parms[0].items->data;	/* -database */
    listuheader = (as->parms[1].items ? 1 : 0);	/* -uheader  */
    listheader = (as->parms[2].items ? 1 : 0);	/* -vheader  */
    listservers = (as->parms[3].items ? 1 : 0);	/* -servers  */
    listentries = (as->parms[4].items ? 1 : 0);	/* -entries  */
    verbose = (as->parms[5].items ? 1 : 0);	/* -verbose  */

    /* open the vldb database file */
    fd = open(dbfile, O_RDONLY, 0);
    if (fd < 0) {
	printf("can't open file '%s'. error = %d\n", dbfile, errno);
	return 0;
    }

    /* read the ubik header and the vldb database header */
    readUbikHeader();
    readheader(&header);
    if (header.vital_header.vldbversion < 3) {
	printf("does not support vldb with version less than 3\n");
	return 0;
    }

    maxentries = (header.vital_header.eofPtr / sizeof(vlentry)) + 1;
    record = (struct er *)malloc(maxentries * sizeof(struct er));
    memset((char *)record, 0, (maxentries * sizeof(struct er)));
    memset((char *)serveraddrs, 0, sizeof(serveraddrs));

    /* Will fill in the record array of entries it found */
    ReadAllEntries(&header);
    listentries = 0;		/* Listed all the entries */

    /* Check the multihomed blocks for valid entries as well as
     * the IpMappedAddrs array in the header for valid entries.
     */
    CheckIpAddrs(&header);

    /* Follow the hash tables */
    FollowNameHash(&header);
    FollowIdHash(&header);

    /* Follow the chain of free entries */
    FollowFreeChain(&header);

    /* Now check the record we have been keeping for inconsistancies
     * For valid vlentries, also check that the server we point to is 
     * valid (the serveraddrs array).
     */
    if (verbose)
	printf("Verify each volume entry\n");
    for (i = 0; i < maxentries; i++) {
	if (record[i].type == 0)
	    continue;

	/* If a vlentry, verify that its name is valid, its name and ids are
	 * on the hash chains, and its server numbers are good.
	 */
	if (record[i].type & VL) {
	    readentry(record[i].addr, &vlentry, &type);

	    if (InvalidVolname(vlentry.name))
		printf("Volume '%s' at addr %u has an invalid name\n",
		       vlentry.name, record[i].addr);

	    if (!(record[i].type & NH))
		printf("Volume '%s' not found in name hash\n", vlentry.name);

	    if (vlentry.volumeId[0] && !(record[i].type & RWH))
		printf("Volume '%s' id %u not found in RW hash chain\n",
		       vlentry.name, vlentry.volumeId[0]);

	    if (vlentry.volumeId[1] && !(record[i].type & ROH))
		printf("Volume '%s' id %u not found in RO hash chain\n",
		       vlentry.name, vlentry.volumeId[1]);

	    if (vlentry.volumeId[2] && !(record[i].type & BKH))
		printf("Volume '%s' id %u not found in BK hash chain\n",
		       vlentry.name, vlentry.volumeId[2]);

	    for (j = 0; j < NMAXNSERVERS; j++) {
		if ((vlentry.serverNumber[j] != 255)
		    && (serveraddrs[vlentry.serverNumber[j]] == 0)) {
		    printf
			("Volume '%s', index %d points to empty server entry %d\n",
			 vlentry.name, j, vlentry.serverNumber[j]);
		}
	    }

	    if (record[i].type & 0xffffff00)
		printf
		    ("Volume '%s' id %u also found on other chains (0x%x)\n",
		     vlentry.name, vlentry.volumeId[0], record[i].type);

	    /* A free entry */
	} else if (record[i].type & FR) {
	    if (!(record[i].type & FRC))
		printf("Free vlentry at %u not on free chain\n",
		       record[i].addr);

	    if (record[i].type & 0xfffffdf0)
		printf
		    ("Free vlentry at %u also found on other chains (0x%x)\n",
		     record[i].addr, record[i].type);

	    /* A multihomed entry */
	} else if (record[i].type & MH) {
	    if (!(record[i].type & MHC))
		printf("Multihomed block at %u is orphaned\n",
		       record[i].addr);

	    if (record[i].type & 0xfffffef0)
		printf
		    ("Multihomed block at %u also found on other chains (0x%x)\n",
		     record[i].addr, record[i].type);

	} else {
	    printf("Unknown entry type at %u (0x%x)\n", record[i].addr,
		   record[i].type);
	}
    }
    return 0;
}

main(argc, argv)
     int argc;
     char **argv;
{
    struct cmd_syndesc *ts;

    setlinebuf(stdout);

    ts = cmd_CreateSyntax(NULL, WorkerBee, NULL, "vldb check");
    cmd_AddParm(ts, "-database", CMD_SINGLE, CMD_REQUIRED, "vldb_file");
    cmd_AddParm(ts, "-uheader", CMD_FLAG, CMD_OPTIONAL,
		"Display UBIK header");
    cmd_AddParm(ts, "-vheader", CMD_FLAG, CMD_OPTIONAL,
		"Display VLDB header");
    cmd_AddParm(ts, "-servers", CMD_FLAG, CMD_OPTIONAL,
		"Display server list");
    cmd_AddParm(ts, "-entries", CMD_FLAG, CMD_OPTIONAL, "Display entries");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose");

    return cmd_Dispatch(argc, argv);
}
