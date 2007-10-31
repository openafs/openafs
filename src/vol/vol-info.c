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
#include "volinodes.h"
#include <afs/afssyscalls.h>

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

int DumpVnodes = 0;		/* Dump everything, i.e. summary of all vnodes */
int DumpInodeNumber = 0;	/* Dump inode numbers with vnodes */
int DumpDate = 0;		/* Dump vnode date (server modify date) with vnode */
int InodeTimes = 0;		/* Dump some of the dates associated with inodes */
#if defined(AFS_NAMEI_ENV)
int PrintFileNames = 0;
#endif
int online = 0;
int dheader = 0;
int dsizeOnly = 0, totvolsize = 0, Vauxsize = 0, Vdiskused = 0, Vvnodesize =
    0;
int Vvnodesize_k = 0, Vauxsize_k = 0;
int Totvolsize = 0, TVauxsize = 0, TVdiskused = 0, TVvnodesize = 0;
int Stotvolsize = 0, SVauxsize = 0, SVdiskused = 0, SVvnodesize = 0;
int fixheader = 0, saveinodes = 0, orphaned = 0;
int VolumeChanged;

/* Forward Declarations */
void PrintHeader(register Volume * vp);
void HandleAllPart(void);
void HandlePart(struct DiskPartition *partP);
void HandleVolume(struct DiskPartition *partP, char *name);
struct DiskPartition *FindCurrentPartition(void);
Volume *AttachVolume(struct DiskPartition *dp, char *volname,
		     register struct VolumeHeader *header);
#if defined(AFS_NAMEI_ENV)
void PrintVnode(int offset, VnodeDiskObject * vnode, VnodeId vnodeNumber,
		Inode ino, Volume * vp);
#else
void PrintVnode(int offset, VnodeDiskObject * vnode, VnodeId vnodeNumber,
		Inode ino);
#endif
void PrintVnodes(Volume * vp, VnodeClass class);

char *
date(time_t date)
{
#define MAX_DATE_RESULT	100
    static char results[8][MAX_DATE_RESULT];
    static next;
    struct tm *tm = localtime(&date);
    char buf[32];

    (void)strftime(buf, 32, "%Y/%m/%d.%H:%M:%S", tm);	/* NT does not have %T */
    (void)afs_snprintf(results[next = (next + 1) & 7], MAX_DATE_RESULT,
		       "%lu (%s)", (unsigned long)date, buf);
    return results[next];
}

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

char name[VMAXPATHLEN];

char BU[1000];

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
	printf("Inode %s: Bad magic %x (%x): IGNORED\n",
	       PrintInode(NULL, ih->ih_ino), vsn->magic, magic);
    }

    /* Check is conditional, in case caller wants to inspect version himself */
    if (version && vsn->version != version) {
	bad++;
	printf("Inode %s: Bad version %x (%x): IGNORED\n",
	       PrintInode(NULL, ih->ih_ino), vsn->version, version);
    }
    if (bad && fixheader) {
	vsn->magic = magic;
	vsn->version = version;
	printf("Special index inode %s has a bad header. Reconstructing...\n",
	       PrintInode(NULL, ih->ih_ino));
	code = IH_IWRITE(ih, 0, to, size);
	if (code != size) {
	    printf("Write failed; header left in damaged state\n");
	}
    } else {
	if (!dsizeOnly && !saveinodes) {
	    printf("Inode %s: Good magic %x and version %x\n",
		   PrintInode(NULL, ih->ih_ino), magic, version);
	}
    }
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
    struct DiskPartition *partP = NULL;


#ifndef AFS_NT40_ENV
    if (geteuid() != 0) {
	printf("vol-info must be run as root; sorry\n");
	exit(1);
    }
