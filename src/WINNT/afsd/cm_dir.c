/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <osi.h>
#include "afsd.h"
#ifdef USE_BPLUS
#include "cm_btree.h"
#endif
#include <rx/rx.h>


afs_int32 DErrno;

afs_uint32 dir_lookup_hits = 0;
afs_uint32 dir_lookup_misses = 0;
afs_uint32 dir_create_entry = 0;
afs_uint32 dir_remove_entry = 0;

afs_uint64 dir_lookup_time = 0;
afs_uint64 dir_create_time = 0;
afs_uint64 dir_remove_time = 0;

afs_uint64 dir_enums = 0;

afs_int32  cm_BPlusTrees = 1;

int cm_MemDumpDirStats(FILE *outputFile, char *cookie, int lock)
{
    int zilch;
    char output[128];

    sprintf(output, "%s - Dir Lookup   Hits: %-8d\r\n", cookie, dir_lookup_hits);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    sprintf(output, "%s -            Misses: %-8d\r\n", cookie, dir_lookup_misses);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    sprintf(output, "%s -             Enums: %-8d\r\n", cookie, dir_enums);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    sprintf(output, "%s -            Create: %-8d\r\n", cookie, dir_create_entry);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    sprintf(output, "%s -            Remove: %-8d\r\n", cookie, dir_remove_entry);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    sprintf(output, "%s - Dir Times  Lookup: %-16I64d\r\n", cookie, dir_lookup_time);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    sprintf(output, "%s -            Create: %-16I64d\r\n", cookie, dir_create_time);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    sprintf(output, "%s -            Remove: %-16I64d\r\n", cookie, dir_remove_time);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    return(0);
}

void cm_DirDumpStats(void)
{
    afsi_log("Dir Lookup   Hits: %-8d", dir_lookup_hits);
    afsi_log("           Misses: %-8d", dir_lookup_misses);
    afsi_log("            Enums: %-8d", dir_enums);
    afsi_log("           Create: %-8d", dir_create_entry);
    afsi_log("           Remove: %-8d", dir_remove_entry);

    afsi_log("Dir Times  Lookup: %-16I64d", dir_lookup_time);
    afsi_log("           Create: %-16I64d", dir_create_time);
    afsi_log("           Remove: %-16I64d", dir_remove_time);
}


/* Local static prototypes */
static long
cm_DirGetBlob(cm_dirOp_t * op,
              unsigned int blobno, cm_buf_t ** bufferpp, cm_dirEntry_t ** blobpp);

static long
cm_DirFindItem(cm_dirOp_t * op,
               char *ename,
               cm_buf_t ** itembufpp, cm_dirEntry_t ** itempp,
               cm_buf_t ** prevbufpp, unsigned short **previtempp);

static long
cm_DirOpAddBuffer(cm_dirOp_t * op, cm_buf_t * buffer);

/* flags for cm_DirOpDelBuffer */
#define DIROP_MODIFIED  1
#define DIROP_SCPLOCKED 2

static int
cm_DirOpDelBuffer(cm_dirOp_t * op, cm_buf_t * buffer, int flags);

static long
cm_DirCheckStatus(cm_dirOp_t * op, afs_uint32 locked);

static long
cm_DirReleasePage(cm_dirOp_t * op, cm_buf_t ** bufferpp, int modified);

static long
cm_DirGetPage(cm_dirOp_t * op,
              long index, cm_buf_t ** bufferpp, void ** datapp);

static long
cm_DirFindBlobs(cm_dirOp_t * op, int nblobs);

static long
cm_DirAddPage(cm_dirOp_t * op, int pageno);

static long
cm_DirFreeBlobs(cm_dirOp_t * op, int firstblob, int nblobs);


/* compute how many 32 byte entries an AFS 3 dir requires for storing
 * the specified name.
 */
long 
cm_NameEntries(char *namep, long *lenp)
{
    long i;
        
    i = (long)strlen(namep);
    if (lenp) *lenp = i;
    return 1 + ((i+16) >> 5);
}

/* Create an entry in a file.  Dir is a file representation, while
   entry is a string name.

   On entry:
       op->scp->mx is unlocked

   On exit:
       op->scp->mx is unlocked

   None of the directory buffers for op->scp should be locked by the
   calling thread.
*/
long
cm_DirCreateEntry(cm_dirOp_t * op, char *entry, cm_fid_t * cfid)
{
    int blobs, firstelt;
    int i;
    LARGE_INTEGER start, end;

    cm_dirEntry_t *ep = NULL;
    cm_buf_t *entrybuf = NULL;

    unsigned short *pp = NULL;
    cm_buf_t *prevptrbuf = NULL;

    cm_dirHeader_t *dhp = NULL;
    cm_buf_t *dhpbuf = NULL;

    long code = 0;

    /* check name quality */
    if (*entry == 0)
	return EINVAL;

    QueryPerformanceCounter(&start);

    dir_create_entry++;

    osi_Log4(afsd_logp, "cm_DirCreateEntry for op 0x%p, name [%s] and fid[%d,%d]",
             op, osi_LogSaveString(afsd_logp, entry), cfid->vnode, cfid->unique);

    /* First check if file already exists. */
    code = cm_DirFindItem(op,
                          entry,
                          &entrybuf, &ep,
                          &prevptrbuf, &pp);
    if (code == 0) {
        cm_DirReleasePage(op, &entrybuf, FALSE);
        cm_DirReleasePage(op, &prevptrbuf, FALSE);
	code = EEXIST;
        goto done;
    }

    blobs = cm_NameEntries(entry, NULL);	/* number of entries required */
    firstelt = cm_DirFindBlobs(op, blobs);
    if (firstelt < 0) {
        osi_Log0(afsd_logp, "cm_DirCreateEntry returning EFBIG");
	code = EFBIG;		/* directory is full */
        goto done;
    }

    /* First, we fill in the directory entry. */
    code = cm_DirGetBlob(op, firstelt, &entrybuf, &ep);
    if (code != 0) {
	code = EIO;
        goto done;
    }

    ep->flag = CM_DIR_FFIRST;
    ep->fid.vnode = htonl(cfid->vnode);
    ep->fid.unique = htonl(cfid->unique);
    strcpy(ep->name, entry);

    /* Now we just have to thread it on the hash table list. */
    code = cm_DirGetPage(op, 0, &dhpbuf, &dhp);
    if (code != 0) {
	cm_DirReleasePage(op, &entrybuf, TRUE);
	code = EIO;
        goto done;
    }

    i = cm_DirHash(entry);

    ep->next = dhp->hashTable[i];
    dhp->hashTable[i] = htons(firstelt);

    cm_DirReleasePage(op, &dhpbuf, TRUE);
    cm_DirReleasePage(op, &entrybuf, TRUE);

    osi_Log0(afsd_logp, "cm_DirCreateEntry returning success");

    code = 0;
  done:
    QueryPerformanceCounter(&end);

    dir_create_time += (end.QuadPart - start.QuadPart);
    return code;
}

