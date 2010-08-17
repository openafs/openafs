/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Interface to the ACL and quota-related operations used by
 *	the AFS user account facility.
 */

#ifndef _USS_ACL_H_
#define _USS_ACL_H_ 1
/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_acl_SetAccess(char *a_access, int a_clear,
       				   int a_negative);
    /*
     * Summary:
     *    Set the value of the given ACL.
     *
     * Args:
     *    a_access   : Ptr to the pathname & ACL to set.
     *    a_clear    : Should we clear out the ACL first?
     *    a_negative : Set the negative list?
     *
     * Returns:
     *    0 if everything went well,
     *    Lower-level code otherwise.
     */

extern afs_int32 uss_acl_SetDiskQuota(char *a_path, int a_q);
    /*
     * Summary:
     *    Set the initial disk quota for a user.
     *
     * Args:
     *    a_path : Pathname for volume mountpoint.
     *    a_q    : Quota value.
     *
     * Returns:
     *    0 if everything went well,
     *    Lower-level code otherwise.
     */

extern afs_int32 uss_acl_CleanUp(void);
    /*
     * Summary:
     *    Remove the uss_AccountCreator from the various ACLs s/he
     *    had to wiggle into in order to carry out the account
     *    manipulation.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 (always)
     */

#endif /* _USS_ACL_H_ */
