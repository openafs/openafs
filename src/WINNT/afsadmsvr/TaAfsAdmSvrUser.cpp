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

#include "TaAfsAdmSvrInternal.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */


      // AfsAdmSvr_ChangeUser
      // ...changes a user account's properties.
      //
extern "C" int AfsAdmSvr_ChangeUser (DWORD idClient, ASID idCell, ASID idUser, LPAFSADMSVR_CHANGEUSER_PARAMS pChange, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_USER_CHANGE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.User_Change.idUser = idUser;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeUser (idUser=0x%08lX)"), idClient, idUser);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Find this user's current properties
   //
   LPASOBJPROP pCurrentProperties;
   if ((pCurrentProperties = AfsAdmSvr_GetCurrentProperties (idUser, pStatus)) == NULL)
      {
      Print (dlERROR, TEXT("Client 0x%08lX: ChangeUser failed; no properties"), idClient);
      AfsAdmSvr_EndOperation (iOp);
      return FALSE;
      }

   // Build an AFSCLASS-style USERPROPERTIES structure that reflects the
   // new properties for the user; mark the structure's dwMask bit to indicate
   // what we're changing.
   //
   USERPROPERTIES NewProperties;
   memset (&NewProperties, 0x00, sizeof(NewProperties));

   if ((NewProperties.fAdmin = pChange->fIsAdmin) != pCurrentProperties->u.UserProperties.KASINFO.fIsAdmin)
      NewProperties.dwMask |= MASK_USERPROP_fAdmin;
   if ((NewProperties.fGrantTickets = pChange->fCanGetTickets) != pCurrentProperties->u.UserProperties.KASINFO.fCanGetTickets)
      NewProperties.dwMask |= MASK_USERPROP_fGrantTickets;
   if ((NewProperties.fCanEncrypt = pChange->fEncrypt) != pCurrentProperties->u.UserProperties.KASINFO.fEncrypt)
      NewProperties.dwMask |= MASK_USERPROP_fCanEncrypt;
   if ((NewProperties.fCanChangePassword = pChange->fCanChangePassword) != pCurrentProperties->u.UserProperties.KASINFO.fCanChangePassword)
      NewProperties.dwMask |= MASK_USERPROP_fCanChangePassword;
   if ((NewProperties.fCanReusePasswords = pChange->fCanReusePasswords) != pCurrentProperties->u.UserProperties.KASINFO.fCanReusePasswords)
      NewProperties.dwMask |= MASK_USERPROP_fCanReusePasswords;
   if ((NewProperties.cdayPwExpires = pChange->cdayPwExpire) != pCurrentProperties->u.UserProperties.KASINFO.cdayPwExpire)
      NewProperties.dwMask |= MASK_USERPROP_cdayPwExpires;
   if ((NewProperties.csecTicketLifetime = pChange->csecTicketLifetime) != pCurrentProperties->u.UserProperties.KASINFO.csecTicketLifetime)
      NewProperties.dwMask |= MASK_USERPROP_csecTicketLifetime;
   if ((NewProperties.nFailureAttempts = pChange->cFailLogin) != pCurrentProperties->u.UserProperties.KASINFO.cFailLogin)
      NewProperties.dwMask |= MASK_USERPROP_nFailureAttempts;
   if ((NewProperties.csecFailedLoginLockTime = pChange->csecFailLoginLock) != pCurrentProperties->u.UserProperties.KASINFO.csecFailLoginLock)
      NewProperties.dwMask |= MASK_USERPROP_csecFailedLoginLockTime;
   if ((NewProperties.cGroupCreationQuota = pChange->cgroupCreationQuota) != pCurrentProperties->u.UserProperties.PTSINFO.cgroupCreationQuota)
      NewProperties.dwMask |= MASK_USERPROP_cGroupCreationQuota;
   if ((NewProperties.aaListStatus = pChange->aaListStatus) != pCurrentProperties->u.UserProperties.PTSINFO.aaListStatus)
      NewProperties.dwMask |= MASK_USERPROP_aaListStatus;
   if ((NewProperties.aaGroupsOwned = pChange->aaGroupsOwned) != pCurrentProperties->u.UserProperties.PTSINFO.aaGroupsOwned)
      NewProperties.dwMask |= MASK_USERPROP_aaGroupsOwned;
   if ((NewProperties.aaMembership = pChange->aaMembership) != pCurrentProperties->u.UserProperties.PTSINFO.aaMembership)
      NewProperties.dwMask |= MASK_USERPROP_aaMembership;
   memcpy (&NewProperties.timeAccountExpires, &pChange->timeExpires, sizeof(SYSTEMTIME));
   if (memcmp (&NewProperties.timeAccountExpires, &pCurrentProperties->u.UserProperties.KASINFO.timeExpires, sizeof(SYSTEMTIME)))
      NewProperties.dwMask |= MASK_USERPROP_timeAccountExpires;

   // If we've decided to change anything, call AfsClass to actually do it
   //
   if (NewProperties.dwMask == 0)
      {
      Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeUser succeeded (nothing to do)"), idClient);
      }
   else
      {
      ULONG status;
      if (!AfsClass_SetUserProperties ((LPIDENT)idUser, &NewProperties, &status))
         {
         Print (dlERROR, TEXT("Client 0x%08lX: ChangeUser failed; error 0x%08lX"), idClient, status);
         return FALSE_(status,pStatus,iOp);
         }

      Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeUser succeeded"), idClient);
      }

   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_SetUserPassword
      // ...changes the password for the specified user account. Pass a non-empty
      //    string in {keyString} to encrypt the specified string; otherwise,
      //    pass a valid encryption key in {keyData}.
      //
extern "C" int AfsAdmSvr_SetUserPassword (DWORD idClient, ASID idCell, ASID idUser, int keyVersion, STRING keyString, BYTE keyData[ ENCRYPTIONKEYLENGTH ], ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   ASACTION Action;
   Action.Action = ACTION_USER_PW_CHANGE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.User_Pw_Change.idUser = idUser;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: SetUserPassword (idUser=0x%08lX)"), idClient, idUser);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Change the user's password
   //
   if (keyString && keyString[0])
      {
      rc = AfsClass_SetUserPassword ((LPIDENT)idUser, keyVersion, keyString, &status);
      }
   else // (!keyString || !keyString[0])
      {
      rc = AfsClass_SetUserPassword ((LPIDENT)idUser, keyVersion, (LPENCRYPTIONKEY)keyData, &status);
      }

   if (!rc)
      return FALSE_(status,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: SetUserPassword succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_UnlockUser
      // ...unlocks a user's account
      //
extern "C" int AfsAdmSvr_UnlockUser (DWORD idClient, ASID idCell, ASID idUser, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_USER_UNLOCK;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.User_Unlock.idUser = idUser;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: UnlockUser (idUser=0x%08lX)"), idClient, idUser);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Unlock the user's account
   //
   ULONG status;
   if (!AfsClass_UnlockUser ((LPIDENT)idUser, &status))
      return FALSE_(status,pStatus,iOp);

   Print (dlDETAIL, TEXT("Client 0x%08lX: UnlockUser succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_CreateUser
      // ...creates a new user account
      //
extern "C" int AfsAdmSvr_CreateUser (DWORD idClient, ASID idCell, LPAFSADMSVR_CREATEUSER_PARAMS pCreate, ASID *pidUser, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_USER_CREATE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   lstrcpy (Action.u.User_Create.szUser, pCreate->szName);
   lstrcpy (Action.u.User_Create.szInstance, pCreate->szInstance);
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: CreateUser (szUser=%s)"), idClient, pCreate->szName);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Create the user account
   //
   ULONG status;
   LPIDENT lpiUser;
   if ((lpiUser = AfsClass_CreateUser ((LPIDENT)idCell, pCreate->szName, pCreate->szInstance, pCreate->szPassword, pCreate->idUser, pCreate->fCreateKAS, pCreate->fCreatePTS, &status)) == NULL)
      {
      Print (dlERROR, TEXT("Client 0x%08lX: CreateUser failed; error 0x%08lX"), idClient, status);
      return FALSE_(status,pStatus,iOp);
      }

   if (pidUser)
      *pidUser = (ASID)lpiUser;

   // Creating a user account may change the max user ID
   AfsAdmSvr_TestProperties (idCell);

   Print (dlDETAIL, TEXT("Client 0x%08lX: CreateUser succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}


      // AfsAdmSvr_DeleteUser
      // ...deletes a user's account
      //
extern "C" int AfsAdmSvr_DeleteUser (DWORD idClient, ASID idCell, ASID idUser, LPAFSADMSVR_DELETEUSER_PARAMS pDelete, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_USER_DELETE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   Action.u.User_Delete.idUser = idUser;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: DeleteUser (idUser=0x%08lX)"), idClient, idUser);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Delete the user's accounts
   //
   ULONG status;
   if (!AfsClass_DeleteUser ((LPIDENT)idUser, pDelete->fDeleteKAS, pDelete->fDeletePTS, &status))
      {
      Print (dlERROR, TEXT("Client 0x%08lX: DeleteUser failed; error 0x%08lX"), idClient, status);
      return FALSE_(status,pStatus,iOp);
      }

   Print (dlDETAIL, TEXT("Client 0x%08lX: DeleteUser succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}

