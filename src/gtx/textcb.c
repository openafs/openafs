/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implementation of the gator circular buffer package for its scrollable
 * text object.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "gtxtextcb.h"		/*Module interface */
#include <stdio.h>		/*Standard I/O stuff */
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static int gator_textcb_debug;	/*Is debugging output turned on? */

/*------------------------------------------------------------------------
 * gator_textcb_Init
 *
 * Description:
 *	Initialize the text circular buffer package.
 *
 * Arguments:
 *	a_debug : Should debugging output be turned on?
 *
 * Returns:
 *	Zero if successful,
 *	Error value otherwise.
 *
 * Environment:
 *	MUST BE THE FIRST ROUTINE CALLED FROM THIS PACKAGE.
 *
 * Side Effects:
 *	Just remembers if debugging output should be generated.
 *------------------------------------------------------------------------*/

int
gator_textcb_Init(a_debug)
     int a_debug;

{				/*gator_textcb_Init */

    static int initd;		/*Have we been called already? */
    static char rn[] = "gator_textcb_Init";	/*Routine name */

    if (initd) {
	fprintf(stderr,
		"[%s] Initialization routine called multiple times!!\n", rn);
	return (0);
    } else
	initd = 1;

    gator_textcb_debug = a_debug;
    return (0);

}				/*gator_textcb_Init */

/*------------------------------------------------------------------------
 * gator_textcb_Create
 *
 * Description:
 *	Create a new circular buffer.
 *
 * Arguments:
 *	int a_maxEntriesStored : How many entries should it have?
 *	int a_maxCharsPerEntry : Max chars in each entry.
 *
 * Returns:
 *	Ptr to the fully-initialized circular buffer hdr if successful,
 *	Null pointer otherwise.
 *
 * Environment:
 *	Makes sure the lock structure is properly initialized.
 *
 * Side Effects:
 *	As advertised; space is allocated for the circ buff.
 *------------------------------------------------------------------------*/

struct gator_textcb_hdr *
gator_textcb_Create(a_maxEntriesStored, a_maxCharsPerEntry)
     int a_maxEntriesStored;
     int a_maxCharsPerEntry;

