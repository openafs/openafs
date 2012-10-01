/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define	NONOTIFIER		    "__NONOTIFIER__"

/* exit values indicating that NT SCM integrator should restart bosserver */
#ifdef AFS_NT40_ENV
#define BOSEXIT_RESTART        0xA0
#define BOSEXIT_DORESTART(code)  (((code) & ~(0xF)) == BOSEXIT_RESTART)
#endif

/* max time to wait for fileserver shutdown */
#define	FSSDTIME	(30 * 60)	/* seconds */
