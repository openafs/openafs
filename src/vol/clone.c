/*
 * For copyright information, see IPL which you accepted in order to
 * download this software.
 *
 */
/*

	System:		VICE-TWO
	Module:		clone.c

 */

/* Clone a volume.  Assumes the new volume is already created */

#include <afs/param.h>
#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <windows.h>
#include <winbase.h>
#include <io.h>
#include <time.h>
#else
#include <sys/file.h>
#include <sys/time.h>
#endif
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

int (*vol_PollProc)() =	0;  /* someone must init this */

#define ERROR_EXIT(code) {error = code; goto error_exit;}

/* parameters for idec call - this could just be an IHandle_t, but leaving
 * open the possibility of decrementing the special files as well.
 */
struct clone_rock {
    IHandle_t *h;
    afs_int32 vol;
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

static ci_AddItem(ah, aino)
Inode aino;
struct clone_head *ah; {
    register struct clone_items *ti;

    /* if no last elt (first call) or last item full, get a new one */
    if ((!ah->last) || ah->last->nitems >= CLONE_MAXITEMS) {
	ti = (struct clone_items *) malloc(sizeof(struct clone_items));
	ti->nitems = 0;
	ti->next = (struct clone_items *) 0;
	if (ah->last) {
	    ah->last->next = ti;
	    ah->last = ti;
	}
	else {
	    /* first dude in the list */
	    ah->first = ah->last = ti;
	}
    }
    else ti = ah->last;

