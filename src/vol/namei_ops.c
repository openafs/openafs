/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* I/O operations for the Unix open by name (namei) interface. */

#include <afsconfig.h>
#include <afs/param.h>


#ifdef AFS_NAMEI_ENV
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <afs/assert.h>
#include <string.h>
#include <sys/file.h>
#include <sys/param.h>
#include <lock.h>
#if defined(AFS_SUN5_ENV) || defined(AFS_HPUX_ENV)
#include <unistd.h>
#endif
#include <afs/afsutil.h>
#include <lwp.h>
#include "nfs.h"
#include <afs/afsint.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"
#include "voldefs.h"
#include "partition.h"
#include "fssync.h"
#include "volume_inline.h"
#include "common.h"
#include <afs/errors.h>

/*@+fcnmacros +macrofcndecl@*/
#ifdef O_LARGEFILE
#ifdef S_SPLINT_S
#endif /*S_SPLINT_S */
#define afs_stat		stat64
#define afs_fstat		fstat64
#define afs_open		open64
#define afs_fopen		fopen64
#else /* !O_LARGEFILE */
#ifdef S_SPLINT_S
#endif /*S_SPLINT_S */
#define afs_stat		stat
#define afs_fstat		fstat
#define afs_open		open
#define afs_fopen		fopen
#endif /* !O_LARGEFILE */
/*@=fcnmacros =macrofcndecl@*/

#ifndef LOCK_SH
#define   LOCK_SH   1    /* shared lock */
#define   LOCK_EX   2    /* exclusive lock */
#define   LOCK_NB   4    /* don't block when locking */
#define   LOCK_UN   8    /* unlock */
#endif

#ifdef AFS_SALSRV_ENV
#include <pthread.h>
#include <afs/work_queue.h>
#include <afs/thread_pool.h>
#include <vol/vol-salvage.h>
#endif

#ifndef HAVE_FLOCK
#include <fcntl.h>

/*
 * This function emulates a subset of flock()
 */
int
emul_flock(int fd, int cmd)
{    struct flock f;

    memset(&f, 0, sizeof (f));

    if (cmd & LOCK_UN)
        f.l_type = F_UNLCK;
    if (cmd & LOCK_SH)
        f.l_type = F_RDLCK;
    if (cmd & LOCK_EX)
        f.l_type = F_WRLCK;

    return fcntl(fd, (cmd & LOCK_NB) ? F_SETLK : F_SETLKW, &f);
}

#define flock(f,c)      emul_flock(f,c)
#endif

int Testing=0;


afs_sfsize_t
namei_iread(IHandle_t * h, afs_foff_t offset, char *buf, afs_fsize_t size)
{
    afs_sfsize_t nBytes;
    FdHandle_t *fdP;

    fdP = IH_OPEN(h);
    if (fdP == NULL)
	return -1;

    nBytes = FDH_PREAD(fdP, buf, size, offset);
    FDH_CLOSE(fdP);
    return nBytes;
}

afs_sfsize_t
namei_iwrite(IHandle_t * h, afs_foff_t offset, char *buf, afs_fsize_t size)
{
    afs_sfsize_t nBytes;
    FdHandle_t *fdP;

    fdP = IH_OPEN(h);
    if (fdP == NULL)
	return -1;

    nBytes = FDH_PWRITE(fdP, buf, size, offset);
    FDH_CLOSE(fdP);
    return nBytes;
}



/* Inode number format:
 * low 26 bits - vnode number - all 1's if volume special file.
 * next 3 bits - tag
 * next 3 bits spare (0's)
 * high 32 bits - uniquifier (regular) or type if spare
 */
#define NAMEI_VNODEMASK    0x003ffffff
#define NAMEI_TAGMASK      0x7
#define NAMEI_TAGSHIFT     26
#define NAMEI_UNIQMASK     0xffffffff
#define NAMEI_UNIQSHIFT    32
#define NAMEI_INODESPECIAL ((Inode)NAMEI_VNODEMASK)
#define NAMEI_VNODESPECIAL NAMEI_VNODEMASK

/* dir1 is the high 8 bits of the 26 bit vnode */
#define VNO_DIR1(vno) ((vno >> 14) & 0xff)
/* dir2 is the next 9 bits */
#define VNO_DIR2(vno) ((vno >> 9) & 0x1ff)
/* "name" is the low 9 bits of the vnode, the 3 bit tag and the uniq */

#define NAMEI_SPECDIR "special"
#define NAMEI_SPECDIRLEN (sizeof(NAMEI_SPECDIR)-1)

#define NAMEI_MAXVOLS 5		/* Maximum supported number of volumes per volume
				 * group, not counting temporary (move) volumes.
				 * This is the number of separate files, all having
				 * the same vnode number, which can occur in a volume
				 * group at once.
				 */


typedef struct {
    int ogm_owner;
    int ogm_group;
    int ogm_mode;
} namei_ogm_t;

static int namei_GetLinkCount2(FdHandle_t * h, Inode ino, int lockit, int fixup, int nowrite);

static int GetFreeTag(IHandle_t * ih, int vno);

/* namei_HandleToInodeDir
 *
 * Construct the path name of the directory holding the inode data.
 * Format: /<vicepx>/INODEDIR
 *
 */
#define PNAME_BLEN 64
static void
namei_HandleToInodeDir(namei_t * name, IHandle_t * ih)
{
    size_t offset;

    memset(name, '\0', sizeof(*name));

    /*
     * Add the /vicepXX string to the start of name->n_base and then calculate
     * offset as the number of bytes we know we added.
     *
     * FIXME: This embeds knowledge of the vice partition naming scheme and
     * mapping from device numbers.  There needs to be an API that tells us
     * this offset.
     */
    volutil_PartitionName_r(ih->ih_dev, name->n_base, sizeof(name->n_base));
    offset = VICE_PREFIX_SIZE + (ih->ih_dev > 25 ? 2 : 1);
    name->n_base[offset] = '/';
    offset++;
    strlcpy(name->n_base + offset, INODEDIR, sizeof(name->n_base) - offset);
    strlcpy(name->n_path, name->n_base, sizeof(name->n_path));
}

#define addtoname(N, C)                                 \
do {                                                    \
    strlcat((N)->n_path, "/", sizeof((N)->n_path));     \
    strlcat((N)->n_path, (C), sizeof((N)->n_path));     \
} while(0)


static void
namei_HandleToVolDir(namei_t * name, IHandle_t * ih)
{
    lb64_string_t tmp;

    namei_HandleToInodeDir(name, ih);
    (void)int32_to_flipbase64(tmp, (int64_t) (ih->ih_vid & 0xff));
    strlcpy(name->n_voldir1, tmp, sizeof(name->n_voldir1));
    addtoname(name, name->n_voldir1);
    (void)int32_to_flipbase64(tmp, (int64_t) ih->ih_vid);
    strlcpy(name->n_voldir2, tmp, sizeof(name->n_voldir2));
    addtoname(name, name->n_voldir2);
}

/* namei_HandleToName
 *
 * Constructs a file name for the fully qualified handle.
 * Note that special files end up in /vicepX/InodeDir/Vxx/V*.data/special
 */
void
namei_HandleToName(namei_t * name, IHandle_t * ih)
{
    lb64_string_t str;
    int vno = (int)(ih->ih_ino & NAMEI_VNODEMASK);

    namei_HandleToVolDir(name, ih);

    if (vno == NAMEI_VNODESPECIAL) {
	strlcpy(name->n_dir1, NAMEI_SPECDIR, sizeof(name->n_dir1));
	addtoname(name, name->n_dir1);
	name->n_dir2[0] = '\0';
    } else {
	(void)int32_to_flipbase64(str, VNO_DIR1(vno));
	strlcpy(name->n_dir1, str, sizeof(name->n_dir1));
	addtoname(name, name->n_dir1);
	(void)int32_to_flipbase64(str, VNO_DIR2(vno));
	strlcpy(name->n_dir2, str, sizeof(name->n_dir2));
	addtoname(name, name->n_dir2);
    }
    (void)int64_to_flipbase64(str, (int64_t) ih->ih_ino);
    strlcpy(name->n_inode, str, sizeof(name->n_inode));
    addtoname(name, name->n_inode);
}

/* The following is a warning to tell sys-admins to not muck about in this
 * name space.
 */
#define VICE_README "These files and directories are a part of the AFS \
namespace. Modifying them\nin any way will result in loss of AFS data,\n\
ownership and permissions included.\n"
int
namei_ViceREADME(char *partition)
{
    char filename[32];
    int fd;

    /* Create the inode directory if we're starting for the first time */
    (void)afs_snprintf(filename, sizeof filename, "%s/%s", partition,
		       INODEDIR);
    mkdir(filename, 0700);

    (void)afs_snprintf(filename, sizeof filename, "%s/%s/README", partition,
		       INODEDIR);
    fd = afs_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0444);
    if (fd >= 0) {
	(void)write(fd, VICE_README, strlen(VICE_README));
	close(fd);
    }
    return (errno);
}


#define create_dir() \
do { \
    if (mkdir(tmp, 0700)<0) { \
	if (errno != EEXIST) \
	    return -1; \
    } \
    else { \
	*created = 1; \
    } \
} while (0)

#define create_nextdir(A) \
do { \
	 strcat(tmp, "/"); strcat(tmp, A); create_dir();  \
} while(0)

/* namei_CreateDataDirectories
 *
 * If creating the file failed because of ENOENT or ENOTDIR, try
 * creating all the directories first.
 */
static int
namei_CreateDataDirectories(namei_t * name, int *created)
{
    char tmp[256];

    *created = 0;

    strlcpy(tmp, name->n_base, sizeof(tmp));
    create_dir();

    create_nextdir(name->n_voldir1);
    create_nextdir(name->n_voldir2);
    create_nextdir(name->n_dir1);
    if (name->n_dir2[0]) {
	create_nextdir(name->n_dir2);
    }
    return 0;
}

