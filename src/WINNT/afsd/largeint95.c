/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Large integer support for DJGPP */

#include <stdlib.h>
#include "largeint95.h"

LARGE_INTEGER LargeIntegerAdd(LARGE_INTEGER a, LARGE_INTEGER b)
{
  LARGE_INTEGER c;
  long long *c1 = (long long *) &c;
  long long *a1 = (long long *) &a;
  long long *b1 = (long long *) &b;
  *c1 = *a1 + *b1;
  return c;
}
  
LARGE_INTEGER LargeIntegerSubtract(LARGE_INTEGER a, LARGE_INTEGER b)
{
  LARGE_INTEGER c;
  long long *c1 = (long long *) &c;
  long long *a1 = (long long *) &a;
  long long *b1 = (long long *) &b;
  *c1 = *a1 - *b1;
  return c;
}
  
LARGE_INTEGER ConvertLongToLargeInteger(unsigned long a)
{
  LARGE_INTEGER n;

  n.LowPart = a;
  n.HighPart = 0;
  return n;
}

LARGE_INTEGER LargeIntegerMultiplyLongByLong(unsigned long a, unsigned long b)
{
  LARGE_INTEGER c;
  long long *c1 = (long long *) &c;

  *c1 = (long long) a * (long long) b;
  return c;
}
  
LARGE_INTEGER LargeIntegerMultiplyByLong(LARGE_INTEGER a, unsigned long b)
{
  LARGE_INTEGER c;
  long long *c1 = (long long *) &c;
  long long *a1 = (long long *) &a;

  *c1 = *a1 * (long long) b;
  return c;
}
  
unsigned long LargeIntegerDivideByLong(LARGE_INTEGER a, unsigned long b)
{
  lldiv_t q;
  long long *a1 = (long long *) &a;

  q = lldiv(*a1, (long long) b);
  return (unsigned long) q.quot;
}

#if 0
LARGE_INTEGER LargeIntegerAdd(LARGE_INTEGER a, LARGE_INTEGER b)
{
  LARGE_INTEGER c;

  c.LowPart = a.LowPart + b.LowPart;
  c.HighPart = a.HighPart + b.HighPart;

  /* not sure how to do a real carry */
  if (c.LowPart < a.LowPart)
    c.HighPart++;

  return c;
}

LARGE_INTEGER LargeIntegerSubtract(LARGE_INTEGER a, LARGE_INTEGER b)
{
  LARGE_INTEGER c;

  c.LowPart = a.LowPart - b.LowPart;
  c.HighPart = a.HighPart - b.HighPart;

  /* borrow */
  if (c.LowPart > a.LowPart)
    c.HighPart--;

  return c;
}

__inline__ unsigned long mult32(unsigned long a, unsigned long b,
                                unsigned long *ov)
{
  unsigned long p, o;
  
  /* multiply low part and save the overflow bits */
  __asm__ __volatile__ ("movl %2, %%eax\n
                         mull %3, %%eax\n
                         movl %%eax, %0\n
                         movl %%edx, %1"
                        : "=g" (p), "=g" (o)
                        : "g" (a), "g" (b)
                        : "ax", "dx", "memory"
                        );
  *ov = o;
  return p;
}

__inline__ unsigned long div32(unsigned long a, unsigned long b,
                               unsigned long *rem)
{
  unsigned long q, r;
  
  /* multiply low part and save the overflow bits */
  __asm__ __volatile__ ("movl %2, %%eax\n
                         divl %3, %%eax\n
                         movl %%eax, %0\n
                         movl %%edx, %1"
                        : "=g" (q), "=g" (r)
                        : "g" (a), "g" (b)
                        : "ax", "dx", "memory"
                        );
  *rem = r;
  return q;
}

LARGE_INTEGER LargeIntegerMultiplyLongByLong(unsigned long a, unsigned long b)
{
  LARGE_INTEGER prod;

  prod.LowPart = mult32(a, b, &prod.HighPart);
  return prod;
}
  
LARGE_INTEGER LargeIntegerMultiplyByLong(LARGE_INTEGER a, unsigned long b)
{
  LARGE_INTEGER prod;
  unsigned long x, prodl, prodh, ovl, ovh;
  
  /* multiply low part and save the overflow bits */
  prod.LowPart = mult32(a.LowPart, b, &ovl);
  
  /* multiply high part */
  prod.HighPart = mult32(a.HighPart, b, &ovh);
  
  /* add overflow from low part */
  prod.HighPart += ovl;

  return prod;
}
  
unsigned long LargeIntegerDivideByLong(LARGE_INTEGER a, unsigned long b, unsigned long *rem)
{
  unsigned long n, r, q; 
  LARGE_INTEGER t;
  
  if (b == 0) { return 0; }
  if (b == 1) { *rem = 0; return a.LowPart; }

  n = div32(a.LowPart, b, &r);
  if (a.HighPart == 0)
  {
    *rem = r;
    return n;
  }
  else
  {
    q = div32(0xffffffff-b+1, b, &r);
    q++;
    n += q * a.HighPart;
    n += LargeIntegerDivideByLong(LargeIntegerMultiplyLongByLong(r, a.HighPart), b, rem);
    return n;
  }
}
#endif
  
#if 0
int LargeIntegerGreaterThan(LARGE_INTEGER a, LARGE_INTEGER b)
{
  if (a.HighPart > b.HighPart) return 1;
  else if (a.HighPart == b.HighPart && a.LowPart > b.LowPart) return 1;
  else return 0;
}

int LargeIntegerGreaterThanOrEqualTo(LARGE_INTEGER a, LARGE_INTEGER b)
{
  if (a.HighPart > b.HighPart) return 1;
  else if (a.HighPart == b.HighPart && a.LowPart >= b.LowPart) return 1;
  else return 0;
}
  
int LargeIntegerEqualTo(LARGE_INTEGER a, LARGE_INTEGER b)
{
  if (a.HighPart == b.HighPart && a.LowPart == b.LowPart) return 1;
  else return 0;
}

int LargeIntegerGreaterOrEqualToZero(LARGE_INTEGER a)
{
  return ((a.HighPart & 0x8fffffff) ? 0 : 1);
}

int LargeIntegerLessThanZero(LARGE_INTEGER a)
{
  return ((a.HighPart & 0x8fffffff) ? 1 : 0);
}
#endif
