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
	Module:		clone.c

 */

/* Clone a volume.  Assumes the new volume is already created */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <stdio.h>
#include <afs/afs_assert.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <windows.h>
#include <winbase.h>
#include <io.h>
#include <time.h>
#else
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include "viceinode.h"
#include "vol_prototypes.h"
#include "common.h"

int (*vol_PollProc) (void) = 0;	/* someone must init this */

#define ERROR_EXIT(code) do { \
    error = code; \
    goto error_exit; \
} while (0)

/* parameters for idec call - this could just be an IHandle_t, but leaving
 * open the possibility of decrementing the special files as well.
 */
struct clone_rock {
    IHandle_t *h;
    VolId vol;
};

#define CLONE_MAXITEMS	100
struct clone_items {
    struct clone_items *next;
    afs_int32 nitems;
    Inode data[CLONE_MAXITEMS];
};

struct clone_head {
    struct clone_items *first;
    struct clone_items *last;
};

void CloneVolume(Error *, Volume *, Volume *, Volume *);

static int
ci_AddItem(struct clone_head *ah, Inode aino)
{
    struct clone_items *ti;

    /* if no last elt (first call) or last item full, get a new one */
    if ((!ah->last) || ah->last->nitems >= CLONE_MAXITEMS) {
	ti = (struct clone_items *)malloc(sizeof(struct clone_items));
	if (!ti) {
	    Log("ci_AddItem: malloc failed\n");
	    osi_Panic("ci_AddItem: malloc failed\n");
	}
	ti->nitems = 0;
	ti->next = (struct clone_items *)0;
	if (ah->last) {
	    ah->last->next = ti;
	    ah->last = ti;
	} else {
	    /* first dude in the list */
	    ah->first = ah->last = ti;
	}
    } else
	ti = ah->last;

    /* now ti points to the end of the list, to a clone_item with room
     * for at least one more element.  Add it.
     */
    ti->data[ti->nitems++] = aino;
    return 0;
}

/* initialize a clone header */
int
ci_InitHead(struct clone_head *ah)
{
    memset(ah, 0, sizeof(*ah));
    return 0;
}

/* apply a function to all dudes in the set */
int
ci_Apply(struct clone_head *ah, int (*aproc) (Inode,  void *), void *arock)
{
    struct clone_items *ti;
    int i;

    for (ti = ah->first; ti; ti = ti->next) {
	for (i = 0; i < ti->nitems; i++) {
	    (*aproc) (ti->data[i], arock);
	}
    }
    return 0;
}

/* free all dudes in the list */
int
ci_Destroy(struct clone_head *ah)
{
    struct clone_items *ti, *ni;

    for (ti = ah->first; ti; ti = ni) {
	ni = ti->next;		/* guard against freeing */
	free(ti);
    }
    return 0;
}

static int
IDecProc(Inode adata, void *arock)
{
    struct clone_rock *aparm = (struct clone_rock *)arock;
    IH_DEC(aparm->h, adata, aparm->vol);
    DOPOLL;
    return 0;
}

