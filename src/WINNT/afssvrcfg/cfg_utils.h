/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_WIZ_UTILS_H_
#define _WIZ_UTILS_H_


BOOL IsStepEnabled(const CONFIG_STATE& step);
void EnableStep(CONFIG_STATE& step, BOOL bEnable = TRUE);

BOOL ShouldConfig(const CONFIG_STATE& state);		
BOOL DontConfig(const CONFIG_STATE& state);
void ToggleConfig(CONFIG_STATE& state);
BOOL ConfiguredOrConfiguring(const CONFIG_STATE& step);
BOOL Configured(const CONFIG_STATE& step);
BOOL ShouldUnconfig(const CONFIG_STATE& state);

void RedrawGraphic();

UINT GetAppTitleID();

const char *GetAdminLibErrorCodeMessage(afs_status_t nErrorCode);

void LogError(afs_status_t nErrorCode);

void ShowError(HWND hDlg, afs_status_t nErrorCode, UINT uiErrorMsgID);
int ShowWarning(HWND hDlg, UINT uiMsgID);


#endif	// _WIZ_UTILS_H_

