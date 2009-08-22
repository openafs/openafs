/*
 * $Id$
 *
 * dumptool - A tool to manage MR-AFS dump files
 *
 * The dump file format ought to be documented _somewhere_, and
 * this seems like a good as a place as any ...
 *
 * A AFS dump file is marked off into a series of sections.  Each
 * section marked by a dump tag.  A tag is a single byte who's value
 * corresponds with the next section.  The sections are (in order):
 *
 * DUMPHEADER (tag 0x01)
 * VOLUMEHEADER (tag 0x02)
 * VNODE (tag 0x03)
 * DUMPEND (tag 0x04)
 *
 * Descriptions of the sections follow.  Note that in all cases, data is
 * stored in the dump in network byte order.
 *
 * DUMPHEADER:
 *
 * DUMPHEADER contains two parts: the DUMPMAGIC magic number (32 bits)
 * and the dump header itself.
 *
 * The dump header itself consists of a series of tagged values,
 * each tag marking out members of the DumpHeader structure.  The
 * routine ReadDumpHeader explains the specifics of these tags.
 *
 * VOLUMEHEADER:
 *
 * VOLUMEHEADER is a series of tagged values corresponding to the elements
 * of the VolumeDiskData structure.  See ReadVolumeHeader for more
 * information
 *
 * VNODE:
 *
 * The VNODE section is all vnodes contained in the volume (each vnode
 * itself is marked with the VNODE tag, so it's really a sequence of
 * VNODE tags, unlike other sections).
 *
 * Each vnode consists of three parts: the vnode number (32 bits), the
 * uniqifier (32 bits), and a tagged list of elements corresponding to
 * the elements of the VnodeDiskData structure.  See ScanVnodes for
 * more information.  Note that if file data is associated with a vnode,
 * it will be contained here.
 *
 * DUMPEND:
 *
 * The DUMPEND section consists of one part: the DUMPENDMAGIC magic
 * number (32 bits).
 * 
 * Notes:
 *
 * The tagged elements are all ASCII letters, as opposed to the section
 * headers (which are 0x01, 0x02, ...).  Thus, an easy way to tell when
 * you've reached the end of an element sequence is to check to see if
 * the next tag is a printable character (this code tests for < 20).
 *
 * "vos dump" dumps the large vnode index, then the small vnode index,
 * so directories will appear first in the VNODE section.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <fnmatch.h>
#include <fcntl.h>

#include <lock.h>
#include <afs/param.h>
#include <afs/afsint.h>
#include <afs/nfs.h>
#include <afs/acl.h>
#if !defined(PRE_AFS_36) && !defined(RESIDENCY)
#include <afs/ihandle.h>
#endif /* !defined(PRE_AFS_36) && !defined(RESIDENCY) */
#include <afs/vnode.h>
#include <afs/volume.h>

#ifdef AFS_LINUX24_ENV
#define _LARGEFILE64_SOURCE 1
#endif
#ifdef RESIDENCY
#include <afs/rsdefs.h>
#include <afs/remioint.h>
#endif /* RESIDENCY */

#include <afs/dir.h>

/*
 * Sigh.  Linux blows it again
 */

#ifdef linux
#include <pty.h>
#endif

/*
 * Stuff that is in private AFS header files, unfortunately
 */

#define DUMPVERSION	1
#define DUMPENDMAGIC	0x3A214B6E
#define DUMPBEGINMAGIC	0xB3A11322
#define D_DUMPHEADER	1
#define D_VOLUMEHEADER	2
#define D_VNODE		3
#define D_DUMPEND	4
#define D_MAX		20

#define MAXDUMPTIMES	50

struct DumpHeader {
    int32_t version;
    VolumeId volumeId;
    char volumeName[VNAMESIZE];
    int nDumpTimes;		/* Number of pairs */
    struct {
	int32_t from, to;
    } dumpTimes[MAXDUMPTIMES];
};

/*
 * Our command-line arguments
 */

#ifdef RESIDENCY
struct {
    int Algorithm;		/* Conversion algorithm */
    int Size;			/* Directory hierarchy size */
    int FSType;			/* File system type */
    int DeviceTag;		/* Device Tag */
} rscmdlineinfo[RS_MAXRESIDENCIES];

/*
 * This stuff comes from ufsname.c (which itself takes it from
 * ufs_interfaces.c)
 */

/* There is an assumption that all of the prefixes will have exactly one '/' */
static char *Ufs_Prefixes[] = { "/ufs", "/slowufs", "/cdmf", "/sdmf" };

#define MAX_ITERATIONS 10
#define UFS_SUMMARYTREENAME "Summaries"
#define UFS_STAGINGTREENAME "Staging"
#define UFS_VOLUMEHEADERTREENAME "VolHeaders"
#define UFS_VOLUMETREENAME "Volumes"
#define UFS_ALGORITHMBASE 'A'
#define UFS_MOUNTPOINTBASE 'a'
#define UFS_ALGORITHMS 3
#define UFS_LINK_MAX 64		/* Arbitrary. */
#define HARD_LINKED_FILE -2
#define TAGSTONAME(FileName, MountPoint, Sections, Level1, RWVolume, Vnode, Uniquifier, Algorithm) \
{ \
    if (Level1) \
        sprintf(FileName,"%s/%lu/%lu/%c%lu.%lu.%lu.%lu.%lu", MountPoint, \
                (Sections)[0], (Sections)[1], UFS_ALGORITHMBASE + Algorithm, \
                (Sections)[2], (Sections)[3], RWVolume, Vnode, Uniquifier); \
    else \
        sprintf(FileName,"%s/%lu/%c%lu.%lu.%lu.%lu.%lu", MountPoint, \
                (Sections)[0], UFS_ALGORITHMBASE + Algorithm, \
                (Sections)[1], (Sections)[2], RWVolume, Vnode, Uniquifier); \
}
#define TAGSTOSTAGINGNAME(FileName, MountPoint, RWVolume, Vnode, Uniquifier) \
    sprintf(FileName,"%s/%s/%lu.%lu.%lu", MountPoint, \
	    UFS_STAGINGTREENAME, RWVolume, Vnode, Uniquifier)
#define TAGSTOVOLUMEHEADERNAME(FileName, MountPoint, FileTag1, FileTag2) \
    sprintf(FileName,"%s/%s/%lu", MountPoint, UFS_VOLUMEHEADERTREENAME, \
	    FileTag1)
#define TAGSTOVOLUMEINFONAME(FileName, MountPoint, FileTag1, FileTag2) \
    sprintf(FileName,"%s/%s/%lu/%lu", MountPoint, \
	    UFS_VOLUMETREENAME, FileTag2, FileTag1)
#define TAGSTOVOLUMENAME(FileName, MountPoint, FileTag1, FileTag2, RWVolume) \
    sprintf(FileName,"%s/%s/%lu/%lu.%lu", MountPoint, \
	    UFS_VOLUMETREENAME, FileTag2, FileTag1, RWVolume)
#define TAGSTOSUMMARYNAME(FileName, MountPoint, FileTag1, FileTag2, SummaryRequestor, Residency) \
    sprintf(FileName,"%s/%s/%lu.%lu.%lu.%lu", MountPoint, \
	    UFS_SUMMARYTREENAME, FileTag1, FileTag2, SummaryRequestor, \
	    Residency)
#define DEVICETAGNUMBERTOMOUNTPOINT(MountPoint, DeviceTagNumber, FSType) \
    sprintf(MountPoint,"%s%c", Ufs_Prefixes[FSType], UFS_MOUNTPOINTBASE + \
	    DeviceTagNumber)
#define MOUNTPOINTTODEVICETAGNUMBER(MountPoint) \
    MountPoint[strlen(MountPoint) - 1] - UFS_MOUNTPOINTBASE
#define DEVICETAGNUMBERTOVOLUMEHEADERTREE(TreeName, MountPoint) \
    sprintf(TreeName,"%s/%s", MountPoint, UFS_VOLUMEHEADERTREENAME)
#define UFS_RESIDENCIES_FILE "Residencies"

/* We don't ever want to map to uid/gid -1.  fchown() takes that as a
   don't change flag.  We know however that volume number range from
   0x2000000 to 0x20FFFFFF (see VAllocateVolumeId() in vol/vutil.c)
   so we will use that to insure that -1 never appears. */
#define RWVolumeToUid(RWVolume) ((RWVolume >> 12) & 0xFFFF)
#define RWVolumeToGid(RWVolume) ((RWVolume & 0xFFF) | \
				 (((RWVolume >> 28) & 0xF) << 12))
#define UidGidToRWVolume(Uid, Gid) ((Gid & 0xFFF) | ((Uid & 0xFFFF) << 12) | \
				    ((Gid & 0xF000) << 16))


/* These routines generate a file name to correspond to the given tag
   numbers. */

/* The following entropy array contains the order of bits from highest entropy
   to lowest in the numbers FileTag1 and FileTag2.  Bit numbers 32 and above
   correspond to FileTag2.  This ordering was determined by examining all read-
   write volumes in the psc.edu cell. */
char UfsEntropy[1][64] = {
    {1, 2, 3, 4, 33, 5, 6, 7, 44, 45, 46, 36, 8, 34, 42, 35,
     9, 40, 38, 32, 43, 10, 39, 37, 11, 41, 12, 13, 14, 0,
     15, 16, 61, 17, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51,
     50, 49, 48, 47, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22,
     21, 20, 19, 18, 62, 63},
};

uint32_t Directories[3][2] = { {256, 0}, {256, 16}, {256, 256}, };
#endif /* RESIDENCY */

static int verbose = 0;
static int numNoDirData = 0;
static int termsize = 0;
int Testing = 0;
#ifdef RESIDENCY
extern resid ServerRequestorId;
#endif /* RESIDENCY */

