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
   Module:		vol-info.c
   Institution:	The Information Technology Center, Carnegie-Mellon University

   */

#include <afsconfig.h>
#include <afs/param.h>


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
#include <afs/dir.h>

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
#include <afs/afsutil.h>

#ifdef _AIX
#include <time.h>
#endif

#include <dirent.h>

#ifdef O_LARGEFILE
#define afs_stat	stat64
#define afs_fstat	fstat64
#define afs_open	open64
#else /* !O_LARGEFILE */
#define afs_stat	stat
#define afs_fstat	fstat
#define afs_open	open
#endif /* !O_LARGEFILE */

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

static const char *progname = "volinfo";

/* Command line options */
typedef enum {
    P_ONLINE,
    P_VNODE,
    P_DATE,
    P_INODE,
    P_ITIME,
    P_PART,
    P_VOLUMEID,
    P_HEADER,
    P_SIZEONLY,
    P_SIZEONLY_COMPAT,
    P_FIXHEADER,
    P_SAVEINODES,
    P_ORPHANED,
    P_FILENAMES
} volinfo_parm_t;

/* Modes */
static int DumpInfo = 1;            /**< Dump volume information, defualt mode*/
static int DumpHeader = 0;          /**< Dump volume header files info */
static int DumpVnodes = 0;          /**< Dump vnode info */
static int DumpInodeNumber = 0;     /**< Dump inode numbers with vnodes */
static int DumpDate = 0;            /**< Dump vnode date (server modify date) with vnode */
static int InodeTimes = 0;          /**< Dump some of the dates associated with inodes */
#if defined(AFS_NAMEI_ENV)
static int PrintFileNames = 0;      /**< Dump vnode and special file name filenames */
#endif
static int ShowOrphaned = 0;        /**< Show "orphaned" vnodes (vnodes with parent of 0) */
static int ShowSizes = 0;           /**< Show volume size summary */
static int SaveInodes = 0;          /**< Save vnode data to files */
static int FixHeader = 0;           /**< Repair header files magic and version fields. */

/**
 * Volume size running totals
 */
struct sizeTotals {
    afs_uint64 diskused_k;	/**< volume size from disk data file, in kilobytes */
    afs_uint64 auxsize;		/**< size of header files, in bytes  */
    afs_uint64 auxsize_k;	/**< size of header files, in kilobytes */
    afs_uint64 vnodesize;	/**< size of the large and small vnodes, in bytes */
    afs_uint64 vnodesize_k;	/**< size of the large and small vnodes, in kilobytes */
    afs_uint64 size_k;		/**< size of headers and vnodes, in kilobytes */
};

static struct sizeTotals volumeTotals = { 0, 0, 0, 0, 0, 0 };
static struct sizeTotals partitionTotals = { 0, 0, 0, 0, 0, 0 };
static struct sizeTotals serverTotals = { 0, 0, 0, 0, 0, 0 };
static int PrintingVolumeSizes = 0;	/*print volume size lines */

/* Forward Declarations */
void PrintHeader(Volume * vp);
void HandleAllPart(void);
void HandlePart(struct DiskPartition64 *partP);
void HandleVolume(struct DiskPartition64 *partP, char *name);
struct DiskPartition64 *FindCurrentPartition(void);
Volume *AttachVolume(struct DiskPartition64 *dp, char *volname,
		     struct VolumeHeader *header);
void PrintVnode(afs_foff_t offset, VnodeDiskObject * vnode, VnodeId vnodeNumber,
		Inode ino, Volume * vp);
void HandleVnodes(Volume * vp, VnodeClass class);

/**
 * Format time as a timestamp string
 *
 * @param[in] date  time value to format
 *
 * @return timestamp string in the form YYYY/MM/DD.hh:mm:ss
 *
 * @note A static array of 8 strings are stored by this
 *       function. The array slots are overwritten, so the
 *       caller must not reference the returned string after
 *       seven additional calls to this function.
 */
char *
date(time_t date)
{
#define MAX_DATE_RESULT	100
    static char results[8][MAX_DATE_RESULT];
    static int next;
    struct tm *tm = localtime(&date);
    char buf[32];

    (void)strftime(buf, 32, "%Y/%m/%d.%H:%M:%S", tm);	/* NT does not have %T */
    (void)afs_snprintf(results[next = (next + 1) & 7], MAX_DATE_RESULT,
		       "%lu (%s)", (unsigned long)date, buf);
    return results[next];
}

#ifdef AFS_NT40_ENV
/**
 * Format file time as a timestamp string
 *
 * @param[in] ft  file time
 *
 * @return timestamp string in the form YYYY/MM/DD.hh:mm:ss
 *
 * @note A static array of 8 strings are stored by this
 *       function. The array slots are overwritten, so the
 *       caller must not reference the returned string after
 *       seven additional calls to this function.
 */
