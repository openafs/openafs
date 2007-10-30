/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Description:
 *	Implementation of the gator light object.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "gtxlightobj.h"	/*Interface for this module */
#include <stdio.h>		/*Standard I/O stuff */
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/*Externally-advertised array of light onode operations*/
struct onodeops gator_light_ops = {
    gator_light_destroy,
    gator_light_display,
    gator_light_release
};

static char mn[] = "gator_lightobject";	/*Module name */

/*------------------------------------------------------------------------
 * gator_light_create
 *
 * Description:
 *	Create a gator light object.
 *
 * Arguments:
 *      struct onode *light_onp : Ptr to the light onode to fill out.
 *	struct onode_createparams *params : Generic ptr to creation
 *	    parameters.
 *
 * Returns:
 *	Zero if successful,
 *	Error value otherwise.
 *
 * Environment:
 *	The base onode fields have already been set.  Lights are turned
 *	off at creation.
 *
 * Side Effects:
 *	Creates and initializes the light private data area, including
 *	a window string-drawing parameter structure.  These areas are
 *	garbage-collected upon failure.
 *------------------------------------------------------------------------*/

int
gator_light_create(light_onp, params)
     struct onode *light_onp;
     struct onode_createparams *params;

{				/*gator_light_create */

    static char rn[] = "gator_light_create";	/*Routine name */
    struct gator_light_crparams *light_params;	/*My specific creation params */
    struct gator_lightobj *light_data;	/*Ptr to private data */
    struct gwin_strparams *light_strparams;	/*Light label params */

    light_params = (struct gator_light_crparams *)params;
    if (objects_debug) {
	fprintf(stderr, "[%s:%s] Private data passed to light object:\n", mn,
		rn);
	fprintf(stderr,
		"\tAppearance: %d, flashfreq: %d, label at offset (%d, %d): '%s'\n",
		light_params->appearance, light_params->flashfreq,
		light_params->label_x, light_params->label_y,
		light_params->label);
    }

    /*
     * Allocate the private data area, including the lower-level
     * structure, then fill it in.
     */
    light_data =
	(struct gator_lightobj *)malloc(sizeof(struct gator_lightobj));
    if (light_data == (struct gator_lightobj *)0) {
	fprintf(stderr,
		"[%s:%s] Can't allocate %d bytes for light object private data region, errno is %d\n",
		mn, rn, sizeof(struct gator_lightobj), errno);
	return (errno);
    }

    light_strparams =
	(struct gwin_strparams *)malloc(sizeof(struct gwin_strparams));
    if (light_strparams == (struct gwin_strparams *)0) {
	fprintf(stderr,
		"[%s:%s] Can't allocate %d bytes for light object label in private data region, errno is %d\n",
		mn, rn, sizeof(struct gwin_strparams), errno);
	free(light_data);
	return (errno);
    }

    /*
     * Now that we have the private structures allocated, set them up.
     */
    light_data->setting = 0;
    light_data->appearance = light_params->appearance;
    light_data->flashfreq = light_params->flashfreq;
    light_data->lasttoggletime = 0;
    strcpy(light_data->label, light_params->label);

    light_strparams->x = light_onp->o_x + light_params->label_x;
    light_strparams->y = light_onp->o_y + light_params->label_y;
    light_strparams->s = light_data->label;
    light_strparams->highlight = 0;
    light_data->llrock = (int *)light_strparams;

    /*
     * Attach the private data to the onode, then return the happy news.
     */
    light_onp->o_data = (int *)light_data;
    return (0);

}				/*gator_light_create */

/*------------------------------------------------------------------------
 * gator_light_destroy
 *
 * Description:
 *	Destroy a gator light object.
 *
 * Arguments:
 *	struct onode *onp : Ptr to the light onode to delete.
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_light_destroy(onp)
     struct onode *onp;

{				/*gator_light_destroy */

    /*
     * For now, this is a no-op.
     */
    return (0);

}				/*gator_light_destroy */

/*------------------------------------------------------------------------
 * gator_light_display
 *
 * Description:
 *	Display/redraw a gator light object.
 *
 * Arguments:
 *	struct onode *onp: Ptr to the light onode to display.
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	Light objects have a pointer to string-drawing params in the
 *	lower-level rock, with the proper highlighting set according
 *	to whether the light is on or off, so we just have to draw
 *	that string to get the proper effect.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_light_display(onp)
     struct onode *onp;

{				/*gator_light_display */

    static char rn[] = "gator_light_display";	/*Routine name */
    struct gator_lightobj *light_data;	/*Ptr to light obj data */
    struct gwin_strparams *label_strparams;	/*String-drawing params */

    /*
     * Draw the label, with proper highlighting depending on whether
     * the light is on.
     */
    light_data = (struct gator_lightobj *)(onp->o_data);
    label_strparams = (struct gwin_strparams *)(light_data->llrock);
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Printing out light label '%s' at (%d, %d)\n",
		mn, rn, label_strparams->s, label_strparams->x,
		label_strparams->y);
    WOP_DRAWSTRING(onp->o_window, label_strparams);
    return (0);

}				/*gator_light_display */

/*------------------------------------------------------------------------
 * gator_light_release
 *
 * Description:
 *	Drop the refcount on a gator light object.
 *
 * Arguments:
 *	struct onode *onp : Ptr to the onode whose refcount is
 *	                             to be dropped.
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
gator_light_release(onp)
     struct onode *onp;

{				/*gator_light_release */

    /*
     * For now, this is a no-op.
     */
    return (0);

}				/*gator_light_release */

/*------------------------------------------------------------------------
 * gator_light_set
 *
 * Description:
 *	Set the value of the given gator light object.
 *
 * Arguments:
 *	  struct onode *onp : Ptr to the light onode to be set.
 *	  int setting       : Non-zero for ``on'', zero for ``off''.
 *
 * Returns:
 *	0: Success
 *	Error value otherwise.
 *
 * Environment:
 *	We need to set not only the setting field, but the lower-
 *	level structure stored in the rock must have its highlight
 *	field set correctly.
 *
 * Side Effects:
 *	Does NOT redisplay the light object.
 *------------------------------------------------------------------------*/

int
gator_light_set(onp, setting)
     struct onode *onp;
     int setting;

{				/*gator_light_set */

    static char rn[] = "gator_light_set";	/*Routine name */
    struct gator_lightobj *light_data;	/*Ptr to light obj data */
    struct gwin_strparams *label_strparams;	/*String-drawing params */

    /*
     * Set the object correctly, then set the highlight field in
     * the lower-level rock.
     */
    light_data = (struct gator_lightobj *)(onp->o_data);
    label_strparams = (struct gwin_strparams *)(light_data->llrock);
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Setting light object at 0x%x to %d (%s)", mn,
		rn, onp, setting, (setting ? "ON" : "OFF"));
    light_data->setting = setting;
    label_strparams->highlight = setting;

    return (0);

}				/*gator_light_set */
