/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSIUTILS_H_ENV_
#define _OSIUTILS_H_ENV_ 1

extern int osi_UIDCmp(UUID *uid1, UUID *uid2);

extern void osi_LongToUID(long inval, UUID *outuidp);

#endif /*_OSIUTILS_H_ENV_ */