char *
NT_date(FILETIME * ft)
{
    static char result[8][64];
    static int next = 0;
    SYSTEMTIME st;
    FILETIME lft;

    if (!FileTimeToLocalFileTime(ft, &lft)
	|| !FileTimeToSystemTime(&lft, &st)) {
	fprintf(stderr, "%s: Time conversion failed.\n", progname);
	exit(1);
    }
    sprintf(result[next = ((next + 1) & 7)], "%4d/%02d/%02d.%2d:%2d:%2d",
	    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return result[next];
}
#endif

/**
 * Print the volume size table heading line, if needed.
 *
 * @return none
 */
static void
PrintVolumeSizeHeading(void)
{
    if (!PrintingVolumeSizes) {
	printf
	    ("Volume-Id\t  Volsize  Auxsize Inodesize  AVolsize SizeDiff                (VolName)\n");
	PrintingVolumeSizes = 1;
    }
}

/**
 * Accumulate totals.
 *
 * @return 0
 */
static void
AddSizeTotals(struct sizeTotals *a, struct sizeTotals *b)
{
    a->diskused_k += b->diskused_k;
    a->auxsize += b->auxsize;
    a->auxsize_k += b->auxsize_k;
    a->vnodesize += b->vnodesize;
    a->vnodesize_k += b->vnodesize_k;
    a->size_k += b->size_k;
}

/**
 * Print the sizes for a volume.
 *
 * @return none
 */
static void
PrintVolumeSizes(Volume * vp)
{
    afs_int64 diff_k = volumeTotals.size_k - volumeTotals.diskused_k;

    PrintVolumeSizeHeading();
    printf("%u\t%9llu%9llu%10llu%10llu%9lld\t%24s\n",
	   V_id(vp),
	   volumeTotals.diskused_k,
	   volumeTotals.auxsize_k, volumeTotals.vnodesize_k,
	   volumeTotals.size_k, diff_k, V_name(vp));
}

/**
 * Print the size totals for the partition.
 *
 * @return none
 */
static void
PrintPartitionTotals(afs_uint64 nvols)
{
    afs_int64 diff_k = partitionTotals.size_k - partitionTotals.diskused_k;

    PrintVolumeSizeHeading();
    printf("\nPart Totals  %12llu%9llu%10llu%10llu%9lld (%llu volumes)\n\n",
	   partitionTotals.diskused_k, partitionTotals.auxsize,
	   partitionTotals.vnodesize, partitionTotals.size_k, diff_k, nvols);
    PrintingVolumeSizes = 0;
}

/**
 * Print the size totals for all the partitions.
 *
 * @return none
 */
static void
PrintServerTotals(void)
{
    afs_int64 diff_k = serverTotals.size_k - serverTotals.diskused_k;

    printf("\nServer Totals%12lld%9lld%10lld%10lld%9lld\n",
	   serverTotals.diskused_k, serverTotals.auxsize,
	   serverTotals.vnodesize, serverTotals.size_k, diff_k);
}

/**
 * Read a volume header file
 *
 * @param[in] ih        ihandle of the header file
 * @param[in] to        destination
 * @param[in] size      expected header size
 * @param[in] magic     expected header magic number
 * @param[in] version   expected header version number
 *
 * @return error code
 *   @retval 0 success
 *   @retval -1 failed to read file
 */
int
ReadHdr1(IHandle_t * ih, char *to, int size, u_int magic, u_int version)
{
    struct versionStamp *vsn;
    int bad = 0;
    int code;

    vsn = (struct versionStamp *)to;

    code = IH_IREAD(ih, 0, to, size);
    if (code != size)
	return -1;

    if (vsn->magic != magic) {
	bad++;
	fprintf(stderr, "%s: Inode %s: Bad magic %x (%x): IGNORED\n",
		progname, PrintInode(NULL, ih->ih_ino), vsn->magic, magic);
    }

    /* Check is conditional, in case caller wants to inspect version himself */
    if (version && vsn->version != version) {
	bad++;
	fprintf(stderr, "%s: Inode %s: Bad version %x (%x): IGNORED\n",
		progname,
		PrintInode(NULL, ih->ih_ino), vsn->version, version);
    }
    if (bad && FixHeader) {
	vsn->magic = magic;
	vsn->version = version;
	printf("Special index inode %s has a bad header. Reconstructing...\n",
	       PrintInode(NULL, ih->ih_ino));
	code = IH_IWRITE(ih, 0, to, size);
	if (code != size) {
	    fprintf(stderr,
		    "%s: Write failed for inode %s; header left in damaged state\n",
		    progname, PrintInode(NULL, ih->ih_ino));
	}
    } else {
	if (DumpInfo) {
	    printf("Inode %s: Good magic %x and version %x\n",
		   PrintInode(NULL, ih->ih_ino), magic, version);
	}
    }
    return 0;
}

/**
 * Simplified attach volume
 *
 * param[in] dp       vice disk partition object
 * param[in] volname  volume header file name
 * param[in] header   volume header object
 *
 * @return volume object or null on error
 */
Volume *
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

/**
 * Convert the partition device number into a partition name.
 *
 * @param[in]   partId      partition number, 0 to 254
 * @param[out]  partName    buffer to hold partition name (e.g. /vicepa)
 * @param[in]   partNameSize    size of partName buffer
 *
 * @return status
 *   @retval 0 success
 *   @retval -1 error, partId is out of range
 *   @retval -2 error, partition name exceeds partNameSize
 */
static int
GetPartitionName(afs_uint32 partId, char *partName, afs_size_t partNameSize)
{
    const int OLD_NUM_DEVICES = 26;	/* a..z */

    if (partId < OLD_NUM_DEVICES) {
	if (partNameSize < 8) {
	    return -2;
	}
	strlcpy(partName, "/vicep", partNameSize);
	partName[6] = partId + 'a';
	partName[7] = '\0';
	return 0;
    }
    if (partId < VOLMAXPARTS) {
	if (partNameSize < 9) {
	    return -2;
	}
	strlcpy(partName, "/vicep", partNameSize);
	partId -= OLD_NUM_DEVICES;
	partName[6] = 'a' + (partId / OLD_NUM_DEVICES);
	partName[7] = 'a' + (partId % OLD_NUM_DEVICES);
	partName[8] = '\0';
	return 0;
    }
    return -1;
}

/**
 * Process command line options and start scanning
 *
 * @param[in] as     command syntax object
 * @param[in] arock  opaque object; not used
 *
 * @return error code
 */
static int
handleit(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    int err = 0;
    afs_uint32 volumeId = 0;
    char *partNameOrId = 0;
    char partName[64] = "";
    struct DiskPartition64 *partP = NULL;


#ifndef AFS_NT40_ENV
    if (geteuid() != 0) {
	fprintf(stderr, "%s: Must be run as root; sorry\n", progname);
	return 1;
    }
#endif

    if (as->parms[P_ONLINE].items) {
	fprintf(stderr, "%s: -online not supported\n", progname);
	return 1;
    }
    if (as->parms[P_VNODE].items) {
	DumpVnodes = 1;
    }
    if (as->parms[P_DATE].items) {
	DumpDate = 1;
    }
    if (as->parms[P_INODE].items) {
	DumpInodeNumber = 1;
    }
    if (as->parms[P_ITIME].items) {
	InodeTimes = 1;
    }
    if ((ti = as->parms[P_PART].items)) {
	partNameOrId = ti->data;
    }
    if ((ti = as->parms[P_VOLUMEID].items)) {
	volumeId = strtoul(ti->data, NULL, 10);
    }
    if (as->parms[P_HEADER].items) {
	DumpHeader = 1;
    }
    if (as->parms[P_SIZEONLY].items || as->parms[P_SIZEONLY_COMPAT].items) {
	ShowSizes = 1;
    }
    if (as->parms[P_FIXHEADER].items) {
	FixHeader = 1;
    }
    if (as->parms[P_SAVEINODES].items) {
	SaveInodes = 1;
    }
    if (as->parms[P_ORPHANED].items) {
	ShowOrphaned = 1;
    }
#if defined(AFS_NAMEI_ENV)
    if (as->parms[P_FILENAMES].items) {
	PrintFileNames = 1;
    }
#endif

    /* -saveinodes and -sizeOnly override the default mode.
     * For compatibility with old versions of volinfo, -orphaned
     * and -filename options imply -vnodes */
    if (SaveInodes || ShowSizes) {
	DumpInfo = 0;
	DumpHeader = 0;
	DumpVnodes = 0;
	InodeTimes = 0;
	ShowOrphaned = 0;
    } else if (ShowOrphaned) {
	DumpVnodes = 1;		/* implied */
#ifdef AFS_NAMEI_ENV
    } else if (PrintFileNames) {
	DumpVnodes = 1;		/* implied */
#endif
    }

    /* Allow user to specify partition by name or id. */
    if (partNameOrId) {
	afs_uint32 partId = volutil_GetPartitionID(partNameOrId);
	if (partId == -1) {
	    fprintf(stderr, "%s: Could not parse '%s' as a partition name.\n",
		    progname, partNameOrId);
	    return 1;
	}
	if (GetPartitionName(partId, partName, sizeof(partName))) {
	    fprintf(stderr,
		    "%s: Could not format '%s' as a partition name.\n",
		    progname, partNameOrId);
	    return 1;
	}
    }

    DInit(10);

    err = VAttachPartitions();
    if (err) {
	fprintf(stderr, "%s: %d partitions had errors during attach.\n",
		progname, err);
    }

    if (partName[0]) {
	partP = VGetPartition(partName, 0);
	if (!partP) {
	    fprintf(stderr,
		    "%s: %s is not an AFS partition name on this server.\n",
		    progname, partName);
	    return 1;
	}
    }

    if (!volumeId) {
	if (!partP) {
	    HandleAllPart();
	} else {
	    HandlePart(partP);
	}
    } else {
	char name1[128];

	if (!partP) {
	    partP = FindCurrentPartition();
	    if (!partP) {
		fprintf(stderr,
			"%s: Current partition is not a vice partition.\n",
			progname);
		return 1;
	    }
	}
	(void)afs_snprintf(name1, sizeof name1, VFORMAT,
			   afs_printable_uint32_lu(volumeId));
	HandleVolume(partP, name1);
    }
    return 0;
}

/**
 * Determine if the current directory is a vice partition
 *
 * @return disk partition object
 */
#ifdef AFS_NT40_ENV
#include <direct.h>
struct DiskPartition64 *
FindCurrentPartition(void)
{
    int dr = _getdrive();
    struct DiskPartition64 *dp;

    dr--;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	if (*dp->devName - 'A' == dr)
	    break;
    }
    if (!dp) {
	fprintf(stderr, "%s: Current drive is not a valid vice partition.\n",
		progname);
    }
    return dp;
}
#else
struct DiskPartition64 *
FindCurrentPartition(void)
{
    char partName[1024];
    char tmp = '\0';
    char *p;
    struct DiskPartition64 *dp;

    if (!getcwd(partName, 1023)) {
	fprintf(stderr, "%s: Failed to get current working directory: ",
		progname);
	perror("pwd");
	return NULL;
    }
    p = strchr(&partName[1], OS_DIRSEPC);
    if (p) {
	tmp = *p;
	*p = '\0';
    }
    if (!(dp = VGetPartition(partName, 0))) {
	if (tmp)
	    *p = tmp;
	fprintf(stderr, "%s: %s is not a valid vice partition.\n", progname,
		partName);
	return NULL;
    }
    return dp;
}
#endif

