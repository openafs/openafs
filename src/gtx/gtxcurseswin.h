/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

#ifndef __gator_curseswindows_h
#define	__gator_curseswindows_h  1

#include "gtxwindows.h"  /*Base gator window dfns*/
#if defined(AFS_HPUX110_ENV) && !defined(__HP_CURSES)
#define __HP_CURSES
#endif
#ifdef AFS_LINUX20_ENV
#include <ncurses.h>	    /*Curses library*/
#else
#include <curses.h>	    /*Curses library*/
#endif

/*Value for gwin w_type field*/
#define	GATOR_WIN_CURSES    2

/*Private data for a curses gwin*/
struct gator_cursesgwin {
    WINDOW *wp;		/*Window pointer*/
    int	charwidth;	/*Character width in pixels*/
    int	charheight;	/*Character height in pixels*/
    char box_vertchar;	/*Vertical char for boxing purposes*/
    char box_horizchar;	/*Horizontal char for boxing purposes*/
};

/*Curses gwin's creation parameters*/
struct gator_cursesgwin_params {
    struct gwin_createparams gwin_params;   /*Basic params for the window*/
    int	charwidth;			    /*Character width in pixels*/
    int	charheight;			    /*Character height in pixels*/
    char box_vertchar;			    /*Vertical char for boxing purposes*/
    char box_horizchar;			    /*Horizontal char for boxing purposes*/
};

/*Curses initialization routine*/

extern int gator_cursesgwin_init();
    /*
     * Summary:
     *    Initialize the curses window package.
     *
     * Args:
     *	  int adebug: Is debugging turned on?
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

/*Curses window's creation routine*/

extern struct gwin *gator_cursesgwin_create();
    /*
     * Summary:
     *    Create a curses window.
     *
     * Args:
     *	  struct gator_cursesgwin_params *params : Ptr to creation parameters.
     *
     * Returns:
     *	  Ptr to the created curses window structure if successful,
     *	  Error value otherwise.
     */

/*Curses cleanup routine*/

extern int gator_cursesgwin_cleanup();
    /*
     * Summary:
     *    Clean up after the curses window package.
     *
     * Args:
     *	  struct gwin *gwp : Ptr to base window.
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

extern struct gwinbaseops gator_curses_gwinbops;

/*Curses window's routines*/

extern int gator_cursesgwin_box();
    /*
     * Summary:
     *    Draw a box around the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window to draw
     *	                            a box around.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_clear();
    /*
     * Summary:
     *    Clear out the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window to clear
     *	                            out.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_destroy();
    /*
     * Summary:
     *    Destroy the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window to destroy.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_display();
    /*
     * Summary:
     *    Display/redraw the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window to draw.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_drawline();
    /*
     * Summary:
     *    Draw a line between two points in the given curses
     *    window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window in which
     *	                            the line is to be drawn.
     *	 struct gwin_lineparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_drawrectangle();
    /*
     * Summary:
     *    Draw a rectangle in the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window in which
     *	                            the rectangle is to be drawn.
     *	 struct gwin_rectparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_drawchar();
    /*
     * Summary:
     *    Draw a character in the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window in which
     *	                            the character is to be drawn.
     *	 struct gwin_charparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_drawstring();
    /*
     * Summary:
     *    Draw a string in the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window in which
     *	                            the string is to be drawn.
     *	 struct gwin_strparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_invert();
    /*
     * Summary:
     *    Invert a region in the given curses window.
     *
     * Args:
     *	 struct gwin *gwp : Ptr to the curses window in which
     *	                            the inverted region lies.
     *	 struct gwin_invparams *params : Ptr to other params.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_cursesgwin_getchar();

extern int gator_cursesgwin_getdimensions();

extern int gator_cursesgwin_wait();

#endif /* __gator_curseswindows_h */
