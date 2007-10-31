/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file implements the pts related funtions for afscp
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "pts.h"

/*
 * Utility functions
 */

/*
 * Generic fuction for converting input string to an integer.  Pass
 * the error_msg you want displayed if there is an error converting
 * the string.
 */

static int
GetIntFromString(const char *int_str, const char *error_msg)
{
    int i;
    char *bad_char = NULL;

    i = strtoul(int_str, &bad_char, 10);
    if ((bad_char == NULL) || (*bad_char == 0)) {
	return i;
    }

    ERR_EXT(error_msg);
}

/*
 * Generic fuction for converting input string to an pts_groupAccess_t.  Pass
 * the error_msg you want displayed if there is an error converting
 * the string.
 */

static pts_groupAccess_t
GetGroupAccessFromString(const char *in_str, const char *error_msg)
{
    pts_groupAccess_t access;

    if (!strcasecmp("owner", in_str)) {
	access = PTS_GROUP_OWNER_ACCESS;
    } else if (!strcasecmp("group", in_str)) {
	access = PTS_GROUP_ACCESS;
    } else if (!strcasecmp("any", in_str)) {
	access = PTS_GROUP_ANYUSER_ACCESS;
    } else {
	ERR_EXT(error_msg);
    }

    return access;
}

/*
 * Generic fuction for converting input string to an pts_userAccess_t.  Pass
 * the error_msg you want displayed if there is an error converting
 * the string.
 */

static pts_userAccess_t
GetUserAccessFromString(const char *in_str, const char *error_msg)
{
    pts_userAccess_t access;

    if (!strcasecmp("owner", in_str)) {
	access = PTS_USER_OWNER_ACCESS;
    } else if (!strcasecmp("any", in_str)) {
	access = PTS_USER_ANYUSER_ACCESS;
    } else {
	ERR_EXT(error_msg);
    }

    return access;
}

int
DoPtsGroupMemberAdd(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER, GROUP } DoPtsGroupMemberAdd_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    const char *group = as->parms[GROUP].items->data;

    if (!pts_GroupMemberAdd(cellHandle, user, group, &st)) {
	ERR_ST_EXT("pts_GroupMemberAdd", st);
    }

    return 0;
}

int
DoPtsGroupOwnerChange(struct cmd_syndesc *as, void *arock)
{
    typedef enum { OWNER, GROUP } DoPtsGroupOwnerChange_parm_t;
    afs_status_t st = 0;
    const char *owner = as->parms[OWNER].items->data;
    const char *group = as->parms[GROUP].items->data;

    if (!pts_GroupOwnerChange(cellHandle, group, owner, &st)) {
	ERR_ST_EXT("pts_GroupOwnerChange", st);
    }

    return 0;
}

int
DoPtsGroupCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { OWNER, GROUP } DoPtsGroupCreate_parm_t;
    afs_status_t st = 0;
    const char *owner = as->parms[OWNER].items->data;
    const char *group = as->parms[GROUP].items->data;
    int new_group_id;

    if (!pts_GroupCreate(cellHandle, group, owner, &new_group_id, &st)) {
	ERR_ST_EXT("pts_GroupMemberAdd", st);
    }

    printf("Group id %d\n", new_group_id);

    return 0;
}

static void
Print_pts_groupAccess_p(pts_groupAccess_p access, const char *prefix)
{
    if (*access == PTS_GROUP_OWNER_ACCESS) {
	printf("%sPTS_GROUP_OWNER_ACCESS\n", prefix);
    } else if (*access == PTS_GROUP_ACCESS) {
	printf("%sPTS_GROUP_ACCESS\n", prefix);
    } else if (*access == PTS_GROUP_ANYUSER_ACCESS) {
	printf("%sPTS_GROUP_ANYUSER_ACCESS\n", prefix);
    }
}