/*
 * We use this structure to hold vnode data in our hash table.
 * It's indexed by vnode number.
 */

struct vnodeData {
    struct VnodeDiskObject *vnode;	/* A pointer to the disk vnode */
    int vnodeNumber;		/* The vnode number */
    off64_t dumpdata;		/* File offset of dump data (if
				 * available */
    unsigned char *filedata;	/* A pointer to the actual file
				 * data itself (if available) */
    unsigned int datalength;	/* The length of the data */
};

/*
 * This contains the current location when we're doing a scan of a
 * directory.
 */

struct DirCursor {
    int hashbucket;		/* Current hash bucket */
    int entry;			/* Entry within hash bucket */
};

/*
 * Arrays to hold vnode data
 */

struct vnodeData **LargeVnodeIndex;
struct vnodeData **SmallVnodeIndex;
int numLargeVnodes = 0;
int numSmallVnodes = 0;

/*
 * Crap for the libraries
 */

int ShutdownInProgress = 0;

/*
 * Our local function prototypes
 */

static int ReadDumpHeader(FILE *, struct DumpHeader *);
static int ReadVolumeHeader(FILE *, VolumeDiskData *);
static int ScanVnodes(FILE *, VolumeDiskData *, int);
static int DumpVnodeFile(FILE *, struct VnodeDiskObject *, VolumeDiskData *);
static struct vnodeData *InsertVnode(unsigned int, struct VnodeDiskObject *);
static struct vnodeData *GetVnode(unsigned int);
static int CompareVnode(const void *, const void *);
static void InteractiveRestore(FILE *, VolumeDiskData *);
static void DirectoryList(int, char **, struct vnodeData *, VolumeDiskData *);
static void DirListInternal(struct vnodeData *, char *[], int, int, int, int,
			    int, int, VolumeDiskData *, char *);
static int CompareDirEntry(const void *, const void *);
static struct vnodeData *ChangeDirectory(int, char **, struct vnodeData *);
static void CopyFile(int, char **, struct vnodeData *, FILE *);
static void CopyVnode(int, char **, FILE *);
static void DumpAllFiles(int, char **, struct vnodeData *, VolumeDiskData *);
static void DumpAllResidencies(FILE *, struct vnodeData *, VolumeDiskData *);
static struct vnodeData *FindFile(struct vnodeData *, char *);
static void ResetDirCursor(struct DirCursor *, struct vnodeData *);
static struct DirEntry *ReadNextDir(struct DirCursor *, struct vnodeData *);
static void MakeArgv(char *, int *, char ***);
static char *GetToken(char *, char **, char *, char *[]);
static int ReadInt16(FILE *, uint16_t *);
static int ReadInt32(FILE *, uint32_t *);
static int ReadString(FILE *, char *, int);
static int ReadByteString(FILE *, void *, int);

int
main(int argc, char *argv[])
{
    int c, errflg = 0, dumpvnodes = 0, force = 0, inode = 0;
    unsigned int magic;
    struct DumpHeader dheader;
    VolumeDiskData vol;
    off64_t offset;
    int Res, Arg1, Arg2, Arg3, i;
    char *p;
    struct winsize win;
    FILE *f;
    int fd;
    time_t tmv;

#ifdef RESIDENCY
    for (i = 0; i < RS_MAXRESIDENCIES; i++) {
	rscmdlineinfo[i].Algorithm = -1;
	rscmdlineinfo[i].Size = -1;
	rscmdlineinfo[i].DeviceTag = -1;
	rscmdlineinfo[i].FSType = -1;
    }
#endif /* RESIDENCY */

    /*
     * Sigh, this is dumb, but we need the terminal window size
     * to do intelligent things with "ls" later on.
     */

    if (isatty(STDOUT_FILENO)) {
	if ((p = getenv("COLUMNS")) != NULL)
	    termsize = atoi(p);
	else if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0
		 && win.ws_col > 0)
	    termsize = win.ws_col;
    }

    while ((c = getopt(argc, argv, "difr:t:v")) != EOF)
	switch (c) {
	case 't':
#ifdef RESIDENCY
	    if (sscanf(optarg, "%d/%d", &Res, &Arg1) != 2) {
		errflg++;
		break;
	    }

	    if (1 << (ffs(Res) - 1) != Res) {
		fprintf(stderr, "Invalid residency %d\n", Res);
		errflg++;
		break;
	    }

	    if (Arg1 < 0 || Arg1 > 26) {
		fprintf(stderr, "Invalid device tag: %d\n", Arg1);
		errflg++;
		break;
	    }
	    rscmdlineinfo[ffs(Res) - 1].DeviceTag = Arg1;
#else /* RESIDENCY */
	    fprintf(stderr, "-t not supported in non-MRAFS " "dumptool.\n");
	    errflg++;
#endif /* RESIDENCY */
	    break;

	case 'r':
#ifdef RESIDENCY
	    if (sscanf(optarg, "%d/%d/%d/%d", &Res, &Arg1, &Arg2, &Arg3) != 4) {
		errflg++;
		break;
	    }

	    if (Arg1 < 0 || Arg1 > 3) {
		fprintf(stderr, "Invalid fstype: %d\n", Arg1);
		errflg++;
		break;
	    }

	    if (Arg2 < 0 || Arg2 > 2) {
		fprintf(stderr, "Invalid size: %d\n", Arg2);
		errflg++;
		break;
	    }

	    if (Arg3 <= 0 || Arg3 > UFS_ALGORITHMS) {
		fprintf(stderr, "Invalid algorithm: %d\n", Arg3);
		errflg++;
		break;
	    }
	    rscmdlineinfo[ffs(Res) - 1].FSType = Arg1;
	    rscmdlineinfo[ffs(Res) - 1].Size = Arg2;
	    rscmdlineinfo[ffs(Res) - 1].Algorithm = Arg3;
#else /* RESIDENCY */
	    fprintf(stderr, "-r not supported in non-MRAFS " "dumptool.\n");
	    errflg++;
#endif /* RESIDENCY */
	    break;
	case 'd':
#ifdef RESIDENCY
	    dumpvnodes++;
#else /* RESIDENCY */
	    fprintf(stderr, "-d not supported in non-MRAFS " "dumptool.\n");
	    errflg++;
#endif /* RESIDENCY */
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'f':
	    force++;
	    break;
	case 'i':
	    inode++;
	    break;
	case '?':
	default:
	    errflg++;
	}

    if (errflg || optind == argc) {
	fprintf(stderr, "Usage: %s\n\t[-v] [-f]\n\t"
#ifdef RESIDENCY
		"[-t Residency/Tag]\n\t"
		"[-r Residency/Type/Size/Algorithm]\n\t"
		"[-d] filename [file_in_dump [file in dump ...]]\n",
#else /* RESIDENCY */
		"filename\n",
#endif /* RESIDENCY */
		argv[0]);
	exit(1);
    }

    /*
     * Try opening the dump file
     */

#ifdef O_LARGEFILE
    if ((fd = open(argv[optind], O_RDONLY | O_LARGEFILE)) < 0) {
#else
    if ((fd = open(argv[optind], O_RDONLY)) < 0) {
#endif
	fprintf(stderr, "open of dumpfile %s failed: %s\n", argv[optind],
		strerror(errno));
	exit(1);
    }

    if ((f = fdopen(fd, "rb")) == NULL) {
        fprintf(stderr, "fdopen of dumpfile %s failed: %s\n", argv[optind],
		strerror(errno));
	exit(1);
    }

    if (ReadDumpHeader(f, &dheader)) {
	fprintf(stderr, "Failed to read dump header!\n");
	exit(1);
    }

    if (verbose)
	printf("Dump is for volume %lu (%s)\n", dheader.volumeId,
	       dheader.volumeName);

    if (getc(f) != D_VOLUMEHEADER) {
	fprintf(stderr, "Volume header is missing from dump, aborting\n");
	exit(1);
    }

    if (ReadVolumeHeader(f, &vol)) {
	fprintf(stderr, "Unable to read volume header\n");
	exit(1);
    }

    if (verbose) {
	printf("Volume information:\n");
	printf("\tid = %lu\n", vol.id);
	printf("\tparent id = %lu\n", vol.parentId);
	printf("\tname = %s\n", vol.name);
	printf("\tflags =");
	if (vol.inUse)
	    printf(" inUse");
	if (vol.inService)
	    printf(" inService");
	if (vol.blessed)
	    printf(" blessed");
	if (vol.needsSalvaged)
	    printf(" needsSalvaged");
	printf("\n");
	printf("\tuniquifier = %lu\n", vol.uniquifier);
	tmv = vol.creationDate;
	printf("\tCreation date = %s", ctime(&tmv));
	tmv = vol.accessDate;
	printf("\tLast access date = %s", ctime(&tmv));
	tmv = vol.updateDate;
	printf("\tLast update date = %s", ctime(&tmv));
	printf("\tVolume owner = %lu\n", vol.owner);
    }

    if (verbose)
	printf("Scanning vnodes (this may take a while)\n");

    /*
     * We need to do two vnode scans; one to get the number of
     * vnodes, the other to actually build the index.
     */

    offset = ftello64(f);

    if (ScanVnodes(f, &vol, 1)) {
	fprintf(stderr, "First vnode scan failed, aborting\n");
	exit(1);
    }

    fseeko64(f, offset, SEEK_SET);

    if (ScanVnodes(f, &vol, 0)) {
	fprintf(stderr, "Second vnode scan failed, aborting\n");
	exit(1);
    }

    if (getc(f) != D_DUMPEND || ReadInt32(f, &magic) || magic != DUMPENDMAGIC) {
	fprintf(stderr, "Couldn't find dump postamble, ");
	if (!force) {
	    fprintf(stderr, "aborting (use -f to override)\n");
	    exit(1);
	} else {
	    fprintf(stderr, "continuing anyway\n");
	    fprintf(stderr, "WARNING: Dump may not be complete!\n");
	}
    }

    /*
     * If we wanted to simply dump all vnodes, do it now
     */

#ifdef RESIDENCY
    if (dumpvnodes) {
	struct vnodeData *vdata;

	for (i = 0; i < numLargeVnodes; i++) {

	    vdata = LargeVnodeIndex[i];

	    if (vdata->vnode->type == vFidLookup)
		if (DumpVnodeFile(stdout, vdata->vnode, &vol)) {
		    fprintf(stderr, "DumpVnodeFile failed, " "aborting\n");
		    exit(1);
		}
	}

	for (i = 0; i < numSmallVnodes; i++) {

	    vdata = SmallVnodeIndex[i];

	    if (vdata->vnode->type == vFidLookup)
		if (DumpVnodeFile(stdout, vdata->vnode, &vol)) {
		    fprintf(stderr, "DumpVnodeFile failed, " "aborting\n");
		    exit(1);
		}
	}

    } else
#endif /* RESIDENCY */
    if (inode) {
	/*
	 * Dump out all filenames with their corresponding FID
	 */

	struct vnodeData *rootvdata;

	if ((rootvdata = GetVnode(1)) == NULL) {
	    fprintf(stderr,
		    "Can't get vnode data for root " "vnode!  Aborting\n");
	    exit(1);
	}

	DirListInternal(rootvdata, NULL, 0, 0, 1, 0, 1, 0, &vol, "");

    } else if (argc > optind + 1) {
#ifdef RESIDENCY
	/*
	 * Dump out residencies of files given on the command line.
	 */

	struct vnodeData *vdata, *rootvdata;

	if ((rootvdata = GetVnode(1)) == NULL) {
	    fprintf(stderr,
		    "Can't get vnode data for root " "vnode!  Aborting\n");
	    exit(1);
	}

	for (i = optind + 1; i < argc; i++) {

	    if ((vdata = FindFile(rootvdata, argv[i])) == NULL) {
		fprintf(stderr, "Skipping file %s\n", argv[i]);
		continue;
	    }

	    if (verbose)
		printf("Residency locations for %s:\n", argv[i]);

	    while (vdata->vnode->NextVnodeId != 0) {

		vdata = GetVnode(vdata->vnode->NextVnodeId);

		if (vdata == NULL) {
		    fprintf(stderr,
			    "We had a vnode chain " "pointer to a vnode that "
			    "doesn't exist, aborting!\n");
		    exit(1);
		}
		if (vdata->vnode->type == vFidLookup)
		    DumpVnodeFile(stdout, vdata->vnode, &vol);
	    }
	}
#else /* RESIDENCY */
	fprintf(stderr, "Extra arguments after dump filename: %s\n",
		argv[optind]);
	exit(1);
#endif /* RESIDENCY */
    } else {
	/*
	 * Perform an interactive restore
	 */

	InteractiveRestore(f, &vol);
    }

    exit(0);
}

