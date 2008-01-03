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

RCSID
    ("$Header$");

#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <time.h>
#include <io.h>
#else
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#endif
#include <afs/cmd.h>

#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/errors.h>
#include "lock.h"
#include "lwp.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include "viceinode.h"
#include <afs/afssyscalls.h>
#include "acl.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef _AIX
#include <time.h>
#endif

#include <dirent.h>

#include "volser/volser.h"
#include "volser/volint.h"
#include "volser/dump.h"

#define putint32(p, v)  *p++ = v>>24, *p++ = v>>16, *p++ = v>>8, *p++ = v
#define putshort(p, v) *p++ = v>>8, *p++ = v

#ifdef O_LARGEFILE
#define afs_stat	stat64
#define afs_fstat	fstat64
#define afs_open	open64
#else /* !O_LARGEFILE */
#define afs_stat	stat
#define afs_fstat	fstat
#define afs_open	open
#endif /* !O_LARGEFILE */

int VolumeChanged;		/* needed by physio - leave alone */
int verbose = 0;

/* Forward Declarations */
void HandleVolume(struct DiskPartition *partP, char *name, char *filename);
Volume *AttachVolume(struct DiskPartition *dp, char *volname,
		     register struct VolumeHeader *header);
static void DoMyVolDump(Volume * vp, struct DiskPartition *dp,
			char *dumpfile);

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

char name[VMAXPATHLEN];


int
ReadHdr1(IHandle_t * ih, char *to, int size, u_int magic, u_int version)
{
    int bad = 0;
    int code;

    code = IH_IREAD(ih, 0, to, size);
    if (code != size)
	return -1;

    return 0;
}


Volume *
AttachVolume(struct DiskPartition * dp, char *volname,
	     register struct VolumeHeader * header)
{
    register Volume *vp;
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
    vp->header = (struct volHeader *)calloc(1, sizeof(*vp->header));
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
    register struct cmd_item *ti;
    int err = 0;
    int volumeId = 0;
    char *partName = 0;
    char *fileName = NULL;
    struct DiskPartition *partP = NULL;
    char name1[128];
    char tmpPartName[20];


#ifndef AFS_NT40_ENV
#if 0
    if (geteuid() != 0) {
	fprintf(stderr, "voldump must be run as root; sorry\n");
	exit(1);
    }
#endif
#endif

    if ((ti = as->parms[0].items))
	partName = ti->data;
    if ((ti = as->parms[1].items))
	volumeId = atoi(ti->data);
    if ((ti = as->parms[2].items))
	fileName = ti->data;
    if ((ti = as->parms[3].items))
	verbose = 1;

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

    (void)afs_snprintf(name1, sizeof name1, VFORMAT, (unsigned long)volumeId);
    HandleVolume(partP, name1, fileName);
    return 0;
}

void
HandleVolume(struct DiskPartition *dp, char *name, char *filename)
{
    struct VolumeHeader header;
    struct VolumeDiskHeader diskHeader;
    struct afs_stat status, stat;
    register int fd;
    Volume *vp;
    IHandle_t *ih;
    char headerName[1024];

    afs_int32 n;

    (void)afs_snprintf(headerName, sizeof headerName, "%s/%s",
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

    DoMyVolDump(vp, dp, filename);
}


int
main(int argc, char **argv)
{
    register struct cmd_syndesc *ts;
    afs_int32 code;

    VInitVolumePackage(volumeUtility, 5, 5, DONT_CONNECT_FS, 0);

    ts = cmd_CreateSyntax(NULL, handleit, NULL,
			  "Dump a volume to a 'vos dump' format file without using volserver");
    cmd_AddParm(ts, "-part", CMD_LIST, CMD_OPTIONAL, "AFS partition name");
    cmd_AddParm(ts, "-volumeid", CMD_LIST, CMD_OPTIONAL, "Volume id");
    cmd_AddParm(ts, "-file", CMD_LIST, CMD_OPTIONAL, "Dump filename");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL,
		"Trace dump progress (very verbose)");
    code = cmd_Dispatch(argc, argv);
    return code;
}




static int
DumpDouble(int dumpfd, char tag, register afs_uint32 value1,
	   register afs_uint32 value2)
{
    int res;
    char tbuffer[9];
    register byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    putint32(p, value1);
    putint32(p, value2);

    res = write(dumpfd, tbuffer, 9);
    return ((res == 9) ? 0 : VOLSERDUMPERROR);
}

