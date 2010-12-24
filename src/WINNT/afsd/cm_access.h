/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_ACCESS_H
#define OPENAFS_WINNT_AFSD_CM_ACCESS_H 1

#include "cm_user.h"

extern int cm_HaveAccessRights(struct cm_scache *scp, struct cm_user *up,
	afs_uint32 rights, afs_uint32 *outRights);

extern long cm_GetAccessRights(struct cm_scache *scp, struct cm_user *up,
	struct cm_req *reqp);

extern int cm_accessPerFileCheck;
#endif /* OPENAFS_WINNT_AFSD_CM_ACCESS_H */