{				/*gator_textcb_Create */

    static char rn[] = "gator_textcb_Create";	/*Routine name */
    char *newBuff;		/*Ptr to new text buffer */
    struct gator_textcb_entry *newEntries;	/*Ptr to new text entries */
    struct gator_textcb_hdr *newHdr;	/*Ptr to new text hdr */
    int bytesToAllocate;	/*Num bytes to allocate */
    int numBuffBytes;		/*Num bytes in text buffer */
    int curr_ent_num;		/*Current entry number */
    struct gator_textcb_entry *curr_ent;	/*Ptr to current entry */
    char *curr_buff;		/*Ptr to current buff pos */
    int curr_inv;		/*Current inversion idx */
    char *blankLine;		/*Ptr to blank line */
    int i;			/*Loop variable */

    /*
     * Start off by allocating the text buffer itself.  Don't forget we
     * need to allocate one more character per line, to make sure we can
     * always null-terminate them.  We also need to allocate the blank
     * line buffer.
     */
    numBuffBytes = bytesToAllocate =
	a_maxEntriesStored * (a_maxCharsPerEntry + 1);
    if (gator_textcb_debug)
	fprintf(stderr, "[%s] Allocating %d bytes for the text buffer\n", rn,
		bytesToAllocate);
    newBuff = (char *)malloc(bytesToAllocate);
    if (newBuff == NULL) {
	fprintf(stderr,
		"[%s] Can't allocate %d bytes for text buffer; errno is %d\n",
		rn, bytesToAllocate, errno);
	return ((struct gator_textcb_hdr *)0);
    } else if (gator_textcb_debug)
	fprintf(stderr, "[%s] Text buffer allocated at 0x%x\n", rn, newBuff);
    blankLine = (char *)malloc(a_maxCharsPerEntry + 1);
    if (blankLine == NULL) {
	fprintf(stderr,
		"[%s] Can't allocate %d bytes for blank line buffer; errno is %d\n",
		rn, a_maxCharsPerEntry + 1, errno);
	free(newBuff);
	return ((struct gator_textcb_hdr *)0);
    }

    /*
     * Next, allocate the entry array.
     */
    bytesToAllocate = a_maxEntriesStored * sizeof(struct gator_textcb_entry);
    if (gator_textcb_debug)
	fprintf(stderr,
		"[%s] Allocating %d bytes for the %d text entry array items\n",
		rn, bytesToAllocate, a_maxEntriesStored);
    newEntries = (struct gator_textcb_entry *)malloc(bytesToAllocate);
    if (newEntries == (struct gator_textcb_entry *)0) {
	fprintf(stderr,
		"[%s] Can't allocate %d bytes for the %d-member text entry array; errno is %d\n",
		rn, bytesToAllocate, a_maxEntriesStored, errno);
	free(newBuff);
	free(blankLine);
	return ((struct gator_textcb_hdr *)0);
    } else if (gator_textcb_debug)
	fprintf(stderr, "[%s] Text buffer entry array allocated at 0x%x\n",
		rn, newEntries);

    /*
     * Finish off by allocating the text circular buffer header.
     */
    bytesToAllocate = sizeof(struct gator_textcb_hdr);
    if (gator_textcb_debug)
	fprintf(stderr,
		"[%s] Allocating %d bytes for the text circular buffer header\n",
		rn, bytesToAllocate);
    newHdr = (struct gator_textcb_hdr *)malloc(bytesToAllocate);
    if (newHdr == (struct gator_textcb_hdr *)0) {
	fprintf(stderr,
		"[%s] Can't allocate %d bytes for text circular buffer header; errno is %d\n",
		rn, bytesToAllocate, errno);
	free(newBuff);
	free(blankLine);
	free(newEntries);
	return ((struct gator_textcb_hdr *)0);
    } else if (gator_textcb_debug)
	fprintf(stderr,
		"[%s] Text circular buffer header allocated at 0x%x\n", rn,
		newHdr);

    /*
     * Now, just initialize all the pieces and plug them in.
     */
    if (gator_textcb_debug)
	fprintf(stderr, "[%s] Zeroing %d bytes in text buffer at 0x%x\n", rn,
		numBuffBytes, newBuff);
    memset(newBuff, 0, numBuffBytes);

    if (gator_textcb_debug)
	fprintf(stderr, "[%s] Initializing blank line buffer at 0x%x\n", rn,
		blankLine);
    for (i = 0; i < a_maxCharsPerEntry; i++)
	*(blankLine + i) = ' ';
    *(blankLine + a_maxCharsPerEntry) = '\0';

    /*
     * Initialize each buffer entry.
     */
    for (curr_ent_num = 0, curr_buff = newBuff, curr_ent = newEntries;
	 curr_ent_num < a_maxEntriesStored;
	 curr_ent++, curr_ent_num++, curr_buff += (a_maxCharsPerEntry + 1)) {
	if (gator_textcb_debug)
	    fprintf(stderr,
		    "[%s] Initializing buffer entry %d; its text buffer address is 0x%x\n",
		    rn, curr_ent_num, curr_buff);
	curr_ent->ID = 0;
	curr_ent->highlight = 0;
	curr_ent->numInversions = 0;
	curr_ent->charsUsed = 0;
	curr_ent->textp = curr_buff;
	memcpy(curr_ent->textp, blankLine, a_maxCharsPerEntry + 1);
	for (curr_inv = 0; curr_inv < GATOR_TEXTCB_MAXINVERSIONS; curr_inv++)
	    curr_ent->inversion[curr_inv] = 0;

    }				/*Init each buffer entry */

    if (gator_textcb_debug)
	fprintf(stderr, "[%s] Filling in circ buff header at 0x%x\n", rn,
		newHdr);
    Lock_Init(&(newHdr->cbLock));
    newHdr->maxEntriesStored = a_maxEntriesStored;
    newHdr->maxCharsPerEntry = a_maxCharsPerEntry;
    newHdr->currEnt = 0;
    newHdr->currEntIdx = 0;
    newHdr->oldestEnt = 0;
    newHdr->oldestEntIdx = 0;
    newHdr->entry = newEntries;
    newHdr->blankLine = blankLine;

    /*
     * Finally, return the location of the new header.
     */
    return (newHdr);

}				/*gator_textcb_Create */

