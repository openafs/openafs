/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef SET_COL_H
#define SET_COL_H


/*
 * FILESET-VIEW COLUMNS _______________________________________________________
 *
 */

typedef enum
   {
   setcolNAME,
   setcolTYPE,
   setcolDATE_CREATE,
   setcolDATE_UPDATE,
   setcolDATE_ACCESS,
   setcolDATE_BACKUP,
   setcolQUOTA_USED,
   setcolQUOTA_USED_PER,
   setcolQUOTA_FREE,
   setcolQUOTA_TOTAL,
   setcolSTATUS,
   setcolAGGREGATE,
   setcolID,
   setcolFILES,
   } FILESETCOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
FILESETCOLUMNS[] =
   {
      { IDS_SETCOL_NAME,           220 }, // setcolNAME
      { IDS_SETCOL_TYPE,           100 }, // setcolTYPE
      { IDS_SETCOL_DATE_CREATE,    100 }, // setcolDATE_CREATE
      { IDS_SETCOL_DATE_UPDATE,    100 }, // setcolDATE_UPDATE
      { IDS_SETCOL_DATE_ACCESS,    100 }, // setcolDATE_ACCESS
      { IDS_SETCOL_DATE_BACKUP,    100 }, // setcolDATE_BACKUP
      { IDS_SETCOL_QUOTA_USED,     100 | COLUMN_RIGHTJUST }, // setcolQUOTA_USED
      { IDS_SETCOL_QUOTA_USED_PER, 100 | COLUMN_RIGHTJUST }, // setcolQUOTA_USED_PER
      { IDS_SETCOL_QUOTA_FREE,     100 | COLUMN_RIGHTJUST }, // setcolQUOTA_FREE
      { IDS_SETCOL_QUOTA_TOTAL,    100 | COLUMN_RIGHTJUST }, // setcolQUOTA_TOTAL
      { IDS_SETCOL_STATUS,         300 }, // setcolSTATUS
      { IDS_SETCOL_AGGREGATE,      100 }, // setcolAGGREGATE
      { IDS_SETCOL_ID,             100 }, // setcolID
      { IDS_SETCOL_FILES,          100 | COLUMN_RIGHTJUST }, // setcolFILES
   };

#define nFILESETCOLUMNS  (sizeof(FILESETCOLUMNS)/sizeof(FILESETCOLUMNS[0]))


/*
 * REPLICA-VIEW COLUMNS _______________________________________________________
 *
 */

typedef enum
   {
   repcolSERVER,
   repcolAGGREGATE,
   repcolDATE_UPDATE,
   } REPLICACOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
REPLICACOLUMNS[] =
   {
      { IDS_REPCOL_SERVER,         100 }, // repcolSERVER
      { IDS_REPCOL_AGGREGATE,      100 }, // repcolAGGREGATE
      { IDS_REPCOL_DATE_UPDATE,    100 }, // repcolDATE_UPDATE
   };

#define nREPLICACOLUMNS  (sizeof(REPLICACOLUMNS)/sizeof(REPLICACOLUMNS[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_SetDefaultView (LPVIEWINFO lpvi);

size_t Filesets_GetAlertCount (LPFILESET lpFileset);
LPTSTR Filesets_GetColumnText (LPIDENT lpi, FILESETCOLUMN setcol, BOOL fShowServerName = FALSE);


void Replicas_SetDefaultView (LPVIEWINFO lpvi);

LPTSTR Replicas_GetColumnText (LPIDENT lpi, REPLICACOLUMN repcol);


#endif