/*
 * Read the dump header, which is at the beginning of every dump
 */

static int
ReadDumpHeader(FILE * f, struct DumpHeader *header)
{
    unsigned int magic;
    int tag, i;

    if (getc(f) != D_DUMPHEADER || ReadInt32(f, &magic)
	|| ReadInt32(f, (unsigned int *)
		     &header->version) || magic != DUMPBEGINMAGIC) {
	if (verbose)
	    fprintf(stderr, "Couldn't find dump magic numbers\n");
	return -1;
    }

    header->volumeId = 0;
    header->nDumpTimes = 0;

    while ((tag = getc(f)) > D_MAX && tag != EOF) {
	unsigned short length;
	switch (tag) {
	case 'v':
	    if (ReadInt32(f, &header->volumeId)) {
		if (verbose)
		    fprintf(stderr, "Failed to read " "volumeId\n");
		return -1;
	    }
	    break;
	case 'n':
	    if (ReadString(f, header->volumeName, sizeof(header->volumeName))) {
		if (verbose)
		    fprintf(stderr, "Failed to read " "volume name\n");
		return -1;
	    }
	    break;
	case 't':
	    if (ReadInt16(f, &length)) {
		if (verbose)
		    fprintf(stderr,
			    "Failed to read " "dump time array length\n");
		return -1;
	    }
	    header->nDumpTimes = (length >> 1);
	    for (i = 0; i < header->nDumpTimes; i++)
		if (ReadInt32(f, (unsigned int *)
			      &header->dumpTimes[i].from)
		    || ReadInt32(f, (unsigned int *)
				 &header->dumpTimes[i].to)) {
		    if (verbose)
			fprintf(stderr, "Failed to " "read dump times\n");
		    return -1;
		}
	    break;
	default:
	    if (verbose)
		fprintf(stderr, "Unknown dump tag \"%c\"\n", tag);
	    return -1;
	}
    }

    if (!header->volumeId || !header->nDumpTimes) {
	if (verbose)
	    fprintf(stderr,
		    "We didn't get a volume Id or " "dump times listing\n");
	return 1;
    }

    ungetc(tag, f);
    return 0;
}

/*
 * Read the volume header; this is the information stored in VolumeDiskData.
 *
 * I'm not sure we need all of this, but read it in just in case.
 */

static int
ReadVolumeHeader(FILE * f, VolumeDiskData * vol)
{
    int tag;
    unsigned int trash;
    memset((void *)vol, 0, sizeof(*vol));

    while ((tag = getc(f)) > D_MAX && tag != EOF) {
	switch (tag) {
	case 'i':
	    if (ReadInt32(f, &vol->id))
		return -1;
	    break;
	case 'v':
	    if (ReadInt32(f, &trash))
		return -1;
	    break;
	case 'n':
	    if (ReadString(f, vol->name, sizeof(vol->name)))
		return -1;
	    break;
	case 's':
	    vol->inService = getc(f);
	    break;
	case 'b':
	    vol->blessed = getc(f);
	    break;
	case 'u':
	    if (ReadInt32(f, &vol->uniquifier))
		return -1;
	    break;
	case 't':
	    vol->type = getc(f);
	    break;
	case 'p':
	    if (ReadInt32(f, &vol->parentId))
		return -1;
	    break;
	case 'c':
	    if (ReadInt32(f, &vol->cloneId))
		return -1;
	    break;
	case 'q':
	    if (ReadInt32(f, (uint32_t *) & vol->maxquota))
		return -1;
	    break;
	case 'm':
	    if (ReadInt32(f, (uint32_t *) & vol->minquota))
		return -1;
	    break;
	case 'd':
	    if (ReadInt32(f, (uint32_t *) & vol->diskused))
		return -1;
	    break;
	case 'f':
	    if (ReadInt32(f, (uint32_t *) & vol->filecount))
		return -1;
	    break;
	case 'a':
	    if (ReadInt32(f, &vol->accountNumber))
		return -1;
	    break;
	case 'o':
	    if (ReadInt32(f, &vol->owner))
		return -1;
	    break;
	case 'C':
	    if (ReadInt32(f, &vol->creationDate))
		return -1;
	    break;
	case 'A':
	    if (ReadInt32(f, &vol->accessDate))
		return -1;
	    break;
	case 'U':
	    if (ReadInt32(f, &vol->updateDate))
		return -1;
	    break;
	case 'E':
	    if (ReadInt32(f, &vol->expirationDate))
		return -1;
	    break;
	case 'B':
	    if (ReadInt32(f, &vol->backupDate))
		return -1;
	    break;
	case 'O':
	    if (ReadString
		(f, vol->offlineMessage, sizeof(vol->offlineMessage)))
		return -1;
	    break;
	case 'M':
	    if (ReadString(f, (char *)vol->stat_reads, VMSGSIZE))
		return -1;
	    break;
	case 'W':{
		unsigned short length;
		int i;
		unsigned int data;
		if (ReadInt16(f, &length))
		    return -1;
		for (i = 0; i < length; i++) {
		    if (ReadInt32(f, &data))
			return -1;
		    if (i < sizeof(vol->weekUse) / sizeof(vol->weekUse[0]))
			vol->weekUse[i] = data;
		}
		break;
	    }
	case 'D':
	    if (ReadInt32(f, &vol->dayUseDate))
		return -1;
	    break;
	case 'Z':
	    if (ReadInt32(f, (uint32_t *) & vol->dayUse))
		return -1;
	    break;
#ifdef RESIDENCY
	case 'R':{
		unsigned short length;
		int i;
		unsigned int data;

		if (ReadInt16(f, &length))
		    return -1;
		for (i = 0; i < length; i++) {
		    if (ReadInt32(f, &data))
			return -1;
		    if (i <
			sizeof(vol->DesiredInfo.DesiredResidencyWords) /
			sizeof(vol->DesiredInfo.DesiredResidencyWords[0]))
			vol->DesiredInfo.DesiredResidencyWords[i] = data;
		}
		break;
	    }
	case 'S':{
		unsigned short length;
		int i;
		unsigned int data;

		if (ReadInt16(f, &length))
		    return -1;
		for (i = 0; i < length; i++) {
		    if (ReadInt32(f, &data))
			return -1;
		    if (i <
			sizeof(vol->UnDesiredInfo.UnDesiredResidencyWords) /
			sizeof(vol->UnDesiredInfo.UnDesiredResidencyWords[0]))
			vol->UnDesiredInfo.UnDesiredResidencyWords[i] = data;
		}
		break;
	    }
#endif
	default:
	    if (verbose)
		fprintf(stderr, "Unknown dump tag \"%c\"\n", tag);
	    return -1;
	}
    }

    ungetc(tag, f);
    return 0;
}

/*
 * Scan all our vnode entries, and build indexing information.
 */

