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
#include <limits.h>

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

    a1=a.HighPart;
    a1<<=32;
    a1 |= a.LowPart;
    q1=a1/b;
    r1=a1-(q1*b);
    if (r1 > ULONG_MAX) /*XXX */;
    result.HighPart=(LONG)(q1 >> 32);
    result.LowPart=(DWORD)(q1 & 0xffffffff);
    *remainder=(unsigned long)(r1 & 0xffffffff);
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

    a1=a.HighPart;
    a1<<=32;
    a1|=a.LowPart;
    b1=b.HighPart;
    b1<<=32;
    b1|=b.LowPart;
    q1=a1/b1;
    r1=a1-(q1*b1);
    result.HighPart=(LONG)(q1 >> 32);
    result.LowPart=(DWORD)(q1 & 0xffffffff);
    remainder->HighPart=(LONG)(r1 >> 32);
    remainder->LowPart=(DWORD)(r1 & 0xffffffff);
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
