/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	__CM_RPC_H__
#define __CM_RPC_H__

#include "afsrpc.h"

void cm_RegisterNewTokenEvent(afs_uuid_t uuid, char sessionKey[8]);
BOOL cm_FindTokenEvent(afs_uuid_t uuid, char sessionKey[8]);

extern long RpcInit(void);
extern void RpcShutdown(void);
#endif /* __CM_RPC_H__ */
