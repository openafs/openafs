/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
   System:		VICE-TWO
   Module:		vol-dump.c
   Institution:	The Information Technology Center, Carnegie-Mellon University

   */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wformat"
#endif

#include <roken.h>

#include <ctype.h>

#include <afs/cmd.h>
#include <rx/xdr.h>
#include <rx/rx_queue.h>
#include <afs/afsint.h>
#include <afs/nfs.h>
#include <afs/errors.h>
#include <afs/afs_lock.h>
#include <lwp.h>
#include <afs/afssyscalls.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/partition.h>
#include <afs/viceinode.h>
#include <afs/afssyscalls.h>
#include <afs/acl.h>
#include <afs/dir.h>
#include <afs/com_err.h>

#include "volser.h"
#include "volint.h"
#include "dump.h"

#define afs_putint32(p, v)  *p++ = v>>24, *p++ = v>>16, *p++ = v>>8, *p++ = v
#define afs_putshort(p, v) *p++ = v>>8, *p++ = v

int VolumeChanged;		/* needed by physio - leave alone */
int verbose = 0;
static int enable_padding; /* Pad errors with NUL bytes */

/* Forward Declarations */
static void HandleVolume(struct DiskPartition64 *partP, char *name,
			 char *filename, int fromtime);
static Volume *AttachVolume(struct DiskPartition64 *dp, char *volname,
			    struct VolumeHeader *header);
static void DoMyVolDump(Volume * vp, struct DiskPartition64 *dp,
			char *dumpfile, int fromtime);

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

char name[VMAXPATHLEN];


static int
ReadHdr1(IHandle_t * ih, char *to, int size, u_int magic, u_int version)
{
    int code;

    code = IH_IREAD(ih, 0, to, size);
    if (code != size)
	return -1;

    return 0;
}


static Volume *
AttachVolume(struct DiskPartition64 * dp, char *volname,
	     struct VolumeHeader * header)
{
    Volume *vp;
    afs_int32 ec = 0;

    vp = (Volume *) calloc(1, sizeof(Volume));
    vp->specialStatus = 0;
    vp->device = dp->device;
    vp->partition = dp;
    IH_INIT(vp->vnodeIndex[vLarge].handle, dp->device, header->parent,
	    header->largeVnodeIndex);
    IH_INIT(vp->vnodeIndex[vSmall].handle, dp->device, header->parent,
	    header->smallVnodeIndex);
    IH_INIT(vp->diskDataHandle, dp->device, header->parent,
	    header->volumeInfo);
    IH_INIT(V_linkHandle(vp), dp->device, header->parent, header->linkTable);
    vp->cacheCheck = 0;		/* XXXX */
    vp->shuttingDown = 0;
    vp->goingOffline = 0;
    vp->nUsers = 1;
    vp->header = calloc(1, sizeof(*vp->header));
    ec = ReadHdr1(V_diskDataHandle(vp), (char *)&V_disk(vp),
		  sizeof(V_disk(vp)), VOLUMEINFOMAGIC, VOLUMEINFOVERSION);
    if (!ec) {
	struct IndexFileHeader iHead;
	ec = ReadHdr1(vp->vnodeIndex[vSmall].handle, (char *)&iHead,
		      sizeof(iHead), SMALLINDEXMAGIC, SMALLINDEXVERSION);
    }
    if (!ec) {
	struct IndexFileHeader iHead;
	ec = ReadHdr1(vp->vnodeIndex[vLarge].handle, (char *)&iHead,
		      sizeof(iHead), LARGEINDEXMAGIC, LARGEINDEXVERSION);
    }
#ifdef AFS_NAMEI_ENV
    if (!ec) {
	struct versionStamp stamp;
	ec = ReadHdr1(V_linkHandle(vp), (char *)&stamp, sizeof(stamp),
		      LINKTABLEMAGIC, LINKTABLEVERSION);
    }
#endif
    if (ec)
	return (Volume *) 0;
    return vp;
}