static int
ScanVnodes(FILE * f, VolumeDiskData * vol, int sizescan)
{
    int vnodeNumber;
    int tag;
    int numFileVnodes = 0;
    int numDirVnodes = 0;
    unsigned char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    off64_t offset, oldoffset;
    struct vnodeData *vdata;
    unsigned int length;

    tag = getc(f);

    while (tag == D_VNODE) {

	offset = 0;
	length = 0;
	vnode->type = -1;
	vnode->length = -1;

	if (ReadInt32(f, (uint32_t *) & vnodeNumber)) {
	    fprintf(stderr, "failed int32 for 'vnodenum'\n");
	    return -1;
	}

	if (ReadInt32(f, &vnode->uniquifier)) {
	    fprintf(stderr, "failed int32 for 'uniquifier'\n");
	    return -1;
	}

	if (verbose > 1 && !sizescan)
	    printf("Got vnode %d\n", vnodeNumber);

	while ((tag = getc(f)) > D_MAX && tag != EOF)
	    switch (tag) {
	    case 't':
		vnode->type = (VnodeType) getc(f);
		break;
	    case 'l':
		{
		    unsigned short tmp;
		    if (ReadInt16(f, &tmp)) {
			fprintf(stderr, "failed int16 for 'l'\n");
			return -1;
		    }
		    vnode->linkCount = tmp;
		}
		break;
	    case 'v':
		if (ReadInt32(f, &vnode->dataVersion)) {
		    fprintf(stderr, "failed int32 for 'v'\n");
		    return -1;
		}
		break;
	    case 'm':
		if (ReadInt32(f, (uint32_t *) & vnode->unixModifyTime)) {
		    fprintf(stderr, "failed int32 for 'm'\n");
		    return -1;
		}
		break;
	    case 's':
		if (ReadInt32(f, (uint32_t *) & vnode->serverModifyTime)) {
		    fprintf(stderr, "failed int32 for 's'\n");
		    return -1;
		}
		break;
	    case 'a':
		if (ReadInt32(f, &vnode->author)) {
		    fprintf(stderr, "failed int32 for 'a'\n");
		    return -1;
		}
		break;
	    case 'o':
		if (ReadInt32(f, &vnode->owner)) {
		    fprintf(stderr, "failed int32 for 'o'\n");
		    return -1;
		}
		break;
	    case 'g':
		if (ReadInt32(f, (uint32_t *) & vnode->group)) {
		    fprintf(stderr, "failed int32 for 'g'\n");
		    return -1;
		}
		break;
	    case 'b':{
		    unsigned short modeBits;
		    if (ReadInt16(f, &modeBits))
			return -1;
		    vnode->modeBits = modeBits;
		    break;
		}
	    case 'p':
		if (ReadInt32(f, &vnode->parent)) {
		    fprintf(stderr, "failed int32 for 'p'\n");
		    return -1;
		}
		break;
#ifdef RESIDENCY
	    case 'N':
		if (ReadInt32(f, &vnode->NextVnodeId)) {
		    fprintf(stderr, "failed int32 for 'N'\n");
		    return -1;
		}
		break;
	    case 'R':
		if (ReadInt32(f, &VLkp_Residencies(vnode))) {
		    fprintf(stderr, "failed int32 for 'R'\n");
		    return -1;
		}
		break;
#endif
	    case 'S':
		if (ReadInt32(f, &vnode->length)) {
		    fprintf(stderr, "failed int32 for 'S'\n");
		    return -1;
		}
		break;
	    case 'F':
		if (ReadInt32(f, (uint32_t *) & vnode->vn_ino_lo))
		    return -1;
		break;
	    case 'A':
		if (ReadByteString
		    (f, (void *)VVnodeDiskACL(vnode), VAclDiskSize(vnode))) {
		    fprintf(stderr, "failed readbystring for 'A'\n");
		    return -1;
		}
#if 0
		acl_NtohACL(VVnodeDiskACL(vnode));
#endif
		break;
#ifdef RESIDENCY
	    case 'h':
		if (ReadInt32(f, &vnode->length_hi)) {
		    fprintf(stderr, "failed int32 for 'h'\n");
		    return -1;
		}
#endif
	    case 'f':
		if (verbose > 1 && !sizescan)
		    printf("We have file data!\n");
		if (ReadInt32(f, &length)) {
		    fprintf(stderr, "failed int32 for 'f'\n");
		    return -1;
		}
		vnode->length = length;
		offset = ftello64(f);
		fseeko64(f, length, SEEK_CUR);
		break;
	    default:
		if (verbose)
		    fprintf(stderr, "Unknown dump tag \"%c\"\n", tag);
		return -1;
	    }

	/*
	 * If we're doing an incremental restore, then vnodes
	 * will be listed in the dump, but won't contain any
	 * vnode information at all (I don't know why they're
	 * included _at all_).  If we get one of these vnodes, then
	 * just skip it (because we can't do anything with it.
	 */

	if (vnode->type == -1)
	    continue;

#ifdef RESIDENCY
	if (verbose > 1 && vnode->type == vFidLookup && !sizescan) {
	    printf
		("This is an auxiliary vnode (lookup) for vnode %d, residency %d\n",
		 VLkp_ParentVnodeId(vnode), VLkp_Residencies(vnode));
	    if (DumpVnodeFile(stdout, vnode, vol))
		return -1;
	}

	if (verbose > 1 && vnode->type == vAccessHistory && !sizescan)
	    printf("This is an auxiliary vnode (history) for vnode %d\n",
		   VLkp_ParentVnodeId(vnode));
#endif

	if (vnode->type == vDirectory)
	    numDirVnodes++;
	else
	    numFileVnodes++;

	/*
	 * We know now all we would ever know about the vnode;
	 * insert it into our hash table (but only if we're not
	 * doing a vnode scan).
	 */

	if (!sizescan) {

	    vdata = InsertVnode(vnodeNumber, vnode);

	    if (vdata == NULL) {
		if (verbose)
		    fprintf(stderr,
			    "Failed to insert " "vnode into hash table");
		return -1;
	    }

	    vdata->dumpdata = offset;
	    vdata->datalength = length;

	    /*
	     * Save directory data, since we'll need it later.
	     */

	    if (vnode->type == vDirectory && length) {

		vdata->filedata = malloc(length);

		if (!vdata->filedata) {
		    if (verbose)
			fprintf(stderr,
				"Unable to " "allocate space for "
				"file data (%d)\n", length);
		    return -1;
		}

		oldoffset = ftello64(f);
		fseeko64(f, offset, SEEK_SET);

		if (fread(vdata->filedata, length, 1, f) != 1) {
		    if (verbose)
			fprintf(stderr, "Unable to " "read in file data!\n");
		    return -1;
		}

		fseeko64(f, oldoffset, SEEK_SET);
	    } else if (vnode->type == vDirectory)
		/*
		 * Warn the user we may not have all directory
		 * vnodes
		 */
		numNoDirData++;
	}
    }

    ungetc(tag, f);

    if (!sizescan) {

	numLargeVnodes = numDirVnodes;
	numSmallVnodes = numFileVnodes;

    } else {
	LargeVnodeIndex = (struct vnodeData **)
	    malloc(numDirVnodes * sizeof(struct vnodeData));
	SmallVnodeIndex = (struct vnodeData **)
	    malloc(numFileVnodes * sizeof(struct vnodeData));

	if (LargeVnodeIndex == NULL || SmallVnodeIndex == NULL) {
	    if (verbose)
		fprintf(stderr,
			"Unable to allocate space " "for vnode tables\n");
	    return -1;
	}
    }

    if (verbose)
	fprintf(stderr, "%s vnode scan completed\n",
		sizescan ? "Primary" : "Secondary");

    return 0;
}

/*
 * Perform an interactive restore
 *
 * Parsing the directory information is a pain, but other than that
 * we just use the other tools we already have in here.
 */
#define CMDBUFSIZE 	(AFSPATHMAX * 2)
static void
InteractiveRestore(FILE * f, VolumeDiskData * vol)
{
    struct vnodeData *vdatacwd;	/* Vnode data for our current dir */
    char cmdbuf[CMDBUFSIZE];
    int argc;
    char **argv;

    /*
     * Let's see if we can at least get the data for our root directory.
     * If we can't, there's no way we can do an interactive restore.
     */

    if ((vdatacwd = GetVnode(1)) == NULL) {
	fprintf(stderr, "No entry for our root vnode!  Aborting\n");
	return;
    }

    if (!vdatacwd->filedata) {
	fprintf(stderr,
		"There is no directory data for the root "
		"vnode (1.1).  An interactive\nrestore is not "
		"possible.\n");
	return;
    }

    /*
     * If you're doing a selective dump correctly, then you should get all
     * directory vnode data.  But just in case you didn't, let the user
     * know there may be a problem.
     */

    if (numNoDirData)
	fprintf(stderr,
		"WARNING: %d directory vnodes had no file "
		"data.  An interactive restore\nmay not be possible\n",
		numNoDirData);

    printf("> ");
    while (fgets(cmdbuf, CMDBUFSIZE, stdin)) {

	cmdbuf[strlen(cmdbuf) - 1] = '\0';

	if (strlen(cmdbuf) == 0) {
	    printf("> ");
	    continue;
	}

	MakeArgv(cmdbuf, &argc, &argv);

	if (strcmp(argv[0], "ls") == 0) {
	    DirectoryList(argc, argv, vdatacwd, vol);
	} else if (strcmp(argv[0], "cd") == 0) {
	    struct vnodeData *newvdata;

	    newvdata = ChangeDirectory(argc, argv, vdatacwd);

	    if (newvdata)
		vdatacwd = newvdata;
	} else if (strcmp(argv[0], "file") == 0) {
	    DumpAllFiles(argc, argv, vdatacwd, vol);
	} else if (strcmp(argv[0], "cp") == 0) {
	    CopyFile(argc, argv, vdatacwd, f);
	} else if (strcmp(argv[0], "vcp") == 0) {
	    CopyVnode(argc, argv, f);
	} else if (strcmp(argv[0], "quit") == 0
		   || strcmp(argv[0], "exit") == 0)
	    break;
	else if (strcmp(argv[0], "?") == 0 || strcmp(argv[0], "help") == 0) {
	    printf("Valid commands are:\n");
	    printf("\tls\t\tList current directory\n");
	    printf("\tcd\t\tChange current directory\n");
	    printf("\tcp\t\tCopy file from dump\n");
	    printf("\tvcp\t\tCopy file from dump (via vnode)\n");
#ifdef RESIDENCY
	    printf("\tfile\t\tList residency filenames\n");
#endif /* RESIDENCY */
	    printf("\tquit | exit\tExit program\n");
	    printf("\thelp | ?\tBrief help\n");
	} else
	    fprintf(stderr,
		    "Unknown command, \"%s\", enter "
		    "\"help\" for a list of commands.\n", argv[0]);

	printf("> ");
    }

    return;
}

