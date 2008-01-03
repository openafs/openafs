/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * gator_curseswindows.c
 *
 * Description:
 *	Implementation of the gator curses window facility.
 *
 *------------------------------------------------------------------------*/
#define	IGNORE_STDS_H
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");


#if defined(AFS_HPUX110_ENV) && !defined(__HP_CURSES)
#define __HP_CURSES
#endif

#ifndef	AFS_SUN5_ENV
#include <curses.h>		/*Curses library */
#endif
#include <sys/types.h>
#include <sys/file.h>
#if !defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV)
#include <sgtty.h>
#endif
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "gtxcurseswin.h"	/*Interface definition */
#include "gtxobjects.h"
#include "gtxframe.h"



int curses_debug;		/*Is debugging turned on? */
static char mn[] = "gator_curseswindows";	/*Module name */

/*
 * Version of standard operations for a curses window.
 */
struct gwinops curses_gwinops = {
    gator_cursesgwin_box,
    gator_cursesgwin_clear,
    gator_cursesgwin_destroy,
    gator_cursesgwin_display,
    gator_cursesgwin_drawline,
    gator_cursesgwin_drawrectangle,
    gator_cursesgwin_drawchar,
    gator_cursesgwin_drawstring,
    gator_cursesgwin_invert,
    gator_cursesgwin_getchar,
    gator_cursesgwin_getdimensions,
    gator_cursesgwin_wait,
};

struct gwinbaseops gator_curses_gwinbops = {
    gator_cursesgwin_create,
    gator_cursesgwin_cleanup,
};


/*
 * Macros to map pixel positions to row & column positions.
 * (Note: for now, they are the identity function!!)
 */
#define	GATOR_MAP_X_TO_COL(w, x)    (x)
#define	GATOR_MAP_Y_TO_LINE(w, y)   (y)

/*------------------------------------------------------------------------
 * gator_cursesgwin_init
 *
 * Description:
 *	Initialize the curses window package.
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
gator_cursesgwin_init(adebug)
     int adebug;

{				/*gator_cursesgwin_init */

    static char rn[] = "gator_cursesgwin_init";	/*Routine name */
    struct gator_cursesgwin *c_data;	/*Ptr to curses-specific data */

    /*
     * Remember if we'll be doing debugging, then init the curses package.
     */
    curses_debug = adebug;

    if (curses_debug)
	fprintf(stderr, "[%s:%s] Calling initscr()\n", mn, rn);
    initscr();

    /*
     * Fill out the base window structure for curses.
     */
    if (curses_debug)
	fprintf(stderr,
		"[%s:%s] Allocating %d bytes for curses window private space in base window\n",
		mn, rn, sizeof(struct gator_cursesgwin));
    c_data =
	(struct gator_cursesgwin *)malloc(sizeof(struct gator_cursesgwin));
    if (c_data == (struct gator_cursesgwin *)0) {
	fprintf(stderr,
		"[%s:%s] Can't allocate %d bytes for curses window private space in base window\n",
		mn, rn, sizeof(struct gator_cursesgwin));
	return (-1);
    }

    /*
     * Fill in the curses-specific base window info.  We assume that chars are 8x13.
     */
    c_data->wp = stdscr;
    c_data->charwidth = 8;
    c_data->charheight = 13;
    c_data->box_vertchar = '|';
    c_data->box_horizchar = '-';

    /*
     * Fill in the generic base window info.
     */
    gator_basegwin.w_type = GATOR_WIN_CURSES;
    gator_basegwin.w_x = 0;
    gator_basegwin.w_y = 0;
    gator_basegwin.w_width = c_data->charwidth * COLS;
    gator_basegwin.w_height = c_data->charheight * LINES;
    gator_basegwin.w_changed = 0;
    gator_basegwin.w_op = &curses_gwinops;
    gator_basegwin.w_parent = NULL;

    /*
     * Plug the private data into the generic part of the base window.
     */
    gator_basegwin.w_data = (int *)c_data;

    /*
     * Now, set the terminal into the right mode for handling input
     */
    raw();			/* curses raw mode */

    /* init the frame */
    gator_basegwin.w_frame = gtxframe_Create();

    /*
     * Clear out the screen and return the good news.
     */
    wclear(((struct gator_cursesgwin *)(gator_basegwin.w_data))->wp);
    return (0);

}				/*gator_cursesgwin_init */