static int
handleit(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    int err = 0;
    afs_uint32 volumeId = 0;
    char *partName = 0;
    char *fileName = NULL;
    struct DiskPartition64 *partP = NULL;
    char name1[128];
    char tmpPartName[20];
    int fromtime = 0;
    afs_int32 code;


    if ((ti = as->parms[0].items))
	partName = ti->data;
    if ((ti = as->parms[1].items))
	volumeId = (afs_uint32)atoi(ti->data);
    if ((ti = as->parms[2].items))
	fileName = ti->data;
    if ((ti = as->parms[3].items))
	verbose = 1;
    if (as->parms[4].items && strcmp(as->parms[4].items->data, "0")) {
	code = ktime_DateToInt32(as->parms[4].items->data, &fromtime);
	if (code) {
	    fprintf(STDERR, "failed to parse date '%s' (error=%d))\n",
		as->parms[4].items->data, code);
		return code;
	}
    }
    if (as->parms[5].items != NULL) { /* -pad-errors */
	enable_padding = 1;
    }

    DInit(10);

    err = VAttachPartitions();
    if (err) {
	fprintf(stderr, "%d partitions had errors during attach.\n", err);
    }

    if (partName) {
	if (strlen(partName) == 1) {
	    if (partName[0] >= 'a' && partName[0] <= 'z') {
		strcpy(tmpPartName, "/vicepa");
		tmpPartName[6] = partName[0];
		partP = VGetPartition(tmpPartName, 0);
	    }
	} else {
	    partP = VGetPartition(partName, 0);
	}
	if (!partP) {
	    fprintf(stderr,
		    "%s is not an AFS partition name on this server.\n",
		    partName);
	    exit(1);
	}
    }

    if (!volumeId) {
	fprintf(stderr, "Must specify volume id!\n");
	exit(1);
    }

    if (!partP) {
	fprintf(stderr, "must specify vice partition.\n");
	exit(1);
    }

    snprintf(name1, sizeof name1, VFORMAT, (unsigned long)volumeId);
    HandleVolume(partP, name1, fileName, fromtime);
    return 0;
}

static void
HandleVolume(struct DiskPartition64 *dp, char *name, char *filename, int fromtime)
{
    struct VolumeHeader header;
    struct VolumeDiskHeader diskHeader;
    struct afs_stat status;
    int fd;
    Volume *vp;
    char headerName[1024];

    afs_int32 n;

    snprintf(headerName, sizeof headerName, "%s" OS_DIRSEP "%s",
	     VPartitionPath(dp), name);
    if ((fd = afs_open(headerName, O_RDONLY)) == -1
	|| afs_fstat(fd, &status) == -1) {
	fprintf(stderr, "Cannot read volume header %s\n", name);
	close(fd);
	exit(1);
    }
    n = read(fd, &diskHeader, sizeof(diskHeader));

    if (n != sizeof(diskHeader)
	|| diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
	fprintf(stderr, "Error reading volume header %s\n", name);
	exit(1);
    }
    if (diskHeader.stamp.version != VOLUMEHEADERVERSION) {
	fprintf(stderr,
		"Volume %s, version number is incorrect; volume needs salvage\n",
		name);
	exit(1);
    }
    DiskToVolumeHeader(&header, &diskHeader);

    close(fd);
    vp = AttachVolume(dp, name, &header);
    if (!vp) {
	fprintf(stderr, "Error attaching volume header %s\n", name);
	exit(1);
    }

    DoMyVolDump(vp, dp, filename, fromtime);

    free(vp);
}


int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    afs_int32 code;
    VolumePackageOptions opts;

    VOptDefaults(volumeUtility, &opts);
    if (VInitVolumePackage2(volumeUtility, &opts)) {
	fprintf(stderr, "errors encountered initializing volume package, but "
	                "trying to continue anyway\n");
    }

    ts = cmd_CreateSyntax(NULL, handleit, NULL, 0,
			  "Dump a volume to a 'vos dump' format file without using volserver");
    cmd_AddParm(ts, "-part", CMD_LIST, CMD_OPTIONAL, "AFS partition name");
    cmd_AddParm(ts, "-volumeid", CMD_LIST, CMD_OPTIONAL, "Volume id");
    cmd_AddParm(ts, "-file", CMD_LIST, CMD_OPTIONAL, "Dump filename");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL,
		"Trace dump progress (very verbose)");
    cmd_AddParm(ts, "-time", CMD_SINGLE, CMD_OPTIONAL, "dump from time");
    cmd_AddParm(ts, "-pad-errors", CMD_FLAG, CMD_OPTIONAL,
		"pad i/o errors with NUL bytes");
    code = cmd_Dispatch(argc, argv);
    return code;
}




