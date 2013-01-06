/*
 * Copyright (c) 2007, Hartmut Reuter,
 * RZG, Max-Planck-Institut f. Plasmaphysik.
 * All Rights Reserved.
 *
 */

#include <afsconfig.h>
#include <afs/param.h>

#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
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
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <errno.h>
#include <sys/stat.h>

#include <afs/dir.h>
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afssyscalls.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/partition.h>
#include <afs/viceinode.h>
#include "vol.h"
#include "volint.h"
#include "volser.h"
#include "physio.h"
#include "volser_internal.h"
#ifdef AFS_RXOSD_SUPPORT
#include "rxosd.h"
#include "vol_osd.h"
#include "../vol/vol_osd_prototypes.h"
#endif

#define NEEDED 	1
#define PARENT 	2
#define CHANGEPARENT 4

#define NAMEI_VNODEMASK    0x03ffffff
#define NAMEI_TAGMASK      0x7
#define NAMEI_TAGSHIFT     26
#define NAMEI_UNIQMASK     0xffffffff
#define NAMEI_UNIQSHIFT    32

struct VnodeExtract {
    afs_uint32 vN;
    afs_uint32 parent;
    afs_uint32 flag;
};

struct Msg {
    struct rx_call * call;
    int verbose;
    char line[1024];
};

static afs_int32
ExtractVnodes(struct Msg *m, Volume *vol, afs_int32 class,
	      struct VnodeExtract **list,
	      afs_uint32 *length, afs_uint32 where,
	      struct VnodeDiskObject *vd,
	      afs_uint32 *parent, struct VnodeDiskObject *parentvd)
{
    afs_int32 code = 0;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)&buf;
    FdHandle_t *fdP = 0;
    StreamHandle_t *stream = 0;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    struct VnodeExtract *e;
    afs_sfsize_t size;
    afs_foff_t offset;

    *length = 0;
    if (parent)
	*parent = 0;

    fdP = IH_OPEN(vol->vnodeIndex[class].handle);
    if (!fdP) {
	sprintf(m->line, "Couldn't open %s Index of volume %u\n",
		class ? "small":"large", V_id(vol));
	rx_Write(m->call, m->line, strlen(m->line));
	code = EIO;
	goto Bad_Extract;
    }
    size = FDH_SIZE(fdP);
    *list = (struct VnodeExtract *) malloc(size / vcp->diskSize
					* sizeof(struct VnodeExtract));
    if (!(*list)) {
	code = ENOMEM;
	goto Bad_Extract;
    }
    memset(*list, 0, size / vcp->diskSize * sizeof(struct VnodeExtract));
    stream = FDH_FDOPEN(fdP, "r");
    if (!stream) {
	sprintf(m->line, "Couldn't stream open %s Index of volume %u\n",
		class ? "small":"large", V_id(vol));
	rx_Write(m->call, m->line, strlen(m->line));
	code = EIO;
	goto Bad_Extract;
    }
    code = STREAM_ASEEK(stream, vcp->diskSize);
    if (code)
	goto Bad_Extract;

    offset = vcp->diskSize;
    e = *list;
    while (!STREAM_EOF(stream)) {
	afs_int32 vN = (offset >> (vcp->logSize -1)) - 1 + class;
   	if (STREAM_READ(vnode, vcp->diskSize, 1, stream) == 1) {
	    if (vnode->type != vNull) {
		e->vN = vN;
		e->parent = vnode->parent;
		if (vN == where && class == vLarge) {
	            memcpy(vd, vnode, vcp->diskSize);
		    *parent = vnode->parent;
		}
		e++;
	    }
	    offset += vcp->diskSize;
	}
    }
    *length = (e - *list);
    if (class == vLarge) {
	if (*parent) {
	    offset = (*parent + 1 - class) << (vcp->logSize -1);
            code = STREAM_ASEEK(stream, offset);
   	    if (STREAM_READ(vnode, vcp->diskSize, 1, stream) == 1)
	        memcpy(parentvd, vnode, vcp->diskSize);
	    else
	        code = EIO;
        } else {
	    sprintf(m->line, "SplitVolume: extract didn't see directory %u\n", where);
	    rx_Write(m->call, m->line, strlen(m->line));
	    code = ENOENT;
	}
    }
    if (m->verbose) {
	sprintf(m->line, "Volume %u has %u %s vnodes in volume %u\n",
			V_parentId(vol), *length, class? "small":"large",
			V_id(vol));
	rx_Write(m->call, m->line, strlen(m->line));
    }