static void
Print_pts_GroupEntry_p(pts_GroupEntry_p entry, const char *prefix)
{
    printf("%sName %s Uid %d\n", prefix, entry->name, entry->nameUid);
    printf("%sOwner %s Uid %d\n", prefix, entry->owner, entry->ownerUid);
    printf("%sCreator %s Uid %d\n", prefix, entry->creator,
	   entry->creatorUid);
    printf("%sMembership count %d\n", prefix, entry->membershipCount);
    printf("%sList status permission:\n", prefix);
    Print_pts_groupAccess_p(&entry->listStatus, "    ");
    printf("%sList groups owned permission:\n", prefix);
    Print_pts_groupAccess_p(&entry->listGroupsOwned, "    ");
    printf("%sList membership permission:\n", prefix);
    Print_pts_groupAccess_p(&entry->listMembership, "    ");
    printf("%sList add permission:\n", prefix);
    Print_pts_groupAccess_p(&entry->listAdd, "    ");
    printf("%sList delete permission:\n", prefix);
    Print_pts_groupAccess_p(&entry->listDelete, "    ");
}

int
DoPtsGroupGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { GROUP } DoPtsGroupGet_parm_t;
    afs_status_t st = 0;
    const char *group = as->parms[GROUP].items->data;
    pts_GroupEntry_t entry;

    if (!pts_GroupGet(cellHandle, group, &entry, &st)) {
	ERR_ST_EXT("pts_GroupGet", st);
    }

    Print_pts_GroupEntry_p(&entry, "");

    return 0;
}

int
DoPtsGroupDelete(struct cmd_syndesc *as, void *arock)
{
    typedef enum { GROUP } DoPtsGroupDelete_parm_t;
    afs_status_t st = 0;
    const char *group = as->parms[GROUP].items->data;

    if (!pts_GroupDelete(cellHandle, group, &st)) {
	ERR_ST_EXT("pts_GroupDelete", st);
    }

    return 0;
}

int
DoPtsGroupMaxGet(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    int max_group_id;

    if (!pts_GroupMaxGet(cellHandle, &max_group_id, &st)) {
	ERR_ST_EXT("pts_GroupMaxGet", st);
    }

    return 0;
}

int
DoPtsGroupMaxSet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { MAX } DoPtsGroupMaxSet_parm_t;
    afs_status_t st = 0;
    const char *max = as->parms[MAX].items->data;
    int max_group_id;

    max_group_id = GetIntFromString(max, "bad group id");

    if (!pts_GroupMaxSet(cellHandle, max_group_id, &st)) {
	ERR_ST_EXT("pts_GroupMaxSet", st);
    }

    return 0;
}

int
DoPtsGroupMemberList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { GROUP } DoPtsGroupMemberList_parm_t;
    afs_status_t st = 0;
    const char *group = as->parms[GROUP].items->data;
    void *iter;
    char member[PTS_MAX_NAME_LEN];

    if (!pts_GroupMemberListBegin(cellHandle, group, &iter, &st)) {
	ERR_ST_EXT("pts_GroupMemberListBegin", st);
    }

    printf("Listing members of group %s\n", group);
    while (pts_GroupMemberListNext(iter, member, &st)) {
	printf("\t%s\n", member);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("pts_GroupMemberListNext", st);
    }

    if (!pts_GroupMemberListDone(iter, &st)) {
	ERR_ST_EXT("pts_GroupMemberListDone", st);
    }

    return 0;
}

int
DoPtsGroupMemberRemove(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER, GROUP } DoPtsGroupMemberRemove_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    const char *group = as->parms[GROUP].items->data;

    if (!pts_GroupMemberRemove(cellHandle, user, group, &st)) {
	ERR_ST_EXT("pts_GroupMemberRemove", st);
    }

    return 0;
}

int
DoPtsGroupRename(struct cmd_syndesc *as, void *arock)
{
    typedef enum { GROUP, NEWNAME } DoPtsGroupRename_parm_t;
    afs_status_t st = 0;
    const char *group = as->parms[GROUP].items->data;
    const char *new_name = as->parms[NEWNAME].items->data;

    if (!pts_GroupRename(cellHandle, group, new_name, &st)) {
	ERR_ST_EXT("pts_GroupRename", st);
    }

    return 0;
}

