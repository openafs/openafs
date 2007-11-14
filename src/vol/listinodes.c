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
	Module:		listinodes.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#define ITC			/* Required by inode.h */

#include <afsconfig.h>
#include <afs/param.h>

#include <string.h>

RCSID
    ("$Header$");

#ifndef AFS_NAMEI_ENV
#if defined(AFS_LINUX20_ENV) || defined(AFS_SUN4_ENV)
/* ListViceInodes
 *
 * Return codes:
 * 0 - success
 * -1 - Unable to read the inodes.
 * -2 - Unable to completely write temp file. Produces warning message in log.
 */
int
ListViceInodes(char *devname, char *mountedOn, char *resultFile,
	       int (*judgeInode) (), int judgeParam, int *forcep, int forceR,
	       char *wpath, void *rock)
{
    Log("ListViceInodes not implemented for this platform!\n");
    return -1;
}
#else
#include <ctype.h>
#include <sys/param.h>
#if defined(AFS_SGI_ENV)
#else
#ifdef	AFS_OSF_ENV
#include <ufs/fs.h>
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#define VFS
#ifdef	  AFS_SUN5_ENV
#include <sys/fs/ufs_fs.h>
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#define itod ino_to_fsba
#else
#include <ufs/fs.h>
#endif
#endif
#else /* AFS_VFSINCL_ENV */
#ifdef	AFS_AIX_ENV
#include <sys/filsys.h>
#else
#include <sys/fs.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */
#include <sys/time.h>
#ifdef AFS_VFSINCL_ENV
#include <sys/vnode.h>
#ifdef	  AFS_SUN5_ENV
#include <sys/fs/ufs_inode.h>
#else
#if !defined(AFS_DARWIN_ENV)
#include <ufs/inode.h>
#endif
#endif
#else /* AFS_VFSINCL_ENV */
#ifdef	AFS_OSF_ENV
#include <ufs/inode.h>
#else /* AFS_OSF_ENV */
#include <sys/inode.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_SGI_ENV */
#include <afs/osi_inode.h>
#include <sys/file.h>
#include <stdio.h>
#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/afssyscalls.h>
#include "viceinode.h"
#include <sys/stat.h>
#if defined (AFS_AIX_ENV) || defined (AFS_HPUX_ENV)
#include <sys/ino.h>
#endif
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#if defined(AFS_HPUX101_ENV)
#include <unistd.h>
#endif
#include "lock.h"
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "volinodes.h"
#include "partition.h"
#include "fssync.h"

/*@+fcnmacros +macrofcndecl@*/
#ifdef O_LARGEFILE
#ifdef S_SPLINT_S
extern off64_t afs_lseek(int FD, off64_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)   lseek64(FD, (off64_t) (O), F)
#define afs_stat      stat64
#define afs_fstat     fstat64
#define afs_open      open64
#define afs_fopen     fopen64
#else /* !O_LARGEFILE */
#ifdef S_SPLINT_S
extern off_t afs_lseek(int FD, off_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)   lseek(FD, (off_t) (O), F)
#define afs_stat      stat
#define afs_fstat     fstat
#define afs_open      open
#define afs_fopen     fopen
#endif /* !O_LARGEFILE */
/*@=fcnmacros =macrofcndecl@*/

/* Notice:  parts of this module have been cribbed from vfsck.c */

#define	ROOTINODE	2
static char *partition;
int Testing=0;
int pfd;

#ifdef	AFS_AIX32_ENV
#include <jfs/filsys.h>

#ifndef FSBSIZE
#define FSBSIZE         (4096)	/* filesystem block size        */
#define FSBSHIFT        (12)	/* log2(FSBSIZE)                */
#define FSBMASK         (FSBSIZE - 1)	/* FSBSIZE mask                 */

#define MIN_FSIZE	DISKMAP_B	/* minimum fs size (FSblocks)   */
#define LAST_RSVD_I	15	/* last reserved inode          */
#endif

#ifndef INOPB
/*
 * This will hopefully eventually make it into the system include files
 */
#define INOPB		(FSBSIZE / sizeof (struct dinode))
#endif

#ifdef AFS_AIX41_ENV
int fragsize;
int iagsize;
int ag512;
int agblocks;
#endif /* AFS_AIX41_ENV */

/*
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 XX This was lifted from some `com/cmd/fs/fshlpr_aix3/Fs.h', which indicated X
 XX a longing to see it make it into a readily accessible include file. XXXXXX
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 *
 * itoo - inode number to offset within disk block
 */
#undef itoo
#define itoo(x)         (int) ((unsigned)(x) % INOPB)

int Bsize = FSBSIZE;		/* block size for this system                   */
daddr_t fmax;			/* total number of blocks n file system         */
ino_t imax, inum;		/* total number of I-nodes in file system       */

static struct superblock fs;
struct dinode *ginode();