/* delTree(): Deletes an entire tree of directories (no files)
 * Input:
 *   root : Full path to the subtree. Should be big enough for PATH_MAX
 *   tree : the subtree to be deleted is rooted here. Specifies only the
 *          subtree beginning at tree (not the entire path). It should be
 *          a pointer into the "root" buffer.
 * Output:
 *  errp : errno of the first error encountered during the directory cleanup.
 *         *errp should have been initialized to 0.
 *
 * Return Values:
 *  -1  : If errors were encountered during cleanup and error is set to
 *        the first errno.
 *   0  : Success.
 *
 * If there are errors, we try to work around them and delete as many
 * directories as possible. We don't attempt to remove directories that still
 * have non-dir entries in them.
 */
static int
delTree(char *root, char *tree, int *errp)
{
    char *cp;
    DIR *ds;
    struct dirent *dirp;
    struct afs_stat st;

    if (*tree) {
	/* delete the children first */
	cp = strchr(tree, '/');
	if (cp) {
	    delTree(root, cp + 1, errp);
	    *cp = '\0';
	} else
	    cp = tree + strlen(tree);	/* move cp to the end of string tree */

	/* now delete all entries in this dir */
	if ((ds = opendir(root)) != (DIR *) NULL) {
	    errno = 0;
	    while ((dirp = readdir(ds))) {
		/* ignore . and .. */
		if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
		    continue;
		/* since root is big enough, we reuse the space to
		 * concatenate the dirname to the current tree
		 */
		strcat(root, "/");
		strcat(root, dirp->d_name);
		if (afs_stat(root, &st) == 0 && S_ISDIR(st.st_mode)) {
		    /* delete this subtree */
		    delTree(root, cp + 1, errp);
		} else
		    *errp = *errp ? *errp : errno;

		/* recover path to our cur tree by truncating it to
		 * its original len
		 */
		*cp = 0;
	    }
	    /* if (!errno) -- closedir not implicit if we got an error */
	    closedir(ds);
	}

	/* finally axe the current dir */
	if (rmdir(root))
	    *errp = *errp ? *errp : errno;

#ifndef AFS_PTHREAD_ENV		/* let rx get some work done */
	IOMGR_Poll();
#endif /* !AFS_PTHREAD_ENV */

    }

    /* if valid tree */
    /* if we encountered errors during cleanup, we return a -1 */
    if (*errp)
	return -1;

    return 0;

}

/* namei_RemoveDataDirectories
 * Return Values:
 * Returns 0 on success.
 * Returns -1 on error. Typically, callers ignore this error bcause we
 * can continue running if the removes fail. The salvage process will
 * finish tidying up for us. We only use the n_base and n_voldir1 entries
 * and only do rmdir's.
 */

static int
namei_RemoveDataDirectories(namei_t * name)
{
    char pbuf[MAXPATHLEN], *path = pbuf;
    int prefixlen = strlen(name->n_base), err = 0;
    int vollen = strlen(name->n_voldir1);
    int code;

    strlcpy(path, name->n_path, sizeof(pbuf));

    /* move past the prefix and n_voldir1 */
    path = path + prefixlen + 1 + vollen + 1;	/* skip over the trailing / */

    /* now delete all dirs upto path */
    code = delTree(pbuf, path, &err);

    /* We've now deleted everything under /n_base/n_voldir1/n_voldir2 that
     * we could. Do not delete /n_base/n_voldir1, since doing such might
     * interrupt another thread trying to create a volume. We could introduce
     * some locking to make this safe (or only remove it for whole-partition
     * salvages), but by not deleting it we only leave behind a maximum of
     * 256 empty directories. So at least for now, don't bother. */

    return code;
}

/* Create the file in the name space.
 *
 * Parameters stored as follows:
 * Regular files:
 * p1 - volid - implied in containing directory.
 * p2 - vnode - name is <vno:31-23>/<vno:22-15>/<vno:15-0><uniq:31-5><tag:2-0>
 * p3 - uniq -- bits 4-0 are in mode bits 4-0
 * p4 - dv ---- dv:15-0 in uid, dv:29-16 in gid, dv:31-30 in mode:6-5
 * Special files:
 * p1 - volid - creation time - dwHighDateTime
 * p2 - vnode - -1 means special, file goes in "S" subdirectory.
 * p3 - type -- name is <type>.<tag> where tag is a file name unqiquifier.
 * p4 - parid - parent volume id - implied in containing directory.
 *
 * Return value is the inode number or (Inode)-1 if error.
 * We "know" there is only one link table, so return EEXIST if there already
 * is a link table. It's up to the calling code to test errno and increment
 * the link count.
 */

/* namei_MakeSpecIno
 *
 * This function is called by VCreateVolume to hide the implementation
 * details of the inode numbers. This only allows for 7 volume special
 * types, but if we get that far, this could should be dead by then.
 */
Inode
namei_MakeSpecIno(int volid, int type)
{
    Inode ino;
    ino = NAMEI_INODESPECIAL;
    type &= NAMEI_TAGMASK;
    ino |= ((Inode) type) << NAMEI_TAGSHIFT;
    ino |= ((Inode) volid) << NAMEI_UNIQSHIFT;
    return ino;
}

/* SetOGM - set owner group and mode bits from parm and tag
 *
 * owner - low 15 bits of parm.
 * group - next 15 bits of parm.
 * mode - 2 bits of parm, then lowest = 3 bits of tag.
 */
static int
SetOGM(int fd, int parm, int tag)
{
    int owner, group, mode;

    owner = parm & 0x7fff;
    group = (parm >> 15) & 0x7fff;
    if (fchown(fd, owner, group) < 0)
	return -1;

    mode = (parm >> 27) & 0x18;
    mode |= tag & 0x7;
    if (fchmod(fd, mode) < 0)
	return -1;

    return 0;

}

/* GetOGM - get parm and tag from owner, group and mode bits. */
static void
GetOGMFromStat(struct afs_stat *status, int *parm, int *tag)
{
    *parm = status->st_uid | (status->st_gid << 15);
    *parm |= (status->st_mode & 0x18) << 27;
    *tag = status->st_mode & 0x7;
}

static int
GetOGM(int fd, int *parm, int *tag)
{
    struct afs_stat status;
    if (afs_fstat(fd, &status) < 0)
	return -1;

    GetOGMFromStat(&status, parm, tag);
    return 0;
}

int big_vno = 0;		/* Just in case we ever do 64 bit vnodes. */

/* Derive the name and create it O_EXCL. If that fails we have an error.
 * Get the tag from a free column in the link table.
 */
Inode
namei_icreate(IHandle_t * lh, char *part, int p1, int p2, int p3, int p4)
{
    namei_t name;
    int fd = -1;
    int code = 0;
    int created_dir = 0;
    IHandle_t tmp;
    FdHandle_t *fdP;
    FdHandle_t tfd;
    int tag;
    int ogm_parm;


    memset((void *)&tmp, 0, sizeof(IHandle_t));


    tmp.ih_dev = volutil_GetPartitionID(part);
    if (tmp.ih_dev == -1) {
	errno = EINVAL;
	return -1;
    }

    if (p2 == -1) {
	/* Parameters for special file:
	 * p1 - volume id - goes into owner/group/mode
	 * p2 - vnode == -1
	 * p3 - type
	 * p4 - parent volume id
	 */
	ogm_parm = p1;
	tag = p3;

	tmp.ih_vid = p4;	/* Use parent volume id, where this file will be. */
	tmp.ih_ino = namei_MakeSpecIno(p1, p3);
    } else {
	int vno = p2 & NAMEI_VNODEMASK;
	/* Parameters for regular file:
	 * p1 - volume id
	 * p2 - vnode
	 * p3 - uniq
	 * p4 - dv
	 */

	if (vno != p2) {
	    big_vno++;
	    errno = EINVAL;
	    return -1;
	}
	/* If GetFreeTag succeeds, it atomically sets link count to 1. */
	tag = GetFreeTag(lh, p2);
	if (tag < 0)
	    goto bad;

	/* name is <uniq(p3)><tag><vno(p2)> */
	tmp.ih_vid = p1;
	tmp.ih_ino = (Inode) p2;
	tmp.ih_ino |= ((Inode) tag) << NAMEI_TAGSHIFT;
	tmp.ih_ino |= ((Inode) p3) << NAMEI_UNIQSHIFT;

	ogm_parm = p4;
    }

    namei_HandleToName(&name, &tmp);
    fd = afs_open(name.n_path, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0);
    if (fd < 0) {
	if (errno == ENOTDIR || errno == ENOENT) {
	    if (namei_CreateDataDirectories(&name, &created_dir) < 0)
		goto bad;
	    fd = afs_open(name.n_path, O_CREAT | O_EXCL | O_TRUNC | O_RDWR,
			  0);
	    if (fd < 0)
		goto bad;
	} else {
	    goto bad;
	}
    }
    if (SetOGM(fd, ogm_parm, tag) < 0) {
	close(fd);
	fd = -1;
	goto bad;
    }

    if (p2 == -1 && p3 == VI_LINKTABLE) {
	/* hack at tmp to setup for set link count call. */
	memset((void *)&tfd, 0, sizeof(FdHandle_t));	/* minimalistic still, but a little cleaner */
	tfd.fd_ih = &tmp;
	tfd.fd_fd = fd;
	code = namei_SetLinkCount(&tfd, (Inode) 0, 1, 0);
    }

  bad:
    if (fd >= 0)
	close(fd);


    if (code || (fd < 0)) {
	if (p2 != -1) {
	    fdP = IH_OPEN(lh);
	    if (fdP) {
		namei_SetLinkCount(fdP, tmp.ih_ino, 0, 0);
		FDH_CLOSE(fdP);
	    }
	}
    }
    return (code || (fd < 0)) ? (Inode) - 1 : tmp.ih_ino;
}


/* namei_iopen */
int
namei_iopen(IHandle_t * h)
{
    int fd;
    namei_t name;

    /* Convert handle to file name. */
    namei_HandleToName(&name, h);
    fd = afs_open(name.n_path, O_RDWR, 0666);
    return fd;
}