    /* now ti points to the end of the list, to a clone_item with room
     * for at least one more element.  Add it.
     */
    ti->data[ti->nitems++] = aino;
    return 0;
}

/* initialize a clone header */
int ci_InitHead(struct clone_head *ah)
{
    bzero(ah, sizeof(*ah));
    return 0;
}

/* apply a function to all dudes in the set */
int ci_Apply(struct clone_head *ah, int (*aproc)(), char *arock)
{
    register struct clone_items *ti;
    register int i;

    for(ti=ah->first; ti; ti=ti->next) {
	for(i=0; i<ti->nitems; i++) {
	    (*aproc)(ti->data[i], arock);
	}
    }
    return 0;
}

/* free all dudes in the list */
int ci_Destroy(struct clone_head *ah)
{
    register struct clone_items *ti, *ni;

    for(ti=ah->first; ti; ti=ni) {
	ni = ti->next;	/* guard against freeing */
	free(ti);
    }
    return 0;
}

static IDecProc(adata, aparm)
Inode adata;
struct clone_rock *aparm; {
    IH_DEC(aparm->h, adata, aparm->vol);
    DOPOLL;
    return 0;
}

afs_int32 DoCloneIndex(rwvp, clvp, class, reclone)
   Volume     *rwvp;     /* The RW volume */
   Volume     *clvp;     /* The cloned volume */
   VnodeClass class;
   int        reclone;   /* Whether to reclone or not */
{
   afs_int32 code, error=0;
   FdHandle_t     *rwFd=0,   *clFdIn=0,   *clFdOut=0;
   StreamHandle_t *rwfile=0, *clfilein=0, *clfileout=0;
   IHandle_t      *rwH=0,    *clHin=0,    *clHout=0;
   char buf[SIZEOF_LARGEDISKVNODE], dbuf[SIZEOF_LARGEDISKVNODE];
   struct VnodeDiskObject *rwvnode = (struct VnodeDiskObject *) buf;
   struct VnodeDiskObject *clvnode = (struct VnodeDiskObject *) dbuf;
   Inode rwinode, clinode;
   struct clone_head decHead;
   struct clone_rock decRock;
   afs_int32 offset, dircloned, inodeinced;

   struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
   int ReadWriteOriginal = VolumeWriteable(rwvp);
   struct DiskPartition *partition = rwvp->partition;
   Device device = rwvp->device;

   /* Open the RW volume's index file and seek to beginning */
   IH_COPY(rwH, rwvp->vnodeIndex[class].handle);
   rwFd = IH_OPEN(rwH);
   if (!rwFd) ERROR_EXIT(EIO);
   rwfile = FDH_FDOPEN(rwFd, ReadWriteOriginal? "r+":"r");
   if (!rwfile) ERROR_EXIT(EIO);
   STREAM_SEEK(rwfile, vcp->diskSize, 0); /* Will fail if no vnodes */
	
   /* Open the clone volume's index file and seek to beginning */
   IH_COPY(clHout, clvp->vnodeIndex[class].handle);
   clFdOut = IH_OPEN(clHout);
   if (!clFdOut) ERROR_EXIT(EIO);
   clfileout = FDH_FDOPEN(clFdOut, "a");
   if (!clfileout) ERROR_EXIT(EIO);
   code = STREAM_SEEK(clfileout, vcp->diskSize, 0);
   if (code) ERROR_EXIT(EIO);

   /* If recloning, open the new volume's index; this time for
    * reading. We never read anything that we're simultaneously
    * writing, so this all works.
    */
   if (reclone) {
      IH_COPY(clHin, clvp->vnodeIndex[class].handle);
      clFdIn = IH_OPEN(clHin);
      if (!clFdIn) ERROR_EXIT(EIO);
      clfilein = FDH_FDOPEN(clFdIn, "r");
      if (!clfilein) ERROR_EXIT(EIO);
      STREAM_SEEK(clfilein, vcp->diskSize, 0); /* Will fail if no vnodes */
   }

   /* Initialize list of inodes to nuke */
   ci_InitHead(&decHead);
   decRock.h = V_linkHandle(rwvp);
   decRock.vol = V_parentId(rwvp);

   /* Read each vnode in the old volume's index file */
   for (offset=vcp->diskSize;
	STREAM_READ(rwvnode,vcp->diskSize,1,rwfile) == 1;
	offset+=vcp->diskSize) {
      dircloned = inodeinced = 0;

      /* If we are recloning the volume, read the corresponding vnode
       * from the clone and determine its inode number.
       */
      if ( reclone && !STREAM_EOF(clfilein) && 
	   (STREAM_READ(clvnode, vcp->diskSize, 1, clfilein) == 1) ) {
	 clinode = VNDISK_GET_INO(clvnode);
      } else {
	 clinode = 0;
      }

      if (rwvnode->type != vNull) {
	 if (rwvnode->vnodeMagic != vcp->magic) ERROR_EXIT(-1);
	 rwinode = VNDISK_GET_INO(rwvnode);

	 /* Increment the inode if not already */
	 if (clinode && (clinode == rwinode)) {
	    clinode = 0; /* already cloned - don't delete later */
	 } else if (rwinode) {
	    assert(IH_INC(V_linkHandle(rwvp), rwinode, V_parentId(rwvp)) != -1);
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
	       This will cause the inode created by the file server
	       on copy-on-write to be stamped with a dataVersion bigger
	       than the current one.  The salvager will then do the
	       right thing */
	    rwvnode->dataVersion++;
#endif /* DVINC */
	    rwvnode->cloned = 1;
	    code = STREAM_SEEK(rwfile, offset, 0);
	    if (code == -1) goto clonefailed;
	    code = STREAM_WRITE(rwvnode, vcp->diskSize, 1, rwfile);
	    if (code != 1) goto clonefailed;
	    dircloned = 1;
	    code = STREAM_SEEK(rwfile, offset + vcp->diskSize, 0);
	    if (code == -1) goto clonefailed;
#ifdef DVINC
	    rwvnode->dataVersion--; /* Really needs to be set to the value in the inode,
				     for the read-only volume */
#endif /* DVINC */
	 }
      }

      /* Overwrite the vnode etnry in the clone volume */
      rwvnode->cloned = 0;
      code = STREAM_WRITE(rwvnode, vcp->diskSize, 1, clfileout);
      if (code != 1) {
       clonefailed:
	 /* Couldn't clone, go back and decrement the inode's link count */
	 if (inodeinced) {
	    assert(IH_DEC(V_linkHandle(rwvp), rwinode, V_parentId(rwvp)) != -1);
	 }
	 /* And if the directory was marked clone, unmark it */
	 if (dircloned) {
	    rwvnode->cloned = 0;
	    if (STREAM_SEEK(rwfile, offset, 0) != -1)
	       STREAM_WRITE(rwvnode, vcp->diskSize, 1, rwfile);
	 }
	 ERROR_EXIT(EIO);
      }

      /* Removal of the old cloned inode */
      if (clinode) {
	 ci_AddItem(&decHead, clinode);	/* just queue it */
      }

      DOPOLL;
   }
   if (STREAM_ERROR(clfileout)) ERROR_EXIT(EIO);

   /* Clean out any junk at end of clone file */
   if (reclone) {
       STREAM_SEEK(clfilein, offset, 0);
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
   if (rwfile)    STREAM_CLOSE(rwfile);
   if (clfilein)  STREAM_CLOSE(clfilein);
   if (clfileout) STREAM_CLOSE(clfileout);

   if (rwFd)      FDH_CLOSE(rwFd);
   if (clFdIn)    FDH_CLOSE(clFdIn);
   if (clFdOut)   FDH_CLOSE(clFdOut);

   if (rwH)       IH_RELEASE(rwH);
   if (clHout)    IH_RELEASE(clHout);
   if (clHin)     IH_RELEASE(clHin);
   
   /* Next, we sync the disk. We have to reopen in case we're truncating,
    * since we were using stdio above, and don't know when the buffers
    * would otherwise be flushed.  There's no stdio fftruncate call.
    */
   rwFd = IH_OPEN(clvp->vnodeIndex[class].handle);
   if (rwFd == NULL) {
      if (!error) error = EIO;
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
   code = ci_Apply(&decHead, IDecProc, (char *) &decRock);
   if (!error) error = code;
   ci_Destroy(&decHead);

   return error;
}

CloneVolume(error, original, new, old)
    Error *error;
    Volume *original, *new, *old;
{
    VOL_LOCK
    CloneVolume_r(error, original, new, old);
    VOL_UNLOCK
}

CloneVolume_r(rerror, original, new, old)
    Error *rerror;
    Volume *original, *new, *old;
{
    afs_int32 code, error=0;
    afs_int32 reclone;

    *rerror = 0;
    reclone = ((new == old) ? 1 : 0);

    code = DoCloneIndex(original, new, vLarge, reclone);
    if (code) ERROR_EXIT(code);
    code = DoCloneIndex(original, new, vSmall, reclone);
    if (code) ERROR_EXIT(code);

    code = CopyVolumeHeader_r(&V_disk(original), &V_disk(new));
    if (code) ERROR_EXIT(code);

  error_exit:
    *rerror = error;
}
