/*
 * Copyright (C) 1998  Transarc Corporation.
 * All rights reserved.
 *
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

