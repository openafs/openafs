/*
 * Copyright (c) 2009 Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __SMB_RPC_H__
#define __SMB_RPC_H__

struct smb_rpc;

#ifdef SMB_RPC_IMPL

#include "msrpc.h"

typedef
struct smb_rpc {
    smb_fid_t * fidp;
    msrpc_conn  rpc_conn;
} smb_rpc_t;

#endif

afs_int32
smb_SetupRPCFid(smb_fid_t * fidp, const clientchar_t * epnamep,
		unsigned short * file_type,
		unsigned short * device_state);

void
smb_CleanupRPCFid(IN smb_fid_t * fidp);

afs_int32
smb_RPCRead(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

afs_int32
smb_RPCWrite(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

afs_int32
smb_RPCV3Read(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

afs_int32
smb_RPCV3Write(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

afs_int32
smb_RPCNmpipeTransact(smb_fid_t *fidp, smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op);

#endif	/* __SMB_RPC_H__ */
