/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)ufs_subr.c	7.11 (Berkeley) 12/30/89
 */

#include <afsconfig.h>
#include <afs/param.h>


#define VICE

#include <sys/param.h>
#include <sys/time.h>
#ifdef	AFS_OSF_ENV
#include <ufs/fs.h>
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_fs.h>
#else
#include <ufs/fs.h>
#endif
#else /* AFS_VFSINCL_ENV */
#include <sys/fs.h>
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */

extern int around[9];
extern int inside[9];
extern u_char *fragtbl[];

/*
 * Update the frsum fields to reflect addition or deletion
 * of some frags.
 */
fragacct(fs, fragmap, fraglist, cnt)
     struct fs *fs;
     int fragmap;
     afs_int32 fraglist[];
     int cnt;
{
    int inblk;
    int field, subfield;
    int siz, pos;

    inblk = (int)(fragtbl[fs->fs_frag][fragmap]) << 1;
    fragmap <<= 1;
    for (siz = 1; siz < fs->fs_frag; siz++) {
	if ((inblk & (1 << (siz + (fs->fs_frag % NBBY)))) == 0)
	    continue;
	field = around[siz];
	subfield = inside[siz];
	for (pos = siz; pos <= fs->fs_frag; pos++) {
	    if ((fragmap & field) == subfield) {
		fraglist[siz] += cnt;
		pos += siz;
		field <<= siz;
		subfield <<= siz;
	    }
	    field <<= 1;
	    subfield <<= 1;
	}
    }
}

/*
 * block operations
 *
 * check if a block is available
 */
isblock(fs, cp, h)
     struct fs *fs;
     unsigned char *cp;
     daddr_t h;
{
    unsigned char mask;

    switch ((int)fs->fs_frag) {
    case 8:
	return (cp[h] == 0xff);
    case 4:
	mask = 0x0f << ((h & 0x1) << 2);
	return ((cp[h >> 1] & mask) == mask);
    case 2:
	mask = 0x03 << ((h & 0x3) << 1);
	return ((cp[h >> 2] & mask) == mask);
    case 1:
	mask = 0x01 << (h & 0x7);
	return ((cp[h >> 3] & mask) == mask);
    default:
	panic("isblock");
	return (NULL);
    }
}

/*
 * take a block out of the map
 */
clrblock(fs, cp, h)
     struct fs *fs;
     u_char *cp;
     daddr_t h;
{

    switch ((int)fs->fs_frag) {
    case 8:
	cp[h] = 0;
	return;
    case 4:
	cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
	return;
    case 2:
	cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
	return;
    case 1:
	cp[h >> 3] &= ~(0x01 << (h & 0x7));
	return;
    default:
	panic("clrblock");
    }
}

/*
 * put a block into the map
 */
setblock(fs, cp, h)
     struct fs *fs;
     unsigned char *cp;
     daddr_t h;
{

    switch ((int)fs->fs_frag) {

    case 8:
	cp[h] = 0xff;
	return;
    case 4:
	cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
	return;
    case 2:
	cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
	return;
    case 1:
	cp[h >> 3] |= (0x01 << (h & 0x7));
	return;
    default:
	panic("setblock");
    }
}

#if (!defined(vax) && !defined(tahoe)) || defined(VAX630) || defined(VAX650)
/*
 * C definitions of special instructions.
 * Normally expanded with inline.
 */
scanc(size, cp, table, mask)
     u_int size;
     u_char *cp, table[];
     u_char mask;
{
    u_char *end = &cp[size];

    while (cp < end && (table[*cp] & mask) == 0)
	cp++;
    return (end - cp);
}
#endif

#if !defined(vax) && !defined(tahoe)
skpc(mask, size, cp)
     u_char mask;
     u_int size;
     u_char *cp;
{
    u_char *end = &cp[size];

    while (cp < end && *cp == mask)
	cp++;
    return (end - cp);
}

locc(mask, size, cp)
     u_char mask;
     u_int size;
     u_char *cp;
{
    u_char *end = &cp[size];

    while (cp < end && *cp != mask)
	cp++;
    return (end - cp);
}
#endif
