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

afs_int32 pr_Initialize(afs_int32 secLevel, const char *confDir, char *cell);
int pr_End(void);
int pr_CreateUser(char name[PR_MAXNAMELEN], afs_int32 *id);
int pr_CreateGroup(char name[PR_MAXNAMELEN], char owner[PR_MAXNAMELEN], afs_int32 *id);
int pr_Delete(char *name);
int pr_DeleteByID(afs_int32 id);
int pr_AddToGroup(char *user, char *group);
int pr_RemoveUserFromGroup(char *user, char *group);
int pr_NameToId(namelist *names, idlist *ids);
int pr_SNameToId(char name[PR_MAXNAMELEN], afs_int32 *id);
int pr_IdToName(idlist *ids, namelist *names);
int pr_SIdToName(afs_int32 id, char name[PR_MAXNAMELEN]);
int pr_GetCPS(afs_int32 id, prlist *CPS);
int pr_GetCPS2(afs_int32 id, afs_int32 host, prlist *CPS);
int pr_GetHostCPS(afs_int32 host, prlist *CPS);
int pr_ListMembers(char *group, namelist *lnames);
int pr_ListOwned(afs_int32 oid, namelist *lnames, afs_int32 *moreP);
int pr_IDListMembers(afs_int32 gid, namelist *lnames);
int pr_ListEntry(afs_int32 id, struct prcheckentry *aentry);
afs_int32 pr_ListEntries(int flag, afs_int32 startindex, afs_int32 *nentries, struct prlistentries **entries, afs_int32 *nextstartindex);
int pr_CheckEntryByName(char *name, afs_int32 *id, char *owner, char *creator);
int pr_CheckEntryById(char *name, afs_int32 id, char *owner, char *creator);
int pr_ChangeEntry(char *oldname, char *newname, afs_int32 *newid, char *newowner);
int pr_IsAMemberOf(char *uname, char *gname, afs_int32 *flag);
int pr_ListMaxUserId(afs_int32 *mid);
int pr_SetMaxUserId(afs_int32 mid);
int pr_ListMaxGroupId(afs_int32 *mid);
int pr_SetMaxGroupId(afs_int32 mid);
afs_int32 pr_SetFieldsEntry(afs_int32 id, afs_int32 mask, afs_int32 flags, afs_int32 ngroups, afs_int32 nusers);
#endif /* PTUSER_H */
