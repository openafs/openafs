/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _KAUTH_KKIDS_H
#define _KAUTH_KKIDS_H

extern int init_child(char *);
extern int password_bad(char *);
extern int give_to_child(char *);
extern int terminate_child(void);
#endif