Bad_Extract:
    if (stream)
	STREAM_CLOSE(stream);
    if (fdP)
	FDH_CLOSE(fdP);
    if (code) {
	free(*list);
	*list = 0;
    }
    return code;
}

static afs_int32
FindVnodes(struct Msg *m, afs_uint32 where,
	   struct VnodeExtract *list, afs_int32 length,
	   struct VnodeExtract *dlist, afs_int32 dlength,
	   afs_uint32 *needed, afs_int32 class)
{
    afs_int32 i, j, found = 0;
    afs_int32 parent = 0;

    *needed = 0;
    for (i=0; i<length; i++) {
	if (list[i].vN == where) {        /* dir to be replaced by mount point */
	    list[i].flag |= NEEDED;
	    parent = list[i].parent;
	    found = 1;
	    (*needed)++;
	}
	if (list[i].parent == where) { 		/* all 1st generation children */
	    list[i].flag |= (NEEDED + CHANGEPARENT);
	    (*needed)++;
	}
    }
    if (list[0].vN & 1) { 		/* only for directories */
	if (!found) {
	    sprintf(m->line,
		"SplitVolume: directory %u where to start new volume not found\n",
		 where);
	    rx_Write(m->call, m->line, strlen(m->line));
	    return ENOENT;
	}
	found = 0;
	for (i=0; i<length; i++) {
	    if (list[i].vN == parent) { /* dir where to create mount point */
		list[i].flag |= PARENT;
		found = 1;
		break;
	    }
	}
	if (!found) {
	    sprintf(m->line, "SplitVolume: parent directory %u not found\n",
			parent);
	    rx_Write(m->call, m->line, strlen(m->line));
	    return ENOENT;
	}
    }
    found = 1;
    while (found) {
	found = 0;
	for (i=0; i<dlength; i++) {
	    if (!(dlist[i].flag & NEEDED)) /* dirs to remain in old volume */
		continue;
	    for (j=0; j<length; j++) {
		if (list[j].parent == dlist[i].vN && !(list[j].flag & NEEDED)) {
		    list[j].flag |= NEEDED;
		    (*needed)++;
		    found = 1;
		}
	    }
	}
    }
    if (m->verbose) {
	sprintf(m->line, "%u %s vnodes will go into the new volume\n",
			*needed, class ? "small" : "large");
	rx_Write(m->call, m->line, strlen(m->line));
    }
    return 0;
}