/**
 * Scan and handle all the partitions detected on this server
 *
 * @return none
 */
void
HandleAllPart(void)
{
    struct DiskPartition64 *partP;


    for (partP = DiskPartitionList; partP; partP = partP->next) {
	printf("Processing Partition %s:\n", partP->name);
	HandlePart(partP);
	if (ShowSizes) {
	    AddSizeTotals(&serverTotals, &partitionTotals);
	}
    }

    if (ShowSizes) {
	PrintServerTotals();
    }
}

/**
 * Scan the partition and handle volumes
 *
 * @param[in] partP  disk partition to scan
 *
 * @return none
 */
void
HandlePart(struct DiskPartition64 *partP)
{
    afs_int64 nvols = 0;
    DIR *dirp;
    struct dirent *dp;
#ifdef AFS_NT40_ENV
    char pname[64];
    char *p = pname;
    (void)sprintf(pname, "%s\\", VPartitionPath(partP));
#else
    char *p = VPartitionPath(partP);
#endif

    if ((dirp = opendir(p)) == NULL) {
	fprintf(stderr, "%s: Can't read directory %s; giving up\n", progname,
		p);
	return;
    }
    while ((dp = readdir(dirp))) {
	p = (char *)strrchr(dp->d_name, '.');
	if (p != NULL && strcmp(p, VHDREXT) == 0) {
	    HandleVolume(partP, dp->d_name);
	    if (ShowSizes) {
		nvols++;
		AddSizeTotals(&partitionTotals, &volumeTotals);
	    }
	}
    }
    closedir(dirp);
    if (ShowSizes) {
	PrintPartitionTotals(nvols);
    }
}

