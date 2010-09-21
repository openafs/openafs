/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX:  Globals for internal use, basically */

/* This controls the size of an fd_set; it must be defined early before
 * the system headers define that type and the macros that operate on it.
 * Its value should be as large as the maximum file descriptor limit we
 * are likely to run into on any platform.  Right now, that is 65536
 * which is the default hard fd limit on Solaris 9 */
#if !defined(_WIN32) && !defined(KERNEL)
#define FD_SETSIZE 65536
#endif

#include <afsconfig.h>
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif


/* Enable data initialization when the header file is included */
#define GLOBALSINIT(stuff) = stuff
#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#define EXT __declspec(dllexport)
#define EXT2 __declspec(dllexport)
#else
#define EXT
#define EXT2
#endif

#ifdef KERNEL
#ifndef UKERNEL
#include "h/types.h"
#else /* !UKERNEL */
#include	"afs/sysincludes.h"
#endif /* UKERNEL */
#endif /* KERNEL */

#include "rx_globals.h"

void rx_SetMaxReceiveWindow(int packets)
{
    if (packets > rx_maxWindow)
	packets = rx_maxWindow;

    rx_maxReceiveWindow = packets;
}

int rx_GetMaxReceiveWindow(void)
{
    return rx_maxReceiveWindow;
}

void rx_SetMaxSendWindow(int packets)
{
    if (packets > rx_maxWindow)
	packets = rx_maxWindow;

    rx_maxSendWindow = packets;
}

int rx_GetMaxSendWindow(void)
{
    return rx_maxSendWindow;
}

void rx_SetMinPeerTimeout(int timeo)
{
    if (timeo >= 1 && timeo < 1000)
        rx_minPeerTimeout = timeo;
}

int rx_GetMinPeerTimeout(void)
{
    return rx_minPeerTimeout;
}

#ifdef AFS_NT40_ENV

void rx_SetRxDeadTime(int seconds)
{
    rx_connDeadTime = seconds;
}

int rx_GetMinUdpBufSize(void)
{
    return 64*1024;
}

void rx_SetUdpBufSize(int x)
{
    if (x > rx_GetMinUdpBufSize())
        rx_UdpBufSize = x;
}

#endif /* AFS_NT40_ENV */
