/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

#ifndef __gator_lightobject_h
#define	__gator_lightobject_h  1

#include "gtxobjects.h"		/*Basic gator object definitions*/

/*Value for onode o_type field*/
#define	GATOR_OBJ_LIGHT	1

/*Light masks*/
#define	GATOR_LIGHTMASK_OUTLINE	   0x1	    /*Outline the light?*/
#define	GATOR_LIGHTMASK_INVVIDEO   0x2	    /*Show light in inverse video?*/
#define	GATOR_LIGHTMASK_FLASH	   0x4	    /*Flash light when turned on?*/
#define	GATOR_LIGHTMASK_FLASHCYCLE 0x8	    /*Current flash cycle*/

#define GATOR_LABEL_CHARS 128		    /*Max chars in light label*/

/*Private data for light onode*/
struct gator_lightobj {
    int	*llrock;			/*Rock for lower-level graphics layer*/
    int setting;                        /*Is light on or off*/
    int	appearance;			/*Bit array describing light's appearance*/
    int	flashfreq;			/*Flashing frequency in msecs*/
    int	lasttoggletime;			/*Last time ``flashed''*/
    char label[GATOR_LABEL_CHARS];      /*Light label*/
};

/*Light object's creation parameters*/
struct gator_light_crparams {
    struct onode_createparams onode_params; /*Creation params for the whole onode*/
    int	appearance;			    /*General appearance*/
    int	flashfreq;			    /*Flash frequency in msecs, if any*/
    char label[GATOR_LABEL_CHARS];          /*Light label*/
    int label_x, label_y;                   /*X,Y offsets for label within light*/
};

/*Light object's creation routine*/

extern int gator_light_create();
    /*
     * Summary:
     *    Create a gator light object.
     *
     * Args:
     *	  struct onode *light_onp : Ptr to the light onode to fill out.
     *	  struct onode_createparams *params : Ptr to creation params.
     *       (Note: this actually points to a gator_light_crparams
     *        structure, but we use the generic version of the ptr)
     *
     * Returns:
     *	  Zero if successful,
     *	  Error value otherwise.
     */

/*Light object's generic onode routines*/

extern int gator_light_destroy();
    /*
     * Summary:
     *    Destroy a gator light object.
     *
     * Args:
     *	 struct onode *onp : Ptr to the light onode to delete.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_light_display();
    /*
     * Summary:
     *    Display/redraw a gator light object.
     *
     * Args:
     *	  struct onode *onp: Ptr to the light onode to display.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

extern int gator_light_release();
    /*
     * Summary:
     *    Drop the refcount on a gator light object.
     *
     * Args:
     *	  struct onode *onp : Ptr to the onode whose refcount is
     *                                   to be dropped.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

/*
 * Additional, light-specific operations.
 */

extern int gator_light_set();
    /*
     * Summary:
     *    Set the value of the given gator light object.
     *
     * Args:
     *	  struct onode *onp : Ptr to the light onode to be set.
     *	  int setting       : Non-zero for ``on'', zero for ``off''.
     *
     * Returns:
     *	  0: Success.
     *	  Error value otherwise.
     */

/*
 * Set of exported generic light onode operations.
 */
extern struct onodeops gator_light_ops;

#endif /* __gator_lightobject_h */
