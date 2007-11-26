/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <string.h>

#include "gtxwindows.h"
#include "gtxobjects.h"
#include "gtxtextobj.h"
#include "gtxlightobj.h"
#include "gtxkeymap.h"
#include "gtxframe.h"

struct gtx_frame *frameA, *frameB;

struct onode *objectA, *objectB;
struct onode *lightA, *lightB;

static int
ChangeMenuCmd(awin, arock)
     afs_int32 arock;
     register struct gwin *awin;
{
    register struct gtx_frame *tf;
    register afs_int32 code;

    tf = awin->w_frame;
    if (!tf)
	return -1;

    if (arock == 1) {
	gtxframe_ClearMenus(tf);
    } else if (arock == 2) {
	code = gtxframe_DeleteMenu(tf, "NewLabel");
	if (code)
	    gtxframe_DisplayString(tf, "Can't delete menu!");
    } else if (arock == 3) {
	code = gtxframe_ClearMenus(tf);
	gtxframe_AddMenu(frameA, "FrameB", "b");
	gtxframe_AddMenu(frameA, "NewLabel", "c");
    }
    return 0;
}

static int
ChangeListCmd(awin, arock)
     afs_int32 arock;
     register struct gwin *awin;
{
    register struct gtx_frame *tf;
    register afs_int32 code;

    tf = awin->w_frame;
    if (!tf)
	return -1;

    if (arock == 1) {
	gtxframe_ClearList(tf);
    } else if (arock == 2) {
	code = gtxframe_RemoveFromList(tf, lightA);
	if (code)
	    gtxframe_DisplayString(tf, "Can't delete light!");
    } else if (arock == 3) {
	code = gtxframe_ClearList(tf);
	gtxframe_AddToList(frameA, objectA);
	gtxframe_AddToList(frameA, lightA);
    }
    return 0;
}

static int
NoCallCmd(awin)
     struct gwin *awin;
{
    gtxframe_DisplayString(awin->w_frame,
			   "Function should be mapped on '$d', not 'd'");
    return 0;
}

static int
ChangeCmd(awin)
     struct gwin *awin;
{
    char tbuffer[100];
    register afs_int32 code;

    code =
	gtxframe_AskForString(awin->w_frame, "New object string: ", "TestNew",
			      tbuffer, sizeof(tbuffer));
    if (code == 0) {
	/* have new value, write it to object A */
	gator_text_Write(objectA, tbuffer, 0, 0, 0);
    }
    return 0;
}

static int
StupidCmd(awin)
     struct gwin *awin;
{
    gtxframe_DisplayString(awin->w_frame,
			   "You're already showing that frame!");
    return 0;
}

static int
SwitchToACmd(awin)
     struct gwin *awin;
{
    gtxframe_SetFrame(awin, frameA);
    return 0;
}

static int
SwitchToBCmd(awin)
     struct gwin *awin;
{
    gtxframe_SetFrame(awin, frameB);
    return 0;
}

#include "AFS_component_version_number.c"

