/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * object_test: A test of the gator object operations.
 *--------------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "gtxscreenobj.h"	/*Gator screen object interface */
#include "gtxtextobj.h"		/*Gator text object interface */
#include "gtxlightobj.h"	/*Gator light object interface */
#include "gtxwindows.h"		/*Gator generic window package */
#include "gtxcurseswin.h"	/*Gator curses window package */
#include "gtxdumbwin.h"		/*Gator dumb terminal window package */
#include "gtxX11win.h"		/*Gator X11 window package */
#include <errno.h>
#include <stdio.h>		/*Standard I/O stuff */
#include <cmd.h>		/*Command interpretation library */


/*
 * Command line parameter indicies.
 */
#define	P_PACKAGE   0
#define	P_DEBUG	    1

static char pn[] = "object_test";	/*Program name */
static int object_debug = 0;	/*Is debugging turned on? */

/*--------------------------------------------------------------------------------
 * test_objects
 *
 * Description:
 *	Routine that does the actual testing of gator objects.
 *
 * Arguments:
 *	int pkg : Number of windowing package to use.
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
test_objects(pkg)
     int pkg;

{				/*test_objects */

    static char rn[] = "test_objects";	/*Routine name */
    register int code;		/*Return code */
    struct onode_initparams oi_params;	/*Init params */
    struct gwin_initparams wi_params;	/*Window initialization params */
#if 0
    /*We don't need these, do we? */
    struct gator_cursesgwin_params c_crparams;	/*Curses window creation params */
    struct gator_dumbgwin_params d_crparams;	/*Dumb terminal window creation params */
    struct gator_X11gwin_params x_crparams;	/*X11 window creation params */
