/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "set_general.h"


/*
 * FILESET PREFERENCES ________________________________________________________
 *
 */

PVOID Filesets_LoadPreferences (LPIDENT lpiFileset)
{
   LPFILESET_PREF pfp = New (FILESET_PREF);

   if (!RestorePreferences (lpiFileset, pfp, sizeof(FILESET_PREF)))
      {
      pfp->perWarnSetFull = -1;	// use the server's default value

      Alert_SetDefaults (&pfp->oa);
      }

   Alert_Initialize (&pfp->oa);
   return pfp;
}


BOOL Filesets_SavePreferences (LPIDENT lpiFileset)
{
   BOOL rc = FALSE;

   PVOID pfp = lpiFileset->GetUserParam();
   if (pfp != NULL)
      {
      rc = StorePreferences (lpiFileset, pfp, sizeof(FILESET_PREF));
      }

   return rc;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

LPIDENT Filesets_GetSelected (HWND hDlg)
{
   return (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_SET_LIST));
}


LPIDENT Filesets_GetFocused (HWND hDlg, POINT *pptHitTest)
{
   HWND hList = GetDlgItem (hDlg, IDC_SET_LIST);

   if (pptHitTest == NULL)
      {
      return (LPIDENT)FL_GetFocusedData (hList);
      }

   HLISTITEM hItem;
   if ((hItem = FastList_ItemFromPoint (hList, pptHitTest, TRUE)) != NULL)
      {
      return (LPIDENT)FL_GetData (hList, hItem);
      }

   return NULL;
}


BOOL Filesets_fIsLocked (LPFILESETSTATUS pfs)
{
   return (pfs->State & fsLOCKED) ? TRUE : FALSE;
}

