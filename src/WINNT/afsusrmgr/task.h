/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TASK_H
#define TASK_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

typedef struct
   {
   TCHAR szCell[ cchNAME ];
   BOOL fCloseAppOnFail;
   UINT_PTR hCreds;
   } OPENCELL_PARAMS, *LPOPENCELL_PARAMS;

typedef struct
   {
   ASID idUser;
   AFSADMSVR_CHANGEUSER_PARAMS NewProperties;
   } USER_CHANGE_PARAMS, *LPUSER_CHANGE_PARAMS;

typedef struct
   {
   LPASIDLIST pUsers;
   LPASIDLIST pGroups;
   BOOL fMembership;
   } USER_GROUPLIST_SET_PARAMS, *LPUSER_GROUPLIST_SET_PARAMS;

typedef struct
   {
   ASID idUser;
   int keyVersion;
   TCHAR keyString[ cchRESOURCE ];
   BYTE keyData[ ENCRYPTIONKEYLENGTH ];
   } USER_CPW_PARAMS, *LPUSER_CPW_PARAMS;

typedef struct
   {
   LPTSTR mszNames;
   TCHAR szPassword[ cchRESOURCE ];
   UINT_PTR idUser;
   AFSADMSVR_CHANGEUSER_PARAMS Properties;
   LPASIDLIST pGroupsMember;
   LPASIDLIST pGroupsOwner;
   BOOL fCreateKAS;
   BOOL fCreatePTS;
   } USER_CREATE_PARAMS, *LPUSER_CREATE_PARAMS;

typedef struct
   {
   LPASIDLIST pUserList;
   BOOL fDeleteKAS;
   BOOL fDeletePTS;
   } USER_DELETE_PARAMS, *LPUSER_DELETE_PARAMS;

typedef struct
   {
   ASID idGroup;
   AFSADMSVR_CHANGEGROUP_PARAMS NewProperties;
   } GROUP_CHANGE_PARAMS, *LPGROUP_CHANGE_PARAMS;

typedef struct
   {
   LPASIDLIST pUserList;
   BOOL fMembership;
   } GROUP_SEARCH_PARAMS, *LPGROUP_SEARCH_PARAMS;

typedef struct
   {
   LPASIDLIST pGroups;
   LPASIDLIST pMembers;
   } GROUP_MEMBERS_SET_PARAMS, *LPGROUP_MEMBERS_SET_PARAMS;

typedef struct
   {
   ASID idGroup;
   TCHAR szNewName[ cchNAME ];
   } GROUP_RENAME_PARAMS, *LPGROUP_RENAME_PARAMS;

typedef struct
   {
   ASID idGroup;
   LPASIDLIST pOwnedGroups;
   } GROUP_OWNED_SET_PARAMS, *LPGROUP_OWNED_SET_PARAMS;

typedef struct
   {
   LPTSTR mszNames;
   TCHAR szOwner[ cchNAME ];
   UINT_PTR idGroup;
   AFSADMSVR_CHANGEGROUP_PARAMS Properties;
   LPASIDLIST pMembers;
   LPASIDLIST pGroupsOwner;
   } GROUP_CREATE_PARAMS, *LPGROUP_CREATE_PARAMS;

typedef struct
   {
   LPCTSTR pszNames;
   ASOBJTYPE Type;
   } LIST_TRANSLATE_PARAMS, *LPLIST_TRANSLATE_PARAMS;

typedef struct
   {
   HWND hNotify;
   LPASIDLIST pAsidList;
   } OBJECT_LISTEN_PARAMS, *LPOBJECT_LISTEN_PARAMS;

typedef struct
   {
   ASID idCell;
   UINT_PTR idUserMax;
   UINT_PTR idGroupMax;
   } CELL_CHANGE_PARAMS, *LPCELL_CHANGE_PARAMS;

typedef enum
   {
   taskOPENCELL,	// lp = new OPENCELL_PARAMS
   taskUPD_CREDS,	// lp = unused
   taskUPD_USERS,	// lp = unused
   taskUPD_GROUPS,	// lp = unused
   taskUPD_MACHINES,	// lp = unused
   taskREFRESH,	// lp = (ASID)idRefreshScope
   taskREFRESHMULT,	// lp = scope (from asc_AsidListCreate)
   taskGET_ACTIONS,	// lp = unused
   taskGET_RANDOM_KEY,	// lp = unused
   taskUSER_CHANGE,	// lp = new USER_CHANGE_PARAMS
   taskUSER_FIND,	// lp = CloneString(szUserName)
   taskUSER_ENUM,	// lp = CloneString(szPattern) or NULL
   taskUSER_GROUPLIST_SET,	// lp = new USER_GROUPLIST_SET_PARAMS
   taskUSER_CPW,	// lp = new USER_CPW_PARAMS
   taskUSER_UNLOCK,	// lp = users (from asc_AsidListCreate)
   taskUSER_CREATE,	// lp = new USER_CREATE_PARAMS
   taskUSER_DELETE,	// lp = new USER_DELETE_PARAMS
   taskGROUP_CHANGE,	// lp = new GROUP_CHANGE_PARAMS
   taskGROUP_SEARCH,	// lp = new GROUP_SEARCH_PARAMS
   taskGROUP_MEMBERS_GET,	// lp = groups (from asc_AsidListCreate)
   taskGROUP_MEMBERS_SET,	// lp = new GROUP_MEMBERS_SET_PARAMS
   taskGROUP_ENUM,	// lp = CloneString(szPattern) or NULL
   taskGROUP_RENAME,	// lp = new GROUP_RENAME_PARAMS
   taskGROUP_OWNED_GET,	// lp = (ASID)idGroup
   taskGROUP_OWNED_SET,	// lp = new GROUP_OWNED_SET_PARAMS
   taskGROUP_CREATE,	// lp = new GROUP_CREATE_PARAMS
   taskGROUP_DELETE,	// lp = groups (from asc_AsidListCreate)
   taskCELL_CHANGE,	// lp = new CELL_CHANGE_PARAMS
   taskLIST_TRANSLATE,	// lp = new LIST_TRANSLATE_PARAMS
   taskOBJECT_LISTEN,	// lp = new OBJECT_LISTEN_PARAMS
   taskOBJECT_GET,	// lp = (ASID)idObject
   taskSET_REFRESH,	// lp = unused
   taskEXPIRED_CREDS	// lp = unused
   } TASK;

typedef struct
   {
   ASID idCell;	// what was found out?
   LPASIDLIST pAsidList;
   LPASACTIONLIST pActionList;
   TCHAR szPattern[ cchNAME ];
   ASID idObject;
   ASOBJTYPE Type;
   ASOBJPROP Properties;
   BOOL fMembership;
   BYTE key[ ENCRYPTIONKEYLENGTH ];
   } TASKPACKETDATA, *LPTASKPACKETDATA;

#define TASKDATA(_ptp) ((LPTASKPACKETDATA)(ptp->pReturn))


LPTASKPACKET CreateTaskPacket (int idTask, HWND hReply, PVOID lpUser);
void FreeTaskPacket (LPTASKPACKET ptp);
void PerformTask (LPTASKPACKET ptp);


#endif

