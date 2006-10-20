/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef DISPLAY_H
#define DISPLAY_H

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   dtINVALID = 0,
   dtCELL,
   dtSERVERS,
   dtSERVICES,
   dtAGGREGATES,
   dtFILESETS,
   dtREPLICAS,
   dtSERVERWINDOW
   } DISPLAYTARGET;

#define ACT_ENDCHANGE  0x0002
#define ACT_UNCOVER    0x0004
#define ACT_SELPREVIEW 0x0008

typedef struct
   {
   HWND hChild;	// g.hMain or server window to update
   DISPLAYTARGET dt;	// expect which kind of listview?
   LPIDENT lpiNotify;	// NULL or specific LPIDENT to update
   ULONG status;	// if (lpiNotify), associated error code
   LPIDENT lpiServer;	// NULL or parent server
   LPIDENT lpiAggregate;	// NULL or parent aggregate
   LPIDENT lpiToSelect;	// NULL or LPIDENT to select when done
   LPVIEWINFO lpvi;	// NULL or specific viewinfo to use
   HWND hList;	// (worker routines set this)
   WORD actOnDone;	// (worker routines set this)
   BOOL fList;	// (worker routines set this)
   } DISPLAYREQUEST, *LPDISPLAYREQUEST;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

ICONVIEW Display_GetServerIconView (void);

BOOL CALLBACK GetItemText (HWND hList, LPFLN_GETITEMTEXT_PARAMS pfln, DWORD dwCookie);

/*
 *** HandleColumnNotify
 *
 * This routine handles the FLN_COLUMNCLICK and FLN_COLUMNRESIZE notifications.
 * This routine returns TRUE if it handled the message; if so, its caller
 * should return FASLE from the dlgproc.
 *
 */

BOOL HandleColumnNotify (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp, LPVIEWINFO pvi);

/*
 *** UpdateDisplay
 *
 * This routine is the interface to updating any list-of-LPIDENTs in the tool;
 * that includes the list-of-servers, the listviews and treeviews on each
 * of the server window's tabs, and things like aggregate lists and fileset
 * replica lists in various dialogs.
 *
 * If called with fWait=FALSE, the request is queued and performed on a
 * separate worker thread; if it's TRUE, the request is performed before
 * returning (as always, don't ever block the main thread this way).
 *
 * You'll need to pass in a filled-in DISPLAYREQUEST structure; this is copied
 * to local storage, so it can be a local variable even if !fWait.
 *
 */

void UpdateDisplay (LPDISPLAYREQUEST pdr, BOOL fWait);


/*
 *** UpdateDisplay_*
 *
 * These routines act as wrappers to UpdateDisplay(); they create an
 * appropriate DISPLAYREQUEST packet and pass it along.
 *
 */

void UpdateDisplay_Cell (BOOL fWait);
void UpdateDisplay_Servers (BOOL fWait, LPIDENT lpiNotify, ULONG status);
void UpdateDisplay_Services (BOOL fWait, HWND hChild, LPIDENT lpiNotify, ULONG status);
void UpdateDisplay_Aggregates (BOOL fWait, HWND hListOrCombo, LPIDENT lpiNotify, ULONG status, LPIDENT lpiServer, LPIDENT lpiToSelect, LPVIEWINFO lpvi);
void UpdateDisplay_Filesets (BOOL fWait, HWND hListOrCombo, LPIDENT lpiNotify, ULONG status, LPIDENT lpiServer, LPIDENT lpiAggregate, LPIDENT lpiToSelect);
void UpdateDisplay_Replicas (BOOL fWait, HWND hList, LPIDENT lpiRW, LPIDENT lpiRO);
void UpdateDisplay_ServerWindow (BOOL fWait, LPIDENT lpiServer);
void UpdateDisplay_SetIconView (BOOL fWait, HWND hDialog, LPICONVIEW piv, ICONVIEW ivNew);


#endif

