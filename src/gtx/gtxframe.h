/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __GTX_FRAME_INCL_
#define __GTX_FRAME_INCL_ 1
struct gtxframe_dlist {
    struct gtxframe_dlist *next;
    char *data;
};

struct gtxframe_menu {
    struct gtxframe_menu *next;
    char *name;
    char *cmdString;
};

struct gtx_frame {
    struct keymap_map *keymap;	/*Map for handling keystrokes */
    struct keymap_map *savemap;	/* Map saved during recursive edit */
    struct keymap_state *keystate;	/*Associated key state */
    struct gtxframe_dlist *display;	/*Display list */
    struct gtxframe_menu *menus;	/* Menu list */
    char *messageLine;		/* message line */
    char *promptLine;		/* prompt for a response */
    char *defaultLine;		/* default response */
    struct gwin *window;	/* window we're currently showing on */
    int flags;
};

#define GTXFRAME_NEWDISPLAY	1	/* just did a display string this command */
#define GTXFRAME_RECURSIVEEND	2	/* end recursive edit */
#define GTXFRAME_RECURSIVEERR	4	/* recursive edit failed due to error */

extern int gtxframe_SetFrame(struct gwin *awin, struct gtx_frame *aframe);
extern struct gtx_frame *gtxframe_GetFrame(struct gwin *);
extern int gtxframe_AddMenu(struct gtx_frame *aframe, char *alabel,
			    char *astring);
extern int gtxframe_DeleteMenu(struct gtx_frame *aframe, char *alabel);
extern int gtxframe_ClearMenus(struct gtx_frame *aframe);
extern int gtxframe_AskForString(struct gtx_frame *aframe, char *aprompt,
				 char *adefault, char *aresult,
				 int aresultSize);
extern int gtxframe_DisplayString(struct gtx_frame *, char *);
extern int gtxframe_ClearMessageLine(struct gtx_frame *aframe);
extern struct gtx_frame *gtxframe_Create(void);
extern int gtxframe_Delete(struct gtx_frame *aframe);
extern int gtxframe_Display(struct gtx_frame *aframe, struct gwin *awm);
extern int gtxframe_AddToList(struct gtx_frame *aframe, struct onode *aobj);
extern int gtxframe_RemoveFromList(struct gtx_frame *aframe,
				   struct onode *aobj);
extern int gtxframe_ClearList(struct gtx_frame *aframe);

/*
  * The gtxframe_ExitCmd() routine, normally bound to ^C, allows a caller
  * to cleanly exit from its gtx environment, returning the terminal or
  * window to its normal state.  If a non-zero exit value is desired, then
  * the caller should place it in gtxframe_exitValue.
  */
extern int gtxframe_ExitCmd(void *, void *);
extern int gtxframe_exitValue;

#endif /* __GTX_FRAME_INCL_ */