/* Need to detect vol special file and just unlink. In those cases, the
 * handle passed in _is_ for the inode. We only check p1 for the special
 * files.
 */
int
namei_dec(IHandle_t * ih, Inode ino, int p1)
{
    int count = 0;
    namei_t name;
    int code = 0;
    FdHandle_t *fdP;

    if ((ino & NAMEI_INODESPECIAL) == NAMEI_INODESPECIAL) {
	IHandle_t *tmp;
	int inode_p1, tag;
	int type = (int)((ino >> NAMEI_TAGSHIFT) & NAMEI_TAGMASK);

	/* Verify this is the right file. */
	IH_INIT(tmp, ih->ih_dev, ih->ih_vid, ino);

	fdP = IH_OPEN(tmp);
	if (fdP == NULL) {
	    IH_RELEASE(tmp);
	    errno = EINVAL;
	    return -1;
	}

	if ((GetOGM(fdP->fd_fd, &inode_p1, &tag) < 0) || (inode_p1 != p1)) {
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(tmp);
	    errno = EINVAL;
	    return -1;
	}

	/* If it's the link table itself, decrement the link count. */
	if (type == VI_LINKTABLE) {
	    if ((count = namei_GetLinkCount(fdP, (Inode) 0, 1)) < 0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return -1;
	    }

	    count--;
	    if (namei_SetLinkCount(fdP, (Inode) 0, count < 0 ? 0 : count, 1) <
		0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return -1;
	    }

	    if (count > 0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return 0;
	    }
	}

	namei_HandleToName(&name, tmp);
	if ((code = unlink(name.n_path)) == 0) {
	    if (type == VI_LINKTABLE) {
		/* Try to remove directory. If it fails, that's ok.
		 * Salvage will clean up.
		 */
		(void)namei_RemoveDataDirectories(&name);
	    }
	}
	FDH_REALLYCLOSE(fdP);
	IH_RELEASE(tmp);
    } else {
	/* Get a file descriptor handle for this Inode */
	fdP = IH_OPEN(ih);
	if (fdP == NULL) {
	    return -1;
	}

	if ((count = namei_GetLinkCount(fdP, ino, 1)) < 0) {
	    FDH_REALLYCLOSE(fdP);
	    return -1;
	}

	count--;
	if (count >= 0) {
	    if (namei_SetLinkCount(fdP, ino, count, 1) < 0) {
		FDH_REALLYCLOSE(fdP);
		return -1;
	    }
	} else {
	    IHandle_t *th;
	    IH_INIT(th, ih->ih_dev, ih->ih_vid, ino);
	    Log("Warning: Lost ref on ihandle dev %d vid %d ino %" AFS_INT64_FMT "\n",
		th->ih_dev, th->ih_vid, (afs_int64)th->ih_ino);
	    IH_RELEASE(th);

	    /* If we're less than 0, someone presumably unlinked;
	       don't bother setting count to 0, but we need to drop a lock */
	    if (namei_SetLinkCount(fdP, ino, 0, 1) < 0) {
		FDH_REALLYCLOSE(fdP);
		return -1;
	    }
	}
	if (count == 0) {
	    IHandle_t *th;
	    IH_INIT(th, ih->ih_dev, ih->ih_vid, ino);

	    namei_HandleToName(&name, th);
	    IH_RELEASE(th);
	    code = unlink(name.n_path);
	}
	FDH_CLOSE(fdP);
    }

    return code;
}

int
namei_inc(IHandle_t * h, Inode ino, int p1)
{
    int count;
    int code = 0;
    FdHandle_t *fdP;

    if ((ino & NAMEI_INODESPECIAL) == NAMEI_INODESPECIAL) {
	int type = (int)((ino >> NAMEI_TAGSHIFT) & NAMEI_TAGMASK);
	if (type != VI_LINKTABLE)
	    return 0;
	ino = (Inode) 0;
    }

    /* Get a file descriptor handle for this Inode */
    fdP = IH_OPEN(h);
    if (fdP == NULL) {
	return -1;
    }

    if ((count = namei_GetLinkCount(fdP, ino, 1)) < 0)
	code = -1;
    else {
	count++;
	if (count > 7) {
	    errno = EINVAL;
	    code = -1;
	    count = 7;
	}
	if (namei_SetLinkCount(fdP, ino, count, 1) < 0)
	    code = -1;
    }
    if (code) {
	FDH_REALLYCLOSE(fdP);
    } else {
	FDH_CLOSE(fdP);
    }
    return code;
}

int
namei_replace_file_by_hardlink(IHandle_t *hLink, IHandle_t *hTarget)
{
    afs_int32 code;
    namei_t nameLink;
    namei_t nameTarget;

    /* Convert handle to file name. */
    namei_HandleToName(&nameLink, hLink);
    namei_HandleToName(&nameTarget, hTarget);

    unlink(nameLink.n_path);
    code = link(nameTarget.n_path, nameLink.n_path);
    return code;
}

int
namei_copy_on_write(IHandle_t *h)
{
    afs_int32 fd, code = 0;
    namei_t name;
    FdHandle_t *fdP;
    struct afs_stat tstat;
    afs_foff_t offset;

    namei_HandleToName(&name, h);
    if (afs_stat(name.n_path, &tstat) < 0)
	return EIO;
    if (tstat.st_nlink > 1) {                   /* do a copy on write */
	char path[259];
	char *buf;
	afs_size_t size;
	ssize_t tlen;

	fdP = IH_OPEN(h);
	if (!fdP)
	    return EIO;
	afs_snprintf(path, sizeof(path), "%s-tmp", name.n_path);
	fd = afs_open(path, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0);
	if (fd < 0) {
	    FDH_CLOSE(fdP);
	    return EIO;
	}
	buf = malloc(8192);
	if (!buf) {
	    close(fd);
	    unlink(path);
	    FDH_CLOSE(fdP);
	    return ENOMEM;
	}
	size = tstat.st_size;
	offset = 0;
	while (size) {
	    tlen = size > 8192 ? 8192 : size;
	    if (FDH_PREAD(fdP, buf, tlen, offset) != tlen)
		break;
	    if (write(fd, buf, tlen) != tlen)
		break;
	    size -= tlen;
	    offset += tlen;
	}
	close(fd);
	FDH_REALLYCLOSE(fdP);
	free(buf);
	if (size)
	    code = EIO;
	else {
	    unlink(name.n_path);
	    code = rename(path, name.n_path);
	}
    }
    return code;
}

/************************************************************************
 * File Name Structure
 ************************************************************************
 *
 * Each AFS file needs a unique name and it needs to be findable with
 * minimal lookup time. Note that the constraint on the number of files and
 * directories in a volume is the size of the vnode index files and the
 * max file size AFS supports (for internal files) of 2^31. Since a record
 * in the small vnode index file is 64 bytes long, we can have at most
 * (2^31)/64 or 33554432 files. A record in the large index file is
 * 256 bytes long, giving a maximum of (2^31)/256 = 8388608 directories.
 * Another layout parameter is that there is roughly a 16 to 1 ratio between
 * the number of files and the number of directories.
 *
 * Using this information we can see that a layout of 256 directories, each
 * with 512 subdirectories and each of those having 512 files gives us
 * 256*512*512 = 67108864 AFS files and directories.
 *
 * The volume, vnode, uniquifier and data version, as well as the tag
 * are required, either for finding the file or for salvaging. It's best to
 * restrict the name to something that can be mapped into 64 bits so the
 * "Inode" is easily comparable (using "==") to other "Inodes". The tag
 * is used to distinguish between different versions of the same file
 * which are currently in the RW and clones of a volume. See "Link Table
 * Organization" below for more information on the tag. The tag is
 * required in the name of the file to ensure a unique name.
 *
 * We can store data in the uid, gid and mode bits of the files, provided
 * the directories have root only access. This gives us 15 bits for each
 * of uid and gid (GNU chown considers 65535 to mean "don't change").
 * There are 9 available mode bits. Adn we need to store a total of
 * 32 (volume id) + 26 (vnode) + 32 (uniquifier) + 32 (data-version) + 3 (tag)
 * or 131 bits somewhere.
 *
 * The format of a file name for a regular file is:
 * /vicepX/AFSIDat/V1/V2/AA/BB/<tag><uniq><vno>
 * V1 - low 8 bits of RW volume id
 * V2 - all bits of RW volume id
 * AA - high 8 bits of vnode number.
 * BB - next 9 bits of vnode number.
 * <tag><uniq><vno> - file name
 *
 * Volume special files are stored in a separate directory:
 * /vicepX/AFSIDat/V1/V2/special/<tag><uniq><vno>
 *
 *
 * The vnode is hashed into the directory using the high bits of the
 * vnode number. This is so that consecutively created vnodes are in
 * roughly the same area on the disk. This will at least be optimal if
 * the user is creating many files in the same AFS directory. The name
 * should be formed so that the leading characters are different as quickly
 * as possible, leading to faster discards of incorrect matches in the
 * lookup code.
 *
 */


/************************************************************************
 *  Link Table Organization
 ************************************************************************
 *
 * The link table volume special file is used to hold the link counts that
 * are held in the inodes in inode based AFS vice filesystems. For user
 * space access, the link counts are being kept in a separate
 * volume special file. The file begins with the usual version stamp
 * information and is then followed by one row per vnode number. vnode 0
 * is used to hold the link count of the link table itself. That is because
 * the same link table is shared among all the volumes of the volume group
 * and is deleted only when the last volume of a volume group is deleted.
 *
 * Within each row, the columns are 3 bits wide. They can each hold a 0 based
 * link count from 0 through 7. Each colume represents a unique instance of
 * that vnode. Say we have a file shared between the RW and a RO and a
 * different version of the file (or a different uniquifer) for the BU volume.
 * Then one column would be holding the link count of 2 for the RW and RO
 * and a different column would hold the link count of 1 for the BU volume.
 * Note that we allow only 5 volumes per file, giving 15 bits used in the
 * short.
 */
#define LINKTABLE_WIDTH 2
#define LINKTABLE_SHIFT 1	/* log 2 = 1 */