main()
{
    struct gwin *win;
    struct gator_textobj_params textcrparams;
    struct gator_light_crparams lightcrparams;
    register struct keymap_map *tmap;

    win = gtx_Init(0, -1);

    /* create a couple of objects, a and b, and have the "a" and "b" keys
     * switch the display from one to the other */
    textcrparams.onode_params.cr_type = GATOR_OBJ_TEXT;
    strcpy(textcrparams.onode_params.cr_name, "Text1-A");
    textcrparams.onode_params.cr_x = 30;
    textcrparams.onode_params.cr_y = 10;
    textcrparams.onode_params.cr_width = 35;
    textcrparams.onode_params.cr_height = 7;
    textcrparams.onode_params.cr_window = win;	/* ???? */
    textcrparams.onode_params.cr_home_obj = NULL;
    textcrparams.onode_params.cr_prev_obj = NULL;
    textcrparams.onode_params.cr_parent_obj = NULL;
    textcrparams.onode_params.cr_helpstring = "Help string for text";
    textcrparams.maxEntries = 7;
    textcrparams.maxCharsPerEntry = 35;

    objectA =
	gator_objects_create((struct onode_createparams *)(&textcrparams));
    gator_text_Write(objectA, "This is object A", 0, 0, 0);

    /* create a couple of objects, a and b, and have the "a" and "b" keys
     * switch the display from one to the other */
    textcrparams.onode_params.cr_type = GATOR_OBJ_TEXT;
    strcpy(textcrparams.onode_params.cr_name, "Text2-B");
    textcrparams.onode_params.cr_x = 30;
    textcrparams.onode_params.cr_y = 10;
    textcrparams.onode_params.cr_width = 35;
    textcrparams.onode_params.cr_height = 7;
    textcrparams.onode_params.cr_window = win;	/* ???? */
    textcrparams.onode_params.cr_home_obj = NULL;
    textcrparams.onode_params.cr_prev_obj = NULL;
    textcrparams.onode_params.cr_parent_obj = NULL;
    textcrparams.onode_params.cr_helpstring = "Help string for text";
    textcrparams.maxEntries = 7;
    textcrparams.maxCharsPerEntry = 35;

    objectB =
	gator_objects_create((struct onode_createparams *)(&textcrparams));
    gator_text_Write(objectB, "This is object B", 0, 0, 0);

    lightcrparams.onode_params.cr_type = GATOR_OBJ_LIGHT;
    lightcrparams.onode_params.cr_x = 10;
    lightcrparams.onode_params.cr_y = 10;
    lightcrparams.onode_params.cr_width = 10;
    lightcrparams.onode_params.cr_height = 10;
    lightcrparams.onode_params.cr_window = win;	/* ???? */
    lightcrparams.onode_params.cr_home_obj = NULL;
    lightcrparams.onode_params.cr_prev_obj = NULL;
    lightcrparams.onode_params.cr_parent_obj = NULL;
    lightcrparams.onode_params.cr_helpstring = "Help string for text";
    strcpy(lightcrparams.label, "Light-1");
    lightcrparams.label_x = 0;
    lightcrparams.label_y = 0;
    lightcrparams.flashfreq = 100;
    lightcrparams.appearance = GATOR_LIGHTMASK_INVVIDEO;
    lightA =
	gator_objects_create((struct onode_createparams *)(&lightcrparams));

    /* create basic frames */
    frameA = gtxframe_Create();
    frameB = gtxframe_Create();

    /* setup A's frame */
    gtxframe_ClearList(frameA);
    gtxframe_AddToList(frameA, objectA);
    gtxframe_AddToList(frameA, lightA);
    keymap_BindToString(frameA->keymap, "b", SwitchToBCmd, NULL, NULL);
    keymap_BindToString(frameA->keymap, "a", StupidCmd, NULL, NULL);
    keymap_BindToString(frameA->keymap, "c", ChangeCmd, NULL, NULL);
    keymap_BindToString(frameA->keymap, "\033a", ChangeMenuCmd, "ChangeMenu",
			(char *)1);
    keymap_BindToString(frameA->keymap, "\033b", ChangeMenuCmd, "ChangeMenu",
			(char *)2);
    keymap_BindToString(frameA->keymap, "\033c", ChangeMenuCmd, "ChangeMenu",
			(char *)3);
    keymap_BindToString(frameA->keymap, "\0331", ChangeListCmd, "ChangeList",
			(char *)1);
    keymap_BindToString(frameA->keymap, "\0332", ChangeListCmd, "ChangeList",
			(char *)2);
    keymap_BindToString(frameA->keymap, "\0333", ChangeListCmd, "ChangeList",
			(char *)3);
    gtxframe_AddMenu(frameA, "FrameB", "b");
    gtxframe_AddMenu(frameA, "NewLabel", "c");

    /* setup B's frame */
    gtxframe_ClearList(frameB);
    gtxframe_AddToList(frameB, objectB);
    keymap_BindToString(frameB->keymap, "a", SwitchToACmd, NULL, NULL);
    keymap_BindToString(frameB->keymap, "b", StupidCmd, NULL, NULL);
    keymap_BindToString(frameB->keymap, "d", NoCallCmd, NULL, NULL);
    keymap_BindToString(frameB->keymap, "d", (int (*)())0, NULL, NULL);
    keymap_BindToString(frameB->keymap, "\033d", NoCallCmd, NULL, NULL);
    gtxframe_AddMenu(frameB, "FrameA", "a");

    /* finally setup the first window */
    gtxframe_AddToList(frameA, objectA);
    gtxframe_SetFrame(win, frameA);

    /* play with maps for a while */
    tmap = (struct keymap_map *)keymap_Create();
    keymap_BindToString(tmap, "d", NoCallCmd, "test", (char *)1);
    keymap_BindToString(tmap, "cd", NoCallCmd, "bozo", NULL);
    keymap_Delete(tmap);

    gtx_InputServer(win);
    return 0;
}
