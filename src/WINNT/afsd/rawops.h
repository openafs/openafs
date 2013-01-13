/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern afs_int32
raw_ReadData( cm_scache_t *scp, osi_hyper_t *offsetp,
              afs_uint32 length, char *bufferp, afs_uint32 *readp,
              cm_user_t *userp, cm_req_t *reqp);


extern afs_uint32
raw_WriteData( cm_scache_t *scp, osi_hyper_t *offsetp, afs_uint32 length,
               char *bufferp, cm_user_t *userp, cm_req_t *reqp,
               afs_uint32 *writtenp);