static int
DumpDouble(int dumpfd, char tag, afs_uint32 value1,
	   afs_uint32 value2)
{
    int res;
    char tbuffer[9];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    afs_putint32(p, value1);
    afs_putint32(p, value2);

    res = write(dumpfd, tbuffer, 9);
    return ((res == 9) ? 0 : VOLSERDUMPERROR);
}

static int
DumpInt32(int dumpfd, char tag, afs_uint32 value)
{
    char tbuffer[5];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    afs_putint32(p, value);
    return ((write(dumpfd, tbuffer, 5) == 5) ? 0 : VOLSERDUMPERROR);
}

static int
DumpString(int dumpfd, char tag, char *s)
{
    int n;
    int code = 0;
    code = write(dumpfd, &tag, 1);
    if (code != 1)
	return VOLSERDUMPERROR;
    n = strlen(s) + 1;
    code = write(dumpfd, s, n);
    if (code != n)
	return VOLSERDUMPERROR;
    return 0;
}


static int
DumpArrayInt32(int dumpfd, char tag, afs_uint32 * array,
	       int nelem)
{
    char tbuffer[4];
    afs_uint32 v;
    int code = 0;
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    afs_putshort(p, nelem);
    code = write(dumpfd, tbuffer, 3);
    if (code != 3)
	return VOLSERDUMPERROR;
    while (nelem--) {
	p = (unsigned char *)tbuffer;
	v = *array++;		/*this was register */

	afs_putint32(p, v);
	code = write(dumpfd, tbuffer, 4);
	if (code != 4)
	    return VOLSERDUMPERROR;
    }
    return 0;
}




static int
DumpDumpHeader(int dumpfd, Volume * vp, afs_int32 fromtime)
{
    int code = 0;
    afs_int32 dumpTimes[2];

    if (verbose)
	fprintf(stderr, "dumping dump header\n");

    if (!code)
	code = DumpDouble(dumpfd, D_DUMPHEADER, DUMPBEGINMAGIC, DUMPVERSION);

    if (!code)
	code = DumpInt32(dumpfd, 'v', V_id(vp));

    if (!code)
	code = DumpString(dumpfd, 'n', V_name(vp));

    dumpTimes[0] = fromtime;
    switch (V_type(vp)) {
    case readwriteVolume:
	dumpTimes[1] = V_updateDate(vp);	/* until last update */
	break;
    case readonlyVolume:
	dumpTimes[1] = V_creationDate(vp);	/* until clone was updated */
	break;
    case backupVolume:
	/* until backup was made */
	dumpTimes[1] = V_backupDate(vp) != 0 ? V_backupDate(vp) :
					       V_creationDate(vp);
	break;
    default:
	code = EINVAL;
    }
    if (!code)
	code = DumpArrayInt32(dumpfd, 't', (afs_uint32 *) dumpTimes, 2);

    return code;
}


static int
DumpEnd(int dumpfd)
{
    return (DumpInt32(dumpfd, D_DUMPEND, DUMPENDMAGIC));
}

static int
DumpByte(int dumpfd, char tag, byte value)
{
    char tbuffer[2];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p = value;
    return ((write(dumpfd, tbuffer, 2) == 2) ? 0 : VOLSERDUMPERROR);
}

static int
DumpTag(int dumpfd, int tag)
{
    char p;

    p = tag;
    return ((write(dumpfd, &p, 1) == 1) ? 0 : VOLSERDUMPERROR);

}

static int
DumpBool(int dumpfd, char tag, unsigned int value)
{
    char tbuffer[2];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p = value;
    return ((write(dumpfd, tbuffer, 2) == 2) ? 0 : VOLSERDUMPERROR);
}



