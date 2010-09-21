/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Description:
 *	Implementation of the gator dumb window facility.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>


#include "gtxdumbwin.h"		/*Interface definition */
#include <stdio.h>		/*Standard I/O package */
#include <errno.h>

int dumb_debug;			/*Is debugging turned on? */
static char mn[] = "gator_dumbwindows";	/*Module name */

/*
 * Version of standard operations for a dumb window.
 */
struct gwinops dumb_gwinops = {
    gator_dumbgwin_box,
    gator_dumbgwin_clear,
    gator_dumbgwin_destroy,
    gator_dumbgwin_display,
    gator_dumbgwin_drawline,
    gator_dumbgwin_drawrectangle,
    gator_dumbgwin_drawchar,
    gator_dumbgwin_drawstring,
    gator_dumbgwin_invert,
    gator_dumbgwin_getchar,
    gator_dumbgwin_getdimensions,
    gator_dumbgwin_wait,
};

struct gwinbaseops gator_dumb_gwinbops = {
    gator_dumbgwin_create,
    gator_dumbgwin_cleanup
};

/*
 * Macros to map pixel positions to row & column positions.
 * (Note: for now, they are the identity function!!)
 */
#define	GATOR_MAP_X_TO_COL(w, x)    (x)
#define	GATOR_MAP_Y_TO_LINE(w, y)   (y)

/*------------------------------------------------------------------------
 * gator_dumbgwin_init
 *
 * Description:
 *	Initialize the dumb window package.
 *
 * Arguments:
 *	int adebug: Is debugging turned on?
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_init(int adebug)
{				/*gator_dumbgwin_init */

    static char rn[] = "gator_dumbgwin_init";	/*Routine name */

    /*
     * Remember if we'll be doing debugging, init dumb and clear the
     * standard screen.
     */
    dumb_debug = adebug;

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * We always return success here.
     */
    return (0);

}				/*gator_dumbgwin_init */

/*------------------------------------------------------------------------
 * gator_dumbgwin_create
 *
 * Description:
 *	Create a dumb window.
 *
 * Arguments:
 *	struct gator_dumbgwin_params *params : Ptr to creation parameters.
 *
 * Returns:
 *	Ptr to the created dumb window if successful,
 *	Null ptr otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

struct gwin *
gator_dumbgwin_create(void *rock)
{				/*gator_dumbgwin_create */

    static char rn[] = "gator_dumbgwin_create";	/*Routine name */
    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Return failure here, fill this routine in at some point.
     */
    return (NULL);

}				/*gator_dumbgwin_create */

/*------------------------------------------------------------------------
 * gator_dumbgwin_cleanup
 *
 * Description:
 *	Create a dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to base window.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_cleanup(struct gwin *gwp)
{				/*gator_dumbgwin_cleanup */

    static char rn[] = "gator_dumbgwin_cleanup";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Return success here, fill this routine in at some point.
     */
    return (0);

}				/*gator_dumbgwin_cleanup */

/*------------------------------------------------------------------------
 * gator_dumbgwin_box
 *
 * Description:
 *	Draw a box around the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window to draw
 *	                   a box around.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_box(struct gwin *gwp)
{				/*gator_dumbgwin_box */

    static char rn[] = "gator_dumbgwin_box";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Return success here, fill in the routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_box */

/*------------------------------------------------------------------------
 * gator_dumbgwin_clear
 *
 * Description:
 *	Clear out the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window to clear out.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_clear(struct gwin *gwp)
{				/*gator_dumbgwin_clear */

    static char rn[] = "gator_dumbgwin_clear";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Return success, fill in this routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_clear */

/*------------------------------------------------------------------------
 * gator_dumbgwin_destroy
 *
 * Description:
 *	Destroy the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window to destroy.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_destroy(struct gwin *gwp)
{				/*gator_dumbgwin_destroy */

    static char rn[] = "gator_dumbgwin_destroy";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called", mn, rn);

    return (0);

}				/*gator_dumbgwin_destroy */

/*------------------------------------------------------------------------
 * gator_dumbgwin_display
 *
 * Description:
 *	Display/redraw the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window to draw.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_display(struct gwin *gwp)
{				/*gator_dumbgwin_display */

    static char rn[] = "gator_dumbgwin_display";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Return success, fill in this routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_display */