int
DoPtsGroupModify(struct cmd_syndesc *as, void *arock)
{
    typedef enum { GROUP, LISTSTATUS, LISTGROUPSOWNED, LISTMEMBERSHIP,
	LISTADD, LISTDELTE
    } DoPtsGroupModify_parm_t;
    afs_status_t st = 0;
    const char *group = as->parms[GROUP].items->data;
    pts_GroupUpdateEntry_t entry;

    entry.listStatus =
	GetGroupAccessFromString(as->parms[LISTSTATUS].items->data,
				 "invalid permission specifier");
    entry.listGroupsOwned =
	GetGroupAccessFromString(as->parms[LISTGROUPSOWNED].items->data,
				 "invalid permission specifier");
    entry.listMembership =
	GetGroupAccessFromString(as->parms[LISTMEMBERSHIP].items->data,
				 "invalid permission specifier");
    entry.listAdd =
	GetGroupAccessFromString(as->parms[LISTADD].items->data,
				 "invalid permission specifier");
    entry.listDelete =
	GetGroupAccessFromString(as->parms[LISTDELTE].items->data,
				 "invalid permission specifier");

    if (!pts_GroupModify(cellHandle, group, &entry, &st)) {
	ERR_ST_EXT("pts_GroupModify", st);
    }

    return 0;
}

int
DoPtsUserCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER } DoPtsUserCreate_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    int new_user_id;

    if (!pts_UserCreate(cellHandle, user, &new_user_id, &st)) {
	ERR_ST_EXT("pts_UserCreate", st);
    }

    printf("Created user id %d\n", new_user_id);

    return 0;
}

int
DoPtsUserDelete(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER } DoPtsUserDelete_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;

    if (!pts_UserDelete(cellHandle, user, &st)) {
	ERR_ST_EXT("pts_UserDelete", st);
    }

    return 0;
}

static void
Print_pts_userAccess_p(pts_userAccess_p access, const char *prefix)
{
    if (*access == PTS_USER_OWNER_ACCESS) {
	printf("%sPTS_USER_OWNER_ACCESS\n", prefix);
    } else if (*access == PTS_USER_ANYUSER_ACCESS) {
	printf("%sPTS_USER_ANYUSER_ACCESS\n", prefix);
    }
}

static void
Print_pts_UserEntry_p(pts_UserEntry_p entry, const char *prefix)
{
    printf("%sName %s Uid %d\n", prefix, entry->name, entry->nameUid);
    printf("%sOwner %s Uid %d\n", prefix, entry->owner, entry->ownerUid);
    printf("%sCreator %s Uid %d\n", prefix, entry->creator,
	   entry->creatorUid);
    printf("%sGroup creation quota %d\n", prefix, entry->groupCreationQuota);
    printf("%sGroup membership count %d\n", prefix,
	   entry->groupMembershipCount);
    printf("%sList status permission\n", prefix);
    Print_pts_userAccess_p(&entry->listStatus, "    ");
    printf("%sList groups owned permission\n", prefix);
    Print_pts_userAccess_p(&entry->listGroupsOwned, "    ");
    printf("%sList membership permission\n", prefix);
    Print_pts_userAccess_p(&entry->listMembership, "    ");
    printf("%s\n", prefix);
}

int
DoPtsUserGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER } DoPtsUserGet_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    pts_UserEntry_t entry;

    if (!pts_UserGet(cellHandle, user, &entry, &st)) {
	ERR_ST_EXT("pts_UserGet", st);
    }

    Print_pts_UserEntry_p(&entry, "");

    return 0;
}

int
DoPtsUserRename(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER, NEWNAME } DoPtsUserRename_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    const char *new_name = as->parms[NEWNAME].items->data;

    if (!pts_UserRename(cellHandle, user, new_name, &st)) {
	ERR_ST_EXT("pts_UserRename", st);
    }

    return 0;
}

