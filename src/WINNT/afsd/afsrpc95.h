#ifndef AFSRPC_H
#define AFSRPC_H

/* Copied from declaration of GUID in RPCDCE.H */
typedef struct afs_uuid {
        unsigned long Data1;
        unsigned short Data2;
        unsigned short Data3;
        unsigned char Data4[8];
} afs_uuid_t;

long AFSRPC_SetToken(
        afs_uuid_t      uuid,
        unsigned char   sessionKey[8]
);

long AFSRPC_GetToken(
        afs_uuid_t      uuid,
        unsigned char   sessionKey[8]
);

#endif
