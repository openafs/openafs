/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AL_DYNLINK_H
#define AL_DYNLINK_H

extern "C" {
#include <afs/afs_Admin.h>
#include <afs/afs_utilAdmin.h>
#include <afs/afs_kasAdmin.h>
#include <afs/afs_clientAdmin.h>
} // extern "C"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL OpenUtilLibrary (ULONG *pStatus = NULL);
void CloseUtilLibrary (void);

BOOL OpenKasLibrary (ULONG *pStatus = NULL);
void CloseKasLibrary (void);

BOOL OpenClientLibrary (ULONG *pStatus = NULL);
void CloseClientLibrary (void);


/*
 * INTERFACES _________________________________________________________________
 *
 */

typedef int (ADMINAPI *util_AdminErrorCodeTranslate_t)(afs_status_t errorCode, int langId, const char **errorTextP, afs_status_p st);

typedef int (ADMINAPI *kas_PrincipalGetBegin_t)(const void *cellHandle, const void *serverHandle, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *kas_PrincipalGetNext_t)(const void *iterationId, kas_identity_p who, afs_status_p st);
typedef int (ADMINAPI *kas_PrincipalGetDone_t)(const void *iterationIdP, afs_status_p st);
typedef int (ADMINAPI *kas_PrincipalGet_t) (const void *cellHandle, const void *serverHandle, const kas_identity_p who, kas_principalEntry_p principal, afs_status_p st);

typedef int (ADMINAPI *afsclient_TokenGetExisting_t)(const char *cellName, void **tokenHandle, afs_status_p st);
typedef int (ADMINAPI *afsclient_TokenGetNew_t)(const char *cellName, const char *principal, const char *password, void **tokenHandle, afs_status_p st);
typedef int (ADMINAPI *afsclient_TokenClose_t)(const void *tokenHandle, afs_status_p st);
typedef int (ADMINAPI *afsclient_TokenQuery_t)(void *tokenHandle, unsigned long *expirationDateP, char *principal, char *identity, char *cellName, int *bHasKasTokenP, afs_status_p st);
typedef int (ADMINAPI *afsclient_CellOpen_t)(const char *cellName, const void *tokenHandle, void **cellHandleP, afs_status_p st);
typedef int (ADMINAPI *afsclient_CellClose_t)(const void *cellHandle, afs_status_p st);
typedef int (ADMINAPI *afsclient_LocalCellGet_t)(char *cellName, afs_status_p st);


extern util_AdminErrorCodeTranslate_t util_AdminErrorCodeTranslateP;

extern kas_PrincipalGetBegin_t kas_PrincipalGetBeginP;
extern kas_PrincipalGetNext_t kas_PrincipalGetNextP;
extern kas_PrincipalGetDone_t kas_PrincipalGetDoneP;
extern kas_PrincipalGet_t kas_PrincipalGetP;

extern afsclient_TokenGetExisting_t afsclient_TokenGetExistingP;
extern afsclient_TokenGetNew_t afsclient_TokenGetNewP;
extern afsclient_TokenClose_t afsclient_TokenCloseP;
extern afsclient_TokenQuery_t afsclient_TokenQueryP;
extern afsclient_CellOpen_t afsclient_CellOpenP;
extern afsclient_CellClose_t afsclient_CellCloseP;
extern afsclient_LocalCellGet_t afsclient_LocalCellGetP;


#define util_AdminErrorCodeTranslate (*util_AdminErrorCodeTranslateP)

#define kas_PrincipalGetBegin (*kas_PrincipalGetBeginP)
#define kas_PrincipalGetNext (*kas_PrincipalGetNextP)
#define kas_PrincipalGetDone (*kas_PrincipalGetDoneP)
#define kas_PrincipalGet (*kas_PrincipalGetP)

#define afsclient_TokenGetExisting (*afsclient_TokenGetExistingP)
#define afsclient_TokenGetNew (*afsclient_TokenGetNewP)
#define afsclient_TokenClose (*afsclient_TokenCloseP)
#define afsclient_TokenQuery (*afsclient_TokenQueryP)
#define afsclient_CellOpen (*afsclient_CellOpenP)
#define afsclient_CellClose (*afsclient_CellCloseP)
#define afsclient_LocalCellGet (*afsclient_LocalCellGetP)

#endif