/* Return the length of a directory in pages

   On entry:
       op->scp->mx is locked

   On exit:
       op->scp->mx is locked

   The first directory page for op->scp should not be locked by the
   calling thread.
*/
int
cm_DirLength(cm_dirOp_t * op)
{
    int i, ctr;
    cm_dirHeader_t *dhp = NULL;
    cm_buf_t       *dhpbuf = NULL;

    long code;

    code = cm_DirGetPage(op, 0, &dhpbuf, &dhp);
    if (code != 0)
	return 0;

    if (dhp->header.pgcount != 0)
	ctr = ntohs(dhp->header.pgcount);
    else {
	/* old style, count the pages */
	ctr = 0;
	for (i = 0; i < CM_DIR_MAXPAGES; i++)
	    if (dhp->alloMap[i] != CM_DIR_EPP)
		ctr++;
    }
    cm_DirReleasePage(op, &dhpbuf, FALSE);
    return ctr * CM_DIR_PAGESIZE;
}

/* Delete a directory entry.

   On entry:
       op->scp->mx is unlocked

   On exit:
       op->scp->mx is unlocked

   None of the directory buffers for op->scp should be locked by the
   calling thread.
 */
int
cm_DirDeleteEntry(cm_dirOp_t * op, char *entry)
{
    /* Delete an entry from a directory, including update of all free
       entry descriptors. */

    int nitems, index;
    cm_dirEntry_t *firstitem = NULL;
    cm_buf_t      *itembuf = NULL;
    unsigned short *previtem = NULL;
    cm_buf_t      *pibuf = NULL;
    osi_hyper_t    thyper;
    unsigned long  junk;
    long code;
    LARGE_INTEGER start, end;

    QueryPerformanceCounter(&start);

    osi_Log2(afsd_logp, "cm_DirDeleteEntry for op 0x%p, entry [%s]",
             op, osi_LogSaveString(afsd_logp, entry));

    code = cm_DirFindItem(op, entry,
                          &itembuf, &firstitem,
                          &pibuf, &previtem);
    if (code != 0) {
        osi_Log0(afsd_logp, "cm_DirDeleteEntry returning ENOENT");
	code = ENOENT;
        goto done;
    }

    dir_remove_entry++;

    *previtem = firstitem->next;
    cm_DirReleasePage(op, &pibuf, TRUE);

    thyper = itembuf->offset;
    thyper = LargeIntegerAdd(thyper,
                             ConvertLongToLargeInteger(((char *) firstitem) - itembuf->datap));
    thyper = ExtendedLargeIntegerDivide(thyper, 32, &junk);

    index = thyper.LowPart;
    osi_assert(thyper.HighPart == 0);

    nitems = cm_NameEntries(firstitem->name, NULL);
    cm_DirReleasePage(op, &itembuf, FALSE);

    cm_DirFreeBlobs(op, index, nitems);

    osi_Log0(afsd_logp, "cm_DirDeleteEntry returning success");
    code = 0;

  done:
    QueryPerformanceCounter(&end);

    dir_remove_time += (end.QuadPart - start.QuadPart);

    return code;
}

/* Find a bunch of contiguous entries; at least nblobs in a row.

   Called with op->scp->mx */
static long
cm_DirFindBlobs(cm_dirOp_t * op, int nblobs)
{
    int i, j, k;
    int failed = 0;

    cm_dirHeader_t *dhp = NULL;
    cm_buf_t *dhpbuf = NULL;
    int dhpModified = FALSE;

    cm_pageHeader_t *pp = NULL;
    cm_buf_t *pagebuf = NULL;
    int pageModified = FALSE;

    int pgcount;

    long code;

    osi_Log2(afsd_logp, "cm_DirFindBlobs for op 0x%p, nblobs = %d",
             op, nblobs);

    code = cm_DirGetPage(op, 0, &dhpbuf, (void **) &dhp);
    if (code)
	return -1;

    for (i = 0; i < CM_DIR_BIGMAXPAGES; i++) {
	if (i >= CM_DIR_MAXPAGES || dhp->alloMap[i] >= nblobs) {
	    /* if page could contain enough entries */
	    /* If there are CM_DIR_EPP free entries, then the page is
               not even allocated. */
	    if (i >= CM_DIR_MAXPAGES) {

		/* this pages exists past the end of the old-style dir */
		pgcount = ntohs(dhp->header.pgcount);
		if (pgcount == 0) {
		    pgcount = CM_DIR_MAXPAGES;
		    dhp->header.pgcount = htons(pgcount);
                    dhpModified = TRUE;
		}

		if (i > pgcount - 1) {
		    /* this page is bigger than last allocated page */
                    cm_DirAddPage(op, i);
		    dhp->header.pgcount = htons(i + 1);
                    dhpModified = TRUE;
		}
	    } else if (dhp->alloMap[i] == CM_DIR_EPP) {
		/* Add the page to the directory. */
		cm_DirAddPage(op, i);
		dhp->alloMap[i] = CM_DIR_EPP - 1;
		dhp->header.pgcount = htons(i + 1);
                dhpModified = TRUE;
	    }

            code = cm_DirGetPage(op, i, &pagebuf, &pp);
            if (code) {
                cm_DirReleasePage(op, &dhpbuf, dhpModified);
                break;
            }

	    for (j = 0; j <= CM_DIR_EPP - nblobs; j++) {
		failed = 0;
		for (k = 0; k < nblobs; k++)
		    if ((pp->freeBitmap[(j + k) >> 3] >> ((j + k) & 7)) & 1) {
			failed = 1;
			break;
		    }
		if (!failed)
		    break;
		failed = 1;
	    }

	    if (!failed) {
		/* Here we have the first index in j.  We update the allocation maps
		 * and free up any resources we've got allocated. */
		if (i < CM_DIR_MAXPAGES) {
		    dhp->alloMap[i] -= nblobs;
                    dhpModified = TRUE;
                }

                cm_DirReleasePage(op, &dhpbuf, dhpModified);

		for (k = 0; k < nblobs; k++)
		    pp->freeBitmap[(j + k) >> 3] |= 1 << ((j + k) & 7);

                cm_DirReleasePage(op, &pagebuf, TRUE);

                osi_Log0(afsd_logp, "cm_DirFindBlobs returning success");

		return j + i * CM_DIR_EPP;
	    }
            cm_DirReleasePage(op, &pagebuf, pageModified);
	}
    }

    /* If we make it here, the directory is full. */
    osi_Log0(afsd_logp, "cm_DirFindBlobs directory is full");
    cm_DirReleasePage(op, &dhpbuf, dhpModified);
    return -1;
}

