/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define	HELPFILE_NATIVE	"afs-nt.hlp"
#define	HELPFILE_LIGHT	"afs-light.hlp"

#define	HELPTYPE	HELP_CONTEXT

#define AUTHENTICATION_HELP_ID		16
#define GET_TOKENS_HELP_ID			17
#define DISCARD_TOKENS_HELP_ID		18
#define SET_AFS_ACL_HELP_ID			19
#define ADD_ACL_ENTRY_HELP_ID		20		
#define COPY_ACL_HELP_ID			21
#define VOLUME_INFO_HELP_ID			27
#define PARTITION_INFO_HELP_ID		28
#define MOUNT_POINTS_HELP_ID		33
#define REMOVE_MOUNT_POINTS_HELP_ID	34
#define MAKE_MOUNT_POINT_HELP_ID	35
#define SHOW_FILE_SERVERS_HELP_ID	38
#define SHOW_CELL_HELP_ID			39
#define SERVER_STATUS_HELP_ID		40

#define DOWN_SERVERS_HELP_ID		38	// What should this really be?

#define SUBMOUNTS_HELP_ID			44
#define ADD_SUBMT_HELP_ID			45
#define EDIT_PATH_NAME_HELP_ID		46

void SetHelpPath(const char *pszDefaultHelpFilePath);
void ShowHelp(HWND hWnd, DWORD nHelpID);