static afs_int32
copyDir(struct Msg *m, IHandle_t *inh, IHandle_t *outh)
{
    FdHandle_t *infdP, *outfdP;
    char *tbuf;
    afs_sfsize_t size;
    afs_foff_t offset;

    infdP = IH_OPEN(inh);
    if (!infdP) {
	sprintf(m->line, "Couldn't open input directory %u.%u.%u\n",
		    inh->ih_vid,
		    (afs_uint32)(inh->ih_ino & NAMEI_VNODEMASK),
		    (afs_uint32)(inh->ih_ino >> NAMEI_UNIQSHIFT));
	rx_Write(m->call, m->line, strlen(m->line));
	return EIO;
    }
    outfdP = IH_OPEN(outh);
    /*
     * In case that a file with the same (NAMEI) name existed before and is still
     * open outfdP may point to the wrong (unlinked) file. To make sure we write
     * into the correct file it's safer to 1st FDH_REALLYCLOSE it and then to
     * re-open it.
     */
    if (outfdP)
	FDH_REALLYCLOSE(outfdP);
    outfdP = IH_OPEN(outh);
    if (!outfdP) {
	sprintf(m->line, "Couldn't open output directory %u.%u.%u\n",
		    outh->ih_vid,
		    (afs_uint32)(outh->ih_ino & NAMEI_VNODEMASK),
		    (afs_uint32)(outh->ih_ino >> NAMEI_UNIQSHIFT));
	rx_Write(m->call, m->line, strlen(m->line));
	FDH_REALLYCLOSE(infdP);
	return EIO;
    }
    tbuf = malloc(2048);
    offset = 0;
    size = FDH_SIZE(infdP);
    while (size) {
	size_t tlen;
        tlen = size > 2048 ? 2048 : size;
        if (FDH_PREAD(infdP, tbuf, tlen, offset) != tlen) {
       	    sprintf(m->line, "Couldn't read directory %u.%u.%u\n",
		    infdP->fd_ih->ih_vid,
		    (afs_uint32)(infdP->fd_ih->ih_ino & NAMEI_VNODEMASK),
		    (afs_uint32)(infdP->fd_ih->ih_ino >> NAMEI_UNIQSHIFT));
	    rx_Write(m->call, m->line, strlen(m->line));
	    FDH_REALLYCLOSE(infdP);
	    FDH_REALLYCLOSE(outfdP);
	    free(tbuf);
	    return EIO;
	}
	if (FDH_PWRITE(outfdP, tbuf, tlen, offset) != tlen) {
	    sprintf(m->line, "Couldn't write directory %u.%u.%u\n",
		    outfdP->fd_ih->ih_vid,
		    (afs_uint32)(outfdP->fd_ih->ih_ino & NAMEI_VNODEMASK),
		    (afs_uint32)(outfdP->fd_ih->ih_ino >> NAMEI_UNIQSHIFT));
	    rx_Write(m->call, m->line, strlen(m->line));
	    FDH_REALLYCLOSE(infdP);
	    FDH_REALLYCLOSE(outfdP);
	    free(tbuf);
	    return EIO;
	}
	size -= tlen;
	offset += tlen;
    }
    free(tbuf);
    FDH_CLOSE(outfdP);
    FDH_REALLYCLOSE(infdP);
    return 0;
}