/**
 * Inspect a volume header special file.
 *
 * @param[in]  name       descriptive name of the type of header special file
 * @param[in]  dp         partition object for this volume
 * @param[in]  header     header object for this volume
 * @param[in]  inode      fileserver inode number for this header special file
 * @param[out] psize      total of the header special file
 *
 * @return none
 */
void
HandleSpecialFile(const char *name, struct DiskPartition64 *dp,
		  struct VolumeHeader *header, Inode inode,
		  afs_sfsize_t * psize)
{
    afs_sfsize_t size = 0;
    IHandle_t *ih = NULL;
    FdHandle_t *fdP = NULL;
#ifdef AFS_NAMEI_ENV
    namei_t filename;
#endif /* AFS_NAMEI_ENV */

    IH_INIT(ih, dp->device, header->parent, inode);
    fdP = IH_OPEN(ih);
    if (fdP == NULL) {
	fprintf(stderr,
		"%s: Error opening header file '%s' for volume %u", progname,
		name, header->id);
	perror("open");
	goto error;
    }
    size = FDH_SIZE(fdP);
    if (size == -1) {
	fprintf(stderr,
		"%s: Error getting size of header file '%s' for volume %u",
		progname, name, header->id);
	perror("fstat");
	goto error;
    }
    if (DumpInfo) {
	printf("\t%s inode\t= %s (size = %lld)\n",
	       name, PrintInode(NULL, inode), size);
#ifdef AFS_NAMEI_ENV
	namei_HandleToName(&filename, ih);
	printf("\t%s namei\t= %s\n", name, filename.n_path);
#endif /* AFS_NAMEI_ENV */
    }
    *psize += size;

  error:
    if (fdP != NULL) {
	FDH_REALLYCLOSE(fdP);
    }
    if (ih != NULL) {
	IH_RELEASE(ih);
    }
}

