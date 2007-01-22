/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

/*
 * Description:
 *	Implementation of the gator text object.
 *------------------------------------------------------------------------*/

#include <osi/osi.h>

/*
 * XXX
 * the following code is commented out because it breaks the libosi
 * header files, which are required since we are in the process of
 * integrating lwp shared locks into libosi.  I'm not sure which
 * platforms require src/config/stds.h to not be parsed; if we run
 * into a platform on which this breaks, then we'll need to revisit.
 */
#if 0
#define        IGNORE_STDS_H
#include <afsconfig.h>
#include <afs/param.h>
#endif



RCSID
    ("$Header$");

#include "gtxtextobj.h"		/*Interface for this module */
#include "gtxwindows.h"		/*Gator window interface */
#include "gtxcurseswin.h"	/*Gator curses window interface */
#include "gtxdumbwin.h"		/*Gator dumb terminal window interface */
#include "gtxX11win.h"		/*Gator X11 window interface */
#include <stdio.h>		/*Standard I/O stuff */
#include <errno.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <stdlib.h>

/*Externally-advertised array of text onode operations*/
struct onodeops gator_text_ops = {
    gator_text_destroy,
    gator_text_display,
    gator_text_release
};

static char mn[] = "gator_textobject";	/*Module name */

#define GATOR_TEXTCB_DO_BOX	0

/*------------------------------------------------------------------------
 * gator_text_create
 *
 * Description:
 *	Create a gator text object.
 *
 * Arguments:
 *      struct onode *text_onp : Ptr to the text onode to fill out.
 *	struct onode_createparams *params : Generic ptr to creation
 *	    parameters.
 *
 * Returns:
 *	Zero if successful,
 *	Error value otherwise.
 *
 * Environment:
 *	The base onode fields have already been set.  Text onodes are
 *	empty upon creation.
 *
 * Side Effects:
 *	Upon successful creation, the onode's o_window field is
 *	replaced with a new window created for the text object,
 *	with the parent window is remembered within the new window
 *	structure.  On failure, it remains unchanged.
 *------------------------------------------------------------------------*/

int
gator_text_create(text_onp, params)
     struct onode *text_onp;
     struct onode_createparams *params;

{				/*gator_text_create */

    static char rn[] = "gator_text_create";	/*Routine name */
    struct gator_textobj_params *text_params;	/*My specific creation params */
    struct gator_textobj *text_data;	/*Ptr to private data */
    struct gator_textcb_hdr *newCB;	/*Ptr to CB hdr */

    text_params = (struct gator_textobj_params *)params;
    if (objects_debug) {
	fprintf(stderr,
		"[%s:%s] Private data passed to text object at 0x%x:\n", mn,
		rn, text_onp);
	fprintf(stderr, "\tmaxEntries: %d, maxCharsPerEntry: %d\n",
		text_params->maxEntries, text_params->maxCharsPerEntry);
    }

    /*
     * Allocate the private data area.
     */
    if (objects_debug)
	fprintf(stderr,
		"[%s:%s] Allocating %d bytes for text object private data region\n",
		mn, rn, sizeof(struct gator_textobj));
    text_data = (struct gator_textobj *)malloc(sizeof(struct gator_textobj));
    if (text_data == (struct gator_textobj *)0) {
	fprintf(stderr,
		"[%s:%s] Can't allocate %d bytes for text object private data region, errno is %d\n",
		mn, rn, sizeof(struct gator_textobj), errno);
	return (errno);
    }

    /*
     * Create the text circular buffer for this new object.
     */
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Creating circular buffer, %dx%d chars\n", mn,
		rn, text_params->maxEntries, text_params->maxCharsPerEntry);
    newCB =
	gator_textcb_Create(text_params->maxEntries,
			    text_params->maxCharsPerEntry);
    if (newCB == (struct gator_textcb_hdr *)0) {
	fprintf(stderr, "[%s:%s] Can't create text object circular buffer\n",
		mn, rn);
	free(text_data);
	return (-1);
    }

    /*
     * Now that we have the private structures allocated, set them up.
     */
    text_data->llrock = (int *)0;
    text_data->numLines = text_onp->o_height;
    text_data->cbHdr = newCB;
    text_data->firstEntShown = 0;
    text_data->lastEntShown = 0;
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Number of lines in window: %d: ", mn, rn,
		text_data->numLines);

    /*
     * Attach the text-specific private
     * data to the generic onode and return the happy news.
     */
    text_onp->o_data = (int *)text_data;
    return (0);

}				/*gator_text_create */