/*------------------------------------------------------------------------
 * gator_cursesgwin_create
 *
 * Description:
 *	Create a curses window (incorrectly).
 *
 * Arguments:
 *	struct gator_cursesgwin_params *params : Ptr to creation parameters.
 *
 * Returns:
 *	Ptr to the created curses window if successful,
 *	Null ptr otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

struct gwin *
gator_cursesgwin_create(params)
     struct gator_cursesgwin_params *params;

{				/*gator_cursesgwin_create */

    static char rn[] = "gator_cursesgwin_create";	/*Routine name */
    struct gwin *newgwin;	/*Ptr to new curses window */
    struct gator_cursesgwin *c_data;	/*Ptr to curses-specific data */
    WINDOW *newcursgwin;	/*Ptr to new curses window */

    if (curses_debug)
	fprintf(stderr,
		"[%s:%s] Allocating %d bytes for new gwin structure\n", mn,
		rn, sizeof(struct gwin));
    newgwin = (struct gwin *)malloc(sizeof(struct gwin));
    if (newgwin == NULL) {
	fprintf(stderr,
		"[%s:%s] Can't malloc() %d bytes for new gwin structure: Errno is %d\n",
		mn, rn, sizeof(struct gwin), errno);
	return (NULL);
    }

    newgwin->w_type = GATOR_WIN_CURSES;
    newgwin->w_x = params->gwin_params.cr_x;
    newgwin->w_y = params->gwin_params.cr_y;
    newgwin->w_width = params->gwin_params.cr_width;
    newgwin->w_height = params->gwin_params.cr_height;
    newgwin->w_changed = 1;
    newgwin->w_op = &curses_gwinops;
    newgwin->w_parent = params->gwin_params.cr_parentwin;

    if (curses_debug)
	fprintf(stderr,
		"[%s:%s] Allocating %d bytes for curses window private space\n",
		mn, rn, sizeof(struct gator_cursesgwin));
    c_data =
	(struct gator_cursesgwin *)malloc(sizeof(struct gator_cursesgwin));
    if (c_data == (struct gator_cursesgwin *)0) {
	fprintf(stderr,
		"[%s:%s] Can't allocate %d bytes for curses window private space\n",
		mn, rn, sizeof(struct gator_cursesgwin));
	free(newgwin);
	return (NULL);
    }

    newcursgwin = newwin(newgwin->w_height,	/*Number of lines */
			 newgwin->w_width,	/*Number of columns */
			 newgwin->w_y,	/*Beginning y value */
			 newgwin->w_x);	/*Beginning x value */
    if (newcursgwin == (WINDOW *) 0) {
	fprintf(stderr,
		"[%s:%s] Failed to create curses window via newwin()\n", mn,
		rn);
	free(newgwin);
	free(c_data);
	return (NULL);
    }

    /*
     * Now, fill in the curses-specific window info.
     */
    c_data->wp = newcursgwin;
    c_data->charwidth = params->charwidth;
    c_data->charheight = params->charheight;
    c_data->box_vertchar = params->box_vertchar;
    c_data->box_horizchar = params->box_horizchar;

    /*
     * Plug in a frame at the top-level.
     */
    newgwin->w_frame = gtxframe_Create();

    /*
     * Plug the curses private data into the generic window object, then
     * return the new window's info.
     */
    newgwin->w_data = (int *)c_data;
    return (newgwin);

}				/*gator_cursesgwin_create */

/*------------------------------------------------------------------------
 * gator_cursesgwin_cleanup
 *
 * Description:
 *	Clean up, probably right before the caller exits.
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
gator_cursesgwin_cleanup(gwp)
     struct gwin *gwp;

{				/*gator_cursesgwin_cleanup */

    static char rn[] = "gator_cursesgwin_cleanup";	/*Routine name */
    struct gator_cursesgwin *cwp;	/*Curses private area ptr */

    cwp = (struct gator_cursesgwin *)(gwp->w_data);

    /*
     * Cleaning up in curses is extremely easy - one simple call.  We also
     * want to clear the screen before we go.
     */
    if (curses_debug)
	fprintf(stderr, "[%s:%s] Calling wclear() on window at 0x%x\n", mn,
		rn, cwp->wp);
    wclear(cwp->wp);
    wrefresh(cwp->wp);

    /*
     * Now, set the terminal back into normal mode.
     */
    noraw();

    if (curses_debug)
	fprintf(stderr, "[%s:%s] Calling endwin()\n", mn, rn);
    endwin();

    return (0);

}				/*gator_cursesgwin_cleanup */

