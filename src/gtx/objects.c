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
 *	Implementation of the gator object interface.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "gtxobjects.h"		/*Interface for this module */
#include "gtxtextobj.h"		/*Text object interface */
#include "gtxlightobj.h"	/*Light object interface */
#include "gtxobjdict.h"		/*Object dictionary module */
#include <stdio.h>		/*Standard I/O stuff */
#include <errno.h>

#include <string.h>
#include <stdlib.h>

/*
 * Number of known gator object types.
 */
#define GATOR_NUM_OBJTYPES 3

static char mn[] = "gator_objects";	/*Module name */
int objects_debug;		/*Is debugging output on? */

int (*on_create[GATOR_NUM_OBJTYPES]) ();	/*Array of ptrs to creation functions */
struct onodeops objops[GATOR_NUM_OBJTYPES];	/*Per-type op arrays */


/*--------------------------------------------------------------------------------
 * gator_objects_init
 *
 * Description:
 *	Initialize the gator object package.
 *
 * Arguments:
 *	struct onode_initparams *params: Initialization parameters.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	*** MUST BE THE FIRST ROUTINE CALLED FROM
 *	      THIS PACKAGE ***
 *
 * Side Effects:
 *	
 *--------------------------------------------------------------------------------*/

int
gator_objects_init(params)
     struct onode_initparams *params;

{				/*gator_objects_init */

    static char rn[] = "gator_objects_init";	/*Routine name */
    static int initialized = 0;	/*Have we been called? */
    register int code;		/*Return code */

    /*
     * If we've already been called, just return.
     */
    if (initialized) {
	initialized++;
	if (objects_debug)
	    fprintf(stderr, "[%s:%s] Called more than once!! (%d time[s])\n",
		    mn, rn, initialized);
	return (0);
    }

    /*
     * Remember our debugging level.
     */
    objects_debug = params->i_debug;

    /*
     * Set up the onode op array, one entry for each known gator object type.
     */
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Setting up objops array\n", mn, rn);
    objops[GATOR_OBJ_TEXT] = gator_text_ops;
    objops[GATOR_OBJ_LIGHT] = gator_light_ops;

    /*
     * Initialize the object dictionary.
     */
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Initializing object dictionary\n", mn, rn);
    code = gator_objdict_init(objects_debug);
    if (code) {
	fprintf(stderr,
		"[%s:%s] Can't initialize object dictionary: error code is %d\n",
		mn, rn, code);
	return (code);
    }

    /*
     * Initialize the chosen window package.  Remember the base window
     * is accessible as gator_basegwin.
     */
    if (objects_debug) {
	fprintf(stderr,
		"[%s:%s] Initializing gator window module for package %d.\n",
		mn, rn, params->i_gwparams->i_type);
	fprintf(stderr,
		"\tWindow init params are: type %d, (%d, %d), width=%d, height=%d, debug=%d\n",
		params->i_gwparams->i_type, params->i_gwparams->i_x,
		params->i_gwparams->i_y, params->i_gwparams->i_width,
		params->i_gwparams->i_height, params->i_gwparams->i_debug);
    }
    code = gw_init(params->i_gwparams);
    if (code) {
	fprintf(stderr,
		"[%s:%s] Can't initialize gator windows for package %d; error is: %d\n",
		mn, rn, params->i_gwparams->i_type, code);
	return (code);
    }

    /*
     * Set up the array of creation functions.
     */
    if (objects_debug)
	fprintf(stderr,
		"[%s:%s] Initializing gator object creation function array.\n",
		mn, rn);
    on_create[GATOR_OBJ_TEXT] = gator_text_create;
    on_create[GATOR_OBJ_LIGHT] = gator_light_create;

    /*
     * Finally, return the good news.
     */
    return (0);

}				/*gator_objects_init */

/*--------------------------------------------------------------------------------
 * gator_objects_create
 *
 * Description:
 *	Create an onode of the given type.
 *
 * Arguments:
 *	struct onode_createparams *params: Ptr to creation params.
 *
 * Returns:
 *	Ptr to newly-created onode if successful,
 *	Null pointer otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *--------------------------------------------------------------------------------*/

struct onode *
gator_objects_create(params)
     struct onode_createparams *params;

{				/*gator_objects_create */

    static char rn[] = "gator_objects_create";	/*Routine name */
    register int code;		/*Return code */
    struct onode *new_onode;	/*Ptr to new onode */