#endif

    if (as->parms[0].items)
	online = 1;
    else
	online = 0;
    if (as->parms[1].items)
	DumpVnodes = 1;
    else
	DumpVnodes = 0;
    if (as->parms[2].items)
	DumpDate = 1;
    else
	DumpDate = 0;
    if (as->parms[3].items)
	DumpInodeNumber = 1;
    else
	DumpInodeNumber = 0;
    if (as->parms[4].items)
	InodeTimes = 1;
    else
	InodeTimes = 0;
    if ((ti = as->parms[5].items))
	partName = ti->data;
    if ((ti = as->parms[6].items))
	volumeId = strtoul(ti->data, NULL, 10);
    if (as->parms[7].items)
	dheader = 1;
    else
	dheader = 0;
    if (as->parms[8].items) {
	dsizeOnly = 1;
	dheader = 1;
	DumpVnodes = 1;
    } else
	dsizeOnly = 0;
    if (as->parms[9].items) {
	fixheader = 1;
    } else
	fixheader = 0;
    if (as->parms[10].items) {
	saveinodes = 1;
	dheader = 1;
	DumpVnodes = 1;
    } else
	saveinodes = 0;
    if (as->parms[11].items) {
	orphaned = 1;
	DumpVnodes = 1;
    } else
#if defined(AFS_NAMEI_ENV)
    if (as->parms[12].items) {
	PrintFileNames = 1;
	DumpVnodes = 1;
    } else
#endif
	orphaned = 0;

    DInit(10);

    err = VAttachPartitions();
    if (err) {
	printf("%d partitions had errors during attach.\n", err);
    }

    if (partName) {
	partP = VGetPartition(partName, 0);
	if (!partP) {
	    printf("%s is not an AFS partition name on this server.\n",
		   partName);
	    exit(1);
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
		printf("Current partition is not a vice partition.\n");
		exit(1);
	    }
	}
	(void)afs_snprintf(name1, sizeof name1, VFORMAT,
			   (unsigned long)volumeId);
	if (dsizeOnly && !saveinodes)
	    printf
		("Volume-Id\t  Volsize  Auxsize Inodesize  AVolsize SizeDiff                (VolName)\n");
	HandleVolume(partP, name1);
    }
    return 0;
}

#ifdef AFS_NT40_ENV
#include <direct.h>
struct DiskPartition *
FindCurrentPartition()
{
    int dr = _getdrive();
    struct DiskPartition *dp;

    dr--;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	if (*dp->devName - 'A' == dr)
	    break;
    }
    if (!dp) {
	printf("Current drive is not a valid vice partition.\n");
    }
    return dp;
}
#else
struct DiskPartition *
FindCurrentPartition()
{
    char partName[1024];
    char tmp = '\0';
    char *p;
    struct DiskPartition *dp;

    if (!getcwd(partName, 1023)) {
	perror("pwd");
	exit(1);
    }
    p = strchr(&partName[1], '/');
    if (p) {
	tmp = *p;
	*p = '\0';
    }
    if (!(dp = VGetPartition(partName, 0))) {
	if (tmp)
	    *p = tmp;
	printf("%s is not a valid vice partition.\n", partName);
	exit(1);
    }
    return dp;
}
#endif

void
HandleAllPart(void)
{
    struct DiskPartition *partP;


    for (partP = DiskPartitionList; partP; partP = partP->next) {
	printf("Processing Partition %s:\n", partP->name);
	HandlePart(partP);
	Stotvolsize += Totvolsize;
	SVauxsize += TVauxsize;
	SVvnodesize += TVvnodesize;
	SVdiskused += TVdiskused;
    }

    if (dsizeOnly) {
	printf("\nServer Totals%12d%9d%10d%10d%9d\n", SVdiskused, SVauxsize,
	       SVvnodesize, Stotvolsize, Stotvolsize - SVdiskused);
    }
}


