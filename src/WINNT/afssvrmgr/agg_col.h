/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AGGREGS_H
#define AGGREGS_H


/*
 * AGGREGATE-VIEW COLUMNS _____________________________________________________
 *
 */

typedef enum
   {
   aggcolNAME,
   aggcolID,
   aggcolDEVICE,
   aggcolUSED,
   aggcolUSED_PER,
   aggcolALLOCATED,
   aggcolFREE,
   aggcolTOTAL,
   aggcolSTATUS
   } AGGREGATECOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
AGGREGATECOLUMNS[] =
   {
      { IDS_AGGCOL_NAME,     100 }, // aggcolNAME
      { IDS_AGGCOL_ID,       100 | COLUMN_RIGHTJUST }, // aggcolID
      { IDS_AGGCOL_DEVICE,   100 }, // aggcolDEVICE
      { IDS_AGGCOL_USED,     100 | COLUMN_RIGHTJUST }, // aggcolUSED
      { IDS_AGGCOL_USED_PER, 100 | COLUMN_RIGHTJUST }, // aggcolUSED_PER
      { IDS_AGGCOL_ALLOCATED,100 | COLUMN_RIGHTJUST }, // aggcolALLOCATED
      { IDS_AGGCOL_FREE,     100 | COLUMN_RIGHTJUST }, // aggcolFREE
      { IDS_AGGCOL_TOTAL,    100 | COLUMN_RIGHTJUST }, // aggcolTOTAL
      { IDS_AGGCOL_STATUS,   300 }, // aggcolSTATUS
   };

#define nAGGREGATECOLUMNS  (sizeof(AGGREGATECOLUMNS)/sizeof(AGGREGATECOLUMNS[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Aggregates_SetDefaultView (LPVIEWINFO lpvi);

size_t Aggregates_GetAlertCount (LPAGGREGATE lpAggregate);
LPTSTR Aggregates_GetColumnText (LPIDENT lpi, AGGREGATECOLUMN aggcol, BOOL fShowServerName = FALSE);


#endif