#endif /* 0 */
    struct gator_light_crparams light_crparams;	/*Light creation params */
    char helpstring1[128];	/*Help string to use */
    char helpstring2[128];	/*Help string to use */
    char helpstring3[128];	/*Help string to use */
    char helpstring4[128];	/*Help string to use */
    char helpstringt1[128];	/*Help string to use */
    struct onode *light_onp1;	/*Ptr to light onode */
    struct onode *light_onp2;	/*Ptr to another light onode */
    struct onode *light_onp3;	/*Ptr to another light onode */
    struct onode *light_onp4;	/*Ptr to another light onode */
    struct onode *text_onp1;	/*Ptr to text onode */
    int i;			/*Generic loop variable */
    int setting;		/*Current light setting */
    struct gator_textobj_params text_crparams;	/*Text creation params */
    char s[128];		/*Another string */
    struct gwin_strparams strparams;	/*String-drawing params */

    /*
     * Initialize the chosen gator window package.
     */
    if (object_debug)
	fprintf(stderr,
		"[%s:%s] Setting up initialization params for window package %d\n",
		pn, rn, pkg);
    wi_params.i_type = pkg;
    wi_params.i_x = 0;
    wi_params.i_y = 0;
    wi_params.i_width = 80;
    wi_params.i_height = 200;
    wi_params.i_debug = object_debug;

    /*
     * Set up the basic onode initialization parameters, throwing in
     * the graphics-specific stuff.
     */
    oi_params.i_debug = object_debug;
    oi_params.i_gwparams = &wi_params;
    code = gator_objects_init(&oi_params);
    if (code) {
	fprintf(stderr,
		"[%s:%s] Can't initialize gator objects package for window system %d; error is: %d\n",
		pn, rn, pkg, code);
	return (code);
    }

    /*
     * Set up some light objects and put them up on the screen.
     */
    sprintf(helpstring1, "%s", "Help string for light 1");
    light_crparams.onode_params.cr_type = GATOR_OBJ_LIGHT;
    sprintf(light_crparams.onode_params.cr_name, "%s", "Light1");
    light_crparams.onode_params.cr_x = 10;
    light_crparams.onode_params.cr_y = 10;
    light_crparams.onode_params.cr_width = 10;
    light_crparams.onode_params.cr_height = 1;
    light_crparams.onode_params.cr_window = &gator_basegwin;
    light_crparams.onode_params.cr_home_obj = NULL;
    light_crparams.onode_params.cr_prev_obj = NULL;
    light_crparams.onode_params.cr_parent_obj = NULL;
    light_crparams.onode_params.cr_helpstring = helpstring1;

    light_crparams.appearance = 0;
    light_crparams.flashfreq = 0;
    sprintf(light_crparams.label, "%s", "Light 1   ");
    light_crparams.label_x = 0;
    light_crparams.label_y = 0;

    light_onp1 =
	gator_objects_create((struct onode_createparams *)(&light_crparams));
    if (light_onp1 == NULL) {
	fprintf(stderr, "[%s:%s] Can't create light object\n", pn, rn);
	exit(-1);
    }

    sprintf(helpstring2, "%s", "Help string for light 2");
    light_crparams.onode_params.cr_helpstring = helpstring2;
    light_crparams.onode_params.cr_x = 10;
    light_crparams.onode_params.cr_y = 12;
    sprintf(light_crparams.onode_params.cr_name, "%s", "Light2");
    sprintf(light_crparams.label, "%s", "Light 2   ");
    light_onp2 =
	gator_objects_create((struct onode_createparams *)(&light_crparams));
    if (light_onp2 == NULL) {
	fprintf(stderr, "[%s:%s] Can't create light object\n", pn, rn);
	exit(-1);
    }

    sprintf(helpstring3, "%s", "Help string for light 3");
    light_crparams.onode_params.cr_helpstring = helpstring3;
    light_crparams.onode_params.cr_x = 10;
    light_crparams.onode_params.cr_y = 14;
    sprintf(light_crparams.onode_params.cr_name, "%s", "Light3");
    sprintf(light_crparams.label, "%s", "Light 3   ");
    light_onp3 =
	gator_objects_create((struct onode_createparams *)(&light_crparams));
    if (light_onp3 == NULL) {
	fprintf(stderr, "[%s:%s] Can't create light object\n", pn, rn);
	exit(-1);
    }

    sprintf(helpstring4, "%s", "Help string for light 4");
    light_crparams.onode_params.cr_helpstring = helpstring4;
    light_crparams.onode_params.cr_x = 21;
    light_crparams.onode_params.cr_y = 10;
    sprintf(light_crparams.onode_params.cr_name, "%s", "Light4");
    sprintf(light_crparams.label, "%s", "Light 4   ");
    light_onp4 =
	gator_objects_create((struct onode_createparams *)(&light_crparams));
    if (light_onp4 == NULL) {
	fprintf(stderr, "[%s:%s] Can't create light object\n", pn, rn);
	exit(-1);
    }

    /*
     * Create a text object, too.
     */
    sprintf(helpstringt1, "%s", "Help string for text");
    text_crparams.onode_params.cr_type = GATOR_OBJ_TEXT;
    sprintf(text_crparams.onode_params.cr_name, "%s", "Text1");
    text_crparams.onode_params.cr_x = 30;
    text_crparams.onode_params.cr_y = 10;
    text_crparams.onode_params.cr_width = 35;
    text_crparams.onode_params.cr_height = 7;
    text_crparams.onode_params.cr_window = &gator_basegwin;
    text_crparams.onode_params.cr_home_obj = NULL;
    text_crparams.onode_params.cr_prev_obj = NULL;
    text_crparams.onode_params.cr_parent_obj = NULL;
    text_crparams.onode_params.cr_helpstring = helpstringt1;
    text_crparams.maxEntries = 7;
    text_crparams.maxCharsPerEntry = 35;

    text_onp1 =
	gator_objects_create((struct onode_createparams *)(&text_crparams));
    if (text_onp1 == NULL) {
	fprintf(stderr, "[%s:%s] Can't create text object\n", pn, rn);
	exit(-1);
    }
    OOP_DISPLAY(text_onp1);
    sleep(2);

    /*
     * Now that we have our lights, turn them on and off a few times.
     */
    setting = 1;
    sprintf(s, "%s", "ABCD");
    strparams.x = 0;
    strparams.y = 0;
    strparams.s = s;
    strparams.highlight = 0;

    for (i = 0; i < 10; i++) {
	code = gator_light_set(light_onp1, setting);
	if (code)
	    fprintf(stderr,
		    "[%s:%s] Can't set gator light at 0x%x to %d (%s)\n", pn,
		    rn, light_onp1, setting, (setting ? "ON" : "OFF"));
	else
	    OOP_DISPLAY(light_onp1);

	code = gator_light_set(light_onp2, setting);
	if (code)
	    fprintf(stderr,
		    "[%s:%s] Can't set gator light at 0x%x to %d (%s)\n", pn,
		    rn, light_onp2, setting, (setting ? "ON" : "OFF"));
	else
	    OOP_DISPLAY(light_onp2);

	code = gator_light_set(light_onp3, setting);
	if (code)
	    fprintf(stderr,
		    "[%s:%s] Can't set gator light at 0x%x to %d (%s)\n", pn,
		    rn, light_onp3, setting, (setting ? "ON" : "OFF"));
	else
	    OOP_DISPLAY(light_onp3);

	code = gator_light_set(light_onp4, setting);
	if (code)
	    fprintf(stderr,
		    "[%s:%s] Can't set gator light at 0x%x to %d (%s)\n", pn,
		    rn, light_onp4, setting, (setting ? "ON" : "OFF"));
	else
	    OOP_DISPLAY(light_onp4);
	setting = (setting ? 0 : 1);

	sleep(1);

	WOP_DRAWSTRING(text_onp1->o_window, &strparams);
	strparams.x++;
	strparams.y++;
	sleep(1);

    }				/*Flash loop */

    OOP_DISPLAY(text_onp1);

    /*
     * Start writing stuff to our text object.
     */
    sprintf(s, "%s", "This is the first line");
    code = gator_text_Write(text_onp1, s, strlen(s), 0, 1);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(2);

    sprintf(s, "%s", "This is the");
    code = gator_text_Write(text_onp1, s, strlen(s), 0, 0);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(2);

    sprintf(s, "%s", " second line");
    code = gator_text_Write(text_onp1, s, strlen(s), 0, 1);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(2);

    sprintf(s, "%s", "This is the highlighted third line");
    code = gator_text_Write(text_onp1, s, strlen(s), 1, 1);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(2);

    sprintf(s, "%s",
	    "This is the very, very, very, very, very, very, very long fourth line");
    code = gator_text_Write(text_onp1, s, strlen(s), 0, 1);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(2);

    sprintf(s, "%s", "This is line 5");
    code = gator_text_Write(text_onp1, s, strlen(s), 0, 1);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(2);

    sprintf(s, "%s", "This is line 6");
    code = gator_text_Write(text_onp1, s, strlen(s), 0, 1);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(2);

    sprintf(s, "%s", "This is line 7");
    code = gator_text_Write(text_onp1, s, strlen(s), 0, 1);
    if (code)
	fprintf(stderr,
		"[%s:%s] Can't write '%s' (%d chars) to text object at 0x%x; error code is %d\n",
		pn, rn, s, strlen(s), text_onp1, code);
    sleep(4);

    /*
     * Now, try to scroll the sucker.
     */
    for (i = 0; i < 10; i++) {
	code = gator_text_Scroll(text_onp1, 1, GATOR_TEXT_SCROLL_UP);
	if (code)
	    fprintf(stderr,
		    "[%s:%s] Can't scroll up 1 line in text object at 0x%x\n",
		    pn, rn, text_onp1);
	sleep(2);
    }

    for (i = 0; i < 10; i++) {
	code = gator_text_Scroll(text_onp1, 2, GATOR_TEXT_SCROLL_DOWN);
	fprintf(stderr,
		"[%s:%s] Can't scroll down 2 lines in text object at 0x%x\n",
		pn, rn, text_onp1);
	sleep(2);
    }

    /*
     * Before leaving, we clean up our windows.
     */
    WOP_CLEANUP(&gator_basegwin);

}				/*test_objects */

