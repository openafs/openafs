/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header: /tmp/cvstemp/openafs/src/pam/afs_account.c,v 1.1.1.4 2001/07/14 22:23:11 hartmans Exp $");

#include <security/pam_appl.h>
#include <security/pam_modules.h>

extern int
pam_sm_acct_mgmt(
	pam_handle_t	*pamh,
	int		flags,
	int		argc,
	const char	**argv)
{
    return PAM_SUCCESS;
}
