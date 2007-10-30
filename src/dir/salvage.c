/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This is the directory salvager.  It consists of two routines.  The first, DirOK, checks to see if the directory looks good.  If the directory does NOT look good, the approved procedure is to then call Salvage, which copies all the good entries from the damaged dir into a new directory. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <string.h>

#include "dir.h"
#define printf	Log		/* To make it work with volume salvager */

/* This routine is called with one parameter, the id (the same thing that is passed to physio or the buffer package) of a directory to check.  It returns 1 if the directory looks good, and 0 otherwise. */

#define MAXENAME 256

extern afs_int32 DErrno;

/* figure out how many pages in use in a directory, given ptr to its (locked) header */
static
ComputeUsedPages(dhp)
     register struct DirHeader *dhp;
{
    register afs_int32 usedPages, i;

    if (dhp->header.pgcount != 0) {
	/* new style */
	usedPages = ntohs(dhp->header.pgcount);
    } else {
	/* old style */
	usedPages = 0;
	for (i = 0; i < MAXPAGES; i++) {
	    if (dhp->alloMap[i] == EPP) {
		usedPages = i;
		break;
	    }
	}
	if (usedPages == 0)
	    usedPages = MAXPAGES;
    }
    return usedPages;
}

/* returns true if something went wrong checking, or if dir is fine.  Returns
 * false if we *know* that the dir is bad.
 */