/**
 * compute namei link table file and bit offset from inode number.
 *
 * @param[in]   ino     inode number
 * @param[out]  offset  link table file offset
 * @param[out]  index   bit offset within 2-byte record
 *
 * @internal
 */
static void
namei_GetLCOffsetAndIndexFromIno(Inode ino, afs_foff_t * offset, int *index)
{
    int toff = (int)(ino & NAMEI_VNODEMASK);
    int tindex = (int)((ino >> NAMEI_TAGSHIFT) & NAMEI_TAGMASK);

    *offset = (afs_foff_t) ((toff << LINKTABLE_SHIFT) + 8);	/* * 2 + sizeof stamp */
    *index = (tindex << 1) + tindex;
}

#ifdef AFS_PTHREAD_ENV
/* XXX do static initializers work for WINNT/pthread? */
pthread_mutex_t _namei_glc_lock = PTHREAD_MUTEX_INITIALIZER;
#define NAMEI_GLC_LOCK assert(pthread_mutex_lock(&_namei_glc_lock) == 0)
#define NAMEI_GLC_UNLOCK assert(pthread_mutex_unlock(&_namei_glc_lock) == 0)
#else /* !AFS_PTHREAD_ENV */
#define NAMEI_GLC_LOCK
#define NAMEI_GLC_UNLOCK
#endif /* !AFS_PTHREAD_ENV */

/**
 * get the link count of an inode.
 *
 * @param[in]  h        namei link count table file handle
 * @param[in]  ino      inode number for which we are requesting a link count
 * @param[in]  lockit   if asserted, return with lock held on link table file
 * @param[in]  fixup    if asserted, write 1 to link count when read() returns
 *                      zero (at EOF)
 * @param[in]  nowrite  return success on zero byte read or ZLC
 *
 * @post if lockit asserted and lookup was successful, will return with write
 *       lock on link table file descriptor
 *
 * @return link count
 *    @retval -1 namei link table i/o error
 *
 * @internal
 */
static int
namei_GetLinkCount2(FdHandle_t * h, Inode ino, int lockit, int fixup, int nowrite)
{
    unsigned short row = 0;
    afs_foff_t offset;
    ssize_t rc;
    int index;

    /* there's no linktable yet. the salvager will create one later */
    if (h->fd_fd == -1 && fixup)
       return 1;
    namei_GetLCOffsetAndIndexFromIno(ino, &offset, &index);

    if (lockit) {
	if (flock(h->fd_fd, LOCK_EX) < 0)
	    return -1;
    }

    rc = FDH_PREAD(h, (char*)&row, sizeof(row), offset);
    if ((rc == 0 || !((row >> index) & NAMEI_TAGMASK)) && fixup && nowrite)
        return 1;
    if (rc == 0 && fixup) {
	/*
	 * extend link table and write a link count of 1 for ino
	 *
	 * in order to make MT-safe, truncation (extension really)
	 * must happen under a mutex
	 */
        struct stat st;
	NAMEI_GLC_LOCK;
        if (fstat(h->fd_fd, &st) || st.st_size >= offset+sizeof(row)) {
	    NAMEI_GLC_UNLOCK;
	    goto bad_getLinkByte;
	}
        FDH_TRUNC(h, offset+sizeof(row));
        row = 1 << index;
	rc = FDH_PWRITE(h, (char *)&row, sizeof(row), offset);
	NAMEI_GLC_UNLOCK;
    }
    if (rc != sizeof(row)) {
	goto bad_getLinkByte;
    }

    if (fixup && !((row >> index) & NAMEI_TAGMASK)) {
	/*
	 * fix up zlc
	 *
	 * in order to make this mt-safe, we need to do the read-modify-write
	 * under a mutex.  thus, we repeat the read inside the lock.
	 */
	NAMEI_GLC_LOCK;
	rc = FDH_PREAD(h, (char *)&row, sizeof(row), offset);
	if (rc == sizeof(row)) {
	    row |= 1<<index;
	    rc = FDH_PWRITE(h, (char *)&row, sizeof(row), offset);
	}
	NAMEI_GLC_UNLOCK;
        if (rc != sizeof(row))
	    goto bad_getLinkByte;
    }

    return (int)((row >> index) & NAMEI_TAGMASK);

  bad_getLinkByte:
    if (lockit)
	flock(h->fd_fd, LOCK_UN);
    return -1;
}

int
namei_GetLinkCount(FdHandle_t * h, Inode ino, int lockit)
{
    return namei_GetLinkCount2(h, ino, lockit, 0, 1);
}

/* Return a free column index for this vnode. */
static int
GetFreeTag(IHandle_t * ih, int vno)
{
    FdHandle_t *fdP;
    afs_foff_t offset;
    int col;
    int coldata;
    short row;
    ssize_t nBytes;


    fdP = IH_OPEN(ih);
    if (fdP == NULL)
	return -1;

    /* Only one manipulates at a time. */
    if (flock(fdP->fd_fd, LOCK_EX) < 0) {
	FDH_REALLYCLOSE(fdP);
	return -1;
    }

    offset = (vno << LINKTABLE_SHIFT) + 8;	/* * 2 + sizeof stamp */

    nBytes = FDH_PREAD(fdP, (char *)&row, sizeof(row), offset);
    if (nBytes != sizeof(row)) {
	if (nBytes != 0)
	    goto badGetFreeTag;
	row = 0;
    }

    /* Now find a free column in this row and claim it. */
    for (col = 0; col < NAMEI_MAXVOLS; col++) {
	coldata = 7 << (col * 3);
	if ((row & coldata) == 0)
	    break;
    }
    if (col >= NAMEI_MAXVOLS) {
	errno = ENOSPC;
	goto badGetFreeTag;
    }

    coldata = 1 << (col * 3);
    row |= coldata;

    if (FDH_PWRITE(fdP, (char *)&row, sizeof(row), offset) != sizeof(row)) {
	goto badGetFreeTag;
    }
    FDH_SYNC(fdP);
    flock(fdP->fd_fd, LOCK_UN);
    FDH_REALLYCLOSE(fdP);
    return col;;

  badGetFreeTag:
    flock(fdP->fd_fd, LOCK_UN);
    FDH_REALLYCLOSE(fdP);
    return -1;
}



/* namei_SetLinkCount
 * If locked is set, assume file is locked. Otherwise, lock file before
 * proceeding to modify it.
 */
int
namei_SetLinkCount(FdHandle_t * fdP, Inode ino, int count, int locked)
{
    afs_foff_t offset;
    int index;
    unsigned short row;
    int junk;
    ssize_t nBytes = -1;

    namei_GetLCOffsetAndIndexFromIno(ino, &offset, &index);

    if (!locked) {
	if (flock(fdP->fd_fd, LOCK_EX) < 0) {
	    return -1;
	}
    }

    nBytes = FDH_PREAD(fdP, (char *)&row, sizeof(row), offset);
    if (nBytes != sizeof(row)) {
	if (nBytes != 0) {
	    errno = EBADF;
	    goto bad_SetLinkCount;
	}
	row = 0;
    }

    junk = 7 << index;
    count <<= index;
    row &= (unsigned short)~junk;
    row |= (unsigned short)count;

    if (FDH_PWRITE(fdP, (char *)&row, sizeof(short), offset) != sizeof(short)) {
	errno = EBADF;
	goto bad_SetLinkCount;
    }
    FDH_SYNC(fdP);

    nBytes = 0;


  bad_SetLinkCount:
    flock(fdP->fd_fd, LOCK_UN);

    return nBytes;
}


/* ListViceInodes - write inode data to a results file. */
static int DecodeInode(char *dpath, char *name, struct ViceInodeInfo *info,
		       unsigned int volid);
static int DecodeVolumeName(char *name, unsigned int *vid);
static int namei_ListAFSSubDirs(IHandle_t * dirIH,
				int (*write_fun) (FILE *,
						  struct ViceInodeInfo *,
						  char *, char *), FILE * fp,
				int (*judgeFun) (struct ViceInodeInfo *,
						 afs_uint32 vid, void *),
				afs_uint32 singleVolumeNumber, void *rock);


/* WriteInodeInfo
 *
 * Write the inode data to the results file.
 *
 * Returns -2 on error, 0 on success.
 *
 * This is written as a callback simply so that other listing routines
 * can use the same inode reading code.
 */
static int
WriteInodeInfo(FILE * fp, struct ViceInodeInfo *info, char *dir, char *name)
{
    int n;
    n = fwrite(info, sizeof(*info), 1, fp);
    return (n == 1) ? 0 : -2;
}


int mode_errors;		/* Number of errors found in mode bits on directories. */
void
VerifyDirPerms(char *path)
{
    struct afs_stat status;

    if (afs_stat(path, &status) < 0) {
	Log("Unable to stat %s. Please manually verify mode bits for this"
	    " directory\n", path);
    } else {
	if (((status.st_mode & 0777) != 0700) || (status.st_uid != 0))
	    mode_errors++;
    }
}

/**
 * Fill the results file with the requested inode information.
 *
 * This code optimizes single volume salvages by just looking at that one
 * volume's directory.
 *
 * @param[in]   devname             device name string
 * @param[in]   moutnedOn           vice partition mount point
 * @param[in]   resultFile          result file in which to write inode
 *                                  metadata.  If NULL, write routine is not
 *                                  called.
 * @param[in]   judgeInode          filter function pointer.  if not NULL, only
 *                                  inodes for which this routine returns non-
 *                                  zero will be written to the results file.
 * @param[in]   singleVolumeNumber  volume id filter
 * @param[out]  forcep              always set to 0 for namei impl
 * @param[in]   forceR              not used by namei impl
 * @param[in]   wpath               not used by namei impl
 * @param[in]   rock                opaque pointer passed to judgeInode
 *
 * @return operation status
 *    @retval 0   success
 *    @retval -1  complete failure, salvage should terminate.
 *    @retval -2  not enough space on partition, salvager has error message
 *                for this.
 */