/**
 * Inspect this volume header files.
 *
 * @param[in]  dp         partition object for this volume
 * @param[in]  header_fd  volume header file descriptor
 * @param[in]  header     volume header object
 * @param[out] psize      total of the header special file
 *
 * @return none
 */
void
HandleHeaderFiles(struct DiskPartition64 *dp, FD_t header_fd,
		  struct VolumeHeader *header)
{
    afs_sfsize_t size = 0;

    if (DumpInfo) {
	size = OS_SIZE(header_fd);
	printf("Volume header (size = %lld):\n", size);
	printf("\tstamp\t= 0x%x\n", header->stamp.version);
	printf("\tVolId\t= %u\n", header->id);
	printf("\tparent\t= %u\n", header->parent);
    }

    HandleSpecialFile("Info", dp, header, header->volumeInfo, &size);
    HandleSpecialFile("Small", dp, header, header->smallVnodeIndex,
		      &size);
    HandleSpecialFile("Large", dp, header, header->largeVnodeIndex,
		      &size);
#ifdef AFS_NAMEI_ENV
    HandleSpecialFile("Link", dp, header, header->linkTable, &size);
#endif /* AFS_NAMEI_ENV */

    if (DumpInfo) {
	printf("Total aux volume size = %lld\n\n", size);
    }

    if (ShowSizes) {
        volumeTotals.auxsize = size;
        volumeTotals.auxsize_k = size / 1024;
    }
}

/**
 * Attach and scan the volume and handle the header and vnodes
 *
 * Print the volume header and vnode information, depending on the
 * current modes.
 *
 * @param[in] dp    vice partition object for this volume
 * @param[in] name  volume header file name
 *
 * @return none
 */
void
HandleVolume(struct DiskPartition64 *dp, char *name)
{
    struct VolumeHeader header;
    struct VolumeDiskHeader diskHeader;
    struct afs_stat status;
    int fd;
    Volume *vp;
    char headerName[1024];
    afs_sfsize_t n;

    (void)afs_snprintf(headerName, sizeof headerName, "%s" OS_DIRSEP "%s",
		       VPartitionPath(dp), name);
    if ((fd = afs_open(headerName, O_RDONLY)) == -1) {
	fprintf(stderr, "%s: Cannot open volume header %s\n", progname, name);
	return;
    }

    if (afs_fstat(fd, &status) == -1) {
	fprintf(stderr, "%s: Cannot read volume header %s\n", progname, name);
	close(fd);
	return;
    }
    n = read(fd, &diskHeader, sizeof(diskHeader));
    if (n != sizeof(diskHeader)
	|| diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
	fprintf(stderr, "%s: Error reading volume header %s\n", progname,
		name);
	close(fd);
	return;
    }
    if (diskHeader.stamp.version != VOLUMEHEADERVERSION) {
	fprintf(stderr,
		"%s: Volume %s, version number is incorrect; volume needs to be salvaged\n",
		progname, name);
	close(fd);
	return;
    }
    DiskToVolumeHeader(&header, &diskHeader);
    if (DumpHeader || ShowSizes) {
	HandleHeaderFiles(dp, fd, &header);
    }
    close(fd);
    vp = AttachVolume(dp, name, &header);
    if (!vp) {
	fprintf(stderr, "%s: Error attaching volume header %s\n",
		progname, name);
	return;
    }
    if (DumpInfo) {
	PrintHeader(vp);
    }
    if (DumpVnodes || ShowSizes || ShowOrphaned) {
	HandleVnodes(vp, vLarge);
    }
    if (DumpVnodes || ShowSizes || SaveInodes || ShowOrphaned) {
	HandleVnodes(vp, vSmall);
    }
    if (ShowSizes) {
	volumeTotals.diskused_k = V_diskused(vp);
	volumeTotals.size_k =
	    volumeTotals.auxsize_k + volumeTotals.vnodesize_k;
	PrintVolumeSizes(vp);
    }
    free(vp->header);
    free(vp);
}


