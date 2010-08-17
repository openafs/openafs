/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * gator_windows.c
 *
 * Description:
 *	Implementation of the gator windows interface.
 *
 *--------------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>


/* On DUX "IN" is a variable in curses.h, so this can be a bit of a problem */
#ifdef IN
#undef IN
#endif

#include "gtxwindows.h"		/*Interface for this module */
#include "gtxcurseswin.h"	/*Interface for the curses module */
#include "gtxdumbwin.h"		/*Interface for the dumb terminal module */
#include "gtxX11win.h"		/*Interface for the X11 module */

static char mn[] = "gator_windows";	/*Module name */
struct gwinbaseops gwinbops;	/*Base window operation fn array */
struct gwin gator_basegwin;	/*Base gator window */

/*--------------------------------------------------------------------------------
 * gw_init
 *
 * Description:
 *	Initialize the gator window package.
 *
 * Arguments:
 *	struct gwin_initparams *params : Ptr to initialization params.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	*** MUST BE THE FIRST ROUTINE CALLED FROM
 *	      THIS PACKAGE ***
 *
 * Side Effects:
 *	Sets up the chosen lower-level graphics package, as well
 *	as the base operation array (gwinbops).  Also sets up the
 *	base window.
 *--------------------------------------------------------------------------------*/

int
gw_init(struct gwin_initparams *params)
{				/*gw_init */

    static char rn[] = "gw_init";	/*Routine name */
    int code;		/*Return code */
    int gwin_debug;		/*Is debugging turned on? */

    /*
     * Remember our debugging level.
     */
    gwin_debug = params->i_debug;
    if (gwin_debug)
	fprintf(stderr, "[%s:%s] Window debugging turned on\n", mn, rn);

    /*
     * What we do/call depends on the type of lower-level graphics
     * package we'll be using.
     */
    switch (params->i_type) {
    case GATOR_WIN_DUMB:	/*Dumb terminal */
	if (gwin_debug)
	    fprintf(stderr,
		    "[%s:%s] Initializing for the dumb terminal package\n",
		    mn, rn);
	gwinbops = gator_dumb_gwinbops;
	code = gator_dumbgwin_init(gwin_debug);
	if (code) {
	    fprintf(stderr,
		    "[%s:%s] Error in dumb terminal initialization routine, gator_dumbgwin_init(): %d\n",
		    mn, rn, code);
	    return (code);
	}
	break;

    case GATOR_WIN_CURSES:	/*Curses */
	if (gwin_debug)
	    fprintf(stderr, "[%s:%s] Initializing for the curses package\n",
		    mn, rn);
	gwinbops = gator_curses_gwinbops;
	code = gator_cursesgwin_init(gwin_debug);
	if (code) {
	    fprintf(stderr,
		    "[%s:%s] Error in curses initialization routine, gator_cursesgwin_init(): %d\n",
		    mn, rn, code);
	    return (code);
	}
	break;

    case GATOR_WIN_X11:	/*X11 */
	if (gwin_debug)
	    fprintf(stderr, "[%s:%s] Initializing for the X11 package\n", mn,
		    rn);
	gwinbops = gator_X11_gwinbops;
	code = gator_X11gwin_init(gwin_debug);
	if (code) {
	    fprintf(stderr,
		    "[%s:%s] Error in X11 initialization routine, gator_X11gwin_init(): %d\n",
		    mn, rn, code);
	    return (code);
	}
	break;

    default:
	fprintf(stderr, "[%s:%s] Illegal choice of graphics system: %d\n", mn,
		rn, params->i_type);
	fprintf(stderr, "\tLegal choices are:\n");
	fprintf(stderr, "\t\t%d: Dumb terminal\n", GATOR_WIN_DUMB);
	fprintf(stderr, "\t\t%d: Curses\n", GATOR_WIN_CURSES);
	fprintf(stderr, "\t\t%d: X11\n", GATOR_WIN_X11);
	return (-1);
    }				/*end switch (params->i_type) */

    /*
     * Finally, return the good news.
     */
    return (0);

}				/*gw_init */
