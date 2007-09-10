/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSI_H_ENV_
#define _OSI_H_ENV_ 1

#include <afs/param.h>

/* misc definitions */

/* large int */
#include <rpc.h>
#if !defined(_MSC_VER) || (_MSC_VER < 1300)
#include "largeint.h"
#endif
#include "osithrdnt.h"

typedef LARGE_INTEGER osi_hyper_t;
#if _MSC_VER >= 1300
LARGE_INTEGER LargeIntegerAdd(LARGE_INTEGER a, LARGE_INTEGER b);
LARGE_INTEGER LargeIntegerSubtract(LARGE_INTEGER a, LARGE_INTEGER b);
LARGE_INTEGER ExtendedLargeIntegerDivide(LARGE_INTEGER a, unsigned long b, unsigned long *remainder);
LARGE_INTEGER LargeIntegerDivide(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER *remainder);
LARGE_INTEGER ConvertLongToLargeInteger(unsigned long a); 
#define LargeIntegerGreaterThan(a, b) \
 ((a).HighPart > (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart > (b).LowPart))

#define LargeIntegerGreaterThanOrEqualTo(a, b) \
 ((a).HighPart > (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart >= (b).LowPart))
  
#define LargeIntegerLessThan(a, b) \
 ((a).HighPart < (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart < (b).LowPart))

#define LargeIntegerLessThanOrEqualTo(a, b) \
 ((a).HighPart < (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart <= (b).LowPart))

#define LargeIntegerEqualTo(a, b) \
  ((a).HighPart == (b).HighPart && (a).LowPart == (b).LowPart)
  
#define LargeIntegerGreaterThanZero(a) \
 ((a).HighPart > 0 || ((a).HighPart == 0 && (a).LowPart != 0))

#define LargeIntegerGreaterOrEqualToZero(a) ((a).HighPart >= 0)
  
#define LargeIntegerLessThanZero(a) ((a).HighPart < 0)

#define LargeIntegerNotEqualToZero(a) ((a).HighPart || (a).LowPart)
#endif
typedef GUID osi_uid_t;

typedef int int32;

/* basic util functions */
#include "osiutils.h"

/* FD operations */
#include "osifd.h"

/* lock type definitions */
#include "osiltype.h"

/* basic sleep operations */
#include "osisleep.h"

/* base lock definitions */
#include "osibasel.h"

/* statistics gathering lock definitions */
#include "osistatl.h"

/* RPC debug stuff */
#include "osidb.h"

/* log stuff */
#include "osilog.h"

#endif /*_OSI_H_ENV_ */
