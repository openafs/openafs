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
    struct keymap_map *keymap;		/*Map for handling keystrokes */
    struct keymap_map *savemap;		/* Map saved during recursive edit */
    struct keymap_state *keystate;	/*Associated key state */
    struct gtxframe_dlist *display;	/*Display list */
    struct gtxframe_menu *menus;	/* Menu list */
    char *messageLine;			/* message line */
    char *promptLine;			/* prompt for a response */
    char *defaultLine;			/* default response */
    struct gwin *window;		/* window we're currently showing on */
    int flags;
};

#define GTXFRAME_NEWDISPLAY	1	/* just did a display string this command */
#define GTXFRAME_RECURSIVEEND	2	/* end recursive edit */
#define GTXFRAME_RECURSIVEERR	4	/* recursive edit failed due to error */

extern struct gtx_frame *gtxframe_Create();
extern struct gtx_frame *gtxframe_GetFrame();

/*
  * The gtxframe_ExitCmd() routine, normally bound to ^C, allows a caller
  * to cleanly exit from its gtx environment, returning the terminal or
  * window to its normal state.  If a non-zero exit value is desired, then
  * the caller should place it in gtxframe_exitValue.
  */
extern int gtxframe_ExitCmd();
extern int gtxframe_exitValue;

#endif /* __GTX_FRAME_INCL_ */
