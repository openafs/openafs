/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
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