afs_int32 copyVnodes(struct Msg *m, Volume *vol, Volume *newvol,
			afs_int32 class,
			struct VnodeExtract *list, afs_int32 length,
			afs_int32 where, afs_uint64 *blocks,
			struct VnodeDiskObject *parVnode)
{
    afs_int32 i, code = 0;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)&buf;
    FdHandle_t *fdP = 0;
    FdHandle_t *newfdP = 0;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    struct VnodeExtract *e;
    afs_sfsize_t size;
    afs_uint64 offset;
    Inode ino, newino;

    fdP = IH_OPEN(vol->vnodeIndex[class].handle);
    if (!fdP) {
	Log("Couldn't open %s Index of volume %u\n",
		class ? "small":"large", V_id(vol));
	code = EIO;
	goto Bad_Copy;
    }
    newfdP = IH_OPEN(newvol->vnodeIndex[class].handle);
    if (!newfdP) {
	Log("Couldn't open %s Index of volume %u\n",
		class ? "small":"large", V_id(newvol));
	code = EIO;
	goto Bad_Copy;
    }
    size = FDH_SIZE(fdP);

    for (i=0; i<length; i++) {
	e = &list[i];
	if (e->flag) {
	    afs_uint64 size;
	    offset = (e->vN + 1 - class) << (vcp->logSize -1);
	    if (FDH_PREAD(fdP, vnode, vcp->diskSize, offset) != vcp->diskSize) {
		Log("Couldn't read in %s Index of volume %u at offset %"
		    AFS_UINT64_FMT "\n", class ? "small":"large",
		    V_id(vol), offset);
		code = EIO;
		goto Bad_Copy;
	    }
	    if (e->flag & PARENT) {
		/*
		 *   do a preventive copy on write for later update
                 */
		IHandle_t *newh = 0;
		IHandle_t *h = 0;
#if defined(NEARINODE_HINT) && !defined(AFS_NAMEI_ENV)
		Inode nearInode;
		V_pref(vol,nearInode)
#endif

	        newino = IH_CREATE(V_linkHandle(vol), V_device(vol),
			        VPartitionPath(V_partition(vol)),
			        nearInode, V_parentId(vol),
			        e->vN, vnode->uniquifier,
			        vnode->dataVersion);
	        IH_INIT(newh, V_device(vol), V_parentId(vol), newino);
	        ino = VNDISK_GET_INO(vnode);
	        IH_INIT(h, V_device(vol), V_parentId(vol), ino);
		code = copyDir(m, h, newh);
		if (code)
		    goto Bad_Copy;
		/* Now update the vnode and write it back to disk */
		VNDISK_SET_INO(vnode, newino);
		vnode->cloned = 0;
	        if (FDH_PWRITE(fdP, vnode, vcp->diskSize, offset) != vcp->diskSize) {
		    Log("Couldn't write in %s Index of volume %u at offset %"
			AFS_UINT64_FMT "\n", class ? "small":"large",
			V_id(vol), offset);
		    code = EIO;
		    goto Bad_Copy;
	        }
	        memcpy(parVnode, vnode, sizeof(struct VnodeDiskObject));
	    }
	    if (e->flag & NEEDED && e->vN != where) {
	        VNDISK_GET_LEN(size, vnode);
	        *blocks += (size + 0x3ff) >> 10;
	        ino = VNDISK_GET_INO(vnode);
	        if (ino) {
		    IHandle_t *h, *newh;
		    Inode nearInode AFS_UNUSED;
#if defined(NEARINODE_HINT) && !defined(AFS_NAMEI_ENV)
		    V_pref(vol,nearInode)
#endif
	            IH_INIT(h, vol->device, V_parentId(vol), ino);
	            if (e->parent == where)
		        vnode->parent = 1;
	            newino = IH_CREATE(V_linkHandle(newvol), V_device(newvol),
			        VPartitionPath(V_partition(newvol)),
			        nearInode, V_parentId(newvol),
			        e->vN, vnode->uniquifier,
			        vnode->dataVersion);
	            if (!VALID_INO(newino)) {
		        Log("IH_CREATE failed for %u.%u.%u\n",
			    V_id(newvol), e->vN, vnode->uniquifier);
		        code = EIO;
		        goto Bad_Copy;
	            }
	            nearInode = newino;
	            IH_INIT(newh, newvol->device, V_parentId(newvol), newino);
	       	    code = namei_replace_file_by_hardlink(newh, h);
		    VNDISK_SET_INO(vnode, newino);
#ifdef AFS_RXOSD_SUPPORT
       	        } else {
		    code = osd_split_objects(vol, newvol, vnode, e->vN);
#endif /*  AFS_RXOSD_SUPPORT */
	        }
	        if (code)
		    goto Bad_Copy;
		if (e->flag & CHANGEPARENT)
		    vnode->parent = 1; /* in new root-directory */
		vnode->cloned = 0;
	        if (FDH_PWRITE(newfdP, vnode, vcp->diskSize, offset) != vcp->diskSize) {
		    Log("Couldn't write in %s Index of volume %u to offset %"
			AFS_UINT64_FMT "\n", class ? "small":"large",
			V_id(newvol), offset);
		    code = EIO;
		    goto Bad_Copy;
	        }
	    }
        }
    }
    /*
     *  Now copy the root directory from old to new volume
     */
    if (class == vLarge) {
	IHandle_t *h, *newh;
        char buf2[SIZEOF_LARGEDISKVNODE];
        struct VnodeDiskObject *vnode2 = (struct VnodeDiskObject *)&buf2;
	afs_uint64 newoffset;

	newoffset = vcp->diskSize;
	if (FDH_PREAD(newfdP, vnode2, vcp->diskSize, newoffset) != vcp->diskSize) {
	    Log("splitvolume: couldn't read in large Index of new volume %u at offset %u\n",
		    V_id(newvol), vcp->diskSize);
	    code = EIO;
	    goto Bad_Copy;
  	}
	offset = (where + 1 - class) << (vcp->logSize -1);
	if (FDH_PREAD(fdP, vnode, vcp->diskSize, offset) != vcp->diskSize) {
	    Log("Couldn't read in large Index of old volume %u at offset %"
		AFS_UINT64_FMT "\n", V_id(vol), offset);
	    code = EIO;
	    goto Bad_Copy;
	}
	VNDISK_GET_LEN(size, vnode);
	*blocks += (size + 0x3ff) >> 10;
	ino = VNDISK_GET_INO(vnode);
	IH_INIT(h, vol->device, V_parentId(vol), ino);
	newino = VNDISK_GET_INO(vnode2);
	IH_INIT(newh, newvol->device, V_parentId(newvol), newino);
	code = copyDir(m, h, newh);
 	if (code) {
	    Log("splitvolume: copyDir failed for new root from "
		"%u.%u.%u to %u.1.1\n",
	        V_id(vol), where, vnode->uniquifier, V_id(newvol));
	    code = EIO;
	    goto Bad_Copy;
	}
	VNDISK_SET_INO(vnode, newino);
	vnode->uniquifier = 1;
	vnode->cloned = 0;
	vnode->parent = vnode2->parent;
	vnode->serverModifyTime = vnode2->serverModifyTime;
	if (FDH_PWRITE(newfdP, vnode, vcp->diskSize, newoffset) != vcp->diskSize) {
	    Log("splitvolume: couldn't write in large Index of %u at offset %u\n",
		    V_id(newvol), vcp->diskSize);
	    code = EIO;
	}
    }
