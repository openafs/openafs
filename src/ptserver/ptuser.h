/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef PTUSER_H
#define PTUSER_H

#include "afs/ptint.h"

/* ptuser.c */
extern afs_int32 pr_Initialize(afs_int32 secLevel, const char *confDir,
 			       char *cell);
extern afs_int32 pr_Initialize2(afs_int32 secLevel, const char *confDir,
				char *cell, int rxgk_level);
extern int pr_End(void);
extern int pr_CreateUser(prname name, afs_int32 *id) AFS_NONNULL();
extern int pr_CreateGroup(prname name, prname owner,
 			  afs_int32 *id) AFS_NONNULL((1,3));
extern int pr_Delete(prname name) AFS_NONNULL();
extern int pr_DeleteByID(afs_int32 id);
extern int pr_AddToGroup(prname user, prname group) AFS_NONNULL();
extern int pr_RemoveUserFromGroup(prname user, prname group) AFS_NONNULL();
extern int pr_NameToId(namelist *names, idlist *ids) AFS_NONNULL();
extern int pr_SNameToId(prname name, afs_int32 *id) AFS_NONNULL();
extern int pr_IdToName(idlist *ids, namelist *names) AFS_NONNULL();
extern int pr_SIdToName(afs_int32 id, prname name) AFS_NONNULL();
extern int pr_GetCPS(afs_int32 id, prlist *CPS) AFS_NONNULL();
extern int pr_GetCPS2(afs_int32 id, afs_uint32 host, prlist *CPS) AFS_NONNULL();
extern int pr_GetHostCPS(afs_uint32 host, prlist *CPS) AFS_NONNULL();
extern int pr_ListMembers(prname group, namelist *lnames) AFS_NONNULL();
extern int pr_ListOwned(afs_int32 oid, namelist *lnames, afs_int32 *moreP)
		       AFS_NONNULL();
extern int pr_IDListMembers(afs_int32 gid, namelist *lnames) AFS_NONNULL();
extern int pr_ListEntry(afs_int32 id, struct prcheckentry *aentry)
		       AFS_NONNULL();
extern afs_int32 pr_ListEntries(int flag, afs_int32 startindex,
 				afs_int32 *nentries,
 				struct prlistentries **entries,
 				afs_int32 *nextstartindex) AFS_NONNULL();
extern int pr_CheckEntryByName(prname name, afs_int32 *id, prname owner,
			       prname creator) AFS_NONNULL();
extern int pr_CheckEntryById(prname name, afs_int32 id, prname owner,
			     prname creator) AFS_NONNULL();
extern int pr_ChangeEntry(prname oldname, prname newname, afs_int32 *newid,
			  prname newowner) AFS_NONNULL((1));
extern int pr_IsAMemberOf(prname uname, prname gname, afs_int32 *flag)
			 AFS_NONNULL();
extern int pr_ListMaxUserId(afs_int32 *mid) AFS_NONNULL();
extern int pr_SetMaxUserId(afs_int32 mid);
extern int pr_ListMaxGroupId(afs_int32 *mid) AFS_NONNULL();
extern int pr_SetMaxGroupId(afs_int32 mid);
extern afs_int32 pr_SetFieldsEntry(afs_int32 id, afs_int32 mask,
				   afs_int32 flags, afs_int32 ngroups,
				   afs_int32 nusers);
extern int pr_ListSuperGroups(afs_int32 gid, namelist *lnames) AFS_NONNULL();
extern int pr_IDListExpandedMembers(afs_int32 gid, namelist *lnames)
				   AFS_NONNULL();

#endif /* PTUSER_H */