int
ListViceInodes(char *devname, char *mountedOn, FILE *inodeFile,
	       int (*judgeInode) (struct ViceInodeInfo * info, afs_uint32 vid, void *rock),
	       afs_uint32 singleVolumeNumber, int *forcep, int forceR, char *wpath,
	       void *rock)
{
    int ninodes;
    struct afs_stat status;

    *forcep = 0; /* no need to salvage until further notice */

    /* Verify protections on directories. */
    mode_errors = 0;
    VerifyDirPerms(mountedOn);

    ninodes =
	namei_ListAFSFiles(mountedOn, WriteInodeInfo, inodeFile, judgeInode,
			   singleVolumeNumber, rock);

    if (!inodeFile)
	return ninodes;

    if (ninodes < 0) {
	return ninodes;
    }

    if (fflush(inodeFile) == EOF) {
	Log("Unable to successfully flush inode file for %s\n", mountedOn);
	return -2;
    }
    if (fsync(fileno(inodeFile)) == -1) {
	Log("Unable to successfully fsync inode file for %s\n", mountedOn);
	return -2;
    }

    /*
     * Paranoia:  check that the file is really the right size
     */
    if (afs_fstat(fileno(inodeFile), &status) == -1) {
	Log("Unable to successfully stat inode file for %s\n", mountedOn);
	return -2;
    }
    if (status.st_size != ninodes * sizeof(struct ViceInodeInfo)) {
	Log("Wrong size (%d instead of %lu) in inode file for %s\n",
	    (int) status.st_size,
	    (long unsigned int) ninodes * sizeof(struct ViceInodeInfo),
	    mountedOn);
	return -2;
    }
    return 0;
}


/**
 * Collect all the matching AFS files on the drive.
 * If singleVolumeNumber is non-zero, just return files for that volume.
 *
 * @param[in] dev                 vice partition path
 * @param[in] writeFun            function pointer to a function which
 *                                writes inode information to FILE fp
 * @param[in] fp                  file stream where inode metadata is sent
 * @param[in] judgeFun            filter function pointer.  if not NULL,
 *                                only entries for which a non-zero value
 *                                is returned are written to fp
 * @param[in] singleVolumeNumber  volume id filter.  if nonzero, only
 *                                process files for that specific volume id
 * @param[in] rock                opaque pointer passed into writeFun and
 *                                judgeFun
 *
 * @return operation status
 *    @retval <0 error
 *    @retval >=0 number of matching files found
 */
int
namei_ListAFSFiles(char *dev,
		   int (*writeFun) (FILE *, struct ViceInodeInfo *, char *,
				    char *),
		   FILE * fp,
		   int (*judgeFun) (struct ViceInodeInfo *, afs_uint32, void *),
		   afs_uint32 singleVolumeNumber, void *rock)
{
    IHandle_t ih;
    namei_t name;
    int ninodes = 0;
    DIR *dirp1, *dirp2;
    struct dirent *dp1, *dp2;
    char path2[512];
#ifdef DELETE_ZLC
    static void FreeZLCList(void);
#endif

    memset((void *)&ih, 0, sizeof(IHandle_t));
    ih.ih_dev = volutil_GetPartitionID(dev);

    if (singleVolumeNumber) {
	ih.ih_vid = singleVolumeNumber;
	namei_HandleToVolDir(&name, &ih);
	ninodes =
	    namei_ListAFSSubDirs(&ih, writeFun, fp, judgeFun,
				 singleVolumeNumber, rock);
	if (ninodes < 0)
	    return ninodes;
    } else {
	/* Find all volume data directories and descend through them. */
	namei_HandleToInodeDir(&name, &ih);
	ninodes = 0;
	dirp1 = opendir(name.n_path);
	if (!dirp1)
	    return 0;
	while ((dp1 = readdir(dirp1))) {
	    if (*dp1->d_name == '.')
		continue;
	    afs_snprintf(path2, sizeof(path2), "%s/%s", name.n_path,
			 dp1->d_name);
	    dirp2 = opendir(path2);
	    if (dirp2) {
		while ((dp2 = readdir(dirp2))) {
		    if (*dp2->d_name == '.')
			continue;
		    if (!DecodeVolumeName(dp2->d_name, &ih.ih_vid)) {
			ninodes +=
			    namei_ListAFSSubDirs(&ih, writeFun, fp, judgeFun,
						 0, rock);
		    }
		}
		closedir(dirp2);
	    }
	}
	closedir(dirp1);
    }
#ifdef DELETE_ZLC
    FreeZLCList();
#endif
    return ninodes;
}

#ifdef DELETE_ZLC
static void AddToZLCDeleteList(char dir, char *name);
static void DeleteZLCFiles(char *path);
#endif

/**
 * examine a namei volume special file.
 *
 * @param[in] path1               volume special directory path
 * @param[in] dname               directory entry name
 * @param[in] myIH                inode handle to volume directory
 * @param[out] linkHandle         namei link count fd handle.  if
 *                                the inode in question is the link
 *                                table, then the FdHandle is populated
 * @param[in] writeFun            metadata write function pointer
 * @param[in] fp                  file pointer where inode metadata
 *                                is written by (*writeFun)()
 * @param[in] judgeFun            inode filter function pointer.  if
 *                                not NULL, only inodes for which this
 *                                function returns non-zero are recorded
 *                                into fp by writeFun
 * @param[in] singleVolumeNumer   volume id filter.  if non-zero, only
 *                                inodes associated with this volume id
 *                                are recorded by writeFun
 * @param[in] rock                opaque pointer passed to writeFun and
 *                                judgeFun
 *
 * @return operation status
 *    @retval 1 count this inode
 *    @retval 0 don't count this inode
 *    @retval -1 failure
 *
 * @internal
 */
static int
_namei_examine_special(char * path1,
		       char * dname,
		       IHandle_t * myIH,
		       FdHandle_t * linkHandle,
		       int (*writeFun) (FILE *, struct ViceInodeInfo *, char *,
					char *),
		       FILE * fp,
		       int (*judgeFun) (struct ViceInodeInfo *, afs_uint32, void *),
		       int singleVolumeNumber,
		       void *rock)
{
    int ret = 0;
    struct ViceInodeInfo info;

    if (DecodeInode(path1, dname, &info, myIH->ih_vid) < 0) {
	ret = 0;
	goto error;
    }

    if (info.u.param[2] != VI_LINKTABLE) {
	info.linkCount = 1;
    } else {
	char path2[512];
	/* Open this handle */
	(void)afs_snprintf(path2, sizeof(path2),
			   "%s/%s", path1, dname);
	linkHandle->fd_fd = afs_open(path2, Testing ? O_RDONLY : O_RDWR, 0666);
	info.linkCount =
	    namei_GetLinkCount2(linkHandle, (Inode) 0, 1, 1, Testing);
    }

    if (!judgeFun ||
	(*judgeFun) (&info, singleVolumeNumber, rock)) {
	ret = 1;
        if ((*writeFun) (fp, &info, path1, dname) < 0) {
	    ret = -1;
	}
    }

 error:
    return ret;
}

/**
 * examine a namei file.
 *
 * @param[in] path1               volume special directory path
 * @param[in] dname               directory entry name
 * @param[in] myIH                inode handle to volume directory
 * @param[in] linkHandle          namei link count fd handle.
 * @param[in] writeFun            metadata write function pointer
 * @param[in] fp                  file pointer where inode metadata
 *                                is written by (*writeFun)()
 * @param[in] judgeFun            inode filter function pointer.  if
 *                                not NULL, only inodes for which this
 *                                function returns non-zero are recorded
 *                                into fp by writeFun
 * @param[in] singleVolumeNumer   volume id filter.  if non-zero, only
 *                                inodes associated with this volume id
 *                                are recorded by writeFun
 * @param[in] rock                opaque pointer passed to writeFun and
 *                                judgeFun
 *
 * @return operation status
 *    @retval 1 count this inode
 *    @retval 0 don't count this inode
 *    @retval -1 failure
 *    @retval -2 request ZLC delete
 *
 * @internal
 */
static int
_namei_examine_reg(char * path3,
		   char * dname,
		   IHandle_t * myIH,
		   FdHandle_t * linkHandle,
		   int (*writeFun) (FILE *, struct ViceInodeInfo *, char *,
				    char *),
		   FILE * fp,
		   int (*judgeFun) (struct ViceInodeInfo *, afs_uint32, void *),
		   int singleVolumeNumber,
		   void *rock)
{
    int ret = 0;
    struct ViceInodeInfo info;
#ifdef DELETE_ZLC
    int i; /* XXX this isn't set anywhere, nor was it set in
	    *     namei_ListAFSSubdirs.  wtf? */
#endif

    if (DecodeInode(path3, dname, &info, myIH->ih_vid) < 0) {
	goto error;
    }

    info.linkCount =
	namei_GetLinkCount2(linkHandle,
			    info.inodeNumber, 1, 1, Testing);
    if (info.linkCount == 0) {
#ifdef DELETE_ZLC
	Log("Found 0 link count file %s/%s, deleting it.\n", path3, dname);
#ifdef AFS_SALSRV_ENV
	/* defer -- the AddToZLCDeleteList() interface is not MT-safe */
	ret = -2;
#else /* !AFS_SALSRV_ENV */
	AddToZLCDeleteList((char)i, dname);
#endif /* !AFS_SALSRV_ENV */
#else /* !DELETE_ZLC */
	Log("Found 0 link count file %s/%s.\n", path3,
	    dname);
#endif
	goto error;
    }

    if (!judgeFun ||
	(*judgeFun) (&info, singleVolumeNumber, rock)) {
	ret = 1;
        if ((*writeFun) (fp, &info, path3, dname) < 0) {
	    ret = -1;
	}
    }

 error:
    return ret;
}

/**
 * listsubdirs work queue node.
 */
struct listsubdirs_work_node {
#ifdef AFS_SALSRV_ENV
    int *error;                         /**< *error set if an error was
                                         *   encountered in any listsubdirs
                                         *   thread. */
#endif

    IHandle_t * IH;                     /**< volume directory handle */
    FdHandle_t *linkHandle;             /**< namei link count fd handle. when
                                         *   examinining the link table special
                                         *   inode, this will be pointed at the
                                         *   link table
                                         */
    FILE * fp;                          /**< file pointer for writeFun */