void
HandlePart(struct DiskPartition *partP)
{
    int nvols = 0;
    DIR *dirp;
    struct dirent *dp;
#ifdef AFS_NT40_ENV
    char pname[64];
    char *p = pname;
    (void)sprintf(pname, "%s\\", VPartitionPath(partP));
#else
    char *p = VPartitionPath(partP);
#endif

    if (chdir(p) == -1) {
	printf("Can't chdir to partition %s; giving up\n", p);
	exit(1);
    }
    if ((dirp = opendir(".")) == NULL) {
	printf("Can't read directory %s; giving up\n", p);
	exit(1);
    }
    if (dsizeOnly && !saveinodes)
	printf
	    ("Volume-Id\t  Volsize  Auxsize Inodesize  AVolsize SizeDiff                (VolName)\n");
    while ((dp = readdir(dirp))) {
	p = (char *)strrchr(dp->d_name, '.');
	if (p != NULL && strcmp(p, VHDREXT) == 0) {
	    HandleVolume(partP, dp->d_name);
	    Totvolsize += totvolsize;
	    TVauxsize += Vauxsize;
	    TVvnodesize += Vvnodesize;
	    TVdiskused += Vdiskused;
	    nvols++;
	}
    }
    closedir(dirp);
    if (dsizeOnly) {
	printf("\nPart Totals  %12d%9d%10d%10d%9d (%d volumes)\n\n",
	       TVdiskused, TVauxsize, TVvnodesize, Totvolsize,
	       Totvolsize - TVdiskused, nvols);
    }
}


void
HandleVolume(struct DiskPartition *dp, char *name)
{
    struct VolumeHeader header;
    struct VolumeDiskHeader diskHeader;
    struct afs_stat status, stat;
    register int fd;
    Volume *vp;
    IHandle_t *ih;
    char headerName[1024];

    if (online) {
	printf("volinfo: -online not supported\n");
	exit(1);
    } else {
	afs_int32 n;

	(void)afs_snprintf(headerName, sizeof headerName, "%s/%s",
			   VPartitionPath(dp), name);
	if ((fd = afs_open(headerName, O_RDONLY)) == -1
	    || afs_fstat(fd, &status) == -1) {
	    printf("Volinfo: Cannot read volume header %s\n", name);
	    close(fd);
	    exit(1);
	}
	n = read(fd, &diskHeader, sizeof(diskHeader));

	if (n != sizeof(diskHeader)
	    || diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
	    printf("Volinfo: Error reading volume header %s\n", name);
	    exit(1);
	}
	if (diskHeader.stamp.version != VOLUMEHEADERVERSION) {
	    printf
		("Volinfo: Volume %s, version number is incorrect; volume needs salvage\n",
		 name);
	    exit(1);
	}
	DiskToVolumeHeader(&header, &diskHeader);

	if (dheader) {
	    FdHandle_t *fdP;
	    int size = 0;
	    int code;

	    if (afs_fstat(fd, &stat) == -1) {
		perror("stat");
		exit(1);
	    }
	    if (!dsizeOnly && !saveinodes) {
		printf("Volume header (size = %d):\n", size = stat.st_size);
		printf("\tstamp\t= 0x%x\n", header.stamp.version);
		printf("\tVolId\t= %u\n", header.id);
	    }

	    IH_INIT(ih, dp->device, header.parent, header.volumeInfo);
	    fdP = IH_OPEN(ih);
	    if (fdP == NULL) {
		perror("opening volume info");
		exit(1);
	    }
	    code = FDH_SIZE(fdP);
	    if (code == -1) {
		perror("fstat");
		exit(1);
	    }
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ih);
	    size += code;
	    if (!dsizeOnly && !saveinodes) {
		printf("\tparent\t= %u\n", header.parent);
		printf("\tInfo inode\t= %s (size = %d)\n",
		       PrintInode(NULL, header.volumeInfo), code);
	    }

	    IH_INIT(ih, dp->device, header.parent, header.smallVnodeIndex);
	    fdP = IH_OPEN(ih);
	    if (fdP == NULL) {
		perror("opening small vnode index");
		exit(1);
	    }
	    code = FDH_SIZE(fdP);
	    if (code == -1) {
		perror("fstat");
		exit(1);
	    }
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ih);
	    size += code;
	    if (!dsizeOnly && !saveinodes) {
		printf("\tSmall inode\t= %s (size = %d)\n",
		       PrintInode(NULL, header.smallVnodeIndex), code);
	    }

	    IH_INIT(ih, dp->device, header.parent, header.largeVnodeIndex);
	    fdP = IH_OPEN(ih);
	    if (fdP == NULL) {
		perror("opening large vnode index");
		exit(1);
	    }
	    code = FDH_SIZE(fdP);
	    if (code == -1) {
		perror("fstat");
		exit(1);
	    }
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ih);
	    size += code;
	    if (!dsizeOnly && !saveinodes) {
		printf("\tLarge inode\t= %s (size = %d)\n",
		       PrintInode(NULL, header.largeVnodeIndex), code);
#ifndef AFS_NT40_ENV
		printf("Total aux volume size = %d\n\n", size);
#endif
	    }
