/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef HAVE_UUID_UUID_H
# include <uuid/uuid.h>
#endif

#ifdef AFS_NT40_ENV
# include <rpc.h>
#endif

#include <hcrypto/rand.h>

#include "uuid.h"
#include "jhash.h"

static const opr_uuid_t nilUid = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

void
opr_uuid_create(opr_uuid_t *uuid)
{
#if defined (AFS_NT40_ENV)
    struct opr_uuid_unpacked raw;

    UuidCreate((UUID *) &raw);
    opr_uuid_pack(uuid, &raw);

#elif !defined(KERNEL) && defined(HAVE_UUID_GENERATE)

    uuid_generate(uuid->data);

#else
    RAND_bytes(uuid->data, 16);

    uuid->data[6] = (uuid->data[6] & 0x0F) | 0x40; /* verison is 4 */
    uuid->data[8] = (uuid->data[8] & 0x3F) | 0x80; /* variant is DCE */
#endif
}

int
opr_uuid_isNil(const opr_uuid_t *uuid)
{
   return opr_uuid_equal(uuid, &nilUid);
}

int
opr_uuid_equal(const opr_uuid_t *uuid1, const opr_uuid_t *uuid2)
{
   return memcmp(uuid1, uuid2, sizeof(opr_uuid_t)) == 0;
}

unsigned int
opr_uuid_hash(const opr_uuid_t *uuid)
{
   /* uuid->data is a (unsigned char *), so there's every danger that this
    * may cause an unaligned access on some platforms */
   return opr_jhash((const afs_uint32 *)uuid->data, 4, 0);
}

#if !defined(KERNEL)
int
opr_uuid_toString(const opr_uuid_t *uuid, char **string)
{
   unsigned const char *p;
   int r;

   p = uuid->data;
   r = asprintf(string,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
		"%02x%02x%02x%02x%02x%02x",
		p[0], p[1], p[2],  p[3],  p[4],  p[5],  p[6],  p[7],
		p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
   if (r < 0) {
       *string = NULL;
       return ENOMEM;
   }
   return 0;
}

void
opr_uuid_freeString(char *string)
{
    free(string);
}

int
opr_uuid_fromString(opr_uuid_t *uuid, const char *string)
{
    unsigned int i[16];
    int items, c;

    /* XXX - Traditionally, AFS has printed UUIDs as
     * 00000000-0000-00-00-00000000. We should perhaps also accept
     * that format here
     */
    items = sscanf(string,
		   "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
		   "%02x%02x%02x%02x%02x%02x",
		   &i[0],  &i[1],  &i[2],  &i[3], &i[4],  &i[5],
		   &i[6],  &i[7],  &i[8],  &i[9], &i[10], &i[11],
		   &i[12], &i[13], &i[14], &i[15]);
    if (items !=16) {
	/* Originally, AFS's printed UUIDs would take the form
	 * 00000000-0000-0000-00-00-00000000. Also handle this. */
	items = sscanf(string,
		   "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x-%02x-"
		   "%02x%02x%02x%02x%02x%02x",
		   &i[0],  &i[1],  &i[2],  &i[3], &i[4],  &i[5],
		   &i[6],  &i[7],  &i[8],  &i[9], &i[10], &i[11],
		   &i[12], &i[13], &i[14], &i[15]);
    }
    if (items !=16)
	return EINVAL;

    for (c=0; c<16; c++)
	uuid->data[c] = i[c];

    return 0;
}

#endif

void
opr_uuid_pack(opr_uuid_t *uuid, const struct opr_uuid_unpacked *raw)
{
    afs_uint32 intval;
    unsigned short shortval;

    intval = htonl(raw->time_low);
    memcpy(&uuid->data[0], &intval, sizeof(uint32_t));

    shortval = htons(raw->time_mid);
    memcpy(&uuid->data[4], &shortval, sizeof(uint16_t));

    shortval = htons(raw->time_hi_and_version);
    memcpy(&uuid->data[6], &shortval, sizeof(uint16_t));

    uuid->data[8] = raw->clock_seq_hi_and_reserved;
    uuid->data[9] = raw->clock_seq_low;

    memcpy(&uuid->data[10], &raw->node, 6);
}

void
opr_uuid_unpack(const opr_uuid_t *uuid, struct opr_uuid_unpacked *raw)
{
    afs_uint32 intval;
    unsigned short shortval;

    memcpy(&intval, &uuid->data[0], sizeof(uint32_t));
    raw->time_low = ntohl(intval);

    memcpy(&shortval, &uuid->data[4], sizeof(uint16_t));
    raw->time_mid = ntohs(shortval);

    memcpy(&shortval, &uuid->data[6], sizeof(uint16_t));
    raw->time_hi_and_version = ntohs(shortval);

    raw->clock_seq_hi_and_reserved = uuid->data[8];
    raw->clock_seq_low = uuid->data[9];

    memcpy(&raw->node, &uuid->data[10], 6);
}

