/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

#ifndef __gator_textobject_h
#define	__gator_textobject_h  1

#include "gtxobjects.h"	/*Basic gator object definitions*/
#include "gtxtextcb.h"	/*Text object's circular buffer facility*/

/*Value for onode o_type field*/
#define	GATOR_OBJ_TEXT	0

/*Scroll directions*/
#define GATOR_TEXT_SCROLL_DOWN	0
#define GATOR_TEXT_SCROLL_UP	1

/*Private data for text onode*/
struct gator_textobj {
    int	*llrock;			/*Rock for lower-level graphics layer*/
    int numLines;			/*Num lines we can display*/
    struct gator_textcb_hdr *cbHdr;	/*Ptr to circular buffer header*/
    int	firstEntShown;			/*ID of first text entry displayed*/
    int	lastEntShown;			/*ID of last text entry displayed*/
};

/*Text object's creation parameters*/
struct gator_textobj_params {
    struct onode_createparams onode_params;	/*Params for the whole onode*/
    int	maxEntries;				/*Max text entries to store*/
    int maxCharsPerEntry;			/*Max chars per text entry*/
};

/*Text object's creation routine*/

extern int gator_text_create();
    /*
     * Summary:
     *    Create a gator text object.
     *
     * Args:
     *	  struct onode *text_onp : Ptr to the text onode to fill out.
     *	  struct onode_createparams *params : Ptr to creation params.
     *       (Note: this actually points to a gator_text_crparams
     *        structure, but we use the generic version of the ptr)
     *
     * Returns:
     *	  Zero if successful,
     *	  Error value otherwise.
     */

/*Text object's generic onode routines*/

extern int gator_text_destroy();
    /*
     * Summary:
     *    Destroy a gator text object.
     *
     * Args:
     *	  struct onode *onp : Ptr to the text onode to delete.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_text_display();
    /*
     * Summary:
     *    Display/redraw a gator text object.
     *
     * Args:
     *	  struct onode *onp: Ptr to the text onode to display.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_text_release();
    /*
     * Summary:
     *    Drop the refcount on a gator text object.
     *
     * Args:
     *	  struct onode *onp : Ptr to the onode whose refcount is
     *	                               to be dropped.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

/*
 * Additional, text-specific operations.
 */

extern int gator_text_Scroll();
    /*
     * Summary:
     *    Scroll a text object some number of lines.
     *
     * Args:
     *	  struct onode *onp	: Ptr to the text onode to be scrolled.
     *	  int nlines		: Number of lines to scroll.
     *	  int down		: Scroll down?
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_text_Write();
    /*
     * Summary:
     *    Write the given string to the end of the gator text object.
     *
     * Args:
     *	  struct onode *onp	: Ptr to the text onode to which we're
     *					writing.
     *	  char *strToWrite	: String to write.
     *	  int numChars		: Number of chars to write.
     *	  int highlight		: Use highlighting?
     *	  int skip		: Force a skip to the next line?
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_text_BlankLine();
    /*
     * Summary:
     *    Write a given number of blank lines to the given text object.
     *
     * Args:
     *	  struct onode *onp : Ptr to the onode to which we're writing.
     *	  int numBlanks	    : Number of blank lines to write.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

/*
 * Set of exported generic text onode operations.
 */
extern struct onodeops gator_text_ops;

#endif /* __gator_textobject_h */
