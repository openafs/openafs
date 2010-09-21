/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		vutils.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef AFS_VUTILS_H
#define AFS_VUTILS_H


/* Common definitions for volume utilities */

#define VUTIL_TIMEOUT	15	/* Timeout period for a remote host */


/* Exit codes -- see comments in tcp/exits.h */
#define VUTIL_RESTART	64	/* please restart this job later */
#define VUTIL_ABORT	1	/* do not restart this job */

#endif /* AFS_VUTILS_H */
