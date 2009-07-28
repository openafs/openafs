/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _SOFTSIG_H
#define _SOFTSIG_H

void softsig_init(void);
void softsig_signal(int signo, void (*handler) (int));

#endif
