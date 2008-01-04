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
static void ZapList (struct AclEntry *alist);

static int PruneList (struct AclEntry **ae, int dfs);

static int CleanAcl(struct Acl *aa, char *fname);

static int SetVolCmd(struct cmd_syndesc *as, void *arock);

static int GetCellName(char *cellNamep, struct afsconf_cell *infop);

static int VLDBInit(int noAuthFlag, struct afsconf_cell *infop);

static void Die(int code, char *filename);
#endif /* FS_H_ENV */