    if (objects_debug) {
	fprintf(stderr, "[%s:%s] Creating onode type %d, named '%s'\n", mn,
		rn, params->cr_type, params->cr_name);
	fprintf(stderr, "\tOrigin at (%d, %d)\n", params->cr_x, params->cr_y);
	fprintf(stderr, "\tWidth=%d, height=%d\n", params->cr_width,
		params->cr_height);
	fprintf(stderr, "\tHelpstring='%s'\n", params->cr_helpstring);
	fprintf(stderr, "\tWindow struct at 0x%x\n", params->cr_window);
    }

    if (objects_debug)
	fprintf(stderr,
		"[%s:%s] Allocating %d bytes for new onode structure\n", mn,
		rn, sizeof(struct onode));
    new_onode = (struct onode *)malloc(sizeof(struct onode));
    if (new_onode == NULL) {
	fprintf(stderr,
		"[%s:%s] Can't allocate %d bytes for new onode structure; errno is %d\n",
		mn, rn, sizeof(struct onode), errno);
	return (NULL);
    }

    /*
     * Fill in the onode fields we can do right away.
     * **** Don't do anything with cr_helpstring yet - eventually,
     *      we'll create a scrollable text help object with it ****
     */
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Filling in onode fields\n", mn, rn,
		sizeof(struct onode));
    new_onode->o_type = params->cr_type;
    strcpy(new_onode->o_name, params->cr_name);
    new_onode->o_x = params->cr_x;
    new_onode->o_y = params->cr_y;
    new_onode->o_width = params->cr_width;
    new_onode->o_height = params->cr_height;
    new_onode->o_changed = 1;
    new_onode->o_refcount = 1;
    new_onode->o_window = params->cr_window;
    new_onode->o_op = &(objops[params->cr_type]);
    new_onode->o_home = params->cr_home_obj;
    new_onode->o_help = NULL;
    new_onode->o_nextobj = NULL;
    new_onode->o_upobj = params->cr_parent_obj;
    new_onode->o_downobj = NULL;

    /*
     * Call the proper routine to initialize the private parts of the
     * given object.
     */
    if (objects_debug)
	fprintf(stderr,
		"[%s:%s] Calling the creation routine for gator object type %d\n",
		mn, rn, params->cr_type);
    code = (on_create[params->cr_type]) (new_onode, params);
    if (code) {
	if (objects_debug)
	    fprintf(stderr,
		    "[%s:%s] Error %d in creation routine for gator object type %d\n",
		    mn, rn, code, params->cr_type);
	free(new_onode);
	return (NULL);
    }

    /*
     * Set the links on the parent and previous objects, if so directed.
     */
    if (params->cr_prev_obj != NULL) {
	if (objects_debug)
	    fprintf(stderr,
		    "[%s:%s] Setting o_nextobj pointer in the previous object located at 0x%x (previous value was 0x%x)\n",
		    mn, rn, params->cr_prev_obj,
		    params->cr_prev_obj->o_nextobj);
	params->cr_prev_obj->o_nextobj = new_onode;
    }
    if (params->cr_parent_obj != NULL) {
	if (objects_debug)
	    fprintf(stderr,
		    "[%s:%s] Setting o_downobj pointer in the parent object located at 0x%x (previous value was 0x%x)\n",
		    mn, rn, params->cr_parent_obj,
		    params->cr_parent_obj->o_downobj);
	params->cr_parent_obj->o_downobj = new_onode;
    }

    /*
     * Return the location of the completely-initialized onode object.
     */
    return (new_onode);

}				/*gator_objects_create */

/*--------------------------------------------------------------------------------
 * gator_objects_lookup
 *
 * Description:
 *	
 *
 * Arguments:
 *	char *onode_name: Onode string name to find.
 *
 * Returns:
 *	Ptr to onode matching the given name if one exists,
 *	Null pointer otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *--------------------------------------------------------------------------------*/

struct onode *
gator_objects_lookup(onode_name)
     char *onode_name;

{				/*gator_objects_lookup */

    static char rn[] = "gator_objects_lookup";	/*Routine name */

    /*
     * Life is very simple here - just call the dictionary routine.
     */
    if (objects_debug)
	fprintf(stderr, "[%s:%s] Looking up gator object '%s'\n", mn, rn,
		onode_name);
    return (gator_objdict_lookup(onode_name));

}				/*gator_objects_lookup */
