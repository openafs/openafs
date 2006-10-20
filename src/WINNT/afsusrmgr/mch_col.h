/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef MCH_COL_H
#define MCH_COL_H

#include "display.h"


/*
 * MACHINELIST COLUMNS ________________________________________________________
 *
 */

typedef enum
   {
   mchcolNAME,
   mchcolCGROUPMAX,
   mchcolUID,
   mchcolOWNER,
   mchcolCREATOR
   } MACHINECOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
MACHINECOLUMNS[] =
   {
      { IDS_MCHCOL_NAME,       100 }, // mchcolNAME
      { IDS_MCHCOL_CGROUPMAX,   50 | COLUMN_RIGHTJUST }, // mchcolCGROUPMAX
      { IDS_MCHCOL_UID,         50 | COLUMN_RIGHTJUST }, // mchcolUID
      { IDS_MCHCOL_OWNER,      100 }, // mchcolOWNER
      { IDS_MCHCOL_CREATOR,    100 }, // mchcolCREATOR
   };

#define nMACHINECOLUMNS     (sizeof(MACHINECOLUMNS)/sizeof(MACHINECOLUMNS[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Machine_SetDefaultView (LPVIEWINFO lpvi, ICONVIEW *piv);

void Machine_GetColumn (ASID idObject, MACHINECOLUMN iCol, LPTSTR pszText, LPSYSTEMTIME pstDate, LONG *pcsec, COLUMNTYPE *pcType);


#endif

