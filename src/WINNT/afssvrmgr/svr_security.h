/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVR_SECURITY_H
#define SVR_SECURITY_H


/*
 * SERVER KEY COLUMNS _________________________________________________________
 *
 */

typedef enum
   {
   svrkeyVERSION,
   svrkeyDATA,
   svrkeyCHECKSUM,
   } SERVERKEYCOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
SERVERKEYCOLUMNS[] =
   {
      { IDS_SVRKEY_VERSION,     75 }, // svrkeyVERSION
      { IDS_SVRKEY_DATA,       150 }, // svrkeyDATA
      { IDS_SVRKEY_CHECKSUM,    75 | COLUMN_RIGHTJUST }, // svrkeyCHECKSUM
   };

#define nSERVERKEYCOLUMNS  (sizeof(SERVERKEYCOLUMNS)/sizeof(SERVERKEYCOLUMNS[0]))


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiServer;
   int keyVersion;
   TCHAR szString[ cchRESOURCE ];
   ENCRYPTIONKEY key;
   } KEY_CREATE_PARAMS, *LPKEY_CREATE_PARAMS;

typedef struct
   {
   LPIDENT lpiServer;
   int keyVersion;
   } KEY_DELETE_PARAMS, *LPKEY_DELETE_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_Key_SetDefaultView (LPVIEWINFO lpvi);

void Server_Security (LPIDENT lpiServer, BOOL fJumpToKeys = FALSE);


#endif