/*------------------------------------------------------------------------
 * bumpCB
 *
 * Description:
 *	Move down to the next circular buffer entry.
 *
 * Arguments:
 *	struct gator_textcb_hdr *a_cbhdr : Circ buff header to bump.
 *
 * Returns:
 *	Ptr to the newest current entry.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static struct gator_textcb_entry *
bumpEntry(a_cbhdr)
     struct gator_textcb_hdr *a_cbhdr;

{				/*bumpEntry */

    static char rn[] = "bumpEntry";	/*Routine name */
    struct gator_textcb_entry *curr_ent;	/*Ptr to current entry */
    int inv;			/*Inversion number */

    /*
     * Bump the total number of writes, and don't forget to advance
     * the oldest entry, if appropriate.
     */
    if (gator_textcb_debug)
	fprintf(stderr,
		"[%s]: Bumping entry for circular buffer at 0x%x; current values: currEnt=%d (idx %d), oldestEnt=%d (idx %d), maxEntriesStored=%d\n",
		rn, a_cbhdr, a_cbhdr->currEnt, a_cbhdr->currEntIdx,
		a_cbhdr->oldestEnt, a_cbhdr->oldestEntIdx,
		a_cbhdr->maxEntriesStored);

    a_cbhdr->currEnt++;
    if (++(a_cbhdr->currEntIdx) >= a_cbhdr->maxEntriesStored)
	a_cbhdr->currEntIdx = 0;
    curr_ent = a_cbhdr->entry + a_cbhdr->currEntIdx;

    if (gator_textcb_debug)
	fprintf(stderr, "[%s] Zeroing entry %d (idx %d) at 0x%x\n", rn,
		a_cbhdr->currEnt, a_cbhdr->currEntIdx, curr_ent);

    curr_ent->ID = a_cbhdr->currEnt;
    curr_ent->highlight = 0;
    curr_ent->numInversions = 0;
    curr_ent->charsUsed = 0;
    /*
     * Copy over a blank line into the one we're initializing.  We
     * copy over the trailing null, too.
     */
    memcpy(curr_ent->textp, a_cbhdr->blankLine,
	   a_cbhdr->maxCharsPerEntry + 1);
    for (inv = 0; inv < GATOR_TEXTCB_MAXINVERSIONS; inv++)
	curr_ent->inversion[inv] = 0;

    /*
     * If we've already stated circulating in the buffer, remember to
     * bump the oldest entry info too.
     */
    if (a_cbhdr->currEnt >= a_cbhdr->maxEntriesStored) {
	if (gator_textcb_debug)
	    fprintf(stderr,
		    "[%s]: Advancing oldest entry number & index; was entry %d, index %d, now ",
		    rn, a_cbhdr->oldestEnt, a_cbhdr->oldestEntIdx);
	a_cbhdr->oldestEnt++;
	if (++(a_cbhdr->oldestEntIdx) >= a_cbhdr->maxEntriesStored)
	    a_cbhdr->oldestEntIdx = 0;
	if (gator_textcb_debug)
	    fprintf(stderr, "entry %d, index %d\n", a_cbhdr->oldestEnt,
		    a_cbhdr->oldestEntIdx);
    }

    /*Bump oldest entry info */
    /*
     * Finally, return the address of the newest current entry.
     */
    return (curr_ent);

}				/*bumpEntry */