int
DoPtsUserModify(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER, GROUPQUOTA, LISTSTATUS, LISTGROUPSOWNED,
	LISTMEMBERSHIP
    } DoPtsUserModify_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    pts_UserUpdateEntry_t entry;
    int have_quota = 0;
    int have_list_status_perm = 0;
    int have_list_groups_owned_perm = 0;
    int have_list_membership_perm = 0;

    entry.flag = 0;

    if (as->parms[GROUPQUOTA].items) {
	entry.groupCreationQuota =
	    GetIntFromString(as->parms[GROUPQUOTA].items->data, "bad quota");
	entry.flag = PTS_USER_UPDATE_GROUP_CREATE_QUOTA;
	have_quota = 1;
    }

    if (as->parms[LISTSTATUS].items) {
	entry.listStatus =
	    GetUserAccessFromString(as->parms[LISTSTATUS].items->data,
				    "invalid permission specifier");
	entry.flag |= PTS_USER_UPDATE_PERMISSIONS;
	have_list_status_perm = 1;
    }

    if (as->parms[LISTGROUPSOWNED].items) {
	entry.listGroupsOwned =
	    GetUserAccessFromString(as->parms[LISTGROUPSOWNED].items->data,
				    "invalid permission specifier");
	entry.flag |= PTS_USER_UPDATE_PERMISSIONS;
	have_list_groups_owned_perm = 1;
    }

    if (as->parms[LISTMEMBERSHIP].items) {
	entry.listMembership =
	    GetUserAccessFromString(as->parms[LISTMEMBERSHIP].items->data,
				    "invalid permission specifier");
	entry.flag |= PTS_USER_UPDATE_PERMISSIONS;
	have_list_membership_perm = 1;
    }

    if (entry.flag == 0) {
	ERR_EXT("you must specify either quota or permissions to modify");
    } else {
	if (entry.flag & PTS_USER_UPDATE_PERMISSIONS) {
	    if ((have_list_status_perm + have_list_groups_owned_perm +
		 have_list_membership_perm) != 3) {
		ERR_EXT("you must completely specify all permissions "
			"when modifying a user");
	    }
	}
    }

    if (!pts_UserModify(cellHandle, user, &entry, &st)) {
	ERR_ST_EXT("pts_UserModify", st);
    }

    return 0;
}

int
DoPtsUserMaxGet(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    int max_user_id;

    if (!pts_UserMaxGet(cellHandle, &max_user_id, &st)) {
	ERR_ST_EXT("pts_UserMaxGet", st);
    }

    return 0;
}

int
DoPtsUserMaxSet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { MAX } DoPtsUserMaxSet_parm_t;
    afs_status_t st = 0;
    const char *max = as->parms[MAX].items->data;
    int max_user_id;

    max_user_id = GetIntFromString(max, "bad user id");

    if (!pts_UserMaxSet(cellHandle, max_user_id, &st)) {
	ERR_ST_EXT("pts_UserMaxSet", st);
    }

    return 0;
}

int
DoPtsUserMemberList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER } DoPtsUserMemberList_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    void *iter;
    char group[PTS_MAX_NAME_LEN];

    if (!pts_UserMemberListBegin(cellHandle, user, &iter, &st)) {
	ERR_ST_EXT("pts_UserMemberListBegin", st);
    }

    printf("Listing group membership for %s\n", user);

    while (pts_UserMemberListNext(iter, group, &st)) {
	printf("\t%s\n", group);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("pts_UserMemberListNext", st);
    }

    if (!pts_UserMemberListDone(iter, &st)) {
	ERR_ST_EXT("pts_UserMemberListDone", st);
    }

    return 0;
}

int
DoPtsOwnedGroupList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { USER } DoPtsOwnedGroupList_parm_t;
    afs_status_t st = 0;
    const char *user = as->parms[USER].items->data;
    void *iter;
    char group[PTS_MAX_NAME_LEN];

    if (!pts_OwnedGroupListBegin(cellHandle, user, &iter, &st)) {
	ERR_ST_EXT("pts_OwnedGroupListBegin", st);
    }

    printf("Listing groups owned by %s\n", user);

    while (pts_OwnedGroupListNext(iter, group, &st)) {
	printf("\t%s\n", group);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("pts_OwnedGroupListNext", st);
    }

    if (!pts_OwnedGroupListDone(iter, &st)) {
	ERR_ST_EXT("pts_OwnedGroupListDone", st);
    }

    return 0;
}

