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


#if defined(AFS_NT40_ENV)

#include <winsock2.h>
#if (_WIN32_WINNT < 0x0501)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <mswsock.h>

#if (_WIN32_WINNT < 0x0600)
/*
 * WSASendMsg -- send data to a specific destination, with options, using
 *    overlapped I/O where applicable.
 *
 * Valid flags for dwFlags parameter:
 *    MSG_DONTROUTE
 *    MSG_PARTIAL (a.k.a. MSG_EOR) (only for non-stream sockets)
 *    MSG_OOB (only for stream style sockets) (NYI)
 *
 * Caller must provide either lpOverlapped or lpCompletionRoutine
 * or neither (both NULL).
 */
typedef
INT
(PASCAL FAR * LPFN_WSASENDMSG) (
    IN SOCKET s,
    IN LPWSAMSG lpMsg,
    IN DWORD dwFlags,
    __out_opt LPDWORD lpNumberOfBytesSent,
    IN LPWSAOVERLAPPED lpOverlapped OPTIONAL,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine OPTIONAL
    );

#define WSAID_WSASENDMSG /* a441e712-754f-43ca-84a7-0dee44cf606d */ \
    {0xa441e712,0x754f,0x43ca,{0x84,0xa7,0x0d,0xee,0x44,0xcf,0x60,0x6d}}
#endif /* _WINNT_WIN32 */

#include "rx.h"
#include "rx_packet.h"
#include "rx_globals.h"
#include "rx_xmit_nt.h"
#include <malloc.h>
#include <errno.h>


/*
 * WSASendMsg is only supported on Vista and above
 * Neither function is part of the public WinSock API
 * and therefore the function pointers must be
 * obtained via WSAIoctl()
 */
static LPFN_WSARECVMSG pWSARecvMsg = NULL;
static LPFN_WSASENDMSG pWSASendMsg = NULL;

void
rxi_xmit_init(osi_socket s)
{
    int rc;
    GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
    GUID WSASendMsg_GUID = WSAID_WSASENDMSG;
    DWORD dwIn, dwOut, NumberOfBytes;

    rc = WSAIoctl( s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                   &WSARecvMsg_GUID, sizeof(WSARecvMsg_GUID),
                   &pWSARecvMsg, sizeof(pWSARecvMsg),
                   &NumberOfBytes, NULL, NULL);

    rc = WSAIoctl( s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                   &WSASendMsg_GUID, sizeof(WSASendMsg_GUID),
                   &pWSASendMsg, sizeof(pWSASendMsg),
                   &NumberOfBytes, NULL, NULL);

    /* Turn on UDP PORT_UNREACHABLE messages */
    dwIn = 1;
    rc = WSAIoctl( s, SIO_UDP_CONNRESET,
                   &dwIn, sizeof(dwIn),
                   &dwOut, sizeof(dwOut),
                   &NumberOfBytes, NULL, NULL);

    /* Turn on UDP CIRCULAR QUEUEING messages */
    dwIn = 1;
    rc = WSAIoctl( s, SIO_ENABLE_CIRCULAR_QUEUEING,
                   &dwIn, sizeof(dwIn),
                   &dwOut, sizeof(dwOut),
                   &NumberOfBytes, NULL, NULL);
}

int
recvmsg(osi_socket socket, struct msghdr *msgP, int flags)
{
    int code;

    if (pWSARecvMsg) {
        WSAMSG wsaMsg;
        DWORD  dwBytes;

        wsaMsg.name = (LPSOCKADDR)(msgP->msg_name);
        wsaMsg.namelen = (INT)(msgP->msg_namelen);

        wsaMsg.lpBuffers = (LPWSABUF) msgP->msg_iov;
        wsaMsg.dwBufferCount = msgP->msg_iovlen;
        wsaMsg.Control.len = 0;
        wsaMsg.Control.buf = NULL;
        wsaMsg.dwFlags = flags;

        code = pWSARecvMsg(socket, &wsaMsg, &dwBytes, NULL, NULL);
        if (code == 0) {
            /* success - return the number of bytes read */
            code = (int)dwBytes;
        } else {
            /* error - set errno and return -1 */
            if (code == SOCKET_ERROR)
                code = WSAGetLastError();
            if (code == WSAEWOULDBLOCK || code == WSAECONNRESET)
                errno = WSAEWOULDBLOCK;
            else
                errno = EIO;
            code = -1;
        }
    } else {
        char rbuf[RX_MAX_PACKET_SIZE];
        int size;
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
            if (code == SOCKET_ERROR)
                code = WSAGetLastError();
            if (code == WSAEWOULDBLOCK || code == WSAECONNRESET)
                errno = WSAEWOULDBLOCK;
            else
                errno = EIO;
            code = -1;
        }
    }

    return code;
}

int
sendmsg(osi_socket socket, struct msghdr *msgP, int flags)
{
    int code;

    if (pWSASendMsg) {
        WSAMSG wsaMsg;
        DWORD  dwBytes;

        wsaMsg.name = (LPSOCKADDR)(msgP->msg_name);
        wsaMsg.namelen = (INT)(msgP->msg_namelen);

        wsaMsg.lpBuffers = (LPWSABUF) msgP->msg_iov;
        wsaMsg.dwBufferCount = msgP->msg_iovlen;
        wsaMsg.Control.len = 0;
        wsaMsg.Control.buf = NULL;
        wsaMsg.dwFlags = 0;

        code = pWSASendMsg(socket, &wsaMsg, flags, &dwBytes, NULL, NULL);
        if (code == 0) {
            /* success - return the number of bytes read */
            code = (int)dwBytes;
        } else {
            /* error - set errno and return -1 */
            if (code == SOCKET_ERROR)
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
        }
    } else {
        char buf[RX_MAX_PACKET_SIZE];
        char *sbuf = buf;
        int size, tmp;
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
        } else {
            if (code < size) {
                errno = EIO;
                code = -1;
            }
        }
    }
    return code;

}
#endif /* AFS_NT40_ENV */