/*
 * Do a listing of all files in a directory.  Sigh, I wish this wasn't
 * so complicated.
 *
 * With the reorganizing, this is just a front-end to DirListInternal()
 */

static void
DirectoryList(int argc, char **argv, struct vnodeData *vdata,
	      VolumeDiskData * vol)
{
    int errflg = 0, lflag = 0, iflag = 0, Fflag = 0, sflag = 0, Rflag = 0;
    int c;

    optind = 1;

    while ((c = getopt(argc, argv, "liFRs")) != EOF)
	switch (c) {
	case 'l':
	    lflag++;
	    break;
	case 'i':
	    iflag++;
	    break;
	case 'F':
	    Fflag++;
	    break;
	case 'R':
	    Rflag++;
	case 's':
	    sflag++;
	    break;
	case '?':
	default:
	    errflg++;
	}

    if (errflg) {
	fprintf(stderr, "Usage: %s [-liFs] filename [filename ...]\n",
		argv[0]);
	return;
    }

    DirListInternal(vdata, &(argv[optind]), argc - optind, lflag, iflag,
		    Fflag, Rflag, 1, vol, NULL);

    return;
}

/*
 * Function that does the REAL work in terms of directory listing
 */

static void
DirListInternal(struct vnodeData *vdata, char *pathnames[], int numpathnames,
		int lflag, int iflag, int Fflag, int Rflag, int verbose,
		VolumeDiskData * vol, char *path)
{
    struct DirEntry *ep, **eplist = NULL, **eprecurse = NULL;
    struct DirCursor cursor;
    struct vnodeData *lvdata;

    int i, j, numentries = 0, longestname = 0, numcols, col, numrows;
    int numrecurse = 0;

    if (!vdata->filedata) {
	fprintf(stderr, "There is no vnode data for this " "directory!\n");
	return;
    }

    ResetDirCursor(&cursor, vdata);

    /*
     * Scan through the whole directory
     */

    while ((ep = ReadNextDir(&cursor, vdata)) != NULL) {

	/*
	 * If we didn't get any filenames on the command line,
	 * get them all.
	 */

	if (numpathnames == 0) {
	    eplist =
		realloc(eplist, sizeof(struct DirEntry *) * ++numentries);
	    eplist[numentries - 1] = ep;
	    if (strlen(ep->name) > longestname)
		longestname = strlen(ep->name);
	    if (Rflag)
		if ((lvdata = GetVnode(ntohl(ep->fid.vnode)))
		    && lvdata->vnode->type == vDirectory
		    && !(strcmp(ep->name, ".") == 0
			 || strcmp(ep->name, "..") == 0)) {
		    eprecurse =
			realloc(eprecurse,
				sizeof(struct DirEntry *) * ++numrecurse);
		    eprecurse[numrecurse - 1] = ep;
		}

	} else {
	    /*
	     * Do glob matching via fnmatch()
	     */

	    for (i = 0; i < numpathnames; i++)
		if (fnmatch(pathnames[i], ep->name, FNM_PATHNAME) == 0) {
		    eplist =
			realloc(eplist,
				sizeof(struct DirEntry *) * ++numentries);
		    eplist[numentries - 1] = ep;
		    if (strlen(ep->name) > longestname)
			longestname = strlen(ep->name);
		    if (Rflag)
			if ((lvdata = GetVnode(ntohl(ep->fid.vnode)))
			    && lvdata->vnode->type == vDirectory
			    && !(strcmp(ep->name, ".") == 0
				 || strcmp(ep->name, "..") == 0)) {
			    eprecurse =
				realloc(eprecurse,
					sizeof(struct DirEntry *) *
					++numrecurse);
			    eprecurse[numrecurse - 1] = ep;
			}
		    break;
		}
	}
    }

    qsort((void *)eplist, numentries, sizeof(struct DirEntry *),
	  CompareDirEntry);

    if (Rflag && eprecurse)
	qsort((void *)eprecurse, numrecurse, sizeof(struct DirEntry *),
	      CompareDirEntry);
    /*
     * We don't have to do column printing if we have the -l or the -i
     * options.  Sigh, column printing is WAY TOO FUCKING COMPLICATED!
     */

    if (!lflag && !iflag) {
	char c;

	if (Fflag)
	    longestname++;

	longestname++;

	numcols = termsize / longestname ? termsize / longestname : 1;
	numrows = numentries / numcols + (numentries % numcols ? 1 : 0);

	for (i = 0; i < numrows; i++) {
	    col = 0;
	    while (col < numcols && (i + col * numrows) < numentries) {
		ep = eplist[i + col++ * numrows];
		if (Fflag) {
		    if (!(lvdata = GetVnode(ntohl(ep->fid.vnode))))
			c = ' ';
		    else if (lvdata->vnode->type == vDirectory)
			c = '/';
		    else if (lvdata->vnode->type == vSymlink)
			c = '@';
		    else if (lvdata->vnode->modeBits & 0111 != 0)
			c = '*';
		    else
			c = ' ';
		    printf("%s%-*c", ep->name, longestname - strlen(ep->name),
			   c);
		} else
		    printf("%-*s", longestname, ep->name);
	    }

	    printf("\n");
	}
    } else if (iflag)
	for (i = 0; i < numentries; i++)
	    if (!(lvdata = GetVnode(ntohl(eplist[i]->fid.vnode))))
		printf("%d.0.0\t%s\n",
		       vol->parentId ? vol->parentId : vol->id,
		       eplist[i]->name);
	    else if (path)
		printf("%d.%d.%d\t%s/%s\n", vol->id,
		       ntohl(eplist[i]->fid.vnode),
		       ntohl(eplist[i]->fid.vunique), path, eplist[i]->name);
	    else
		printf("%d.%d.%d\t%s\n", vol->id, ntohl(eplist[i]->fid.vnode),
		       ntohl(eplist[i]->fid.vunique), eplist[i]->name);
    else if (lflag) {
	for (i = 0; i < numentries; i++)
	    if (!(lvdata = GetVnode(ntohl(eplist[i]->fid.vnode))))
		printf("----------   0 0        " "0                 0 %s\n",
		       eplist[i]->name);
	    else {
		switch (lvdata->vnode->type) {
		case vDirectory:
		    printf("d");
		    break;
		case vSymlink:
		    printf("l");
		    break;
		default:
		    printf("-");
		}

		for (j = 8; j >= 0; j--) {
		    if (lvdata->vnode->modeBits & (1 << j))
			switch (j % 3) {
			case 2:
			    printf("r");
			    break;
			case 1:
			    printf("w");
			    break;
			case 0:
			    printf("x");
		    } else
			printf("-");
		}

		printf(" %-3d %-8d %-8d %10d %s\n", lvdata->vnode->linkCount,
		       lvdata->vnode->owner, lvdata->vnode->group,
		       lvdata->vnode->length, eplist[i]->name);
	    }
    }

    free(eplist);

    if (Rflag && eprecurse) {
	char *lpath;
	lpath = NULL;
	for (i = 0; i < numrecurse; i++) {
	    if (verbose)
		printf("\n%s:\n", eprecurse[i]->name);
	    if (path) {
		lpath = malloc(strlen(path) + strlen(eprecurse[i]->name) + 2);
		if (lpath)
		    sprintf(lpath, "%s/%s", path, eprecurse[i]->name);
	    }
	    DirListInternal(GetVnode(ntohl(eprecurse[i]->fid.vnode)), NULL, 0,
			    lflag, iflag, Fflag, Rflag, verbose, vol, lpath);
	    if (lpath) {
		free(lpath);
		lpath = NULL;
	    }
	}
    }

    if (eprecurse)
	free(eprecurse);

    return;
}


/*
 * Directory name comparison function, used by qsort
 */

static int
CompareDirEntry(const void *e1, const void *e2)
{
    struct DirEntry **ep1 = (struct DirEntry **)e1;
    struct DirEntry **ep2 = (struct DirEntry **)e2;

    return strcmp((*ep1)->name, (*ep2)->name);
}

/*
 * Change a directory.  Return a pointer to our new vdata structure for
 * this directory.
 */

static struct vnodeData *
ChangeDirectory(int argc, char **argv, struct vnodeData *vdatacwd)
{
    struct vnodeData *newvdatacwd;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s directory\n", argv[0]);
	return NULL;
    }

    if ((newvdatacwd = FindFile(vdatacwd, argv[1])) == NULL)
	return NULL;

    if (newvdatacwd->vnode->type != vDirectory) {
	fprintf(stderr, "%s: Not a directory\n", argv[1]);
	return NULL;
    }

    if (newvdatacwd->filedata == NULL) {
	fprintf(stderr, "%s: No directory data found.\n", argv[1]);
	return NULL;
    }

    return newvdatacwd;
}