afs_int32
DoCloneIndex(Volume * rwvp, Volume * clvp, VnodeClass class, int reclone)
{
    afs_int32 code, error = 0;
    FdHandle_t *rwFd = 0, *clFdIn = 0, *clFdOut = 0;
    StreamHandle_t *rwfile = 0, *clfilein = 0, *clfileout = 0;
    IHandle_t *rwH = 0, *clHin = 0, *clHout = 0;
    char buf[SIZEOF_LARGEDISKVNODE], dbuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *rwvnode = (struct VnodeDiskObject *)buf;
    struct VnodeDiskObject *clvnode = (struct VnodeDiskObject *)dbuf;
    Inode rwinode = 0;
    Inode clinode;
    struct clone_head decHead;
    struct clone_rock decRock;
    afs_foff_t offset = 0;
    afs_int32 dircloned, inodeinced;
    afs_int32 filecount = 0, diskused = 0;
    afs_ino_str_t stmp;

    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    /*
     * The fileserver's -readonly switch should make this false, but we
     * have no useful way to know in the volserver.
     * This doesn't make client data mutable.
     */
    int ReadWriteOriginal = 1;

    /* Correct number of files in volume: this assumes indexes are always
       cloned starting with vLarge */
    if (ReadWriteOriginal && class != vLarge) {
	filecount = V_filecount(rwvp);
	diskused = V_diskused(rwvp);
    }

    /* Initialize list of inodes to nuke - must do this before any calls
     * to ERROR_EXIT, as the error handler requires an initialised list
     */
    ci_InitHead(&decHead);
    decRock.h = V_linkHandle(rwvp);
    decRock.vol = V_parentId(rwvp);

    /* Open the RW volume's index file and seek to beginning */
    IH_COPY(rwH, rwvp->vnodeIndex[class].handle);
    rwFd = IH_OPEN(rwH);
    if (!rwFd)
	ERROR_EXIT(EIO);
    rwfile = FDH_FDOPEN(rwFd, ReadWriteOriginal ? "r+" : "r");
    if (!rwfile)
	ERROR_EXIT(EIO);
    STREAM_ASEEK(rwfile, vcp->diskSize);	/* Will fail if no vnodes */

    /* Open the clone volume's index file and seek to beginning */
    IH_COPY(clHout, clvp->vnodeIndex[class].handle);
    clFdOut = IH_OPEN(clHout);
    if (!clFdOut)
	ERROR_EXIT(EIO);
    clfileout = FDH_FDOPEN(clFdOut, "a");
    if (!clfileout)
	ERROR_EXIT(EIO);
    code = STREAM_ASEEK(clfileout, vcp->diskSize);
    if (code)
	ERROR_EXIT(EIO);

    /* If recloning, open the new volume's index; this time for
     * reading. We never read anything that we're simultaneously
     * writing, so this all works.
     */
    if (reclone) {
	IH_COPY(clHin, clvp->vnodeIndex[class].handle);
	clFdIn = IH_OPEN(clHin);
	if (!clFdIn)
	    ERROR_EXIT(EIO);
	clfilein = FDH_FDOPEN(clFdIn, "r");
	if (!clfilein)
	    ERROR_EXIT(EIO);
	STREAM_ASEEK(clfilein, vcp->diskSize);	/* Will fail if no vnodes */
    }

    /* Read each vnode in the old volume's index file */
    for (offset = vcp->diskSize;
	 STREAM_READ(rwvnode, vcp->diskSize, 1, rwfile) == 1;
	 offset += vcp->diskSize) {
	dircloned = inodeinced = 0;

	/* If we are recloning the volume, read the corresponding vnode
	 * from the clone and determine its inode number.
	 */
	if (reclone && !STREAM_EOF(clfilein)
	    && (STREAM_READ(clvnode, vcp->diskSize, 1, clfilein) == 1)) {
	    clinode = VNDISK_GET_INO(clvnode);
	} else {
	    clinode = 0;
	}

	if (rwvnode->type != vNull) {
	    afs_fsize_t ll;

	    if (rwvnode->vnodeMagic != vcp->magic)
		ERROR_EXIT(-1);
	    rwinode = VNDISK_GET_INO(rwvnode);
	    filecount++;
	    VNDISK_GET_LEN(ll, rwvnode);
	    diskused += nBlocks(ll);

	    /* Increment the inode if not already */
	    if (clinode && (clinode == rwinode)) {
		clinode = 0;	/* already cloned - don't delete later */
	    } else if (rwinode) {
		if (IH_INC(V_linkHandle(rwvp), rwinode, V_parentId(rwvp)) ==
		    -1) {
		    Log("IH_INC failed: %"AFS_PTR_FMT", %s, %u errno %d\n",
			V_linkHandle(rwvp), PrintInode(stmp, rwinode),
			V_parentId(rwvp), errno);
		    VForceOffline(rwvp);
		    ERROR_EXIT(EIO);
		}
		inodeinced = 1;
	    }

	    /* If a directory, mark vnode in old volume as cloned */
	    if ((rwvnode->type == vDirectory) && ReadWriteOriginal) {
#ifdef DVINC
		/*
		 * It is my firmly held belief that immediately after
		 * copy-on-write, the two directories can be identical.
		 * If the new copy is changed (presumably, that is the very
		 * next thing that will happen) then the dataVersion will
		 * get bumped.
		 */
		/* NOTE:  the dataVersion++ is incredibly important!!!.
		 * This will cause the inode created by the file server
		 * on copy-on-write to be stamped with a dataVersion bigger
		 * than the current one.  The salvager will then do the
		 * right thing */
		rwvnode->dataVersion++;
#endif /* DVINC */
		rwvnode->cloned = 1;
		code = STREAM_ASEEK(rwfile, offset);
		if (code == -1)
		    goto clonefailed;
		code = STREAM_WRITE(rwvnode, vcp->diskSize, 1, rwfile);
		if (code != 1)
		    goto clonefailed;
		dircloned = 1;
		code = STREAM_ASEEK(rwfile, offset + vcp->diskSize);
		if (code == -1)
		    goto clonefailed;
#ifdef DVINC
		rwvnode->dataVersion--;	/* Really needs to be set to the value in the inode,
					 * for the read-only volume */
#endif /* DVINC */
	    }
	}

	/* Overwrite the vnode entry in the clone volume */
	rwvnode->cloned = 0;
	code = STREAM_WRITE(rwvnode, vcp->diskSize, 1, clfileout);
	if (code != 1) {
	  clonefailed:
	    /* Couldn't clone, go back and decrement the inode's link count */
	    if (inodeinced) {
		if (IH_DEC(V_linkHandle(rwvp), rwinode, V_parentId(rwvp)) ==
		    -1) {
		    Log("IH_DEC failed: %"AFS_PTR_FMT", %s, %u errno %d\n",
			V_linkHandle(rwvp), PrintInode(stmp, rwinode),
			V_parentId(rwvp), errno);
		    VForceOffline(rwvp);
		    ERROR_EXIT(EIO);
		}
	    }
	    /* And if the directory was marked clone, unmark it */
	    if (dircloned) {
		rwvnode->cloned = 0;
		if (STREAM_ASEEK(rwfile, offset) != -1)
		    (void)STREAM_WRITE(rwvnode, vcp->diskSize, 1, rwfile);
	    }
	    ERROR_EXIT(EIO);
	}

	/* Removal of the old cloned inode */
	if (clinode) {
	    ci_AddItem(&decHead, clinode);	/* just queue it */
	}

	DOPOLL;
    }
    if (STREAM_ERROR(clfileout))
	ERROR_EXIT(EIO);

    /* Clean out any junk at end of clone file */
    if (reclone) {
	STREAM_ASEEK(clfilein, offset);
	while (STREAM_READ(clvnode, vcp->diskSize, 1, clfilein) == 1) {
	    if (clvnode->type != vNull && VNDISK_GET_INO(clvnode) != 0) {
		ci_AddItem(&decHead, VNDISK_GET_INO(clvnode));
	    }
	    DOPOLL;
	}
    }

    /* come here to finish up.  If code is non-zero, we've already run into problems,
     * and shouldn't do the idecs.
     */
  error_exit:
    if (rwfile)
	STREAM_CLOSE(rwfile);
    if (clfilein)
	STREAM_CLOSE(clfilein);
    if (clfileout)
	STREAM_CLOSE(clfileout);

    if (rwFd)
	FDH_CLOSE(rwFd);
    if (clFdIn)
	FDH_CLOSE(clFdIn);
    if (clFdOut)
	FDH_CLOSE(clFdOut);

    if (rwH)
	IH_RELEASE(rwH);
    if (clHout)
	IH_RELEASE(clHout);
    if (clHin)
	IH_RELEASE(clHin);

    /* Next, we sync the disk. We have to reopen in case we're truncating,
     * since we were using stdio above, and don't know when the buffers
     * would otherwise be flushed.  There's no stdio fftruncate call.
     */
    rwFd = IH_OPEN(clvp->vnodeIndex[class].handle);
    if (rwFd == NULL) {
	if (!error)
	    error = EIO;
    } else {
	if (reclone) {
	    /* If doing a reclone, we're keeping the clone. We need to
	     * truncate the file to offset bytes.
	     */
	    if (reclone && !error) {
		error = FDH_TRUNC(rwFd, offset);
	    }
	}
	FDH_SYNC(rwFd);
	FDH_CLOSE(rwFd);
    }

    /* Now finally do the idec's.  At this point, all potential
     * references have been cleaned up and sent to the disk
     * (see above fclose and fsync). No matter what happens, we
     * no longer need to keep these references around.
     */
    code = ci_Apply(&decHead, IDecProc, (char *)&decRock);
    if (!error)
	error = code;
    ci_Destroy(&decHead);

    if (ReadWriteOriginal && filecount > 0)
	V_filecount(rwvp) = filecount;
    if (ReadWriteOriginal && diskused > 0)
	V_diskused(rwvp) = diskused;
    return error;
}

void
CloneVolume(Error * rerror, Volume * original, Volume * new, Volume * old)
{
    afs_int32 code, error = 0;
    afs_int32 reclone;
    afs_int32 filecount = V_filecount(original), diskused = V_diskused(original);

    *rerror = 0;
    reclone = ((new == old) ? 1 : 0);

    code = DoCloneIndex(original, new, vLarge, reclone);
    if (code)
	ERROR_EXIT(code);
    code = DoCloneIndex(original, new, vSmall, reclone);
    if (code)
	ERROR_EXIT(code);
    if (filecount != V_filecount(original) || diskused != V_diskused(original))
       Log("Clone %u: filecount %d -> %d diskused %d -> %d\n",
	    V_id(original), filecount, V_filecount(original), diskused, V_diskused(original));

    code = CopyVolumeHeader(&V_disk(original), &V_disk(new));
    if (code)
	ERROR_EXIT(code);

  error_exit:
    *rerror = error;
}
