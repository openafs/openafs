/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <rpc.h>
#include "osiutils.h"

void osi_LongToUID(long inval, UUID *outuidp)
{
	/* generic UUID whose seconds field we'll never use again */
	UUID genericCazamarUUID = { /* 7C45E3F8-5F73-101B-89A3-204C4F4F5020 */
		0x7C45E3F8,
		0x5F73,
		0x101B,
		{0x89, 0xa3, 0x20, 0x4C, 0x4F, 0x4F, 0x50, 0x20}
	};

	genericCazamarUUID.Data1 = inval;
        memcpy(outuidp, &genericCazamarUUID, sizeof(UUID));
}

/* compare two UIDs in the dictionary ordering */
int osi_UIDCmp(UUID *uid1p, UUID *uid2p)
{
	register int i;
        unsigned int v1;
        unsigned int v2;
        unsigned char *t1p;
        unsigned char *t2p;
	
	if (uid1p->Data1 < uid2p->Data1) return -1;
        else if (uid1p->Data1 > uid2p->Data1) return 1;

	if (uid1p->Data2 < uid2p->Data2) return -1;
        else if (uid1p->Data2 > uid2p->Data2) return 1;

	if (uid1p->Data3 < uid2p->Data3) return -1;
        else if (uid1p->Data3 > uid2p->Data3) return 1;

        t1p = uid1p->Data4;
	t2p = uid2p->Data4;

	for(i=0; i<8; i++) {
		v1 = *t1p++;
                v2 = *t2p++;
                if (v1 < v2) return -1;
                else if (v1 > v2) return 1;
        }
        return 0;
}

void * __RPC_API MIDL_user_allocate(size_t size)
{
  return (void *) malloc(size);
}

void __RPC_API MIDL_user_free(void *p)
{
  free(p);
}
