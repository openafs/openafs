/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSI_H_ENV_
#define _OSI_H_ENV_ 1

#include <afs/param.h>

/* misc definitions */

/* large int */
#ifndef DJGPP
#include <rpc.h>
#include <largeint.h>
#include "osithrdnt.h"
#else /* DJGPP */
#include "largeint95.h"
#endif /* !DJGPP */

typedef LARGE_INTEGER osi_hyper_t;
#ifndef DJGPP
typedef GUID osi_uid_t;
#else /* DJGPP */
typedef int osi_uid_t;
#endif /* !DJGPP */

typedef int int32;

#ifndef DJGPP
/* basic util functions */
#include "osiutils.h"

/* FD operations */
#include "osifd.h"

/* lock type definitions */
#include "osiltype.h"
#endif /* !DJGPP */

/* basic sleep operations */
#include "osisleep.h"

#ifndef DJGPP
/* base lock definitions */
#include "osibasel.h"

/* statistics gathering lock definitions */
#include "osistatl.h"

/* RPC debug stuff */
#include "osidb.h"
#else /* DJGPP */
#include "osithrd95.h"
#endif /* !DJGPP */

/* log stuff */
#include "osilog.h"

#endif /*_OSI_H_ENV_ */