/*
 * Copy a file from out of the dump file
 */

#define COPYBUFSIZE 8192

static void
CopyFile(int argc, char **argv, struct vnodeData *vdatacwd, FILE * f)
{
    struct vnodeData *vdata;
    FILE *out;
    off64_t cur = 0;
    int bytes, ret;
    char buffer[COPYBUFSIZE];

    if (argc != 3) {
	fprintf(stderr, "Usage: %s dumpfile destfile\n", argv[0]);
	return;
    }

    if ((vdata = FindFile(vdatacwd, argv[1])) == NULL)
	return;

    if (vdata->dumpdata == 0) {
	fprintf(stderr, "File %s has no data in dump file\n", argv[1]);
	return;
    }

    if ((out = fopen(argv[2], "wb")) == NULL) {
	fprintf(stderr, "Open of %s failed: %s\n", argv[2], strerror(errno));
	return;
    }

    if (fseeko64(f, vdata->dumpdata, SEEK_SET)) {
	fprintf(stderr, "Seek failed: %s\n", strerror(errno));
	fclose(out);
	return;
    }

    while (cur < vdata->datalength) {

	bytes =
	    cur + COPYBUFSIZE <
	    vdata->datalength ? COPYBUFSIZE : vdata->datalength - cur;

	ret = fread(buffer, sizeof(char), bytes, f);
	if (ret != bytes) {
	    if (ret != 0)
		fprintf(stderr, "Short read (expected %d, " "got %d)\n",
			bytes, ret);
	    else
		fprintf(stderr, "Error during read: %s\n", strerror(errno));
	    fclose(out);
	    return;
	}

	ret = fwrite(buffer, sizeof(char), bytes, out);
	if (ret != bytes) {
	    if (ret != 0)
		fprintf(stderr, "Short write (expected %d, " "got %d)\n",
			bytes, ret);
	    else
		fprintf(stderr, "Error during write: %s\n", strerror(errno));
	    fclose(out);
	    return;
	}

	cur += bytes;
    }

    fclose(out);
}

/*
 * Copy a file from out of the dump file, by using the vnode
 */

static void
CopyVnode(int argc, char *argv[], FILE * f)
{
    struct vnodeData *vdata;
    FILE *out;
    off64_t cur = 0;
    int bytes, ret;
    char buffer[COPYBUFSIZE];
    unsigned int vnode, uniquifier = 0;

    if (argc != 3) {
	fprintf(stderr, "Usage: %s vnode[.uniqifier] destfile\n", argv[0]);
	return;
    }

    ret = sscanf(argv[1], "%d.%d", &vnode, &uniquifier);

    if (ret < 1) {
	fprintf(stderr, "Invalid file identifier: %s\n", argv[1]);
	return;
    }

    if (!(vdata = GetVnode(vnode))) {
	fprintf(stderr, "Vnode %d not in dump file\n", vnode);
	return;
    }

    if (ret == 2 && vdata->vnode->uniquifier != uniquifier) {
	fprintf(stderr,
		"Specified uniquifier %d did not match "
		"uniquifier %d found in dump file!\n", uniquifier,
		vdata->vnode->uniquifier);
	return;
    }

    if (vdata->dumpdata == 0) {
	fprintf(stderr, "File %s has no data in dump file\n", argv[1]);
	return;
    }

    if ((out = fopen(argv[2], "wb")) == NULL) {
	fprintf(stderr, "Open of %s failed: %s\n", argv[2], strerror(errno));
	return;
    }

    if (fseeko64(f, vdata->dumpdata, SEEK_SET)) {
	fprintf(stderr, "Seek failed: %s\n", strerror(errno));
	fclose(out);
	return;
    }

    while (cur < vdata->datalength) {

	bytes =
	    cur + COPYBUFSIZE <
	    vdata->datalength ? COPYBUFSIZE : vdata->datalength - cur;

	ret = fread(buffer, sizeof(char), bytes, f);
	if (ret != bytes) {
	    if (ret != 0)
		fprintf(stderr, "Short read (expected %d, " "got %d)\n",
			bytes, ret);
	    else
		fprintf(stderr, "Error during read: %s\n", strerror(errno));
	    fclose(out);
	    return;
	}

	ret = fwrite(buffer, sizeof(char), bytes, out);
	if (ret != bytes) {
	    if (ret != 0)
		fprintf(stderr, "Short write (expected %d, " "got %d)\n",
			bytes, ret);
	    else
		fprintf(stderr, "Error during write: %s\n", strerror(errno));
	    fclose(out);
	    return;
	}

	cur += bytes;
    }

    fclose(out);
}

/*
 * Dump all residency filenames associated with a file, or all files
 * within a directory.
 */

static void
DumpAllFiles(int argc, char **argv, struct vnodeData *vdatacwd,
	     VolumeDiskData * vol)
{
#ifdef RESIDENCY
    struct vnodeData *vdata, *nvdata;
    struct DirCursor cursor;
    struct DirEntry *ep;
    FILE *f = stdout;
    int c, i;
    int dflag = 0, fflag = 0, errflg = 0;

    optind = 1;

    while ((c = getopt(argc, argv, "df:")) != EOF)
	switch (c) {
	case 'd':
	    dflag++;
	    break;
	case 'f':
	    if ((f = fopen(optarg, "a")) == NULL) {
		fprintf(stderr, "Cannot open \"%s\": %s\n", optarg,
			strerror(errno));
		return;
	    }
	    fflag++;
	    break;
	case 'h':
	case '?':
	default:
	    errflg++;
	}

    if (errflg || argc == optind) {
	fprintf(stderr, "Usage: %s [-d] [-f filename] file " "[file ...]\n",
		argv[0]);
	if (fflag)
	    fclose(f);
	return;
    }

    for (i = optind; i < argc; i++) {

	if ((vdata = FindFile(vdatacwd, argv[i])) == NULL)
	    continue;

	if (vdata->vnode->type == vDirectory && !dflag) {

	    ResetDirCursor(&cursor, vdata);

	    while ((ep = ReadNextDir(&cursor, vdata)) != NULL) {

		if (!(nvdata = GetVnode(ntohl(ep->fid.vnode)))) {
		    fprintf(stderr,
			    "Cannot find vnode " "entry for %s (%d)\n",
			    ep->name, ntohl(ep->fid.vnode));
		    continue;
		}


		if (!fflag) {
		    printf("Residency locations for %s:\n", ep->name);

		    if (nvdata->dumpdata)
			printf("Local disk (in dump " "file)\n");
		}

		DumpAllResidencies(f, nvdata, vol);

	    }

	} else {
	    if (!fflag) {
		printf("Residency locations for %s:\n", argv[i]);

		if (vdata->dumpdata)
		    printf("Local disk (in dump file)\n");
	    }

	    DumpAllResidencies(f, vdata, vol);
	}
    }

    if (fflag)
	fclose(f);
#else /* RESIDENCY */
    fprintf(stderr,
	    "The \"file\" command is not available in the non-"
	    "MRAFS version of dumptool.\n");
#endif /* RESIDENCY */
    return;
}

/*
 * Take a vnode, traverse the vnode chain, and dump out all files on
 * all residencies corresponding to that parent vnode.
 */

#ifdef RESIDENCY
static void
DumpAllResidencies(FILE * f, struct vnodeData *vdata,
		   struct VolumeDiskData *vol)
{
    unsigned int nextVnodeNum;

    while (nextVnodeNum = vdata->vnode->NextVnodeId) {
	if ((vdata = GetVnode(nextVnodeNum)) == NULL) {
	    fprintf(stderr,
		    "We had a pointer to %lu in it's "
		    "vnode chain, but there\nisn't a record of "
		    "it!  The dump might be corrupt.\n", nextVnodeNum);
	    return;
	}

	if (vdata->vnode->type == vFidLookup)
	    DumpVnodeFile(f, vdata->vnode, vol);
    }

    return;
}
#endif


/*
 * Given a directory vnode and a filename, return the vnode corresponding
 * to the file in that directory.
 * 
 * We now handle pathnames with directories in them.
 */

static struct vnodeData *
FindFile(struct vnodeData *vdatacwd, char *filename)
{
    struct DirHeader *dhp;
    struct DirEntry *ep;
    int i, num;
    struct vnodeData *vdata;
    char *c, newstr[MAXPATHLEN];

    if (!vdatacwd->filedata) {
	fprintf(stderr, "There is no vnode data for this " "directory!\n");
	return NULL;
    }

    /*
     * If we have a "/" in here, look up the vnode data for the
     * directory (everything before the "/") and use that as our
     * current directory.  We automagically handle multiple directories
     * by using FindFile recursively.
     */

    if ((c = strrchr(filename, '/')) != NULL) {

	strncpy(newstr, filename, c - filename);
	newstr[c - filename] = '\0';

	if ((vdatacwd = FindFile(vdatacwd, newstr)) == NULL)
	    return NULL;

	if (vdatacwd->vnode->type != vDirectory) {
	    fprintf(stderr, "%s: Not a directory\n", newstr);
	    return NULL;
	}

	filename = c + 1;
    }

    dhp = (struct DirHeader *)vdatacwd->filedata;

    i = DirHash(filename);

    num = ntohs(dhp->hashTable[i]);

    while (num) {
	ep = (struct DirEntry *)(vdatacwd->filedata + (num * 32));
	if (strcmp(ep->name, filename) == 0)
	    break;
	num = ntohs(ep->next);
    }

    if (!num) {
	fprintf(stderr, "%s: No such file or directory\n", filename);
	return NULL;
    }

    if ((vdata = GetVnode(ntohl(ep->fid.vnode))) == NULL) {
	fprintf(stderr, "%s: No vnode information for %lu found\n", filename,
		ntohl(ep->fid.vnode));
	return NULL;
    }

    return vdata;
}