#ifdef AFS_NAMEI_ENV
	    IH_INIT(ih, dp->device, header.parent, header.linkTable);
	    fdP = IH_OPEN(ih);
	    if (fdP == NULL) {
		perror("opening link table index");
		exit(1);
	    }
	    code = FDH_SIZE(fdP);
	    if (code == -1) {
		perror("fstat");
		exit(1);
	    }
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ih);
	    size += code;
	    if (!dsizeOnly && !saveinodes) {
		printf("\tLink inode\t= %s (size = %d)\n",
		       PrintInode(NULL, header.linkTable), code);
		printf("Total aux volume size = %d\n\n", size);
	    }
#endif
	    Vauxsize = size;
	    Vauxsize_k = size / 1024;
	}
	close(fd);
	vp = AttachVolume(dp, name, &header);
	if (!vp) {
	    printf("Volinfo: Error attaching volume header %s\n", name);
	    return;
	}
    }
    PrintHeader(vp);
    if (DumpVnodes) {
	if (!dsizeOnly && !saveinodes)
	    printf("\nLarge vnodes (directories)\n");
	PrintVnodes(vp, vLarge);
	if (!dsizeOnly && !saveinodes) {
	    printf("\nSmall vnodes(files, symbolic links)\n");
	    fflush(stdout);
	}
	if (saveinodes)
	    printf("Saving all volume files to current directory ...\n");
	PrintVnodes(vp, vSmall);
    }
    if (dsizeOnly) {
	totvolsize = Vauxsize_k + Vvnodesize_k;
	if (saveinodes)
	    printf
		("Volume-Id\t  Volsize  Auxsize Inodesize  AVolsize SizeDiff                (VolName)\n");
	printf("%u\t%9d%9d%10d%10d%9d\t%24s\n", V_id(vp), Vdiskused,
	       Vauxsize_k, Vvnodesize_k, totvolsize, totvolsize - Vdiskused,
	       V_name(vp));
    }
    free(vp->header);
    free(vp);
}

int
main(int argc, char **argv)
{
    register struct cmd_syndesc *ts;
    afs_int32 code;

    ts = cmd_CreateSyntax(NULL, handleit, NULL, "Dump volume's internal state");
    cmd_AddParm(ts, "-online", CMD_FLAG, CMD_OPTIONAL,
		"Get info from running fileserver");
    cmd_AddParm(ts, "-vnode", CMD_FLAG, CMD_OPTIONAL, "Dump vnode info");
    cmd_AddParm(ts, "-date", CMD_FLAG, CMD_OPTIONAL,
		"Also dump vnode's mod date");
    cmd_AddParm(ts, "-inode", CMD_FLAG, CMD_OPTIONAL,
		"Dump vnode's inode number");
    cmd_AddParm(ts, "-itime", CMD_FLAG, CMD_OPTIONAL,
		"Dump special inode's mod times");
    cmd_AddParm(ts, "-part", CMD_LIST, CMD_OPTIONAL,
		"AFS partition name (default current partition)");
    cmd_AddParm(ts, "-volumeid", CMD_LIST, CMD_OPTIONAL, "Volume id");
    cmd_AddParm(ts, "-header", CMD_FLAG, CMD_OPTIONAL,
		"Dump volume's header info");
    cmd_AddParm(ts, "-sizeOnly", CMD_FLAG, CMD_OPTIONAL,
		"Dump volume's size");
    cmd_AddParm(ts, "-fixheader", CMD_FLAG, CMD_OPTIONAL,
		"Try to fix header");
    cmd_AddParm(ts, "-saveinodes", CMD_FLAG, CMD_OPTIONAL,
		"Try to save all inodes");
    cmd_AddParm(ts, "-orphaned", CMD_FLAG, CMD_OPTIONAL,
		"List all dir/files without a parent");
#if defined(AFS_NAMEI_ENV)
    cmd_AddParm(ts, "-filenames", CMD_FLAG, CMD_OPTIONAL, "Print filenames");
#endif
    code = cmd_Dispatch(argc, argv);
    return code;
}