int
DirOK(file)
     char *file;
{
    struct DirHeader *dhp;
    struct PageHeader *pp;
    struct DirEntry *ep;
    int i, j, k, up;
    int havedot = 0, havedotdot = 0;
    int usedPages, count, entry;
    char eaMap[BIGMAXPAGES * EPP / 8];	/* Change eaSize initialization below, too. */
    int eaSize;
    afs_int32 entcount, maxents;
    unsigned short ne;

    eaSize = BIGMAXPAGES * EPP / 8;

    /* Read the directory header */
    dhp = (struct DirHeader *)DRead(file, 0);
    if (!dhp) {
	/* if DErrno is 0, then we know that the read worked, but was short,
	 * and the damage is permanent.  Otherwise, we got an I/O or programming
	 * error.  Claim the dir is OK, but log something.
	 */
	if (DErrno != 0) {
	    printf("Could not read first page in directory (%d)\n", DErrno);
	    Die("dirok1");
	    return 1;
	}
	printf("First page in directory does not exist.\n");
	return 0;
    }

    /* Check magic number for first page */
    if (dhp->header.tag != htons(1234)) {
	printf("Bad first pageheader magic number.\n");
	DRelease(dhp, 0);
	return 0;
    }

    /* Verify that the number of free entries in each directory page
     * is within range (0-EPP). Also ensure directory is contiguous:
     * Once the first alloMap entry with EPP free entries is found,
     * the rest should match.
     */
    up = 0;			/* count of used pages if total pages < MAXPAGES */
    k = 0;			/* found last page */
    for (i = 0; i < MAXPAGES; i++) {
	j = dhp->alloMap[i];

	/* Check if in range */
	if (i == 0) {
	    if ((j < 0) || (j > EPP - (13 + 2))) {
		/* First page's dirheader uses 13 entries and at least
		 * two must exist for "." and ".."
		 */
		printf("The dir header alloc map for page %d is bad.\n", i);
		DRelease(dhp, 0);
		return 0;
	    }
	} else {
	    if ((j < 0) || (j > EPP)) {
		printf("The dir header alloc map for page %d is bad.\n", i);
		DRelease(dhp, 0);
		return 0;
	    }
	}

	/* Check if contiguous */
	if (k) {		/* last page found */
	    if (j != EPP) {	/* remaining entries must be EPP */
		printf
		    ("A partially-full page occurs in slot %d, after the dir end.\n",
		     i);
		DRelease(dhp, 0);
		return 0;
	    }
	} else if (j == EPP) {	/* is this the last page */
	    k = 1;		/* yes */
	} else {		/* a used page */
	    up++;		/* keep count */
	}
    }

    /* Compute number of used directory pages and max entries in all 
     ** those pages, the value of 'up' must be less than pgcount. The above
     ** loop only checks the first MAXPAGES in a directory. An alloMap does
     ** not exists for pages between MAXPAGES and BIGMAXPAGES */
    usedPages = ComputeUsedPages(dhp);
    if (usedPages < up) {
	printf
	    ("Count of used directory pages does not match count in directory header\n");
	DRelease(dhp, 0);
	return 0;
    }

    /* For each directory page, check the magic number in each page
     * header, and check that number of free entries (from freebitmap)
     * matches the count in the alloMap from directory header.
     */
    for (i = 0; i < usedPages; i++) {
	/* Read the page header */
	pp = (struct PageHeader *)DRead(file, i);
	if (!pp) {
	    DRelease(dhp, 0);
	    if (DErrno != 0) {
		/* couldn't read page, but not because it wasn't there permanently */
		printf("Failed to read dir page %d (errno %d)\n", i, DErrno);
		Die("dirok2");
		return 1;
	    }
	    printf("Directory shorter than alloMap indicates (page %d)\n", i);
	    return 0;
	}

	/* check the tag field */
	if (pp->tag != htons(1234)) {
	    printf("Directory page %d has a bad magic number.\n", i);
	    DRelease(pp, 0);
	    DRelease(dhp, 0);
	    return 0;
	}

	/* Count the number of entries allocated in this single
	 * directory page using the freebitmap in the page header.
	 */
	count = 0;
	for (j = 0; j < EPP / 8; j++) {
	    k = pp->freebitmap[j];
	    if (k & 0x80)
		count++;
	    if (k & 0x40)
		count++;
	    if (k & 0x20)
		count++;
	    if (k & 0x10)
		count++;
	    if (k & 0x08)
		count++;
	    if (k & 0x04)
		count++;
	    if (k & 0x02)
		count++;
	    if (k & 0x01)
		count++;
	}
	count = EPP - count;	/* Change to count of free entries */

	/* Now check that the count of free entries matches the count in the alloMap */
	if ((i < MAXPAGES) && ((count & 0xff) != (dhp->alloMap[i] & 0xff))) {
	    printf
		("Header alloMap count doesn't match count in freebitmap for page %d.\n",
		 i);
	    DRelease(pp, 0);
	    DRelease(dhp, 0);
	    return 0;
	}

	DRelease(pp, 0);
    }

    /* Initialize the in-memory freebit map for all pages. */
    for (i = 0; i < eaSize; i++) {
	eaMap[i] = 0;
	if (i < usedPages * (EPP / 8)) {
	    if (i == 0) {
		eaMap[i] = 0xff;	/* A dir header uses first 13 entries */
	    } else if (i == 1) {
		eaMap[i] = 0x1f;	/* A dir header uses first 13 entries */
	    } else if ((i % 8) == 0) {
		eaMap[i] = 0x01;	/* A page header uses only first entry */
	    }
	}
    }
    maxents = usedPages * EPP;

    /* Walk down all the hash lists, ensuring that each flag field has FFIRST
     * in it.  Mark the appropriate bits in the in-memory freebit map.
     * Check that the name is in the right hash bucket.
     * Also check for loops in the hash chain by counting the entries.
     */
    for (entcount = 0, i = 0; i < NHASHENT; i++) {
	for (entry = ntohs(dhp->hashTable[i]); entry; entry = ne) {
	    /* Verify that the entry is within range */
	    if (entry < 0 || entry >= maxents) {
		printf("Out-of-range hash id %d in chain %d.\n", entry, i);
		DRelease(dhp, 0);
		return 0;
	    }

	    /* Read the directory entry */
	    DErrno = 0;
	    ep = GetBlob(file, entry);
	    if (!ep) {
		if (DErrno != 0) {
		    /* something went wrong reading the page, but it wasn't
		     * really something wrong with the dir that we can fix.
		     */
		    printf("Could not get dir blob %d (errno %d)\n", entry,
			   DErrno);
		    DRelease(dhp, 0);
		    Die("dirok3");
		}
		printf("Invalid hash id %d in chain %d.\n", entry, i);
		DRelease(dhp, 0);
		return 0;
	    }
	    ne = ntohs(ep->next);

	    /* There can't be more than maxents entries */
	    if (++entcount >= maxents) {
		printf("Directory's hash chain %d is circular.\n", i);
		DRelease(ep, 0);
		DRelease(dhp, 0);
		return 0;
	    }

	    /* A null name is no good */
	    if (ep->name[0] == '\000') {
		printf("Dir entry %x in chain %d has bogus (null) name.\n",
		       (int)ep, i);
		DRelease(ep, 0);
		DRelease(dhp, 0);
		return 0;
	    }

	    /* The entry flag better be FFIRST */
	    if (ep->flag != FFIRST) {
		printf("Dir entry %x in chain %d has bogus flag field.\n", (int)ep,
		       i);
		DRelease(ep, 0);
		DRelease(dhp, 0);
		return 0;
	    }

	    /* Check the size of the name */
	    j = strlen(ep->name);
	    if (j >= MAXENAME) {	/* MAXENAME counts the null */
		printf("Dir entry %x in chain %d has too-long name.\n", (int)ep,
		       i);
		DRelease(ep, 0);
		DRelease(dhp, 0);
		return 0;
	    }

	    /* The name used up k directory entries, set the bit in our in-memory
	     * freebitmap for each entry used by the name.
	     */
	    k = NameBlobs(ep->name);
	    for (j = 0; j < k; j++) {
		eaMap[(entry + j) >> 3] |= (1 << ((entry + j) & 7));
	    }

	    /* Hash the name and make sure it is in the correct name hash */
	    if ((j = DirHash(ep->name)) != i) {
		printf
		    ("Dir entry %x should be in hash bucket %d but IS in %d.\n",
		     (int)ep, j, i);
		DRelease(ep, 0);
		DRelease(dhp, 0);
		return 0;
	    }

	    /* Check that if this is entry 13 (the 1st entry), then name must be "." */
	    if (entry == 13) {
		if (strcmp(ep->name, ".") == 0) {
		    havedot = 1;
		} else {
		    printf
			("Dir entry %x, index 13 has name '%s' should be '.'\n",
			 (int)ep, ep->name);
		    DRelease(ep, 0);
		    DRelease(dhp, 0);
		    return 0;
		}
	    }

	    /* Check that if this is entry 14 (the 2nd entry), then name must be ".." */
	    if (entry == 14) {
		if (strcmp(ep->name, "..") == 0) {
		    havedotdot = 1;
		} else {
		    printf
			("Dir entry %x, index 14 has name '%s' should be '..'\n",
			 (int)ep, ep->name);
		    DRelease(ep, 0);
		    DRelease(dhp, 0);
		    return 0;
		}
	    }

	    /* CHECK FOR DUPLICATE NAMES? */

	    DRelease(ep, 0);
	}
    }

    /* Verify that we found '.' and '..' in the correct place */
    if (!havedot || !havedotdot) {
	printf
	    ("Directory entry '.' or '..' does not exist or is in the wrong index.\n");
	DRelease(dhp, 0);
	return 0;
    }

    /* The in-memory freebit map has been computed.  Check that it
     * matches the one in the page header.
     * Note that if this matches, alloMap has already been checked against it.
     */
    for (i = 0; i < usedPages; i++) {
	pp = DRead(file, i);
	if (!pp) {
	    printf
		("Failed on second attempt to read dir page %d (errno %d)\n",
		 i, DErrno);
	    DRelease(dhp, 0);
	    /* if DErrno is 0, then the dir is really bad, and we return dir *not* OK.
	     * otherwise, we want to return true (1), meaning the dir isn't known
	     * to be bad (we can't tell, since I/Os are failing.
	     */
	    if (DErrno != 0)
		Die("dirok4");
	    else
		return 0;	/* dir is really shorter */
	}

	count = i * (EPP / 8);
	for (j = 0; j < EPP / 8; j++) {
	    if (eaMap[count + j] != pp->freebitmap[j]) {
		printf
		    ("Entry freebitmap error, page %d, map offset %d, %x should be %x.\n",
		     i, j, pp->freebitmap[j], eaMap[count + j]);
		DRelease(pp, 0);
		DRelease(dhp, 0);
		return 0;
	    }
	}

	DRelease(pp, 0);
    }

    /* Finally cleanup and return. */
    DRelease(dhp, 0);
    return 1;
}

