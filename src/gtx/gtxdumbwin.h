/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

#ifndef __gator_dumbwindows_h
#define	__gator_dumbwindows_h  1

#include "gtxwindows.h"		/*Base gator window dfns*/

/*Value for gwin w_type field*/
#define	GATOR_WIN_DUMB    1

/*Private data for a dumb gwin*/
#if 0
struct gator_dumbgwin {
    WINDOW *wp;		/*Window pointer*/
    int	charwidth;	/*Character width in pixels*/
    int	charheight;	/*Character height in pixels*/
    char box_vertchar;	/*Vertical char for boxing purposes*/
    char box_horizchar;	/*Horizontal char for boxing purposes*/
};
#endif /* 0 */

/*Dumb gwin's creation parameters*/
struct gator_dumbgwin_params {
    struct gwin_createparams gwin_params;   /*Basic params for the window*/
    char box_vertchar;			    /*Vertical char for boxing purposes*/
    char box_horizchar;			    /*Horizontal char for boxing purposes*/
};

/*Dumb windows initialization routine*/

extern int gator_dumbgwin_init();
    /*
     * Summary:
     *    Initialize the dumb window package.
     *
     * Args:
     *	  int adebug: Is debugging turned on?
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

/*Dumb window's creation routine*/

extern struct gwin *gator_dumbgwin_create();
    /*
     * Summary:
     *    Create a dumb window.
     *
     * Args:
     *	  struct gator_dumbgwin_params *params : Ptr to creation parameters.
     *
     * Returns:
     *	  Ptr to the created dumb window structure if successful,
     *	  Error value otherwise.
     */

/*Dumb cleanup routine*/

extern int gator_dumbgwin_cleanup();
    /*
     * Summary:
     *    Clean up after the dumb window package.
     *
     * Args:
     *	  struct gwin *gwp : Ptr to base window.
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

extern struct gwinbaseops gator_dumb_gwinbops;

/*Dumb window's routines*/

extern int gator_dumbgwin_box();
    /*
     * Summary:
     *    Draw a box around the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window to draw
     *	                            a box around.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_clear();
    /*
     * Summary:
     *    Clear out the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window to clear
     *	                            out.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_destroy();
    /*
     * Summary:
     *    Destroy the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window to destroy.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_display();
    /*
     * Summary:
     *    Display/redraw the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window to draw.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_drawline();
    /*
     * Summary:
     *    Draw a line between two points in the given dumb
     *    window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window in which
     *	                            the line is to be drawn.
     *	 struct gwin_lineparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_drawrectangle();
    /*
     * Summary:
     *    Draw a rectangle in the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window in which
     *	                            the rectangle is to be drawn.
     *	 struct gwin_rectparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_drawchar();
    /*
     * Summary:
     *    Draw a character in the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window in which
     *	                            the character is to be drawn.
     *	 struct gwin_charparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_drawstring();
    /*
     * Summary:
     *    Draw a string in the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window in which
     *	                            the string is to be drawn.
     *	 struct gwin_strparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_invert();
    /*
     * Summary:
     *    Invert a region in the given dumb window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the dumb window in which
     *	                            the inverted region lies.
     *	 struct gwin_invparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_dumbgwin_getchar();
    /* get a character from a window */

extern int gator_dumbgwin_getdimensions();

extern int gator_dumbgwin_wait();

#endif /* __gator_dumbwindows_h */