void
SetupPtsAdminCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("PtsGroupMemberAdd", DoPtsGroupMemberAdd, NULL,
			  "add a user to a group");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to add");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to modify");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupOwnerChange", DoPtsGroupOwnerChange, NULL,
			  "change the owner of a group");
    cmd_AddParm(ts, "-owner", CMD_SINGLE, CMD_REQUIRED, "new owner");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to modify");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupCreate", DoPtsGroupCreate, NULL,
			  "create a new group");
    cmd_AddParm(ts, "-owner", CMD_SINGLE, CMD_REQUIRED, "owner of group");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to create");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupGet", DoPtsGroupGet, NULL,
			  "get information about a group");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupDelete", DoPtsGroupDelete, NULL,
			  "delete a group");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to delete");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupMaxGet", DoPtsGroupMaxGet, NULL,
			  "get the maximum group id");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupMaxSet", DoPtsGroupMaxSet, NULL,
			  "set the maximum group id");
    cmd_AddParm(ts, "-max", CMD_SINGLE, CMD_REQUIRED, "new max group id");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupMemberList", DoPtsGroupMemberList, NULL,
			  "list members of a group");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupMemberRemove", DoPtsGroupMemberRemove, NULL,
			  "remove a member from a group");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to remove");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to modify");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupRename", DoPtsGroupRename, NULL,
			  "rename a group");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to modify");
    cmd_AddParm(ts, "-newname", CMD_SINGLE, CMD_REQUIRED, "new group name");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsGroupModify", DoPtsGroupModify, NULL,
			  "modify a group");
    cmd_AddParm(ts, "-group", CMD_SINGLE, CMD_REQUIRED, "group to modify");
    cmd_AddParm(ts, "-liststatus", CMD_SINGLE, CMD_REQUIRED,
		"list status permission <owner | group | any>");
    cmd_AddParm(ts, "-listgroupsowned", CMD_SINGLE, CMD_REQUIRED,
		"list groups owned permission <owner | any>");
    cmd_AddParm(ts, "-listmembership", CMD_SINGLE, CMD_REQUIRED,
		"list membership permission <owner | group | any>");
    cmd_AddParm(ts, "-listadd", CMD_SINGLE, CMD_REQUIRED,
		"list add permission <owner | group | any>");
    cmd_AddParm(ts, "-listdelete", CMD_SINGLE, CMD_REQUIRED,
		"list delete permission <owner | group>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserCreate", DoPtsUserCreate, NULL,
			  "create a new user");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to create");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserDelete", DoPtsUserDelete, NULL,
			  "delete a user");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to delete");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserGet", DoPtsUserGet, NULL,
			  "get information about a user");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserRename", DoPtsUserRename, NULL,
			  "rename a user");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to modify");
    cmd_AddParm(ts, "-newname", CMD_SINGLE, CMD_REQUIRED, "new user name");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserModify", DoPtsUserModify, NULL,
			  "change a user");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to modify");
    cmd_AddParm(ts, "-groupquota", CMD_SINGLE, CMD_OPTIONAL,
		"group creation quota");
    cmd_AddParm(ts, "-liststatus", CMD_SINGLE, CMD_OPTIONAL,
		"list status permission <owner | any>");
    cmd_AddParm(ts, "-listgroupsowned", CMD_SINGLE, CMD_OPTIONAL,
		"list groups owned permission <owner | any>");
    cmd_AddParm(ts, "-listmembership", CMD_SINGLE, CMD_OPTIONAL,
		"list membership permission <owner | any>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserMaxGet", DoPtsUserMaxGet, NULL,
			  "get the max user id");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserMaxSet", DoPtsUserMaxSet, NULL,
			  "set the max user id");
    cmd_AddParm(ts, "-max", CMD_SINGLE, CMD_REQUIRED, "max user id");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsUserMemberList", DoPtsUserMemberList, NULL,
			  "list group membership for a user");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("PtsOwnedGroupList", DoPtsOwnedGroupList, NULL,
			  "list groups owned by a user");
    cmd_AddParm(ts, "-user", CMD_SINGLE, CMD_REQUIRED, "user to query");
    SetupCommonCmdArgs(ts);
}