    /** function which will write inode metadata to fp */
    int (*writeFun) (FILE *, struct ViceInodeInfo *, char *, char *);

    /** inode filter function */
    int (*judgeFun) (struct ViceInodeInfo *, afs_uint32, void *);
    int singleVolumeNumber;             /**< volume id filter */
    void * rock;                        /**< pointer passed to writeFun and judgeFun */
    int code;                           /**< return code from examine function */
    int special;                        /**< asserted when this is a volume
					 *   special file */
};

/**
 * simple wrapper around _namei_examine_special and _namei_examine_reg.
 *
 * @param[in] work  the struct listsubdirs_work_node for the associated
 *                  "list subdirs" job
 * @param[in] dir   the directory to examine
 * @param[in] filename  the filename in 'dir' to examine
 *
 * @return operation status
 *   @retval 1  count this inode
 *   @retval 0  don't count this inode
 *   @retval -1 failure
 */
static_inline int
_namei_examine_file(const struct listsubdirs_work_node *work, char *dir,
                    char *filename)
{
    if (work->special) {
	return _namei_examine_special(dir, filename, work->IH,
	                              work->linkHandle, work->writeFun, work->fp,
	                              work->judgeFun, work->singleVolumeNumber,
	                              work->rock);
    } else {
	return _namei_examine_reg(dir, filename, work->IH,
	                          work->linkHandle, work->writeFun, work->fp,
	                          work->judgeFun, work->singleVolumeNumber,
	                          work->rock);
    }
}


#ifdef AFS_SALSRV_ENV
/** @addtogroup afs_vol_salsrv_pario */
/*@{*/

/**
 * arguments for the _namei_examine_file_cbk callback function.
 */
struct listsubdirs_args {
    const struct listsubdirs_work_node *work; /**< arguments that are the same
                                               *   for all invocations of
                                               *   _namei_examine_file_cbk, in
                                               *   threads */
    int *result;        /**< where we can store the return code of _namei_examine_file */

    char dir[512];      /**< directory to examine */
    char filename[256]; /**< filename in 'dir' to examine */
};

/**
 * a node in the list of results of listsubdir jobs.
 */
struct listsubdirs_result {
    struct rx_queue q;
    int inodes;        /**< return value from _namei_examine_file */
};

/**
 * clean up a list of 'struct listsubdirs_result's and interpret the results.
 *
 * @param[in] resultlist  a list of 'struct listsubdirs_result's
 *
 * @return number of inodes found
 *   @retval -1  error
 */
static int
_namei_listsubdirs_cleanup_results(struct rx_queue *resultlist)
{
    struct listsubdirs_result *res, *nres;
    int ret = 0;

    for(queue_Scan(resultlist, res, nres, listsubdirs_result)) {
	if (ret < 0) {
	    /* noop, retain erroneous error code */
	} else if (res->inodes < 0) {
	    ret = -1;
	} else {
	    ret += res->inodes;
	}

	queue_Remove(res);
	free(res);
	res = NULL;
    }

    return ret;
}

/**
 * wait for the spawned listsubdirs jobs to finish, and return how many inodes
 * they found.
 *
 * @param[in] queue    queue to wait to finish
 * @param[in] resultlist list of 'struct listsubdirs_result's that the queued
 *                       jobs are storing their results in
 *
 * @return number of inodes found
 *   @retval -1  error
 */
static int
_namei_listsubdirs_wait(struct afs_work_queue *queue, struct rx_queue *resultlist)
{
    int code;

    code = afs_wq_wait_all(queue);
    if (code) {
	return -1;
    }

    return _namei_listsubdirs_cleanup_results(resultlist);
}

/**
 * work queue entry point for examining namei files.
 *
 * @param[in] queue        pointer to struct Vwork_queue
 * @param[in] node         pointer to struct Vwork_queue_node
 * @param[in] queue_rock   opaque pointer to struct salsrv_pool_state
 * @param[in] node_rock    opaque pointer to struct listsubdirs_work_node
 * @param[in] caller_rock  opaque pointer to struct salsrv_worker_thread_state
 *
 * @return operation status
 *
 * @see Vwork_queue_callback_func_t
 *
 * @internal
 */
static int
_namei_examine_file_cbk(struct afs_work_queue *queue,
                        struct afs_work_queue_node *node,
                        void *queue_rock,
                        void *node_rock,
                        void *caller_rock)
{
    int code;
    struct listsubdirs_args *args = node_rock;
    const struct listsubdirs_work_node * work = args->work;
    char *dir = args->dir;
    char *filename = args->filename;

    code = _namei_examine_file(work, dir, filename);

    *(args->result) = code;

    if (code < 0) {
	*(work->error) = 1;
	/* we've errored, so no point in letting more jobs continue */
	afs_wq_shutdown(queue);
    }

    return 0;
}

static pthread_once_t wq_once = PTHREAD_ONCE_INIT;
static pthread_key_t wq_key;

/**
 * create the wq_key key for storing a work queue.
 */
static void
_namei_wq_keycreate(void)
{
    assert(pthread_key_create(&wq_key, NULL) == 0);
}

/**
 * set the work queue for this thread to use for backgrounding namei ops.
 *
 * The work queue will be used in ListAFSSubdirs (called indirectly by
 * ListViceInodes) to examine files in parallel.
 *
 * @param[in] wq  the work queue to use
 */
void
namei_SetWorkQueue(struct afs_work_queue *wq)
{
    assert(pthread_once(&wq_once, _namei_wq_keycreate) == 0);

    assert(pthread_setspecific(wq_key, wq) == 0);
}

/**
 * enqueue an examine file work unit.
 *
 * @param[in] work     the _namei_examine_file arguments that are common to
 *                     all callers within the same ListAFSFiles operation
 * @param[in] dir      the specific directory to look at (string will be
 *                     copied; can be stack/temporary memory)
 * @param[in] filename the filename to look at (string will be copied; can be
 *                     stack/temporary memory)
 * @param[in] wq       work queue to enqueue this work unit to
 * @param[in] resultlist the list to append the 'struct listsubdirs_result' to
 *                       for the enqueued work unit
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 fatal error
 *
 * @note errors MUST be indicated by a -1 error code and nothing else, to be
 *       compatible with _namei_examine_reg and _namei_examine_special
 *
 * @internal
 */
static int
_namei_examine_file_spawn(const struct listsubdirs_work_node *work,
                          const char *dir, const char *filename,
                          struct afs_work_queue *wq,
                          struct rx_queue *resultlist)
{
    int code, ret = 0;
    struct listsubdirs_args *args = NULL;
    struct listsubdirs_result *result = NULL;
    struct afs_work_queue_node *node = NULL;
    struct afs_work_queue_add_opts opts;

    args = malloc(sizeof(*args));
    if (args == NULL) {
	ret = -1;
	goto error;
    }

    result = malloc(sizeof(*result));
    if (result == NULL) {
	ret = -1;
	goto error;
    }

    code = afs_wq_node_alloc(&node);
    if (code) {
	ret = -1;
	goto error;
    }
    code = afs_wq_node_set_detached(node);
    if (code) {
	ret = -1;
	goto error;
    }

    args->work = work;
    args->result = &result->inodes;
    strlcpy(args->dir, dir, sizeof(args->dir));
    strlcpy(args->filename, filename, sizeof(args->filename));

    code = afs_wq_node_set_callback(node,
					 &_namei_examine_file_cbk,
					 args, &free);
    if (code) {
	ret = -1;
	goto error;
    }
    args = NULL;

    afs_wq_add_opts_init(&opts);
    opts.donate = 1;

    code = afs_wq_add(wq, node, &opts);
    if (code) {
	ret = -1;
	goto error;
    }
    node = NULL;

    queue_Append(resultlist, result);
    result = NULL;

 error:
    if (node) {
	afs_wq_node_put(node);
	node = NULL;
    }
    if (args) {
	free(args);
	args = NULL;
    }
    if (result) {
	free(result);
	result = NULL;
    }

    return ret;
}

/*@}*/
#else /* !AFS_SALSRV_ENV */
# define _namei_examine_file_spawn(work, dir, file, wq, resultlist) \
         _namei_examine_file(work, dir, file)
#endif /* !AFS_SALSRV_ENV */

/**
 * traverse and check inodes.
 *
 * @param[in] dirIH               volume group directory handle
 * @param[in] writeFun            function pointer which will write inode
 *                                metadata to FILE stream fp
 * @param[in] fp                  file stream where inode metadata gets
 *                                written
 * @param[in] judgeFun            inode filter function.  if not NULL, only
 *                                inodes for which the filter returns non-zero
 *                                will be written out by writeFun
 * @param[in] singleVolumeNumber  volume id filter.  only inodes matching this
 *                                filter are written out by writeFun
 * @param[in] rock                opaque pointer passed to judgeFun and writeFun
 *
 * @return operation status
 *    @retval <0 error
 *    @retval >=0 number of matching inodes found
 *
 * @todo the salsrv implementation may consume a lot of
 *       memory for a large volume.  at some point we should
 *       probably write a background thread to asynchronously
 *       clean up the resultlist nodes to reduce memory footprint
 *
 * @internal
 */
