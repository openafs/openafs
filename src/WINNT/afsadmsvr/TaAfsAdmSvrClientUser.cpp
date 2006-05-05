/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsAdmSvrClientInternal.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL ADMINAPI asc_UserChange (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_CHANGEUSER_PARAMS pChange, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      if ((rc = AfsAdmSvr_ChangeUser (idClient, idCell, idUser, pChange, &status)) != FALSE)
         {
         // If we succeeded in changing this user's properties, get the
         // newest values for our cache.
         //
         ASOBJPROP Properties;
         rc = asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idUser, &Properties, &status);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_UserPasswordSet (UINT_PTR idClient, ASID idCell, ASID idUser, int keyVersion, LPCTSTR pkeyString, PBYTE pkeyData, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      BYTE keyData[ ENCRYPTIONKEYLENGTH ];
      if (pkeyData)
         memcpy (keyData, pkeyData, sizeof(keyData));
      else
         memset (keyData, 0x00, sizeof(keyData));

      STRING keyString;
      if (pkeyString)
         lstrcpy (keyString, pkeyString);
      else
         memset (keyString, 0x00, sizeof(keyString));

      if ((rc = AfsAdmSvr_SetUserPassword (idClient, idCell, idUser, keyVersion, keyString, keyData, &status)) == TRUE)
         {
         // If we succeeded in changing this user's password, get the
         // newest user properties for our cache.
         //
         ASOBJPROP Properties;
         rc = asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idUser, &Properties, &status);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_UserUnlock (UINT_PTR idClient, ASID idCell, ASID idUser, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      if ((rc = AfsAdmSvr_UnlockUser (idClient, idCell, idUser, &status)) == TRUE)
         {
         // If we succeeded in unlocking this user's account, get the
         // newest user properties for our cache.
         //
         ASOBJPROP Properties;
         rc = asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idUser, &Properties, &status);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_UserCreate (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CREATEUSER_PARAMS pCreate, ASID *pidUser, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      if ((rc = AfsAdmSvr_CreateUser (idClient, idCell, pCreate, pidUser, &status)) == TRUE)
         {
         // If we succeeded in creating this user's account, get the
         // initial user properties for our cache.
         //
         ASOBJPROP Properties;
         rc = asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, *pidUser, &Properties, &status);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}


BOOL ADMINAPI asc_UserDelete (UINT_PTR idClient, ASID idCell, ASID idUser, LPAFSADMSVR_DELETEUSER_PARAMS pDelete, ULONG *pStatus)
{
   BOOL rc = TRUE;
   ULONG status = 0;

   RpcTryExcept
      {
      if ((rc = AfsAdmSvr_DeleteUser (idClient, idCell, idUser, pDelete, &status)) == TRUE)
         {
         // If we succeeded in deleting this user's account, clean up our cache.
         // Expect this call to fail (the user's deleted, right?)
         //
         ASOBJPROP Properties;
         ULONG dummy;
         (void)asc_ObjectPropertiesGet (idClient, GET_ALL_DATA, idCell, idUser, &Properties, &dummy);
         }
      }
   RpcExcept(1)
      {
      rc = FALSE;
      status = RPC_S_CALL_FAILED_DNE;
      }
   RpcEndExcept

   if (!rc && pStatus)
      *pStatus = status;
   return rc;
}

