/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

#ifndef __gator_X11windows_h
#define	__gator_X11windows_h  1

#include "gtxwindows.h"	/*Base gator window dfns */

/*Value for gwin w_type field*/
#define	GATOR_WIN_X11    3

/*Private data for a X11 gwin*/
#if 0
struct gator_X11gwin {
    WINDOW *wp;		/*Window pointer*/
    int	charwidth;	/*Character width in pixels*/
    int	charheight;	/*Character height in pixels*/
    char box_vertchar;	/*Vertical char for boxing purposes*/
    char box_horizchar;	/*Horizontal char for boxing purposes*/
};
#endif /* 0 */

/*X11 gwin's creation parameters*/
struct gator_X11gwin_params {
    struct gwin_createparams gwin_params;   /*Basic params for the window*/
    char box_vertchar;			    /*Vertical char for boxing purposes*/
    char box_horizchar;			    /*Horizontal char for boxing purposes*/
};

/*X11 initialization routine*/

extern int gator_X11gwin_init();
    /*
     * Summary:
     *    Initialize the X11 window package.
     *
     * Args:
     *	  int adebug: Is debugging turned on?
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

/*X11 window's creation routine*/

extern struct gwin *gator_X11gwin_create();
    /*
     * Summary:
     *    Create a X11 window.
     *
     * Args:
     *	  struct gator_X11gwin_params *params : Ptr to creation parameters.
     *
     * Returns:
     *	  Ptr to the created X11 window structure if successful,
     *	  Error value otherwise.
     */

/*X11 cleanup routine*/

extern int gator_X11gwin_cleanup();
    /*
     * Summary:
     *    Clean up after the X11 window package.
     *
     * Args:
     *	  struct gwin *gwp : Ptr to base window.
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

extern struct gwinbaseops gator_X11_gwinbops;

/*X11 window's routines*/

extern int gator_X11gwin_box();
    /*
     * Summary:
     *    Draw a box around the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window to draw
     *	                            a box around.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_clear();
    /*
     * Summary:
     *    Clear out the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window to clear
     *	                            out.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_destroy();
    /*
     * Summary:
     *    Destroy the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window to destroy.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_display();
    /*
     * Summary:
     *    Display/redraw the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window to draw.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_drawline();
    /*
     * Summary:
     *    Draw a line between two points in the given X11
     *    window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window in which
     *	                            the line is to be drawn.
     *	 struct gwin_lineparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_drawrectangle();
    /*
     * Summary:
     *    Draw a rectangle in the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window in which
     *	                            the rectangle is to be drawn.
     *	 struct gwin_rectparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_drawchar();
    /*
     * Summary:
     *    Draw a character in the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window in which
     *	                            the character is to be drawn.
     *	 struct gwin_charparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_drawstring();
    /*
     * Summary:
     *    Draw a string in the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window in which
     *	                            the string is to be drawn.
     *	 struct gwin_strparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_invert();
    /*
     * Summary:
     *    Invert a region in the given X11 window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the X11 window in which
     *	                            the inverted region lies.
     *	 struct gwin_invparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_X11gwin_getchar();

extern int gator_X11gwin_getdimensions();

extern int gator_X11gwin_wait();

#endif /* __gator_X11windows_h */
