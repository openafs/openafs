/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef _OSI_PROTOTYPES_H_
#define _OSI_PROTOTYPES_H_

/* osi_vnodeops.c */
int afs_putapage(struct vnode *vp, struct page *pages,
#if	defined(AFS_SUN56_ENV)
		 u_offset_t *offp,
#else
		 u_int *offp,
#endif
		 u_int *lenp, int flags, struct AFS_UCRED *credp);



#endif /* _OSI_PROTOTYPES_H_ */
