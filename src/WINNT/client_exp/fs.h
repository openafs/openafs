/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __FS_H_ENV__
#define __FS_H_ENV__ 1

/* some forward references */
extern void ZapList(struct AclEntry *);

extern void ZapAcl(struct Acl *);

extern int PruneList (struct AclEntry **, int);

extern void ChangeList(struct Acl *, long, char *, long);

extern int CleanAcl(struct Acl *);

extern void Die(int, char *);

static int SetVolCmd(struct cmd_syndesc *);

#endif /* FS_H_ENV */