/* This routine is called with six parameters.  The first is the id of 
 * the original, currently suspect, directory.  The second is the file 
 * id of the place the salvager should place the new, fixed, directory. 
 * The third and the fourth parameters are the vnode number and the
 * uniquifier of the currently suspect directory. The fifth and the
 * sixth parameters are the vnode number and the uniquifier of the
 * parent directory.
 */
int
DirSalvage(fromFile, toFile, vn, vu, pvn, pvu)
     char *fromFile, *toFile;
     afs_int32 vn, vu, pvn, pvu;
{
    /* First do a MakeDir on the target. */
    afs_int32 dot[3], dotdot[3], lfid[3], code, usedPages;
    char tname[256];
    register int i;
    register char *tp;
    struct DirHeader *dhp;
    struct DirEntry *ep;
    int entry;

    memset(dot, 0, sizeof(dot));
    memset(dotdot, 0, sizeof(dotdot));
    dot[1] = vn;
    dot[2] = vu;
    dotdot[1] = pvn;
    dotdot[2] = pvu;

    MakeDir(toFile, dot, dotdot);	/* Returns no error code. */

    /* Find out how many pages are valid, using stupid heuristic since DRead
     * never returns null.
     */
    dhp = (struct DirHeader *)DRead(fromFile, 0);
    if (!dhp) {
	printf("Failed to read first page of fromDir!\n");
	/* if DErrno != 0, then our call failed and we should let our
	 * caller know that there's something wrong with the new dir.  If not,
	 * then we return here anyway, with an empty, but at least good, directory.
	 */
	return DErrno;
    }

    usedPages = ComputeUsedPages(dhp);

    /* Finally, enumerate all the entries, doing a create on them. */
    for (i = 0; i < NHASHENT; i++) {
	entry = ntohs(dhp->hashTable[i]);
	while (1) {
	    if (!entry)
		break;
	    if (entry < 0 || entry >= usedPages * EPP) {
		printf
		    ("Warning: bogus hash table entry encountered, ignoring.\n");
		break;
	    }
	    DErrno = 0;
	    ep = GetBlob(fromFile, entry);
	    if (!ep) {
		if (DErrno) {
		    printf
			("can't continue down hash chain (entry %d, errno %d)\n",
			 entry, DErrno);
		    DRelease(dhp, 0);
		    return DErrno;
		}
		printf
		    ("Warning: bogus hash chain encountered, switching to next.\n");
		break;
	    }
	    strncpy(tname, ep->name, MAXENAME);
	    tname[MAXENAME - 1] = '\000';	/* just in case */
	    tp = tname;

	    entry = ntohs(ep->next);

	    if ((strcmp(tp, ".") != 0) && (strcmp(tp, "..") != 0)) {
		lfid[1] = ntohl(ep->fid.vnode);
		lfid[2] = ntohl(ep->fid.vunique);
		code = Create(toFile, tname, lfid);
		if (code) {
		    printf
			("Create of %s returned code %d, skipping to next hash chain.\n",
			 tname, code);
		    DRelease(ep, 0);
		    break;
		}
	    }
	    DRelease(ep, 0);
	}
    }

    /* Clean up things. */
    DRelease(dhp, 0);
    return 0;
}