/*------------------------------------------------------------------------
 * gator_textcb_Write
 *
 * Description:
 *	Write the given string to the text circular buffer.  Line
 *	breaks are caused either by overflowing the current text
 *	line or via explicit '\n's.
 *
 * Arguments:
 *	struct gator_textcb_hdr *a_cbhdr : Ptr to circ buff hdr.
 *	char *a_textToWrite		 : Ptr to text to insert.
 *	int a_numChars			 : Number of chars to write.
 *	int a_highlight			 : Use highlighting?
 *	int a_skip;			 : Force a skip to the next line?
 *
 * Returns:
 *	Zero if successful,
 *	Error value otherwise.
 *
 * Environment:
 *	Circular buffer is consistent upon entry, namely the first and
 *	last entry pointers are legal.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_textcb_Write(a_cbhdr, a_textToWrite, a_numChars, a_highlight, a_skip)
     struct gator_textcb_hdr *a_cbhdr;
     char *a_textToWrite;
     int a_numChars;
     int a_highlight;
     int a_skip;

{				/*gator_textcb_Write */

    static char rn[] = "gator_textcb_Write";	/*Routine name */
    struct gator_textcb_entry *curr_ent;	/*Ptr to current text entry */
    int curr_ent_idx;		/*Index of current entry */
    int max_chars;		/*Max chars per entry */
    int chars_to_copy;		/*Num chars to copy in */
    int effective_highlight;	/*Tmp highlight value */
    char *dest;			/*Destination of char copy */

    /*
     * Make sure we haven't been passed a bogus buffer, and lock it
     * before we start.
     */
    if (a_cbhdr == (struct gator_textcb_hdr *)0) {
	fprintf(stderr,
		"[%s]: Null pointer passed in for circ buff header!!  Aborting write operation.\n",
		rn);
	return (-1);
    }
    ObtainWriteLock(&(a_cbhdr->cbLock));

    curr_ent_idx = a_cbhdr->currEntIdx;
    curr_ent = (a_cbhdr->entry) + curr_ent_idx;
    max_chars = a_cbhdr->maxCharsPerEntry;
    effective_highlight = curr_ent->highlight;
    if (curr_ent->numInversions % 2)
	effective_highlight = (effective_highlight ? 0 : 1);
    if (gator_textcb_debug)
	fprintf(stderr,
		"[%s]: Current entry: %d (at index %d, keeping %d max), effective highlight: %d, located at 0x%x\n",
		rn, a_cbhdr->currEnt, curr_ent_idx, a_cbhdr->maxEntriesStored,
		effective_highlight, curr_ent);

    while (a_numChars > 0) {
	/*
	 * There are still characters to stuff into our circular buffer.
	 */
	if (gator_textcb_debug)
	    fprintf(stderr,
		    "[%s]: Top of write loop: %d char(s) left to write.\n",
		    rn, a_numChars);

	if (curr_ent->charsUsed >= max_chars) {
	    /*
	     * Bump the entry in the given circular buffer.
	     */
	    if (gator_textcb_debug)
		fprintf(stderr,
			"[%s]: Entry %d at index %d full, advancing to next one.\n",
			rn, a_cbhdr->currEnt, curr_ent_idx);
	    curr_ent = bumpEntry(a_cbhdr);
	    curr_ent_idx = a_cbhdr->currEntIdx;
	    if (gator_textcb_debug)
		fprintf(stderr,
			"[%s] New CB entry info: currEnt=%d (idx %d), oldestEnt=%d (idx %d), curr entry ptr is 0x%x\n",
			rn, a_cbhdr->currEnt, a_cbhdr->currEntIdx,
			a_cbhdr->oldestEnt, a_cbhdr->oldestEntIdx, curr_ent);
	}

	/*Bump current entry */
	/*
	 * At this point, the current entry has room for at least one more
	 * character, and we have at least one more character to write.
	 * Insert as much from the user text as possible.
	 */
	chars_to_copy = max_chars - curr_ent->charsUsed;
	if (a_numChars < chars_to_copy)
	    chars_to_copy = a_numChars;
	dest = curr_ent->textp + curr_ent->charsUsed;
	if (gator_textcb_debug)
	    fprintf(stderr,
		    "[%s]: Copying %d char(s) into current entry at 0x%x (entry buffer starts at 0x%x)\n",
		    rn, chars_to_copy, dest, curr_ent->textp);

	/*
	 * Handle highlighting and inversions.
	 */
	if (curr_ent->charsUsed == 0) {
	    /*
	     * No chars yet, so this sets the highlight field.
	     */
	    effective_highlight = curr_ent->highlight = a_highlight;
	} else if (effective_highlight != a_highlight) {
	    /*
	     * We need a new inversion, if there's room.
	     */
	    if (gator_textcb_debug)
		fprintf(stderr,
			"[%s]: Highlight mismatch, recording inversion at char loc %d\n",
			rn, curr_ent->charsUsed);
	    if (curr_ent->numInversions < GATOR_TEXTCB_MAXINVERSIONS) {
		effective_highlight = a_highlight;
		curr_ent->inversion[curr_ent->numInversions] =
		    curr_ent->charsUsed;
		curr_ent->numInversions++;
	    } else if (gator_textcb_debug)
		fprintf(stderr, "[%s]: Out of inversions!!\n", rn);
	}

	/*Handle inversion */
	/*
	 * Move the string chunk into its place in the buffer, bump the
	 * number of chars used in the current entry.
	 */
	strncpy(dest, a_textToWrite, chars_to_copy);
	curr_ent->charsUsed += chars_to_copy;
	a_textToWrite += chars_to_copy;
	a_numChars -= chars_to_copy;

    }				/*while (a_numChars > 0) */

    /*
     * All characters have been copied in.  Handle the case where we've
     * been asked to skip to the next entry, even if there's still room
     * in the current one.
     */
    if (a_skip) {
	if (gator_textcb_debug)
	    fprintf(stderr, "[%s] Handling request to skip to next entry\n",
		    rn);
	if (curr_ent->charsUsed > 0)
	    curr_ent = bumpEntry(a_cbhdr);
	else if (gator_textcb_debug)
	    fprintf(stderr,
		    "[%s] Not skipping, we're already on a fresh entry\n",
		    rn);
    }

    /*Skip to the next entry */
    /*
     * We can now unlock the CB and return successfully.
     */
    ReleaseWriteLock(&(a_cbhdr->cbLock));
    return (0);

}				/*gator_textcb_Write */

