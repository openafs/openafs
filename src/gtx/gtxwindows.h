/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

#ifndef __gator_windows_h
#define	__gator_windows_h  1

/*--------------------------------------------------------------------------------
 * gator_windows.h
 *
 * Constants and data structures defining the basis for gator windows,
 * independent of the lower-level graphical environment.
 *--------------------------------------------------------------------------------*/

/*
 * Gator window definition.
 */
struct gwin {
    int	w_type;			/*Type of window*/
    int	w_x, w_y;		/*X and Y coordinates*/
    int	w_width, w_height;	/*Width & height in pixels*/
    int	w_changed;		/*Does the window need to be refreshed?*/
    struct gwinops *w_op;	/*Ptr to the operations defined on the window*/
    struct gwin *w_parent;	/*Parent window, if any*/
    struct gtx_frame *w_frame;	/*Frame information */
    int	*w_data;		/*Ptr to info describing the window*/
};

/*
 * window display list item.
 */
struct gwin_dlist {
    struct gwin_dlist *next;	/* "car" */
    struct onode *data;		/* "cdr" */
};

/*
 * Initialization parameters for the gator window package.
 */
struct gwin_initparams {
    int	i_type;			/*Type of lower-level graphics package used*/
    int	i_x, i_y;		/*X, Y coordinates of the screen area*/
    int	i_width, i_height;	/*Width, height of the screen area*/
    int	i_debug;		/*Should debugging be turned on?*/
};

/*
 * Creation parameters for gator windows.
 */
struct gwin_createparams {
    int	cr_type;		/*Type of window*/
    int	cr_x, cr_y;		/*X and Y coordinates*/
    int	cr_width, cr_height;	/*Width & height in pixels*/
    struct gwin	*cr_parentwin;	/*Ptr to parent window structure*/
};

/*
 * Line-drawing parameters.
 */
struct gwin_lineparams {
    int	x1, y1;	    /*X, Y coordinates of first point*/
    int	x2, y2;	    /*X, Y coordinates of second point*/
};

/*
 * Rectangle-drawing parameters.
 */
struct gwin_rectparams {
    int	x, y;		/*X, Y coordinates of rectangle's origin*/
    int	width, height;	/*Rectangle width & height*/
};

/*
 * Size params.
 */
struct gwin_sizeparams {
    int maxx, maxy;	/* x,y size */
};

/*
 * Char-drawing parameters.
 */
struct gwin_charparams {
    int	x, y;		/*X, Y coordinates of char origin*/
    char c;		/*Char to draw*/
    int	highlight;	/*Print in highlight/standout mode?*/
};

/*
 * String-drawing parameters.
 */
struct gwin_strparams {
    int	x, y;		/*X, Y coordinates of string*/
    int	highlight;	/*Print in highlight/standout mode?*/
    char *s;		/*String to draw*/
};

/*
 * Inversion parameters.
 */
struct gwin_invparams {
    int	x, y;		/*X, Y coordinates of origin*/
    int	width, height;	/*Width & height of rectangle to invert*/
};

/*
 * Operations on gator windows.
 */
struct gwinops {
    int	(*gw_box)();		/* Draw a box around the given window*/
    int	(*gw_clear)();		/* Clear out a window*/
    int	(*gw_destroy)();	/* Destroy a window*/
    int	(*gw_display)();	/* [Re]display a window*/
    int	(*gw_drawline)();	/* Draw a line between two points*/
    int	(*gw_drawrectangle)();	/* Draw a rectangle at the given loc & dimensions*/
    int	(*gw_drawchar)();	/* Draw a char at the given location*/
    int	(*gw_drawstring)();	/* Draw a char string at the given location*/
    int	(*gw_invert)();		/* Invert region*/
    int (*gw_getchar)();	/* Get a character from a window */
    int (*gw_getdimensions)();	/* Get dimensions of a window */
    int (*gw_wait)();		/* Wait for input */
};

/*
 * Macros facilitating the use of the window functions.
 */
#define	WOP_BOX(WNP)			(WNP)->w_op->gw_box(WNP)
#define	WOP_CLEAR(WNP)			(WNP)->w_op->gw_clear(WNP)
#define	WOP_DESTROY(WNP)		(WNP)->w_op->gw_destroy(WNP)
#define	WOP_DISPLAY(WNP)		(WNP)->w_op->gw_display(WNP)
#define	WOP_DRAWLINE(WNP, params)	(WNP)->w_op->gw_drawline(WNP, params)
#define	WOP_DRAWRECTANGLE(WNP, params)  (WNP)->w_op->gw_drawrectangle(WNP, params)
#define	WOP_DRAWCHAR(WNP, params)	(WNP)->w_op->gw_drawchar(WNP, params)
#define	WOP_DRAWSTRING(WNP, params)	(WNP)->w_op->gw_drawstring(WNP, params)
#define	WOP_INVERT(WNP, params)		(WNP)->w_op->gw_invert(WNP, params)
#define WOP_GETCHAR(WNP)		(WNP)->w_op->gw_getchar(WNP)
#define WOP_GETDIMENSIONS(WNP,params)	(WNP)->w_op->gw_getdimensions(WNP, params)
#define WOP_WAIT(WNP)			(WNP)->w_op->gw_wait(WNP)

/*
 * Base operations on the lower-level window system.
 */
struct gwinbaseops {
    struct gwin	*(*gw_createwin)(); /*Create a window*/
    int	(*gw_cleanup)();	    /*Clean up before program exit*/
};

/*
 * Ptr to the base operation array.
 */
extern struct gwinbaseops gwinbops;

/*
 * Macros facilitating the use of the base window operations.
 */
#define	WOP_CREATE(p)	((*(gwinbops.gw_createwin))(p))
#define	WOP_CLEANUP(w)	((*(gwinbops.gw_cleanup))(w))

/*
 * Pointer to the base window, which is created by the initialization routine.
 */
extern struct gwin gator_basegwin;

extern int gw_init();
    /*
     * Summary:
     *    Initialize the gator window package.
     *
     * Args:
     *	  struct gwin_initparams *params: Ptr to initialization params.
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

/* initialize the whole gator toolkit package */
extern struct gwin *gtx_Init();

#endif /* __gator_windows_h */
