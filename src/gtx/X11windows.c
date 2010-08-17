/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * gator_X11windows.c
 *
 * Description:
 *	Implementation of the gator X11 window facility.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>


#include "gtxX11win.h"		/*Interface definition */
#include <stdio.h>		/*Standard I/O package */

#if	!defined(NeXT)
extern int errno;		/*System error number */
#endif /* NeXT */
int X11_debug;			/*Is debugging turned on? */
static char mn[] = "gator_X11windows";	/*Module name */

/*
 * Version of standard operations for a X11 window.
 */
struct gwinops X11_gwinops = {
    gator_X11gwin_box,
    gator_X11gwin_clear,
    gator_X11gwin_destroy,
    gator_X11gwin_display,
    gator_X11gwin_drawline,
    gator_X11gwin_drawrectangle,
    gator_X11gwin_drawchar,
    gator_X11gwin_drawstring,
    gator_X11gwin_invert,
    gator_X11gwin_getchar,
    gator_X11gwin_getdimensions,
    gator_X11gwin_wait,
};

struct gwinbaseops gator_X11_gwinbops = {
    gator_X11gwin_create,
    gator_X11gwin_cleanup
};

/*
 * Macros to map pixel positions to row & column positions.
 * (Note: for now, they are the identity function!!)
 */
#define	GATOR_MAP_X_TO_COL(w, x)    (x)
#define	GATOR_MAP_Y_TO_LINE(w, y)   (y)

/*------------------------------------------------------------------------
 * gator_X11gwin_init
 *
 * Description:
 *	Initialize the X11 window package.
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
gator_X11gwin_init(int adebug)
{				/*gator_X11gwin_init */

    static char rn[] = "gator_X11gwin_init";	/*Routine name */

    /*
     * Remember if we'll be doing debugging, init X11 and clear the
     * standard screen.
     */
    X11_debug = adebug;

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * We return success, fill this routine it at some point.
     */
    return (0);

}				/*gator_X11gwin_init */

/*------------------------------------------------------------------------
 * gator_X11gwin_create
 *
 * Description:
 *	Create a X11 window.
 *
 * Arguments:
 *	struct gator_X11gwin_params *params : Ptr to creation parameters.
 *
 * Returns:
 *	Ptr to the created X11 window if successful,
 *	Null ptr otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

struct gwin *
gator_X11gwin_create(void *rock)
{				/*gator_X11gwin_create */

    static char rn[] = "gator_X11gwin_create";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (NULL);

}				/*gator_X11gwin_create */

/*------------------------------------------------------------------------
 * gator_X11gwin_cleanup
 *
 * Description:
 *	Create a X11 window.
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
gator_X11gwin_cleanup(struct gwin *gwp)
{				/*gator_X11gwin_cleanup */

    static char rn[] = "gator_X11gwin_cleanup";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (0);

}				/*gator_X11gwin_cleanup */

/*------------------------------------------------------------------------
 * gator_X11gwin_box
 *
 * Description:
 *	Draw a box around the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window to draw
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
gator_X11gwin_box(struct gwin *gwp)
{				/*gator_X11gwin_box */

    static char rn[] = "gator_X11gwin_box";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (0);

}				/*gator_X11gwin_box */

/*------------------------------------------------------------------------
 * gator_X11gwin_clear
 *
 * Description:
 *	Clear out the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window to clear out.
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
gator_X11gwin_clear(struct gwin *gwp)
{				/*gator_X11gwin_clear */

    static char rn[] = "gator_X11gwin_clear";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (0);

}				/*gator_X11gwin_clear */

/*------------------------------------------------------------------------
 * gator_X11gwin_destroy
 *
 * Description:
 *	Destroy the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window to destroy.
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
gator_X11gwin_destroy(struct gwin *gwp)
{				/*gator_X11gwin_destroy */

    static char rn[] = "gator_X11gwin_destroy";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (0);

}				/*gator_X11gwin_destroy */

/*------------------------------------------------------------------------
 * gator_X11gwin_display
 *
 * Description:
 *	Display/redraw the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window to draw.
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
gator_X11gwin_display(struct gwin *gwp)
{				/*gator_X11gwin_display */

    static char rn[] = "gator_X11gwin_display";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (0);

}				/*gator_X11gwin_display */

/*------------------------------------------------------------------------
 * gator_X11gwin_drawline
 *
 * Description:
 *	Draw a line between two points in the given X11
 *	window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window in which
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
gator_X11gwin_drawline(struct gwin *gwp, struct gwin_lineparams *params)
{				/*gator_X11gwin_drawline */

    static char rn[] = "gator_X11gwin_drawline";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (0);

}				/*gator_X11gwin_drawline */

/*------------------------------------------------------------------------
 * gator_X11gwin_drawrectangle
 *
 * Description:
 *	Draw a rectangle in the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window in which
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
gator_X11gwin_drawrectangle(struct gwin *gwp, struct gwin_rectparams *params)
{				/*gator_X11gwin_drawrectangle */

    static char rn[] = "gator_X11gwin_drawrectangle";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (0);

}				/*gator_X11gwin_drawrectangle */

/*------------------------------------------------------------------------
 * gator_X11gwin_drawchar
 *
 * Description:
 *	Draw a character in the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window in which
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
gator_X11gwin_drawchar(struct gwin *gwp, struct gwin_charparams *params)
{				/*gator_X11gwin_drawchar */

    static char rn[] = "gator_X11gwin_drawchar";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (0);

}				/*gator_X11gwin_drawchar */

/*------------------------------------------------------------------------
 * gator_X11gwin_drawstring
 *
 * Description:
 *	Draw a string in the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window in which
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
gator_X11gwin_drawstring(struct gwin *gwp, struct gwin_strparams *params)
{				/*gator_X11gwin_drawstring */

    static char rn[] = "gator_X11gwin_drawstring";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    return (0);

}				/*gator_X11gwin_drawstring */

/*------------------------------------------------------------------------
 * gator_X11gwin_invert
 *
 * Description:
 *	Invert a region in the given X11 window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window in which
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
gator_X11gwin_invert(struct gwin *gwp, struct gwin_invparams *params)
{				/*gator_X11gwin_invert */

    static char rn[] = "gator_X11gwin_invert";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (0);

}				/*gator_X11gwin_invert */

/*------------------------------------------------------------------------
 * gator_X11gwin_getchar
 *
 * Description:
 *	Pick up a character from the given window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window to listen to.
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
gator_X11gwin_getchar(struct gwin *gwp)
{				/*gator_X11gwin_getchar */

    static char rn[] = "gator_X11gwin_getchar";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (-1);

}				/*gator_X11gwin_getchar */

/*------------------------------------------------------------------------
 * gator_X11gwin_getdimensions
 *
 * Description:
 *	Get the window's X,Y dimensions.
 *
 * Arguments:
 *	struct gwin *gwp	       : Ptr to the X11 window to examine.
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
gator_X11gwin_getdimensions(struct gwin *gwp, struct gwin_sizeparams *aparms)
{				/*gator_X11gwin_getdimensions */

    static char rn[] = "gator_X11gwin_getdimensions";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (-1);

}				/*gator_X11gwin_getdimensions */

/*------------------------------------------------------------------------
 * gator_X11gwin_wait
 *
 * Description:
 *	Wait until input is available.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the X11 window to wait on.
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
gator_X11gwin_wait(struct gwin *gwp)
{				/*gator_X11gwin_wait */

    static char rn[] = "gator_X11gwin_wait";	/*Routine name */

    if (X11_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (-1);

}				/*gator_X11gwin_wait */
