/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 */

#ifndef OPENAFS_OPR_UUID_H
#define OPENAFS_OPR_UUID_H 1

struct opr_uuid {
    unsigned char data[16];
};

struct opr_uuid_unpacked {
    afs_uint32 time_low;
    unsigned short time_mid;
    unsigned short time_hi_and_version;
    char clock_seq_hi_and_reserved;
    char clock_seq_low;
    char node[6];
};

typedef struct opr_uuid opr_uuid_t;
typedef opr_uuid_t opr_uuid; /* For XDR */

extern void opr_uuid_create(opr_uuid_t *uuid);
extern int opr_uuid_isNil(const opr_uuid_t *uuid);
extern int opr_uuid_equal(const opr_uuid_t *uuid1, const opr_uuid_t *uuid2);
extern unsigned int opr_uuid_hash(const opr_uuid_t *uuid);

#if !defined(KERNEL)
extern int opr_uuid_toString(const opr_uuid_t *uuid, char **string);
extern void opr_uuid_freeString(char *string);
extern int opr_uuid_fromString(opr_uuid_t *uuid, const char *string);
#endif

extern void opr_uuid_pack(opr_uuid_t *uuid, const struct opr_uuid_unpacked *raw);
extern void opr_uuid_unpack(const opr_uuid_t *uuid, struct opr_uuid_unpacked *raw);

#endif