/**
 * volinfo program entry
 */
int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    afs_int32 code;

    ts = cmd_CreateSyntax(NULL, handleit, NULL, "Dump volume's internal state");
    cmd_AddParm(ts, "-online", CMD_FLAG, CMD_OPTIONAL,
		"Get info from running fileserver");
    cmd_AddParm(ts, "-vnode", CMD_FLAG, CMD_OPTIONAL, "Dump vnode info");
    cmd_AddParm(ts, "-date", CMD_FLAG, CMD_OPTIONAL,
		"Also dump vnode's mod date");
    cmd_AddParm(ts, "-inode", CMD_FLAG, CMD_OPTIONAL,
		"Also dump vnode's inode number");
    cmd_AddParm(ts, "-itime", CMD_FLAG, CMD_OPTIONAL,
		"Dump special inode's mod times");
    cmd_AddParm(ts, "-part", CMD_LIST, CMD_OPTIONAL,
		"AFS partition name or id (default current partition)");
    cmd_AddParm(ts, "-volumeid", CMD_LIST, CMD_OPTIONAL, "Volume id");
    cmd_AddParm(ts, "-header", CMD_FLAG, CMD_OPTIONAL,
		"Dump volume's header info");
    cmd_AddParm(ts, "-sizeonly", CMD_FLAG, CMD_OPTIONAL,
		"Dump volume's size");
    /* For compatibility with older versions. */
    cmd_AddParm(ts, "-sizeOnly", CMD_FLAG, CMD_OPTIONAL | CMD_HIDE,
		"Alias for -sizeonly");
    cmd_AddParm(ts, "-fixheader", CMD_FLAG, CMD_OPTIONAL,
		"Try to fix header");
    cmd_AddParm(ts, "-saveinodes", CMD_FLAG, CMD_OPTIONAL,
		"Try to save all inodes");
    cmd_AddParm(ts, "-orphaned", CMD_FLAG, CMD_OPTIONAL,
		"List all dir/files without a parent");
#if defined(AFS_NAMEI_ENV)
    cmd_AddParm(ts, "-filenames", CMD_FLAG, CMD_OPTIONAL, "Also dump vnode's namei filename");
#endif
    code = cmd_Dispatch(argc, argv);
    return code;
}

#define typestring(type) (type == RWVOL? "read/write": type == ROVOL? "readonly": type == BACKVOL? "backup": "unknown")

/**
 * Print the volume header information
 *
 * @param[in]  volume object
 *
 * @return none
 */
void
PrintHeader(Volume * vp)
{
    printf("Volume header for volume %u (%s)\n", V_id(vp), V_name(vp));
    printf("stamp.magic = %x, stamp.version = %u\n", V_stamp(vp).magic,
	   V_stamp(vp).version);
    printf
	("inUse = %d, inService = %d, blessed = %d, needsSalvaged = %d, dontSalvage = %d\n",
	 V_inUse(vp), V_inService(vp), V_blessed(vp), V_needsSalvaged(vp),
	 V_dontSalvage(vp));
    printf
	("type = %d (%s), uniquifier = %u, needsCallback = %d, destroyMe = %x\n",
	 V_type(vp), typestring(V_type(vp)), V_uniquifier(vp),
	 V_needsCallback(vp), V_destroyMe(vp));
    printf
	("id = %u, parentId = %u, cloneId = %u, backupId = %u, restoredFromId = %u\n",
	 V_id(vp), V_parentId(vp), V_cloneId(vp), V_backupId(vp),
	 V_restoredFromId(vp));
    printf
	("maxquota = %d, minquota = %d, maxfiles = %d, filecount = %d, diskused = %d\n",
	 V_maxquota(vp), V_minquota(vp), V_maxfiles(vp), V_filecount(vp),
	 V_diskused(vp));
    printf("creationDate = %s, copyDate = %s\n", date(V_creationDate(vp)),
	   date(V_copyDate(vp)));
    printf("backupDate = %s, expirationDate = %s\n", date(V_backupDate(vp)),
	   date(V_expirationDate(vp)));
    printf("accessDate = %s, updateDate = %s\n", date(V_accessDate(vp)),
	   date(V_updateDate(vp)));
    printf("owner = %u, accountNumber = %u\n", V_owner(vp),
	   V_accountNumber(vp));
    printf
	("dayUse = %u; week = (%u, %u, %u, %u, %u, %u, %u), dayUseDate = %s\n",
	 V_dayUse(vp), V_weekUse(vp)[0], V_weekUse(vp)[1], V_weekUse(vp)[2],
	 V_weekUse(vp)[3], V_weekUse(vp)[4], V_weekUse(vp)[5],
	 V_weekUse(vp)[6], date(V_dayUseDate(vp)));
    printf("volUpdateCounter = %u\n", V_volUpCounter(vp));
}