/* Add a page to a directory. 

   Called with op->scp->mx
*/
static long
cm_DirAddPage(cm_dirOp_t * op, int pageno)
{
    int i;
    cm_pageHeader_t *pp = NULL;
    cm_buf_t *pagebuf = NULL;
    long code = 0;

    osi_Log2(afsd_logp, "cm_DirAddPage for op 0x%p, pageno=%d", op, pageno);

    code = cm_DirGetPage(op, pageno, &pagebuf, (void **) &pp);
    if (code != 0)
        return code;

    pp->tag = htons(1234);
    if (pageno > 0)
	pp->pgcount = 0;
    pp->freeCount = CM_DIR_EPP - 1; /* The first dude is already allocated */
    pp->freeBitmap[0] = 0x01;
    for (i = 1; i < CM_DIR_EPP / 8; i++) /* It's a constant */
	pp->freeBitmap[i] = 0;

    cm_DirReleasePage(op, &pagebuf, TRUE);

    osi_Log0(afsd_logp, "cm_DirAddPage returning success");

    return code;
}

/* Free a whole bunch of directory entries.

   Called with op->scp->mx
*/
static long
cm_DirFreeBlobs(cm_dirOp_t * op, int firstblob, int nblobs)
{
    int i;
    int page;

    cm_dirHeader_t *dhp = NULL;
    cm_buf_t       *dhpbuf = NULL;
    int             dhpmodified = FALSE;

    cm_pageHeader_t *pp = NULL;
    cm_buf_t        *pagebuf = NULL;
    long code = 0;

    osi_Log3(afsd_logp, "cm_DirFreeBlobs for op 0x%p, firstblob=%d, nblobs=%d",
             op, firstblob, nblobs);

    page = firstblob / CM_DIR_EPP;
    firstblob -= CM_DIR_EPP * page;	/* convert to page-relative entry */

    code = cm_DirGetPage(op, 0, &dhpbuf, &dhp);
    if (code)
        return code;

    if (page < CM_DIR_MAXPAGES) {
	dhp->alloMap[page] += nblobs;
        dhpmodified = TRUE;
    }

    cm_DirReleasePage(op, &dhpbuf, dhpmodified);

    code = cm_DirGetPage(op, page, &pagebuf, &pp);
    if (code == 0) {
	for (i = 0; i < nblobs; i++)
	    pp->freeBitmap[(firstblob + i) >> 3] &=
		~(1 << ((firstblob + i) & 7));
        cm_DirReleasePage(op, &pagebuf, TRUE);
    }

    osi_Log1(afsd_logp, "cm_DirFreeBlobs returning code 0x%x", code);

    return code;
}

/*
 * Format an empty directory properly.  Note that the first 13 entries in a
 * directory header page are allocated, 1 to the page header, 4 to the
 * allocation map and 8 to the hash table.
 *
 * Called with op->scp->mx unlocked
 */
int
cm_DirMakeDir(cm_dirOp_t * op, cm_fid_t * me, cm_fid_t * parent)
{
    int i;
    cm_dirHeader_t *dhp = NULL;
    cm_buf_t *dhpbuf = NULL;
    int rc = 0;
    long code;

    osi_Log3(afsd_logp, "cm_DirMakeDir for op 0x%p, directory fid[%d, %d]",
             op, me->vnode, me->unique);
    osi_Log2(afsd_logp, "              parent[%d, %d]",
             parent->vnode, parent->unique);

    code = cm_DirGetPage(op, 0, &dhpbuf, &dhp);
    if (code) {
        rc = 1;
        goto done;
    }

    dhp->header.pgcount = htons(1);
    dhp->header.tag = htons(1234);
    dhp->header.freeCount = (CM_DIR_EPP - CM_DIR_DHE - 1);
    dhp->header.freeBitmap[0] = 0xff;
    dhp->header.freeBitmap[1] = 0x1f;
    for (i = 2; i < CM_DIR_EPP / 8; i++)
	dhp->header.freeBitmap[i] = 0;
    dhp->alloMap[0] = (CM_DIR_EPP - CM_DIR_DHE - 1);
    for (i = 1; i < CM_DIR_MAXPAGES; i++)
	dhp->alloMap[i] = CM_DIR_EPP;
    for (i = 0; i < CM_DIR_NHASHENT; i++)
	dhp->hashTable[i] = 0;

    cm_DirReleasePage(op, &dhpbuf, TRUE);

    cm_DirCreateEntry(op, ".", me);
    cm_DirCreateEntry(op, "..", parent);	/* Virtue is its own .. */

    osi_Log0(afsd_logp, "cm_DirMakeDir returning success");

  done:
    return rc;
}

/* Look up a file name in directory.

   On entry:
       op->scp->mx is unlocked

   On exit:
       op->scp->mx is unlocked

   None of the directory buffers for op->scp should be locked by the
   calling thread.
*/
int
cm_DirLookup(cm_dirOp_t * op, char *entry, cm_fid_t * cfid)
{
    cm_dirEntry_t *firstitem = NULL;
    cm_buf_t      *itembuf = NULL;
    unsigned short *previtem = NULL;
    cm_buf_t      *pibuf = NULL;
    long code;
    LARGE_INTEGER       start;
    LARGE_INTEGER       end;

    QueryPerformanceCounter(&start);

    osi_Log2(afsd_logp, "cm_DirLookup for op 0x%p, entry[%s]",
             op, osi_LogSaveString(afsd_logp, entry));

    code = cm_DirFindItem(op, entry,
                          &itembuf, &firstitem,
                          &pibuf, &previtem);
    if (code != 0) {
        dir_lookup_misses++;
        code = ENOENT;
        goto done;
    }

    cm_DirReleasePage(op, &pibuf, FALSE);

    cfid->cell = op->scp->fid.cell;
    cfid->volume = op->scp->fid.volume;
    cfid->vnode = ntohl(firstitem->fid.vnode);
    cfid->unique = ntohl(firstitem->fid.unique);

    cm_DirReleasePage(op, &itembuf, FALSE);

    osi_Log2(afsd_logp, "cm_DirLookup returning fid[%d,%d]",
             cfid->vnode, cfid->unique);

    dir_lookup_hits++;
    code = 0;

  done:
    QueryPerformanceCounter(&end);

    dir_lookup_time += (end.QuadPart - start.QuadPart);

    return code;
}