static int
DumpVolumeHeader(int dumpfd, Volume * vp)
{
    int code = 0;

    if (verbose)
	fprintf(stderr, "dumping volume header\n");

    if (!code)
	code = DumpTag(dumpfd, D_VOLUMEHEADER);
    if (!code)
	code = DumpInt32(dumpfd, 'i', V_id(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'v', V_stamp(vp).version);
    if (!code)
	code = DumpString(dumpfd, 'n', V_name(vp));
    if (!code)
	code = DumpBool(dumpfd, 's', V_inService(vp));
    if (!code)
	code = DumpBool(dumpfd, 'b', V_blessed(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'u', V_uniquifier(vp));
    if (!code)
	code = DumpByte(dumpfd, 't', (byte) V_type(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'p', V_parentId(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'c', V_cloneId(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'q', V_maxquota(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'm', V_minquota(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'd', V_diskused(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'f', V_filecount(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'a', V_accountNumber(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'o', V_owner(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'C', V_creationDate(vp));	/* Rw volume creation date */
    if (!code)
	code = DumpInt32(dumpfd, 'A', V_accessDate(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'U', V_updateDate(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'E', V_expirationDate(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'B', V_backupDate(vp));	/* Rw volume backup clone date */
    if (!code)
	code = DumpString(dumpfd, 'O', V_offlineMessage(vp));

    /*
     * We do NOT dump the detailed volume statistics residing in the old
     * motd field, since we cannot tell from the info in a dump whether
     * statistics data has been put there.  Instead, we dump a null string,
     * just as if that was what the motd contained.
     */
    if (!code)
	code = DumpString(dumpfd, 'M', "");
    if (!code)
	code =
	    DumpArrayInt32(dumpfd, 'W', (afs_uint32 *) V_weekUse(vp),
			   sizeof(V_weekUse(vp)) / sizeof(V_weekUse(vp)[0]));
    if (!code)
	code = DumpInt32(dumpfd, 'D', V_dayUseDate(vp));
    if (!code)
	code = DumpInt32(dumpfd, 'Z', V_dayUse(vp));
    return code;
}

static int
DumpShort(int dumpfd, char tag, unsigned int value)
{
    char tbuffer[3];
    byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p++ = value >> 8;
    *p = value;
    return ((write(dumpfd, tbuffer, 3) == 3) ? 0 : VOLSERDUMPERROR);
}

static int
DumpByteString(int dumpfd, char tag, byte * bs, int nbytes)
{
    int code = 0;

    code = write(dumpfd, &tag, 1);
    if (code != 1)
	return VOLSERDUMPERROR;
    code = write(dumpfd, (char *)bs, nbytes);
    if (code != nbytes)
	return VOLSERDUMPERROR;
    return 0;
}


static int
DumpFile(int dumpfd, int vnode, FdHandle_t * handleP,  struct VnodeDiskObject *v)
{
    int code = 0;
    afs_int32 pad = 0;
    afs_foff_t offset = 0;
    afs_sfsize_t nbytes, size, howMany;
    ssize_t n = 0;
    afs_foff_t howFar = 0;
    byte *p;
    afs_uint32 hi, lo;
    afs_ino_str_t stmp;

    if (verbose)
	fprintf(stderr, "dumping file for vnode %d\n", vnode);

    code = FDH_BLOCKSIZE(handleP, &size, &howMany);
    if (code != 0) {
	fprintf(stderr, "FDH_BLOCKSIZE returned error code %d on descriptor %d\n",
		code, handleP->fd_fd);
	return VOLSERDUMPERROR;
    }

    if (verbose)
	fprintf(stderr, "  howMany = %u, fdh size = %u\n",
		(unsigned int) howMany, (unsigned int) size);

    SplitInt64(size, hi, lo);
    if (hi == 0L) {
	code = DumpInt32(dumpfd, 'f', lo);
    } else {
	code = DumpDouble(dumpfd, 'h', hi, lo);
    }

    if (code) {
	return VOLSERDUMPERROR;
    }

    p = malloc(howMany);
    if (!p) {
	fprintf(stderr, "out of memory!\n");
	return VOLSERDUMPERROR;
    }

    /* loop through whole file, while we still have bytes left, and no errors, in chunks of howMany bytes */
    for (nbytes = size; (nbytes && !code); ) {
	if (nbytes < howMany)
	    howMany = nbytes;

	n = FDH_PREAD(handleP, p, howMany, howFar);

	/* If read any good data and we null padded previously, log the
	 * amount that we had null padded.
	 */
	if ((n > 0) && pad) {
	    fprintf(stderr, "Null padding file %d bytes at offset %lld\n", pad,
		    (long long)offset);
	    pad = 0;
	}

	if (n < 0) {
	    fprintf(stderr, "Error %d reading inode %s for vnode %d\n",
		    errno, PrintInode(stmp, handleP->fd_ih->ih_ino),
		    vnode);
	    code = VOLSERDUMPERROR;
	}
	if (n == 0) {
	    if (pad == 0) {
		fprintf(stderr, "Unexpected EOF reading inode %s for vnode %d\n",
			PrintInode(stmp, handleP->fd_ih->ih_ino), vnode);
	    }
	    code = VOLSERDUMPERROR;
	}

	if (code != 0 && enable_padding) {
	    /*
	     * If our read failed, NUL-pad the rest of the buffer. This can
	     * happen if, for instance, the media has some bad spots. We don't
	     * want to quit the dump, so we start NUL padding.
	     */
	    memset(p, 0, howMany);

	    /* Remember the offset where we started padding, and keep a total
	     * tally of how much padding we've done. */
	    if (!pad)
		offset = howFar;
	    pad += howMany;

	    /* Pretend we read 'howMany' bytes. */
	    n = howMany;
	    code = 0;
	}
	if (code != 0) {
	    break;
	}

	howFar += n;
	nbytes -= n;

	/* Now write the data out */
	if (write(dumpfd, (char *)p, n) != n)
	    code = VOLSERDUMPERROR;
    }

    if (pad) {			/* Any padding we hadn't reported yet */
	fprintf(stderr, "Null padding file: %d bytes at offset %lld\n", pad,
		(long long)offset);
    }

    free(p);
    return code;
}


static int
DumpVnode(int dumpfd, struct VnodeDiskObject *v, VolumeId volid, int vnodeNumber,
	  int dumpEverything, struct Volume *vp)
{
    int code = 0;
    IHandle_t *ihP;
    FdHandle_t *fdP;
    afs_ino_str_t stmp;

    if (verbose)
	fprintf(stderr, "dumping vnode %d\n", vnodeNumber);

    if (!v || v->type == vNull)
	return code;
    if (!code)
	code = DumpDouble(dumpfd, D_VNODE, vnodeNumber, v->uniquifier);
    if (!dumpEverything)
	return code;
    if (!code)
	code = DumpByte(dumpfd, 't', (byte) v->type);
    if (!code)
	code = DumpShort(dumpfd, 'l', v->linkCount);	/* May not need this */
    if (!code)
	code = DumpInt32(dumpfd, 'v', v->dataVersion);
    if (!code)
	code = DumpInt32(dumpfd, 'm', v->unixModifyTime);
    if (!code)
	code = DumpInt32(dumpfd, 'a', v->author);
    if (!code)
	code = DumpInt32(dumpfd, 'o', v->owner);
    if (!code && v->group)
	code = DumpInt32(dumpfd, 'g', v->group);	/* default group is 0 */
    if (!code)
	code = DumpShort(dumpfd, 'b', v->modeBits);
    if (!code)
	code = DumpInt32(dumpfd, 'p', v->parent);
    if (!code)
	code = DumpInt32(dumpfd, 's', v->serverModifyTime);
    if (v->type == vDirectory) {
	code = acl_HtonACL(VVnodeDiskACL(v));
	if (code) {
	    fprintf(stderr, "Skipping invalid acl in vnode %u (volume %"AFS_VOLID_FMT")\n",
			vnodeNumber, afs_printable_VolumeId_lu(volid));
	}
	if (!code)
	    code =
		DumpByteString(dumpfd, 'A', (byte *) VVnodeDiskACL(v),
			       VAclDiskSize(v));
    }

    if (VNDISK_GET_INO(v)) {
	IH_INIT(ihP, V_device(vp), V_parentId(vp), VNDISK_GET_INO(v));
	fdP = IH_OPEN(ihP);
	if (fdP == NULL) {
	    fprintf(stderr,
		    "Unable to open inode %s for vnode %u "
		    "(volume %"AFS_VOLID_FMT"); not dumped, error %d\n",
		    PrintInode(stmp, VNDISK_GET_INO(v)), vnodeNumber,
		    afs_printable_VolumeId_lu(volid), errno);
	}
	else
	{
		if (verbose)
		    fprintf(stderr, "about to dump inode %s for vnode %u\n",
			    PrintInode(stmp, VNDISK_GET_INO(v)), vnodeNumber);
		code = DumpFile(dumpfd, vnodeNumber, fdP, v);
		FDH_CLOSE(fdP);
	}
	IH_RELEASE(ihP);
    }

    if (verbose)
	fprintf(stderr, "done dumping vnode %d\n", vnodeNumber);
    return code;
}


static int
DumpVnodeIndex(int dumpfd, Volume * vp, VnodeClass class, afs_int32 fromtime,
	       int forcedump)
{
    int code = 0;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    StreamHandle_t *file;
    FdHandle_t *fdP;
    afs_sfsize_t size;
    int flag;
    afs_foff_t offset = 0;
    int vnodeIndex, nVnodes = 0;

    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    file = FDH_FDOPEN(fdP, "r+");
    size = FDH_SIZE(fdP);
    nVnodes = (size / vcp->diskSize) - 1;

    if (nVnodes > 0) {
	STREAM_ASEEK(file, vcp->diskSize);
    } else
	nVnodes = 0;
    for (vnodeIndex = 0;
	 nVnodes && STREAM_READ(vnode, vcp->diskSize, 1, file) == 1 && !code;
	 nVnodes--, vnodeIndex++, offset += vcp->diskSize) {
	flag = forcedump || (vnode->serverModifyTime >= fromtime);
	/* Note:  the >= test is very important since some old volumes may not have
	 * a serverModifyTime.  For an epoch dump, this results in 0>=0 test, which
	 * does dump the file! */
	if (verbose)
	    fprintf(stderr, "about to dump %s vnode %u (vnode offset = %lld)\n",
			class == vSmall ? "vSmall" : "vLarge",
		    bitNumberToVnodeNumber(vnodeIndex, class), (long long)offset);
	if (!code)
	    code =
		DumpVnode(dumpfd, vnode, V_id(vp),
			  bitNumberToVnodeNumber(vnodeIndex, class), flag,
			  vp);
    }
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
    return code;
}



/* A partial dump (no dump header) */
static int
DumpPartial(int dumpfd, Volume * vp, afs_int32 fromtime,
	    int dumpAllDirs)
{
    int code = 0;

    if (verbose)
	fprintf(stderr, "about to dump the volume header\n");
    if (!code)
	code = DumpVolumeHeader(dumpfd, vp);

    if (verbose)
	fprintf(stderr, "about to dump the large vnode index\n");
    if (!code)
	code = DumpVnodeIndex(dumpfd, vp, vLarge, fromtime, dumpAllDirs);

    if (verbose)
	fprintf(stderr, "about to dump the small vnode index\n");
    if (!code)
	code = DumpVnodeIndex(dumpfd, vp, vSmall, fromtime, 0);
    return code;
}



static void
DoMyVolDump(Volume * vp, struct DiskPartition64 *dp, char *dumpfile, int fromtime)
{
    int code = 0;
    int dumpAllDirs = 0;
    int dumpfd = 0;

    if (dumpfile) {
	unlink(dumpfile);
	dumpfd =
	    afs_open(dumpfile, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
	if (dumpfd < 0) {
	    fprintf(stderr, "Failed to open dump file: %s. Exiting.\n",
	            afs_error_message(errno));
	    exit(1);
	}
    } else {
	dumpfd = 1;		/* stdout */
    }

    if (verbose)
	fprintf(stderr, "about to dump the dump header\n");
    if (!code)
	code = DumpDumpHeader(dumpfd, vp, fromtime);

    if (verbose)
	fprintf(stderr, "about to dump volume contents\n");
    if (!code)
	code = DumpPartial(dumpfd, vp, fromtime, dumpAllDirs);

    if (verbose)
	fprintf(stderr, "about to dump the dump postamble\n");
    if (!code)
	code = DumpEnd(dumpfd);

    if (verbose)
	fprintf(stderr, "finished dump\n");
    close(dumpfd);		/* might be closing stdout, no harm */
}