/*
 * Reset a structure containing the current directory scan location
 */

static void
ResetDirCursor(struct DirCursor *cursor, struct vnodeData *vdata)
{
    struct DirHeader *dhp;

    cursor->hashbucket = 0;

    dhp = (struct DirHeader *)vdata->filedata;

    cursor->entry = ntohs(dhp->hashTable[0]);
}

/*
 * Given a cursor and a directory entry, return the next entry in the
 * directory.
 */

static struct DirEntry *
ReadNextDir(struct DirCursor *cursor, struct vnodeData *vdata)
{
    struct DirHeader *dhp;
    struct DirEntry *ep;

    dhp = (struct DirHeader *)vdata->filedata;

    if (cursor->entry) {
	ep = (struct DirEntry *)(vdata->filedata + (cursor->entry * 32));
	cursor->entry = ntohs(ep->next);
	return ep;
    } else {
	while (++(cursor->hashbucket) < NHASHENT) {
	    cursor->entry = ntohs(dhp->hashTable[cursor->hashbucket]);
	    if (cursor->entry) {
		ep = (struct DirEntry *)(vdata->filedata +
					 (cursor->entry * 32));
		cursor->entry = ntohs(ep->next);
		return ep;
	    }
	}
    }

    return NULL;
}

/*
 * Given a string, split it up into components a la Unix argc/argv.
 *
 * This code is most stolen from ftp.
 */

static void
MakeArgv(char *string, int *argc, char ***argv)
{
    static char *largv[64];
    char **la = largv;
    char *s = string;
    static char argbuf[CMDBUFSIZE];
    char *ap = argbuf;

    *argc = 0;
    *argv = largv;

    while (*la++ = GetToken(s, &s, ap, &ap))
	(*argc)++;
}

/*
 * Return a pointer to the next token, and update the current string
 * position.
 */

static char *
GetToken(char *string, char **nexttoken, char argbuf[], char *nextargbuf[])
{
    char *sp = string;
    char *ap = argbuf;
    int got_one = 0;

  S0:
    switch (*sp) {

    case '\0':
	goto OUTTOKEN;

    case ' ':
    case '\t':
	sp++;
	goto S0;

    default:
	goto S1;
    }

  S1:
    switch (*sp) {

    case ' ':
    case '\t':
    case '\0':
	goto OUTTOKEN;		/* End of our token */

    case '\\':
	sp++;
	goto S2;		/* Get next character */

    case '"':
	sp++;
	goto S3;		/* Get quoted string */

    default:
	*ap++ = *sp++;		/* Add a character to our token */
	got_one = 1;
	goto S1;
    }

  S2:
    switch (*sp) {

    case '\0':
	goto OUTTOKEN;

    default:
	*ap++ = *sp++;
	got_one = 1;
	goto S1;
    }

  S3:
    switch (*sp) {

    case '\0':
	goto OUTTOKEN;

    case '"':
	sp++;
	goto S1;

    default:
	*ap++ = *sp++;
	got_one = 1;
	goto S3;
    }

  OUTTOKEN:
    if (got_one)
	*ap++ = '\0';
    *nextargbuf = ap;		/* Update storage pointer */
    *nexttoken = sp;		/* Update token pointer */

    return got_one ? argbuf : NULL;
}

/*
 * Insert vnodes into our hash table.
 */

static struct vnodeData *
InsertVnode(unsigned int vnodeNumber, struct VnodeDiskObject *vnode)
{
    struct VnodeDiskObject *nvnode;
    struct vnodeData *vdata;
    static int curSmallVnodeIndex = 0;
    static int curLargeVnodeIndex = 0;
    struct vnodeData ***vnodeIndex;
    int *curIndex;

    nvnode = (struct VnodeDiskObject *)malloc(sizeof(struct VnodeDiskObject));

    if (!nvnode) {
	if (verbose)
	    fprintf(stderr, "Unable to allocate space for vnode\n");
	return NULL;
    }

    memcpy((void *)nvnode, (void *)vnode, sizeof(struct VnodeDiskObject));

    if (vnodeNumber & 1) {
	vnodeIndex = &LargeVnodeIndex;
	curIndex = &curLargeVnodeIndex;
    } else {
	vnodeIndex = &SmallVnodeIndex;
	curIndex = &curSmallVnodeIndex;
    }

    vdata = (struct vnodeData *)malloc(sizeof(struct vnodeData));

    vdata->vnode = nvnode;
    vdata->vnodeNumber = vnodeNumber;
    vdata->dumpdata = 0;
    vdata->filedata = 0;
    vdata->datalength = 0;

    (*vnodeIndex)[(*curIndex)++] = vdata;

    return vdata;
}

/*
 * Routine to retrieve a vnode from the hash table.
 */

static struct vnodeData *
GetVnode(unsigned int vnodeNumber)
{
    struct vnodeData vnode, *vnodep, **tmp;

    vnode.vnodeNumber = vnodeNumber;
    vnodep = &vnode;

    tmp = (struct vnodeData **)
	bsearch((void *)&vnodep,
		vnodeNumber & 1 ? LargeVnodeIndex : SmallVnodeIndex,
		vnodeNumber & 1 ? numLargeVnodes : numSmallVnodes,
		sizeof(struct vnodeData *), CompareVnode);

    return tmp ? *tmp : NULL;
}

/*
 * Our comparator function for bsearch
 */

static int
CompareVnode(const void *node1, const void *node2)
{
    struct vnodeData **vnode1 = (struct vnodeData **)node1;
    struct vnodeData **vnode2 = (struct vnodeData **)node2;

    if ((*vnode1)->vnodeNumber == (*vnode2)->vnodeNumber)
	return 0;
    else if ((*vnode1)->vnodeNumber > (*vnode2)->vnodeNumber)
	return 1;
    else
	return -1;
}

#ifdef RESIDENCY
/*
 * Dump out the filename corresponding to a particular vnode.
 *
 * This routine has the following dependancies:
 *
 * - Only will work on UFS filesystems at this point
 * - Has to talk to the rsserver.
 * - Can only determine UFS algorithm type when run on the same machine
 *   as the residency (unless you manually specify algorithm information)
 */

static int
DumpVnodeFile(FILE * f, struct VnodeDiskObject *vnode, VolumeDiskData * vol)
{
    static int rscache = 0;
    static rsaccessinfoList rsnlist = { 0, 0 };
    char MountPoint[MAXPATHLEN + 1];
    char FileName[MAXPATHLEN + 1];
    unsigned int Size, Level[4];
    unsigned int DeviceTag, Algorithm;
    FileSystems *FSInfo;
    int i, found, FSType, rsindex;

    /*
     * Maybe we found out something about this residency via the
     * command-line; check that first.
     */

    rsindex = ffs(VLkp_Residencies(vnode)) - 1;

    /*
     * We need to get information from the rsserver (so we can
     * find out the device tag for a given residency).  If we
     * haven't cached that, talk to the rsserver to get it.
     * If we have info about this already, then don't talk to
     * the rsserver (this lets us still do disaster recovery if
     * MR-AFS is completely hosed).
     */

    if (!rscache && rscmdlineinfo[rsindex].DeviceTag == -1) {
	int code;

	code = ServerInitResidencyConnection();

	if (code) {
	    fprintf(stderr,
		    "ServerInitResidencyConnection failed " "with code %d\n",
		    code);
	    return -1;
	}

	code = rs_GetResidencySummary(ServerRequestorId, &rsnlist);

	if (code) {
	    fprintf(stderr, "rs_GetResidencySummary failed " "with code %d\n",
		    code);
	    return -1;
	}

	rscache = 1;
    }

    /*
     * For a given residency (as specified in the vnode),
     * find out it's device tag number, either via the rsserver
     * or via the command line.
     */

    if (rscmdlineinfo[rsindex].DeviceTag != -1) {
	DeviceTag = rscmdlineinfo[rsindex].DeviceTag;
	found = 1;
    } else
	for (i = 0, found = 0; (i < rsnlist.rsaccessinfoList_len) && (!found);
	     i++) {
	    if (rsnlist.rsaccessinfoList_val[i].id.residency ==
		VLkp_Residencies(vnode)) {
		found = 1;
		DeviceTag = rsnlist.rsaccessinfoList_val[i].devicetagnumber;
		break;
	    }
	}

    if (!found) {
	if (verbose)
	    fprintf(stderr,
		    "Unable to find residency %d in "
		    "rsserver database, aborting\n", VLkp_Residencies(vnode));
	return -1;
    }

    /*
     * Okay, now we've got the DeviceTag ... which we can use to
     * lookup the on-disk configuration information (which we
     * assume is locally stored).  We also need the DeviceTag to
     * print out which partition we're using (but that comes later).
     *
     * We lookup the on-disk configuration information by calling
     * Ufs_GetFSInfo() to get the configuration information on the
     * filesystems specified by the given DeviceTag.
     *
     * Before we call Ufs_GetFSInfo, check the command-line cache;
     * if we got something via the command-line, don't go to disk.
     */

    if (rscmdlineinfo[rsindex].FSType == -1
	&& Ufs_GetFSInfo(&FSInfo, DeviceTag)) {
	if (verbose)
	    fprintf(stderr,
		    "Ufs_GetFSInfo failed for DeviceTag "
		    "%d, Residency %d\n", DeviceTag, VLkp_Residencies(vnode));
	return -1;
    }

    /*
     * The FSInfo structure has the last two things we need: the
     * FSType (ufs, slowufs, etc etc), and the usage algorithm (which
     * ends up being how many directories are being used on the
     * residency filesystem).
     *
     * With these last two parameters, use routines stolen from
     * ufsname to generate the filename.
     *
     * (Actually, I lied - we also need the "Size" parameter, which
     * we can also get from FSInfo);
     */

    if (rscmdlineinfo[rsindex].FSType != -1) {
	FSType = rscmdlineinfo[rsindex].FSType;
	Algorithm = rscmdlineinfo[rsindex].Algorithm;
	Size = rscmdlineinfo[rsindex].Size;
    } else {
	FSType = FSInfo->FileSystems_u.UfsInterface.FSType;
	Algorithm = FSInfo->FileSystems_u.UfsInterface.Algorithm;
	if (FSInfo->FileSystems_u.UfsInterface.Directories[1] == 0)
	    Size = 0;
	else if (FSInfo->FileSystems_u.UfsInterface.Directories[1] == 16)
	    Size = 1;
	else if (FSInfo->FileSystems_u.UfsInterface.Directories[1] == 256)
	    Size = 2;
	else {
	    if (verbose)
		fprintf(stderr, "Unknown directory size %d, " "aborting\n",
			FSInfo->FileSystems_u.UfsInterface.Directories[1]);
	    return -1;
	}
    }

    /*
     * First, generate our mount point from the DeviceTag and
     * FSType.
     */

    DEVICETAGNUMBERTOMOUNTPOINT(MountPoint, DeviceTag, FSType);

    /*
     * Then, generate the "level" (directory bitmasks) from the
     * file tags, size, and algorithm
     */

    UfsTagsToLevel(VLkp_FileTag1(vnode), VLkp_FileTag2(vnode), Algorithm,
		   Size, Level, VLkp_ParentVnodeId(vnode),
		   VLkp_ParentUniquifierId(vnode));

    /*
     * Finally, take the above information and generate the
     * corresponding filename (this macro ends up being a
     * sprintf() call)
     */

    TAGSTONAME(FileName, MountPoint, Level, Directories[Size][1],
	       vol->parentId, VLkp_ParentVnodeId(vnode),
	       VLkp_ParentUniquifierId(vnode), Algorithm);

    fprintf(f, "%s\n", FileName);

    return 0;
}
#endif

