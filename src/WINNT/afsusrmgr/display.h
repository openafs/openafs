/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef DISPLAY_H
#define DISPLAY_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   ttUSERS,
   ttGROUPS,
   ttMACHINES
   } TABTYPE;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Display_StartWorking (void);
void Display_StopWorking (void);

void Display_PopulateList (void);
void Display_PopulateUserList (void);
void Display_PopulateGroupList (void);
void Display_PopulateMachineList (void);

void Display_OnEndTask_UpdUsers (LPTASKPACKET ptp);
void Display_OnEndTask_UpdGroups (LPTASKPACKET ptp);
void Display_OnEndTask_UpdMachines (LPTASKPACKET ptp);

void Display_RefreshView (LPVIEWINFO lpviNew, ICONVIEW ivNew);
void Display_RefreshView_Fast (void);

void Display_SelectAll (void);
LPASIDLIST Display_GetSelectedList (void);
size_t Display_GetSelectedCount (void);
TABTYPE Display_GetActiveTab (void);

BOOL Display_HandleColumnNotify (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp, LPVIEWINFO pvi);
BOOL CALLBACK Display_GetItemText (HWND hList, LPFLN_GETITEMTEXT_PARAMS pfln, DWORD dwCookie);
void Display_GetImageIcons (DWORD dwStyle, ICONVIEW iv, ASID idObject, int iImageNormal, int iImageAlert, int *piFirstImage, int *piSecondImage);


#endif

