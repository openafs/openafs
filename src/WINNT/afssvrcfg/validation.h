/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_VALIDATION_H_
#define _VALIDATION_H_


enum VALIDATION_TYPE { 
	VALID_AFS_PARTITION_NAME, 
	VALID_AFS_CELL_NAME,
	VALID_AFS_PASSWORD,
	VALID_AFS_UID,
	VALID_AFS_SERVER_NAME,
	VALID_FILENAME,
	VALID_PATH
};


BOOL Validation_IsValid(TCHAR *pszInput, VALIDATION_TYPE type, BOOL bShowErorr = TRUE);

#endif	// _VALIDATION_H_
