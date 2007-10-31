/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * screen_test: A test of the gator screen operations.
 *--------------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "gtxwindows.h"		/*Generalized window interface */
#include "gtxcurseswin.h"	/*Curses window interface */
#include "gtxdumbwin.h"		/*Dumb terminal window interface */
#include "gtxX11win.h"		/*X11 window interface */
#include <cmd.h>		/*Command interpretation library */
#include <errno.h>


/*
 * Command line parameter indicies.
 */
#define	P_PACKAGE   0
#define	P_DEBUG	    1

static char pn[] = "screen_test";	/*Program name */
static int screen_debug = 0;	/*Is debugging turned on? */

/*--------------------------------------------------------------------------------
 * test_this_package
 *
 * Description:
 *	Routine that does the actual testing of the chosen graphics
 *	package.
 *
 * Arguments:
 *	pkg : Number of package to test.
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
 *--------------------------------------------------------------------------------*/

static int
test_this_package(pkg)
     int pkg;

{				/*test_this_package */

    static char rn[] = "test_this_package";	/*Routine name */
    register int code;		/*Return code */
    struct gwin_initparams init_params;	/*Window initialization params */
    struct gator_cursesgwin_params c_crparams;	/*Curses window creation params */
    struct gator_dumbgwin_params d_crparams;	/*Dumb terminal window creation params */
    struct gator_X11gwin_params x_crparams;	/*X11 window creation params */
    struct gwin *newwin;	/*New (sub)window */
    struct gwin_strparams strparams;	/*String-drawing params */
    struct gwin_charparams charparams;	/*Char-drawing params */
    char s[128];		/*Test string */
    int currx, curry;		/*Sliding values of x & y */
    int currhighlight;		/*Highlight this time around? */

    /*
     * Initialize the gator window package to drive the desired subsystem.
     */
    init_params.i_type = pkg;
    init_params.i_x = 0;
    init_params.i_y = 0;
    init_params.i_width = 80;
    init_params.i_height = 200;
    init_params.i_debug = screen_debug;

    code = gw_init(&init_params);
    if (code) {
	fprintf(stderr,
		"[%s:%s] Can't initialize gator windows for package %d; error is: %d\n",
		pn, rn, pkg, code);
	return (code);
    }

    sprintf(s, "Screen has %d lines", LINES);
    strparams.x = 5;
    strparams.y = LINES / 2;
    strparams.s = s;
    strparams.highlight = 0;
    WOP_DRAWSTRING(&gator_basegwin, &strparams);

    sprintf(s, "and %d columns", COLS);
    strparams.x = 5;
    strparams.y = strparams.y + 1;
    strparams.s = s;
    strparams.highlight = 1;
    WOP_DRAWSTRING(&gator_basegwin, &strparams);

    /*
     * Draw a set of chars down a diagonal, pausing inbetween.
     */
    currhighlight = 1;
    for (currx = curry = 0; (currx < COLS) && (curry < LINES);
	 currx++, curry++) {
	charparams.x = currx;
	charparams.y = curry;
	charparams.c = 'x';
	charparams.highlight = currhighlight;
	currhighlight = (currhighlight ? 0 : 1);
	WOP_DRAWCHAR(&gator_basegwin, &charparams);
	sleep(1);
    }
    WOP_BOX(&gator_basegwin);

    /*
     * Fill in the new window creation parameters and go for it.
     */
    c_crparams.gwin_params.cr_type = pkg;
    c_crparams.gwin_params.cr_x = 40;
    c_crparams.gwin_params.cr_y = 5;
    c_crparams.gwin_params.cr_width = 20;
    c_crparams.gwin_params.cr_height = 10;
    c_crparams.gwin_params.cr_parentwin = (struct gwin *)(&gator_basegwin);
    c_crparams.charwidth = 8;
    c_crparams.charheight = 13;
    c_crparams.box_vertchar = '|';
    c_crparams.box_horizchar = '-';
    newwin = WOP_CREATE(&c_crparams);
    if (newwin == NULL) {
	fprintf(stderr, "[%s:%s] Can't create a new window\n", pn, rn);
    } else if (screen_debug)
	fprintf(stderr, "[%s:%s] New window created at 0x%x\n", pn, rn,
		newwin);

    /*
     * Draw something to the new window; first, a highlighted banner.
     */
    sprintf(s, "%s", "Sub-window        ");
    strparams.x = 1;
    strparams.y = 1;
    strparams.s = s;
    strparams.highlight = 1;
    WOP_DRAWSTRING(newwin, &strparams);

    /*
     * Next, draw an `x' at each corner.
     */
    charparams.c = 'x';
    charparams.highlight = 1;
    charparams.x = 1;
    charparams.y = 2;
    WOP_DRAWCHAR(newwin, &charparams);
    charparams.x = 18;
    charparams.y = 2;
    WOP_DRAWCHAR(newwin, &charparams);
    charparams.x = 1;
    charparams.y = 8;
    WOP_DRAWCHAR(newwin, &charparams);
    charparams.x = 18;
    charparams.y = 8;
    WOP_DRAWCHAR(newwin, &charparams);

    /*
     * Finally, box the sucker.
     */
    WOP_BOX(newwin);

    /*
     * Draw a few other things in the original window.
     */
    sprintf(s, "Screen has %d lines", LINES);
    strparams.x = 5;
    strparams.y = LINES / 2;
    strparams.s = s;
    strparams.highlight = 0;
    WOP_DRAWSTRING(&gator_basegwin, &strparams);

    sprintf(s, "and %d columns", COLS);
    strparams.x = 5;
    strparams.y = strparams.y + 1;
    strparams.s = s;
    strparams.highlight = 1;
    WOP_DRAWSTRING(&gator_basegwin, &strparams);

    /*
     * Let people admire the handiwork, then clean up.
     */
    WOP_DISPLAY(&gator_basegwin);
    sleep(10);
    WOP_DISPLAY(&gator_basegwin);
    WOP_CLEANUP(&gator_basegwin);

}				/*test_this_package */

