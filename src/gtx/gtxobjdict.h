/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __gator_objdict_h
#define	__gator_objdict_h  1

/*--------------------------------------------------------------------------------
 * objdict.h
 *
 * Definitions for the gator object dictionary.
 *--------------------------------------------------------------------------------*/

#include "gtxobjects.h"		/*Standard gator object defns */

extern int gator_objdict_init(int adebug);
    /*
     * Summary:
     *    Initialize the gator object dictionary package.
     *
     * Args:
     *    int adebug: Is debugging output turned on?
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern int gator_objdict_add(struct onode *objtoadd);
    /*
     * Summary:
     *    Add an entry to the gator object dictionary.
     *
     * Args:
     *    struct onode *objtoadd: Ptr to object to add.
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern int gator_objdict_delete(struct onode *objtodelete);
    /*
     * Summary:
     *    Delete an entry from the gator object dictionary.
     *
     * Args:
     *    struct onode *objtodelete: Ptr to object to delete.
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern struct onode *gator_objdict_lookup(char *nametofind);
    /*
     * Summary:
     *    Look up a gator object by name.
     *
     * Args:
     *    char *nametofind: String name of desired onode.
     *
     * Returns:
     *    Ptr to desired onode if successful,
     *    Null pointer otherwise.
     */

#endif /* __gator_objdict_h */