Bad_Copy:
    if (fdP)
	FDH_CLOSE(fdP);
    if (newfdP)
	FDH_CLOSE(newfdP);
    return code;
}

static afs_int32
findName(Volume *vol, struct VnodeDiskObject *vd, afs_uint32 vN,
	 afs_uint32 un, char *name,afs_int32 length)
{
    afs_int32 code;
    Inode ino;
    DirHandle dir;

    ino = VNDISK_GET_INO(vd);
    SetSalvageDirHandle(&dir, V_id(vol), V_device(vol), ino);

    code = InverseLookup(&dir, vN, un, name, length);
    FidZap(&dir);
    return code;
}

static afs_int32
createMountpoint(Volume *vol, Volume *newvol, struct VnodeDiskObject *parent,
		afs_uint32 vN,  struct VnodeDiskObject *vd, char *name)
{
    afs_int32 code;
    Inode ino, newino;
    DirHandle dir;
    IHandle_t *h, *hp;
    struct VnodeDiskObject vnode;
    FdHandle_t *fdP, *fdP2;
    afs_uint64 size;
    afs_foff_t offset;
    afs_int32 class = vSmall;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
#if defined(NEARINODE_HINT) && !defined(AFS_NAMEI_ENV)
    Inode nearInode = 0;
#endif
    AFSFid fid;
    struct timeval now;
    afs_uint32 newvN;
    char symlink[32];
    ssize_t rc;

    FT_GetTimeOfDay(&now, 0);
    fdP = IH_OPEN(vol->vnodeIndex[vSmall].handle);
    if (!fdP) {
	Log("split volume: error opening small vnode index of %u\n", V_id(vol));
	return EIO;
    }
    offset = vcp->diskSize;
    while (1) {
	rc = FDH_PREAD(fdP, &vnode, vcp->diskSize, offset);
	if (rc != vcp->diskSize) {
	    if (rc < 0) {
	        Log("split volume: error reading small vnode index of %u\n", V_id(vol));
	        return EIO;
	     }
	     if (rc == 0)
	         break;
	     if (rc < vcp->diskSize)
	         break;
	}
	if (vnode.type == vNull)
	    break;
	offset += vcp->diskSize;
    }
    memset(&vnode, 0, sizeof(vnode));
    vnode.type = vSymlink;
    V_nextVnodeUnique(vol)++;
    vnode.uniquifier = V_nextVnodeUnique(vol);
    vnode.author = vd->author;
    vnode.owner = vd->owner;
    vnode.group = vd->group;
    vnode.modeBits = 0644;
    vnode.unixModifyTime = now.tv_sec;
    vnode.serverModifyTime = now.tv_sec;
    vnode.dataVersion = 1;
    vnode.linkCount = 1;
    vnode.parent = vN;

    newvN = (offset >> (VnodeClassInfo[class].logSize - 1)) - 1 + class;
#if defined(NEARINODE_HINT) && !defined(AFS_NAMEI_ENV)
    V_pref(vol,nearInode)
#endif
    newino = IH_CREATE(V_linkHandle(vol), V_device(vol),
		VPartitionPath(V_partition(vol)), nearInode,
                V_parentId(vol), newvN, vnode.uniquifier, 1);

    IH_INIT(h, V_device(vol), V_parentId(vol), newino);
    fdP2 = IH_OPEN(h);
    if (!fdP2) {
	Log("split volume: couldn't open inode for mountpoint %u.%u.%u\n",
		V_id(vol), newvN, vnode.uniquifier);
	return EIO;
    }
    sprintf(symlink, "#%s", V_name(newvol));
    size = strlen(symlink) + 1;
    if (FDH_PWRITE(fdP2, symlink, size, 0) != size) {
	Log("split volume: couldn't write mountpoint %u.%u.%u\n",
		V_id(vol), newvN, vnode.uniquifier);
	return EIO;
    }
    FDH_REALLYCLOSE(fdP2);
    IH_RELEASE(h);
    VNDISK_SET_INO(&vnode, newino);
    VNDISK_SET_LEN(&vnode, size);
#ifndef AFS_RXOSD_SUPPORT
    vnode.vnodeMagic = SMALLVNODEMAGIC;
#endif
    if (FDH_PWRITE(fdP, &vnode, vcp->diskSize, offset) != vcp->diskSize) {
	Log("split volume: couldn't write vnode for mountpoint %u.%u.%u\n",
		V_id(vol), newvN, vnode.uniquifier);
	return EIO;
    }
    FDH_REALLYCLOSE(fdP);

    fid.Volume = V_id(vol);
    fid.Vnode = newvN;
    fid.Unique = vnode.uniquifier;

    /*
     * Now  update the parent directory.
     */

    ino = VNDISK_GET_INO(parent);
    SetSalvageDirHandle(&dir, V_id(vol), V_device(vol), ino);

    code = Delete(&dir, name);
    if (code) {
	Log("splitvolume: couldn't delete directory entry for %s in %u.%u.%u, code = %d\n",
			name, V_id(vol), vN, parent->uniquifier, code);
	return code;
    }
    code = Create(&dir, name, &fid);
    FidZap(&dir);

    /* Make sure the directory file doesn't remain open */
    IH_INIT(hp, V_device(vol), V_parentId(vol), ino);
    fdP = IH_OPEN(hp);
    if (fdP)
	FDH_REALLYCLOSE(fdP);
    IH_RELEASE(hp);

    class = vLarge;
    vcp = &VnodeClassInfo[class];
    fdP = IH_OPEN(vol->vnodeIndex[class].handle);
    offset = (vN + 1 - class) << (vcp->logSize -1);
    parent->dataVersion++;
    if (FDH_PWRITE(fdP, parent, vcp->diskSize, offset) != vcp->diskSize) {
	Log("split volume: couldn't write vnode for parent directory %u.%u.%u\n",
		V_id(vol), vN, parent->uniquifier);
	return EIO;
    }
    FDH_REALLYCLOSE(fdP);
    return code;
}