int
ListViceInodes(char *devname, char *mountedOn, char *resultFile,
	       int (*judgeInode) (), int judgeParam, int *forcep, int forceR,
	       char *wpath, void *rock)
{
    FILE *inodeFile = NULL;
    char dev[50], rdev[51];
    struct stat status;
    struct dinode *p;
    struct ViceInodeInfo info;
    struct stat root_inode;
    int ninodes = 0, err = 0;

    pfd = -1;			/* initialize so we don't close on error output below. */
    *forcep = 0;
    sync();
    sleep(1);			/* simulate operator    */
    sync();
    sleep(1);
    sync();
    sleep(1);

    partition = mountedOn;
    sprintf(dev, "/dev/%s", devname);
    sprintf(rdev, "/dev/r%s", devname);

    if (stat(mountedOn, &root_inode) < 0) {
	Log("cannot stat: %s\n", mountedOn);
	return -1;
    }

    if (root_inode.st_ino != ROOTDIR_I) {
	Log("%s is not root of a filesystem\n", mountedOn);
	return -1;
    }


    /*
     * done with the superblock, now try to read the raw device.
     */
    if (ReadSuper(&fs, dev) < 0)
	return -1;

    switch (fs.s_fmod) {
    default:
    case FM_CLEAN:		/* clean and unmounted                  */
	Log("Most peculiar - Super blk in FM_CLEAN state!\n");
	goto out;
    case FM_MOUNT:		/* mounted cleanly                      */
	break;

    case FM_MDIRTY:		/* dirty when mounted or commit fail    */
    case FM_LOGREDO:		/* log redo attempted but failed        */
	Log("File system %s is in a bad state.\n", rdev);
	Log("Call your IBM representative.\n");
	return -1;
    }
#ifdef AFS_AIX42_ENV
    if (IsBigFilesFileSystem(&fs, (char *)0)) {
	Log("%s is a big files filesystem, can't salvage.\n", mountedOn);
	return -1;
    }
#else
    if (strncmp(fs.s_magic, fsv3magic, strlen(fsv3magic)) != 0) {
#ifdef	AFS_AIX41_ENV
	if ((strncmp(fs.s_magic, fsv3pmagic, strlen(fsv3pmagic)) != 0)
	    || (fs.s_version != fsv3pvers)) {
	    Log("Super block doesn't have the problem magic (%s vs v3magic %s v3pmagic %s)\n", fs.s_magic, fsv3magic, fsv3pmagic);
	    return -1;
	}
#else
	Log("Super block doesn't have the problem magic (%s vs v3magic %s)\n",
	    fs.s_magic, fsv3magic);
	return -1;
#endif
    }
#endif

#ifdef AFS_AIX41_ENV
    fragsize = (fs.s_fragsize) ? fs.s_fragsize : FSBSIZE;
    iagsize = (fs.s_iagsize) ? fs.s_iagsize : fs.s_agsize;
    ag512 = fragsize * fs.s_agsize / 512;
    agblocks = fragsize * fs.s_agsize >> BSHIFT;
#endif /* AFS_AIX41_ENV */

    fmax = fs.s_fsize / (FSBSIZE / 512);	/* first invalid blk num */

    pfd = afs_open(rdev, O_RDONLY);
    if (pfd < 0) {
	Log("Unable to open `%s' inode for reading\n", rdev);
	return -1;
    }

    if (resultFile) {
	inodeFile = fopen(resultFile, "w");
	if (inodeFile == NULL) {
	    Log("Unable to create inode description file %s\n", resultFile);
	    goto out;
	}
    }

    /*
     * calculate the maximum number of inodes possible
     */
#ifdef AFS_AIX41_ENV
    imax = iagsize * (fs.s_fsize / ag512) - 1;
#else /* AFS_AIX41_ENV */
    imax =
	((fmax / fs.s_agsize +
	  ((fmax % fs.s_agsize) >= fs.s_agsize / INOPB ? 1 : 0))
	 * fs.s_agsize) - 1;
#endif /* AFS_AIX41_ENV */

    /*
     * check for "FORCESALVAGE" equivalent:
     *      LAST_RSVD_I is a vice inode, with dead beef, and
     *      di_nlink == 2 to indicate the FORCE.
     */
    assert(p = ginode(LAST_RSVD_I));

    if (p->di_vicemagic == VICEMAGIC && p->di_vicep1 == 0xdeadbeef
	&& p->di_nlink == 2) {
	*forcep = 1;
	idec(root_inode.st_dev, LAST_RSVD_I, 0xdeadbeef);
    }

    for (inum = LAST_RSVD_I + 1; inum <= imax; ++inum) {
	if ((p = ginode(inum)) == NULL || p->di_vicemagic != VICEMAGIC
	    || (p->di_mode & IFMT) != IFREG)
	    continue;

	info.inodeNumber = inum;
	info.byteCount = p->di_size;
	info.linkCount = p->di_nlink;
	info.u.param[0] = p->di_vicep1;
	info.u.param[1] = p->di_vicep2;
	info.u.param[2] = p->di_vicep3;
	info.u.param[3] = p->di_vicep4;

	if (judgeInode && (*judgeInode) (&info, judgeParam, rock) == 0)
	    continue;

	if (inodeFile) {
	    if (fwrite(&info, sizeof info, 1, inodeFile) != 1) {
		Log("Error writing inode file for partition %s\n", partition);
		goto out;
	    }
	}
	++ninodes;
    }

    if (inodeFile) {
	if (fflush(inodeFile) == EOF) {
	    Log("Unable to successfully flush inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (fsync(fileno(inodeFile)) == -1) {
	    Log("Unable to successfully fsync inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (fclose(inodeFile) == EOF) {
	    Log("Unable to successfully close inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}

	/*
	 * Paranoia:  check that the file is really the right size
	 */
	if (stat(resultFile, &status) == -1) {
	    Log("Unable to successfully stat inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (status.st_size != ninodes * sizeof(struct ViceInodeInfo)) {
	    Log("Wrong size (%d instead of %d) in inode file for %s\n",
		status.st_size, ninodes * sizeof(struct ViceInodeInfo),
		partition);
	    err = -2;
	    goto out1;
	}
    }
    close(pfd);
    return 0;

  out:
    err = -1;
  out1:
    if (pfd >= 0)
	close(pfd);
    if (inodeFile)
	fclose(inodeFile);

    return err;
}

/* Read in the superblock for devName */
int
ReadSuper(struct superblock *fs, char *devName)
{
    int pfd;

    pfd = afs_open(devName, O_RDONLY);
    if (pfd < 0) {
	Log("Unable to open inode on %s for reading superblock.\n", devName);
	return -1;
    }

    if (bread(pfd, fs, SUPER_B, sizeof(struct superblock)) < 0) {
	Log("Unable to read superblock on %s.\n", devName);
	return -1;
    }
    close(pfd);
    return (0);
}

#ifdef AFS_AIX42_ENV
/* IsBigFilesFileSystem returns 1 if it's a big files filesystem, 0 otherwise. */
int
IsBigFilesFileSystem(struct superblock *sb)
{
    if ((strncmp(sb->s_magic, fsv3pmagic, 4) == 0)
	&& (sb->s_version == fsbigfile)
	&& (sb->s_bigexp))
	return 1;
    else
	return 0;
}
#endif

struct dinode *
ginode(inum)
{
    int ag;
    daddr_t pblk;
    struct dinode *dp;
    static char buf[FSBSIZE];
    static daddr_t last_blk = -1;

#ifdef AFS_AIX41_ENV
    ag = inum / iagsize;
    pblk =
	(ag ==
	 0) ? INODES_B + inum / INOPB : ag * agblocks + (inum -
							 ag * iagsize) /
	INOPB;
#else /* AFS_AIX41_ENV */
    ag = inum / fs.s_agsize;
    pblk =
	(ag ==
	 0) ? INODES_B + inum / INOPB : ag * fs.s_agsize + (inum -
							    ag *
							    fs.s_agsize) /
	INOPB;
#endif /* AFS_AIX41_ENV */

    if (last_blk != pblk) {
	if (bread(pfd, buf, pblk, sizeof(buf)) < 0) {
	    last_blk = -1;
	    return 0;
	}
	last_blk = pblk;
    }

    dp = (struct dinode *)buf;
    dp += itoo(inum);
    return (dp);
}

#else /* !AFS_AIX31_ENV       */

#if defined(AFS_SGI_ENV)

/* libefs.h includes <assert.h>, which we don't want */
#define	__ASSERT_H__

#ifdef AFS_SGI_EFS_IOPS_ENV
#include "sgiefs/libefs.h"
extern int Log();

/* afs_efs_figet() replaces the SGI library routine because we are malloc'ing
 * memory for all the inodes on all the cylinder groups without releasing
 * it when we're done. Using afs_efs_figet ensures more efficient use of
 * memory.
 */
struct efs_dinode *
afs_efs_figet(EFS_MOUNT * mp, struct efs_dinode *dinodeBuf, int *last_cgno,
	      ino_t inum)
{
    int cgno = EFS_ITOCG(mp->m_fs, inum);


    if (cgno != *last_cgno) {
	if (efs_readb
	    (mp->m_fd, (char *)dinodeBuf, EFS_CGIMIN(mp->m_fs, cgno),
	     mp->m_fs->fs_cgisize) != mp->m_fs->fs_cgisize) {
	    Log("Unable to read inodes for cylinder group %d.\n", cgno);
	    return NULL;
	}
	*last_cgno = cgno;
    }

    return dinodeBuf + (inum % (mp->m_fs->fs_cgisize * EFS_INOPBB));
}


int
efs_ListViceInodes(char *devname, char *mountedOn, char *resultFile,
		   int (*judgeInode) (), int judgeParam, int *forcep,
		   int forceR, char *wpath, void *rock)
{
    FILE *inodeFile = NULL;
    char dev[50], rdev[51];
    struct stat status;
    struct efs_dinode *p;
    struct ViceInodeInfo info;
    int ninodes = 0, err = 0;
    struct efs_dinode *dinodeBuf = NULL;
    int last_cgno;
    EFS_MOUNT *mp;
    ino_t imax, inum;		/* total number of I-nodes in file system */

    *forcep = 0;

    partition = mountedOn;
    sprintf(dev, "/dev/dsk/%s", devname);
    sprintf(rdev, "/dev/rdsk/%s", devname);


    /*
     * open raw device
     */
    efs_init(Log);
    if ((stat(rdev, &status) == -1)
	|| ((mp = efs_mount(rdev, O_RDONLY)) == NULL)) {
	sprintf(rdev, "/dev/r%s", devname);
	mp = efs_mount(rdev, O_RDONLY);
    }
    if (mp == NULL) {
	Log("Unable to open `%s' inode for reading\n", rdev);
	return -1;
    }

    if (resultFile) {
	inodeFile = fopen(resultFile, "w");
	if (inodeFile == NULL) {
	    Log("Unable to create inode description file %s\n", resultFile);
	    goto out;
	}
    }

    /* Allocate space for one cylinder group's worth of inodes. */
    dinodeBuf = (struct efs_dinode *)malloc(mp->m_fs->fs_cgisize * BBSIZE);
    if (!dinodeBuf) {
	Log("Unable to malloc %lu bytes for inode buffer.\n",
	    mp->m_fs->fs_cgisize * BBSIZE);
	goto out;
    }

    /*
     * calculate the maximum number of inodes possible
     */
    imax = mp->m_fs->fs_ncg * mp->m_fs->fs_ipcg;

    last_cgno = -1;
    for (inum = 2; inum < imax; ++inum) {
	p = afs_efs_figet(mp, dinodeBuf, &last_cgno, inum);
	if (!p) {
	    Log("Unable to read all inodes from partition.\n");
	    goto out;
	}
	if (!IS_DVICEMAGIC(p) || !((p->di_mode & IFMT) == IFREG)) {
	    continue;
	}
#if defined(AFS_SGI_EXMAG)
	/* volume ID */
	info.u.param[0] =
	    dmag(p, 0) << 24 | dmag(p, 1) << 16 | dmag(p, 2) << 8 | dmag(p,
									 3) <<
	    0;
	if ((p)->di_version == EFS_IVER_AFSSPEC) {
	    info.u.param[1] = INODESPECIAL;
	    /* type */
	    info.u.param[2] = dmag(p, 8);
	    /* parentId */
	    info.u.param[3] =
		dmag(p, 4) << 24 | dmag(p, 5) << 16 | dmag(p,
							   6) << 8 | dmag(p,
									  7)
		<< 0;
	} else {
	    /* vnode number */
	    info.u.param[1] =
		dmag(p, 4) << 16 | dmag(p, 5) << 8 | dmag(p, 6) << 0;
	    /* disk uniqifier */
	    info.u.param[2] =
		dmag(p, 7) << 16 | dmag(p, 8) << 8 | dmag(p, 9) << 0;
	    /* data version */
	    info.u.param[3] =
		dmag(p, 10) << 16 | dmag(p, 11) << 8 | (p)->di_spare;
	}
#else
	BOMB ! !
#endif
	    info.inodeNumber = inum;
	info.byteCount = p->di_size;
	info.linkCount = p->di_nlink;
#ifdef notdef
	Log("Ino=%d, bytes=%d, linkCnt=%d, [%x,%x,%x,%x]\n", inum, p->di_size,
	    p->di_nlink, info.u.param[0], info.u.param[1], info.u.param[2],
	    info.u.param[3]);
#endif
	if (judgeInode && (*judgeInode) (&info, judgeParam, rock) == 0)
	    continue;

	if (inodeFile) {
	    if (fwrite(&info, sizeof info, 1, inodeFile) != 1) {
		Log("Error writing inode file for partition %s\n", partition);
		goto out;
	    }
	}
	++ninodes;
    }

    if (inodeFile) {
	if (fflush(inodeFile) == EOF) {
	    Log("Unable to successfully flush inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (fsync(fileno(inodeFile)) == -1) {
	    Log("Unable to successfully fsync inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (fclose(inodeFile) == EOF) {
	    Log("Unable to successfully close inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}

	/*
	 * Paranoia:  check that the file is really the right size
	 */
	if (stat(resultFile, &status) == -1) {
	    Log("Unable to successfully stat inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (status.st_size != ninodes * sizeof(struct ViceInodeInfo)) {
	    Log("Wrong size (%d instead of %d) in inode file for %s\n",
		status.st_size, ninodes * sizeof(struct ViceInodeInfo),
		partition);
	    err = -2;
	    goto out1;
	}
    }
    efs_umount(mp);
    if (dinodeBuf) {
	free(dinodeBuf);
    }
    return 0;

  out:
    err = -1;
  out1:
    if (dinodeBuf) {
	free(dinodeBuf);
    }
    efs_umount(mp);
    if (inodeFile)
	fclose(inodeFile);

    return err;
}
#endif /* AFS_SGI_EFS_IOPS_ENV */

#ifdef AFS_SGI_XFS_IOPS_ENV
#include <dirent.h>
#include <afs/xfsattrs.h>
/* xfs_ListViceInodes
 *
 * xfs_ListViceInodes verifies and correct the XFS namespace as it collects
 * the inode information. The name is required for the idec operation to work. 
 * Steps 2 and 3 below are for the AFS_XFS_NAME_VERS == 1. If the name space
 * changes, the algorithm will need to change. 
 * 1) If the parent inode number does not match the directory's inod number,
 *    change it in the attribute.
 * 2) If the unqifier in the attribute does not match the name, rename the
 *    file. This is done by doing an exclusive open, incrementing the tag
 *    number until a file can be created. If the tag changes, then the
 *    attribute will need updating.
 * 3) If the tag in the attribute does not match the name, change the
 *    attribute.
 * 4) Verify uid = RW volume id and gid = XFS_VICEMAGIC.
 *
 */

/* xfs_VerifyInode
 * 
 * Does the verifications listed above.
 * We can't change the names until the readdir is complete, so we set the
 * rename flag if the file needs renaming.
 */
int
xfs_VerifyInode(char *dir, uint64_t pino, char *name, i_list_inode_t * info,
		int *rename)
{
    char path[1024];
    int vno;
    int update_pino = 0;
    int update_tag = 0;
    int update_chown = 0;
    int retCode = 0;
    char tmpName[32];
    b64_string_t stmp;
    int tag;

    *rename = 0;
    (void)sprintf(path, "%s/%s", dir, name);
    /* Verify uid and gid fields */
    if (info->ili_magic != XFS_VICEMAGIC) {
	Log("%s  magic for %s/%s (inode %s) from %d to %d\n",
	    Testing ? "Would have changed" : "Changing", dir, name,
	    PrintInode(NULL, info->ili_info.inodeNumber), info->ili_magic,
	    XFS_VICEMAGIC);
	if (!Testing)
	    update_chown = 1;
    }

    vno = info->ili_info.param[0];
    if (info->ili_vno != AFS_XFS_VNO_CLIP(vno)) {
	Log("%s volume id for %s/%s (inode %s) from %d to %d\n",
	    Testing ? "Would have changed" : "Changing", dir, name,
	    PrintInode(NULL, info->ili_info.inodeNumber), info->ili_vno,
	    AFS_XFS_VNO_CLIP(vno));
	if (!Testing)
	    update_chown = 1;
    }

    if (update_chown) {
	if (chown(path, AFS_XFS_VNO_CLIP(vno), XFS_VICEMAGIC) < 0) {
	    Log("Can't chown %s to uid=%d, gid=0x%x\n", path,
		AFS_XFS_VNO_CLIP(vno), XFS_VICEMAGIC);
	    retCode = -1;
	}
    }

    /* Check Parent inode number. */
    if (info->ili_pino != pino) {
	afs_ino_str_t sino, sipino, spino;
	(void)PrintInode(sino, info->ili_info.inodeNumber);
	(void)PrintInode(sipino, info->ili_pino);
	(void)PrintInode(spino, pino);
	Log("%s parent ino for %s (inode %s) from %s to %s.\n",
	    Testing ? "Would have changed" : "Changing", path, sino, sipino,
	    spino);
	if (!Testing)
	    update_pino = 1;
    }

    /* Verify the file name. */
    (void)strcpy(tmpName, ".");
    (void)strcat(tmpName, int_to_base64(stmp, info->ili_info.param[2]));
    if (strncmp(name, tmpName, strlen(tmpName))) {
	Log("%s name %s (inode %s) in directory %s, unique=%d, tag=%d\n",
	    Testing ? "Would have returned bad" : "Bad", name,
	    PrintInode(NULL, info->ili_info.inodeNumber), dir,
	    info->ili_info.param[2], info->ili_tag);
	if (!Testing)
	    *rename = 1;
    }

    if (!*rename) {
	/* update the tag? */
	(void)strcat(tmpName, ".");
	(void)strcat(tmpName, int_to_base64(stmp, info->ili_tag));
	if (strcmp(name, tmpName)) {
	    char *p;
	    (void)strcpy(tmpName, name);
	    p = strchr(tmpName + 1, '.');
	    if (!p) {
		Log("No tag found on name %s (inode %s)in directory, %s.\n",
		    name, PrintInode(NULL, info->ili_info.inodeNumber), dir,
		    Testing ? "would have renamed" : "will rename");
		if (!Testing)
		    *rename = 1;
	    } else {
		tag = base64_to_int(p + 1);
		Log("%s the tag for %s (inode %s) from %d to %d.\n",
		    Testing ? "Would have changed" : "Will change", path,
		    PrintInode(NULL, info->ili_info.inodeNumber), dir, tag,
		    info->ili_tag);
		if (!Testing)
		    update_tag = 1;
	    }
	}
    }

    if (update_pino || update_tag) {
	afs_xfs_attr_t attrs;
	int length;

	length = SIZEOF_XFS_ATTR_T;
	if (attr_get(path, AFS_XFS_ATTR, (char *)&attrs, &length, ATTR_ROOT) <
	    0) {
	    Log("Can't get AFS attribute for %s\n", path);
	    return -1;
	}
	if (update_pino)
	    attrs.at_pino = pino;
	if (update_tag)
	    attrs.at_tag = tag;
	if (attr_set
	    (path, AFS_XFS_ATTR, (char *)&attrs, length,
	     ATTR_ROOT | ATTR_REPLACE) < 0) {
	    Log("Can't set AFS attribute into %s\n", path);
	    retCode = -1;
	}
    }

    return retCode;
}

typedef struct {
    int uniq;
    char name[28];
} xfs_Rename_t;

int
xfs_RenameFiles(char *dir, xfs_Rename_t * renames, int n_renames)
{
    int i, j;
    char opath[128], nbase[128], npath[128];
    afs_xfs_attr_t attrs;
    int length = SIZEOF_XFS_ATTR_T;
    b64_string_t stmp;
    int tag;
    int fd;

    for (i = 0; i < n_renames; i++) {
	(void)sprintf(opath, "%s/%s", dir, renames[i].name);
	(void)sprintf(nbase, "%s/.%s", dir,
		      int_to_base64(stmp, renames[i].uniq));
	for (tag = 2, j = 0; j < 64; tag++, j++) {
	    (void)sprintf(npath, "%s.%s", nbase, int_to_base64(stmp, tag));
	    fd = afs_open(npath, O_CREAT | O_EXCL | O_RDWR, 0);
	    if (fd > 0) {
		close(fd);
		break;
	    }
	}
	if (j != 64) {
	    Log("Can't find a new name for %s\n", opath);
	    return -1;
	}
	if (rename(opath, npath) < 0) {
	    Log("Can't rename %s to %s\n", opath, npath);
	    return -1;
	}
	Log("Renamed %s to %s\n", opath, npath);
	return 0;
    }
}


int
xfs_ListViceInodes(char *devname, char *mountedOn, char *resultFile,
		   int (*judgeInode) (), int judgeParam, int *forcep,
		   int forceR, char *wpath, void *rock)
{
    FILE *inodeFile = NULL;
    i_list_inode_t info;
    int info_size = sizeof(i_list_inode_t);
    int fd;
    DIR *top_dirp;
    dirent64_t *top_direntp;
    DIR *vol_dirp;
    dirent64_t *vol_direntp;
    struct stat64 sdirbuf;
    struct stat64 sfilebuf;
    afs_xfs_attr_t attrs;
    afs_xfs_dattr_t dattrs;
    int length;
    char vol_dirname[1024];
    int ninodes = 0;
    int code = 0;
    xfs_Rename_t *renames = (xfs_Rename_t *) 0;
    int rename;
#define N_RENAME_STEP 64
    int n_renames = 0;
    int n_avail = 0;
    uint64_t pino;
    struct stat status;
    int errors = 0;

    *forcep = 0;

    if (stat64(mountedOn, &sdirbuf) < 0) {
	perror("xfs_ListViceInodes: stat64");
	return -1;
    }

    if (resultFile) {
	inodeFile = fopen(resultFile, "w");
	if (inodeFile == NULL) {
	    Log("Unable to create inode description file %s\n", resultFile);
	    return -1;
	}
    }

    if ((top_dirp = opendir(mountedOn)) == NULL) {
	Log("Can't open directory %s to read inodes.\n", mountedOn);
	return -1;
    }

    while (top_direntp = readdir64(top_dirp)) {
	/* Only descend directories with the AFSDIR attribute set.
	 * Could also verify the contents of the atribute, but currently
	 * they are not used.
	 * Performance could be improved for single volume salvages by
	 * only going through the directory containing the volume's inodes.
	 * But I'm being complete as a first pass.
	 */
	(void)sprintf(vol_dirname, "%s/%s", mountedOn, top_direntp->d_name);
	length = SIZEOF_XFS_DATTR_T;
	if (attr_get
	    (vol_dirname, AFS_XFS_DATTR, (char *)&dattrs, &length, ATTR_ROOT))
	    continue;

	if ((vol_dirp = opendir(vol_dirname)) == NULL) {
	    if (errno == ENOTDIR)
		continue;
	    Log("Can't open directory %s to read inodes.\n", vol_dirname);
	    goto err1_exit;
	}

	pino = top_direntp->d_ino;
	n_renames = 0;
	while (vol_direntp = readdir64(vol_dirp)) {
	    if (vol_direntp->d_name[1] == '\0'
		|| vol_direntp->d_name[1] == '.')
		continue;

	    info.ili_version = AFS_XFS_ILI_VERSION;
	    info_size = sizeof(i_list_inode_t);
	    code =
		ilistinode64(sdirbuf.st_dev, vol_direntp->d_ino, &info,
			     &info_size);
	    if (code) {
		/* Where possible, give more explicit messages. */
		switch (errno) {
		case ENXIO:
		case ENOSYS:
		    Log("%s (device id %d) is not on an XFS filesystem.\n",
			vol_dirname, sdirbuf.st_dev);
		    goto err1_exit;
		    break;
		case EINVAL:
		case E2BIG:
		    if (info_size != sizeof(i_list_inode_t)
			|| info.ili_version != AFS_XFS_ILI_VERSION) {
			Log("Version skew between kernel and salvager.\n");
			goto err1_exit;
		    }
		    break;
		}
		/* Continue, so we collect all the errors in the first pass. */
		Log("Error listing inode named %s/%s: %s\n", vol_dirname,
		    vol_direntp->d_name, strerror(errno));
		errors++;
		continue;
	    }

	    if (info.ili_attr_version != AFS_XFS_ATTR_VERS) {
		Log("Unrecognized XFS attribute version %d in %s/%s. Upgrade salvager\n", info.ili_attr_version, vol_dirname, vol_direntp->d_name);
		goto err1_exit;
	    }

	    if (judgeInode && (*judgeInode) (&info.ili_info, judgeParam, rock) == 0)
		continue;

	    rename = 0;
	    if (xfs_VerifyInode
		(vol_dirname, pino, vol_direntp->d_name, &info,
		 &rename) < 0) {
		errors++;
	    }

	    if (rename) {
		/* Add this name to the list of items to rename. */
		if (n_renames >= n_avail) {
		    n_avail += N_RENAME_STEP;
		    if (n_avail == N_RENAME_STEP)
			renames = (xfs_Rename_t *)
			    malloc(n_avail * sizeof(xfs_Rename_t));
		    else
			renames = (xfs_Rename_t *)
			    realloc((char *)renames,
				    n_avail * sizeof(xfs_Rename_t));
		    if (!renames) {
			Log("Can't %salloc %lu bytes for rename list.\n",
			    (n_avail == N_RENAME_STEP) ? "m" : "re",
			    n_avail * sizeof(xfs_Rename_t));
			goto err1_exit;
		    }
		}
		(void)strcpy(renames[n_renames].name, vol_direntp->d_name);
		renames[n_renames].uniq = info.ili_info.param[2];
		n_renames++;
	    }

	    if (inodeFile) {
		if (fwrite(&info.ili_info, sizeof(vice_inode_info_t), 1, inodeFile) != 1) {
		    Log("Error writing inode file for partition %s\n", mountedOn);
		    goto err1_exit;
		}
	    }
	    ninodes++;

	}			/* end while vol_direntp */

	closedir(vol_dirp);
	vol_dirp = (DIR *) 0;
	if (n_renames) {
	    Log("Renaming files.\n");
	    if (xfs_RenameFiles(vol_dirname, renames, n_renames) < 0) {
		goto err1_exit;
	    }
	}
    }

    closedir(top_dirp);
    if (renames)
	free((char *)renames);

    if (inodeFile) {
	if (fflush(inodeFile) == EOF) {
	    ("Unable to successfully flush inode file for %s\n", mountedOn);
	    fclose(inodeFile);
	    return errors ? -1 : -2;
	}
	if (fsync(fileno(inodeFile)) == -1) {
	    Log("Unable to successfully fsync inode file for %s\n", mountedOn);
	    fclose(inodeFile);
	    return errors ? -1 : -2;
	}
	if (fclose(inodeFile) == EOF) {
	    Log("Unable to successfully close inode file for %s\n", mountedOn);
	    return errors ? -1 : -2;
	}
	/*
	 * Paranoia:  check that the file is really the right size
	 */
	if (stat(resultFile, &status) == -1) {
	    Log("Unable to successfully stat inode file for %s\n", partition);
	    return errors ? -1 : -2;
	}
	if (status.st_size != ninodes * sizeof(struct ViceInodeInfo)) {
	    Log("Wrong size (%d instead of %d) in inode file for %s\n",
		status.st_size, ninodes * sizeof(struct ViceInodeInfo),
		partition);
	    return errors ? -1 : -2;
	}
    }

    if (errors) {
	Log("Errors encontered listing inodes, not salvaging partition.\n");
	return -1;
    }

    return 0;

  err1_exit:
    if (vol_dirp)
	closedir(vol_dirp);
    if (top_dirp)
	closedir(top_dirp);
    if (renames)
	free((char *)renames);
    if (inodeFile)
	fclose(inodeFile);
    return -1;
}

#endif

int
ListViceInodes(char *devname, char *mountedOn, char *resultFile,
	       int (*judgeInode) (), int judgeParam, int *forcep, int forceR,
	       char *wpath, void *rock)
{
    FILE *inodeFile = NULL;
    char dev[50], rdev[51];
    struct stat status;
    struct efs_dinode *p;
    struct ViceInodeInfo info;
    struct stat root_inode;
    int ninodes = 0, err = 0;
    struct efs_dinode *dinodeBuf = NULL;
    int last_cgno;
#ifdef AFS_SGI_EFS_IOPS_ENV
    EFS_MOUNT *mp;
#endif
    ino_t imax, inum;		/* total number of I-nodes in file system */

    *forcep = 0;
    sync();
    sleep(1);			/* simulate operator    */
    sync();
    sleep(1);
    sync();
    sleep(1);

    if (stat(mountedOn, &root_inode) < 0) {
	Log("cannot stat: %s\n", mountedOn);
	return -1;
    }
#ifdef AFS_SGI_XFS_IOPS_ENV
    if (!strcmp("xfs", root_inode.st_fstype)) {
	return xfs_ListViceInodes(devname, mountedOn, resultFile, judgeInode,
				  judgeParam, forcep, forceR, wpath, rock);
    } else
#endif
#ifdef AFS_SGI_EFS_IOPS_ENV
    if (root_inode.st_ino == EFS_ROOTINO) {
	return efs_ListViceInodes(devname, mountedOn, resultFile, judgeInode,
				  judgeParam, forcep, forceR, wpath, rock);
    } else
#endif
    {
	Log("%s is not root of a filesystem\n", mountedOn);
	return -1;
    }
}

#else /* AFS_SGI_ENV */

#ifdef AFS_HPUX_ENV
#define SPERB	(MAXBSIZE / sizeof(short))
#define MAXNINDIR (MAXBSIZE / sizeof(daddr_t))

struct bufarea {
    struct bufarea *b_next;	/* must be first */
    daddr_t b_bno;
    int b_size;
    union {
	char b_buf[MAXBSIZE];	/* buffer space */
	short b_lnks[SPERB];	/* link counts */
	daddr_t b_indir[MAXNINDIR];	/* indirect block */
	struct fs b_fs;		/* super block */
	struct cg b_cg;		/* cylinder group */
    } b_un;
    char b_dirty;
};
typedef struct bufarea BUFAREA;

BUFAREA sblk;
#define sblock sblk.b_un.b_fs
#endif /* AFS_HPUX_ENV */

extern char *afs_rawname();
int
ListViceInodes(char *devname, char *mountedOn, char *resultFile,
	       int (*judgeInode) (), int judgeParam, int *forcep, int forceR,
	       char *wpath, void *rock)
{
    union {
#ifdef	AFS_AIX_ENV
	struct filsys fs;
	char block[BSIZE];
#else				/* !AFS_AIX_ENV */
	struct fs fs;
	char block[SBSIZE];
#endif
    } super;
    int i, c, e, bufsize, code, err = 0;
    FILE *inodeFile = NULL;
    char dev[50], rdev[100], err1[512], *ptr1;
    struct dinode *inodes = NULL, *einodes, *dptr;
    struct stat status;
    int ninodes = 0;
    struct dinode *p;
    struct ViceInodeInfo info;

    *forcep = 0;
    partition = mountedOn;
    sprintf(rdev, "%s/%s", wpath, devname);
    ptr1 = afs_rawname(rdev);
    strcpy(rdev, ptr1);

    sync();
    /* Bletch:  this is terrible;  is there a better way to do this? Does this work? vfsck doesn't even sleep!! */
#ifdef	AFS_AIX_ENV
    sleep(5);			/* Trying a smaller one for aix */
#else
    sleep(10);
#endif

    pfd = afs_open(rdev, O_RDONLY);
    if (pfd <= 0) {
	sprintf(err1, "Could not open device %s to get inode list\n", rdev);
	perror(err1);
	return -1;
    }
#ifdef	AFS_AIX_ENV
    if (bread(pfd, (char *)&super.fs, SUPERB, sizeof super.fs) == -1) {
#else
#ifdef AFS_HPUX_ENV
    if (bread(pfd, (char *)&sblock, SBLOCK, SBSIZE) == -1) {
#else
    if (bread(pfd, super.block, SBLOCK, SBSIZE) == -1) {
#endif /* AFS_HPUX_ENV */
#endif
	Log("Unable to read superblock, partition %s\n", partition);
	goto out;
    }

    if (resultFile) {
	inodeFile = fopen(resultFile, "w");
	if (inodeFile == NULL) {
	    Log("Unable to create inode description file %s\n", resultFile);
	    goto out;
	}
    }
#ifdef	AFS_AIX_ENV
    /*
     * char *FSlabel(), *fslabel=0;
     * fslabel = FSlabel(&super.fs);
     */
    if (super.fs.s_bsize == 0)
	super.fs.s_bsize = 512;
    if (super.fs.s_bsize != BSIZE) {
	Log("SuperBlk: Cluster size not %d; run vfsck\n", BSIZE);
	goto out;
    }
    fmax = super.fs.s_fsize;	/* first invalid blk num */
    imax = ((ino_t) super.fs.s_isize - (SUPERB + 1)) * INOPB;
    if (imax == 0) {
	Log("Size check: imax==0!\n");
	goto out;
    }
    if (GetAuxInodeFile(partition, &status) == 0) {
	Log("Can't access Aux inode file for partition %s, aborting\n",
	    partition);
	goto out;
    }
    for (inum = 1; inum <= imax; inum++) {
	struct dauxinode *auxp;
	if ((auxp = IsAfsInode(inum)) == NULL) {
	    /* Not an afs inode, keep going */
	    continue;
	}
	if ((p = ginode(inum)) == NULL)
	    continue;
	/* deleted/non-existent inode when di_mode == 0 */
	if (!p->di_mode)
	    continue;
	info.inodeNumber = (int)inum;
	info.byteCount = p->di_size;
	info.linkCount = p->di_nlink;
	info.u.param[0] = auxp->aux_param1;
	info.u.param[1] = auxp->aux_param2;
	info.u.param[2] = auxp->aux_param3;
	info.u.param[3] = auxp->aux_param4;
	if (judgeInode && (*judgeInode) (&info, judgeParam, rock) == 0)
	    continue;
	if (inodeFile) {
	    if (fwrite(&info, sizeof info, 1, inodeFile) != 1) {
		Log("Error writing inode file for partition %s\n", partition);
		goto out;
	    }
	}
	ninodes++;
    }
#else
    /*
     * run a few consistency checks of the superblock
     * (Cribbed from vfsck)
     */
#ifdef AFS_HPUX_ENV
#if defined(FD_FSMAGIC)
    if ((sblock.fs_magic != FS_MAGIC) && (sblock.fs_magic != FS_MAGIC_LFN)
	&& (sblock.fs_magic != FD_FSMAGIC)
#if	defined(AFS_HPUX101_ENV)
	&& (sblock.fs_magic != FD_FSMAGIC_2)
#endif
	) {
#else
    if ((sblock.fs_magic != FS_MAGIC) && (sblock.fs_magic != FS_MAGIC_LFN)) {
#endif
	Log("There's something wrong with the superblock for partition %s; bad magic (%d) run vfsck\n", partition, sblock.fs_magic);
	goto out;
    }
    if (sblock.fs_ncg < 1) {
	Log("There's something wrong with the superblock for partition %s; NCG OUT OF RANGE (%d) run vfsck\n", partition, sblock.fs_ncg);
	goto out;
    }
    if (sblock.fs_cpg < 1 || sblock.fs_cpg > MAXCPG) {
	Log("There's something wrong with the superblock for partition %s; CPG OUT OF RANGE (%d) run vfsck\n", partition, sblock.fs_cpg);
	goto out;
    }
    if (sblock.fs_ncg * sblock.fs_cpg < sblock.fs_ncyl
	|| (sblock.fs_ncg - 1) * sblock.fs_cpg >= sblock.fs_ncyl) {
	Log("There's something wrong with the superblock for partition %s; NCYL LESS THAN NCG*CPG run vfsck\n", partition);
	goto out;
    }
    if (sblock.fs_sbsize > SBSIZE) {
	Log("There's something wrong with the superblock for partition %s; bsize too large (%d vs. %d) run vfsck\n", partition, sblock.fs_sbsize, sblock.fs_bsize);
	goto out;
    }
#else
    if ((super.fs.fs_magic != FS_MAGIC)
	|| (super.fs.fs_ncg < 1)
#if	defined(AFS_SUN_ENV) || defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	|| (super.fs.fs_cpg < 1)
#else
	|| (super.fs.fs_cpg < 1 || super.fs.fs_cpg > MAXCPG)
#endif
	|| (super.fs.fs_ncg * super.fs.fs_cpg < super.fs.fs_ncyl
	    || (super.fs.fs_ncg - 1) * super.fs.fs_cpg >= super.fs.fs_ncyl)
	|| (super.fs.fs_sbsize > SBSIZE)) {
	Log("There's something wrong with the superblock for partition %s; run vfsck\n", partition);
	goto out;
    }
#endif /* AFS_HPUX_ENV */

#ifdef AFS_HPUX_ENV
    bufsize = sblock.fs_ipg * sizeof(struct dinode);
#else
    bufsize = super.fs.fs_ipg * sizeof(struct dinode);
#endif /* AFS_HPUX_ENV */
    inodes = (struct dinode *)malloc(bufsize);
    einodes = (struct dinode *)(((char *)inodes) + bufsize);
    if (inodes == NULL) {
	Log("Unable to allocate enough memory to scan inodes; help!\n");
	goto out;
    }
    Log("Scanning inodes on device %s...\n", rdev);
#ifdef AFS_HPUX_ENV
    for (c = 0; c < sblock.fs_ncg; c++) {
	i = c * sblock.fs_ipg;
	e = i + sblock.fs_ipg;
#if	defined(AFS_HPUX102_ENV)
	if (afs_lseek(pfd, dbtoo(fsbtodb(&sblock, itod(&sblock, i))), L_SET) ==
	    -1) {
#else
	if (afs_lseek(pfd, dbtob(fsbtodb(&sblock, itod(&sblock, i))), L_SET) ==
	    -1) {
#endif
#else
    for (c = 0; c < super.fs.fs_ncg; c++) {
	daddr_t dblk1;
#if defined(AFS_SUN5_ENV) || defined(AFS_DARWIN_ENV)
	daddr_t f1;
#if defined(AFS_DARWIN_ENV)
#define offset_t off_t
#define llseek lseek
#endif
	offset_t off;
#endif /* AFS_SUN5_ENV */
	i = c * super.fs.fs_ipg;
	e = i + super.fs.fs_ipg;
#ifdef	AFS_OSF_ENV
	dblk1 = fsbtodb(&super.fs, itod(&super.fs, i));
	if (afs_lseek(pfd, (off_t) ((off_t) dblk1 * DEV_BSIZE), L_SET) == -1) {
#else
#if defined(AFS_SUN5_ENV) || defined(AFS_DARWIN_ENV)
	f1 = fsbtodb(&super.fs, itod(&super.fs, i));
	off = (offset_t) f1 << DEV_BSHIFT;
	if (llseek(pfd, off, L_SET) == -1) {
#else
	if (afs_lseek(pfd, dbtob(fsbtodb(&super.fs, itod(&super.fs, i))), L_SET)
	    == -1) {
#endif /* AFS_SUN5_ENV */
#endif /* AFS_OSF_ENV */
#endif /* AFS_HPUX_ENV */
	    Log("Error reading inodes for partition %s; run vfsck\n",
		partition);
	    goto out;
	}
	while (i < e) {
	    if (!forceR) {
		if (read(pfd, inodes, bufsize) != bufsize) {
		    Log("Error reading inodes for partition %s; run vfsck\n",
			partition);
		    goto out;
		}
	    } else {
		register int bj, bk;
		dptr = inodes;
		for (bj = bk = 0; bj < bufsize; bj = bj + 512, bk++) {
		    if ((code = read(pfd, dptr, 512)) != 512) {
			Log("Error reading inode %d? for partition %s (errno = %d); run vfsck\n", bk + i, partition, errno);
			if (afs_lseek(pfd, 512, L_SET) == -1) {
			    Log("Lseek failed\n");
			    goto out;
			}
			dptr->di_mode = 0;
			dptr++;
			dptr->di_mode = 0;
			dptr++;
			dptr->di_mode = 0;
			dptr++;
			dptr->di_mode = 0;
			dptr++;
		    } else
			dptr += 4;
		}
	    }
	    for (p = inodes; p < einodes && i < e; i++, p++) {
#ifdef notdef
		Log("Ino=%d, v1=%x, v2=%x, v3=%x, mode=%x size=%d, lcnt=%d\n",
		    i, p->di_vicep1, p->di_vicep2, p->di_vicep3, p->di_mode,
		    p->di_size, p->di_nlink);
		printf
		    ("Ino=%d, v1=%x, v2=%x, v3=%x, mode=%x size=%d, lcnt=%d\n",
		     i, p->di_vicep1, p->di_vicep2, p->di_vicep3, p->di_mode,
		     p->di_size, p->di_nlink);
#endif
#ifdef AFS_OSF_ENV
#ifdef AFS_3DISPARES
		/* Check to see if this inode is a pre-"OSF1 4.0D" inode */
		if ((p->di_uid || p->di_gid)
		    && !(p->di_flags & (IC_XUID | IC_XGID))) {
		    Log("Found unconverted inode %d: Use 'fs_conv_dux40D convert' on partition %s\n", i, partition);
		    goto out;
		}
#else
		assert(0);	/* define AFS_3DISPARES in param.h */
#endif
#endif
#if	defined(AFS_SUN56_ENV)
		/* if this is a pre-sol2.6 unconverted inode, bail out */
		{
		    afs_uint32 p1, p2, p3, p4;
		    int p5;
		    quad *q;

		    q = (quad *) & (p->di_ic.ic_lsize);
		    p1 = p->di_gen;
		    p2 = p->di_ic.ic_flags;
		    p3 = q->val[0];
		    p4 = p->di_ic.ic_uid;
		    p5 = p->di_ic.ic_gid;

		    if ((p2 || p3) && !p4 && (p5 == -2)) {
			Log("Found unconverted inode %d\n", i);
			Log("You should run the AFS file conversion utility\n");
			goto out;
		    }
		}
#endif
		if (IS_DVICEMAGIC(p) && (p->di_mode & IFMT) == IFREG) {
		    afs_uint32 p2 = p->di_vicep2, p3 = DI_VICEP3(p);

		    info.u.param[0] = p->di_vicep1;
#ifdef	AFS_3DISPARES
		    if (((p2 >> 3) == INODESPECIAL) && (p2 & 0x3)) {
			info.u.param[1] = INODESPECIAL;
			info.u.param[2] = p3;
			info.u.param[3] = p2 & 0x3;
		    } else {
			info.u.param[1] = ((p2 >> 27) << 16) + (p3 & 0xffff);
			info.u.param[2] = (p2 & 0x3fffff);
			info.u.param[3] =
			    (((p2 >> 22) & 0x1f) << 16) + (p3 >> 16);
		    }
#else
		    info.u.param[1] = p->di_vicep2;
		    info.u.param[2] = DI_VICEP3(p);
		    info.u.param[3] = p->di_vicep4;
#endif
		    info.inodeNumber = i;
		    info.byteCount = p->di_size;
		    info.linkCount = p->di_nlink;
		    if (judgeInode && (*judgeInode) (&info, judgeParam, rock) == 0)
			continue;
		    if (inodeFile) {
			if (fwrite(&info, sizeof info, 1, inodeFile) != 1) {
			    Log("Error writing inode file for partition %s\n",
				partition);
			    goto out;
			}
		    }
		    ninodes++;
		}
	    }
	}
    }
    if (inodes)
	free(inodes);
#endif
    if (inodeFile) {
	if (fflush(inodeFile) == EOF) {
	    Log("Unable to successfully flush inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (fsync(fileno(inodeFile)) == -1) {
	    Log("Unable to successfully fsync inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (fclose(inodeFile) == EOF) {
	    Log("Unable to successfully close inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	
	/*
	 * Paranoia:  check that the file is really the right size
	 */
	if (stat(resultFile, &status) == -1) {
	    Log("Unable to successfully stat inode file for %s\n", partition);
	    err = -2;
	    goto out1;
	}
	if (status.st_size != ninodes * sizeof(struct ViceInodeInfo)) {
	    Log("Wrong size (%d instead of %d) in inode file for %s\n",
		status.st_size, ninodes * sizeof(struct ViceInodeInfo),
		partition);
	    err = -2;
	    goto out1;
	}
    }
    close(pfd);
    return 0;

  out:
    err = -1;
  out1:
    close(pfd);
    if (inodeFile)
	fclose(inodeFile);
    if (inodes)
	free(inodes);
    return err;
}
#endif /* !AFS_SGI_ENV */
#endif /* !AFS_AIX31_ENV       */

#ifdef AFS_DARWIN_ENV
#undef dbtob
#define dbtob(db) ((unsigned)(db) << DEV_BSHIFT)
#endif

int
bread(int fd, char *buf, daddr_t blk, afs_int32 size)
{
#ifdef	AFS_AIX_ENV
#ifdef  AFS_AIX41_ENV
    offset_t off = (offset_t) blk << FSBSHIFT;
    if (llseek(fd, off, 0) < 0) {
	Log("Unable to seek to offset %llu for block %u\n", off, blk);
	return -1;
    }
#else /* AFS_AIX41_ENV */
    if (afs_lseek(fd, blk * Bsize, 0) < 0) {
	Log("Unable to seek to offset %u for block %u\n", blk * Bsize, blk);
    }
#endif /* AFS_AIX41_ENV */
#else
    if (afs_lseek(fd, (off_t) dbtob(blk), L_SET) < 0) {
	Log("Unable to seek to offset %u for block %u\n", dbtob(blk), blk);
    }
#endif
    if (read(fd, buf, size) != size) {
	Log("Unable to read block %d, partition %s\n", blk, partition);
	return -1;
    }
    return 0;
}

#endif /* AFS_LINUX20_ENV */
static afs_int32
convertVolumeInfo(int fdr, int fdw, afs_uint32 vid)
{
    struct VolumeDiskData vd;
    char *p;

    if (read(fdr, &vd, sizeof(struct VolumeDiskData)) !=
        sizeof(struct VolumeDiskData)) {
        Log("1 convertiVolumeInfo: read failed for %lu with code %d\n", vid,
            errno);
        return -1;
    }
    vd.restoredFromId = vd.id;  /* remember the RO volume here */
    vd.cloneId = vd.id;
    vd.id = vd.parentId;
    vd.type = RWVOL;
    vd.dontSalvage = 0;
    vd.inUse = 0;
    vd.uniquifier += 5000;      /* just in case there are still file copies 
				   from the old RW volume around */

    p = strrchr(vd.name, '.');
    if (p && !strcmp(p, ".readonly")) {
        memset(p, 0, 9);
    }

    if (write(fdw, &vd, sizeof(struct VolumeDiskData)) !=
        sizeof(struct VolumeDiskData)) {
        Log("1 convertiVolumeInfo: write failed for %lu with code %d\n", vid,
            errno);
        return -1;
    }
    return 0;
}

struct specino {
    afs_int32 inodeType;
    Inode inodeNumber;
    Inode ninodeNumber;
};


int
UpdateThisVolume(struct ViceInodeInfo *inodeinfo, VolumeId singleVolumeNumber, 
		 struct specino *specinos)
{
    struct dinode *p;
    if ((inodeinfo->u.vnode.vnodeNumber == INODESPECIAL) &&
	(inodeinfo->u.vnode.volumeId == singleVolumeNumber)) {
	specinos[inodeinfo->u.special.type].inodeNumber = 
	    inodeinfo->inodeNumber;
    }
    return 0; /* We aren't using a result file, we're caching */
}

static char *
getDevName(char *pbuffer, char *wpath)
{
    char pbuf[128], *ptr;
    strcpy(pbuf, pbuffer);
    ptr = (char *)strrchr(pbuf, '/');
    if (ptr) {
        *ptr = '\0';
        strcpy(wpath, pbuf);
    } else
        return NULL;
    ptr = (char *)strrchr(pbuffer, '/');
    if (ptr) {
        strcpy(pbuffer, ptr + 1);
        return pbuffer;
    } else
        return NULL;
}

int
inode_ConvertROtoRWvolume(char *pname, afs_int32 volumeId)
{
    char dir_name[512], oldpath[512], newpath[512];
    char volname[20];
    char headername[16];
    char *name;
    int fd, err, forcep, len, j, code;
    struct dirent *dp;
    struct DiskPartition *partP;
    struct ViceInodeInfo info;
    struct VolumeDiskHeader h;
    IHandle_t *ih, *ih2;
    FdHandle_t *fdP, *fdP2;
    char wpath[100];
    char tmpDevName[100];
    char buffer[128];
    struct specino specinos[VI_LINKTABLE+1];
    Inode nearInode = 0;

    memset(&specinos, 0, sizeof(specinos));
	   
    (void)afs_snprintf(headername, sizeof headername, VFORMAT, volumeId);
    (void)afs_snprintf(oldpath, sizeof oldpath, "%s/%s", pname, headername);
    fd = open(oldpath, O_RDONLY);
    if (fd < 0) {
        Log("1 inode_ConvertROtoRWvolume: Couldn't open header for RO-volume %lu.\n", volumeId);
        return ENOENT;
    }
    if (read(fd, &h, sizeof(h)) != sizeof(h)) {
        Log("1 inode_ConvertROtoRWvolume: Couldn't read header for RO-volume %lu.\n", volumeId);
        close(fd);
        return EIO;
    }
    close(fd);
    FSYNC_VolOp(volumeId, pname, FSYNC_VOL_BREAKCBKS, 0, NULL);

    /* now do the work */
	   
    for (partP = DiskPartitionList; partP && strcmp(partP->name, pname);
         partP = partP->next);
    if (!partP) {
        Log("1 inode_ConvertROtoRWvolume: Couldn't find DiskPartition for %s\n", pname);
        return EIO;
    }

    strcpy(tmpDevName, partP->devName);
    name = getDevName(tmpDevName, wpath);

    if ((err = ListViceInodes(name, VPartitionPath(partP), 
			      NULL, UpdateThisVolume, volumeId, 
			      &forcep, 0, wpath, &specinos)) < 0)
    {
	Log("1 inode_ConvertROtoRWvolume: Couldn't get special inodes\n");
	return EIO;
    }
	   
#if defined(NEARINODE_HINT)
    nearInodeHash(volumeId, nearInode);
    nearInode %= partP->f_files;
#endif

    for (j = VI_VOLINFO; j < VI_LINKTABLE+1; j++) {
	if (specinos[j].inodeNumber > 0) {
	    specinos[j].ninodeNumber = 
		IH_CREATE(NULL, partP->device, VPartitionPath(partP),
			  nearInode, h.parent, INODESPECIAL, j, h.parent);
	    IH_INIT(ih, partP->device, volumeId, 
		    specinos[j].inodeNumber);
	    fdP = IH_OPEN(ih);
	    if (!fdP) {
		Log("1 inode_ConvertROtoRWvolume: Couldn't find special inode %d for %d\n", j, volumeId); 
		return -1;
	    }
	    
	    IH_INIT(ih2, partP->device, h.parent, specinos[j].ninodeNumber);
	    fdP2 = IH_OPEN(ih2); 
	    if (!fdP2) { 
		Log("1 inode_ConvertROtoRWvolume: Couldn't find special inode %d for %d\n", j, h.parent);  
		return -1; 
	    } 
	    
	    if (j == VI_VOLINFO)
		convertVolumeInfo(fdP->fd_fd, fdP2->fd_fd, ih2->ih_vid);
	    else {
		while (1) {
		    len = read(fdP->fd_fd, buffer, sizeof(buffer));
		    if (len < 0)
			return errno;
		    if (len == 0)
			break;
		    code = write(fdP2->fd_fd, buffer, len);
		    if (code != len)
			return -1;
		}
	    }
		
	    FDH_CLOSE(fdP);
	    FDH_CLOSE(fdP2);
	    IH_RELEASE(ih);
	    IH_RELEASE(ih2);
	}
    }
   
    h.id = h.parent;
#ifdef AFS_64BIT_IOPS_ENV
    h.volumeInfo_lo = (afs_int32)specinos[VI_VOLINFO].ninodeNumber & 0xffffffff;
    h.volumeInfo_hi = (afs_int32)(specinos[VI_VOLINFO].ninodeNumber >> 32) && 0xffffffff;
    h.smallVnodeIndex_lo = (afs_int32)specinos[VI_SMALLINDEX].ninodeNumber & 0xffffffff;
    h.smallVnodeIndex_hi = (afs_int32)(specinos[VI_SMALLINDEX].ninodeNumber >> 32) & 0xffffffff;
    h.largeVnodeIndex_lo = (afs_int32)specinos[VI_LARGEINDEX].ninodeNumber & 0xffffffff;
    h.largeVnodeIndex_hi = (afs_int32)(specinos[VI_LARGEINDEX].ninodeNumber >> 32) & 0xffffffff;
    if (specinos[VI_LINKTABLE].ninodeNumber) {
	h.linkTable_lo = (afs_int32)specinos[VI_LINKTABLE].ninodeNumber & 0xffffffff;
	h.linkTable_hi = (afs_int32)specinos[VI_LINKTABLE].ninodeNumber & 0xffffffff;
    }
#else
    h.volumeInfo_lo = specinos[VI_VOLINFO].ninodeNumber;
    h.smallVnodeIndex_lo = specinos[VI_SMALLINDEX].ninodeNumber;
    h.largeVnodeIndex_lo = specinos[VI_LARGEINDEX].ninodeNumber;
    if (specinos[VI_LINKTABLE].ninodeNumber) {
	h.linkTable_lo = specinos[VI_LINKTABLE].ninodeNumber;
    }
#endif

    (void)afs_snprintf(headername, sizeof headername, VFORMAT, h.id);
    (void)afs_snprintf(newpath, sizeof newpath, "%s/%s", pname, headername);
    fd = open(newpath, O_CREAT | O_EXCL | O_RDWR, 0644);
    if (fd < 0) {
        Log("1 inode_ConvertROtoRWvolume: Couldn't create header for RW-volume %lu.\n", h.id);
        return EIO;
    }
    if (write(fd, &h, sizeof(h)) != sizeof(h)) {
        Log("1 inode_ConvertROtoRWvolume: Couldn't write header for RW-volume %lu.\n", h.id);
        close(fd);
        return EIO;
    }
    close(fd);
    if (unlink(oldpath) < 0) {
        Log("1 inode_ConvertROtoRWvolume: Couldn't unlink RO header, error = %d\n", errno);
    }
    FSYNC_VolOp(volumeId, pname, FSYNC_VOL_DONE, 0, NULL);
    FSYNC_VolOp(h.id, pname, FSYNC_VOL_ON, 0, NULL);
    return 0;
}
#endif /* AFS_NAMEI_ENV */
