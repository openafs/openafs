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

typedef enum cm_token_tag {
    CM_TOKEN_KAD,
    CM_TOKEN_K5PLUS
} cm_token_tag;

typedef struct tokenEvent {
    cm_token_tag tag;
    afs_uuid_t uuid;
    union {
        char sessionKey[8];
#ifdef AFS_RXK5
        afs_token_wrapper_t wrapped_token[1];
#endif
    };
    struct tokenEvent *next;
} token_event_u;

void cm_RegisterNewTokenEvent(afs_uuid_t uuid, char sessionKey[8]);
BOOL cm_FindTokenEvent(afs_uuid_t uuid, char sessionKey[8]);

void cm_RegisterNewTokenEvent2(token_event_u nte[1]);
BOOL cm_FindTokenEvent2(token_event_u nte[1]);

extern long RpcInit(void);
extern void RpcShutdown(void);
#endif /* __CM_RPC_H__ */