static afs_int32
deleteVnodes(Volume *vol, afs_int32 class,
	     struct VnodeExtract *list, afs_int32 length,
	     afs_uint64 *blocks)
{
    afs_int32 i, code = 0;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)&buf;
    FdHandle_t *fdP = 0;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    struct VnodeExtract *e;
    afs_sfsize_t size;
    afs_uint64 offset;
    Inode ino;

    fdP = IH_OPEN(vol->vnodeIndex[class].handle);
    if (!fdP) {
	Log("Couldn't open %s Index of volume %u\n",
		class ? "small":"large", V_id(vol));
	code = EIO;
	goto Bad_Delete;
    }

    for (i=0; i<length; i++) {
	e = &list[i];
	if (e->flag & NEEDED) {
	    offset = (e->vN + 1 - class) << (vcp->logSize -1);
	    if (FDH_PREAD(fdP, vnode, vcp->diskSize, offset) != vcp->diskSize) {
		Log("Couldn't read in %s Index of volume %u at offset %"
		    AFS_UINT64_FMT "\n", class ? "small":"large", V_id(vol),
		    offset);
		code = EIO;
		goto Bad_Delete;
	    }
	    VNDISK_GET_LEN(size, vnode);
	    *blocks += (size + 0x3ff) >> 10;
	    ino = VNDISK_GET_INO(vnode);
	    if (ino) {
		IHandle_t *h;
	        IH_INIT(h, vol->device, V_parentId(vol), ino);
		    IH_DEC(h, ino, V_parentId(vol));
#ifdef AFS_RXOSD_SUPPORT
       	    } else {
		code = osdRemove(vol, vnode, e->vN);
#endif /*  AFS_RXOSD_SUPPORT */
	    }
	    memset(vnode, 0, vcp->diskSize);
	    vnode->type = vNull;
	    if (FDH_PWRITE(fdP, vnode, vcp->diskSize, offset) != vcp->diskSize) {
	           Log("Couldn't write in %s Index of volume %u to offset %"
		       AFS_UINT64_FMT "\n", class ? "small":"large",
		       V_id(vol), offset);
	    }
        }
    }
