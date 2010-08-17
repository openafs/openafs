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
extern int pr_End(void);
extern int pr_CreateUser(char name[PR_MAXNAMELEN], afs_int32 *id);
extern int pr_CreateGroup(char name[PR_MAXNAMELEN], char owner[PR_MAXNAMELEN],
 			  afs_int32 *id);
extern int pr_Delete(char *name);
extern int pr_DeleteByID(afs_int32 id);
extern int pr_AddToGroup(char *user, char *group);
extern int pr_RemoveUserFromGroup(char *user, char *group);
extern int pr_NameToId(namelist *names, idlist *ids);
extern int pr_SNameToId(char name[PR_MAXNAMELEN], afs_int32 *id);
extern int pr_IdToName(idlist *ids, namelist *names);
extern int pr_SIdToName(afs_int32 id, char name[PR_MAXNAMELEN]);
extern int pr_GetCPS(afs_int32 id, prlist *CPS);
extern int pr_GetCPS2(afs_int32 id, afs_uint32 host, prlist *CPS);
extern int pr_GetHostCPS(afs_uint32 host, prlist *CPS);
extern int pr_ListMembers(char *group, namelist *lnames);
extern int pr_ListOwned(afs_int32 oid, namelist *lnames, afs_int32 *moreP);
extern int pr_IDListMembers(afs_int32 gid, namelist *lnames);
extern int pr_ListEntry(afs_int32 id, struct prcheckentry *aentry);
extern afs_int32 pr_ListEntries(int flag, afs_int32 startindex,
 				afs_int32 *nentries,
 				struct prlistentries **entries,
 				afs_int32 *nextstartindex);
extern int pr_CheckEntryByName(char *name, afs_int32 *id, char *owner,
			       char *creator);
extern int pr_CheckEntryById(char *name, afs_int32 id, char *owner,
			     char *creator);
extern int pr_ChangeEntry(char *oldname, char *newname, afs_int32 *newid,
			  char *newowner);
extern int pr_IsAMemberOf(char *uname, char *gname, afs_int32 *flag);
extern int pr_ListMaxUserId(afs_int32 *mid);
extern int pr_SetMaxUserId(afs_int32 mid);
extern int pr_ListMaxGroupId(afs_int32 *mid);
extern int pr_SetMaxGroupId(afs_int32 mid);
extern afs_int32 pr_SetFieldsEntry(afs_int32 id, afs_int32 mask,
				   afs_int32 flags, afs_int32 ngroups,
				   afs_int32 nusers);
extern int pr_ListSuperGroups(afs_int32 gid, namelist *lnames);
extern int pr_IDListExpandedMembers(afs_int32 gid, namelist *lnames);

#endif /* PTUSER_H */
