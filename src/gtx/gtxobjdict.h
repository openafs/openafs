/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

#ifndef __gator_objdict_h
#define	__gator_objdict_h  1

/*--------------------------------------------------------------------------------
 * objdict.h
 *
 * Definitions for the gator object dictionary.
 *--------------------------------------------------------------------------------*/

#include "gtxobjects.h"		/*Standard gator object defns*/

extern int gator_objdict_init();
    /*
     * Summary:
     *    Initialize the gator object dictionary package.
     *
     * Args:
     *	  int adebug: Is debugging output turned on?
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

extern int gator_objdict_add();
    /*
     * Summary:
     *    Add an entry to the gator object dictionary.
     *
     * Args:
     *	  struct onode *objtoadd: Ptr to object to add.
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

extern int gator_objdict_delete();
    /*
     * Summary:
     *    Delete an entry from the gator object dictionary.
     *
     * Args:
     *	  struct onode *objtodelete: Ptr to object to delete.
     *
     * Returns:
     *	  0 on success,
     *	  Error value otherwise.
     */

extern struct onode *gator_objdict_lookup();
    /*
     * Summary:
     *    Look up a gator object by name.
     *
     * Args:
     *	  char *nametofind: String name of desired onode.
     *
     * Returns:
     *	  Ptr to desired onode if successful,
     *	  Null pointer otherwise.
     */

#endif /* __gator_objdict_h */
