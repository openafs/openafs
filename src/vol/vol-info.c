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

#include <roken.h>

#include <ctype.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include <afs/cmd.h>
#include <afs/dir.h>
#include <afs/afsint.h>
#include <afs/errors.h>
#include <afs/acl.h>
#include <afs/prs_fs.h>
#include <rx/rx_queue.h>

#include "nfs.h"
#include "lock.h"
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include "salvage.h"
#include "daemon_com_inline.h"
#include "fssync_inline.h"

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

static const char *progname = NULL;
static const char *PLACEHOLDER = "-";

/* Command line options */
typedef enum {
    P_CHECKOUT,
    P_VNODE,
    P_DATE,
    P_INODE,
    P_ITIME,
    P_PART,
    P_VOLUMEID,
    P_HEADER,
    P_SIZEONLY,
    P_FIXHEADER,
    P_SAVEINODES,
    P_ORPHANED,
    P_FILENAMES,
    P_TYPE,
    P_FIND,
    P_MASK,
    P_OUTPUT,
    P_DELIM,
    P_NOHEADING,
    P_NOMAGIC,
} volinfo_parm_t;

/*
 * volscan output columns
 */
#define VOLSCAN_COLUMNS \
    c(host) \
    c(desc) \
    c(vid) \
    c(offset) \
    c(vtype) \
    c(vname) \
    c(part) \
    c(partid) \
    c(fid) \
    c(path) \
    c(target) \
    c(mount) \
    c(mtype) \
    c(mcell) \
    c(mvol) \
    c(aid) \
    c(arights) \
    c(vntype) \
    c(cloned) \
    c(mode) \
    c(links) \
    c(length) \
    c(uniq) \
    c(dv) \
    c(inode) \
    c(namei) \
    c(modtime) \
    c(author) \
    c(owner) \
    c(parent) \
    c(magic) \
    c(lock) \
    c(smodtime) \
    c(group)

/* Numeric column type ids */
typedef enum columnType {
#define c(x) col_##x,
    VOLSCAN_COLUMNS
#undef c
    max_column_type
} columnType;

struct columnName {
    columnType type;
    const char *name;
};

/* Table of id:name tuples of possible columns. */
struct columnName ColumnName[] = {
#define c(x) { col_##x, #x },
    VOLSCAN_COLUMNS
#undef c
    {max_column_type, NULL}
};

/* All columns names as a single string. */
#define c(x) #x " "
static char *ColumnNames = VOLSCAN_COLUMNS;
#undef c
#undef VOLSCAN_COLUMNS


/* VnodeDetails union descriminator */
typedef enum {
    VNODE_U_NONE,
    VNODE_U_MOUNT,
    VNODE_U_SYMLINK,
    VNODE_U_POS_ACCESS,
    VNODE_U_NEG_ACCESS
} vnode_details_u_t;

struct VnodeDetails {
    Volume *vp;
    VnodeClass class;
    VnodeDiskObject *vnode;
    VnodeId vnodeNumber;
    afs_foff_t offset;
    int index;
    char *path;
    vnode_details_u_t t;
    union {
	struct {
	    char type;
	    char *cell;
	    char *vol;
	} mnt;
	char *target;
	struct acl_accessEntry *access;
    } u;
};

/* Modes */
static int Checkout = 0;            /**< Use FSSYNC to checkout volumes from the fileserver. */
static int DumpInfo = 0;            /**< Dump volume information */
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
static char Hostname[64] = "";      /**< This hostname, for volscan output. */
static int NeedDirIndex = 0;        /**< Large vnode index handle is needed for path lookups. */
static char ColumnDelim[16] = " ";  /**< Column delimiter char(s) */
static char PrintHeading = 0;       /**< Print column heading */
static int CheckMagic = 1;          /**< Check directory vnode magic when looking up paths */
static unsigned int ModeMask[64];

static FdHandle_t *DirIndexFd = NULL; /**< Current large vnode index handle for path lookups. */

static int NumOutputColumns = 0;
static columnType OutputColumn[max_column_type];

#define SCAN_RW  0x01		/**< scan read-write volumes vnodes */
#define SCAN_RO  0x02		/**< scan read-only volume vnodes */
#define SCAN_BK  0x04		/**< scan backup volume vnodes */
static int ScanVolType = 0;	/**< volume types to scan; zero means do not check */

#define FIND_FILE       0x01	/**< find regular files */
#define FIND_DIR        0x02	/**< find directories */
#define FIND_MOUNT      0x04	/**< find afs mount points */
#define FIND_SYMLINK    0x08	/**< find symlinks */
#define FIND_ACL        0x10	/**< find access entiries */
static int FindVnType = 0;	/**< types of objects to find */

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

/**
 * List of procedures to call when scanning vnodes.
 */
struct VnodeScanProc {
    struct opr_queue link;
    const char *heading;
    void (*proc) (struct VnodeDetails * vdp);
};
static struct opr_queue VnodeScanLists[nVNODECLASSES];

/* Forward Declarations */
void PrintHeader(Volume * vp);
void HandleAllPart(void);
void HandlePart(struct DiskPartition64 *partP);
void HandleVolume(struct DiskPartition64 *partP, char *name);
struct DiskPartition64 *FindCurrentPartition(void);
Volume *AttachVolume(struct DiskPartition64 *dp, char *volname,
		     struct VolumeHeader *header);
void HandleVnodes(Volume * vp, VnodeClass class);
static void AddVnodeToSizeTotals(struct VnodeDetails *vdp);
static void SaveInode(struct VnodeDetails *vdp);
static void PrintVnode(struct VnodeDetails *vdp);
static void PrintVnodeDetails(struct VnodeDetails *vdp);
static void ScanAcl(struct VnodeDetails *vdp);
static void PrintColumnHeading(void);
static void PrintColumns(struct VnodeDetails *vdp, const char *desc);

/* externs */
extern void SetSalvageDirHandle(DirHandle * dir, afs_int32 volume,
				Device device, Inode inode,
				int *volumeChanged);
