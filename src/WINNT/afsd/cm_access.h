/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef _CM_ACCESS_H_ENV__
#define _CM_ACCESS_H_ENV__ 1

extern int cm_HaveAccessRights(struct cm_scache *scp, struct cm_user *up,
	long rights, long *outRights);

extern long cm_GetAccessRights(struct cm_scache *scp, struct cm_user *up,
	struct cm_req *reqp);

#endif /*  _CM_ACCESS_H_ENV__ */