/*--------------------------------------------------------------------------------
 * screen_testInit
 *
 * Description:
 *	Routine that is called when screen_test is invoked, responsible
 *	for basic initialization and command line parsing.
 *
 * Arguments:
 *	as	: Command syntax descriptor.
 *	arock	: Associated rock (not used here).
 *
 * Returns:
 *	Zero (but may exit the entire program on error!)
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	Initializes this program.
 *--------------------------------------------------------------------------------*/

static int
screen_testInit(struct cmd_syndesc *as, void *arock)
{				/*screen_testInit */

    static char rn[] = "screen_testInit";	/*Routine name */
    int pkg_to_test;		/*Which package to test */

    if (as->parms[P_DEBUG].items != 0)
	screen_debug = 1;
    pkg_to_test = atoi(as->parms[P_PACKAGE].items->data);
    fprintf(stderr, "[%s:%s] Testing graphics package %d: ", pn, rn,
	    pkg_to_test);
    switch (pkg_to_test) {
    case GATOR_WIN_CURSES:
	fprintf(stderr, "curses\n");
	break;
    case GATOR_WIN_DUMB:
	fprintf(stderr, "dumb terminal\n");
	break;
    case GATOR_WIN_X11:
	fprintf(stderr, "X11\n");
	break;
    default:
	fprintf(stderr, "Illegal graphics package: %d\n", pkg_to_test);
    }				/*end switch (pkg_to_test) */

    /*
     * Now, drive the sucker.
     */
    test_this_package(pkg_to_test);

    /*
     * We initialized (and ran) correctly, so return the good news.
     */
    return (0);

}				/*screen_testInit */

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;

{				/*main */

    static char rn[] = "main";	/*Routine name */
    register afs_int32 code;	/*Return code */
    register struct cmd_syndesc *ts;	/*Ptr to cmd line syntax descriptor */

    /*
     * There really aren't any opcodes here, but we do want to interpret switches
     * from the command line.  So, all we need do is set up the initcmd ``opcode''.
     */
    ts = cmd_CreateSyntax("initcmd", screen_testInit, NULL,
			  "Initialize, interpret command line");
    cmd_AddParm(ts, "-package", CMD_SINGLE, CMD_REQUIRED,
		"Graphics package to use");
    cmd_AddParm(ts, "-debug", CMD_FLAG, CMD_OPTIONAL, "Turn debugging on");

    /*
     * Parse command-line switches & execute the test, then get the heck out of here.
     */
    code = cmd_Dispatch(argc, argv);
    if (code) {
	fprintf(stderr, "[%s:%s] Call to cmd_Dispatch() failed; code is %d\n",
		pn, rn, code);
	exit(1);
    }

}				/*main */