extern void FidZap(DirHandle * file);

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
    snprintf(results[next = (next + 1) & 7], MAX_DATE_RESULT,
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
 * Add vnode size to the running volume totals.
 *
 * @param[in]  vdp   vnode details object
 *
 * @return none
 */
static void
AddVnodeToSizeTotals(struct VnodeDetails *vdp)
{
    afs_fsize_t fileLength;

    VNDISK_GET_LEN(fileLength, vdp->vnode);
    if (fileLength > 0) {
	volumeTotals.vnodesize += fileLength;
	volumeTotals.vnodesize_k += fileLength / 1024;
    }
}

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
    printf("%" AFS_VOLID_FMT "\t%9llu%9llu%10llu%10llu%9lld\t%24s\n",
	   afs_printable_VolumeId_lu(V_id(vp)),
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
ReadHdr1(IHandle_t * ih, char *to, afs_sfsize_t size, u_int magic, u_int version)
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
    }
    if (!bad && DumpInfo) {
	printf("Inode %s: Good magic %x and version %x\n",
	       PrintInode(NULL, ih->ih_ino), magic, version);
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

    if (Checkout) {
	afs_int32 code;
	SYNC_response response;
	VolumeId volid = header->id;

	memset(&response, 0, sizeof(response));
	code =
	    FSYNC_VolOp(volid, dp->name, FSYNC_VOL_NEEDVOLUME, V_DUMP,
			&response);
	if (code != SYNC_OK) {
	    if (response.hdr.reason == FSYNC_SALVAGE) {
		fprintf(stderr,
			"%s: file server says volume %lu is salvaging.\n",
			progname, afs_printable_uint32_lu(volid));
		return NULL;
	    } else {
		fprintf(stderr,
			"%s: attach of volume %lu denied by file server.\n",
			progname, afs_printable_uint32_lu(volid));
		return NULL;
	    }
	}
    }

    vp = (Volume *) calloc(1, sizeof(Volume));
    if (!vp) {
	fprintf(stderr, "%s: Failed to allocate volume object.\n", progname);
	return NULL;
    }
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
    if (!vp->header) {
	fprintf(stderr, "%s: Failed to allocate volume header.\n", progname);
	free(vp);
	return NULL;
    }
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
 * Simplified detach volume
 *
 * param[in] vp       volume object from AttachVolume
 *
 * @return none
 */
static void
DetachVolume(Volume * vp)
{
    if (Checkout) {
	afs_int32 code;
	SYNC_response response;
	memset(&response, 0, sizeof(response));

	code = FSYNC_VolOp(V_id(vp), V_partition(vp)->name,
			   FSYNC_VOL_ON, FSYNC_WHATEVER, &response);
	if (code != SYNC_OK) {
	    fprintf(stderr, "%s: FSSYNC error %d (%s)\n", progname, code,
		    SYNC_res2string(code));
	    fprintf(stderr, "%s:  protocol response code was %d (%s)\n",
		    progname, response.hdr.response,
		    SYNC_res2string(response.hdr.response));
	    fprintf(stderr, "%s:  protocol reason code was %d (%s)\n",
		    progname, response.hdr.reason,
		    FSYNC_reason2string(response.hdr.reason));
	}
    }

    IH_RELEASE(vp->vnodeIndex[vLarge].handle);
    IH_RELEASE(vp->vnodeIndex[vSmall].handle);
    IH_RELEASE(vp->diskDataHandle);
    IH_RELEASE(V_linkHandle(vp));
    free(vp->header);
    free(vp);
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
    const afs_uint32 OLD_NUM_DEVICES = 26;	/* a..z */

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
 * Scan the volumes in the partitions
 *
 * Scan the specified volume in the specified partition if both
 * are given. Scan all the volumes in the specified partition if
 * only the partition is given.  If neither a partition nor volume
 * is given, scan all the volumes in all the partitions.  If only
 * the volume is given, attempt to find it in the current working
 * directory.
 *
 * @param[in] partNameOrId   partition name or id to be scannned
 * @param[in] volumeId       volume id to be scanned
 *
 * @return 0 if successful
 */
static int
ScanPartitions(char *partNameOrId, VolumeId volumeId)
{
    int err = 0;
    char partName[64] = "";
    struct DiskPartition64 *partP = NULL;

#ifndef AFS_NT40_ENV
    if (geteuid() != 0) {
	fprintf(stderr, "%s: Must be run as root; sorry\n", progname);
	return 1;
    }
#endif

    if (Checkout) {
	if (!FSYNC_clientInit()) {
	    fprintf(stderr, "%s: Failed to connect to fileserver.\n",
		    progname);
	    return 1;
	}
    }

    DInit(1024);
    VInitVnodes(vLarge, 0);
    VInitVnodes(vSmall, 0);

    /* Allow user to specify partition by name or id. */
    if (partNameOrId) {
	afs_uint32 partId = volutil_GetPartitionID(partNameOrId);
	if (partId == -1) {
	    fprintf(stderr, "%s: Could not parse '%s' as a partition name.\n",
		    progname, partNameOrId);
	    err = 1;
	    goto cleanup;
	}
	if (GetPartitionName(partId, partName, sizeof(partName))) {
	    fprintf(stderr,
		    "%s: Could not format '%s' as a partition name.\n",
		    progname, partNameOrId);
	    err = 1;
	    goto cleanup;
	}
    }

    err = VAttachPartitions();
    if (err) {
	fprintf(stderr, "%s: %d partitions had errors during attach.\n",
		progname, err);
	err = 1;
	goto cleanup;
    }

    if (partName[0]) {
	partP = VGetPartition(partName, 0);
	if (!partP) {
	    fprintf(stderr,
		    "%s: %s is not an AFS partition name on this server.\n",
		    progname, partName);
	    err = 1;
	    goto cleanup;
	}
    }

    if (!volumeId) {
	if (PrintHeading) {
	    PrintColumnHeading();
	}
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
		err = 1;
		goto cleanup;
	    }
	}
	snprintf(name1, sizeof name1, VFORMAT,
		 afs_printable_VolumeId_lu(volumeId));
	if (PrintHeading) {
	    PrintColumnHeading();
	}
	HandleVolume(partP, name1);
    }

  cleanup:
    if (Checkout) {
	FSYNC_clientFinis();
    }
    return err;
}

/**
 * Add a vnode scanning procedure
 *
 * @param[in]   class   vnode class for this handler
 * @param[in]   proc    handler proc to be called by HandleVnodes
 * @param[in]   heading optional string to pring before scanning vnodes
 *
 * @return none
 */
static void
AddVnodeHandler(VnodeClass class, void (*proc) (struct VnodeDetails * vdp),
		const char *heading)
{
    struct VnodeScanProc *entry = malloc(sizeof(struct VnodeScanProc));
    entry->proc = proc;
    entry->heading = heading;
    opr_queue_Append(&VnodeScanLists[class], (struct opr_queue *)entry);
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
VolInfo(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    VolumeId volumeId = 0;
    char *partNameOrId = NULL;

    DumpInfo = 1;		/* volinfo default mode */

    if (as->parms[P_CHECKOUT].items) {
	Checkout = 1;
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
    if (as->parms[P_SIZEONLY].items) {
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

    if (SaveInodes) {
	AddVnodeHandler(vSmall, SaveInode,
			"Saving all volume files to current directory ...\n");
    }
    if (ShowSizes) {
	AddVnodeHandler(vLarge, AddVnodeToSizeTotals, NULL);
	AddVnodeHandler(vSmall, AddVnodeToSizeTotals, NULL);
    }
    if (DumpVnodes) {
	AddVnodeHandler(vLarge, PrintVnode, "\nLarge vnodes (directories)\n");
	AddVnodeHandler(vSmall, PrintVnode,
			"\nSmall vnodes(files, symbolic links)\n");
    }
    return ScanPartitions(partNameOrId, volumeId);
}

/**
 * Add a column type to be displayed.
 *
 * @param[in]  name   column type name
 *
 * @return error code
 *   @retval 0 success
 *   @retval 1 too many columns
 *   @retval 2 invalid column name
 */
static int
AddOutputColumn(char *name)
{
    int i;

    if (NumOutputColumns >= sizeof(OutputColumn) / sizeof(*OutputColumn)) {
	fprintf(stderr, "%s: Too many output columns (%d).\n", progname,
		NumOutputColumns);
	return 1;
    }
    for (i = 0; i < max_column_type; i++) {
	if (!strcmp(ColumnName[i].name, name)) {
	    columnType t = ColumnName[i].type;
	    OutputColumn[NumOutputColumns++] = t;

	    if (t == col_path) {
		NeedDirIndex = 1;
	    }
	    return 0;
	}
    }
    return 2;
}

/**
 * Process command line options and start scanning
 *
 * @param[in] as     command syntax object
 * @param[in] arock  opaque object; not used
 *
 * @return error code
 *    @retval 0 success
 *    @retvol 1 failure
 */
static int
VolScan(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    VolumeId volumeId = 0;
    char *partNameOrId = NULL;
    int i;

    if (as->parms[P_CHECKOUT].items) {
	Checkout = 1;
    }
    if ((ti = as->parms[P_PART].items)) {
	partNameOrId = ti->data;
    }
    if ((ti = as->parms[P_VOLUMEID].items)) {
	volumeId = strtoul(ti->data, NULL, 10);
    }
    if (as->parms[P_NOHEADING].items) {
	PrintHeading = 0;
    } else {
	PrintHeading = 1;
    }
    if (as->parms[P_NOMAGIC].items) {
        CheckMagic = 0;
    }
    if ((ti = as->parms[P_DELIM].items)) {
	strncpy(ColumnDelim, ti->data, 15);
	ColumnDelim[15] = '\0';
    }

    /* Limit types of volumes to scan. */
    if (!as->parms[P_TYPE].items) {
	ScanVolType = (SCAN_RW | SCAN_RO | SCAN_BK);
    } else {
	ScanVolType = 0;
	for (ti = as->parms[P_TYPE].items; ti; ti = ti->next) {
	    if (strcmp(ti->data, "rw") == 0) {
		ScanVolType |= SCAN_RW;
	    } else if (strcmp(ti->data, "ro") == 0) {
		ScanVolType |= SCAN_RO;
	    } else if (strcmp(ti->data, "bk") == 0) {
		ScanVolType |= SCAN_BK;
	    } else {
		fprintf(stderr, "%s: Unknown -type value: %s\n", progname,
			ti->data);
		return 1;
	    }
	}
    }

    /* Limit types of vnodes to find (plus access entries) */
    if (!as->parms[P_FIND].items) {
	FindVnType = (FIND_FILE | FIND_DIR | FIND_MOUNT | FIND_SYMLINK);
    } else {
	FindVnType = 0;
	for (ti = as->parms[P_FIND].items; ti; ti = ti->next) {
	    if (strcmp(ti->data, "file") == 0) {
		FindVnType |= FIND_FILE;
	    } else if (strcmp(ti->data, "dir") == 0) {
		FindVnType |= FIND_DIR;
	    } else if (strcmp(ti->data, "mount") == 0) {
		FindVnType |= FIND_MOUNT;
	    } else if (strcmp(ti->data, "symlink") == 0) {
		FindVnType |= FIND_SYMLINK;
	    } else if (strcmp(ti->data, "acl") == 0) {
		FindVnType |= FIND_ACL;
	    } else {
		fprintf(stderr, "%s: Unknown -find value: %s\n", progname,
			ti->data);
		return 1;
	    }
	}
    }

    /* Show vnodes matching one of the mode masks */
    for (i = 0, ti = as->parms[P_MASK].items; ti; i++, ti = ti->next) {
	if (i >= (sizeof(ModeMask) / sizeof(*ModeMask))) {
	    fprintf(stderr, "Too many -mask values\n");
	    return -1;
	}
	ModeMask[i] = strtol(ti->data, NULL, 8);
	if (!ModeMask[i]) {
	    fprintf(stderr, "Invalid -mask value: %s\n", ti->data);
	    return 1;
	}
    }

    /* Set which columns to be printed. */
    if (!as->parms[P_OUTPUT].items) {
	AddOutputColumn("host");
	AddOutputColumn("desc");
	AddOutputColumn("fid");
	AddOutputColumn("dv");
	if (FindVnType & FIND_ACL) {
	    AddOutputColumn("aid");
	    AddOutputColumn("arights");
	}
	AddOutputColumn("path");
    } else {
	for (ti = as->parms[P_OUTPUT].items; ti; ti = ti->next) {
	    if (AddOutputColumn(ti->data) != 0) {;
		fprintf(stderr, "%s: Unknown -output value: %s\n", progname,
			ti->data);
		return 1;
	    }
	}
    }

    if (FindVnType & FIND_DIR) {
	AddVnodeHandler(vLarge, PrintVnodeDetails, NULL);
    }
    if (FindVnType & (FIND_FILE | FIND_MOUNT | FIND_SYMLINK)) {
	AddVnodeHandler(vSmall, PrintVnodeDetails, NULL);
    }
    if (FindVnType & FIND_ACL) {
	AddVnodeHandler(vLarge, ScanAcl, NULL);
    }

    return ScanPartitions(partNameOrId, volumeId);
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
	if (DumpInfo || SaveInodes || ShowSizes) {
	    printf("Processing Partition %s:\n", partP->name);
	}
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
    afs_sfsize_t size = -1;
    IHandle_t *ih = NULL;
    FdHandle_t *fdP = NULL;
#ifdef AFS_NAMEI_ENV
    namei_t filename;
#endif /* AFS_NAMEI_ENV */

    IH_INIT(ih, dp->device, header->parent, inode);
    fdP = IH_OPEN(ih);
    if (fdP == NULL) {
	fprintf(stderr,
		"%s: Error opening header file '%s' for volume %" AFS_VOLID_FMT "\n", progname,
		name, afs_printable_VolumeId_lu(header->id));
	perror("open");
	goto error;
    }
    size = FDH_SIZE(fdP);
    if (size == -1) {
	fprintf(stderr,
		"%s: Error getting size of header file '%s' for volume %" AFS_VOLID_FMT "\n",
		progname, name, afs_printable_VolumeId_lu(header->id));
	perror("fstat");
	goto error;
    }
    *psize += size;

  error:
    if (DumpInfo) {
	printf("\t%s inode\t= %s (size = ", name, PrintInode(NULL, inode));
	if (size != -1) {
	    printf("%lld)\n", size);
	} else {
	    printf("unknown)\n");
	}
#ifdef AFS_NAMEI_ENV
	namei_HandleToName(&filename, ih);
	printf("\t%s namei\t= %s\n", name, filename.n_path);
#endif /* AFS_NAMEI_ENV */
    }

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
	printf("\tVolId\t= %" AFS_VOLID_FMT "\n", afs_printable_VolumeId_lu(header->id));
	printf("\tparent\t= %" AFS_VOLID_FMT "\n", afs_printable_VolumeId_lu(header->parent));
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
 * Determine if the vnodes of this volume should be scanned.
 *
 * @param[in]  vp   volume object
 *
 * @return true if vnodes should be scanned
 */
static int
IsScannable(Volume * vp)
{
    if (opr_queue_IsEmpty(&VnodeScanLists[vLarge]) &&
	opr_queue_IsEmpty(&VnodeScanLists[vSmall])) {
	return 0;
    }
    if (!ScanVolType) {
	return 1;		/* filtering disabled; do not check vol type */
    }
    switch (V_type(vp)) {
    case RWVOL:
	return ScanVolType & SCAN_RW;
    case ROVOL:
	return ScanVolType & SCAN_RO;
    case BACKVOL:
	return ScanVolType & SCAN_BK;
    default:
	fprintf(stderr, "%s: Volume %" AFS_VOLID_FMT "; Unknown volume type %d\n", progname,
		afs_printable_VolumeId_lu(V_id(vp)), V_type(vp));
	break;
    }
    return 0;
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
    FD_t fd = INVALID_FD;
    Volume *vp = NULL;
    char headerName[1024];
    afs_sfsize_t n;

    snprintf(headerName, sizeof headerName, "%s" OS_DIRSEP "%s",
	     VPartitionPath(dp), name);
    if ((fd = OS_OPEN(headerName, O_RDONLY, 0666)) == INVALID_FD) {
	fprintf(stderr, "%s: Cannot open volume header %s\n", progname, name);
	goto cleanup;
    }
    if (OS_SIZE(fd) < 0) {
	fprintf(stderr, "%s: Cannot read volume header %s\n", progname, name);
	goto cleanup;
    }
    n = OS_READ(fd, &diskHeader, sizeof(diskHeader));
    if (n != sizeof(diskHeader)
	|| diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
	fprintf(stderr, "%s: Error reading volume header %s\n", progname,
		name);
	goto cleanup;
    }
    if (diskHeader.stamp.version != VOLUMEHEADERVERSION) {
	fprintf(stderr,
		"%s: Volume %s, version number is incorrect; volume needs to be salvaged\n",
		progname, name);
	goto cleanup;
    }

    DiskToVolumeHeader(&header, &diskHeader);
    if (DumpHeader || ShowSizes) {
	HandleHeaderFiles(dp, fd, &header);
    }

    vp = AttachVolume(dp, name, &header);
    if (!vp) {
	fprintf(stderr, "%s: Error attaching volume header %s\n",
		progname, name);
	goto cleanup;
    }

    if (DumpInfo) {
	PrintHeader(vp);
    }
    if (IsScannable(vp)) {
	if (NeedDirIndex) {
	    IHandle_t *ih = vp->vnodeIndex[vLarge].handle;
	    DirIndexFd = IH_OPEN(ih);
	    if (DirIndexFd == NULL) {
		fprintf(stderr, "%s: Failed to open index for directories.",
			progname);
	    }
	}

	HandleVnodes(vp, vLarge);
	HandleVnodes(vp, vSmall);

	if (DirIndexFd) {
	    FDH_CLOSE(DirIndexFd);
	    DirIndexFd = NULL;
	}
    }
    if (ShowSizes) {
	volumeTotals.diskused_k = V_diskused(vp);
	volumeTotals.size_k =
	    volumeTotals.auxsize_k + volumeTotals.vnodesize_k;
	if (SaveInodes) {
	    PrintingVolumeSizes = 0;	/* print heading again */
	}
	PrintVolumeSizes(vp);
    }

  cleanup:
    if (fd != INVALID_FD) {
	OS_CLOSE(fd);
    }
    if (vp) {
	DetachVolume(vp);
    }
}

/**
 * Declare volinfo command line syntax
 *
 * @returns none
 */
static void
VolInfoSyntax(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, VolInfo, NULL,
			  "Dump volume's internal state");
    cmd_AddParmAtOffset(ts, P_CHECKOUT, "-checkout", CMD_FLAG, CMD_OPTIONAL,
			"Checkout volumes from running fileserver");
    cmd_AddParmAtOffset(ts, P_VNODE, "-vnode", CMD_FLAG, CMD_OPTIONAL,
			"Dump vnode info");
    cmd_AddParmAtOffset(ts, P_DATE, "-date", CMD_FLAG, CMD_OPTIONAL,
			"Also dump vnode's mod date");
    cmd_AddParmAtOffset(ts, P_INODE, "-inode", CMD_FLAG, CMD_OPTIONAL,
			"Also dump vnode's inode number");
    cmd_AddParmAtOffset(ts, P_ITIME, "-itime", CMD_FLAG, CMD_OPTIONAL,
			"Dump special inode's mod times");
    cmd_AddParmAtOffset(ts, P_PART, "-part", CMD_LIST, CMD_OPTIONAL,
			"AFS partition name or id (default current partition)");
    cmd_AddParmAtOffset(ts, P_VOLUMEID, "-volumeid", CMD_LIST, CMD_OPTIONAL,
			"Volume id");
    cmd_AddParmAtOffset(ts, P_HEADER, "-header", CMD_FLAG, CMD_OPTIONAL,
			"Dump volume's header info");
    cmd_AddParmAtOffset(ts, P_SIZEONLY, "-sizeonly", CMD_FLAG, CMD_OPTIONAL,
			"Dump volume's size");
    cmd_AddParmAtOffset(ts, P_FIXHEADER, "-fixheader", CMD_FLAG,
			CMD_OPTIONAL, "Try to fix header");
    cmd_AddParmAtOffset(ts, P_SAVEINODES, "-saveinodes", CMD_FLAG,
			CMD_OPTIONAL, "Try to save all inodes");
    cmd_AddParmAtOffset(ts, P_ORPHANED, "-orphaned", CMD_FLAG, CMD_OPTIONAL,
			"List all dir/files without a parent");
#if defined(AFS_NAMEI_ENV)
    cmd_AddParmAtOffset(ts, P_FILENAMES, "-filenames", CMD_FLAG,
			CMD_OPTIONAL, "Also dump vnode's namei filename");
#endif

    /* For compatibility with older versions. */
    cmd_AddParmAlias(ts, P_SIZEONLY, "-sizeOnly");
}

/**
 * Declare volscan command line syntax
 *
 * @returns none
 */
static void
VolScanSyntax(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, VolScan, NULL,
			  "Print volume vnode information");

    cmd_AddParmAtOffset(ts, P_CHECKOUT, "-checkout", CMD_FLAG, CMD_OPTIONAL,
			"Checkout volumes from running fileserver");
    cmd_AddParmAtOffset(ts, P_PART, "-partition", CMD_SINGLE, CMD_OPTIONAL,
			"AFS partition name or id (default current partition)");
    cmd_AddParmAtOffset(ts, P_VOLUMEID, "-volumeid", CMD_SINGLE, CMD_OPTIONAL,
			"Volume id (-partition required)");
    cmd_AddParmAtOffset(ts, P_TYPE, "-type", CMD_LIST, CMD_OPTIONAL,
			"Volume types: rw, ro, bk");
    cmd_AddParmAtOffset(ts, P_FIND, "-find", CMD_LIST, CMD_OPTIONAL,
			"Objects to find: file, dir, mount, symlink, acl");
    cmd_AddParmAtOffset(ts, P_MASK, "-mask", CMD_LIST, (CMD_OPTIONAL | CMD_HIDE),
                        "Unix mode mask (example: 06000)");
    cmd_AddParmAtOffset(ts, P_OUTPUT, "-output", CMD_LIST, CMD_OPTIONAL,
			ColumnNames);
    cmd_AddParmAtOffset(ts, P_DELIM, "-delim", CMD_SINGLE, CMD_OPTIONAL,
			"Output field delimiter");
    cmd_AddParmAtOffset(ts, P_NOHEADING, "-noheading", CMD_FLAG, CMD_OPTIONAL,
			"Do not print the heading line");
    cmd_AddParmAtOffset(ts, P_NOMAGIC, "-ignore-magic", CMD_FLAG, CMD_OPTIONAL,
			"Skip directory vnode magic checks when looking up paths.");
}

/**
 * volinfo/volscan program entry
 */
int
main(int argc, char **argv)
{
    afs_int32 code;
    char *base;

    opr_queue_Init(&VnodeScanLists[vLarge]);
    opr_queue_Init(&VnodeScanLists[vSmall]);
    memset(ModeMask, 0, sizeof(ModeMask) / sizeof(*ModeMask));
    gethostname(Hostname, sizeof(Hostname));

    base = strrchr(argv[0], '/');
#ifdef AFS_NT40_ENV
    if (!base) {
	base = strrchr(argv[0], '\\');
    }
#endif /* AFS_NT40_ENV */
    if (!base) {
	base = argv[0];
    }
#ifdef AFS_NT40_ENV
    if ((base[0] == '/' || base[0] == '\\') && base[1] != '\0') {
#else /* AFS_NT40_ENV */
    if (base[0] == '/' && base[1] != '\0') {
#endif /* AFS_NT40_ENV */
	base++;
    }
    progname = base;

#ifdef AFS_NT40_ENV
    if (stricmp(progname, "volscan") == 0
	|| stricmp(progname, "volscan.exe") == 0) {
#else /* AFS_NT40_ENV */
    if (strcmp(progname, "volscan") == 0) {
#endif /* AFS_NT40_ENV */
	VolScanSyntax();
    } else {
	VolInfoSyntax();
    }

    code = cmd_Dispatch(argc, argv);
    return code;
}

/**
 * Return a display string for the volume type.
 *
 * @param[in]  type  volume type
 *
 * @return volume type description string
 */
static_inline char *
volumeTypeString(int type)
{
    return
	(type == RWVOL ? "read/write" :
	 (type == ROVOL ? "readonly" :
	  (type == BACKVOL ? "backup" : "unknown")));
}

/**
 * Return a short display string for the volume type.
 *
 * @param[in]  type  volume type
 *
 * @return volume type short description string
 */
static_inline char *
volumeTypeShortString(int type)
{
    return
	(type == RWVOL ? "RW" :
	 (type == ROVOL ? "RO" : (type == BACKVOL ? "BK" : "??")));
}

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
    printf("Volume header for volume %" AFS_VOLID_FMT " (%s)\n", afs_printable_VolumeId_lu(V_id(vp)), V_name(vp));
    printf("stamp.magic = %x, stamp.version = %u\n", V_stamp(vp).magic,
	   V_stamp(vp).version);
    printf
	("inUse = %d, inService = %d, blessed = %d, needsSalvaged = %d, dontSalvage = %d\n",
	 V_inUse(vp), V_inService(vp), V_blessed(vp), V_needsSalvaged(vp),
	 V_dontSalvage(vp));
    printf
	("type = %d (%s), uniquifier = %u, needsCallback = %d, destroyMe = %x\n",
	 V_type(vp), volumeTypeString(V_type(vp)), V_uniquifier(vp),
	 V_needsCallback(vp), V_destroyMe(vp));
    printf
	("id = %" AFS_VOLID_FMT ", parentId = %" AFS_VOLID_FMT ", cloneId = %" AFS_VOLID_FMT ", backupId = %" AFS_VOLID_FMT ", restoredFromId = %" AFS_VOLID_FMT "\n",
	 afs_printable_VolumeId_lu(V_id(vp)), afs_printable_VolumeId_lu(V_parentId(vp)), afs_printable_VolumeId_lu(V_cloneId(vp)), afs_printable_VolumeId_lu(V_backupId(vp)), afs_printable_VolumeId_lu(V_restoredFromId(vp)));
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
    struct afs_stat_st status;
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
 * @param[in] vdp     vnode details object
 *
 * @return none
 */
static void
SaveInode(struct VnodeDetails *vdp)
{
    IHandle_t *ih;
    FdHandle_t *fdP;
    char nfile[50], buffer[256];
    int ofd = 0;
    afs_foff_t total;
    ssize_t len;
    Inode ino = VNDISK_GET_INO(vdp->vnode);

    if (!VALID_INO(ino)) {
	return;
    }

    IH_INIT(ih, V_device(vdp->vp), V_parentId(vdp->vp), ino);
    fdP = IH_OPEN(ih);
    if (fdP == NULL) {
	fprintf(stderr,
		"%s: Can't open inode %s error %d (ignored)\n",
		progname, PrintInode(NULL, ino), errno);
	return;
    }
    snprintf(nfile, sizeof nfile, "TmpInode.%s", PrintInode(NULL, ino));
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
	nBytes = write(ofd, buffer, (size_t)len);
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
 * get the VnodeDiskObject for a directory given its vnode id.
 *
 * @param[in]   vp       volume object
 * @param[in]   parent   vnode id to read
 * @param[out]  pvn      vnode disk object to populate
 *
 * @post pvn contains copy of disk object for parent id
 *
 * @return operation status
 *   @retval 0   success
 *   @retval -1  failure
 */
int
GetDirVnode(Volume * vp, VnodeId parent, VnodeDiskObject * pvn)
{
    afs_int32 code;
    afs_foff_t offset;

    if (!DirIndexFd) {
	return -1;		/* previously failed to open the large vnode index. */
    }
    if (parent % 2 == 0) {
	fprintf(stderr, "%s: Invalid parent vnode id %lu in volume %lu\n",
		progname,
		afs_printable_uint32_lu(parent),
		afs_printable_uint32_lu(V_id(vp)));
    }
    offset = vnodeIndexOffset(&VnodeClassInfo[vLarge], parent);
    code = FDH_SEEK(DirIndexFd, offset, 0);
    if (code == -1) {
	fprintf(stderr,
		"%s: GetDirVnode: seek failed for %lu.%lu to offset %llu\n",
		progname, afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(parent), (long long unsigned)offset);
	return -1;
    }
    code = FDH_READ(DirIndexFd, pvn, SIZEOF_LARGEDISKVNODE);
    if (code != SIZEOF_LARGEDISKVNODE) {
	fprintf(stderr,
		"%s: GetDirVnode: read failed for %lu.%lu at offset %llu\n",
		progname, afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(parent), (long long unsigned)offset);
	return -1;
    }
    if (CheckMagic && (pvn->vnodeMagic != LARGEVNODEMAGIC)) {
	fprintf(stderr, "%s: GetDirVnode: bad vnode magic for %lu.%lu at offset %llu\n",
		progname, afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(parent), (long long unsigned)offset);
	return -1;
    }
    if (!pvn->dataVersion) {
	fprintf(stderr, "%s: GetDirVnode: dv is zero for %lu.%lu at offset %llu\n",
		progname, afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(parent), (long long unsigned)offset);
	return -1;
    }
    return 0;
}

/**
 * Perform inverse lookup on a vice directory object to map a fid onto a dirent string.
 *
 * @param[in]   vp          volume object
 * @param[in]   pvnode      parent directory vnode object
 * @param[in]   cvnid       child vnode id to inverse lookup
 * @param[in]   cuniq       child uniquifier to inverse lookup
 * @param[out]  dirent      buffer in which to store dirent string
 * @param[out]  dirent_len  length of dirent buffer
 *
 * @post dirent contains string for the (cvnid, cuniq) entry
 *
 * @return operation status
 *    @retval 0 success
 */
static int
GetDirEntry(Volume * vp, VnodeDiskObject * pvnode, VnodeId cvnid,
	    afs_uint32 cuniq, char *dirent, size_t dirent_len)
{
    DirHandle dir;
    Inode ino;
    int volumeChanged;
    afs_int32 code;

    ino = VNDISK_GET_INO(pvnode);
    if (!VALID_INO(ino)) {
	fprintf(stderr, "%s: GetDirEntry invalid parent ino\n", progname);
	return -1;
    }
    SetSalvageDirHandle(&dir, V_parentId(vp), V_device(vp), ino,
			&volumeChanged);
    code = afs_dir_InverseLookup(&dir, cvnid, cuniq, dirent, dirent_len);
    if (code) {
	fprintf(stderr, "%s: afs_dir_InverseLookup failed with code %d\n",
		progname, code);
    }
    FidZap(&dir);
    return code;
}

/**
 * Lookup the path of this vnode, relative to the root of the volume.
 *
 * @param[in] vdp    vnode details
 *
 * @return status
 *   @retval 0 success
 *   @retval -1 error
 */
int
LookupPath(struct VnodeDetails *vdp)
{
#define MAX_PATH_LEN 1023
    static char path_buffer[MAX_PATH_LEN + 1];
    static char dirent[MAX_PATH_LEN + 1];
    char vnode_buffer[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *pvn = (struct VnodeDiskObject *)vnode_buffer;
    int code = 0;
    int space;
    char *cursor;
    Volume *vp = vdp->vp;
    VnodeId parent = vdp->vnode->parent;
    VnodeId cvnid = vdp->vnodeNumber;
    afs_uint32 cuniq = vdp->vnode->uniquifier;

    if (!parent) {
	vdp->path = "/";	/* this is root */
	return 0;
    }

    space = sizeof(path_buffer) - 1;
    cursor = &path_buffer[space];
    *cursor = '\0';

    while (parent) {
	int len;

	code = GetDirVnode(vp, parent, pvn);
	if (code) {
	    cursor = NULL;
	    break;
	}
	code = GetDirEntry(vp, pvn, cvnid, cuniq, dirent, MAX_PATH_LEN);
	if (code) {
	    cursor = NULL;
	    break;
	}

	len = strlen(dirent);
	if (len == 0) {
	    fprintf(stderr,
		    "%s: Failed to lookup path for fid %lu.%lu.%lu: empty dir entry\n",
		    progname, afs_printable_uint32_lu(V_id(vdp->vp)),
		    afs_printable_uint32_lu(vdp->vnodeNumber),
		    afs_printable_uint32_lu(vdp->vnode->uniquifier));
	    cursor = NULL;
	    code = -1;
	    break;
	}

	if (space < (len + 1)) {
	    fprintf(stderr,
		    "%s: Failed to lookup path for fid %lu.%lu.%lu: path exceeds max length (%u).\n",
		    progname, afs_printable_uint32_lu(V_id(vdp->vp)),
		    afs_printable_uint32_lu(vdp->vnodeNumber),
		    afs_printable_uint32_lu(vdp->vnode->uniquifier),
		    MAX_PATH_LEN);
	    cursor = NULL;
	    code = -1;
	    break;
	}

	/* prepend path component */
	cursor -= len;
	memcpy(cursor, dirent, len);
	*--cursor = '/';
	space -= (len + 1);

	/* next parent */
	cvnid = parent;
	cuniq = pvn->uniquifier;
	parent = pvn->parent;
    }

    if (cursor) {
	vdp->path = cursor;
    }
    return code;
}

/**
 * Read the symlink target and determine if this vnode is a mount point.
 *
 * @param[inout]   vdp    vnode details object
 *
 * @return error code
 *   @retval 0 success
 *   @retval -1 failure
 */
static int
ReadSymlinkTarget(struct VnodeDetails *vdp)
{
#define MAX_SYMLINK_LEN 1023
    static char buffer[MAX_SYMLINK_LEN + 1];
    int code;
    Volume *vp = vdp->vp;
    VnodeDiskObject *vnode = vdp->vnode;
    VnodeId vnodeNumber = vdp->vnodeNumber;
    IHandle_t *ihP = NULL;
    FdHandle_t *fdP = NULL;
    afs_fsize_t fileLength;
    int readLength;
    Inode ino;

    ino = VNDISK_GET_INO(vnode);
    VNDISK_GET_LEN(fileLength, vnode);

    if (fileLength > MAX_SYMLINK_LEN) {
	fprintf(stderr,
		"%s: Symlink contents for fid (%lu.%lu.%lu.%lu) exceeds "
		"%u, file length is %llu)!\n", progname,
		afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(vnodeNumber),
		afs_printable_uint32_lu(vnode->uniquifier),
		afs_printable_uint32_lu(vnode->dataVersion),
                MAX_SYMLINK_LEN,
		fileLength);
	return -1;
    }
    if (fileLength == 0) {
	fprintf(stderr,
		"%s: Symlink contents for fid (%lu.%lu.%lu.%lu) is empty.\n",
		progname,
		afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(vnodeNumber),
		afs_printable_uint32_lu(vnode->uniquifier),
		afs_printable_uint32_lu(vnode->dataVersion));
	return -1;
    }

    IH_INIT(ihP, V_device(vp), V_parentId(vp), ino);
    fdP = IH_OPEN(ihP);
    if (fdP == NULL) {
	code = -1;
	goto cleanup;
    }
    if (FDH_SEEK(fdP, 0, SEEK_SET) < 0) {
	code = -1;
	goto cleanup;
    }
    readLength = FDH_READ(fdP, buffer, fileLength);
    if (readLength < 0) {
	fprintf(stderr,
		"%s: Error reading symlink contents for fid (%lu.%lu.%lu.%lu); "
		"errno %d\n",
		progname,
		afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(vnodeNumber),
		afs_printable_uint32_lu(vnode->uniquifier),
		afs_printable_uint32_lu(vnode->dataVersion), errno);
	code = -1;
	goto cleanup;
    } else if (readLength != fileLength) {
	fprintf(stderr,
		"%s: Symlink contents for fid (%lu.%lu.%lu.%lu) don't match "
		"vnode file length metadata (len=%llu, actual=%lld)!\n",
		progname,
		afs_printable_uint32_lu(V_id(vp)),
		afs_printable_uint32_lu(vnodeNumber),
		afs_printable_uint32_lu(vnode->uniquifier),
		afs_printable_uint32_lu(vnode->dataVersion), fileLength,
		(long long)readLength);
	code = -1;
	goto cleanup;
    }
    code = 0;

    if (readLength > 1 && (buffer[0] == '#' || buffer[0] == '%')
	&& buffer[readLength - 1] == '.') {
	char *sep;
	buffer[readLength - 1] = '\0';	/* stringify; clobbers trailing dot */
	sep = strchr(buffer, ':');
	vdp->t = VNODE_U_MOUNT;
	vdp->u.mnt.type = buffer[0];
	if (!sep) {
	    vdp->u.mnt.cell = NULL;
	    vdp->u.mnt.vol = buffer + 1;
	} else {
	    *sep = '\0';
	    vdp->u.mnt.cell = buffer + 1;
	    vdp->u.mnt.vol = sep + 1;
	}
    } else {
	buffer[readLength] = '\0';
	vdp->t = VNODE_U_SYMLINK;
	vdp->u.target = buffer;
    }

  cleanup:
    if (fdP) {
	FDH_CLOSE(fdP);
    }
    if (ihP) {
	IH_RELEASE(ihP);
    }
    return code;
}

/**
 * Print vnode details line
 *
 * @param[inout]  vdp   vnode details object
 *
 * @return none
 */
static void
PrintVnodeDetails(struct VnodeDetails *vdp)
{
    switch (vdp->vnode->type) {
    case vNull:
	break;
    case vFile:
	if (FindVnType & FIND_FILE) {
	    PrintColumns(vdp, "file");
	}
	break;
    case vDirectory:
	if (FindVnType & FIND_DIR) {
	    PrintColumns(vdp, "dir");
	}
	break;
    case vSymlink:
	if (FindVnType & (FIND_MOUNT | FIND_SYMLINK)) {
	    ReadSymlinkTarget(vdp);
	    if ((FindVnType & FIND_MOUNT) && (vdp->t == VNODE_U_MOUNT)) {
		PrintColumns(vdp, "mount");
	    }
	    if ((FindVnType & FIND_SYMLINK) && (vdp->t == VNODE_U_SYMLINK)) {
		PrintColumns(vdp, "symlink");
	    }
	}
	break;
    default:
	fprintf(stderr,
		"%s: Warning: unexpected vnode type %u on fid %lu.%lu.%lu",
		progname, vdp->vnode->type,
		afs_printable_uint32_lu(V_id(vdp->vp)),
		afs_printable_uint32_lu(vdp->vnodeNumber),
		afs_printable_uint32_lu(vdp->vnode->uniquifier));
    }
}

/**
 * Print each access entry of a vnode
 *
 * @param[in]  vdp   vnode details object
 *
 * @return none
 */
static void
ScanAcl(struct VnodeDetails *vdp)
{
    int i;
    struct acl_accessList *acl;
    VnodeDiskObject *vnode = vdp->vnode;

    if (vnode->type == vNull) {
	return;
    }

    acl = VVnodeDiskACL(vnode);
    for (i = 0; i < acl->positive; i++) {
	vdp->t = VNODE_U_POS_ACCESS;
	vdp->u.access = &(acl->entries[i]);
	PrintColumns(vdp, "acl");
    }
    for (i = (acl->total - 1); i >= (acl->total - acl->negative); i--) {
	vdp->t = VNODE_U_NEG_ACCESS;
	vdp->u.access = &(acl->entries[i]);
	PrintColumns(vdp, "acl");
    }
}

/**
 * Determine if the mode matches all the given masks.
 *
 * Returns true if the mode bits match all the given masks. A mask matches if at
 * least one bit in the mask is present in the mode bits.  An empty mode mask
 * list matches all modes (even if all the mode bits are zero.)
 *
 * param[in]  modeBits  unix mode bits of a vnode
 *
 */
static int
ModeMaskMatch(unsigned int modeBits)
{
    int i;

    for (i = 0; i < sizeof(ModeMask) / sizeof(*ModeMask) && ModeMask[i]; i++) {
	if ((ModeMask[i] & modeBits) == 0) {
	    return 0;		/* at least one mode bit is not present */
	}
    }
    return 1;
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
    IHandle_t *ih = vp->vnodeIndex[class].handle;
    FdHandle_t *fdP = NULL;
    afs_sfsize_t size;
    char *ctime, *atime, *mtime;
    struct opr_queue *scanList = &VnodeScanLists[class];
    struct opr_queue *cursor;

    if (opr_queue_IsEmpty(scanList)) {
	return;
    }

    for (opr_queue_Scan(scanList, cursor)) {
	struct VnodeScanProc *entry = (struct VnodeScanProc *)cursor;
	if (entry->heading) {
	    printf("%s", entry->heading);
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

	struct VnodeDetails vnodeDetails;

	if (!ModeMaskMatch(vnode->modeBits)) {
	    continue;
	}

	memset(&vnodeDetails, 0, sizeof(struct VnodeDetails));
	vnodeDetails.vp = vp;
	vnodeDetails.class = class;
	vnodeDetails.vnode = vnode;
	vnodeDetails.vnodeNumber = bitNumberToVnodeNumber(vnodeIndex, class);
	vnodeDetails.offset = offset;
	vnodeDetails.index = vnodeIndex;

	for (opr_queue_Scan(scanList, cursor)) {
	    struct VnodeScanProc *entry = (struct VnodeScanProc *)cursor;
	    if (entry->proc) {
		(*entry->proc) (&vnodeDetails);
	    }
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
 * @param[in] vdp          vnode details object
 *
 * @return none
 */
void
PrintVnode(struct VnodeDetails *vdp)
{
#if defined(AFS_NAMEI_ENV)
    IHandle_t *ihtmpp;
    namei_t filename;
#endif
    afs_foff_t offset = vdp->offset;
    VnodeDiskObject *vnode = vdp->vnode;
    afs_fsize_t fileLength;
    Inode ino;

    ino = VNDISK_GET_INO(vnode);
    VNDISK_GET_LEN(fileLength, vnode);

    /* The check for orphaned vnodes is currently limited to non-empty
     * vnodes with a parent of zero (and which are not the first entry
     * in the index). */
    if (ShowOrphaned && (fileLength == 0 || vnode->parent || !offset))
	return;

    printf
	("%10lld Vnode %u.%u.%u cloned: %u, length: %llu linkCount: %d parent: %u",
	 (long long)offset, vdp->vnodeNumber, vnode->uniquifier,
	 vnode->dataVersion, vnode->cloned, (afs_uintmax_t) fileLength,
	 vnode->linkCount, vnode->parent);
    if (DumpInodeNumber)
	printf(" inode: %s", PrintInode(NULL, ino));
    if (DumpDate)
	printf(" ServerModTime: %s", date(vnode->serverModifyTime));
#if defined(AFS_NAMEI_ENV)
    if (PrintFileNames) {
	IH_INIT(ihtmpp, V_device(vdp->vp), V_parentId(vdp->vp), ino);
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

/**
 * Print the volume partition id
 *
 * @param[in]  vp  volume object
 *
 * @return none
 */
static void
PrintPartitionId(Volume * vp)
{
    char *partition = VPartitionPath(V_partition(vp));

    if (!strncmp(partition, "/vicep", 6)) {
	printf("%s", partition + 6);
    } else if (!strncmp(partition, "vicep", 5)) {
	printf("%s", partition + 5);
    } else {
	fprintf(stderr, "Invalid partition for volume id %lu\n",
		afs_printable_uint32_lu(V_id(vp)));
	printf("%s", PLACEHOLDER);
    }
}

/**
 * Print the vnode type description string
 *
 * @param[in]  type  vnode type
 *
 * @return none
 */
static void
PrintVnodeType(int type)
{
    switch (type) {
    case vNull:
	printf("null");
	break;
    case vFile:
	printf("file");
	break;
    case vDirectory:
	printf("dir");
	break;
    case vSymlink:
	printf("symlink");
	break;
    default:
	printf("unknown");
    }
}

/**
 * Print right bits as string.
 *
 * param[in] rights  rights bitmap
 */
static void
PrintRights(int rights)
{
    if (rights & PRSFS_READ) {
	printf("r");
    }
    if (rights & PRSFS_LOOKUP) {
	printf("l");
    }
    if (rights & PRSFS_INSERT) {
	printf("i");
    }
    if (rights & PRSFS_DELETE) {
	printf("d");
    }
    if (rights & PRSFS_WRITE) {
	printf("w");
    }
    if (rights & PRSFS_LOCK) {
	printf("k");
    }
    if (rights & PRSFS_ADMINISTER) {
	printf("a");
    }
    if (rights & PRSFS_USR0) {
	printf("A");
    }
    if (rights & PRSFS_USR1) {
	printf("B");
    }
    if (rights & PRSFS_USR2) {
	printf("C");
    }
    if (rights & PRSFS_USR3) {
	printf("D");
    }
    if (rights & PRSFS_USR4) {
	printf("E");
    }
    if (rights & PRSFS_USR5) {
	printf("F");
    }
    if (rights & PRSFS_USR6) {
	printf("G");
    }
    if (rights & PRSFS_USR7) {
	printf("H");
    }
}

/**
 * Print the path to the namei file.
 */
static void
PrintNamei(Volume * vp, VnodeDiskObject * vnode)
{
#ifdef AFS_NAMEI_ENV
    namei_t name;
    IHandle_t *ihP = NULL;
    Inode ino;
    ino = VNDISK_GET_INO(vnode);
    IH_INIT(ihP, V_device(vp), V_parentId(vp), ino);
    namei_HandleToName(&name, ihP);
    printf("%s", name.n_path);
    IH_RELEASE(ihP);
#else
    printf("%s", PLACEHOLDER);
#endif
}

/**
 * Print the column heading line.
 */
static void
PrintColumnHeading(void)
{
    int i;
    const char *name;

    for (i = 0; i < NumOutputColumns; i++) {
	if (i > 0) {
	    printf("%s", ColumnDelim);
	}
	name = ColumnName[OutputColumn[i]].name;
	while (*name) {
	    putchar(toupper(*name++));
	}
    }
    printf("\n");
}

/**
 * Print output columns for the vnode/acess entry.
 *
 * @param[in]  vdp   vnode details object
 * @param[in]  desc  type of line to be printed
 *
 * @return none
 */
static void
PrintColumns(struct VnodeDetails *vdp, const char *desc)
{
    int i;
    afs_fsize_t length;

    for (i = 0; i < NumOutputColumns; i++) {
	if (i > 0) {
	    printf("%s", ColumnDelim);
	}
	switch (OutputColumn[i]) {
	case col_host:
	    printf("%s", Hostname);
	    break;
	case col_desc:
	    printf("%s", desc);
	    break;
	case col_vid:
	    printf("%lu", afs_printable_uint32_lu(V_id(vdp->vp)));
	    break;
	case col_offset:
	    printf("%llu", vdp->offset);
	    break;
	case col_vtype:
	    printf("%s", volumeTypeShortString(V_type(vdp->vp)));
	    break;
	case col_vname:
	    printf("%s", V_name(vdp->vp));
	    break;
	case col_part:
	    printf("%s", VPartitionPath(V_partition(vdp->vp)));
	    break;
	case col_partid:
	    PrintPartitionId(vdp->vp);
	    break;
	case col_fid:
	    printf("%lu.%lu.%lu",
		   afs_printable_uint32_lu(V_id(vdp->vp)),
		   afs_printable_uint32_lu(vdp->vnodeNumber),
		   afs_printable_uint32_lu(vdp->vnode->uniquifier));
	    break;
	case col_path:
	    if (!vdp->path) {
		LookupPath(vdp);
	    }
	    printf("%s", vdp->path ? vdp->path : PLACEHOLDER);
	    break;
	case col_target:
	    printf("%s",
		   (vdp->t == VNODE_U_SYMLINK ? vdp->u.target : PLACEHOLDER));
	    break;
	case col_mount:
	    if (vdp->t != VNODE_U_MOUNT) {
		printf("%s", PLACEHOLDER);
	    } else {
		printf("%c", vdp->u.mnt.type);
		if (vdp->u.mnt.cell) {
		    printf("%s:", vdp->u.mnt.cell);
		}
		printf("%s.", vdp->u.mnt.vol);
	    }
	    break;
	case col_mtype:
	    printf("%c", (vdp->t == VNODE_U_MOUNT ? vdp->u.mnt.type : '-'));
	    break;
	case col_mcell:
	    printf("%s",
		   (vdp->t == VNODE_U_MOUNT && vdp->u.mnt.cell ? vdp->u.mnt.cell : PLACEHOLDER));
	    break;
	case col_mvol:
	    printf("%s",
		   (vdp->t == VNODE_U_MOUNT ? vdp->u.mnt.vol : PLACEHOLDER));
	    break;
	case col_aid:
	    if (vdp->t == VNODE_U_POS_ACCESS || vdp->t == VNODE_U_NEG_ACCESS) {
		printf("%d", vdp->u.access->id);
	    } else {
		printf("%s", PLACEHOLDER);
	    }
	    break;
	case col_arights:
	    if (vdp->t == VNODE_U_POS_ACCESS) {
		printf("+");
		PrintRights(vdp->u.access->rights);
	    } else if (vdp->t == VNODE_U_NEG_ACCESS) {
		printf("-");
		PrintRights(vdp->u.access->rights);
	    }
	    break;
	case col_vntype:
	    PrintVnodeType(vdp->vnode->type);
	    break;
	case col_cloned:
	    printf("%c", vdp->vnode->cloned ? 'y' : 'n');
	    break;
	case col_mode:
	    printf("0%o", vdp->vnode->modeBits);
	    break;
	case col_links:
	    printf("%lu", afs_printable_uint32_lu(vdp->vnode->linkCount));
	    break;
	case col_length:
	    VNDISK_GET_LEN(length, vdp->vnode);
	    printf("%llu", length);
	    break;
	case col_uniq:
	    printf("%lu", afs_printable_uint32_lu(vdp->vnode->uniquifier));
	    break;
	case col_dv:
	    printf("%lu", afs_printable_uint32_lu(vdp->vnode->dataVersion));
	    break;
	case col_inode:
	    printf("%" AFS_UINT64_FMT, VNDISK_GET_INO(vdp->vnode));
	    break;
	case col_namei:
	    PrintNamei(vdp->vp, vdp->vnode);
	    break;
	case col_modtime:
	    printf("%lu",
		   afs_printable_uint32_lu(vdp->vnode->unixModifyTime));
	    break;
	case col_author:
	    printf("%lu", afs_printable_uint32_lu(vdp->vnode->author));
	    break;
	case col_owner:
	    printf("%lu", afs_printable_uint32_lu(vdp->vnode->owner));
	    break;
	case col_parent:
	    printf("%lu", afs_printable_uint32_lu(vdp->vnode->parent));
	    break;
	case col_magic:
	    printf("0x%08X", vdp->vnode->vnodeMagic);
	    break;
	case col_lock:
	    printf("%lu.%lu",
		   afs_printable_uint32_lu(vdp->vnode->lock.lockCount),
		   afs_printable_uint32_lu(vdp->vnode->lock.lockTime));
	    break;
	case col_smodtime:
	    printf("%lu",
		   afs_printable_uint32_lu(vdp->vnode->serverModifyTime));
	    break;
	case col_group:
	    printf("%lu", afs_printable_uint32_lu(vdp->vnode->group));
	    break;
	default:
	    fprintf(stderr, "%s: Unknown column type: %d (%d)\n", progname,
		    OutputColumn[i], i);
	    break;
	}
    }
    printf("\n");
}