/*------------------------------------------------------------------------
 * gator_cursesgwin_box
 *
 * Description:
 *	Draw a box around the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window to draw
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
gator_cursesgwin_box(gwp)
     struct gwin *gwp;

{				/*gator_cursesgwin_box */

    static char rn[] = "gator_cursesgwin_box";	/*Routine name */
    struct gator_cursesgwin *cwp;	/*Ptr to curses private area */

    cwp = (struct gator_cursesgwin *)(gwp->w_data);
    if (curses_debug)
	fprintf(stderr, "[%s:%s] Calling box() on window at 0x%x\n", mn, rn,
		cwp->wp);
    box(cwp->wp, cwp->box_vertchar, cwp->box_horizchar);

    return (0);

}				/*gator_cursesgwin_box */

/*------------------------------------------------------------------------
 * gator_cursesgwin_clear
 *
 * Description:
 *	Clear out the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window to clear out.
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
gator_cursesgwin_clear(gwp)
     struct gwin *gwp;

{				/*gator_cursesgwin_clear */

    static char rn[] = "gator_cursesgwin_clear";	/*Routine name */
    struct gator_cursesgwin *cwp;	/*Ptr to curses private area */

    /*
     * Clearing windows is very easy in curses; just one call will do it.
     */
    cwp = (struct gator_cursesgwin *)(gwp->w_data);
    if (curses_debug)
	fprintf(stderr, "[%s:%s] Calling wclear() on window at 0x%x\n", mn,
		rn, cwp->wp);
    wclear(cwp->wp);

    return (0);

}				/*gator_cursesgwin_clear */

/*------------------------------------------------------------------------
 * gator_cursesgwin_destroy
 *
 * Description:
 *	Destroy the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window to destroy.
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
gator_cursesgwin_destroy(gwp)
     struct gwin *gwp;

{				/*gator_cursesgwin_destroy */

    static char rn[] = "gator_cursesgwin_destroy";	/*Routine name */
    struct gator_cursesgwin *cwp;	/*Ptr to curses private area */

    cwp = (struct gator_cursesgwin *)(gwp->w_data);
    if (curses_debug)
	fprintf(stderr, "[%s:%s] Calling delwin() on window at 0x%x\n", mn,
		rn, cwp->wp);
    delwin(cwp->wp);

    return (0);

}				/*gator_cursesgwin_destroy */

/*------------------------------------------------------------------------
 * gator_cursesgwin_display
 *
 * Description:
 *	Display/redraw the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window to draw.
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
gator_cursesgwin_display(gwp)
     struct gwin *gwp;

{				/*gator_cursesgwin_display */

    struct gator_cursesgwin *cwp;	/*Curses private area ptr */

    cwp = (struct gator_cursesgwin *)(gwp->w_data);

    wclear(cwp->wp);		/* clear screen */
    gtxframe_Display(gwp->w_frame, gwp);	/* display the frame */
    wrefresh(cwp->wp);		/* redraw the guy */
    return (0);

}				/*gator_cursesgwin_display */

/*------------------------------------------------------------------------
 * gator_cursesgwin_drawline
 *
 * Description:
 *	Draw a line between two points in the given curses
 *	window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window in which
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
gator_cursesgwin_drawline(gwp, params)
     struct gwin *gwp;
     struct gwin_lineparams *params;

{				/*gator_cursesgwin_drawline */

    static char rn[] = "gator_cursesgwin_drawline";	/*Routine name */

    if (curses_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (0);

}				/*gator_cursesgwin_drawline */

/*------------------------------------------------------------------------
 * gator_cursesgwin_drawrectangle
 *
 * Description:
 *	Draw a rectangle in the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window in which
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
gator_cursesgwin_drawrectangle(gwp, params)
     struct gwin *gwp;
     struct gwin_rectparams *params;

{				/*gator_cursesgwin_drawrectangle */

    static char rn[] = "gator_cursesgwin_drawrectangle";	/*Routine name */

    if (curses_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (0);

}				/*gator_cursesgwin_drawrectangle */

/*------------------------------------------------------------------------
 * gator_cursesgwin_drawchar
 *
 * Description:
 *	Draw a character in the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window in which
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
gator_cursesgwin_drawchar(gwp, params)
     struct gwin *gwp;
     struct gwin_charparams *params;

{				/*gator_cursesgwin_drawchar */

    static char rn[] = "gator_cursesgwin_drawchar";	/*Routine name */
    struct gator_cursesgwin *cwp;	/*Ptr to curses private area */
    int curses_x, curses_y;	/*Mapped x,y positions */

    cwp = (struct gator_cursesgwin *)(gwp->w_data);
    curses_x = GATOR_MAP_X_TO_COL(cwp, params->x);
    curses_y = GATOR_MAP_Y_TO_LINE(cwp, params->y);
    if (curses_debug)
	fprintf(stderr,
		"[%s:%s] Drawing char '%c' on window at 0x%x at (%d, %d) [line %d, column %d]%s\n",
		mn, rn, params->c, cwp->wp, params->x, params->y, curses_y,
		curses_x, (params->highlight ? ", using standout mode" : ""));
    wmove(cwp->wp, curses_y, curses_x);
    if (params->highlight)
	wstandout(cwp->wp);
    waddch(cwp->wp, params->c);
    if (params->highlight)
	wstandend(cwp->wp);

    return (0);

}				/*gator_cursesgwin_drawchar */

