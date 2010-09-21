/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Interface to the basic procedures for the AFS user account
 *	facility.
 */

#ifndef _USS_PTSERVER_H_
#define _USS_PTSERVER_H_ 1

/*
 * --------------------- Required definitions ---------------------
 */
#include "uss_common.h"		/*Commons uss definitions */


/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_ptserver_AddUser(char *a_user, char *a_uid);
    /*
     * Summary:
     *    Register the given user with the Protection Server.
     *
     * Args:
     *    char *a_user : Name of the user to register.
     *    char *a_uid  : Points to the uid registered for the named user.
     *
     * Returns:
     *    0         if everything went well,
     *    PRIDEXIST if the chosen uid already exists, or
     *    Code returned from a lower-level call.
     */

extern afs_int32 uss_ptserver_DelUser(char *a_name);
    /*
     * Summary:
     *    Delete the given user from the Protection Server.
     *
     * Args:
     *    char *a_name : User name to delete.
     *
     * Returns:
     *    0         if everything went well, or
     *    Code returned from a lower-level call.
     */

extern afs_int32 uss_ptserver_XlateUser(char *a_user, afs_int32 *a_uidP);
    /*
     * Summary:
     *    Ask the Protection Server to translate the given user
     *    name to its corresponding AFS uid.
     *
     * Args:
     *    char *a_user  : Name of the user to translate.
     *    afs_int32 *a_uidP  : Points to the uid registered for the named user.
     *
     * Returns:
     *    0         if everything went well,
     *    Code returned from a lower-level call.
     */

#endif /* _USS_PTSERVER_H_ */
