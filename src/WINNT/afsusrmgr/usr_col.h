/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef USR_COL_H
#define USR_COL_H

#include "display.h"


/*
 * USERLIST COLUMNS ___________________________________________________________
 *
 */

typedef enum
   {
   usrcolNAME,
   usrcolFLAGS,
   usrcolADMIN,
   usrcolTICKET,
   usrcolSYSTEM,
   usrcolCHANGEPW,
   usrcolREUSEPW,
   usrcolEXPIRES,
   usrcolLASTPW,
   usrcolLASTMOD,
   usrcolLASTMODBY,
   usrcolLIFETIME,
   usrcolCDAYPW,
   usrcolCFAILLOGIN,
   usrcolCSECLOCK,
   usrcolCGROUPMAX,
   usrcolUID,
   usrcolOWNER,
   usrcolCREATOR,
   } USERCOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
USERCOLUMNS[] =
   {
      { IDS_USRCOL_NAME,       100 }, // usrcolNAME
      { IDS_USRCOL_FLAGS,       75 }, // usrcolFLAGS
      { IDS_USRCOL_ADMIN,       40 | COLUMN_CENTERJUST }, // usrcolADMIN
      { IDS_USRCOL_TICKET,      40 | COLUMN_CENTERJUST }, // usrcolTICKET
      { IDS_USRCOL_SYSTEM,      40 | COLUMN_CENTERJUST }, // usrcolSYSTEM
      { IDS_USRCOL_CHANGEPW,    40 | COLUMN_CENTERJUST }, // usrcolCHANGEPW
      { IDS_USRCOL_REUSEPW,     40 | COLUMN_CENTERJUST }, // usrcolREUSEPW
      { IDS_USRCOL_EXPIRES,     75 }, // usrcolEXPIRES
      { IDS_USRCOL_LASTPW,      75 }, // usrcolLASTPW
      { IDS_USRCOL_LASTMOD,     75 }, // usrcolLASTMOD
      { IDS_USRCOL_LASTMODBY,  100 }, // usrcolLASTMODBY
      { IDS_USRCOL_LIFETIME,    50 | COLUMN_RIGHTJUST }, // usrcolLIFETIME
      { IDS_USRCOL_CDAYPW,      50 | COLUMN_RIGHTJUST }, // usrcolCDAYPW
      { IDS_USRCOL_CFAILLOGIN,  50 | COLUMN_RIGHTJUST }, // usrcolCFAILLOGIN
      { IDS_USRCOL_CSECLOCK,    50 | COLUMN_RIGHTJUST }, // usrcolCSECLOCK
      { IDS_USRCOL_CGROUPMAX,   50 | COLUMN_RIGHTJUST }, // usrcolCGROUPMAX
      { IDS_USRCOL_UID,         50 | COLUMN_RIGHTJUST }, // usrcolUID
      { IDS_USRCOL_OWNER,      100 }, // usrcolOWNER
      { IDS_USRCOL_CREATOR,    100 }, // usrcolCREATOR
   };

#define nUSERCOLUMNS       (sizeof(USERCOLUMNS)/sizeof(USERCOLUMNS[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void User_SetDefaultView (LPVIEWINFO lpvi, ICONVIEW *piv);

void User_GetColumn (ASID idObject, USERCOLUMN iCol, LPTSTR pszText, LPSYSTEMTIME pstDate, LONG *pcsec, COLUMNTYPE *pcType);

BOOL User_GetDisplayName (LPTSTR pszText, LPASOBJPROP pProperties);
BOOL User_GetDisplayName (LPTSTR pszText, ASID idUser);

void User_SplitDisplayName (LPTSTR pszFull, LPTSTR pszName, LPTSTR pszInstance);


#endif

