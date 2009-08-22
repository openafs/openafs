/* Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BUTM_PROTOTYPES_H
#define _BUTM_PROTOTYPES_H

/* file_tm.c */

extern afs_int32 SeekFile(struct butm_tapeInfo *, int);
extern afs_int32 butm_file_Instantiate(struct butm_tapeInfo *,
		                       struct tapeConfig *);
extern afs_int32 NextFile(struct butm_tapeInfo *);

#endif
