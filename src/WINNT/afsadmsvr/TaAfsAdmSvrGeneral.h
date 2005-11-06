/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRGENERAL_H
#define TAAFSADMSVRGENERAL_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <WINNT/TaAfsAdmSvr.h>


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void AfsAdmSvr_Enter (void);
void AfsAdmSvr_Leave (void);

void AfsAdmSvr_Startup (void);
void AfsAdmSvr_Shutdown (void);

void AfsAdmSvr_EnableAutoShutdown (BOOL fEnable);
size_t AfsAdmSvr_BeginOperation (UINT_PTR idClient, LPASACTION pAction = NULL);
void AfsAdmSvr_EndOperation (size_t iOp);
BOOL AfsAdmSvr_GetOperation (DWORD idAction, LPASACTION pAction);
LPASACTIONLIST AfsAdmSvr_GetOperations (UINT_PTR idClientSearch = 0, ASID idCellSearch = 0);
void AfsAdmSvr_Action_StartRefresh (ASID idScope);
void AfsAdmSvr_Action_StopRefresh (ASID idScope);

BOOL AfsAdmSvr_fIsValidClient (UINT_PTR idClient);
BOOL AfsAdmSvr_AttachClient (LPCTSTR pszName, PVOID *pidClient, ULONG *pStatus);
void AfsAdmSvr_DetachClient (UINT_PTR idClient);
LPCTSTR AfsAdmSvr_GetClientName (UINT_PTR idClient);
LPSOCKADDR_IN AfsAdmSvr_GetClientAddress (UINT_PTR idClient);
void AfsAdmSvr_PingClient (UINT_PTR idClient);

BOOL FALSE_ (ULONG status, ULONG *pStatus, size_t iOp = (size_t)-2);
BOOL Leave_FALSE_ (ULONG status, ULONG *pStatus, size_t iOp = (size_t)-2);
PVOID NULL_ (ULONG status, ULONG *pStatus, size_t iOp = (size_t)-2);
PVOID Leave_NULL_ (ULONG status, ULONG *pStatus, size_t iOp = (size_t)-2);
BOOL TRUE_ (ULONG *pStatus, size_t iOp = (size_t)-2);
BOOL Leave_TRUE_ (ULONG *pStatus, size_t iOp = (size_t)-2);

IDENTTYPE GetAsidType (ASID idObject);
BOOL AfsAdmSvr_ResolveName (LPSOCKADDR_IN pAddress, LPTSTR pszName);

DWORD WINAPI AfsAdmSvr_AutoOpen_ThreadProc (PVOID lp);
void AfsAdmSvr_AddToMinScope (DWORD dwScope);
void AfsAdmSvr_SetMinScope (DWORD dwScope);
DWORD AfsAdmSvr_GetMinScope (void);

void AfsAdmSvr_CallbackManager (void);
DWORD AfsAdmSvr_GetCurrentTime (void);


#endif // TAAFSADMSVRGENERAL_H