#define typestring(type) (type == RWVOL? "read/write": type == ROVOL? "readonly": type == BACKVOL? "backup": "unknown")

void
PrintHeader(register Volume * vp)
{
    Vdiskused = V_diskused(vp);
    if (dsizeOnly || saveinodes)
	return;
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
}

/* GetFileInfo
 * OS independent file info. Die on failure.
 */
#ifdef AFS_NT40_ENV
char *
NT_date(FILETIME * ft)
{
    static char result[8][64];
    static int next = 0;
    SYSTEMTIME st;
    FILETIME lft;

    if (!FileTimeToLocalFileTime(ft, &lft)
	|| !FileTimeToSystemTime(&lft, &st)) {
	printf("Time conversion failed.\n");
	exit(1);
    }
    sprintf(result[next = ((next + 1) & 7)], "%4d/%02d/%02d.%2d:%2d:%2d",
	    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return result[next];
}
#endif

static void
GetFileInfo(FD_t fd, int *size, char **ctime, char **mtime, char **atime)
{
#ifdef AFS_NT40_ENV
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(fd, &fi)) {
	printf("GetFileInformationByHandle failed, exiting\n");
	exit(1);
    }
    *size = (int)fi.nFileSizeLow;
    *ctime = "N/A";
    *mtime = NT_date(&fi.ftLastWriteTime);
    *atime = NT_date(&fi.ftLastAccessTime);
#else
    struct afs_stat status;
    if (afs_fstat(fd, &status) == -1) {
	printf("fstat failed %d\n", errno);
	exit(1);
    }
    *size = (int)status.st_size;
    *ctime = date(status.st_ctime);
    *mtime = date(status.st_mtime);
    *atime = date(status.st_atime);
#endif
}