Bad_Delete:
    if (fdP)
	FDH_CLOSE(fdP);
    return code;
}

afs_int32
split_volume(struct rx_call *call, Volume *vol, Volume *newvol,
	     afs_uint32 where, afs_int32 verbose)
{
    Error code = 0;
    struct VnodeExtract *dirList = 0;
    struct VnodeExtract *fileList = 0;
    afs_uint64 blocks = 0;
    afs_uint32 filesNeeded, dirsNeeded;
    afs_uint32 dl, fl;
    char buf[SIZEOF_LARGEDISKVNODE];
    char buf2[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *rootVnode = (struct VnodeDiskObject *)&buf;
    struct VnodeDiskObject *parVnode = (struct VnodeDiskObject *)&buf2;
    char name[256];
    afs_uint32 parent;
    struct Msg *m;

    m = (struct Msg *) malloc(sizeof(struct Msg));
    memset(m, 0, sizeof(struct Msg));
    m->call = call;
    m->verbose = verbose;

    /*
     *  First step: planning
     *
     *  Find out which directories will belong to the new volume
     *
     */
    if (verbose) {
	sprintf(m->line,
		"1st step: extract vnode essence from large vnode file\n");
	rx_Write(m->call, m->line, strlen(m->line));
    }

    code = ExtractVnodes(m, vol, vLarge, &dirList, &dl, where, rootVnode,
			&parent, parVnode);
    if (code) {
	sprintf(m->line,
		"ExtractVnodes failed for %u for directories with code %d\n",
	        V_id(vol), code);
	rx_Write(m->call, m->line, strlen(m->line));
	return code;
    }

    if (verbose) {
	sprintf(m->line, "2nd step: look for name of vnode %u in directory %u.%u.%u\n",
		where, V_id(vol), parent, parVnode->uniquifier);
	rx_Write(m->call, m->line, strlen(m->line));
    }
    code = findName(vol, parVnode, where, rootVnode->uniquifier,
                    name,  sizeof(name));
    if (code) {
	sprintf(m->line,
		"splitvolume: could'nt find name of %u in directory %u.%u.%u.\n",
		where, V_id(vol), parent, parVnode->uniquifier);
	rx_Write(m->call, m->line, strlen(m->line));
	return code;
    }
    if (verbose) {
	sprintf(m->line, "name of %u is %s\n", where, name);
	rx_Write(m->call, m->line, strlen(m->line));
    }

    if (verbose) {
	sprintf(m->line, "3rd step: find all directory vnodes belonging to the subtree under %u \"%s\"\n",
			where, name);
	rx_Write(m->call, m->line, strlen(m->line));
    }
    code = FindVnodes(m, where, dirList, dl, dirList, dl, &dirsNeeded, 1);
    if (code) {
	sprintf(m->line,
		"FindVnodes for directories failed with code %d\n", code);
	rx_Write(m->call, m->line, strlen(m->line));
	return code;
    }

    if (verbose) {
	sprintf(m->line, "4th step extract vnode essence from small vnode file\n");
	rx_Write(m->call, m->line, strlen(m->line));
    }
    code = ExtractVnodes(m, vol, vSmall, &fileList, &fl, where, 0, 0, 0);
    if (code) {
	sprintf(m->line,
		"ExtractVnodes failed for %u for files with code %d\n",
		V_id(vol), code);
	rx_Write(m->call, m->line, strlen(m->line));
	return code;
    }
    if (verbose) {
	sprintf(m->line, "5th step: find all small vnodes belonging to the subtree under %u \"%s\"\n",
			where, name);
	rx_Write(m->call, m->line, strlen(m->line));
    }
    FindVnodes(m, where, fileList, fl, dirList, dl, &filesNeeded, 0);

    /*
     *  Third step: create hard links for all files needed
     *
     */

    V_destroyMe(newvol) = DESTROY_ME;
    V_inService(newvol) = 0;
    if (verbose) {
	sprintf(m->line, "6th step: create hard links in the AFSIDat tree between files of the old and new volume\n");
	rx_Write(m->call, m->line, strlen(m->line));
    }
    code = copyVnodes(m, vol, newvol, 1, fileList, fl, where, &blocks, 0);
    if (code) {
	sprintf(m->line, "copyVnodes for files failed with code %d\n", code);
	rx_Write(m->call, m->line, strlen(m->line));
	return code;
    }

    /*
     *  Forth step: create hard links for all directories and copy
     *  split directory to new root directory
     */

    if (verbose) {
	sprintf(m->line, "7th step: create hard links in the AFSIDat tree between directories of the old and new volume and make dir %u to new volume's root directory.\n",
		where);
	rx_Write(m->call, m->line, strlen(m->line));
    }
    code = copyVnodes(m, vol, newvol, 0, dirList, dl, where, &blocks, parVnode);
    if (code) {
	sprintf(m->line, "copyVnodes for directories failed with code %d\n", code);
	rx_Write(m->call, m->line, strlen(m->line));
	return code;
    }

    /*
     *  Finalize new volume
     *
     */
    if (verbose) {
	sprintf(m->line, "8th step: write new volume's metadata to disk\n");
	rx_Write(m->call, m->line, strlen(m->line));
    }

    V_diskused(newvol) = blocks;
#ifdef AFS_RXOSD_SUPPORT
    V_osdFlag(newvol) = V_osdFlag(vol);
#endif
    V_filecount(newvol) = filesNeeded + dirsNeeded;
    V_destroyMe(newvol) = 0;
    V_maxquota(newvol) = V_maxquota(vol);
    V_uniquifier(newvol) = V_uniquifier(vol);
    V_inService(newvol) = 1;
    VUpdateVolume(&code, newvol);

    /*
     *  Sixth step: change directory entry in old volume:
     *  rename old tree and create mount point for new volume.
     */
    if (verbose) {
	sprintf(m->line, "9th step: create mountpoint \"%s\" for new volume in old volume's directory %u.\n", name, parent);
	rx_Write(m->call, m->line, strlen(m->line));
    }

    code = createMountpoint(vol, newvol, parVnode, parent, rootVnode, name);
    if (code) {
	sprintf(m->line, "createMountpoint failed with code %d\n", code);
	rx_Write(m->call, m->line, strlen(m->line));
	return code;
    }
    /*
     * Now both volumes should be ready and consistent, but the old volume
     * contains still the vnodes and data we transferred into the new one.
     * Delete orphaned vnodes and data.
     */

    blocks = 0;
    if (verbose) {
	sprintf(m->line, "10th step: delete large vnodes belonging to subtree in the old volume.\n");
	rx_Write(m->call, m->line, strlen(m->line));
    }
    deleteVnodes(vol, vLarge, dirList, dl, &blocks);
    if (verbose) {
	sprintf(m->line, "11th step: delete small vnodes belonging to subtree in the old volume.\n");
	rx_Write(m->call, m->line, strlen(m->line));
    }
    deleteVnodes(vol, vSmall, fileList, fl, &blocks);
    V_diskused(vol) -= blocks;
    V_filecount(vol) -= (filesNeeded + dirsNeeded + 1);
    VUpdateVolume(&code, vol);

    sprintf(m->line, "Finished!\n");
    rx_Write(m->call, m->line, strlen(m->line));
    m->line[0] = 0;
    m->line[1] = 0;
    m->line[2] = 0;
    m->line[3] = 0;
    rx_Write(m->call, m->line, 4);
    free(m);
    return code;
}
#endif
