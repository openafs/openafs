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

#include <rpc.h>
/* misc definitions */
typedef LARGE_INTEGER osi_hyper_t;
typedef GUID osi_uid_t;

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

/* large int */
#include <largeint.h>

#endif /*_OSI_H_ENV_ */