/**
 * Get the size and times of a file
 *
 * @param[in]  fd     file descriptor of file to stat
 * @param[out] size   size of the file
 * @param[out] ctime  ctime of file as a formatted string
 * @param[out] mtime  mtime of file as a formatted string
 * @param[out] atime  atime of file as a formatted string
 *
 * @return error code
 *   @retval 0 success
 *   @retval -1 failed to retrieve file information
 */
static int
GetFileInfo(FD_t fd, afs_sfsize_t * size, char **ctime, char **mtime,
	    char **atime)
{
#ifdef AFS_NT40_ENV
    BY_HANDLE_FILE_INFORMATION fi;
    LARGE_INTEGER fsize;
    if (!GetFileInformationByHandle(fd, &fi)) {
	fprintf(stderr, "%s: GetFileInformationByHandle failed\n", progname);
	return -1;
    }
    if (!GetFileSizeEx(fd, &fsize)) {
	fprintf(stderr, "%s: GetFileSizeEx failed\n", progname);
	return -1;
    }
    *size = fsize.QuadPart;
    *ctime = "N/A";
    *mtime = NT_date(&fi.ftLastWriteTime);
    *atime = NT_date(&fi.ftLastAccessTime);
#else
    struct afs_stat status;
    if (afs_fstat(fd, &status) == -1) {
	fprintf(stderr, "%s: fstat failed %d\n", progname, errno);
	return -1;
    }
    *size = status.st_size;
    *ctime = date(status.st_ctime);
    *mtime = date(status.st_mtime);
    *atime = date(status.st_atime);
#endif
    return 0;
}

/**
 * Copy the inode data to a file in the current directory.
 *
 * @param[in] vp     volume object
 * @param[in] vnode  vnode object
 * @param[in] inode  inode of the source file
 *
 * @return none
 */
static void
SaveInode(Volume * vp, struct VnodeDiskObject *vnode, Inode ino)
{
    IHandle_t *ih;
    FdHandle_t *fdP;
    char nfile[50], buffer[256];
    int ofd = 0;
    afs_foff_t total;
    ssize_t len;

    if (!VALID_INO(ino)) {
        return;
    }

    IH_INIT(ih, V_device(vp), V_parentId(vp), ino);
    fdP = IH_OPEN(ih);
    if (fdP == NULL) {
	fprintf(stderr,
		"%s: Can't open inode %s error %d (ignored)\n",
		progname, PrintInode(NULL, ino), errno);
	return;
    }
    (void)afs_snprintf(nfile, sizeof nfile, "TmpInode.%s", PrintInode(NULL, ino));
    ofd = afs_open(nfile, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (ofd < 0) {
	fprintf(stderr,
		"%s: Can't create file %s; error %d (ignored)\n",
		progname, nfile, errno);

	FDH_REALLYCLOSE(fdP);
	IH_RELEASE(ih);
	return;
    }
    total = 0;
    while (1) {
	ssize_t nBytes;
	len = FDH_PREAD(fdP, buffer, sizeof(buffer), total);
	if (len < 0) {
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ih);
	    close(ofd);
	    unlink(nfile);
	    fprintf(stderr,
		    "%s: Error while reading from inode %s (%d)\n",
		    progname, PrintInode(NULL, ino), errno);
	    return;
	}
	if (len == 0)
	    break;		/* No more input */
	nBytes = write(ofd, buffer, len);
	if (nBytes != len) {
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ih);
	    close(ofd);
	    unlink(nfile);
	    fprintf(stderr,
		    "%s: Error while writing to \"%s\" (%d - ignored)\n",
		    progname, nfile, errno);
	    return;
	}
	total += len;
    }

    FDH_REALLYCLOSE(fdP);
    IH_RELEASE(ih);
    close(ofd);
    printf("... Copied inode %s to file %s (%lu bytes)\n",
	   PrintInode(NULL, ino), nfile, (unsigned long)total);
}

/**
 * Scan a volume index and handle each vnode
 *
 * @param[in] vp      volume object
 * @param[in] class   which index to scan
 *
 * @return none
 */
