/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
* INCLUDES _________________________________________________________________
*
*/
#include <winsock2.h>
#include <ws2tcpip.h>

#include "afscfg.h"
#include "resource.h"
#include "validation.h"


/*
 * DEFINITIONS _________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static BOOL CheckAfsPartitionName(TCHAR *pszInput, int &nErrorMsgResID);
static BOOL CheckAfsCellName(TCHAR *pszInput, int &nErrorMsgResID);
static BOOL CheckAfsPassword(TCHAR *pszInput, int &nErrorMsgResID);
static BOOL CheckAfsUid(TCHAR *pszInput, int &nErrorMsgResID);
static BOOL CheckAfsServerName(TCHAR *pszInput, int &nErrorMsgResID);
static void ShowError(int nErrorMsgResID);


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */
BOOL Validation_IsValid(TCHAR *pszInput, VALIDATION_TYPE type, BOOL bShowError)
{
    BOOL bValid;
    int nErrorMsgResID;

	switch (type) {
		case VALID_AFS_PARTITION_NAME:	bValid = CheckAfsPartitionName(pszInput, nErrorMsgResID);
                                        break;

        case VALID_AFS_CELL_NAME:		bValid = CheckAfsCellName(pszInput, nErrorMsgResID);
                                        break;

		case VALID_AFS_PASSWORD:		bValid = CheckAfsPassword(pszInput, nErrorMsgResID);
                                        break;

		case VALID_AFS_UID:				bValid = CheckAfsUid(pszInput, nErrorMsgResID);
                                        break;

		case VALID_AFS_SERVER_NAME:		bValid = CheckAfsServerName(pszInput, nErrorMsgResID);
                                        break;

		default:						nErrorMsgResID = 0;
										ASSERT(FALSE);
										return FALSE;
	}

    if (!bValid && bShowError)
        ShowError(nErrorMsgResID);

    return bValid;
}



/*
 * STATIC FUNCTIONS _________________________________________________________________
 *
 */

 /*
  * Utility Functions _________________________________________________________________
  *
  */
static BOOL CheckAfsPartitionName(TCHAR *pszPartitionName, int &nErrorMsgResID)
{
    short bIsValid;
    afs_status_t nStatus;

    char *pszName = new char[strlen("/vicpe") + lstrlen(pszPartitionName) + 1];
    if (!pszName) {
        ASSERT(FALSE);
        return TRUE;
    }

    strcpy(pszName, "/vicep");
    strcat(pszName, S2A(pszPartitionName));

    int nResult = cfg_HostPartitionNameValid(pszName, &bIsValid, &nStatus);
    ASSERT(nResult);

    if (!bIsValid)
    	nErrorMsgResID = IDS_PARTITION_NAME_VALIDATION_TYPE;

    delete pszName;

	return bIsValid;
}

static BOOL CheckAfsCellName(TCHAR *pszInput, int &nErrorMsgResID)
{
	nErrorMsgResID = 0;

	return TRUE;
}

static BOOL CheckAfsPassword(TCHAR *pszInput, int &nErrorMsgResID)
{
	nErrorMsgResID = 0;

	return TRUE;
}

static BOOL CheckAfsUid(TCHAR *pszInput, int &nErrorMsgResID)
{
	nErrorMsgResID = 0;

	return TRUE;
}

static BOOL CheckAfsServerName(TCHAR *pszInput, int &nErrorMsgResID)
{
	nErrorMsgResID = 0;

	return TRUE;
}

static void ShowError(int nErrorMsgResID)
{
    Message(MB_ICONSTOP | MB_OK, GetAppTitleID(), IDS_VALIDATION_ERROR_TEMPLATE, TEXT("%m%m"), nErrorMsgResID, nErrorMsgResID);
}
