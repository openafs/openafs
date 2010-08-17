/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#define VICE			/* control whether AFS changes are present */

#ifdef   AFS_OSF_ENV

#include <stdio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <ufs/inode.h>
#include <ufs/dinode.h>
#include <sys/proplist.h>
#include <ufs/fs.h>
#define	_BSD
#define	_KERNEL
#include <ufs/dir.h>
#undef	_KERNEL
#undef	_BSD

#include <afs/osi_inode.h>
#include <pwd.h>
#include "fsck.h"

struct prop_entry_desc {
    struct prop_entry_desc *next;
    int flags;
#define PROP_ENTRY_BAD	0x1
#define PROP_ENTRY_DUP	0x2
    daddr_t blkno;
    int blksize;
    long offset;
    long size;
    char name[PROPLIST_NAME_MAX];
};

int
proplist_scan(dp, idesc)
     struct dinode *dp;
     struct inodesc *idesc;
{
    struct proplist_desc1 *pl_descp;
    struct bufarea *bp;
    struct dinode *ndp;
    long code;
    int offsetinbuf, blksize;
    struct prop_entry_desc *entry_list, *next;

    code = proplist_blkscan(dp, idesc, &entry_list);
    if (code & STOP)
	goto out;

    proplist_markdup(entry_list);

    code = proplist_updateblks(dp, idesc, entry_list);
    if (code & STOP)
	goto out;

    ndp = ginode(idesc->id_number);
    if ((ndp->di_flags & IC_PROPLIST) == 0) {
	code = 0;
	goto out;
    }
    if ((ndp->di_flags & (IC_PROPLIST_BLOCK | IC_PROPLIST_FRAG)) ==
	(IC_PROPLIST_BLOCK | IC_PROPLIST_FRAG)) {
	code = 0;
	goto out;
    }
    if (ndp->di_flags & IC_PROPLIST_FRAG) {
	idesc->id_numfrags = 1;
	blksize = sblock.fs_fsize;
    } else {
	idesc->id_numfrags = sblock.fs_frag;
	blksize = sblock.fs_bsize;
    }
    idesc->id_blkno = ndp->di_proplb;
    for (;;) {
	code = (*idesc->id_func) (idesc);
	if (code & STOP)
	    goto out;

	bp = getdatablk(idesc->id_blkno, blksize);
	for (offsetinbuf = 0; offsetinbuf < blksize;) {
	    pl_descp =
		(struct proplist_desc1 *)(bp->b_un.b_buf + offsetinbuf);
	    offsetinbuf += pl_descp->pl_nextentry;
	}
	if (pl_descp->pl_nextfsb > 0) {
	    daddr_t save_blkno;

	    save_blkno = pl_descp->pl_nextfsb;
	    bp->b_flags &= ~B_INUSE;
	    idesc->id_blkno = save_blkno;
	    blksize = sblock.fs_bsize;
	    idesc->id_numfrags = sblock.fs_frag;
	    continue;
	}
	bp->b_flags &= ~B_INUSE;
	break;
    }
  out:
    for (next = entry_list; entry_list != NULL;) {
	next = entry_list->next;
	free(entry_list);
	entry_list = next;
    }
    return (code);
}

int
proplist_blkscan(dp, idesc, entry_list)
     struct dinode *dp;
     struct inodesc *idesc;
     struct prop_entry_desc **entry_list;
{
    struct proplist_desc1 *pl_descp;
    struct bufarea *bp;
    struct prop_entry_desc *entry, *lastentry;
    int blksize;
    long code, valueresid;

