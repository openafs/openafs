/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _APACHE_AFS_UTILS_H_INCLUDED_
#define _APACHE_AFS_UTILS_H_INCLUDED_

#include <limits.h>
#include <stdio.h>

#include <afs/vice.h>
#include <afs/kautils.h>

#include <netinet/in.h>		/* had to put this for in_addr used in venus.h */
#include <afs/venus.h>		/* for pioctl opcode defines */

#include "assert.h"

#ifndef MAX
#define MAX(A,B)		((A)>(B)?(A):(B))
#endif /* !MAX */
#ifndef MIN
#define MIN(A,B)		((A)<(B)?(A):(B))
#endif /* !MAX */


extern int do_pioctl(char *in_buffer, int in_size, char *out_buffer,
		     int out_size, int opcode, char *path,
		     int followSymLinks);

extern int flipPrimary(char *tokenBuf);
extern afs_int32 getPAG();
extern int haveToken();

extern u_long afsDebugLevel;
#define afslog(level,str) if (level <= afsDebugLevel) (afsLogError str)

/* these are routines used solely for debugging purposes */
extern void hexDump(char *tbuffer, int len);
extern void parseToken(char *buf);
extern int printGroups();


#endif /*_APACHE_AFS_UTILS_H_INCLUDED_ */
