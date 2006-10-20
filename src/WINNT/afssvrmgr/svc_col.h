/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SVC_COL_H
#define SVC_COL_H


/*
 * SERVICE-VIEW COLUMNS _______________________________________________________
 *
 */

typedef enum
   {
   svccolNAME,
   svccolTYPE,
   svccolPARAMS,
   svccolNOTIFIER,
   svccolSTATUS,
   svccolDATE_START,
   svccolDATE_STOP,
   svccolDATE_STARTSTOP,
   svccolDATE_FAILED,
   svccolLASTERROR,
   } SERVICECOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
SERVICECOLUMNS[] =
   {
      { IDS_SVCCOL_NAME,           100 }, // svccolNAME
      { IDS_SVCCOL_TYPE,           100 }, // svccolTYPE
      { IDS_SVCCOL_PARAMS,         100 }, // svccolPARAMS
      { IDS_SVCCOL_NOTIFIER,       100 }, // svccolNOTIFIER
      { IDS_SVCCOL_STATUS,         100 }, // svccolSTATUS
      { IDS_SVCCOL_DATE_START,     100 }, // svccolDATE_START
      { IDS_SVCCOL_DATE_STOP,      100 }, // svccolDATE_STOP
      { IDS_SVCCOL_DATE_STARTSTOP, 100 }, // svccolDATE_STARTSTOP
      { IDS_SVCCOL_DATE_FAILED,    100 }, // svccolDATE_FAILED
      { IDS_SVCCOL_LASTERROR,      100 }, // svccolLASTERROR
   };

#define nSERVICECOLUMNS  (sizeof(SERVICECOLUMNS)/sizeof(SERVICECOLUMNS[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Services_SetDefaultView (LPVIEWINFO lpvi);

size_t Services_GetAlertCount (LPSERVICE lpService);
LPTSTR Services_GetColumnText (LPIDENT lpi, SERVICECOLUMN svccol, BOOL fShowServerName = FALSE);


#endif