static int
namei_ListAFSSubDirs(IHandle_t * dirIH,
		     int (*writeFun) (FILE *, struct ViceInodeInfo *, char *,
				      char *),
		     FILE * fp,
		     int (*judgeFun) (struct ViceInodeInfo *, afs_uint32, void *),
		     afs_uint32 singleVolumeNumber, void *rock)
{
    int code = 0, ret = 0;
    IHandle_t myIH = *dirIH;
    namei_t name;
    char path1[512], path2[512], path3[512];
    DIR *dirp1, *dirp2, *dirp3;
    struct dirent *dp1, *dp2, *dp3;
    FdHandle_t linkHandle;
    int ninodes = 0;
    struct listsubdirs_work_node work;
#ifdef AFS_SALSRV_ENV
    int error = 0;
    struct afs_work_queue *wq;
    int wq_up = 0;
    struct rx_queue resultlist;
#endif
#ifdef DELETE_ZLC
    int i;
    static void AddToZLCDeleteList(char dir, char *name);
    static void DeleteZLCFiles(char *path);
#endif

    namei_HandleToVolDir(&name, &myIH);
    strlcpy(path1, name.n_path, sizeof(path1));

    /* Do the directory containing the special files first to pick up link
     * counts.
     */
    (void)strcat(path1, "/");
    (void)strcat(path1, NAMEI_SPECDIR);

    linkHandle.fd_fd = -1;
#ifdef AFS_SALSRV_ENV
    assert(pthread_once(&wq_once, _namei_wq_keycreate) == 0);

    wq = pthread_getspecific(wq_key);
    if (!wq) {
	ret = -1;
	goto error;
    }
    wq_up = 1;
    queue_Init(&resultlist);
#endif

    memset(&work, 0, sizeof(work));
    work.linkHandle = &linkHandle;
    work.IH = &myIH;
    work.fp = fp;
    work.writeFun = writeFun;
    work.judgeFun = judgeFun;
    work.singleVolumeNumber = singleVolumeNumber;
    work.rock = rock;
    work.special = 1;
#ifdef AFS_SALSRV_ENV
    work.error = &error;
#endif

    dirp1 = opendir(path1);
    if (dirp1) {
	while ((dp1 = readdir(dirp1))) {
	    if (*dp1->d_name == '.')
		continue;

#ifdef AFS_SALSRV_ENV
	    if (error) {
		closedir(dirp1);
		ret = -1;
		goto error;
	    }
#endif /* AFS_SALSRV_ENV */

	    code = _namei_examine_file_spawn(&work, path1, dp1->d_name, wq, &resultlist);

	    switch (code) {
	    case -1:
		/* fatal error */
		closedir(dirp1);
		ret = -1;
		goto error;

	    case 1:
		/* count this inode */
#ifndef AFS_SALSRV_ENV
		ninodes++;
#endif
		break;
	    }
	}
	closedir(dirp1);
    }

#ifdef AFS_SALSRV_ENV
    /* effectively a barrier */
    code = _namei_listsubdirs_wait(wq, &resultlist);
    if (code < 0 || error) {
	ret = -1;
	goto error;
    }
    error = 0;
    ninodes += code;
#endif

    /* Now run through all the other subdirs */
    namei_HandleToVolDir(&name, &myIH);
    strlcpy(path1, name.n_path, sizeof(path1));

    work.special = 0;

    dirp1 = opendir(path1);
    if (dirp1) {
	while ((dp1 = readdir(dirp1))) {
	    if (*dp1->d_name == '.')
		continue;
	    if (!strcmp(dp1->d_name, NAMEI_SPECDIR))
		continue;

	    /* Now we've got a next level subdir. */
	    afs_snprintf(path2, sizeof(path2), "%s/%s", path1, dp1->d_name);
	    dirp2 = opendir(path2);
	    if (dirp2) {
		while ((dp2 = readdir(dirp2))) {
		    if (*dp2->d_name == '.')
			continue;

		    /* Now we've got to the actual data */
		    afs_snprintf(path3, sizeof(path3), "%s/%s", path2,
				 dp2->d_name);

		    dirp3 = opendir(path3);
		    if (dirp3) {
			while ((dp3 = readdir(dirp3))) {
			    if (*dp3->d_name == '.')
				continue;

#ifdef AFS_SALSRV_ENV
			    if (error) {
				closedir(dirp3);
				closedir(dirp2);
				closedir(dirp1);
				ret = -1;
				goto error;
			    }
#endif /* AFS_SALSRV_ENV */

			    code = _namei_examine_file_spawn(&work, path3,
			                                     dp3->d_name, wq,
			                                     &resultlist);

			    switch (code) {
			    case -1:
				closedir(dirp3);
				closedir(dirp2);
				closedir(dirp1);
				ret = -1;
				goto error;

			    case 1:
#ifndef AFS_SALSRV_ENV
				ninodes++;
#endif
				break;
			    }
			}
			closedir(dirp3);
		    }
		}
		closedir(dirp2);
	    }
	}
	closedir(dirp1);
    }

#ifdef AFS_SALSRV_ENV
    /* effectively a barrier */
    code = _namei_listsubdirs_wait(wq, &resultlist);
    if (code < 0 || error) {
	ret = -1;
	goto error;
    }
    error = 0;
    ninodes += code;
    wq_up = 0;
#endif

    if (!ninodes) {
	/* Then why does this directory exist? Blow it away. */
	namei_HandleToVolDir(&name, dirIH);
	namei_RemoveDataDirectories(&name);
    }

 error:
#ifdef AFS_SALSRV_ENV
    if (wq_up) {
	afs_wq_wait_all(wq);
    }
    _namei_listsubdirs_cleanup_results(&resultlist);
#endif
    if (linkHandle.fd_fd >= 0)
	close(linkHandle.fd_fd);

    if (!ret) {
	ret = ninodes;
    }
    return ret;
}

/*@}*/

static int
DecodeVolumeName(char *name, unsigned int *vid)
{
    if (strlen(name) < 1)
	return -1;
    *vid = (unsigned int)flipbase64_to_int64(name);
    return 0;
}


/* DecodeInode
 *
 * Get the inode number from the name.
 * Get
 */
static int
DecodeInode(char *dpath, char *name, struct ViceInodeInfo *info,
	    unsigned int volid)
{
    char fpath[512];
    struct afs_stat status;
    int parm, tag;
    lb64_string_t check;

    afs_snprintf(fpath, sizeof(fpath), "%s/%s", dpath, name);

    if (afs_stat(fpath, &status) < 0) {
	return -1;
    }

    info->byteCount = status.st_size;
    info->inodeNumber = (Inode) flipbase64_to_int64(name);

    int64_to_flipbase64(check, info->inodeNumber);
    if (strcmp(name, check))
	return -1;

    GetOGMFromStat(&status, &parm, &tag);
    if ((info->inodeNumber & NAMEI_INODESPECIAL) == NAMEI_INODESPECIAL) {
	/* p1 - vid, p2 - -1, p3 - type, p4 - rwvid */
	info->u.param[0] = parm;
	info->u.param[1] = -1;
	info->u.param[2] = tag;
	info->u.param[3] = volid;
    } else {
	/* p1 - vid, p2 - vno, p3 - uniq, p4 - dv */
	info->u.param[0] = volid;
	info->u.param[1] = (int)(info->inodeNumber & NAMEI_VNODEMASK);
	info->u.param[2] = (int)((info->inodeNumber >> NAMEI_UNIQSHIFT)
				 & (Inode) NAMEI_UNIQMASK);
	info->u.param[3] = parm;
    }
    return 0;
}

/*
 * Convert the VolumeInfo file from RO to RW
 * this routine is called by namei_convertROtoRWvolume()
 */

#ifdef FSSYNC_BUILD_CLIENT
static afs_int32
convertVolumeInfo(int fdr, int fdw, afs_uint32 vid)
{
    struct VolumeDiskData vd;
    char *p;

    if (read(fdr, &vd, sizeof(struct VolumeDiskData)) !=
	sizeof(struct VolumeDiskData)) {
	Log("1 convertVolumeInfo: read failed for %lu with code %d\n",
	    afs_printable_uint32_lu(vid),
	    errno);
	return -1;
    }
    vd.restoredFromId = vd.id;	/* remember the RO volume here */
    vd.cloneId = vd.id;
    vd.id = vd.parentId;
    vd.type = RWVOL;
    vd.dontSalvage = 0;
    vd.inUse = 0;
    vd.uniquifier += 5000;	/* just in case there are still file copies from
				 * the old RW volume around */
    p = strrchr(vd.name, '.');
    if (p && !strcmp(p, ".readonly")) {
	memset(p, 0, 9);
    }
    if (write(fdw, &vd, sizeof(struct VolumeDiskData)) !=
	sizeof(struct VolumeDiskData)) {
	Log("1 convertVolumeInfo: write failed for %lu with code %d\n",
	    afs_printable_uint32_lu(vid),
	    errno);
	return -1;
    }
    return 0;
}
#endif

/*
 * Convert a RO-volume into a RW-volume
 *
 * This function allows to recover very fast from the loss of a partition
 * from RO-copies if all RO-Copies exist on another partition.
 * Then these RO-volumes can be made to the new RW-volumes.
 * Backup of RW-volumes then consists in "vos release".
 *
 * We must make sure in this partition exists only the RO-volume which
 * is typical for remote replicas.
 *
 * Then the linktable is already ok,
 *      the vnode files need to be renamed
 *      the volinfo file needs to be replaced by another one with
 *                      slightly different contents and new name.
 * The volume header file of the RO-volume in the /vicep<x> directory
 * is destroyed by this call. A new header file for the RW-volume must
 * be created after return from this routine.
 */

