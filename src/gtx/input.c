/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define	IGNORE_STDS_H
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef AFS_HPUX_ENV
#include <sys/types.h>
#endif
#include <lwp.h>

#include "gtxobjects.h"
#include "gtxwindows.h"
#include "gtxcurseswin.h"
#include "gtxinput.h"
#include "gtxkeymap.h"
#include "gtxframe.h"
#include <afs/stds.h>


/* process input */
gtx_InputServer(awin)
     register struct gwin *awin;
{
    register int tc;
    register int code;
    register struct gtx_frame *tframe;

    WOP_DISPLAY(awin);		/* start off with a clean display */
    while (1) {
	/* get a character from the generic window */
	tframe = awin->w_frame;
	code = WOP_WAIT(awin);
	if (code) {
	    printf("***WAIT FAILURE %d****\n", code);
	    exit(1);
	}
	tc = WOP_GETCHAR(awin);
	tframe->flags &= ~GTXFRAME_NEWDISPLAY;	/* OK to clear now */
	if (tc < 0)
	    break;		/* EOF or some such */
	/* otherwise, process the character and go get a new one */
	gtxframe_ClearMessageLine(tframe);
	tframe->flags &= ~(GTXFRAME_RECURSIVEEND | GTXFRAME_RECURSIVEERR);
	keymap_ProcessKey(tframe->keystate, tc, awin);
	tframe = awin->w_frame;	/* in case command changed it */
	if (tframe->flags & GTXFRAME_RECURSIVEEND) {
	    tframe->flags &= ~GTXFRAME_RECURSIVEEND;
	    return 0;
	}
	tframe->flags &= ~GTXFRAME_RECURSIVEEND;
	WOP_DISPLAY(awin);	/* eventually calls gtxframe_Display */
    }
    return 0;
}

struct gwin *
gtx_Init(astartInput, atype)
     int atype;			/* type of window to create */
     int astartInput;
{
    PROCESS junk;
    struct onode_initparams oi_params;	/* object init params */
    struct gwin_initparams wi_params;	/* window initialization params */
    register struct gwin *twin;
    register int code;

    /* setup the main window structure */
    wi_params.i_type = GATOR_WIN_CURSES;
    wi_params.i_x = 0;
    wi_params.i_y = 0;
    wi_params.i_width = 80;
    wi_params.i_height = 200;
    wi_params.i_debug = 0;	/* or 1 if we want debugging done */

    /*
     * Set up the basic onode initialization parameters, throwing in
     * the graphics-specific stuff.
     */
    oi_params.i_debug = 0;	/* or 1 if we want debugging */
    oi_params.i_gwparams = &wi_params;

    code = gator_objects_init(&oi_params);
    if (code)
	return NULL;

    /* if we start input thread */
    IOMGR_Initialize();		/* input thread uses it */
    if (astartInput)
	code =
	    LWP_CreateProcess(gtx_InputServer, 8192, LWP_NORMAL_PRIORITY,
			      (void *)0, "gx-listener", &junk);
    /* all done */
    twin = &gator_basegwin;
    return twin;
}