    *entry_list = NULL;
    idesc->id_blkno = dp->di_proplb;
    if (dp->di_flags & IC_PROPLIST_FRAG) {
	blksize = sblock.fs_fsize;
	idesc->id_numfrags = 1;
    } else {
	blksize = sblock.fs_bsize;
	idesc->id_numfrags = sblock.fs_frag;
    }
    idesc->id_loc = 0;
    valueresid = 0;
    for (;;) {
	if (idesc->id_loc == 0) {
	    if (chkrange(idesc->id_blkno, idesc->id_numfrags)) {
		code = proplist_blkdel(dp, idesc, 0);
		return (code);
	    }
	    bp = getdatablk(idesc->id_blkno, blksize);
	    if (proplist_chkblock(bp, blksize)) {
		bp->b_flags &= ~B_INUSE;
		pwarn("PROPERTY LIST BLOCK CORRUPTED I=%u", idesc->id_number);
		if (preen)
		    printf(" (CLEARED)\n");
		else if (reply("CLEAR") == 0)
		    return (SKIP);
		code = proplist_blkdel(dp, idesc, 0);
		return (code);
	    }
	}
	pl_descp = (struct proplist_desc1 *)(bp->b_un.b_buf + idesc->id_loc);
	if (pl_descp->pl_entrysize) {
	    if (valueresid < 0
		|| (valueresid
		    && strcmp((char *)&pl_descp[1], entry->name))) {
		entry->flags |= PROP_ENTRY_BAD;
		valueresid = 0;
	    }
	    if (valueresid == 0) {
		entry = (struct prop_entry_desc *)
		    malloc(sizeof(struct prop_entry_desc));
		if (entry == NULL)
		    return (SKIP);
		entry->next = NULL;
		entry->flags = 0;
		memcpy(entry->name, (char *)&pl_descp[1],
		       pl_descp->pl_namelen);
		entry->blkno = idesc->id_blkno;
		entry->blksize = blksize;
		entry->offset = idesc->id_loc;
		entry->size = 0;
		if (*entry_list != NULL)
		    lastentry->next = entry;
		else
		    *entry_list = entry;
		lastentry = entry;
		valueresid = pl_descp->pl_valuelen;
	    }
	    entry->size += pl_descp->pl_entrysize;
	    valueresid -= pl_descp->pl_valuelen_entry;
	}
	if (pl_descp->pl_nextfsb > 0) {
	    daddr_t save_blkno;

	    save_blkno = pl_descp->pl_nextfsb;
	    bp->b_flags &= ~B_INUSE;
	    idesc->id_blkno = save_blkno;
	    idesc->id_numfrags = sblock.fs_frag;
	    blksize = sblock.fs_bsize;
	    idesc->id_loc = 0;
	    continue;
	}
	idesc->id_loc += pl_descp->pl_nextentry;
	if (idesc->id_loc == blksize) {
	    bp->b_flags &= ~B_INUSE;
	    if (valueresid) {
		entry->flags |= PROP_ENTRY_BAD;
	    }
	    break;
	}
    }
    return (0);
}

int
proplist_markdup(entry_list)
     struct prop_entry_desc *entry_list;
{
    struct prop_entry_desc *start, *cur;
    int bad_entries, dup_entries;

    for (start = entry_list; start != NULL; start = start->next) {
	if (start->flags & (PROP_ENTRY_BAD | PROP_ENTRY_DUP))
	    continue;
	for (cur = start->next; cur != NULL; cur = cur->next) {
	    if (!strcmp(start->name, cur->name))
		cur->flags |= PROP_ENTRY_DUP;
	}
    }
    return (0);
}

int
proplist_updateblks(dp, idesc, entry_list)
     struct dinode *dp;
     struct inodesc *idesc;
     struct prop_entry_desc *entry_list;
{
    struct proplist_desc1 *pl_descp, *prev_pl_descp;
    struct bufarea *bp;
    struct prop_entry_desc *cur;
    long code;
    daddr_t next_blkno;
    int resid, offset, free, blksize;

    for (cur = entry_list; cur != NULL; cur = cur->next) {
	if (cur->flags == 0)
	    continue;
	idesc->id_blkno = cur->blkno;
	idesc->id_loc = cur->offset;
	blksize = cur->blksize;

	if (cur->flags & PROP_ENTRY_BAD)
	    pwarn("BAD PROPERTY LIST ENTRY FOUND I=%u NAME %0.10s",
		  idesc->id_number, cur->name);
	else
	    pwarn("DUP PROPERTY LIST ENTRY FOUND I=%u NAME %0.10s",
		  idesc->id_number, cur->name);
	if (preen)
	    printf(" (FIXED)\n");
	else if (reply("FIX") == 0)
	    continue;
	for (resid = cur->size; resid > 0;) {
	    bp = getdatablk(idesc->id_blkno, blksize);
	    pl_descp =
		(struct proplist_desc1 *)(bp->b_un.b_buf + idesc->id_loc);
	    if (strcmp((char *)&pl_descp[1], cur->name)) {
		bp->b_flags &= ~B_INUSE;
		break;
	    }
	    if (idesc->id_loc) {
		prev_pl_descp = (struct proplist_desc1 *)bp->b_un.b_buf;
		for (offset = 0; offset < cur->offset;) {
		    prev_pl_descp =
			(struct proplist_desc1 *)(bp->b_un.b_buf + offset);
		    offset += prev_pl_descp->pl_nextentry;
		}
		/*
		 * prev_pl_descp now points to the entry
		 * before the one we need to delete
		 *
		 * Coalesce into previous entry
		 */
		prev_pl_descp->pl_nextentry += pl_descp->pl_nextentry;
		prev_pl_descp->pl_nextfsb = pl_descp->pl_nextfsb;
	    }
	    resid -= pl_descp->pl_entrysize;
	    pl_descp->pl_entrysize = 0;
	    pl_descp->pl_namelen = 0;
	    pl_descp->pl_valuelen = 0;

	    next_blkno = pl_descp->pl_nextfsb;
	    free = prop_avail(bp, blksize);
	    dirty(bp);
	    if (free == blksize)
		proplist_blkdel(dp, idesc, next_blkno);

	    if (next_blkno && resid > 0) {
		idesc->id_blkno = next_blkno;
		blksize = sblock.fs_bsize;
		idesc->id_loc = 0;
		continue;
	    }
	    break;
	}
    }
    return (0);
}

