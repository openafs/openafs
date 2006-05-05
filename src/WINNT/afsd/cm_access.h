/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _CM_ACCESS_H_ENV__
#define _CM_ACCESS_H_ENV__ 1

#include "cm_user.h"

extern int cm_HaveAccessRights(struct cm_scache *scp, struct cm_user *up,
	afs_uint32 rights, afs_uint32 *outRights);

extern long cm_GetAccessRights(struct cm_scache *scp, struct cm_user *up,
	struct cm_req *reqp);

#endif /*  _CM_ACCESS_H_ENV__ */