/* Look up a file name in directory.

   On entry:
       op->scp->mx is locked

   On exit:
       op->scp->mx is locked

   None of the directory buffers for op->scp should be locked by the
   calling thread.
*/
int
cm_DirLookupOffset(cm_dirOp_t * op, char *entry, cm_fid_t *cfid, osi_hyper_t *offsetp)
{
    cm_dirEntry_t *firstitem = NULL;
    cm_buf_t      *itembuf = NULL;
    unsigned short *previtem = NULL;
    cm_buf_t      *pibuf = NULL;

    long code;

    osi_Log2(afsd_logp, "cm_DirLookupOffset for op 0x%p, entry[%s]",
             op, osi_LogSaveString(afsd_logp, entry));

    code = cm_DirFindItem(op, entry,
                          &itembuf, &firstitem,
                          &pibuf, &previtem);
    if (code != 0)
        return ENOENT;

    cm_DirReleasePage(op, &pibuf, FALSE);

    cfid->cell = op->scp->fid.cell;
    cfid->volume = op->scp->fid.volume;
    cfid->vnode = ntohl(firstitem->fid.vnode);
    cfid->unique = ntohl(firstitem->fid.unique);
    if (offsetp) {
        osi_hyper_t thyper;

        thyper = itembuf->offset;
        thyper = LargeIntegerAdd(thyper,
                                 ConvertLongToLargeInteger(((char *) firstitem) - itembuf->datap));

	*offsetp = thyper;
    }

    cm_DirReleasePage(op, &itembuf, FALSE);

    osi_Log2(afsd_logp, "cm_DirLookupOffset returning fid[%d,%d]",
             cfid->vnode, cfid->unique);
    if (offsetp) {
        osi_Log2(afsd_logp, "               offset [%x:%x]",
                 offsetp->HighPart, offsetp->LowPart);
    }

    return 0;
}

/* Apply a function to every directory entry in a directory.

   On entry:
       op->scp->mx is locked

   On exit:
       op->scp->mx is locked

   None of the directory buffers for op->scp should be locked by the
   calling thread.

   The hook function cannot modify or lock any directory buffers.
 */
int
cm_DirApply(cm_dirOp_t * op, int (*hookproc) (void *, char *, long, long), void *hook)
{
    /* Enumerate the contents of a directory. */
    int i;
    int num;

    cm_dirHeader_t *dhp = NULL;
    cm_buf_t       *dhpbuf = NULL;

    cm_dirEntry_t  *ep = NULL;
    cm_buf_t       *epbuf = NULL;

    long code = 0;

    code = cm_DirGetPage(op, 0, &dhpbuf, &dhp);
    if (code != 0)
        return EIO;

    for (i = 0; i < CM_DIR_NHASHENT; i++) {
	/* For each hash chain, enumerate everyone on the list. */
	num = ntohs(dhp->hashTable[i]);
	while (num != 0) {
	    /* Walk down the hash table list. */
	    code = cm_DirGetBlob(op, num, &epbuf, &ep);
	    if (code != 0) {
                cm_DirReleasePage(op, &dhpbuf, FALSE);
                return code;
	    }

	    num = ntohs(ep->next);
	    (*hookproc) (hook, ep->name, ntohl(ep->fid.vnode),
			 ntohl(ep->fid.unique));

            cm_DirReleasePage(op, &epbuf, FALSE);
	}
    }
    cm_DirReleasePage(op, &dhpbuf, FALSE);

    return 0;
}

/* Check if a directory is empty

   On entry:
       op->scp->mx is locked

   On exit:
       op->scp->mx is locked

   None of the directory buffers for op->scp should be locked by the
   calling thread.
 */
int
cm_DirIsEmpty(cm_dirOp_t * op)
{
    /* Enumerate the contents of a directory. */
    int i;
    int num;

    cm_dirHeader_t *dhp = NULL;
    cm_buf_t       *dhpbuf = NULL;

    cm_dirEntry_t  *ep = NULL;
    cm_buf_t       *epbuf = NULL;

    long code = 0;

    code = cm_DirGetPage(op, 0, &dhpbuf, &dhp);
    if (code != 0)
        return 0;

    for (i = 0; i < CM_DIR_NHASHENT; i++) {
	/* For each hash chain, enumerate everyone on the list. */
	num = ntohs(dhp->hashTable[i]);

	while (num != 0) {
	    /* Walk down the hash table list. */
	    code = cm_DirGetBlob(op, num, &epbuf, &ep);
	    if (code != 0)
		break;

	    if (strcmp(ep->name, "..") && strcmp(ep->name, ".")) {
                cm_DirReleasePage(op, &epbuf, FALSE);
                cm_DirReleasePage(op, &dhpbuf, FALSE);
		return 1;
	    }
	    num = ntohs(ep->next);
	    cm_DirReleasePage(op, &epbuf, FALSE);
	}
    }
    cm_DirReleasePage(op, &dhpbuf, FALSE);
    return 0;
}

/* Return a pointer to an entry, given its number.

   On entry:
     scp->mx locked
     if *bufferpp != NULL, then *bufferpp->mx is locked

   During:
     scp->mx may be unlocked
     *bufferpp may be released

   On exit:
     scp->mx locked
     if *bufferpp != NULL, then *bufferpp->mx is locked

     *bufferpp should be released via cm_DirReleasePage() or any other
     *call that releases a directory buffer.
*/
static long
cm_DirGetBlob(cm_dirOp_t * op,
              unsigned int blobno, cm_buf_t ** bufferpp, cm_dirEntry_t ** blobpp)
{
    unsigned char * ep;
    long code = 0;

    osi_Log2(afsd_logp, "cm_DirGetBlob for op 0x%p, blobno=%d",
             op, blobno);

    code = cm_DirGetPage(op, blobno >> CM_DIR_LEPP,
                         bufferpp, (void **) &ep);
    if (code != 0)
        return code;

    *blobpp = (cm_dirEntry_t *) (ep + 32 * (blobno & (CM_DIR_EPP - 1)));

    return code;
}	

int
cm_DirHash(char *string)
{
    /* Hash a string to a number between 0 and NHASHENT. */
    register unsigned char tc;
    register int hval;
    register int tval;
    hval = 0;
    while ((tc = (*string++))) {
	hval *= 173;
	hval += tc;
    }
    tval = hval & (CM_DIR_NHASHENT - 1);
    if (tval == 0)
	return tval;
    else if (hval < 0)
	tval = CM_DIR_NHASHENT - tval;
    return tval;
}