int
namei_ConvertROtoRWvolume(char *pname, afs_uint32 volumeId)
{
    int code = 0;
#ifdef FSSYNC_BUILD_CLIENT
    namei_t n;
    char dir_name[512], oldpath[512], newpath[512];
    char smallName[64];
    char largeName[64];
    char infoName[64];
    IHandle_t t_ih;
    IHandle_t *ih;
    char infoSeen = 0;
    char smallSeen = 0;
    char largeSeen = 0;
    char linkSeen = 0;
    int fd, fd2;
    char *p;
    DIR *dirp;
    Inode ino;
    struct dirent *dp;
    struct DiskPartition64 *partP;
    struct ViceInodeInfo info;
    struct VolumeDiskHeader h;
# ifdef AFS_DEMAND_ATTACH_FS
    int locktype = 0;
# endif /* AFS_DEMAND_ATTACH_FS */

    for (partP = DiskPartitionList; partP && strcmp(partP->name, pname);
         partP = partP->next);
    if (!partP) {
        Log("1 namei_ConvertROtoRWvolume: Couldn't find DiskPartition for %s\n", pname);
	code = EIO;
	goto done;
    }

# ifdef AFS_DEMAND_ATTACH_FS
    locktype = VVolLockType(V_VOLUPD, 1);
    code = VLockVolumeByIdNB(volumeId, partP, locktype);
    if (code) {
	locktype = 0;
	code = EIO;
	goto done;
    }
# endif /* AFS_DEMAND_ATTACH_FS */

    if (VReadVolumeDiskHeader(volumeId, partP, &h)) {
	Log("1 namei_ConvertROtoRWvolume: Couldn't read header for RO-volume %lu.\n",
	    afs_printable_uint32_lu(volumeId));
	code = EIO;
	goto done;
    }

    FSYNC_VolOp(volumeId, pname, FSYNC_VOL_BREAKCBKS, 0, NULL);

    ino = namei_MakeSpecIno(h.parent, VI_LINKTABLE);
    IH_INIT(ih, partP->device, h.parent, ino);

    namei_HandleToName(&n, ih);
    strlcpy(dir_name, n.n_path, sizeof(dir_name));
    p = strrchr(dir_name, '/');
    *p = 0;
    dirp = opendir(dir_name);
    if (!dirp) {
	Log("1 namei_ConvertROtoRWvolume: Could not opendir(%s)\n", dir_name);
	code = EIO;
	goto done;
    }

    while ((dp = readdir(dirp))) {
	/* struct ViceInodeInfo info; */

	if (*dp->d_name == '.')
	    continue;
	if (DecodeInode(dir_name, dp->d_name, &info, ih->ih_vid) < 0) {
	    Log("1 namei_ConvertROtoRWvolume: DecodeInode failed for %s/%s\n",
		dir_name, dp->d_name);
	    closedir(dirp);
	    code = -1;
	    goto done;
	}
	if (info.u.param[1] != -1) {
	    Log("1 namei_ConvertROtoRWvolume: found other than volume special file %s/%s\n", dir_name, dp->d_name);
	    closedir(dirp);
	    code = -1;
	    goto done;
	}
	if (info.u.param[0] != volumeId) {
	    if (info.u.param[0] == ih->ih_vid) {
		if (info.u.param[2] == VI_LINKTABLE) {	/* link table */
		    linkSeen = 1;
		    continue;
		}
	    }
	    Log("1 namei_ConvertROtoRWvolume: found special file %s/%s"
		" for volume %lu\n", dir_name, dp->d_name,
		afs_printable_uint32_lu(info.u.param[0]));
	    closedir(dirp);
	    code = VVOLEXISTS;
	    goto done;
	}
	if (info.u.param[2] == VI_VOLINFO) {	/* volume info file */
	    strlcpy(infoName, dp->d_name, sizeof(infoName));
	    infoSeen = 1;
	} else if (info.u.param[2] == VI_SMALLINDEX) {	/* small vnodes file */
	    strlcpy(smallName, dp->d_name, sizeof(smallName));
	    smallSeen = 1;
	} else if (info.u.param[2] == VI_LARGEINDEX) {	/* large vnodes file */
	    strlcpy(largeName, dp->d_name, sizeof(largeName));
	    largeSeen = 1;
	} else {
	    closedir(dirp);
	    Log("1 namei_ConvertROtoRWvolume: unknown type %d of special file found : %s/%s\n", info.u.param[2], dir_name, dp->d_name);
	    code = -1;
	    goto done;
	}
    }
    closedir(dirp);

    if (!infoSeen || !smallSeen || !largeSeen || !linkSeen) {
	Log("1 namei_ConvertROtoRWvolume: not all special files found in %s\n", dir_name);
	code = -1;
	goto done;
    }

    /*
     * If we come here then there was only a RO-volume and we can safely
     * proceed.
     */

    memset(&t_ih, 0, sizeof(t_ih));
    t_ih.ih_dev = ih->ih_dev;
    t_ih.ih_vid = ih->ih_vid;

    (void)afs_snprintf(oldpath, sizeof oldpath, "%s/%s", dir_name, infoName);
    fd = afs_open(oldpath, O_RDWR, 0);
    if (fd < 0) {
	Log("1 namei_ConvertROtoRWvolume: could not open RO info file: %s\n",
	    oldpath);
	code = -1;
	goto done;
    }
    t_ih.ih_ino = namei_MakeSpecIno(ih->ih_vid, VI_VOLINFO);
    namei_HandleToName(&n, &t_ih);
    fd2 = afs_open(n.n_path, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0);
    if (fd2 < 0) {
	Log("1 namei_ConvertROtoRWvolume: could not create RW info file: %s\n", n.n_path);
	close(fd);
	code = -1;
	goto done;
    }
    code = convertVolumeInfo(fd, fd2, ih->ih_vid);
    close(fd);
    if (code) {
	close(fd2);
	unlink(n.n_path);
	code = -1;
	goto done;
    }
    SetOGM(fd2, ih->ih_vid, 1);
    close(fd2);

    t_ih.ih_ino = namei_MakeSpecIno(ih->ih_vid, VI_SMALLINDEX);
    namei_HandleToName(&n, &t_ih);
    (void)afs_snprintf(newpath, sizeof newpath, "%s/%s", dir_name, smallName);
    fd = afs_open(newpath, O_RDWR, 0);
    if (fd < 0) {
	Log("1 namei_ConvertROtoRWvolume: could not open SmallIndex file: %s\n", newpath);
	code = -1;
	goto done;
    }
    SetOGM(fd, ih->ih_vid, 2);
    close(fd);
    link(newpath, n.n_path);
    unlink(newpath);

    t_ih.ih_ino = namei_MakeSpecIno(ih->ih_vid, VI_LARGEINDEX);
    namei_HandleToName(&n, &t_ih);
    (void)afs_snprintf(newpath, sizeof newpath, "%s/%s", dir_name, largeName);
    fd = afs_open(newpath, O_RDWR, 0);
    if (fd < 0) {
	Log("1 namei_ConvertROtoRWvolume: could not open LargeIndex file: %s\n", newpath);
	code = -1;
	goto done;
    }
    SetOGM(fd, ih->ih_vid, 3);
    close(fd);
    link(newpath, n.n_path);
    unlink(newpath);

    unlink(oldpath);

    h.id = h.parent;
    h.volumeInfo_hi = h.id;
    h.smallVnodeIndex_hi = h.id;
    h.largeVnodeIndex_hi = h.id;
    h.linkTable_hi = h.id;

    if (VCreateVolumeDiskHeader(&h, partP)) {
        Log("1 namei_ConvertROtoRWvolume: Couldn't write header for RW-volume %lu\n",
	    afs_printable_uint32_lu(h.id));
	code = EIO;
	goto done;
    }

    if (VDestroyVolumeDiskHeader(partP, volumeId, h.parent)) {
        Log("1 namei_ConvertROtoRWvolume: Couldn't unlink header for RO-volume %lu\n",
	    afs_printable_uint32_lu(volumeId));
    }

    FSYNC_VolOp(volumeId, pname, FSYNC_VOL_DONE, 0, NULL);
    FSYNC_VolOp(h.id, pname, FSYNC_VOL_ON, 0, NULL);

 done:
# ifdef AFS_DEMAND_ATTACH_FS
    if (locktype) {
	VUnlockVolumeById(volumeId, partP);
    }
# endif /* AFS_DEMAND_ATTACH_FS */
#endif

    return code;
}

/* PrintInode
 *
 * returns a static string used to print either 32 or 64 bit inode numbers.
 */
char *
PrintInode(char *s, Inode ino)
{
    static afs_ino_str_t result;
    if (!s)
	s = result;

    (void)afs_snprintf(s, sizeof(afs_ino_str_t), "%llu", (afs_uintmax_t) ino);

    return s;
}


#ifdef DELETE_ZLC
/* Routines to facilitate removing zero link count files. */
#define MAX_ZLC_NAMES 32
#define MAX_ZLC_NAMELEN 16
typedef struct zlcList_s {
    struct zlcList_s *zlc_next;
    int zlc_n;
    char zlc_names[MAX_ZLC_NAMES][MAX_ZLC_NAMELEN];
} zlcList_t;

static zlcList_t *zlcAnchor = NULL;
static zlcList_t *zlcCur = NULL;

static void
AddToZLCDeleteList(char dir, char *name)
{
    assert(strlen(name) <= MAX_ZLC_NAMELEN - 3);

    if (!zlcCur || zlcCur->zlc_n >= MAX_ZLC_NAMES) {
	if (zlcCur && zlcCur->zlc_next)
	    zlcCur = zlcCur->zlc_next;
	else {
	    zlcList_t *tmp = (zlcList_t *) malloc(sizeof(zlcList_t));
	    if (!tmp)
		return;
	    if (!zlcAnchor) {
		zlcAnchor = tmp;
	    } else {
		zlcCur->zlc_next = tmp;
	    }
	    zlcCur = tmp;
	    zlcCur->zlc_n = 0;
	    zlcCur->zlc_next = NULL;
	}
    }

    (void)sprintf(zlcCur->zlc_names[zlcCur->zlc_n], "%c\\%s", dir, name);
    zlcCur->zlc_n++;
}

static void
DeleteZLCFiles(char *path)
{
    zlcList_t *z;
    int i;
    char fname[1024];

    for (z = zlcAnchor; z; z = z->zlc_next) {
	for (i = 0; i < z->zlc_n; i++) {
	    (void)sprintf(fname, "%s\\%s", path, z->zlc_names[i]);
	    if (namei_unlink(fname) < 0) {
		Log("ZLC: Can't unlink %s, dos error = %d\n", fname,
		    GetLastError());
	    }
	}
	z->zlc_n = 0;		/* Can reuse space. */
    }
    zlcCur = zlcAnchor;
}

static void
FreeZLCList(void)
{
    zlcList_t *tnext;
    zlcList_t *i;

    i = zlcAnchor;
    while (i) {
	tnext = i->zlc_next;
	free(i);
	i = tnext;
    }
    zlcCur = zlcAnchor = NULL;
}
#endif

#endif /* AFS_NAMEI_ENV */
