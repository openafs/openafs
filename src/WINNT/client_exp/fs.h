/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
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

static SetVolCmd(struct cmd_syndesc *);

#endif /* FS_H_ENV */