/* Find a directory entry, given its name.  This entry returns a
 * pointer to a locked buffer, and a pointer to a locked buffer (in
 * previtem) referencing the found item (to aid the delete code).  If
 * no entry is found, however, no items are left locked, and a null
 * pointer is returned instead.
 *
 * On entry:
 *  scp->mx locked
 *
 * On exit:
 *  scp->mx locked
 */
static long
cm_DirFindItem(cm_dirOp_t * op,
               char *ename,
               cm_buf_t ** itembufpp, cm_dirEntry_t ** itempp,
               cm_buf_t ** prevbufpp, unsigned short **previtempp)
{
    int                  i;
    cm_dirHeader_t      *dhp = NULL;
    unsigned short      *lp = NULL;
    cm_dirEntry_t       *tp = NULL;
    cm_buf_t            *hashbufp = NULL;
    cm_buf_t            *itembufp = NULL;
    long code = 0;

    osi_Log2(afsd_logp, "cm_DirFindItem for op 0x%p, entry[%s]",
             op, osi_LogSaveString(afsd_logp, ename));

    i = cm_DirHash(ename);

    if (op->scp->fileType != CM_SCACHETYPE_DIRECTORY) {
        osi_Log0(afsd_logp, "cm_DirFindItem: The scp is not a directory");
        return CM_ERROR_INVAL;
    }

    code = cm_DirGetPage(op, 0, &hashbufp, (void **) &dhp);
    if (code != 0) {
	return code;
    }

    if (dhp->hashTable[i] == 0) {
	/* no such entry */
        osi_Log1(afsd_logp, "cm_DirFindItem: Hash bucket %d is empty", i);
	cm_DirReleasePage(op, &hashbufp, FALSE);
	return ENOENT;
    }

    code = cm_DirGetBlob(op,
                         (u_short) ntohs(dhp->hashTable[i]),
                         &itembufp, &tp);
    if (code != 0) {
        cm_DirReleasePage(op, &hashbufp, FALSE);
	return code;
    }

    lp = &(dhp->hashTable[i]);

    /* loop invariant:

       lp       : pointer to blob number of entry we are looking at
       hashbufp : buffer containing lp
       tp       : pointer to entry we are looking at
       itembufp : buffer containing tp
     */
    while (1) {
	/* Look at each hash conflict entry. */
	if (!strcmp(ename, tp->name)) {
            osi_Log0(afsd_logp, "cm_DirFindItem: returning success");
	    /* Found our entry. */
	    *previtempp = lp;
            *prevbufpp = hashbufp;
            *itempp = tp;
            *itembufpp = itembufp;
	    return 0;
	}

	lp = &(tp->next);
        cm_DirReleasePage(op, &hashbufp, FALSE);
        hashbufp = itembufp;

        itembufp = NULL;
        tp = NULL;

	if (*lp == 0) {
	    /* The end of the line */
            osi_Log0(afsd_logp, "cm_DirFindItem: returning ENOENT");
	    cm_DirReleasePage(op, &hashbufp, FALSE);
	    return ENOENT;
	}

	code = cm_DirGetBlob(op,
                             (u_short) ntohs(*lp),
                             &itembufp, &tp);

	if (code != 0) {
	    cm_DirReleasePage(op, &hashbufp, FALSE);
	    return code;
	}
    }
}

/* Begin a sequence of directory operations.  
 * Called with scp->mx unlocked.
 */
long
cm_BeginDirOp(cm_scache_t * scp, cm_user_t * userp, cm_req_t * reqp,
              afs_uint32 lockType, cm_dirOp_t * op)
{
    long code;
    int i, mxheld = 0, haveWrite = 0;

    osi_Log3(afsd_logp, "Beginning dirOp[0x%p] for scp[0x%p], userp[0x%p]",
             op, scp, userp);

    memset(op, 0, sizeof(*op));

    cm_HoldSCache(scp);
    op->scp = scp;
    cm_HoldUser(userp);
    op->userp = userp;
    cm_InitReq(&op->req);

    op->dirtyBufCount = 0;
    op->nBuffers = 0;

    for (i=0; i < CM_DIROP_MAXBUFFERS; i++) {
        op->buffers[i].flags = 0;
    }

    if (lockType == CM_DIRLOCK_WRITE) {
        lock_ObtainWrite(&scp->dirlock);
        haveWrite = 1;
    } else { 
        lock_ObtainRead(&scp->dirlock);
        haveWrite = 0;
    }
    lock_ObtainMutex(&scp->mx);
    mxheld = 1;
    code = cm_DirCheckStatus(op, 1);
    if (code == 0) {
        op->length = scp->length;
        op->newLength = op->length;
        op->dataVersion = scp->dataVersion;
        op->newDataVersion = op->dataVersion;

#ifdef USE_BPLUS
        if (!cm_BPlusTrees ||
            (scp->dirBplus &&
             scp->dirDataVersion == scp->dataVersion)) 
        {
            /* we know that haveWrite matches lockType at this point */
            switch (lockType) {
            case CM_DIRLOCK_NONE:
                if (haveWrite)
                    lock_ReleaseWrite(&scp->dirlock);
                else
                    lock_ReleaseRead(&scp->dirlock);
                break;
            case CM_DIRLOCK_READ:
                osi_assert(!haveWrite);
                break;
            case CM_DIRLOCK_WRITE:
            default:
                osi_assert(haveWrite);
            }
        } else {
            if (!(scp->dirBplus && 
                  scp->dirDataVersion == scp->dataVersion)) 
            {
              repeat:
                if (!haveWrite) {
                    if (mxheld) {
                        lock_ReleaseMutex(&scp->mx);
                        mxheld = 0;
                    }
                    lock_ReleaseRead(&scp->dirlock);
                    lock_ObtainWrite(&scp->dirlock);
                    haveWrite = 1;
                }
                if (!mxheld) {
                    lock_ObtainMutex(&scp->mx);
                    mxheld = 1;
                }
                if (scp->dirBplus && 
                     scp->dirDataVersion != scp->dataVersion)
                {
                    bplus_dv_error++;
                    bplus_free_tree++;
                    freeBtree(scp->dirBplus);
                    scp->dirBplus = NULL;
                    scp->dirDataVersion = -1;
                }

                if (!scp->dirBplus) {
                    if (mxheld) {
                        lock_ReleaseMutex(&scp->mx);
                        mxheld = 0;
                    }
                    cm_BPlusDirBuildTree(scp, userp, reqp);
                    if (!mxheld) {
                        lock_ObtainMutex(&scp->mx);
                        mxheld = 1;
                    }
                    if (op->dataVersion != scp->dataVersion) {
                        /* We lost the race, therefore we must update the
                         * dirop state and retry to build the tree.
                         */
                        op->length = scp->length;
                        op->newLength = op->length;
                        op->dataVersion = scp->dataVersion;
                        op->newDataVersion = op->dataVersion;
                        goto repeat;
                    }

                    if (scp->dirBplus)
                        scp->dirDataVersion = scp->dataVersion;
                }
            }

            switch (lockType) {
            case CM_DIRLOCK_NONE:
                lock_ReleaseWrite(&scp->dirlock);
                break;
            case CM_DIRLOCK_READ:
                lock_ConvertWToR(&scp->dirlock);
                break;
            case CM_DIRLOCK_WRITE:
            default:
                /* got it already */;
            }
            haveWrite = 0;
        }
#else
        /* we know that haveWrite matches lockType at this point */
        switch (lockType) {
        case CM_DIRLOCK_NONE:
            if (haveWrite)
                lock_ReleaseWrite(&scp->dirlock);
            else
                lock_ReleaseRead(&scp->dirlock);
            break;
        case CM_DIRLOCK_READ:
            osi_assert(!haveWrite);
            break;
        case CM_DIRLOCK_WRITE:
        default:
            osi_assert(haveWrite);
        }
#endif
        op->lockType = lockType;
        if (mxheld)
            lock_ReleaseMutex(&scp->mx);
    } else {
        if (haveWrite)
            lock_ReleaseWrite(&scp->dirlock);
        else
            lock_ReleaseRead(&scp->dirlock);
        if (mxheld)
            lock_ReleaseMutex(&scp->mx);
        cm_EndDirOp(op);
    }

    return code;
}

