/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SYSCFG_H_
#define SYSCFG_H_

/* Functions for accessing NT system configuration information. */

#ifdef __cplusplus
extern "C" {
#endif

extern int
syscfg_GetIFInfo(int *count, int *addrs, int *masks, int *mtus, int *flags);

#ifdef __cplusplus
};
#endif

#endif /* SYSCFG_H_ */
