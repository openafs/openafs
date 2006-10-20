/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef ALERT_H
#define ALERT_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define nAlertsMAX  32

#define cmsec1SECOND  (1000L)
#define cmsec1MINUTE  (cmsec1SECOND * 60L)
#define cmsec1HOUR    (cmsec1MINUTE * 60L)
#define cmsec1DAY     (cmsec1HOUR * 24L)

#define DEFAULT_SCOUT_REFRESH_RATE  (1L * cmsec1HOUR)

typedef enum
   {
   alertINVALID = 0,	// (end-of-list)
   alertSECONDARY,	// Server alerted because agg(etc) did
   alertTIMEOUT,	// Server could not be contacted
   alertFULL,	// Usage is above warning threshold
   alertNO_VLDBENT,	// Fileset has no VLDB entry
   alertNO_SVRENT,	// Fileset has no Server entry
   alertSTOPPED,	// Service stopped unexpectedly
   alertBADCREDS,	// May not be able to access FTSERVER
   alertOVERALLOC,	// Aggregate allocation exceeds capacity
   alertSTATE_NO_VNODE,	// Fileset has not VNode
   alertSTATE_NO_SERVICE,	// Fileset has no service
   alertSTATE_OFFLINE,	// Fileset is offline
   } ALERT;


typedef struct
   {
   ALERT alert;

   struct
      {
         struct {
            LPIDENT lpiSecondary;
            size_t  iSecondary;
         } aiSECONDARY;

         struct {
            SYSTEMTIME stLastAttempt;
            ULONG status;
         } aiTIMEOUT;

         struct {
            short perWarning;
            ULONG ckWarning;
         } aiFULL;

         struct {
            int nothing;
         } aiNO_VLDBENT;

         struct {
            int nothing;
         } aiNO_SVRENT;

         struct {
            SYSTEMTIME stStopped;
            SYSTEMTIME stLastError;
            ULONG      errLastError;
         } aiSTOPPED;

         struct {
            int nothing;
         } aiBADCREDS;

         struct {
            ULONG ckAllocated;
            ULONG ckCapacity;
         } aiOVERALLOC;

         struct {
            FILESETSTATE State;
         } aiSTATE;
      };
   } ALERTINFO, *LPALERTINFO;


typedef struct
   {
   DWORD     cTickRefresh;	// zero indicates no auto-refresh
   DWORD     dwTickNextRefresh;
   DWORD     dwTickNextTest;
   size_t    nAlerts;
   ALERTINFO aAlerts[ nAlertsMAX ];
   } OBJECTALERTS, *LPOBJECTALERTS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

LPOBJECTALERTS Alert_GetObjectAlerts (LPIDENT lpi, BOOL fAlwaysServer = FALSE, ULONG *pStatus = NULL);

void Alert_SetDefaults (LPOBJECTALERTS lpoa);
void Alert_Initialize (LPOBJECTALERTS lpoa);
void Alert_Scout_SetOutOfDate (LPIDENT lpi);
void Alert_Scout_ServerStatus (LPIDENT lpi, ULONG status);

size_t Alert_GetCount       (LPIDENT lpi);
ALERT  Alert_GetAlert       (LPIDENT lpi, size_t iIndex);
LPIDENT Alert_GetIdent      (LPIDENT lpi, size_t iIndex);
LPTSTR Alert_GetDescription (LPIDENT lpi, size_t iIndex, BOOL fFull);
LPTSTR Alert_GetRemedy      (LPIDENT lpi, size_t iIndex);
LPTSTR Alert_GetButton      (LPIDENT lpi, size_t iIndex);
LPTSTR Alert_GetQuickDescription (LPIDENT lpi);

void Alert_RemoveSecondary (LPIDENT lpiChild);
void Alert_Remove (LPIDENT lpi, size_t iIndex);
void Alert_AddPrimary (LPIDENT lpi, LPALERTINFO lpai);

BOOL Alert_StartScout (ULONG *pStatus = NULL);

BOOL Alert_Scout_QueueCheckServer (LPIDENT lpi, ULONG *pStatus = NULL);


#endif