/* Check if it is safe for us to perform local directory updates.
   Called with scp->mx unlocked. */
int
cm_CheckDirOpForSingleChange(cm_dirOp_t * op)
{
    long code;
    int  rc = 0;

    if (op->scp == NULL)
        return 0;

    lock_ObtainMutex(&op->scp->mx);
    code = cm_DirCheckStatus(op, 1);

    if (code == 0 &&
        op->dataVersion == op->scp->dataVersion - 1) {
        /* only one set of changes happened between cm_BeginDirOp()
           and this function.  It is safe for us to perform local
           changes. */
        op->newDataVersion = op->scp->dataVersion;
        op->newLength = op->scp->serverLength;

        rc = 1;
    }
    lock_ReleaseMutex(&op->scp->mx); 
    
    if (rc)
        osi_Log0(afsd_logp, "cm_CheckDirOpForSingleChange succeeded");
    else
        osi_Log3(afsd_logp,
                 "cm_CheckDirOpForSingleChange failed.  code=0x%x, old dv=%I64d, new dv=%I64d",
                 code, op->dataVersion, op->scp->dataVersion);
    return rc;
}

/* End a sequence of directory operations.  
 * Called with op->scp->mx unlocked.*/
long
cm_EndDirOp(cm_dirOp_t * op)
{
    long code = 0;

    if (op->scp == NULL)
        return 0;

    osi_Log2(afsd_logp, "Ending dirOp 0x%p with %d dirty buffer releases",
             op, op->dirtyBufCount);

    if (op->dirtyBufCount > 0) {
#ifdef USE_BPLUS
        /* update the data version on the B+ tree */
        if (op->scp->dirBplus && 
             op->scp->dirDataVersion == op->dataVersion) {

            switch (op->lockType) {
            case CM_DIRLOCK_READ:
                lock_ReleaseRead(&op->scp->dirlock);
                /* fall through ... */
            case CM_DIRLOCK_NONE:
                lock_ObtainWrite(&op->scp->dirlock);
                op->lockType = CM_DIRLOCK_WRITE;
                break;
            case CM_DIRLOCK_WRITE:
            default:
                /* already got it */;
            }
            op->scp->dirDataVersion = op->newDataVersion;
        }
#endif

        /* we made changes.  We should go through the list of buffers
         * and update the dataVersion for each. */
        lock_ObtainMutex(&op->scp->mx);
        code = buf_ForceDataVersion(op->scp, op->dataVersion, op->newDataVersion);
        lock_ReleaseMutex(&op->scp->mx);
    }

    switch (op->lockType) {
    case CM_DIRLOCK_NONE:
        break;
    case CM_DIRLOCK_READ:
        lock_ReleaseRead(&op->scp->dirlock);
        break;
    case CM_DIRLOCK_WRITE:
    default:
        lock_ReleaseWrite(&op->scp->dirlock);
    }

    if (op->scp)
        cm_ReleaseSCache(op->scp);
    op->scp = NULL;

    if (op->userp)
        cm_ReleaseUser(op->userp);
    op->userp = 0;

    osi_assertx(op->nBuffers == 0, "Buffer leak after dirOp termination");

    return code;
}

/* NOTE: Called without scp->mx and without bufferp->mx */
static long
cm_DirOpAddBuffer(cm_dirOp_t * op, cm_buf_t * bufferp)
{
    int i;
    long code = 0;

    osi_Log2(afsd_logp, "cm_DirOpAddBuffer for op 0x%p, buffer %p", op, bufferp);

    if (bufferp == NULL)
        return -1;

    for (i=0; i < CM_DIROP_MAXBUFFERS; i++) {
        if ((op->buffers[i].flags & CM_DIROPBUFF_INUSE) &&
            op->buffers[i].bufferp == bufferp) {
            break;
        }
    }

    if (i < CM_DIROP_MAXBUFFERS) {
        /* we already have this buffer on our list */

        op->buffers[i].refcount++;
        osi_Log0(afsd_logp,
                 "cm_DirOpAddBuffer: the buffer is already listed for the dirOp");
        return 0;
    } else {
        /* we have to add a new buffer */
        osi_assertx(op->nBuffers < CM_DIROP_MAXBUFFERS - 1,
                    "DirOp has exceeded CM_DIROP_MAXBUFFERS buffers");

        for (i=0; i < CM_DIROP_MAXBUFFERS; i++) {
            if (!(op->buffers[i].flags & CM_DIROPBUFF_INUSE))
                break;
        }

        osi_assert(i < CM_DIROP_MAXBUFFERS);

        lock_ObtainMutex(&bufferp->mx);
        lock_ObtainMutex(&op->scp->mx);

        /* Make sure we are synchronized. */
        osi_assert(op->lockType != CM_DIRLOCK_NONE);

        code = cm_SyncOp(op->scp, bufferp, op->userp, &op->req, PRSFS_LOOKUP,
                         CM_SCACHESYNC_NEEDCALLBACK |
                         (op->lockType == CM_DIRLOCK_WRITE ? CM_SCACHESYNC_WRITE : CM_SCACHESYNC_READ) |
                         CM_SCACHESYNC_BUFLOCKED);

        if (code == 0 && bufferp->dataVersion != op->dataVersion) {
            osi_Log2(afsd_logp, "cm_DirOpAddBuffer: buffer data version mismatch. buf dv = %I64d. needs %I64d", 
                     bufferp->dataVersion, op->dataVersion);

            cm_SyncOpDone(op->scp, bufferp,
                          CM_SCACHESYNC_NEEDCALLBACK |
                         (op->lockType == CM_DIRLOCK_WRITE ? CM_SCACHESYNC_WRITE : CM_SCACHESYNC_READ) |
                          CM_SCACHESYNC_BUFLOCKED);

            code = CM_ERROR_INVAL;
        }

        lock_ReleaseMutex(&op->scp->mx);
        lock_ReleaseMutex(&bufferp->mx);

        if (code) {
            osi_Log1(afsd_logp, "cm_DirOpAddBuffer: failed to sync buffer.  code=0x%x",
                     code);
            return code;
        }

        buf_Hold(bufferp);
        op->buffers[i].bufferp = bufferp;
        op->buffers[i].refcount = 1; /* start with one ref */
        op->buffers[i].flags = CM_DIROPBUFF_INUSE;

        op->nBuffers++;

        osi_Log0(afsd_logp, "cm_DirOpAddBuffer: returning success");

        return 0;
    }
}