/*------------------------------------------------------------------------
 * gator_cursesgwin_drawstring
 *
 * Description:
 *	Draw a string in the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window in which
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
gator_cursesgwin_drawstring(gwp, params)
     struct gwin *gwp;
     struct gwin_strparams *params;

{				/*gator_cursesgwin_drawstring */

    static char rn[] = "gator_cursesgwin_drawstring";	/*Routine name */
    struct gator_cursesgwin *cwp;	/*Ptr to curses private area */
    int curses_x, curses_y;	/*Mapped x,y positions */

    cwp = (struct gator_cursesgwin *)(gwp->w_data);
    curses_x = GATOR_MAP_X_TO_COL(cwp, params->x);
    curses_y = GATOR_MAP_Y_TO_LINE(cwp, params->y);
    if (curses_debug)
	fprintf(stderr,
		"[%s:%s] Drawing string '%s' on window at 0x%x at (%d, %d) [line %d, column %d]%s\n",
		mn, rn, params->s, cwp->wp, params->x, params->y, curses_y,
		curses_x, (params->highlight ? ", using standout mode" : ""));
    wmove(cwp->wp, curses_y, curses_x);
    if (params->highlight)
	wstandout(cwp->wp);
    waddstr(cwp->wp, params->s);
    if (params->highlight)
	wstandend(cwp->wp);

    return (0);

}				/*gator_cursesgwin_drawstring */

/*------------------------------------------------------------------------
 * gator_cursesgwin_invert
 *
 * Description:
 *	Invert a region in the given curses window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window in which
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
gator_cursesgwin_invert(gwp, params)
     struct gwin *gwp;
     struct gwin_invparams *params;

{				/*gator_cursesgwin_invert */

    static char rn[] = "gator_cursesgwin_invert";	/*Routine name */

    if (curses_debug)
	fprintf(stderr, "[%s:%s] This routine is currently a no-op\n", mn,
		rn);

    return (0);

}				/*gator_cursesgwin_invert */

/*------------------------------------------------------------------------
 * gator_cursesgwin_getchar
 *
 * Description:
 *	Pick up a character from the given window.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window to listen to.
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
gator_cursesgwin_getchar(gwp)
     struct gwin *gwp;

{				/*gator_cursesgwin_getchar */

    return (getc(stdin));

}				/*gator_cursesgwin_getchar */

/*------------------------------------------------------------------------
 * gator_cursesgwin_wait
 *
 * Description:
 *	Wait until input is available.
 *
 * Arguments:
 *	struct gwin *gwp : Ptr to the curses window to wait on.
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
gator_cursesgwin_wait(gwp)
     struct gwin *gwp;

{				/*gator_cursesgwin_wait */

    while (!LWP_WaitForKeystroke(-1));

    return (0);

}				/*gator_cursesgwin_wait */

/*------------------------------------------------------------------------
 * gator_cursesgwin_getdimensions
 *
 * Description:
 *	Get the window's X,Y dimensions.
 *
 * Arguments:
 *	struct gwin *gwp	       : Ptr to the curses window to examine.
 *	struct gwin_sizeparams *params : Ptr to the size params to set.
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
gator_cursesgwin_getdimensions(gwp, aparms)
     struct gwin_sizeparams *aparms;
     struct gwin *gwp;

{				/*gator_cursesgwin_getdimensions */

    struct gator_cursesgwin *cwp;	/*Curses-specific data */

    cwp = (struct gator_cursesgwin *)(gwp->w_data);
#if defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN60_ENV)
    aparms->maxx = cwp->wp->maxx;
    aparms->maxy = cwp->wp->maxy;
#elif defined(AFS_NBSD_ENV)
    aparms->maxx = getmaxx(cwp->wp);
    aparms->maxy = getmaxy(cwp->wp);
#else
    aparms->maxx = cwp->wp->_maxx;
    aparms->maxy = cwp->wp->_maxy;
#endif

    return (0);

}				/*gator_cursesgwin_getdimensions */
