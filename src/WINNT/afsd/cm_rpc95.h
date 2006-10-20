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

/* Copied from declaration of GUID in RPCDCE.H */
typedef struct afs_uuid {
     unsigned long Data1;
     unsigned short Data2;
     unsigned short Data3;
     unsigned char Data4[8];
} afs_uuid_t;

long AFSRPC_SetToken(
     afs_uuid_t     uuid,
     unsigned char  sessionKey[8]
);

long AFSRPC_GetToken(
     afs_uuid_t     uuid,
     unsigned char  sessionKey[8]
);

#endif /* __CM_RPC_H__ */