/* Note: Called without op->scp->mx */
static int
cm_DirOpFindBuffer(cm_dirOp_t * op, osi_hyper_t offset, cm_buf_t ** bufferpp)
{
    int i;

    for (i=0; i < CM_DIROP_MAXBUFFERS; i++) {
        if ((op->buffers[i].flags & CM_DIROPBUFF_INUSE) &&
            LargeIntegerEqualTo(op->buffers[i].bufferp->offset, offset))
            break;
    }

    if (i < CM_DIROP_MAXBUFFERS) {
        /* found it */
        op->buffers[i].refcount++;
        buf_Hold(op->buffers[i].bufferp);
        *bufferpp = op->buffers[i].bufferp;

        osi_Log2(afsd_logp, "cm_DirOpFindBuffer: found buffer for offset [%x:%x]",
                 offset.HighPart, offset.LowPart);
        return 1;
    }

    osi_Log2(afsd_logp, "cm_DirOpFindBuffer: buffer not found for offset [%x:%x]",
             offset.HighPart, offset.LowPart);
    return 0;
}


/* NOTE: called with scp->mx held or not depending on the flags */
static int
cm_DirOpDelBuffer(cm_dirOp_t * op, cm_buf_t * bufferp, int flags)
{
    int i;

    osi_Log3(afsd_logp, "cm_DirOpDelBuffer for op 0x%p, buffer 0x%p, flags=%d",
             op, bufferp, flags);

    for (i=0; i < CM_DIROP_MAXBUFFERS; i++) {
        if ((op->buffers[i].flags & CM_DIROPBUFF_INUSE) &&
            op->buffers[i].bufferp == bufferp)
            break;
    }

    if (i < CM_DIROP_MAXBUFFERS) {

        if (flags & DIROP_MODIFIED)
            op->dirtyBufCount++;

        osi_assert(op->buffers[i].refcount > 0);
        op->buffers[i].refcount --;

        if (op->buffers[i].refcount == 0) {
            /* this was the last reference we had */

            osi_Log0(afsd_logp, "cm_DirOpDelBuffer: releasing buffer");

            /* if this buffer was modified, then we update the data
               version of the buffer with the data version of the
               scp. */
            if (!(flags & DIROP_SCPLOCKED)) {
                lock_ObtainMutex(&op->scp->mx);
            }

            /* first make sure that the buffer is idle.  It should
               have been idle all along. */
            osi_assertx((bufferp->cmFlags & (CM_BUF_CMFETCHING |
                                            CM_BUF_CMSTORING)) == 0,
                        "Buffer is not idle while performing dirOp");

            cm_SyncOpDone(op->scp, bufferp,
                          CM_SCACHESYNC_NEEDCALLBACK |
                         (op->lockType == CM_DIRLOCK_WRITE ? CM_SCACHESYNC_WRITE : CM_SCACHESYNC_READ));

#ifdef DEBUG
            osi_assert(bufferp->dataVersion == op->dataVersion);
#endif

            lock_ReleaseMutex(&op->scp->mx);

            lock_ObtainMutex(&bufferp->mx);

            if (flags & DIROP_SCPLOCKED) {
                lock_ObtainMutex(&op->scp->mx);
            }

            if (flags & DIROP_MODIFIED) {
                /* We don't update the dataversion here.  Instead we
                   wait until the dirOp is completed and then flip the
                   dataversion on all the buffers in one go.
                   Otherwise we won't know if the dataversion is
                   current because it was fetched from the server or
                   because we touched it during the dirOp. */

                if (bufferp->userp != op->userp) {
                    if (bufferp->userp != NULL)
                        cm_ReleaseUser(bufferp->userp);
                    cm_HoldUser(op->userp);
                    bufferp->userp = op->userp;
                }
            }

            lock_ReleaseMutex(&bufferp->mx);

            op->buffers[i].bufferp = NULL;
            buf_Release(bufferp);
            op->buffers[i].flags = 0;

            op->nBuffers--;

            return 1;
        } else {
            /* we have other references to this buffer. so we have to
               let it be */
            return 0;
        }

    } else {
        osi_Log0(afsd_logp, "cm_DirOpDelBuffer: buffer not found");
        osi_assertx(FALSE, "Attempt to delete a non-existent buffer from a dirOp");
        return -1;
    }
}

/* Check if we have current status and a callback for the given scp.
   This should be called before cm_DirGetPage() is called per scp.

   On entry:
     scp->mx locked state indicated by parameter

   On exit:
     scp->mx same state as upon entry

   During:
     scp->mx may be released
 */
