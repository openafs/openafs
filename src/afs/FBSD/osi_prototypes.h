/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
/*
 * osi_prototypes.h
 *
 * Exported macos support routines.
 */
#ifndef _OSI_PROTO_H_
#define _OSI_PROTO_H_

/* osi_misc.c */
extern int osi_lookupname(char *aname, enum uio_seg seg, int followlink,
	                  struct vnode **dirvpp, struct vnode **vpp);
#endif /* _OSI_PROTO_H_ */