void
HandleVnodes(Volume * vp, VnodeClass class)
{
    afs_int32 diskSize =
	(class == vSmall ? SIZEOF_SMALLDISKVNODE : SIZEOF_LARGEDISKVNODE);
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    StreamHandle_t *file = NULL;
    int vnodeIndex;
    afs_sfsize_t nVnodes;
    afs_foff_t offset = 0;
    Inode ino;
    IHandle_t *ih = vp->vnodeIndex[class].handle;
    FdHandle_t *fdP = NULL;
    afs_sfsize_t size;
    char *ctime, *atime, *mtime;

    /* print vnode table heading */
    if (class == vLarge) {
	if (DumpInfo) {
	    printf("\nLarge vnodes (directories)\n");
	}
    } else {
	if (DumpInfo) {
	    printf("\nSmall vnodes(files, symbolic links)\n");
	    fflush(stdout);
	}
	if (SaveInodes) {
	    printf("Saving all volume files to current directory ...\n");
	    if (ShowSizes) {
		PrintingVolumeSizes = 0;	/* print heading again */
	    }
	}
    }

    fdP = IH_OPEN(ih);
    if (fdP == NULL) {
	fprintf(stderr, "%s: open failed: ", progname);
	goto error;
    }

    file = FDH_FDOPEN(fdP, "r");
    if (!file) {
	fprintf(stderr, "%s: fdopen failed\n", progname);
	goto error;
    }

    if (GetFileInfo(fdP->fd_fd, &size, &ctime, &atime, &mtime) != 0) {
	goto error;
    }
    if (InodeTimes) {
	printf("ichanged : %s\nimodified: %s\niaccessed: %s\n\n", ctime,
	       mtime, atime);
    }

    nVnodes = (size / diskSize) - 1;
    if (nVnodes > 0) {
	STREAM_ASEEK(file, diskSize);
    } else
	nVnodes = 0;

    for (vnodeIndex = 0;
	 nVnodes && STREAM_READ(vnode, diskSize, 1, file) == 1;
	 nVnodes--, vnodeIndex++, offset += diskSize) {

	ino = VNDISK_GET_INO(vnode);
	if (ShowSizes) {
	    afs_fsize_t fileLength;

	    VNDISK_GET_LEN(fileLength, vnode);
	    if (fileLength > 0) {
		volumeTotals.vnodesize += fileLength;
		volumeTotals.vnodesize_k += fileLength / 1024;
	    }
	}
	if (SaveInodes && (class == vSmall)) {
	    SaveInode(vp, vnode, ino);
	}
	if (DumpVnodes || ShowOrphaned) {
	    PrintVnode(offset, vnode,
		       bitNumberToVnodeNumber(vnodeIndex, class), ino, vp);
	}
    }

  error:
    if (file) {
	STREAM_CLOSE(file);
    }
    if (fdP) {
	FDH_CLOSE(fdP);
    }
}

/**
 * Print vnode information
 *
 * @param[in] offset       index offset of this vnode
 * @param[in] vnode        vnode object to be printed
 * @param[in] vnodeNumber  vnode number
 * @param[in] ino          fileserver inode number
 * @param[in] vp           parent volume of the vnode
 *
 * @return none
 */
void
PrintVnode(afs_foff_t offset, VnodeDiskObject * vnode, VnodeId vnodeNumber,
	   Inode ino, Volume * vp)
{
#if defined(AFS_NAMEI_ENV)
    IHandle_t *ihtmpp;
    namei_t filename;
#endif
    afs_fsize_t fileLength;

    VNDISK_GET_LEN(fileLength, vnode);

    /* The check for orphaned vnodes is currently limited to non-empty
     * vnodes with a parent of zero (and which are not the first entry
     * in the index). */
    if (ShowOrphaned && (fileLength == 0 || vnode->parent || !offset))
	return;

    printf
	("%10lld Vnode %u.%u.%u cloned: %u, length: %llu linkCount: %d parent: %u",
	 (long long)offset, vnodeNumber, vnode->uniquifier, vnode->dataVersion,
	 vnode->cloned, (afs_uintmax_t) fileLength, vnode->linkCount,
	 vnode->parent);
    if (DumpInodeNumber)
	printf(" inode: %s", PrintInode(NULL, ino));
    if (DumpDate)
	printf(" ServerModTime: %s", date(vnode->serverModifyTime));
#if defined(AFS_NAMEI_ENV)
    if (PrintFileNames) {
	IH_INIT(ihtmpp, V_device(vp), V_parentId(vp), ino);
	namei_HandleToName(&filename, ihtmpp);
#if !defined(AFS_NT40_ENV)
	printf(" UFS-Filename: %s", filename.n_path);
#else
	printf(" NTFS-Filename: %s", filename.n_path);
#endif
    }
#endif
    printf("\n");
}
