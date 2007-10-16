/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#if defined(UKERNEL)
#include "afs/lock.h"
#include "ubik.h"
#include "afs/ptint.h"
#include "afs/ptserver.h"
#else /* defined(UKERNEL) */
#include <lock.h>
#include <ubik.h>
#include "ptint.h"
#include "ptserver.h"
#endif /* defined(UKERNEL) */


extern int PR_INewEntry();
extern int PR_WhereIsIt();
extern int PR_DumpEntry();
extern int PR_AddToGroup();
extern int PR_NameToID();
extern int PR_IDToName();
extern int PR_Delete();
extern int PR_RemoveFromGroup();
extern int PR_GetCPS();
extern int PR_NewEntry();
extern int PR_ListMax();
extern int PR_SetMax();
extern int PR_ListEntry();
extern int PR_ListEntries();
extern int PR_ChangeEntry();
extern int PR_ListElements();
extern int PR_IsAMemberOf();
extern int PR_SetFieldsEntry();
extern int PR_ListOwned();
extern int PR_GetCPS2();
extern int PR_GetHostCPS();
extern int PR_UpdateEntry();
#if defined(SUPERGROUPS)
extern int PR_ListSuperGroups();
#endif

#define pr_ErrorMsg afs_error_message