/*------------------------------------------------------------------------
 * gator_text_destroy
 *
 * Description:
 *	Destroy a gator text object.
 *
 * Arguments:
 *	struct onode *onp : Ptr to the text onode to delete.
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_text_destroy(onp)
     struct onode *onp;

{				/*gator_text_destroy */

    /*
     * For now, this is a no-op.
     */
    return (0);

}				/*gator_text_destroy */

/*------------------------------------------------------------------------
 * gator_text_display
 *
 * Description:
 *	Display/redraw a gator text object.
 *
 * Arguments:
 *	struct onode *onp: Ptr to the text onode to display.
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_text_display(onp)
     struct onode *onp;

{				/*gator_text_display */

    static char rn[] = "gator_text_display";	/*Routine name */
    struct gator_textobj *text_data;	/*Ptr to text obj data */
    struct gator_textcb_hdr *cbHdr;	/*Ptr to CB header */
    struct gwin_strparams strparams;	/*String-drawing params */
    int currLine;		/*Current line being updated */
    int currLinesUsed;		/*Num screen lines used */
    int currIdx;		/*Current line index */
    int currEnt;		/*Current entry being drawn */
    struct gator_textcb_entry *curr_ent;	/*Ptr to current entry */

    if (objects_debug)
	fprintf(stderr, "[%s:%s] Displaying text object at 0x%x\n", mn, rn,
		onp);
    text_data = (struct gator_textobj *)(onp->o_data);
    cbHdr = text_data->cbHdr;
    if (objects_debug)
	fprintf(stderr,
		"[%s:%s] Displaying text object at 0x%x, object-specific data at 0x%x\n",
		mn, rn, onp, text_data);

    /*
     * Update each line in the screen buffer with its proper contents.
     */
    currEnt = text_data->firstEntShown;
    currLinesUsed = text_data->lastEntShown - currEnt + 1;
    currIdx =
	(cbHdr->oldestEntIdx +
	 (currEnt - cbHdr->oldestEnt)) % cbHdr->maxEntriesStored;
    curr_ent = cbHdr->entry + currIdx;

    if (objects_debug)
	fprintf(stderr,
		"[%s:%s] Drawing %d populated lines, starting with entry %d (index %d) at 0x%x\n",
		mn, rn, currLinesUsed, currEnt, currIdx, curr_ent);

    strparams.x = onp->o_x;
    strparams.y = onp->o_y;
    for (currLine = 0; currLine < text_data->numLines; currLine++) {
	/*
	 * Draw the current entry.
	 */
	if (currLinesUsed > 0) {
	    /*
	     * Drawing a populated line.  We need to iterate if there are
	     * inversions (I don't feel like doing this now).
	     */
	    strparams.s = curr_ent->textp;
	    strparams.highlight = curr_ent->highlight;
	    WOP_DRAWSTRING(onp->o_window, &strparams);

	    currLinesUsed--;
	    currEnt++;
	    currIdx++;
	    if (currIdx >= cbHdr->maxEntriesStored) {
		currIdx = 0;
		curr_ent = cbHdr->entry;
	    } else
		curr_ent++;
	} else {
	    /*
	     * Draw a blank line.
	     */
	    strparams.s = cbHdr->blankLine;
	    strparams.highlight = 0;
	    WOP_DRAWSTRING(onp->o_window, &strparams);
	}

	/*
	 * Adjust the X and Y locations.
	 */
	strparams.x = 0;
	strparams.y++;

    }				/*Update loop */

    /*
     * Box the window before we leave.
     */
#if GATOR_TEXTCB_DO_BOX
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Boxing window structure at 0x%x\n", mn, rn,
		onp->o_window);
    WOP_BOX(onp->o_window);
#endif /* GATOR_TEXTCB_DO_BOX */

    /*
     * For now, this is all we do.
     */
    return (0);

}				/*gator_text_display */

/*------------------------------------------------------------------------
 * gator_text_release
 *
 * Description:
 *	Drop the refcount on a gator text object.
 *
 * Arguments:
 *	struct onode *onp : Ptr to the onode whose refcount is
 *	                             to be dropped.
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_text_release(onp)
     struct onode *onp;

{				/*gator_text_release */

    /*
     * For now, this is a no-op.
     */
    return (0);

}				/*gator_text_release */

