/* Copyright 1998 - Transarc Corporation */


/* NT does not have uio structs, so we roll our own sendmsg and recvmsg.
 *
 * The dangerous part of this code is that it assumes that iovecs 0 and 1
 * are contiguous and that all of 0 is used before any of 1.
 * This is true if rx_packets are being sent, so we should be ok.
 */

#include <afs/param.h>
#ifdef AFS_NT40_ENV

#include <winsock2.h>

#include "rx.h"
#include "rx_packet.h"
#include "rx_globals.h"
#include "rx_xmit_nt.h"
#include <malloc.h>
#include <errno.h>

int recvmsg(int socket, struct msghdr *msgP, int flags)
{
    char rbuf[RX_MAX_PACKET_SIZE];
    int size;
    int code;
    int off, i, n;
    int allocd = 0;


    size = rx_maxJumboRecvSize;
    code = recvfrom((SOCKET)socket, rbuf, size, flags,
		    (struct sockaddr*)(msgP->msg_name),
		    &(msgP->msg_namelen));

    if (code>0) {
	size = code;
	
	for (off = i = 0; size > 0 && i<msgP->msg_iovlen; i++) {
	    if (msgP->msg_iov[i].iov_len) {
		if (msgP->msg_iov[i].iov_len < size) {
		    n = msgP->msg_iov[i].iov_len;
		}
		else {
		    n = size;
		}
		memcpy(msgP->msg_iov[i].iov_base, &rbuf[off], n);
		off += n;
		size -= n;
	    }
	}

	/* Accounts for any we didn't copy in to iovecs. */
	code -= size;
    }
    else {
        code = -1;
    }

    return code;
}

int sendmsg(int socket, struct msghdr *msgP, int flags)
{
    char buf[RX_MAX_PACKET_SIZE];
    char *sbuf=buf;
    int size, tmp;
    int code;
    int off, i, n;
    int allocd = 0;

    for (size = i = 0; i<msgP->msg_iovlen; i++)
	size += msgP->msg_iov[i].iov_len;
	 
    if (msgP->msg_iovlen <= 2) {
	sbuf = msgP->msg_iov[0].iov_base;
    }
    else {
	/* Pack data into array from iovecs */
	tmp = size;
	for (off = i = 0; tmp > 0 && i<msgP->msg_iovlen; i++) {
	    if (msgP->msg_iov[i].iov_len > 0 ) {
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

    code = sendto((SOCKET)socket, sbuf, size, flags,
		  (struct sockaddr*)(msgP->msg_name), msgP->msg_namelen);

    if (code == SOCKET_ERROR) {
	code = WSAGetLastError();
	switch (code) {
	case WSAEINPROGRESS:
	case WSAENETRESET:
	case WSAEWOULDBLOCK:
	case WSAENOBUFS:
	    errno = 0;
	    break;
	default:
	    errno = EIO;
	    break;
	}
	code = -1;
    }

    if (code < size) {
	errno = EIO;
	code = -1;
    }

    return code;

}




#endif
