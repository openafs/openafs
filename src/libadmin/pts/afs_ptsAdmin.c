/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/stds.h>
#include "afs_ptsAdmin.h"
#include "../adminutil/afs_AdminInternal.h"
#include <afs/afs_AdminErrors.h>
#include <afs/afs_utilAdmin.h>
#include <afs/ptint.h>
#include <afs/ptserver.h>

/*
 * IsValidCellHandle - validate the cell handle for making pts
 * requests.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that is to be validated.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
IsValidCellHandle(const afs_cell_handle_p c_handle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (!CellHandleIsValid((void *)c_handle, &tst)) {
	goto fail_IsValidCellHandle;
    }

    if (c_handle->pts_valid == 0) {
	tst = ADMCLIENTCELLPTSINVALID;
	goto fail_IsValidCellHandle;
    }

    if (c_handle->pts == NULL) {
	tst = ADMCLIENTCELLPTSNULL;
	goto fail_IsValidCellHandle;
    }
    rc = 1;


  fail_IsValidCellHandle:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * TranslatePTSNames - translate character representations of pts names
 * into their numeric equivalent.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the id's exist.
 *
 * IN names - the list of names to be translated.
 *
 * OUT ids - the list of translated names
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
TranslatePTSNames(const afs_cell_handle_p cellHandle, namelist * names,
		  idlist * ids, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int i;
    char *p;

    /*
     * Lowercase the names to translate
     */

    for (i = 0; i < names->namelist_len; i++) {
	p = names->namelist_val[i];
	while (*p) {
	    *p = tolower(*p);
	    p++;
	}
    }

    tst = ubik_PR_NameToID(cellHandle->pts, 0, names, ids);

    if (tst) {
	goto fail_TranslatePTSNames;
    }


    /*
     * Check to see if the lookup failed
     */

    for (i = 0; i < ids->idlist_len; i++) {
	if (ids->idlist_val[i] == ANONYMOUSID) {
	    tst = ADMPTSFAILEDNAMETRANSLATE;
	    goto fail_TranslatePTSNames;
	}
    }
    rc = 1;

  fail_TranslatePTSNames:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * TranslateTwoNames - translate two pts names to their pts ids.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN id1 - one id to be translated
 *
 * IN error1 - the error status to be returned in the event that id1 is
 * too long.
 *
 * IN id2 - one id to be translated
 *
 * IN error2 - the error status to be returned in the event that id2 is
 * too long.
 *
 *
 * OUT idlist - the list of pts id's
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
TranslateTwoNames(const afs_cell_handle_p c_handle, const char *id1,
		  afs_status_t error1, const char *id2, afs_status_t error2,
		  idlist * ids, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    namelist names;
    char tmp_array[2 * PTS_MAX_NAME_LEN];

    /*
     * Copy the group and user names in order to translate them
     */

    names.namelist_len = 2;
    names.namelist_val = (prname *) & tmp_array[0];

    strncpy(names.namelist_val[0], id1, PTS_MAX_NAME_LEN);
    names.namelist_val[0][PTS_MAX_NAME_LEN - 1] = '\0';
    strncpy(names.namelist_val[1], id2, PTS_MAX_NAME_LEN);
    names.namelist_val[1][PTS_MAX_NAME_LEN - 1] = '\0';
    ids->idlist_val = 0;
    ids->idlist_len = 0;

    /*
     * Check that user and group aren't too long
     * This is a cheaper check than calling strlen
     */

    if (names.namelist_val[0][PTS_MAX_NAME_LEN - 1] != 0) {
	tst = error1;
	goto fail_TranslateTwoNames;
    }

    if (names.namelist_val[0][PTS_MAX_NAME_LEN - 1] != 0) {
	tst = error2;
	goto fail_TranslateTwoNames;
    }

    /*
     * Translate user and group into pts ID's
     */

    if (TranslatePTSNames(c_handle, &names, ids, &tst) == 0) {
	goto fail_TranslateTwoNames;
    }
    rc = 1;


  fail_TranslateTwoNames:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * TranslateOneName - translate a pts name to its pts id.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN userName - the user to be translated.
 *
 * OUT idlist - the user pts id.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
TranslateOneName(const afs_cell_handle_p c_handle, const char *ptsName,
		 afs_status_t tooLongError, afs_int32 * ptsId,
		 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    namelist names[1];
    char tmp_array[PTS_MAX_NAME_LEN];
    idlist ids;

    /*
     * Copy the name in order to translate it
     */

    names[0].namelist_len = 1;
    names[0].namelist_val = (prname *) & tmp_array[0];

    strncpy((char *)names[0].namelist_val, ptsName, PTS_MAX_NAME_LEN);
    ((char *)names[0].namelist_val)[PTS_MAX_NAME_LEN - 1] = '\0';
    ids.idlist_val = 0;
    ids.idlist_len = 0;

    /*
     * Check that user isn't too long
     * This is a cheaper check than calling strlen
     */

    if (names[0].namelist_val[0][PTS_MAX_NAME_LEN - 1] != 0) {
	tst = tooLongError;
	goto fail_TranslateOneName;
    }

    /*
     * Translate user into pts ID
     */

    if (TranslatePTSNames(c_handle, names, &ids, &tst) == 0) {
	goto fail_TranslateOneName;
    } else {
	if (ids.idlist_val != NULL) {
	    *ptsId = *ids.idlist_val;
	    free(ids.idlist_val);
	}
    }
    rc = 1;


  fail_TranslateOneName:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * TranslatePTSIds - translate numeric representations of pts names
 * into their character equivalent.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the id's exist.
 *
 * IN ids - the list of ids to be translated.
 *
 * OUT names - the list of translated names
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
TranslatePTSIds(const afs_cell_handle_p cellHandle, namelist * names,
		idlist * ids, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    tst = ubik_PR_IDToName(cellHandle->pts, 0, ids, names);

    if (tst) {
	goto fail_TranslatePTSIds;
    }
    rc = 1;

  fail_TranslatePTSIds:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupMemberAdd - add one member to a pts group
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN userName - the name to be added to the group.
 *
 * IN groupName - the group to be modified.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupMemberAdd(const void *cellHandle, const char *userName,
		   const char *groupName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
	idlist ids = {0,0};

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupMemberAdd;
    }

    if ((userName == NULL) || (*userName == 0)) {
	tst = ADMPTSUSERNAMENULL;
	goto fail_pts_GroupMemberAdd;
    }

    if ((groupName == NULL) || (*groupName == 0)) {
	tst = ADMPTSGROUPNAMENULL;
	goto fail_pts_GroupMemberAdd;
    }

    if (!TranslateTwoNames
	(c_handle, userName, ADMPTSUSERNAMETOOLONG, groupName,
	 ADMPTSGROUPNAMETOOLONG, &ids, &tst)) {
	goto fail_pts_GroupMemberAdd;
    }

    /*
     * Make the rpc
     */

    tst =
	ubik_PR_AddToGroup(c_handle->pts, 0, ids.idlist_val[0],
		  ids.idlist_val[1]);

    if (tst != 0) {
	goto fail_pts_GroupMemberAdd;
    }
    rc = 1;

  fail_pts_GroupMemberAdd:

    if (ids.idlist_val != 0) {
	free(ids.idlist_val);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupOwnerChange - change the owner of a group
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN targetGroup - the group to be modified.
 *
 * IN userName - the new owner of the group.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupOwnerChange(const void *cellHandle, const char *targetGroup,
		     const char *newOwner, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    idlist ids;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupOwnerChange;
    }

    if ((newOwner == NULL) || (*newOwner == 0)) {
	tst = ADMPTSNEWOWNERNULL;
	goto fail_pts_GroupOwnerChange;
    }

    if ((targetGroup == NULL) || (*targetGroup == 0)) {
	tst = ADMPTSTARGETGROUPNULL;
	goto fail_pts_GroupOwnerChange;
    }

    if (!TranslateTwoNames
	(c_handle, newOwner, ADMPTSNEWOWNERTOOLONG, targetGroup,
	 ADMPTSTARGETGROUPTOOLONG, &ids, &tst)) {
	goto fail_pts_GroupOwnerChange;
    }

    /*
     * Make the rpc
     */

    tst =
	ubik_PR_ChangeEntry(c_handle->pts, 0, ids.idlist_val[1], "",
		  ids.idlist_val[0], 0);

    if (tst != 0) {
	goto fail_pts_GroupOwnerChange;
    }
    rc = 1;

  fail_pts_GroupOwnerChange:

    if (ids.idlist_val != 0) {
	free(ids.idlist_val);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupCreate - create a new group
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN newGroup - the group to be created.
 *
 * IN newOwner - the owner of the group.  Pass NULL if the current user
 * is to be the new owner, or the character string of the owner otherwise.
 *
 * IN/OUT newGroupId - the pts id of the group.  Pass 0 to have ptserver 
 * generate a value, != 0 to assign a value on your own.  The group id
 * that is used to create the group is copied into this parameter in the
 * event you pass in 0.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupCreate(const void *cellHandle, const char *newGroup,
		const char *newOwner, int *newGroupId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 newOwnerId = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupCreate;
    }

    if ((newGroup == NULL) || (*newGroup == 0)) {
	tst = ADMPTSNEWGROUPNULL;
	goto fail_pts_GroupCreate;
    }

    if (newGroupId == NULL) {
	tst = ADMPTSNEWGROUPIDNULL;
	goto fail_pts_GroupCreate;
    }

    if (*newGroupId > 0) {
	tst = ADMPTSNEWGROUPIDPOSITIVE;
	goto fail_pts_GroupCreate;
    }

    /*
     * If a newOwner was specified, validate that it exists
     */

    if (newOwner != NULL) {
	if (!TranslateOneName
	    (c_handle, newOwner, ADMPTSNEWOWNERTOOLONG, &newOwnerId, &tst)) {
	    goto fail_pts_GroupCreate;
	}
    }

    /*
     * We make a different rpc based upon the input to this function
     */

    if (*newGroupId != 0) {
	tst =
	    ubik_PR_INewEntry(c_handle->pts, 0, newGroup, *newGroupId,
		      newOwnerId);
    } else {
	tst =
	    ubik_PR_NewEntry(c_handle->pts, 0, newGroup, PRGRP,
		      newOwnerId, newGroupId);
    }

    if (tst != 0) {
	goto fail_pts_GroupCreate;
    }
    rc = 1;

  fail_pts_GroupCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * GetGroupAccess - a small convenience function for setting
 * permissions.
 *
 * PARAMETERS
 *
 * IN access - a pointer to a pts_groupAccess_t to be set with the
 * correct permission.
 *
 * IN flag - the current permission flag used to derive the permission.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Since this function cannot fail, it returns void.
 *
 */

static void
GetGroupAccess(pts_groupAccess_p access, afs_int32 flag)
{

    *access = PTS_GROUP_OWNER_ACCESS;
    if (flag == 0) {
	*access = PTS_GROUP_OWNER_ACCESS;
    } else if (flag == 1) {
	*access = PTS_GROUP_ACCESS;
    } else if (flag == 2) {
	*access = PTS_GROUP_ANYUSER_ACCESS;
    }
}


/*
 * pts_GroupGet - retrieve information about a particular group.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN groupName - the group to retrieve.
 *
 * OUT groupP - a pointer to a pts_GroupEntry_t structure that upon
 * successful completion is filled with information about groupName.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupGet(const void *cellHandle, const char *groupName,
	     pts_GroupEntry_p groupP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 groupId = 0;
    afs_int32 flags;
    afs_int32 twobit;
    struct prcheckentry groupEntry;
    idlist ids;
    afs_int32 ptsids[2];
    namelist names;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupGet;
    }

    if ((groupName == NULL) || (*groupName == 0)) {
	tst = ADMPTSGROUPNAMENULL;
	goto fail_pts_GroupGet;
    }

    if (groupP == NULL) {
	tst = ADMPTSGROUPPNULL;
	goto fail_pts_GroupGet;
    }

    /*
     * Translate the group name into an id.
     */

    if (!TranslateOneName
	(c_handle, groupName, ADMPTSGROUPNAMETOOLONG, &groupId, &tst)) {
	goto fail_pts_GroupGet;
    }

    /*
     * Retrieve information about the group
     */

    tst = ubik_PR_ListEntry(c_handle->pts, 0, groupId, &groupEntry);

    if (tst != 0) {
	goto fail_pts_GroupGet;
    }

    groupP->membershipCount = groupEntry.count;
    groupP->nameUid = groupEntry.id;
    groupP->ownerUid = groupEntry.owner;
    groupP->creatorUid = groupEntry.creator;
    strncpy(groupP->name, groupEntry.name, PTS_MAX_NAME_LEN);
    groupP->name[PTS_MAX_NAME_LEN - 1] = '\0';
    /*
     * Set the access rights based upon the value of the flags member
     * of the groupEntry struct.
     *
     * To the best of my ability to decypher the pts code, it looks like
     * the rights are stored in flags as follows:
     *
     * I number my bits from least significant to most significant starting
     * with 0.
     *
     * remove - bit 0
     *     if bit 0 == 0 -> r access is denied
     *     if bit 0 == 1 -> r access is granted
     *
     * add - bits 1 and 2
     *     if bit 2 == 0 and bit 1 == 0 -> a access is denied
     *     if bit 2 == 0 and bit 1 == 1 -> a access is granted
     *     if bit 2 == 1 and bit 1 == 0 -> A access is granted
     *     if bit 2 == 1 and bit 1 == 1 -> this is an error
     *
     * membership - bits 3 and 4
     *     if bit 4 == 0 and bit 3 == 0 -> m access is denied
     *     if bit 4 == 0 and bit 3 == 1 -> m access is granted
     *     if bit 4 == 1 and bit 3 == 0 -> M access is granted
     *     if bit 4 == 1 and bit 3 == 1 -> this is an error
     *
     * owned - bit 5
     *     if bit 5 == 0 -> O access is denied
     *     if bit 5 == 1 -> O access is granted
     *
     * status - bits 6 and 7
     *     if bit 7 == 0 and bit 6 == 0 -> s access is denied
     *     if bit 7 == 0 and bit 6 == 1 -> s access is granted
     *     if bit 7 == 1 and bit 6 == 0 -> S access is granted
     *     if bit 7 == 1 and bit 6 == 1 -> this is an error
     *
     * For cases where the permission doesn't make sense for the
     * type of entry, or where an error occurs, we ignore it.
     * This is the behavior of the pts code.
     */

    flags = groupEntry.flags;
    if (flags & 1) {
	groupP->listDelete = PTS_GROUP_ACCESS;
    } else {
	groupP->listDelete = PTS_GROUP_OWNER_ACCESS;
    }

    flags = flags >> 1;
    twobit = flags & 3;

    GetGroupAccess(&groupP->listAdd, twobit);

    flags = flags >> 2;
    twobit = flags & 3;

    GetGroupAccess(&groupP->listMembership, twobit);

    flags = flags >> 2;

    if (flags & 1) {
	groupP->listGroupsOwned = PTS_GROUP_ANYUSER_ACCESS;
    } else {
	groupP->listGroupsOwned = PTS_GROUP_OWNER_ACCESS;
    }

    flags = flags >> 1;
    twobit = flags & 3;

    GetGroupAccess(&groupP->listStatus, twobit);

    /* 
     * Make another rpc and translate the owner and creator ids into
     * character strings.
     */

    ids.idlist_len = 2;
    ids.idlist_val = ptsids;
    ptsids[0] = groupEntry.owner;
    ptsids[1] = groupEntry.creator;
    names.namelist_len = 0;
    names.namelist_val = 0;


    if (!TranslatePTSIds(c_handle, &names, &ids, &tst)) {
	goto fail_pts_GroupGet;
    }

    strncpy(groupP->owner, names.namelist_val[0], PTS_MAX_NAME_LEN);
    groupP->owner[PTS_MAX_NAME_LEN - 1] = '\0';
    strncpy(groupP->creator, names.namelist_val[1], PTS_MAX_NAME_LEN);
    groupP->creator[PTS_MAX_NAME_LEN - 1] = '\0';
    free(names.namelist_val);
    rc = 1;

  fail_pts_GroupGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * EntryDelete - delete a pts entry (group or user).
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN entryName - the entry to be deleted.
 *
 * IN error1 - the error status to be returned in the event that entryName is
 * null.
 *
 * IN error2 - the error status to be returned in the event that entryName is
 * too long.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
EntryDelete(const void *cellHandle, const char *entryName,
	    afs_status_t error1, afs_status_t error2, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 entryId = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_EntryDelete;
    }

    if ((entryName == NULL) || (*entryName == 0)) {
	tst = error1;
	goto fail_EntryDelete;
    }

    /*
     * Translate the entry name into an id.
     */

    if (!TranslateOneName(c_handle, entryName, error2, &entryId, &tst)) {
	goto fail_EntryDelete;
    }

    /*
     * Make the rpc
     */

    tst = ubik_PR_Delete(c_handle->pts, 0, entryId);

    if (tst != 0) {
	goto fail_EntryDelete;
    }
    rc = 1;

  fail_EntryDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * pts_GroupDelete - delete a group
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN groupName - the group to be deleted.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupDelete(const void *cellHandle, const char *groupName,
		afs_status_p st)
{

    return EntryDelete(cellHandle, groupName, ADMPTSGROUPNAMENULL,
		       ADMPTSGROUPNAMETOOLONG, st);
}

/*
 * pts_GroupMaxGet - get the maximum in use group id.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * OUT maxGroupId - upon successful completion contains the maximum
 * group Id in use at the server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupMaxGet(const void *cellHandle, int *maxGroupId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 maxUserId = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupMaxGet;
    }

    if (maxGroupId == NULL) {
	tst = ADMPTSMAXGROUPIDNULL;
	goto fail_pts_GroupMaxGet;
    }

    tst = ubik_PR_ListMax(c_handle->pts, 0, &maxUserId, maxGroupId);

    if (tst != 0) {
	goto fail_pts_GroupMaxGet;
    }
    rc = 1;

  fail_pts_GroupMaxGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupMaxSet - set the maximum in use group id.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN maxGroupId - the new maximum group id.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupMaxSet(const void *cellHandle, int maxGroupId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupMaxSet;
    }

    tst = ubik_PR_SetMax(c_handle->pts, 0, maxGroupId, PRGRP);

    if (tst != 0) {
	goto fail_pts_GroupMaxSet;
    }
    rc = 1;

  fail_pts_GroupMaxSet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * NOTE
 *
 * I'm not using the common iterator pattern here since the retrival
 * of the member list is actually accomplished in 1 rpc.  There's no
 * sense in trying to fit this pts specific behavior into the more
 * generic model, so instead the Begin functions actually do all the
 * rpc work and the next/done functions just manipulate the retrieved
 * data.
 */

typedef struct pts_group_member_list_iterator {
    int begin_magic;
    int is_valid;
    pthread_mutex_t mutex;	/* hold to manipulate this structure */
    prlist ids;
    namelist names;
    int index;
    int end_magic;
} pts_group_member_list_iterator_t, *pts_group_member_list_iterator_p;

/*
 * pts_GroupMemberListBegin - begin iterating over the list of members
 * of a particular group.
 *
 * PARAMETERS
 *
 * IN iter - an iterator previously returned by pts_GroupMemberListBegin
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
IsValidPtsGroupMemberListIterator(pts_group_member_list_iterator_p iter,
				  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_IsValidPtsGroupMemberListIterator;
    }

    if ((iter->begin_magic != BEGIN_MAGIC) || (iter->end_magic != END_MAGIC)) {
	tst = ADMITERATORBADMAGICNULL;
	goto fail_IsValidPtsGroupMemberListIterator;
    }

    if (iter->is_valid == 0) {
	tst = ADMITERATORINVALID;
	goto fail_IsValidPtsGroupMemberListIterator;
    }
    rc = 1;

  fail_IsValidPtsGroupMemberListIterator:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * MemberListBegin - an internal function which is used to get both
 * the list of members in a group and the list of groups a user belongs
 * to.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN name - the name whose membership will be retrieved.
 *
 * OUT iterationIdP - upon successful completion contains a iterator that
 * can be passed to pts_GroupMemberListNext or pts_UserMemberListNext
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
MemberListBegin(const void *cellHandle, const char *name, afs_status_t error1,
		afs_status_t error2, void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 groupId = 0;
    afs_int32 exceeded = 0;
    pts_group_member_list_iterator_p iter = (pts_group_member_list_iterator_p)
	malloc(sizeof(pts_group_member_list_iterator_t));
    int iter_allocated = 0;
    int ids_allocated = 0;
    int names_allocated = 0;
    int mutex_inited = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_MemberListBegin;
    }

    if ((name == NULL) || (*name == 0)) {
	tst = ADMPTSGROUPNAMENULL;
	goto fail_MemberListBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATORNULL;
	goto fail_MemberListBegin;
    }

    if (iter == NULL) {
	tst = ADMNOMEM;
	goto fail_MemberListBegin;
    }

    iter_allocated = 1;

    /*
     * Translate the name into an id.
     */

    if (!TranslateOneName
	(c_handle, name, ADMPTSGROUPNAMETOOLONG, &groupId, &tst)) {
	goto fail_MemberListBegin;
    }

    if (pthread_mutex_init(&iter->mutex, 0)) {
	tst = ADMMUTEXINIT;
	goto fail_MemberListBegin;
    }

    mutex_inited = 1;

    iter->ids.prlist_len = 0;
    iter->ids.prlist_val = 0;

    tst =
	ubik_PR_ListElements(c_handle->pts, 0, groupId, &iter->ids,
		  &exceeded);

    if (tst != 0) {
	goto fail_MemberListBegin;
    }

    if (exceeded != 0) {
	tst = ADMPTSGROUPMEMEXCEEDED;
	goto fail_MemberListBegin;
    }

    ids_allocated = 1;
    iter->names.namelist_len = 0;
    iter->names.namelist_val = 0;

    if (!TranslatePTSIds
	(c_handle, &iter->names, (idlist *) & iter->ids, &tst)) {
	goto fail_MemberListBegin;
    }

    names_allocated = 1;
    iter->begin_magic = BEGIN_MAGIC;
    iter->end_magic = END_MAGIC;
    iter->index = 0;
    iter->is_valid = 1;

    *iterationIdP = (void *)iter;
    rc = 1;

  fail_MemberListBegin:

    if (ids_allocated) {
	free(iter->ids.prlist_val);
    }

    if (rc == 0) {
	if (names_allocated) {
	    free(iter->names.namelist_val);
	}
	if (mutex_inited) {
	    pthread_mutex_destroy(&iter->mutex);
	}
	if (iter_allocated) {
	    free(iter);
	}
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * pts_GroupMemberListBegin - begin iterating over the list of members
 * of a particular group.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN groupName - the group whose members will be returned.
 *
 * OUT iterationIdP - upon successful completion contains a iterator that
 * can be passed to pts_GroupMemberListNext.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupMemberListBegin(const void *cellHandle, const char *groupName,
			 void **iterationIdP, afs_status_p st)
{
    return MemberListBegin(cellHandle, groupName, ADMPTSGROUPNAMENULL,
			   ADMPTSGROUPNAMETOOLONG, iterationIdP, st);
}

/*
 * pts_GroupMemberListNext - get the next member of a group
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_GroupMemberListBegin
 *
 * OUT memberName - upon successful completion contains the next member of
 * a group.
 *
 * LOCKS
 *
 * The iterator mutex is held during the retrieval of the next member.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupMemberListNext(const void *iterationId, char *memberName,
			afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    pts_group_member_list_iterator_p iter =
	(pts_group_member_list_iterator_p) iterationId;
    int mutex_locked = 0;

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_GroupMemberListNext;
    }

    if (memberName == NULL) {
	tst = ADMPTSMEMBERNAMENULL;
	goto fail_pts_GroupMemberListNext;
    }

    /*
     * Lock the mutex and check the validity of the iterator
     */

    if (pthread_mutex_lock(&iter->mutex)) {
	tst = ADMMUTEXLOCK;
	goto fail_pts_GroupMemberListNext;
    }

    mutex_locked = 1;

    if (!IsValidPtsGroupMemberListIterator(iter, &tst)) {
	goto fail_pts_GroupMemberListNext;
    }

    /*
     * Check to see if we've copied out all the data.  If we haven't,
     * copy another item.  If we have, mark the iterator done.
     */

    if (iter->index >= iter->names.namelist_len) {
	tst = ADMITERATORDONE;
	goto fail_pts_GroupMemberListNext;
    } else {
	strcpy(memberName, iter->names.namelist_val[iter->index]);
	iter->index++;
    }
    rc = 1;

  fail_pts_GroupMemberListNext:

    if (mutex_locked) {
	pthread_mutex_unlock(&iter->mutex);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupMemberListDone - finish using a member list iterator
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_GroupMemberListBegin
 *
 * LOCKS
 *
 * The iterator is locked and then destroyed
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * It is the user's responsibility to make sure pts_GroupMemberListDone
 * is called only once for each iterator.
 */

int ADMINAPI
pts_GroupMemberListDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    pts_group_member_list_iterator_p iter =
	(pts_group_member_list_iterator_p) iterationId;
    int mutex_locked = 0;

    /*
     * Validate arguments
     */

    if (iter == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_GroupMemberListDone;
    }

    /*
     * Lock the mutex and check the validity of the iterator
     */

    if (pthread_mutex_lock(&iter->mutex)) {
	tst = ADMMUTEXLOCK;
	goto fail_pts_GroupMemberListDone;
    }

    mutex_locked = 1;

    if (!IsValidPtsGroupMemberListIterator(iter, &tst)) {
	goto fail_pts_GroupMemberListDone;
    }

    /*
     * Free the namelist and the iterator.
     */

    pthread_mutex_destroy(&iter->mutex);
    mutex_locked = 0;
    iter->is_valid = 0;
    free(iter->names.namelist_val);
    free(iter);
    rc = 1;

  fail_pts_GroupMemberListDone:

    if (mutex_locked) {
	pthread_mutex_unlock(&iter->mutex);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupMemberRemove - remove a member from a group.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN userName - the user to remove.
 *
 * IN groupName - the group to modify
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupMemberRemove(const void *cellHandle, const char *userName,
		      const char *groupName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    idlist ids;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupMemberRemove;
    }

    if ((userName == NULL) || (*userName == 0)) {
	tst = ADMPTSUSERNAMENULL;
	goto fail_pts_GroupMemberRemove;
    }

    if ((groupName == NULL) || (*groupName == 0)) {
	tst = ADMPTSGROUPNAMENULL;
	goto fail_pts_GroupMemberRemove;
    }

    if (!TranslateTwoNames
	(c_handle, userName, ADMPTSUSERNAMETOOLONG, groupName,
	 ADMPTSGROUPNAMETOOLONG, &ids, &tst)) {
	goto fail_pts_GroupMemberRemove;
    }

    /*
     * Make the rpc
     */

    tst =
	ubik_PR_RemoveFromGroup(c_handle->pts, 0, ids.idlist_val[0],
		  ids.idlist_val[1]);

    if (tst != 0) {
	goto fail_pts_GroupMemberRemove;
    }
    rc = 1;

  fail_pts_GroupMemberRemove:

    if (ids.idlist_val != 0) {
	free(ids.idlist_val);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupRename - change the name of a group
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN oldName - the current group name
 *
 * IN newName - the new group name
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupRename(const void *cellHandle, const char *oldName,
		const char *newName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 groupId = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupRename;
    }

    if ((newName == NULL) || (*newName == 0)) {
	tst = ADMPTSNEWNAMENULL;
	goto fail_pts_GroupRename;
    }

    if ((oldName == NULL) || (*oldName == 0)) {
	tst = ADMPTSOLDNAMENULL;
	goto fail_pts_GroupRename;
    }

    /*
     * Translate the group name into an id.
     */

    if (!TranslateOneName
	(c_handle, oldName, ADMPTSOLDNAMETOOLONG, &groupId, &tst)) {
	goto fail_pts_GroupRename;
    }

    /*
     * Make the rpc
     */

    tst = ubik_PR_ChangeEntry(c_handle->pts, 0, groupId, newName, 0, 0);

    if (tst != 0) {
	goto fail_pts_GroupRename;
    }
    rc = 1;

  fail_pts_GroupRename:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * SetGroupAccess - translate our Access notation to pts flags.
 *
 * PARAMETERS
 *
 * IN rights - the permissions.
 *
 * OUT flags - a pointer to an afs_int32 structure that 
 * contains the flags to pass to pts.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
SetGroupAccess(const pts_GroupUpdateEntry_p rights, afs_int32 * flags,
	       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    *flags = 0;

    if (rights->listDelete == PTS_GROUP_ACCESS) {
	*flags |= 1;
    } else if (rights->listDelete == PTS_GROUP_ANYUSER_ACCESS) {
	tst = ADMPTSINVALIDGROUPDELETEPERM;
	goto fail_SetGroupAccess;
    }

    if (rights->listAdd == PTS_GROUP_ACCESS) {
	*flags |= 2;
    } else if (rights->listAdd == PTS_GROUP_ANYUSER_ACCESS) {
	*flags |= 4;
    }

    if (rights->listMembership == PTS_GROUP_ACCESS) {
	*flags |= 8;
    } else if (rights->listMembership == PTS_GROUP_ANYUSER_ACCESS) {
	*flags |= 16;
    }

    if (rights->listGroupsOwned == PTS_GROUP_ANYUSER_ACCESS) {
	*flags |= 32;
    } else if (rights->listGroupsOwned == PTS_GROUP_ACCESS) {
	tst = ADMPTSINVALIDGROUPSOWNEDPERM;
	goto fail_SetGroupAccess;
    }

    if (rights->listStatus == PTS_GROUP_ACCESS) {
	*flags |= 64;
    } else if (rights->listStatus == PTS_GROUP_ANYUSER_ACCESS) {
	*flags |= 128;
    }
    rc = 1;

  fail_SetGroupAccess:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupModify - change the contents of a group entry.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN groupName - the group to change
 *
 * OUT newEntryP - a pointer to a pts_GroupUpdateEntry_t structure that 
 * contains the new information for the group.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupModify(const void *cellHandle, const char *groupName,
		const pts_GroupUpdateEntry_p newEntryP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 groupId = 0;
    afs_int32 flags = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupModify;
    }

    if ((groupName == NULL) || (*groupName == 0)) {
	tst = ADMPTSGROUPNAMENULL;
	goto fail_pts_GroupModify;
    }


    if (newEntryP == NULL) {
	tst = ADMPTSNEWENTRYPNULL;
	goto fail_pts_GroupModify;
    }

    /*
     * Translate the group name into an id.
     */

    if (!TranslateOneName
	(c_handle, groupName, ADMPTSGROUPNAMETOOLONG, &groupId, &tst)) {
	goto fail_pts_GroupModify;
    }

    /*
     * Set the flags argument
     */

    if (!SetGroupAccess(newEntryP, &flags, &tst)) {
	goto fail_pts_GroupModify;
    }

    /*
     * Make the rpc
     */

    tst =
	ubik_PR_SetFieldsEntry(c_handle->pts, 0, groupId, PR_SF_ALLBITS,
		  flags, 0, 0, 0, 0);

    if (tst != 0) {
	goto fail_pts_GroupModify;
    }
    rc = 1;

  fail_pts_GroupModify:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserCreate - create a new user.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN newUser - the name of the new user.
 *
 * IN newUserId - the id to assign to the new user.  Pass 0 to have the
 * id assigned by pts.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserCreate(const void *cellHandle, const char *userName, int *newUserId,
	       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 userId = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_UserCreate;
    }

    if ((userName == NULL) || (*userName == 0)) {
	tst = ADMPTSUSERNAMENULL;
	goto fail_pts_UserCreate;
    }

    if (newUserId == NULL) {
	tst = ADMPTSNEWUSERIDNULL;
	goto fail_pts_UserCreate;
    }

    /*
     * We make a different rpc based upon the input to this function
     */

    if (*newUserId != 0) {
	tst =
	    ubik_PR_INewEntry(c_handle->pts, 0, userName, *newUserId,
		      0);
    } else {
	tst =
	    ubik_PR_NewEntry(c_handle->pts, 0, userName, 0, 0,
		      newUserId);
    }

    if (tst != 0) {
	goto fail_pts_UserCreate;
    }
    rc = 1;

  fail_pts_UserCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserDelete - delete a user.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN user - the name of the user to delete.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserDelete(const void *cellHandle, const char *userName, afs_status_p st)
{
    return EntryDelete(cellHandle, userName, ADMPTSUSERNAMENULL,
		       ADMPTSUSERNAMETOOLONG, st);
}


/*
 * GetUserAccess - a small convenience function for setting
 * permissions.
 *
 * PARAMETERS
 *
 * IN access - a pointer to a pts_userAccess_t to be set with the
 * correct permission.
 *
 * IN flag - the current permission flag used to derive the permission.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Since this function cannot fail, it returns void.
 *
 */

static void
GetUserAccess(pts_userAccess_p access, afs_int32 flag)
{

    *access = PTS_USER_OWNER_ACCESS;
    if (flag == 2) {
	*access = PTS_USER_ANYUSER_ACCESS;
    }
}

/*
 * IsAdministrator - determine if a user is an administrator.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN userEntry - the user data for the user in question.
 *
 * OUT admin - set to 1 if the user is an administrator, 0 otherwise.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
IsAdministrator(const afs_cell_handle_p c_handle, afs_int32 userId,
		int *admin, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_int32 adminId = 0;
    afs_int32 isAdmin = 0;

    *admin = 0;

    if (userId == SYSADMINID) {
	*admin = 1;
    } else {
	if (!TranslateOneName
	    (c_handle, "system:administrators", ADMPTSGROUPNAMETOOLONG,
	     &adminId, &tst)) {
	    goto fail_IsAdministrator;
	}
	tst =
	    ubik_PR_IsAMemberOf(c_handle->pts, 0, userId, adminId,
		      &isAdmin);
	if (tst != 0) {
	    goto fail_IsAdministrator;
	}
	if (isAdmin) {
	    *admin = 1;
	}
    }
    rc = 1;

  fail_IsAdministrator:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserGet - retrieve information about a particular user.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN userName - the name of the user to retrieve.
 *
 * OUT userP - a pointer to a pts_UserEntry_t that is filled upon successful
 * completion.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserGet(const void *cellHandle, const char *userName,
	    pts_UserEntry_p userP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    struct prcheckentry userEntry;
    afs_int32 userId = 0;
    afs_int32 flags;
    afs_int32 twobit;
    idlist ids;
    afs_int32 ptsids[2];
    namelist names;
    int admin = 0;


    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_UserGet;
    }

    if ((userName == NULL) || (*userName == 0)) {
	tst = ADMPTSUSERNAMENULL;
	goto fail_pts_UserGet;
    }

    if (userP == NULL) {
	tst = ADMPTSUSERPNULL;
	goto fail_pts_UserGet;
    }

    /*
     * Translate the group name into an id.
     */

    if (!TranslateOneName
	(c_handle, userName, ADMPTSUSERNAMETOOLONG, &userId, &tst)) {
	goto fail_pts_UserGet;
    }

    /*
     * Retrieve information about the group
     */

    tst = ubik_PR_ListEntry(c_handle->pts, 0, userId, &userEntry);

    if (tst != 0) {
	goto fail_pts_UserGet;
    }

    userP->groupMembershipCount = userEntry.count;
    userP->groupCreationQuota = userEntry.ngroups;
    /*
     * The administrator id, or any member of "system:administrators"
     * has unlimited group creation quota.  Denote this by setting
     * quota to -1.
     */

    if (!IsAdministrator(c_handle, userEntry.id, &admin, &tst)) {
	goto fail_pts_UserGet;
    }

    if (admin != 0) {
	userP->groupCreationQuota = -1;
    }

    userP->nameUid = userEntry.id;
    userP->ownerUid = userEntry.owner;
    userP->creatorUid = userEntry.creator;
    strncpy(userP->name, userEntry.name, PTS_MAX_NAME_LEN);
    userP->name[PTS_MAX_NAME_LEN - 1] = '\0';

    /*
     * The permission bits are described in the GroupGet function above.
     * The user entry only uses 3 of the 5 permissions, so we shift
     * past the unused entries.
     */

    flags = userEntry.flags;
    flags = flags >> 3;
    twobit = flags & 3;

    GetUserAccess(&userP->listMembership, twobit);

    flags = flags >> 2;

    if (flags & 1) {
	userP->listGroupsOwned = PTS_USER_ANYUSER_ACCESS;
    } else {
	userP->listGroupsOwned = PTS_USER_OWNER_ACCESS;
    }

    flags = flags >> 1;
    twobit = flags & 3;

    GetUserAccess(&userP->listStatus, twobit);

    /* 
     * Make another rpc and translate the owner and creator ids into
     * character strings.
     */

    ids.idlist_len = 2;
    ids.idlist_val = ptsids;
    ptsids[0] = userEntry.owner;
    ptsids[1] = userEntry.creator;
    names.namelist_len = 0;
    names.namelist_val = 0;


    if (!TranslatePTSIds(c_handle, &names, &ids, &tst)) {
	goto fail_pts_UserGet;
    }

    strncpy(userP->owner, names.namelist_val[0], PTS_MAX_NAME_LEN);
    userP->owner[PTS_MAX_NAME_LEN - 1] ='\0';
    strncpy(userP->creator, names.namelist_val[1], PTS_MAX_NAME_LEN);
    userP->creator[PTS_MAX_NAME_LEN - 1] = '\0';
    free(names.namelist_val);
    rc = 1;

  fail_pts_UserGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserRename - rename a user.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN oldName - the name of the user to rename.
 *
 * IN newName - the new user name.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserRename(const void *cellHandle, const char *oldName,
	       const char *newName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 userId = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_UserRename;
    }

    if ((oldName == NULL) || (*oldName == 0)) {
	tst = ADMPTSOLDNAMENULL;
	goto fail_pts_UserRename;
    }

    if ((newName == NULL) || (*newName == 0)) {
	tst = ADMPTSNEWNAMENULL;
	goto fail_pts_UserRename;
    }

    /*
     * Translate the user name into an id.
     */

    if (!TranslateOneName
	(c_handle, oldName, ADMPTSOLDNAMETOOLONG, &userId, &tst)) {
	goto fail_pts_UserRename;
    }

    /*
     * Make the rpc
     */

    tst = ubik_PR_ChangeEntry(c_handle->pts, 0, userId, newName, 0, 0);

    if (tst != 0) {
	goto fail_pts_UserRename;
    }
    rc = 1;

  fail_pts_UserRename:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * SetUserAccess - translate our Access notation to pts flags.
 *
 * PARAMETERS
 *
 * IN userP - the user structure that contains the new permissions.
 *
 * OUT flags - a pointer to an afs_int32 structure that 
 * contains the flags to pass to pts.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

static int
SetUserAccess(const pts_UserUpdateEntry_p userP, afs_int32 * flags,
	      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    *flags = 0;

    if (userP->listMembership == PTS_USER_ANYUSER_ACCESS) {
	*flags |= 16;
    }

    if (userP->listGroupsOwned == PTS_USER_ANYUSER_ACCESS) {
	*flags |= 32;
    }

    if (userP->listStatus == PTS_USER_ANYUSER_ACCESS) {
	*flags |= 128;
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * pts_UserModify - update a user entry.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN userName - the name of the user to update.
 *
 * IN newEntryP - a pointer to a pts_UserUpdateEntry_t that contains the
 * new information for user.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserModify(const void *cellHandle, const char *userName,
	       const pts_UserUpdateEntry_p newEntryP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 userId = 0;
    afs_int32 newQuota = 0;
    afs_int32 mask = 0;
    afs_int32 flags = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_UserModify;
    }

    if ((userName == NULL) || (*userName == 0)) {
	tst = ADMPTSUSERNAMENULL;
	goto fail_pts_UserModify;
    }

    if (newEntryP == NULL) {
	tst = ADMPTSNEWENTRYPNULL;
	goto fail_pts_UserModify;
    }

    /*
     * Translate the user name into an id.
     */

    if (!TranslateOneName
	(c_handle, userName, ADMPTSUSERNAMETOOLONG, &userId, &tst)) {
	goto fail_pts_UserModify;
    }


    if (newEntryP->flag & PTS_USER_UPDATE_GROUP_CREATE_QUOTA) {
	mask |= PR_SF_NGROUPS;
	newQuota = newEntryP->groupCreationQuota;
    }

    if (newEntryP->flag & PTS_USER_UPDATE_PERMISSIONS) {
	mask |= PR_SF_ALLBITS;
	if (!SetUserAccess(newEntryP, &flags, &tst)) {
	    goto fail_pts_UserModify;
	}
    }

    /*
     * Make the rpc
     */

    tst =
	ubik_PR_SetFieldsEntry(c_handle->pts, 0, userId, mask, flags,
		  newQuota, 0, 0, 0);

    if (tst != 0) {
	goto fail_pts_UserModify;
    }
    rc = 1;

  fail_pts_UserModify:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserMaxGet - get the maximum in use user id.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * OUT maxUserId - upon successful completion contains the max in use id.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserMaxGet(const void *cellHandle, int *maxUserId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_int32 maxGroupId = 0;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_UserMaxGet;
    }

    if (maxUserId == NULL) {
	tst = ADMPTSMAXUSERIDNULL;
	goto fail_pts_UserMaxGet;
    }

    tst = ubik_PR_ListMax(c_handle->pts, 0, maxUserId, &maxGroupId);

    if (tst != 0) {
	goto fail_pts_UserMaxGet;
    }
    rc = 1;

  fail_pts_UserMaxGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserMaxSet - set the maximum user id.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN maxUserId - the new max user id.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserMaxSet(const void *cellHandle, int maxUserId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_UserMaxSet;
    }

    tst = ubik_PR_SetMax(c_handle->pts, 0, maxUserId, 0);

    if (tst != 0) {
	goto fail_pts_UserMaxSet;
    }
    rc = 1;

  fail_pts_UserMaxSet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserMemberListBegin - begin iterating over the list of groups
 * a particular user belongs to.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN groupName - the group whose members will be returned.
 *
 * OUT iterationIdP - upon successful completion contains a iterator that
 * can be passed to pts_GroupMemberListNext.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserMemberListBegin(const void *cellHandle, const char *userName,
			void **iterationIdP, afs_status_p st)
{
    return MemberListBegin(cellHandle, userName, ADMPTSUSERNAMENULL,
			   ADMPTSUSERNAMETOOLONG, iterationIdP, st);

}

/*
 * pts_UserMemberListNext - get the next group a user belongs to
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_UserMemberListBegin
 *
 * OUT userName - upon successful completion contains the next group a user
 * belongs to.
 *
 * LOCKS
 *
 * The iterator mutex is held during the retrieval of the next member.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserMemberListNext(const void *iterationId, char *userName,
		       afs_status_p st)
{
    return pts_GroupMemberListNext(iterationId, userName, st);
}

/*
 * pts_UserMemberListDone - finish using a user list iterator
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_UserMemberListBegin
 *
 * LOCKS
 *
 * The iterator is locked and then destroyed
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * It is the user's responsibility to make sure pts_UserMemberListDone
 * is called only once for each iterator.
 */

int ADMINAPI
pts_UserMemberListDone(const void *iterationId, afs_status_p st)
{
    return pts_GroupMemberListDone(iterationId, st);
}

typedef struct owned_group_list {
    namelist owned_names;	/* the list of character names owned by this id */
    prlist owned_ids;		/* the list of pts ids owned by this id */
    afs_int32 index;		/* the index into owned_names for the next group */
    afs_int32 owner;		/* the pts id of the owner */
    afs_int32 more;		/* the last parameter to PR_ListOwned */
    int finished_retrieving;	/* set when we've processed the last owned_names */
    afs_cell_handle_p c_handle;	/* ubik client to pts server's from c_handle */
    char group[CACHED_ITEMS][PTS_MAX_NAME_LEN];	/* cache of names */
} owned_group_list_t, *owned_group_list_p;

static int
DeleteOwnedGroupSpecificData(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    owned_group_list_p list = (owned_group_list_p) rpc_specific;

    if (list->owned_names.namelist_val != NULL) {
	free(list->owned_names.namelist_val);
    }

    if (list->owned_ids.prlist_val != NULL) {
	free(list->owned_ids.prlist_val);
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetOwnedGroupRPC(void *rpc_specific, int slot, int *last_item,
		 int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    owned_group_list_p list = (owned_group_list_p) rpc_specific;

    /*
     * We really don't make an rpc for every entry we return here
     * since the pts interface allows several members to be returned
     * with one rpc, but we fake it to make the iterator happy.
     */

    /*
     * Check to see if we are done retrieving data
     */

    if ((list->finished_retrieving) && (list->owned_names.namelist_len == 0)) {
	*last_item = 1;
	*last_item_contains_data = 0;
	goto fail_GetOwnedGroupRPC;
    }

    /*
     * Check to see if we really need to make an rpc
     */

    if ((!list->finished_retrieving) && (list->owned_names.namelist_len == 0)) {
	tst =
	    ubik_PR_ListOwned(list->c_handle->pts, 0, list->owner,
		      &list->owned_ids, &list->more);
	if (tst != 0) {
	    goto fail_GetOwnedGroupRPC;
	}

	if (!TranslatePTSIds
	    (list->c_handle, &list->owned_names, (idlist *) & list->owned_ids,
	     &tst)) {
	    goto fail_GetOwnedGroupRPC;
	}
	list->index = 0;

	if (list->owned_names.namelist_val == NULL) {
	    *last_item = 1;
	    *last_item_contains_data = 0;
	    goto fail_GetOwnedGroupRPC;
	}
    }

    /*
     * We can retrieve the next group from data we already received
     */

    strcpy(list->group[slot], list->owned_names.namelist_val[list->index]);
    list->index++;

    /*
     * Check to see if there is more data to be retrieved
     * We need to free up the previously retrieved data here
     * and then check to see if the last rpc indicated that there
     * were more items to retrieve.
     */

    if (list->index >= list->owned_names.namelist_len) {
	list->owned_names.namelist_len = 0;
	free(list->owned_names.namelist_val);
	list->owned_names.namelist_val = 0;

	list->owned_ids.prlist_len = 0;
	free(list->owned_ids.prlist_val);
	list->owned_ids.prlist_val = 0;

	if (!list->more) {
	    list->finished_retrieving = 1;
	}
    }
    rc = 1;

  fail_GetOwnedGroupRPC:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetOwnedGroupFromCache(void *rpc_specific, int slot, void *dest,
		       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    owned_group_list_p list = (owned_group_list_p) rpc_specific;

    strcpy((char *)dest, list->group[slot]);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }

    return rc;
}

/*
 * pts_OwnedGroupListBegin - begin iterating over the list of groups
 * a particular user owns.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the group exists.
 *
 * IN ownerName - the owner of the groups of interest.
 *
 * OUT iterationIdP - upon successful completion contains a iterator that
 * can be passed to pts_OwnedGroupListNext.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_OwnedGroupListBegin(const void *cellHandle, const char *userName,
			void **iterationIdP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    owned_group_list_p list =
	(owned_group_list_p) malloc(sizeof(owned_group_list_t));

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_OwnedGroupListBegin;
    }

    if ((userName == NULL) || (*userName == 0)) {
	tst = ADMPTSUSERNAMENULL;
	goto fail_pts_OwnedGroupListBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_OwnedGroupListBegin;
    }

    if ((iter == NULL) || (list == NULL)) {
	tst = ADMNOMEM;
	goto fail_pts_OwnedGroupListBegin;
    }

    /*
     * Initialize the iterator specific data
     */

    list->index = 0;
    list->finished_retrieving = 0;
    list->c_handle = c_handle;
    list->owned_names.namelist_len = 0;
    list->owned_names.namelist_val = 0;
    list->owned_ids.prlist_len = 0;
    list->owned_ids.prlist_val = 0;

    /*
     * Translate the user name into an id.
     */

    if (!TranslateOneName
	(c_handle, userName, ADMPTSUSERNAMETOOLONG, &list->owner, &tst)) {
	goto fail_pts_OwnedGroupListBegin;
    }

    if (IteratorInit
	(iter, (void *)list, GetOwnedGroupRPC, GetOwnedGroupFromCache, NULL,
	 DeleteOwnedGroupSpecificData, &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_pts_OwnedGroupListBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (list != NULL) {
	    free(list);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_OwnedGroupListNext - get the next group a user owns.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_OwnedGroupListBegin
 *
 * OUT groupName - upon successful completion contains the next group a user
 * owns.
 *
 * LOCKS
 *
 * The iterator mutex is held during the retrieval of the next member.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_OwnedGroupListNext(const void *iterationId, char *groupName,
		       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_OwnedGroupListNext;
    }

    if (groupName == NULL) {
	tst = ADMPTSGROUPNAMENULL;
	goto fail_pts_OwnedGroupListNext;
    }

    rc = IteratorNext(iter, (void *)groupName, &tst);

  fail_pts_OwnedGroupListNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_OwnedGroupListDone - finish using a group list iterator
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_OwnedGroupListBegin
 *
 * LOCKS
 *
 * The iterator is locked and then destroyed
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * It is the user's responsibility to make sure pts_OwnedGroupListDone
 * is called only once for each iterator.
 */

int ADMINAPI
pts_OwnedGroupListDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_OwnedGroupListDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_pts_OwnedGroupListDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

typedef struct pts_list {
    prlistentries *names;	/* the current list of pts names in this cell */
    prlistentries *currName;	/* the current pts entry */
    afs_int32 index;		/* the index into names for the next pts entry */
    afs_int32 nextstartindex;	/* the next start index for the RPC */
    afs_int32 nentries;		/* the number of entries in names */
    afs_int32 flag;		/* the type of the list */
    int finished_retrieving;	/* set when we've processed the last owned_names */
    afs_cell_handle_p c_handle;	/* ubik client to pts server's from c_handle */
    char entries[CACHED_ITEMS][PTS_MAX_NAME_LEN];	/* cache of pts names */
} pts_list_t, *pts_list_p;

static int
DeletePTSSpecificData(void *rpc_specific, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    pts_list_p list = (pts_list_p) rpc_specific;

    if (list->names) {
	free(list->names);
    }

    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetPTSRPC(void *rpc_specific, int slot, int *last_item,
	  int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    pts_list_p list = (pts_list_p) rpc_specific;

    /*
     * We really don't make an rpc for every entry we return here
     * since the pts interface allows several members to be returned
     * with one rpc, but we fake it to make the iterator happy.
     */

    /*
     * Check to see if we are done retrieving data
     */

    if (list->finished_retrieving) {
	*last_item = 1;
	*last_item_contains_data = 0;
	goto fail_GetPTSRPC;
    }

    /*
     * Check to see if we really need to make an rpc
     */

    if ((!list->finished_retrieving) && (list->index >= list->nentries)) {
	afs_int32 start = list->nextstartindex;
	prentries bulkentries;
	list->nextstartindex = -1;
	bulkentries.prentries_val = 0;
	bulkentries.prentries_len = 0;

	tst =
	    ubik_PR_ListEntries(list->c_handle->pts, 0, list->flag,
		      start, &bulkentries, &(list->nextstartindex));

	if (tst != 0) {
	    goto fail_GetPTSRPC;
	}

	list->nentries = bulkentries.prentries_len;
	list->names = bulkentries.prentries_val;

	list->index = 0;
	list->currName = list->names;

    }

    /*
     * We can retrieve the next entry from data we already received
     */

    strcpy(list->entries[slot], list->currName->name);
    list->index++;
    list->currName++;


    /*
     * Check to see if there is more data to be retrieved
     * We need to free up the previously retrieved data here
     * and then check to see if the last rpc indicated that there
     * were more items to retrieve.
     */

    if (list->index >= list->nentries) {
	if (list->names) {
	    free(list->names);
	}
	list->names = NULL;

	if (list->nextstartindex == -1) {
	    list->finished_retrieving = 1;
	}
    }
    rc = 1;

  fail_GetPTSRPC:

    if (st != NULL) {
	*st = tst;
    }

    return rc;
}

static int
GetPTSFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    pts_list_p list = (pts_list_p) rpc_specific;

    strcpy((char *)dest, list->entries[slot]);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }

    return rc;
}

/*
 * pts_UserListBegin - begin iterating over the list of users
 * in a particular cell
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the users exist.
 *
 * OUT iterationIdP - upon successful completion contains a iterator that
 * can be passed to pts_UserListNext.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserListBegin(const void *cellHandle, void **iterationIdP,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    pts_list_p list = (pts_list_p) malloc(sizeof(pts_list_t));

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_UserListBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_UserListBegin;
    }

    if ((iter == NULL) || (list == NULL)) {
	tst = ADMNOMEM;
	goto fail_pts_UserListBegin;
    }

    /*
     * Initialize the iterator specific data
     */

    list->index = 0;
    list->finished_retrieving = 0;
    list->c_handle = c_handle;
    list->names = NULL;
    list->nextstartindex = 0;
    list->nentries = 0;
    list->flag = PRUSERS;

    if (IteratorInit
	(iter, (void *)list, GetPTSRPC, GetPTSFromCache, NULL,
	 DeletePTSSpecificData, &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_pts_UserListBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (list != NULL) {
	    free(list);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserListNext - get the next user in the cell.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_UserListBegin
 *
 * OUT groupName - upon successful completion contains the next user
 *
 * LOCKS
 *
 * The iterator mutex is held during the retrieval of the next member.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_UserListNext(const void *iterationId, char *userName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_UserListNext;
    }

    if (userName == NULL) {
	tst = ADMPTSUSERNAMENULL;
	goto fail_pts_UserListNext;
    }

    rc = IteratorNext(iter, (void *)userName, &tst);

  fail_pts_UserListNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserListDone - finish using a user list iterator
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_UserListBegin
 *
 * LOCKS
 *
 * The iterator is locked and then destroyed
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * It is the user's responsibility to make sure pts_UserListDone
 * is called only once for each iterator.
 */

int ADMINAPI
pts_UserListDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_UserListDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_pts_UserListDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupListBegin - begin iterating over the list of groups
 * in a particular cell.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle that corresponds
 * to the cell where the groups exist.
 *
 * OUT iterationIdP - upon successful completion contains a iterator that
 * can be passed to pts_GroupListNext.
 *
 * LOCKS
 *
 * No locks are held by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupListBegin(const void *cellHandle, void **iterationIdP,
		   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    pts_list_p list = (pts_list_p) malloc(sizeof(pts_list_t));

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_pts_GroupListBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_GroupListBegin;
    }

    if ((iter == NULL) || (list == NULL)) {
	tst = ADMNOMEM;
	goto fail_pts_GroupListBegin;
    }

    /*
     * Initialize the iterator specific data
     */

    list->index = 0;
    list->finished_retrieving = 0;
    list->c_handle = c_handle;
    list->names = NULL;
    list->nextstartindex = 0;
    list->nentries = 0;
    list->flag = PRGROUPS;

    if (IteratorInit
	(iter, (void *)list, GetPTSRPC, GetPTSFromCache, NULL,
	 DeletePTSSpecificData, &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_pts_GroupListBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (list != NULL) {
	    free(list);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_UserListNext - get the next group in a cell.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_GroupListBegin
 *
 * OUT groupName - upon successful completion contains the next group
 *
 * LOCKS
 *
 * The iterator mutex is held during the retrieval of the next member.
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
pts_GroupListNext(const void *iterationId, char *groupName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_GroupListNext;
    }

    if (groupName == NULL) {
	tst = ADMPTSGROUPNAMENULL;
	goto fail_pts_GroupListNext;
    }

    rc = IteratorNext(iter, (void *)groupName, &tst);

  fail_pts_GroupListNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * pts_GroupListDone - finish using a group list iterator
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by pts_GroupListBegin
 *
 * LOCKS
 *
 * The iterator is locked and then destroyed
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 * ASSUMPTIONS
 *
 * It is the user's responsibility to make sure pts_GroupListDone
 * is called only once for each iterator.
 */

int ADMINAPI
pts_GroupListDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    /*
     * Validate arguments
     */

    if (iterationId == NULL) {
	tst = ADMITERATORNULL;
	goto fail_pts_GroupListDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_pts_GroupListDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
