/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CTL_SOCKADDR_H
#define CTL_SOCKADDR_H

/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef THIS_HINST
#define THIS_HINST  (HINSTANCE)GetModuleHandle(NULL)
#endif

#ifndef EXPORTED
#define EXPORTED
#endif

#ifndef limit
#define limit(_a,_x,_b)  min( max( (_x), (_a) ), (_b) )
#endif

#ifndef inlimit
#define inlimit(_a,_x,_b)  ( (((_x)>=(_a)) && ((_x)<=(_b))) ? TRUE : FALSE )
#endif

#ifndef cxRECT
#define cxRECT(_r)  ((_r).right - (_r).left)
#endif

#ifndef cyRECT
#define cyRECT(_r)  ((_r).bottom - (_r).top)
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

EXPORTED BOOL RegisterSockAddrClass (void);

#define SAM_GETADDR    (WM_USER+301) // SOCKADDR_IN *pAddr = lp;
#define SAM_SETADDR    (WM_USER+302) // SOCKADDR_IN *pAddr = lp;

#define SAN_CHANGE     0x1003  // SOCKADDR_IN *pTime = lp;
#define SAN_UPDATE     0x1004  // SOCKADDR_IN *pAddr = lp;

/*
 * void SA_GetAddr (HWND hSockAddr, SOCKADDR_IN *pAddr)
 * void SA_SetAddr (HWND hSockAddr, SOCKADDR_IN *pAddr)
 *
 */
#define SA_GetAddr(_hsa,_pAddr) \
        SendMessage(_hsa,SAM_GETADDR,(WPARAM)0,(LPARAM)_pAddr)
#define SA_SetAddr(_hsa,_pAddr) \
        SendMessage(_hsa,SAM_SETADDR,(WPARAM)0,(LPARAM)_pAddr)


#endif