static long
cm_DirCheckStatus(cm_dirOp_t * op, afs_uint32 locked)
{
    long code;

    if (!locked)
        lock_ObtainMutex(&op->scp->mx);
    code = cm_SyncOp(op->scp, NULL, op->userp, &op->req, PRSFS_LOOKUP,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (!locked)
        lock_ReleaseMutex(&op->scp->mx);

    osi_Log2(afsd_logp, "cm_DirCheckStatus for op 0x%p returning code 0x%x",
             op, code);

    return code;
}

/* Release a directory buffer that was obtained via a call to
   cm_DirGetPage() or any other function that returns a locked, held,
   directory page buffer.

   Called with scp->mx unlocked
 */
static long
cm_DirReleasePage(cm_dirOp_t * op, cm_buf_t ** bufferpp, int modified)
{
    long code = 0;

    if (!*bufferpp)
        return EINVAL;

    cm_DirOpDelBuffer(op, *bufferpp,
                      ((modified ? DIROP_MODIFIED : 0)));
    buf_Release(*bufferpp);
    *bufferpp = NULL;

    return code;
}

/*
   Returns the index'th directory page from scp.  The userp and reqp
   will be used to fetch the buffer from the fileserver if necessary.
   If the call is successful, a locked and held cm_buf_t is returned
   via buferpp and a pointer to the directory page is returned via
   datapp.

   The returned buffer should be released via a call to
   cm_DirReleasePage() or by passing it into a subsequent call to
   cm_DirGetPage() for the *same* scp.

   If a *locked* buffer for the *same* scp is passed in via bufferpp
   to the function, it will check if the requested directory page is
   located in the specified buffer.  If not, the buffer will be
   released and a new buffer returned that contains the requested
   page.

   Note: If a buffer is specified on entry via bufferpp, it is assumed
   that the buffer is unmodified.  If the buffer is modified, it
   should be released via cm_DirReleasePage().

   On entry:
     scp->mx unlocked.
     If *bufferpp is non-NULL, then *bufferpp->mx is locked.

   On exit:
     scp->mx unlocked
     If *bufferpp is non-NULL, then *bufferpp->mx is locked.

   During:
     scp->mx will be obtained and released

 */
static long
cm_DirGetPage(cm_dirOp_t * op,
              long index, cm_buf_t ** bufferpp, void ** datapp)
{
    osi_hyper_t pageOffset;     /* offset of the dir page from the
                                   start of the directory */
    osi_hyper_t bufferOffset;   /* offset of the buffer from the start
                                   of the directory */
    osi_hyper_t thyper;

    cm_buf_t * bufferp = NULL;

    void * datap = NULL;

    long code = 0;

    osi_Log2(afsd_logp, "cm_DirGetPage for op 0x%p, index %d", op, index);

    pageOffset = ConvertLongToLargeInteger(index * CM_DIR_PAGESIZE);
    bufferOffset.HighPart = pageOffset.HighPart;
    bufferOffset.LowPart = pageOffset.LowPart & ~(cm_data.buf_blockSize - 1);

    bufferp = *bufferpp;
    if (bufferp != NULL) {
        osi_assert(cm_FidCmp(&bufferp->fid, &op->scp->fid) == 0);

        thyper = bufferp->offset;
    }

    if (!bufferp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
        /* wrong buffer */

        if (bufferp) {
            buf_Release(bufferp);
            cm_DirOpDelBuffer(op, bufferp, 0);
            bufferp = NULL;
        }

        /* first check if we are already working with the buffer */
        if (cm_DirOpFindBuffer(op, bufferOffset, &bufferp)) {
            code = 0;
            goto _has_buffer;
        }

        code = buf_Get(op->scp, &bufferOffset, &bufferp);
        if (code) {
            osi_Log1(afsd_logp, "    buf_Get returned code 0x%x", code);
            bufferp = NULL;
            goto _exit;
        }

        osi_assert(bufferp != NULL);

        /* DirOpAddBuffer will obtain bufferp->mx if necessary */
        code = cm_DirOpAddBuffer(op, bufferp);

        if (code != 0) {
            /* for some reason, the buffer was rejected.  We can't use
               this buffer, and since this is the only buffer we can
               potentially use, there's no recourse.*/
            buf_Release(bufferp);
            bufferp = NULL;
            goto _exit;
        }

#if 0
        /* The code below is for making sure the buffer contains
           current data.  This is a bad idea, since the whole point of
           doing directory updates locally is to avoid fetching all
           the data from the server. */
        while (1) {
            lock_ObtainMutex(&op->scp->mx);
            code = cm_SyncOp(op->scp, bufferp, op->userp, &op->req, PRSFS_LOOKUP,
                             CM_SCACHESYNC_NEEDCALLBACK |
                             CM_SCACHESYNC_READ |
                             CM_SCACHESYNC_BUFLOCKED);

            if (code) {
                lock_ReleaseMutex(&op->scp->mx);
                break;
            }

            cm_SyncOpDone(op->scp, bufferp,
                          CM_SCACHESYNC_NEEDCALLBACK |
                          CM_SCACHESYNC_READ |
                          CM_SCACHESYNC_BUFLOCKED);

            if (cm_HaveBuffer(op->scp, bufferp, 1)) {
                lock_ReleaseMutex(&op->scp->mx);
                break;
            }

            lock_ReleaseMutex(&bufferp->mx);
            code = cm_GetBuffer(op->scp, bufferp, NULL, op->userp, &op->req);
            lock_ReleaseMutex(&op->scp->mx);
            lock_ObtainMutex(&bufferp->mx);

            if (code)
                break;
        }

        if (code) {
            cm_DirOpDelBuffer(op, bufferp, 0);
            buf_Release(bufferp);
            bufferp = NULL;
            goto _exit;
        }
#endif
    }

 _has_buffer:

    /* now to figure out where the data is */
    thyper = LargeIntegerSubtract(pageOffset, bufferOffset);

    osi_assert(thyper.HighPart == 0);
    osi_assert(cm_data.buf_blockSize > thyper.LowPart &&
               cm_data.buf_blockSize - thyper.LowPart >= CM_DIR_PAGESIZE);

    datap = (void *) (((char *)bufferp->datap) + thyper.LowPart);

    if (datapp)
        *datapp = datap;

    /* also, if we are writing past EOF, we should make a note of the
       new length */
    thyper = LargeIntegerAdd(pageOffset,
                             ConvertLongToLargeInteger(CM_DIR_PAGESIZE));
    if (LargeIntegerLessThan(op->newLength, thyper)) {
        op->newLength = thyper;
    }

 _exit:

    *bufferpp = bufferp;

    osi_Log1(afsd_logp, "cm_DirGetPage returning code 0x%x", code);

    return code;
}


void
cm_DirEntryListAdd(char * namep, cm_dirEntryList_t ** list)
{
    size_t len;
    cm_dirEntryList_t * entry;

    len = strlen(namep);
    len += sizeof(cm_dirEntryList_t);

    entry = malloc(len);
    if (entry) {
        entry->nextp = *list;
        strcpy(entry->name, namep);
        *list = entry;
    }
}

void
cm_DirEntryListFree(cm_dirEntryList_t ** list)
{
    cm_dirEntryList_t * entry;
    cm_dirEntryList_t * next;

    for (entry = *list; entry; entry = next) {
        next = entry->nextp;
        free(entry);
    }

    *list = NULL;
}

