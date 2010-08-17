/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * some macros to permit a couple of new features while remaining compatible
 * and reasonably portable...
 * could tighten up 0 and 3 a little, but I've left them like this for now
 * for ease of understanding.
 * assumes sizeof(afs_int32) == 4, but then, so does the rest of AFS...
 */
#ifndef KAPORT_H
#define KAPORT_H

#define unpack_long(src, dst) { \
dst[0] = ((unsigned char)(((afs_uint32)(src) & 0xff000000) >> 24) & 0xff); \
dst[1] = ((unsigned char)(((afs_uint32)(src) & 0x00ff0000) >> 16) & 0xff); \
dst[2] = ((unsigned char)(((afs_uint32)(src) & 0x0000ff00) >>  8) & 0xff); \
dst[3] = ((unsigned char)(((afs_uint32)(src) & 0x000000ff) >>  0) & 0xff); }

#define pack_long(src) ( \
(afs_uint32) ( \
		 ((afs_uint32) ((src)[0] & 0xff) << 24) | \
		 ((afs_uint32) ((src)[1] & 0xff) << 16) | \
		 ((afs_uint32) ((src)[2] & 0xff) <<  8) | \
		 ((afs_uint32) ((src)[3] & 0xff) <<  0) ) )

#endif /* KAPORT_H */
