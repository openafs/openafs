/* Copyright (C) 1998  Transarc Corporation.  All rights reserved.
 *
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
