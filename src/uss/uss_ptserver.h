/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * Copyright TRANSARC CORPORATION 1990
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 * uss_ptserver.h
 *	Interface to the basic procedures for the AFS user account
 *	facility.
 */

#ifndef _USS_PTSERVER_H_
#define _USS_PTSERVER_H_ 1

/*
 * --------------------- Required definitions ---------------------
 */
#include "uss_common.h"		/*Commons uss definitions*/


/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_ptserver_AddUser();
    /*
     * Summary:
     *    Register the given user with the Protection Server.
     *
     * Args:
     *	  char *a_user : Name of the user to register.
     *	  char *a_uid  : Points to the uid registered for the named user.
     *
     * Returns:
     *	  0	    if everything went well,
     *	  PRIDEXIST if the chosen uid already exists, or
     *	  Code returned from a lower-level call.
     */

extern afs_int32 uss_ptserver_DelUser();
    /*
     * Summary:
     *    Delete the given user from the Protection Server.
     *
     * Args:
     *	  char *a_name : User name to delete.
     *
     * Returns:
     *	  0	    if everything went well, or
     *	  Code returned from a lower-level call.
     */

extern afs_int32 uss_ptserver_XlateUser();
    /*
     * Summary:
     *    Ask the Protection Server to translate the given user
     *	  name to its corresponding AFS uid.
     *
     * Args:
     *	  char *a_user  : Name of the user to translate.
     *	  afs_int32 *a_uidP  : Points to the uid registered for the named user.
     *
     * Returns:
     *	  0	    if everything went well,
     *	  Code returned from a lower-level call.
     */

#endif /* _USS_PTSERVER_H_ */
