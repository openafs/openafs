/*
 * Copyright (c) 2012 Your File System, Inc.
 */

#ifndef OPENAFS_WINNT_CLIENT_OSI_OSI_INTERNAL_H
#define OPENAFS_WINNT_CLIENT_OSI_OSI_INTERNAL_H

#ifdef DEBUG
#ifdef _M_IX86
static __inline void
osi_InterlockedAnd(LONG * pdest, LONG value)
{
    LONG orig, current, new;

    current = *pdest;

    do
    {
        orig = current;
        new = orig & value;
        current = _InterlockedCompareExchange(pdest, new, orig);
    } while (orig != current);
}

static __inline void
osi_InterlockedOr(LONG * pdest, LONG value)
{
    LONG orig, current, new;

    current = *pdest;

    do
    {
        orig = current;
        new = orig | value;
        current = _InterlockedCompareExchange(pdest, new, orig);
    } while (orig != current);
}

#ifndef _InterlockedOr
  #define _InterlockedOr   osi_InterlockedOr
#endif
#ifndef _InterlockedAnd
#define _InterlockedAnd  osi_InterlockedAnd
#endif

#endif
#endif
#endif /* OPENAFS_WINNT_CLIENT_OSI_OSI_INTERNAL_H */