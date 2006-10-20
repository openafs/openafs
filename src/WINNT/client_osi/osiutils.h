/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSIUTILS_H_ENV_
#define _OSIUTILS_H_ENV_ 1

extern int osi_UIDCmp(UUID *uid1, UUID *uid2);

extern void osi_LongToUID(long inval, UUID *outuidp);

#endif /*_OSIUTILS_H_ENV_ */