void
PrintVnodes(Volume * vp, VnodeClass class)
{
    afs_int32 diskSize =
	(class == vSmall ? SIZEOF_SMALLDISKVNODE : SIZEOF_LARGEDISKVNODE);
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    StreamHandle_t *file;
    register int vnodeIndex, nVnodes, offset = 0;
    Inode ino;
    IHandle_t *ih = vp->vnodeIndex[class].handle;
    FdHandle_t *fdP;
    int size;
    char *ctime, *atime, *mtime;
    char nfile[50], buffer[256];
    int total, ofd, len, code, bad = 0;

    fdP = IH_OPEN(ih);
    if (fdP == NULL) {
	printf("open failed\n");
	exit(1);
    }

    file = FDH_FDOPEN(fdP, "r");
    if (!file) {
	printf("fdopen failed\n");
	exit(1);
    }

    GetFileInfo(fdP->fd_fd, &size, &ctime, &atime, &mtime);
    if (InodeTimes && !dsizeOnly) {
	printf("ichanged : %s\nimodified: %s\niaccessed: %s\n\n", ctime,
	       mtime, atime);
    }

    nVnodes = (size / diskSize) - 1;
    if (nVnodes > 0) {
	STREAM_SEEK(file, diskSize, 0);
    } else
	nVnodes = 0;

    for (vnodeIndex = 0;
	 nVnodes && STREAM_READ(vnode, diskSize, 1, file) == 1;
	 nVnodes--, vnodeIndex++, offset += diskSize) {

	ino = VNDISK_GET_INO(vnode);
	if (saveinodes) {
	    if (VALID_INO(ino) && (class == vSmall)) {
		IHandle_t *ih1;
		FdHandle_t *fdP1;
		IH_INIT(ih1, V_device(vp), V_parentId(vp), ino);
		fdP1 = IH_OPEN(ih1);
		if (fdP1 == NULL) {
		    printf("Can't open inode %s error %d (ignored)\n",
			   PrintInode(NULL, ino), errno);
		    continue;
		}
		(void)afs_snprintf(nfile, sizeof nfile, "TmpInode.%s",
				   PrintInode(NULL, ino));
		ofd = afs_open(nfile, O_CREAT | O_RDWR | O_TRUNC, 0600);
		if (ofd < 0) {
		    printf("Can't create file %s; error %d (ignored)\n",
			   nfile, errno);
		    continue;
		}
		total = bad = 0;
		while (1) {
		    len = FDH_READ(fdP1, buffer, sizeof(buffer));
		    if (len < 0) {
			FDH_REALLYCLOSE(fdP1);
			IH_RELEASE(ih1);
			close(ofd);
			unlink(nfile);
			printf
			    ("Error while reading from inode %s (%d - ignored)\n",
			     PrintInode(NULL, ino), errno);
			bad = 1;
			break;
		    }
		    if (len == 0)
			break;	/* No more input */
		    code = write(ofd, buffer, len);
		    if (code != len) {
			FDH_REALLYCLOSE(fdP1);
			IH_RELEASE(ih1);
			close(ofd);
			unlink(nfile);
			printf
			    ("Error while writing to \"%s\" (%d - ignored)\n",
			     nfile, errno);
			bad = 1;
			break;
		    }
		    total += len;
		}
		if (bad)
		    continue;
		FDH_REALLYCLOSE(fdP1);
		IH_RELEASE(ih1);
		close(ofd);
		printf("... Copied inode %s to file %s (%d bytes)\n",
		       PrintInode(NULL, ino), nfile, total);
	    }
	} else {
#if defined(AFS_NAMEI_ENV)
	    PrintVnode(offset, vnode,
		       bitNumberToVnodeNumber(vnodeIndex, class), ino, vp);
#else
	    PrintVnode(offset, vnode,
		       bitNumberToVnodeNumber(vnodeIndex, class), ino);
#endif
	}
    }
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
}

#if defined(AFS_NAMEI_ENV)
void
PrintVnode(int offset, VnodeDiskObject * vnode, VnodeId vnodeNumber,
	   Inode ino, Volume * vp)
#else
void
PrintVnode(int offset, VnodeDiskObject * vnode, VnodeId vnodeNumber,
	   Inode ino)
#endif
{
#if defined(AFS_NAMEI_ENV)
    IHandle_t *ihtmpp;
#if !defined(AFS_NT40_ENV)
    namei_t filename;
#else
    char filename[MAX_PATH];
#endif
#endif
    afs_fsize_t fileLength;

    VNDISK_GET_LEN(fileLength, vnode);
    Vvnodesize += fileLength;
    Vvnodesize_k += fileLength / 1024;
    if (dsizeOnly)
	return;
    if (orphaned && (fileLength == 0 || vnode->parent || !offset))
	return;
    printf
	("%10d Vnode %u.%u.%u cloned: %u, length: %llu linkCount: %d parent: %u",
	 offset, vnodeNumber, vnode->uniquifier, vnode->dataVersion,
	 vnode->cloned, (afs_uintmax_t) fileLength, vnode->linkCount,
	 vnode->parent);
    if (DumpInodeNumber)
	printf(" inode: %s", PrintInode(NULL, ino));
    if (DumpDate)
	printf(" ServerModTime: %s", date(vnode->serverModifyTime));
#if defined(AFS_NAMEI_ENV)
    if (PrintFileNames) {
	IH_INIT(ihtmpp, V_device(vp), V_parentId(vp), ino);
#if !defined(AFS_NT40_ENV)
	namei_HandleToName(&filename, ihtmpp);
	printf(" UFS-Filename: %s", filename.n_path);
#else
	nt_HandleToName(filename, ihtmpp);
	printf(" NTFS-Filename: %s", filename);
#endif
    }
#endif
    printf("\n");
}
