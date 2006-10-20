/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef GRP_COL_H
#define GRP_COL_H

#include "display.h"


/*
 * GROUPLIST COLUMNS __________________________________________________________
 *
 */

typedef enum
   {
   grpcolNAME,
   grpcolCMEMBERS,
   grpcolUID,
   grpcolOWNER,
   grpcolCREATOR,
   } GROUPCOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
GROUPCOLUMNS[] =
   {
      { IDS_GRPCOL_NAME,     100 }, // grpcolNAME
      { IDS_GRPCOL_CMEMBERS,  50 | COLUMN_RIGHTJUST }, // grpcolCMEMBERS
      { IDS_GRPCOL_UID,       50 | COLUMN_RIGHTJUST }, // grpcolUID
      { IDS_GRPCOL_OWNER,    100 }, // grpcolOWNER
      { IDS_GRPCOL_CREATOR,  100 }, // grpcolCREATOR
   };

#define nGROUPCOLUMNS      (sizeof(GROUPCOLUMNS)/sizeof(GROUPCOLUMNS[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Group_SetDefaultView (LPVIEWINFO lpvi, ICONVIEW *piv);

void Group_GetColumn (ASID idObject, GROUPCOLUMN iCol, LPTSTR pszText, LPSYSTEMTIME pstDate, LONG *pcsec, COLUMNTYPE *pcType);


#endif