static int
DumpInt32(int dumpfd, char tag, register afs_uint32 value)
{
    char tbuffer[5];
    register byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    putint32(p, value);
    return ((write(dumpfd, tbuffer, 5) == 5) ? 0 : VOLSERDUMPERROR);
}

static int
DumpString(int dumpfd, char tag, register char *s)
{
    register int n;
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
DumpArrayInt32(int dumpfd, char tag, register afs_uint32 * array,
	       register int nelem)
{
    char tbuffer[4];
    register afs_uint32 v;
    int code = 0;
    register byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    putshort(p, nelem);
    code = write(dumpfd, tbuffer, 3);
    if (code != 3)
	return VOLSERDUMPERROR;
    while (nelem--) {
	p = (unsigned char *)tbuffer;
	v = *array++;		/*this was register */

	putint32(p, v);
	code = write(dumpfd, tbuffer, 4);
	if (code != 4)
	    return VOLSERDUMPERROR;
    }
    return 0;
}




static int
DumpDumpHeader(int dumpfd, register Volume * vp, afs_int32 fromtime)
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
    dumpTimes[1] = V_backupDate(vp);	/* Until the time the clone was made */
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
    register byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p = value;
    return ((write(dumpfd, tbuffer, 2) == 2) ? 0 : VOLSERDUMPERROR);
}

static int
DumpTag(int dumpfd, register int tag)
{
    char p;

    p = tag;
    return ((write(dumpfd, &p, 1) == 1) ? 0 : VOLSERDUMPERROR);

}

static int
DumpBool(int dumpfd, char tag, unsigned int value)
{
    char tbuffer[2];
    register byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p = value;
    return ((write(dumpfd, tbuffer, 2) == 2) ? 0 : VOLSERDUMPERROR);
}



static int
DumpVolumeHeader(int dumpfd, register Volume * vp)
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
    if (!code)
	code = DumpInt32(dumpfd, 'V', V_volUpCounter(vp));
    return code;
}

static int
DumpShort(int dumpfd, char tag, unsigned int value)
{
    char tbuffer[3];
    register byte *p = (unsigned char *)tbuffer;
    *p++ = tag;
    *p++ = value >> 8;
    *p = value;
    return ((write(dumpfd, tbuffer, 3) == 3) ? 0 : VOLSERDUMPERROR);
}

static int
DumpByteString(int dumpfd, char tag, register byte * bs, register int nbytes)
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
    int code = 0, failed_seek = 0, failed_write = 0;
    afs_int32 pad = 0, offset;
    afs_sfsize_t n, nbytes, howMany, howBig;
    byte *p;
#ifndef AFS_NT40_ENV
    struct afs_stat status;
#endif
    afs_sfsize_t size, tmpsize;
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
    struct statfs tstatfs;
#endif

    if (verbose)
	fprintf(stderr, "dumping file for vnode %d\n", vnode);

#ifdef AFS_NT40_ENV
    howBig = _filelength(handleP->fd_fd);
    howMany = 4096;

#else
    afs_fstat(handleP->fd_fd, &status);
    howBig = status.st_size;

#ifdef	AFS_AIX_ENV
    /* Unfortunately in AIX valuable fields such as st_blksize are 
     * gone from the stat structure.
     */
    fstatfs(handleP->fd_fd, &tstatfs);
    howMany = tstatfs.f_bsize;
#else
    howMany = status.st_blksize;
#endif /* AFS_AIX_ENV */
#endif /* AFS_NT40_ENV */


    size = FDH_SIZE(handleP);

    if (verbose)
	fprintf(stderr, "  howBig = %u, howMany = %u, fdh size = %u\n",
		howBig, howMany, size);

#ifdef AFS_LARGEFILE_ENV
    {
	afs_uint32 hi, lo;
	SplitInt64(size, hi, lo);
	if (hi == 0L) {
	    code = DumpInt32(dumpfd, 'f', lo);
	} else {
	    code = DumpDouble(dumpfd, 'h', hi, lo);
	}
    }
#else /* !AFS_LARGEFILE_ENV */
    code = DumpInt32(dumpfd, 'f', size);