/*------------------------------------------------------------------------
 * gator_dumbgwin_drawline
 *
 * Description:
 *	Draw a line between two points in the given dumb
 *	window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window in which
 *	                           the line is to be drawn.
 *	struct gwin_lineparams *params : Ptr to other params.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_drawline(struct gwin *gwp, struct gwin_lineparams *params)
{				/*gator_dumbgwin_drawline */

    static char rn[] = "gator_dumbgwin_drawline";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    /*
     * Return success, fill in this routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_drawline */

/*------------------------------------------------------------------------
 * gator_dumbgwin_drawrectangle
 *
 * Description:
 *	Draw a rectangle in the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window in which
 *	                           the rectangle is to be drawn.
 *	struct gwin_rectparams *params : Ptr to other params.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_drawrectangle(struct gwin *gwp, struct gwin_rectparams *params)
{				/*gator_dumbgwin_drawrectangle */

    static char rn[] = "gator_dumbgwin_drawrectangle";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    /*
     * Return success, fill in this routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_drawrectangle */

/*------------------------------------------------------------------------
 * gator_dumbgwin_drawchar
 *
 * Description:
 *	Draw a character in the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window in which
 *	                           the character is to be drawn.
 *	struct gwin_charparams *params : Ptr to other params.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_drawchar(struct gwin *gwp, struct gwin_charparams *params)
{				/*gator_dumbgwin_drawchar */

    static char rn[] = "gator_dumbgwin_drawchar";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Return success, fill in this routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_drawchar */

/*------------------------------------------------------------------------
 * gator_dumbgwin_drawstring
 *
 * Description:
 *	Draw a string in the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window in which
 *	                           the string is to be drawn.
 *	struct gwin_strparams *params : Ptr to other params.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_drawstring(struct gwin *gwp, struct gwin_strparams *params)
{				/*gator_dumbgwin_drawstring */

    static char rn[] = "gator_dumbgwin_drawstring";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Return success, fill in this routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_drawstring */

/*------------------------------------------------------------------------
 * gator_dumbgwin_invert
 *
 * Description:
 *	Invert a region in the given dumb window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window in which
 *	                           the inverted region lies.
 *	struct gwin_invparams *params : Ptr to other params.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_invert(struct gwin *gwp, struct gwin_invparams *params)
{				/*gator_dumbgwin_invert */

    static char rn[] = "gator_dumbgwin_invert";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    /*
     * Return success, fill in this routine at some point.
     */
    return (0);

}				/*gator_dumbgwin_invert */

/*------------------------------------------------------------------------
 * gator_dumbgwin_getchar
 *
 * Description:
 *	Pick up a character from the given window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window to listen to.
 *
 * Returns:
 *	Value of the character read,
 *	-1 otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_getchar(struct gwin *gwp)
{				/*gator_dumbgwin_getchar */

    static char rn[] = "gator_dumbgwin_getchar";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (-1);

}				/*gator_dumbgwin_getchar */

/*------------------------------------------------------------------------
 * gator_dumbgwin_getdimensions
 *
 * Description:
 *	Get the window's X,Y dimensions.
 *
 * Arguments:
 *	struct gwin *gwp	       : Ptr to the dumb window to examine.
 *	struct gwin_sizeparams *aparms : Ptr to size params to set.
 *
 * Returns:
 *	0 if successful,
 *	-1 otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_getdimensions(struct gwin *gwp, struct gwin_sizeparams *aparms)
{				/*gator_dumbgwin_getdimensions */

    static char rn[] = "gator_dumbgwin_getdimensions";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (-1);

}				/*gator_dumbgwin_getdimensions */

/*------------------------------------------------------------------------
 * gator_dumbgwin_wait
 *
 * Description:
 *	Wait until input is available.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the dumb window to wait on.
 *
 * Returns:
 *	0 if successful,
 *	-1 otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_dumbgwin_wait(struct gwin *gwp)
{				/*gator_dumbgwin_wait */

    static char rn[] = "gator_dumbgwin_wait";	/*Routine name */

    if (dumb_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (-1);

}				/*gator_dumbgwin_wait */