/*
 * Read a 16 bit integer in network order
 */

static int
ReadInt16(FILE * f, unsigned short *s)
{
    unsigned short in;

    if (fread((void *)&in, sizeof(in), 1, f) != 1) {
	if (verbose)
	    fprintf(stderr, "ReadInt16 failed!\n");
	return -1;
    }

    *s = ntohs(in);

    return 0;
}


/*
 * Read a 32 bit integer in network order
 */

static int
ReadInt32(FILE * f, unsigned int *i)
{
    unsigned int in;

    if (fread((void *)&in, sizeof(in), 1, f) != 1) {
	if (verbose)
	    fprintf(stderr, "ReadInt32 failed!\n");
	return -1;
    }

    *i = ntohl((unsigned long)in);

    return 0;
}

/*
 * Read a string from a dump file
 */

static int
ReadString(FILE * f, char *string, int maxlen)
{
    int c;

    while (maxlen--) {
	if ((*string++ = getc(f)) == 0)
	    break;
    }

    /*
     * I'm not sure what the _hell_ this is supposed to do ...
     * but it was in the original dump code
     */

    if (string[-1]) {
	while ((c = getc(f)) && c != EOF);
	string[-1] = 0;
    }

    return 0;
}

static int
ReadByteString(FILE * f, void *s, int size)
{
    unsigned char *c = (unsigned char *)s;

    while (size--)
	*c++ = getc(f);

    return 0;
}

/*
 * The directory hashing algorithm used by AFS
 */

DirHash(string)
     register char *string;
{
    /* Hash a string to a number between 0 and NHASHENT. */
    register unsigned char tc;
    register int hval;
    register int tval;
    hval = 0;
    while (tc = (*string++)) {
	hval *= 173;
	hval += tc;
    }
    tval = hval & (NHASHENT - 1);
#ifdef AFS_CRAY_ENV		/* actually, any > 32 bit environment */
    if (tval == 0)
	return tval;
    else if (hval & 0x80000000)
	tval = NHASHENT - tval;
#else /* AFS_CRAY_ENV */
    if (tval == 0)
	return tval;
    else if (hval < 0)
	tval = NHASHENT - tval;
#endif /* AFS_CRAY_ENV */
    return tval;
}

#ifdef RESIDENCY
/*
 * Sigh, we need this for the AFS libraries
 */

int
LogErrors(int level, char *a, char *b, char *c, char *d, char *e, char *f,
	  char *g, char *h, char *i, char *j, char *k)
{
    if (level <= 0) {
	fprintf(stderr, a, b, c, d, e, f, g, h, i, j, k);
    }
    return 0;
}

/*
 * These are routines taken from AFS libraries and programs.  Most of
 * them are from ufsname.c, but a few are from the dir library (the dir
 * library has a bunch of hidden dependancies, so it's not suitable to
 * include it outright).
 */

UfsEntropiesToTags(HighEntropy, LowEntropy, Algorithm, FileTag1, FileTag2)
     uint32_t HighEntropy;
     uint32_t LowEntropy;
     uint32_t Algorithm;
     uint32_t *FileTag1;
     uint32_t *FileTag2;
{
    int i;

    if ((Algorithm > UFS_ALGORITHMS) || (Algorithm <= 0))
	return -1;
    *FileTag1 = 0;
    *FileTag2 = 0;
    for (i = 0; i < 32; ++i) {
	if (UfsEntropy[Algorithm - 1][i] < 32)
	    *FileTag1 |=
		((HighEntropy & (1 << i)) ==
		 0) ? 0 : 1 << UfsEntropy[Algorithm - 1][i];
	else
	    *FileTag2 |=
		((HighEntropy & (1 << i)) ==
		 0) ? 0 : 1 << (UfsEntropy[Algorithm - 1][i] - 32);
    }
    for (i = 32; i < 64; ++i) {
	if (UfsEntropy[Algorithm - 1][i] < 32)
	    *FileTag1 |=
		((LowEntropy & (1 << (i - 32))) ==
		 0) ? 0 : 1 << UfsEntropy[Algorithm - 1][i];
	else
	    *FileTag2 |=
		((LowEntropy & (1 << (i - 32))) ==
		 0) ? 0 : 1 << (UfsEntropy[Algorithm - 1][i] - 32);
    }
    return 0;
}

uint32_t
UfsTagsToHighEntropy(FileTag1, FileTag2, Algorithm)
     uint32_t FileTag1;
     uint32_t FileTag2;
     uint32_t Algorithm;
{
    int i;
    uint32_t Value;

    Value = 0;
    for (i = 0; i < 32; ++i) {
	if (UfsEntropy[Algorithm - 1][i] < 32)
	    Value |= ((FileTag1 & (1 << UfsEntropy[Algorithm - 1][i]))
		      == 0) ? 0 : 1 << i;
	else
	    Value |=
		((FileTag2 & (1 << (UfsEntropy[Algorithm - 1][i] - 32))) ==
		 0) ? 0 : 1 << i;
    }
    return Value;
}

uint32_t
UfsTagsToLowEntropy(FileTag1, FileTag2, Algorithm)
     uint32_t FileTag1;
     uint32_t FileTag2;
     uint32_t Algorithm;
{
    int i;
    uint32_t Value;

    Value = 0;
    for (i = 32; i < 64; ++i) {
	if (UfsEntropy[Algorithm - 1][i] < 32)
	    Value |= ((FileTag1 & (1 << UfsEntropy[Algorithm - 1][i]))
		      == 0) ? 0 : 1 << (i - 32);
	else
	    Value |=
		((FileTag2 & (1 << (UfsEntropy[Algorithm - 1][i] - 32))) ==
		 0) ? 0 : 1 << (i - 32);
    }
    return Value;
}

UfsTagsToLevel(FileTag1, FileTag2, Algorithm, Size, Sections, vnode,
	       Uniquifier)
     uint32_t FileTag1;
     uint32_t FileTag2;
     uint32_t Algorithm;
     uint32_t Size;
     uint32_t Sections[4];
     uint32_t vnode;
     uint32_t Uniquifier;
{
    uint32_t HighEntropy;
    uint32_t LowEntropy;

    switch (Algorithm) {
    case 1:
	LowEntropy = UfsTagsToLowEntropy(FileTag1, FileTag2, Algorithm);
	HighEntropy = UfsTagsToHighEntropy(FileTag1, FileTag2, Algorithm);
	Sections[0] = HighEntropy % Directories[Size][0];
	HighEntropy /= Directories[Size][0];
	if (Directories[Size][1]) {
	    Sections[1] = HighEntropy % Directories[Size][1];
	    HighEntropy /= Directories[Size][1];
	    Sections[2] = HighEntropy;
	    Sections[3] = LowEntropy;
	} else {
	    Sections[1] = HighEntropy;
	    Sections[2] = LowEntropy;
	}
	break;
    case 2:
	Sections[0] = FileTag1 & 0xff;
	if (Directories[Size][1]) {
	    Sections[1] = Uniquifier & 0xff;
	    if (Directories[Size][1] == 16)
		Sections[1] &= 0xf;
	    Sections[2] = FileTag1;
	    Sections[3] = FileTag2;
	} else {
	    Sections[1] = FileTag1;
	    Sections[2] = FileTag2;
	}
	break;
    case 3:
	Sections[0] = FileTag1 & 0xff;
	if (Directories[Size][1]) {
	    Sections[1] = (vnode >> 1) & 0xff;
	    if (Directories[Size][1] == 16)
		Sections[1] &= 0xf;
	    Sections[2] = FileTag1;
	    Sections[3] = FileTag2;
	} else {
	    Sections[1] = FileTag1;
	    Sections[2] = FileTag2;
	}
	break;
    default:
	fprintf(stderr, "UfsTagsToLevel: bad algorithm %lu!\n", Algorithm);
	return -1;
    }
    return 0;
}

#include <afs/afscbdummies.h>
#endif /* RESIDENCY */
