#ifndef	__CM_RPC_H__
#define __CM_RPC_H__

#include "afsrpc.h"

void cm_RegisterNewTokenEvent(afs_uuid_t uuid, char sessionKey[8]);
BOOL cm_FindTokenEvent(afs_uuid_t uuid, char sessionKey[8]);

extern long RpcInit();

#endif /* __CM_RPC_H__ */