/*------------------------------------------------------------------------
 * object_testInit
 *
 * Description:
 *	Routine that is called when object_test is invoked, responsible
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
 *------------------------------------------------------------------------*/

static int
object_testInit(struct cmd_syndesc *as, void *arock)
{				/*object_testInit */

    static char rn[] = "object_testInit";	/*Routine name */
    int wpkg_to_use;		/*Window package to use */

    if (as->parms[P_DEBUG].items != 0)
	object_debug = 1;
    wpkg_to_use = atoi(as->parms[P_PACKAGE].items->data);
    fprintf(stderr, "[%s:%s] Using graphics package %d: ", pn, rn,
	    wpkg_to_use);
    switch (wpkg_to_use) {
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
	fprintf(stderr, "Illegal graphics package: %d\n", wpkg_to_use);
	exit(-1);
    }				/*end switch (wpkg_to_use) */

    /*
     * Now, drive the sucker.
     */
    test_objects(wpkg_to_use);

    /*
     * We initialized (and ran) correctly, so return the good news.
     */
    return (0);

}				/*object_testInit */

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;

{				/*main */

    static char rn[] = "main";	/*Routine name */
    register afs_int32 code;	/*Return code */
    register struct cmd_syndesc *ts;	/*Ptr to cmd line syntax descriptor */

    /*
     * Set up the commands we understand.
     */
    ts = cmd_CreateSyntax("initcmd", object_testInit, NULL,
			  "Initialize the program");
    cmd_AddParm(ts, "-package", CMD_SINGLE, CMD_REQUIRED,
		"Graphics package to use");
    cmd_AddParm(ts, "-debug", CMD_FLAG, CMD_OPTIONAL, "Turn debugging on");

    /*
     * Parse command-line switches & execute the test, then get the heck
     * out of here.
     */
    code = cmd_Dispatch(argc, argv);
    if (code) {
	fprintf(stderr, "[%s:%s] Call to cmd_Dispatch() failed; code is %d\n",
		pn, rn, code);
	exit(1);
    }

}				/*main */