int
prop_avail(bp, blksize)
     struct bufarea *bp;
     int blksize;
{
    struct proplist_desc1 *pl_descp;
    int offsetinbuf, total_avail;

    total_avail = 0;
    for (offsetinbuf = 0; offsetinbuf < blksize;) {
	pl_descp = (struct proplist_desc1 *)(bp->b_un.b_buf + offsetinbuf);
	total_avail += (pl_descp->pl_nextentry - pl_descp->pl_entrysize);
	offsetinbuf += pl_descp->pl_nextentry;
    }
    return (total_avail);
}

int
proplist_chkblock(bp, blksize)
     struct bufarea *bp;
     int blksize;
{
    struct proplist_desc1 *pl_descp;
    int offsetinbuf;

    for (offsetinbuf = 0; offsetinbuf < blksize;) {
	pl_descp = (struct proplist_desc1 *)(bp->b_un.b_buf + offsetinbuf);
	if (pl_descp->pl_magic != PROP_LIST_MAGIC_VERS1) {
	    return (1);
	}
	if (pl_descp->pl_entrysize % 8 || pl_descp->pl_nextentry % 8
	    || pl_descp->pl_nextentry < UFSPROPLIST_STRUCT
	    || pl_descp->pl_nextentry + offsetinbuf > blksize) {
	    return (1);
	}
	if (pl_descp->pl_entrysize
	    && (pl_descp->pl_namelen > PROPLIST_NAME_MAX
		|| pl_descp->pl_valuelen_entry > pl_descp->pl_valuelen
		|| pl_descp->pl_entrysize > pl_descp->pl_nextentry
		|| pl_descp->pl_entrysize !=
		UFSPROPLIST_SIZE(pl_descp->pl_namelen,
				 pl_descp->pl_valuelen_entry)
		|| strlen((char *)&pl_descp[1]) > pl_descp->pl_namelen)) {
	    return (1);
	}
	offsetinbuf += pl_descp->pl_nextentry;
	if (offsetinbuf == blksize) {
	    bp->b_flags &= ~B_INUSE;
	    break;
	}
    }
    if (offsetinbuf != blksize) {
	return (1);
    }
    return (0);
}


int
proplist_blkdel(dp, idesc, nextblk)
     struct dinode *dp;
     struct inodesc *idesc;
     daddr_t nextblk;
{
    struct proplist_desc1 *pl_descp;
    struct bufarea *bp;
    int blksize;
    daddr_t badblkno;

    badblkno = idesc->id_blkno;
    if (dp->di_proplb == badblkno) {
	dp = ginode(idesc->id_number);
	dp->di_proplb = nextblk;
	dp->di_flags &= ~IC_PROPLIST;
	if (nextblk)
	    dp->di_flags |= IC_PROPLIST_BLOCK;
	inodirty();
	return (ALTERED);
    }
    idesc->id_blkno = dp->di_proplb;
    if (dp->di_flags & IC_PROPLIST_FRAG) {
	blksize = sblock.fs_fsize;
	idesc->id_numfrags = 1;
    } else {
	blksize = sblock.fs_bsize;
	idesc->id_numfrags = sblock.fs_frag;
    }
    bp = getdatablk(idesc->id_blkno, blksize);
    idesc->id_loc = 0;
    for (;;) {
	pl_descp = (struct proplist_desc1 *)(bp->b_un.b_buf + idesc->id_loc);
	if (pl_descp->pl_nextfsb > 0) {
	    daddr_t save_blkno;

	    if (pl_descp->pl_nextfsb == badblkno) {
		pl_descp->pl_nextfsb = nextblk;
		dirty(bp);
		return (ALTERED);
	    }
	    save_blkno = pl_descp->pl_nextfsb;
	    bp->b_flags &= ~B_INUSE;
	    idesc->id_blkno = save_blkno;
	    idesc->id_numfrags = sblock.fs_frag;
	    blksize = sblock.fs_bsize;
	    bp = getdatablk(save_blkno, blksize);
	    idesc->id_loc = 0;
	    continue;
	}
	idesc->id_loc += pl_descp->pl_nextentry;
	if (idesc->id_loc == blksize) {
	    bp->b_flags &= ~B_INUSE;
	    break;
	}
    }
    return (SKIP);
}

#endif /* AFS_OSF_ENV */
