/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>

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

#if (_MSC_VER >= 1300)
LARGE_INTEGER LargeIntegerAdd(LARGE_INTEGER a, LARGE_INTEGER b)
{ 
	LARGE_INTEGER result;
	int carry;
	result.LowPart=a.LowPart+b.LowPart;
	carry=(result.LowPart < b.LowPart) ? 1 : 0;
	result.HighPart=a.HighPart+b.HighPart+ carry;
	return result;
}
LARGE_INTEGER LargeIntegerSubtract(LARGE_INTEGER a, LARGE_INTEGER b)
{ 
	LARGE_INTEGER result;
	int borrow;
	result.LowPart=a.LowPart-b.LowPart;
	borrow=(result.LowPart > a.LowPart) ? 1 : 0;
	result.HighPart=a.HighPart-b.HighPart- borrow;
	return result;
}
LARGE_INTEGER ExtendedLargeIntegerDivide(LARGE_INTEGER a, unsigned long b, unsigned long *remainder)
{
	LARGE_INTEGER result;
	ULONGLONG a1,q1,r1;

    result.HighPart=0;
    result.LowPart=0;
    if (b == 0) { return result; }
    if (b == 1) { *remainder = 0; return a; }

    a1 = a.HighPart;
    a1 <<= 32;
    a1 |= a.LowPart;
	q1=a1/b;
	r1=a1-(q1*b);
	if (r1 > ULONG_MAX) /*XXX */;
	result.HighPart=q1 >> 32;
	result.LowPart=q1 & 0xffffffff;
	*remainder=r1 & 0xffffffff;
	return result;
}
LARGE_INTEGER LargeIntegerDivide(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER *remainder)
{
	LARGE_INTEGER result;
	ULONGLONG a1,b1,q1,r1;

    result.HighPart=0;
    result.LowPart=0;
    if (b.HighPart == 0 && b.LowPart == 0) { return result; }
    if (b.HighPart == 0 && b.LowPart == 1) { 
		remainder->HighPart = 0; 
		remainder->LowPart = 0;
		return a; 
	}

    a1 = a.HighPart;
    a1 <<= 32;
    a1 |= a.LowPart;
    b1 = b.HighPart;
    b1 <<= 32;
    b1 |= b.LowPart;
	q1=a1/b1;
	r1=a1-(q1*b1);
	result.HighPart=q1 >> 32;
	result.LowPart=q1 & 0xffffffff;
	remainder->HighPart=r1 >> 32;
	remainder->LowPart=r1 & 0xffffffff;
	return result;
}

LARGE_INTEGER ConvertLongToLargeInteger(unsigned long a) 
{
	LARGE_INTEGER result;
	result.HighPart=0;
	result.LowPart=a;
	return result;
}
#endif