/*------------------------------------------------------------------------
 * gator_text_Scroll
 *
 * Description:
 *	Scroll a text object some number of lines.
 *
 * Arguments:
 *	struct onode *onp : Ptr to the text onode to be scrolled.
 *	int nlines	  : Number of lines to scroll.
 *	int direction	  : Scroll up or down?
 *
 * Returns:
 *	0: Success,
 *	Error value otherwise.
 *
 * Environment:
 *	Invariant: the text object's firstEntShown and lastEntShown
 *		are always between oldestEnt and currEnt (inclusive).
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_text_Scroll(onp, nlines, direction)
     struct onode *onp;
     int nlines;
     int direction;

{				/*gator_text_Scroll */

    static char rn[] = "gator_text_Scroll";	/*Routine name */
    struct gator_textobj *text_data;	/*Ptr to text obj data */

    /*
     * We move the markers for first & last entries displayed, depending
     * on what's available to us in the circular buffer.  We never leave
     * the window empty.
     */
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Scrolling text object at 0x%x %d lines %s\n",
		mn, rn, nlines,
		(direction == GATOR_TEXT_SCROLL_UP) ? "UP" : "DOWN");

    text_data = (struct gator_textobj *)(onp->o_data);
    if (direction == GATOR_TEXT_SCROLL_DOWN) {
	/*
	 * Move the object's text ``down'' by sliding the window up.
	 */
	text_data->firstEntShown -= nlines;
	if (text_data->firstEntShown < text_data->cbHdr->oldestEnt)
	    text_data->firstEntShown = text_data->cbHdr->oldestEnt;

	text_data->lastEntShown -= nlines;
	if (text_data->lastEntShown < text_data->cbHdr->oldestEnt)
	    text_data->lastEntShown = text_data->cbHdr->oldestEnt;

    } /*Moving down */
    else {
	/*
	 * Move the object's text ``up'' by sliding the window down.
	 */
	text_data->firstEntShown += nlines;
	if (text_data->firstEntShown > text_data->cbHdr->currEnt)
	    text_data->firstEntShown = text_data->cbHdr->currEnt;

	text_data->lastEntShown += nlines;
	if (text_data->lastEntShown > text_data->cbHdr->currEnt)
	    text_data->lastEntShown = text_data->cbHdr->currEnt;

    }				/*Moving up */

    /*
     * Return the happy news.
     */
    return (0);

}				/*gator_text_Scroll */

