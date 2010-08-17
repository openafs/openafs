/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <errno.h>

#ifndef	AFS_VOL_NFS_H
#define	AFS_VOL_NFS_H 1
/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*

	System:		VICE-TWO
	Module:		nfs.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */
#define private static
#ifndef NULL
#define NULL	0
#endif
#define TRUE	1
#define FALSE	0
typedef afs_uint32 bit32;	/* Unsigned, 32 bits */
typedef unsigned short bit16;	/* Unsigned, 16 bits */
typedef unsigned char byte;	/* Unsigned, 8 bits */

typedef bit32 Device;		/* Unix device number */
#ifndef	Error
#define	Error	bit32
#endif
#endif /* AFS_VOL_NFS_H */
