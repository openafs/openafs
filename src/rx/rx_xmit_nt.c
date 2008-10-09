/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* NT does not have uio structs, so we roll our own sendmsg and recvmsg.
 *
 * The dangerous part of this code is that it assumes that iovecs 0 and 1
 * are contiguous and that all of 0 is used before any of 1.
 * This is true if rx_packets are being sent, so we should be ok.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#if defined(AFS_NT40_ENV) || defined(AFS_DJGPP_ENV)

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
typedef int SOCKET;
#endif

#include "rx.h"
#include "rx_packet.h"
#include "rx_globals.h"
#include "rx_xmit_nt.h"
#ifdef AFS_NT40_ENV
#include <malloc.h>
#endif
#include <errno.h>

int
recvmsg(osi_socket socket, struct msghdr *msgP, int flags)
{
    char rbuf[RX_MAX_PACKET_SIZE];
    int size;
    int code;
    int off, i, n;
    int allocd = 0;


    size = rx_maxJumboRecvSize;
    code =
	recvfrom((SOCKET) socket, rbuf, size, flags,
		 (struct sockaddr *)(msgP->msg_name), &(msgP->msg_namelen));

    if (code > 0) {
	size = code;

	for (off = i = 0; size > 0 && i < msgP->msg_iovlen; i++) {
	    if (msgP->msg_iov[i].iov_len) {
		if (msgP->msg_iov[i].iov_len < size) {
		    n = msgP->msg_iov[i].iov_len;
		} else {
		    n = size;
		}
		memcpy(msgP->msg_iov[i].iov_base, &rbuf[off], n);
		off += n;
		size -= n;
	    }
	}

	/* Accounts for any we didn't copy in to iovecs. */
	code -= size;
    } else {
#ifdef AFS_NT40_ENV
	if (code == SOCKET_ERROR)
	    code = WSAGetLastError();
	if (code == WSAEWOULDBLOCK || code == WSAECONNRESET)
	    errno = WSAEWOULDBLOCK;
	else
	    errno = EIO;
#endif /* AFS_NT40_ENV */
	code = -1;
    }

    return code;
}

int
sendmsg(osi_socket socket, struct msghdr *msgP, int flags)
{
    char buf[RX_MAX_PACKET_SIZE];
    char *sbuf = buf;
    int size, tmp;
    int code;
    int off, i, n;
    int allocd = 0;

    for (size = i = 0; i < msgP->msg_iovlen; i++)
	size += msgP->msg_iov[i].iov_len;

    if (msgP->msg_iovlen <= 2) {
	sbuf = msgP->msg_iov[0].iov_base;
    } else {
	/* Pack data into array from iovecs */
	tmp = size;
	for (off = i = 0; tmp > 0 && i < msgP->msg_iovlen; i++) {
	    if (msgP->msg_iov[i].iov_len > 0) {
		if (tmp > msgP->msg_iov[i].iov_len)
		    n = msgP->msg_iov[i].iov_len;
		else
		    n = tmp;
		memcpy(&sbuf[off], msgP->msg_iov[i].iov_base, n);
		off += n;
		tmp -= n;
	    }
	}
    }

    code =
	sendto((SOCKET) socket, sbuf, size, flags,
	       (struct sockaddr *)(msgP->msg_name), msgP->msg_namelen);

#ifdef AFS_NT40_ENV
    if (code == SOCKET_ERROR) {
	code = WSAGetLastError();
	switch (code) {
	case WSAEINPROGRESS:
	case WSAENETRESET:
	case WSAENOBUFS:
	    errno = 0;
	    break;
	case WSAEWOULDBLOCK:
	case WSAECONNRESET:
	    errno = WSAEWOULDBLOCK;
	    break;
	case WSAEHOSTUNREACH:
		errno = WSAEHOSTUNREACH;
		break;
	default:
	    errno = EIO;
	    break;
	}
	code = -1;
    } else
#endif /* AFS_NT40_ENV */

    if (code < size) {
	errno = EIO;
	code = -1;
    }

    return code;

}
#endif /* AFS_NT40_ENV || AFS_DJGPP_ENV */