/*------------------------------------------------------------------------
 * gator_text_Write
 *
 * Description:
 *	Write the given string to the end of the gator text object.
 *
 * Arguments:
 *	struct onode *onp : Ptr to the onode whose to which we're
 *				writing.
 *	char *strToWrite  : String to write.
 *	int numChars	  : Number of chars to write.
 *	int highlight	  : Use highlighting?
 *	int skip	  : Force a skip to the next line?
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_text_Write(onp, strToWrite, numChars, highlight, skip)
     struct onode *onp;
     char *strToWrite;
     int numChars;
     int highlight;
     int skip;

{				/*gator_text_Write */

    static char rn[] = "gator_text_Write";	/*Routine name */
    register int code;		/*Return value on routines */
    struct gator_textobj *text_data;	/*Ptr to text obj data */
    struct gator_textcb_hdr *cbHdr;	/*Ptr to text CB header */
    int i;			/*Loop variable */
    int oldCurrEnt;		/*CB's old currEnt value */
    int redisplay;		/*Redisplay after write? */
    int shownDiff;		/*Diff between 1st & last lines */
    int writeDiff;		/*Num lines really written */
    int bumpAmount;		/*Amount to bump count */

    /*
     * 
     */
    if (objects_debug) {
	fprintf(stderr,
		"[%s:%s] Writing %d chars to text object at 0x%x (highlight=%d, skip=%d: '",
		rn, numChars, onp, highlight, skip);
	for (i = 0; i < numChars; i++)
	    fprintf(stderr, "%c", strToWrite + i);
	fprintf(stderr, "\n");
    }

    if (numChars == 0)
	numChars = strlen(strToWrite);	/* simplify caller */
    text_data = (struct gator_textobj *)(onp->o_data);
    cbHdr = text_data->cbHdr;
    if (cbHdr == (struct gator_textcb_hdr *)0) {
	fprintf(stderr, "[%s:%s] Text object missing its circular buffer!\n",
		mn, rn);
	return (-1);
    }
    /*
     * If the current CB entry is being displayed, we track the write
     * visually and redisplay.
     */
    if ((cbHdr->currEnt <= text_data->lastEntShown)
	&& (cbHdr->currEnt >= text_data->firstEntShown)) {
	if (objects_debug)
	    fprintf(stderr,
		    "[%s:%s] Current entry is on screen.  Tracking this write\n",
		    mn, rn);
	oldCurrEnt = cbHdr->currEnt;
	redisplay = 1;
    } else {
	if (objects_debug)
	    fprintf(stderr,
		    "[%s:%s] Current entry NOT on screen, not tracking write\n",
		    mn, rn);
	redisplay = 0;
    }


    if (redisplay) {
	/*
	 * We're tracking the write.  Compute the number of screen lines
	 * actually written and adjust our own numbers, then call the
	 * display function.
	 */
	shownDiff = text_data->lastEntShown - text_data->firstEntShown;
	writeDiff = cbHdr->currEnt - oldCurrEnt;
	if (objects_debug)
	    fprintf(stderr,
		    "[%s:%s] Preparing to redisplay.  Difference in shown lines=%d, difference in written lines=%d\n",
		    mn, rn, shownDiff, writeDiff);
	if (shownDiff < (text_data->numLines - 1)) {
	    /*
	     * We weren't showing a full screen of stuff.  Bump the last
	     * line shown by the minimum of the number of free lines still
	     * on the screen and the number of new lines actually written.
	     */
	    bumpAmount = (text_data->numLines - 1) - shownDiff;
	    if (writeDiff < bumpAmount)
		bumpAmount = writeDiff;
	    text_data->lastEntShown += bumpAmount;
	    writeDiff -= bumpAmount;
	    if (objects_debug)
		fprintf(stderr,
			"[%s:%s] Empty lines appeared on screen, bumping bottom line shown by %d; new writeDiff is %d\n",
			mn, rn, bumpAmount, writeDiff);
	}

	/*
	 * If we have any more lines that were written not taken care
	 * of by the above, we just bump the counters.
	 */
	if (writeDiff > 0) {
	    if (objects_debug)
		fprintf(stderr,
			"[%s:%s] Still more lines need to be tracked.  Moving first & last shown down by %d\n",
			mn, rn, writeDiff);
	    text_data->firstEntShown += writeDiff;
	    text_data->lastEntShown += writeDiff;
	}
    }

    /*Redisplay needed */
    /*
     * Simply call the circular buffer write op.
     */
    code = gator_textcb_Write(cbHdr, strToWrite, numChars, highlight, skip);
    if (code) {
	fprintf(stderr,
		"[%s:%s] Can't write to text object's circular buffer, errror code is %d\n",
		mn, rn, code);
	return (code);
    }

    /*
     * Everything went well.  Return the happy news.
     */
    return (0);

}				/*gator_text_Write */

/*------------------------------------------------------------------------
 * gator_text_BlankLine
 *
 * Description:
 *	Write a given number of blank lines to the given text object.
 *
 * Arguments:
 *	struct onode *onp : Ptr to the onode to which we're writing.
 *	int numBlanks	  : Number of blank lines to write.
 *
 * Returns:
 *	0: Success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_text_BlankLine(onp, numBlanks)
     struct onode *onp;
     int numBlanks;

{				/*gator_text_BlankLine */

    static char rn[] = "gator_text_BlankLine";	/*Routine name */
    register int code;		/*Return value on routines */
    struct gator_textobj *text_data;	/*Ptr to text obj data */

    /*
     * We just call the circular buffer routine directly.
     */
    if (objects_debug)
	fprintf(stderr,
		"[%s:%s] Writing %d blank lines to text object at 0x%x\n", mn,
		rn, numBlanks, onp);

    text_data = (struct gator_textobj *)(onp->o_data);
    code = gator_textcb_BlankLine(text_data->cbHdr, numBlanks);
    if (code) {
	fprintf(stderr,
		"[%s:%s] Can't write %d blank lines to text object at 0x%x\n",
		mn, rn, numBlanks, onp);
	return (code);
    }

    /*
     * Blank lines written successfully.  Adjust what lines are currently
     * shown.  Iff we were tracking the end of the buffer, we have to
     * follow the blank lines.
     */
    if (text_data->lastEntShown == text_data->cbHdr->currEnt - numBlanks) {
	text_data->firstEntShown += numBlanks;
	text_data->lastEntShown += numBlanks;
    }

    /*
     * Return the happy news.
     */
    return (0);

}				/*gator_text_BlankLine */
