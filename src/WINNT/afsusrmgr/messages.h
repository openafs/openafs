/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef MESSAGES_H
#define MESSAGES_H

// WM_SHOW_YOURSELF is posted to g.hMain to cause the main window to be
// displayed once a cell has been selected and successfully opened.
//
//    BOOL fForce = (BOOL)lp; // if !fForce, only show if g.idCell set
//
#define WM_SHOW_YOURSELF          (WM_USER + 0x100)

// WM_SHOW_ACTIONS is posted to g.hMain to cause the main window to open
// its Operations In Progress window.
//
#define WM_SHOW_ACTIONS           (WM_USER + 0x101)


#endif