/*------------------------------------------------------------------------
 * gator_textcb_BlankLine
 *
 * Description:
 *	Write out some number of blank lines to the given circular
 *	buffer.
 *
 * Arguments:
 *	struct gator_textcb_hdr *a_cbhdr : Ptr to circ buff hdr.
 *	int *a_numBlanks		 : Num. blank lines to write.
 *
 * Returns:
 *	Zero if successful,
 *	Error value otherwise.
 *
 * Environment:
 *	Circular buffer is consistent upon entry, namely the first and
 *	last entry pointers are legal.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_textcb_BlankLine(a_cbhdr, a_numBlanks)
     struct gator_textcb_hdr *a_cbhdr;
     int a_numBlanks;

{				/*gator_textcb_BlankLine */

    static char rn[] = "gator_textcb_BlankLine";	/*Routine name */

    if (gator_textcb_debug)
	fprintf(stderr, "[%s] Putting out %d blank lines to the CB at 0x%x\n",
		rn, a_numBlanks, a_cbhdr);

    if (a_cbhdr == (struct gator_textcb_hdr *)0) {
	if (gator_textcb_debug)
	    fprintf(stderr, "[%s] Null pointer passed for CB hdr!!\n", rn);
	return (-1);
    }

    while (a_numBlanks > 0) {
	/*
	 * The bumpEntry routine returns a struct gator_textcb_entry
	 * pointer, but we don't need it here, so we don't assign it.
	 */
	bumpEntry(a_cbhdr);
	a_numBlanks--;
    }

    /*
     * Return happily and successfully.
     */
    return (0);

}				/*gator_textcb_Write */

/*------------------------------------------------------------------------
 * gator_textcb_Delete
 *
 * Description:
 *	Delete the storage used by the given circular buffer, including
 *	the header itself.
 *
 * Arguments:
 *	struct gator_textcb_hdr *a_cbhdr : Ptr to the header of the
 *						circ buffer to reap.
 *
 * Returns:
 *	Zero if successful,
 *	Error value otherwise.
 *
 * Environment:
 *	We write-lock the buffer before deleting it, which is slightly
 *	silly, since it will stop existing after we're done.  At least
 *	we'll make sure nobody is actively writing to it while it's
 *	being deleted.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_textcb_Delete(a_cbhdr)
     struct gator_textcb_hdr *a_cbhdr;

{				/*gator_textcb_Delete */

    static char rn[] = "gator_textcb_Delete";	/*Routine name */

    if (gator_textcb_debug)
	fprintf(stderr, "[%s]: Deleting text circular buffer at 0x%x\n", rn,
		a_cbhdr);
    ObtainWriteLock(&(a_cbhdr->cbLock));

    /*
     * The beginning of the text buffer itself is pointed to by the
     * first text entry.
     */
    if (gator_textcb_debug)
	fprintf(stderr,
		"[%s]: Freeing text buffer proper at 0x%x (%d bytes)\n", rn,
		a_cbhdr->entry[0].textp,
		(a_cbhdr->maxEntriesStored * a_cbhdr->maxCharsPerEntry));
    free(a_cbhdr->entry[0].textp);
    a_cbhdr->entry[0].textp = NULL;

    if (gator_textcb_debug)
	fprintf(stderr, "[%s]: Freeing text entry array at 0x%x (%d bytes)\n",
		rn, a_cbhdr->entry,
		(a_cbhdr->maxEntriesStored *
		 sizeof(struct gator_textcb_entry)));
    free(a_cbhdr->entry);
    a_cbhdr->entry = (struct gator_textcb_entry *)0;
    free(a_cbhdr->blankLine);
    a_cbhdr->blankLine = NULL;

    /*
     * Release the write lock on it, then free the header itself.
     */
    ReleaseWriteLock(&(a_cbhdr->cbLock));
    if (gator_textcb_debug)
	fprintf(stderr, "[%s] Freeing cicular buffer header at 0x%x\n", rn,
		a_cbhdr);
    free(a_cbhdr);
    return (0);

}				/*gator_textcb_Delete */