#endif /* !AFS_LARGEFILE_ENV */
    if (code) {
	return VOLSERDUMPERROR;
    }

    p = (unsigned char *)malloc(howMany);
    if (!p) {
	fprintf(stderr, "out of memory!\n");
	return VOLSERDUMPERROR;
    }

    /* loop through whole file, while we still have bytes left, and no errors, in chunks of howMany bytes */
    for (nbytes = size; (nbytes && !failed_write); nbytes -= howMany) {
	if (nbytes < howMany)
	    howMany = nbytes;

	/* Read the data - unless we know we can't */
	n = (failed_seek ? 0 : FDH_READ(handleP, p, howMany));

	/* If read any good data and we null padded previously, log the
	 * amount that we had null padded.
	 */
	if ((n > 0) && pad) {
	    fprintf(stderr, "Null padding file %d bytes at offset %u\n", pad,
		    offset);
	    pad = 0;
	}

	/* If didn't read enough data, null padd the rest of the buffer. This
	 * can happen if, for instance, the media has some bad spots. We don't
	 * want to quit the dump, so we start null padding.
	 */
	if (n < howMany) {

		if (verbose) fprintf(stderr, "  read %u instead of %u bytes.\n", n, howMany);

	    /* Record the read error */
	    if (n < 0) {
		n = 0;
		fprintf(stderr, "Error %d reading inode %s for vnode %d\n",
			errno, PrintInode(NULL, handleP->fd_ih->ih_ino),
			vnode);
	    } else if (!pad) {
		fprintf(stderr, "Error reading inode %s for vnode %d\n",
			PrintInode(NULL, handleP->fd_ih->ih_ino), vnode);
	    }

	    /* Pad the rest of the buffer with zeros. Remember offset we started 
	     * padding. Keep total tally of padding.
	     */
	    memset(p + n, 0, howMany - n);
	    if (!pad)
		offset = (howBig - nbytes) + n;
	    pad += (howMany - n);

	    /* Now seek over the data we could not get. An error here means we
	     * can't do the next read.
	     */
	    failed_seek = FDH_SEEK(handleP, ((size - nbytes) + howMany), SEEK_SET);
	    if (failed_seek != ((size - nbytes) + howMany)) {
		if (failed_seek < 0) {
		    fprintf(stderr,
			    "Error %d seeking in inode %s for vnode %d\n",
			    errno, PrintInode(NULL, handleP->fd_ih->ih_ino),
			    vnode);
		} else {
		    fprintf(stderr,
			    "Error seeking in inode %s for vnode %d\n",
			    PrintInode(NULL, handleP->fd_ih->ih_ino), vnode);
		    failed_seek = -1;
		}
	    } else {
		failed_seek = 0;
	    }
	}

	/* Now write the data out */
	if (write(dumpfd, (char *)p, howMany) != howMany)
	    failed_write = VOLSERDUMPERROR;
    }

    if (pad) {			/* Any padding we hadn't reported yet */
	fprintf(stderr, "Null padding file: %d bytes at offset %u\n", pad,
		offset);
    }

    free(p);
    return failed_write;
}


static int
DumpVnode(int dumpfd, struct VnodeDiskObject *v, int volid, int vnodeNumber,
	  int dumpEverything, struct Volume *vp)
{
    int code = 0;
    IHandle_t *ihP;
    FdHandle_t *fdP;

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
	acl_HtonACL(VVnodeDiskACL(v));
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
		    "Unable to open inode %s for vnode %u (volume %i); not dumped, error %d\n",
		    PrintInode(NULL, VNDISK_GET_INO(v)), vnodeNumber, volid,
		    errno);
	}
	else
	{
		if (verbose)
		    fprintf(stderr, "about to dump inode %s for vnode %u\n",
			    PrintInode(NULL, VNDISK_GET_INO(v)), vnodeNumber);
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
    register int code = 0;
    register struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    StreamHandle_t *file;
    FdHandle_t *fdP;
    int size;
    int flag;
    int offset = 0;
    register int vnodeIndex, nVnodes = 0;

    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    file = FDH_FDOPEN(fdP, "r+");
    size = OS_SIZE(fdP->fd_fd);
    nVnodes = (size / vcp->diskSize) - 1;

    if (nVnodes > 0) {
	STREAM_SEEK(file, vcp->diskSize, 0);
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
	    fprintf(stderr, "about to dump %s vnode %u (vnode offset = %u)\n",
			class == vSmall ? "vSmall" : "vLarge",
		    bitNumberToVnodeNumber(vnodeIndex, class), offset);
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
DumpPartial(int dumpfd, register Volume * vp, afs_int32 fromtime,
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
DoMyVolDump(Volume * vp, struct DiskPartition *dp, char *dumpfile)
{
    int code = 0;
    int fromtime = 0;
    int dumpAllDirs = 0;
    int dumpfd = 0;

    if (dumpfile) {
	unlink(dumpfile);
	dumpfd =
	    open(dumpfile, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
	if (dumpfd < 0) {
	    fprintf(stderr, "Failed to open dump file! Exiting.\n");
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
