/* 
 *  Copyright (C) 1989,2004 by the Massachusetts Institute of Technology
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __AKLOG_H__
#define __AKLOG_H__

#if !defined(lint) && !defined(SABER)
static char *rcsid_aklog_h = "$Id: aklog.h,v 1.1 2004/04/13 03:05:31 jaltman Exp $";
#endif /* lint || SABER */

#ifndef WIN32
#include <afs/param.h>
#endif

#if !defined(vax)
#ifndef WIN32
#include <unistd.h>
#endif
#include <stdlib.h>
#include <limits.h>
#endif

#ifndef WIN32
#include <sys/types.h>
#endif
#include <krb.h>
#include "linked_list.h"

#ifdef __STDC__
#define ARGS(x) x
#else
#define ARGS(x) ()
#endif /* __STDC__ */

#include <afscompat.h>

typedef struct {
    int (*readlink)ARGS((char *, char *, int));
    int (*isdir)ARGS((char *, unsigned char *));
    char *(*getcwd)ARGS((char *, size_t));
    int (*get_cred)ARGS((char *, char *, char *, CREDENTIALS *));
    int (*get_user_realm)ARGS((char *));
    void (*pstderr)ARGS((char *));
    void (*pstdout)ARGS((char *));
    void (*exitprog)ARGS((char));
} aklog_params;

void aklog ARGS((int, char *[], aklog_params *));
void aklog_init_params ARGS((aklog_params *));

#endif /* __AKLOG_H__ */
