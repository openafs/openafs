/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Interface to the authentication-related procedures for the
 *	AFS user account facility.
 *
 */

#ifndef _USS_KAUTH_H_
#define _USS_KAUTH_H_ 1
/*
 * --------------------- Required definitions ---------------------
 */


/*
 * --------------------- Exported definitions ---------------------
 */


/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_kauth_InitAccountCreator(void);
    /*
     * Summary:
     *    Initialize the variable uss_AccountCreator().
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if couldn't get user name from getpwuid().
     */

extern afs_int32 uss_kauth_AddUser(char *, char *);
    /*
     * Summary:
     *    Register the given user with the Authentication Server.
     *
     * Args:
     *    char *a_user   : Name of the user to register.
     *    char *a_passwd : User's (cleartext) password.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem encountered in this function, or
     *    Code returned from a lower-level call.
     */

extern afs_int32 uss_kauth_DelUser(char *);
    /*
     * Summary:
     *    Delete the given user from the Authentication Database.
     *
     * Args:
     *    char *a_user : Name of the user to delete.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem encountered in this function, or
     *    Code returned from a lower-level call.
     */

extern afs_int32 uss_kauth_CheckUserName(void);
    /*
     * Summary:
     *    Make sure the parsed user name is a legal one.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if the user name is not legal.
     */

extern afs_int32 uss_kauth_SetFields(char *username, char *expirestring,
				     char *reuse, char *failures,
				     char *lockout);

#endif /* _USS_KAUTH_H_ */
