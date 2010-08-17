/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*	@(#)assert.h 1.1 83/08/01 SMI; from UCB 4.1 83/05/03	*/
/* Modified to dump core, rather than exit.  May/85 RNS */

void AssertionFailed(char *file, int line) AFS_NORETURN;

#define assert(ex) do{if (!(ex)) AssertionFailed(__FILE__, __LINE__);}while(0)
