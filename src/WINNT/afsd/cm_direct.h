/*
 * Copyright (c) 2012 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of Your File System, Inc nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission from
 *   Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OPENAFS_WINNT_AFSD_CM_DIRECT_H
#define OPENAFS_WINNT_AFSD_CM_DIRECT_H 1

extern afs_int32
cm_SetupDirectStoreBIOD( cm_scache_t *scp,
                         osi_hyper_t *offsetp,
                         afs_uint32   length,
                         cm_bulkIO_t *biodp,
                         cm_user_t   *userp,
                         cm_req_t    *reqp);

extern afs_int32
cm_DirectWrite( IN cm_scache_t *scp,
                IN osi_hyper_t *offsetp,
                IN afs_uint32   length,
                IN afs_uint32   flags,
                IN cm_user_t   *userp,
                IN cm_req_t    *reqp,
                IN void        *memoryRegionp,
                OUT afs_uint32 *bytesWritten);

#define CM_DIRECT_SCP_LOCKED            0x1

typedef struct rock_BkgDirectWrite {
    osi_hyper_t offset;
    afs_uint32  length;
    afs_uint32  bypass_cache;
    void *      memoryRegion;
} rock_BkgDirectWrite_t;

extern afs_int32
cm_BkgDirectWrite( cm_scache_t *scp, void *rockp, struct cm_user *userp, cm_req_t *reqp);

extern void
cm_BkgDirectWriteDone( cm_scache_t *scp, void *vrockp, afs_int32 code);
#endif
