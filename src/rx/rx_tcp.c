/*
 * TCP transport for RX
 */

#include <afsconfig.h>
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#ifdef KERNEL
#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "rx/rx_kcommon.h"
#include "rx/rx_clock.h"
#include "rx/rx_queue.h"
#include "rx/rx_packet.h"
#else /* defined(UKERNEL) */
#ifdef RX_KERNEL_TRACE
#include "../rx/rx_kcommon.h"
#endif
#include "h/types.h"
#ifndef AFS_LINUX20_ENV
#include "h/systm.h"
#endif
#if defined(AFS_SGI_ENV) || defined(AFS_HPUX110_ENV)
#include "afs/sysincludes.h"
#endif
#if defined(AFS_OBSD_ENV)
#include "h/proc.h"
#endif
#include "h/socket.h"
#if !defined(AFS_SUN5_ENV) &&  !defined(AFS_LINUX20_ENV) && !defined(AFS_HPUX110_ENV)
#if	!defined(AFS_OSF_ENV) && !defined(AFS_AIX41_ENV)
#include "sys/mount.h"		/* it gets pulled in by something later anyway */
#endif
#include "h/mbuf.h"
#endif
#include "netinet/in.h"
#include "afs/afs_osi.h"
#include "rx_kmutex.h"
#include "rx/rx_clock.h"
#include "rx/rx_queue.h"
#ifdef	AFS_SUN5_ENV
#include <sys/sysmacros.h>
#endif
#include "rx/rx_packet.h"
#endif /* defined(UKERNEL) */
#include "rx/rx_globals.h"
#else /* KERNEL */
#include "sys/types.h"
#include <sys/stat.h>
#include <errno.h>
#if defined(AFS_NT40_ENV) || defined(AFS_DJGPP_ENV)
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif /* AFS_NT40_ENV */
#include "rx_xmit_nt.h"
#include <stdlib.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include "rx_clock.h"
#include "rx.h"
#include "rx_queue.h"
#ifdef	AFS_SUN5_ENV
#include <sys/sysmacros.h>
#endif
#include "rx_packet.h"
#include "rx_globals.h"
#include <lwp.h>
#include <assert.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif /* KERNEL */

#define RX_TCP_VERSION 0
#define RX_TCP_COMM_HEADER 12
#define RX_TCP_FORWARD (0 << 31)
#define RX_TCP_REVERSE (1 << 31)

#define RX_CALL_LAST_PACKET (1 << 30)

#define RX_TCP_FLAGS_MASK (0x7fffff00)

#define RX_STARTCONN 0
#define RX_ACKCONN 1
#define RX_ENDCONN 2
#define RX_START 3
#define RX_END 4
#define RX_DATA 5
#define RX_SEQ 6
#define RX_VERSION 7
#define RX_CHALLENGE 8
#define RX_RESPONSE 9
#define RX_USERSTATUS 10
#define RX_DEBUG 11
#define RX_ERROR 12

#define RX_TCP_DEFAULT_WINDOW	65536

static void rxi_StartTcpClient(struct rx_connection *);
static void rxi_CancelTcpConnection(struct rx_connection *);
static int rxi_TcpTimeoutRead(struct rx_connection *, unsigned char *, int,
			      int, int);
static int rxi_TcpReadPacket(struct rx_connection *, unsigned char *,
			     unsigned char *, struct rx_call **);
static void rxi_TcpEncodeUint32(afs_uint32, unsigned char *);
static void rxi_TcpDecodeUint32(unsigned char *, afs_uint32 *);
int rxi_TcpWritePacket(struct rx_connection *, int, unsigned char,
		       afs_uint32, afs_uint32, unsigned char *, int,
		       unsigned char *, int);

typedef struct _tcpReadQueue {
	struct rx_queue queue_header;
	unsigned char *data;
	unsigned char *ptr;
	afs_uint32 len;
	afs_uint32 size;
} tcpReadQueueEntry;

static tcpReadQueueEntry *rxi_TcpGetRQEntry(struct rx_connection *);
static void rxi_TcpPutRQEntry(struct rx_connection *, tcpReadQueueEntry *);

static struct rx_queue tcpFreeReadPackets;
static afs_kmutex_t tcpFreePacketLock;

static int rxi_tcp_max_frame = 8192;
static int rxi_tcp_enable_ack = 1;
static int rxi_tcp_send_bufsize = 0;
static int rxi_tcp_recv_bufsize = 0;
static int rxi_tcp_default_window = RX_TCP_DEFAULT_WINDOW;

/*
 * Note that technically these statistics should be locked, but we're
 * not doing that now.  Maybe later when we get into multi-call Rx
 */

int rxi_tcp_data_packets_sent = 0;
int rxi_tcp_ack_packets_sent = 0;
int rxi_tcp_data_packets_received = 0;
int rxi_tcp_ack_packets_received = 0;
int rxi_tcp_transmit_window_closed = 0;
int rxi_tcp_last_packets_sent = 0;
int rxi_tcp_short_circuit_read = 0;

/*
 * Setup variables needed for the RxTCP package.
 */

void rxi_TcpInit(void)
{
	int i;
	tcpReadQueueEntry *newentry;

	MUTEX_INIT(&tcpFreePacketLock, "rxtcp free packet queue",
		   MUTEX_DEFAULT, 0);
	MUTEX_ENTER(&tcpFreePacketLock);

	queue_Init(&tcpFreeReadPackets);

	/*
	 * Pre-allocate a pool of buffers initially
	 */

	for (i = 0; i < 32; i++) {
		newentry = malloc(sizeof(tcpReadQueueEntry));

		if (newentry) {
			queue_Append(&tcpFreeReadPackets, newentry);
		}
	}

	MUTEX_EXIT(&tcpFreePacketLock);
}

void
rxi_TcpNewConnection(struct rx_connection *conn)
{
	int s, flags, err;
	struct sockaddr_in sin;

	/*
	 * We're going to try to create a new TCP connection to the host
	 * If we get any errors, set the connector descriptor to -1 to
	 * indicate that we're falling back to UDP for this connection.
	 * All of the magic really happens inside of rxi_TcpConnect().
	 */

	MUTEX_ENTER(&conn->conn_call_lock);

	queue_Init(&conn->tcpCallQueue);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s < 0) {
		conn->tcpDescriptor = -1;
		MUTEX_EXIT(&conn->conn_call_lock);
		return;
	}

	conn->tcpDescriptor = s;
#if 0
	flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flagsd | O_NONBLOCK);
#endif

	memset((void *) &sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = rx_HostOf(rx_PeerOf(conn));
	sin.sin_port = rx_PortOf(rx_PeerOf(conn));

	if (rxi_tcp_send_bufsize)
		setsockopt(s, SOL_SOCKET, SO_SNDBUF, &rxi_tcp_send_bufsize,
			   sizeof(rxi_tcp_send_bufsize));

	if (rxi_tcp_recv_bufsize)
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rxi_tcp_recv_bufsize,
			   sizeof(rxi_tcp_recv_bufsize));

	err = connect(s, (struct sockaddr *) &sin, sizeof(sin));

	/*
	 * Three possible outcomes:
	 *
	 * No error (generally on the same host).
	 * EINPROGRESS (non-local connect)
	 * other error (something went wrong)
	 */

	if (err == 0) {
		conn->tcpDescriptor = s;
		rxi_StartTcpClient(conn);
		return;
	}

	if (err != EINPROGRESS) {
		conn->tcpDescriptor = -1;
		MUTEX_EXIT(&conn->conn_call_lock);
		return;
	}

	/* XXX */

	return;
}

static void
rxi_StartTcpClient(struct rx_connection *conn)
{
	int cc, length;
	unsigned char buf[32], pbuf[4096], *p, type;
	afs_uint32 *ibuf = (afs_uint32 *) buf, defaultwindow;
	struct rx_call *dummy;

	/*
	 * Start off by sending the version packet, receiving the
	 * version packet, and sending/receiving a STARTCONN/ACKCONN
	 * packet
	 */

	(*ibuf) = htonl(RX_TCP_VERSION);
	length = 4;

	cc = write(conn->tcpDescriptor, buf, length);

	if (cc != 4) {
		rxi_CancelTcpConnection(conn);
		MUTEX_EXIT(&conn->conn_call_lock);
		return;
	}

	cc = rxi_TcpTimeoutRead(conn, buf, length, 0, 0);

	if (cc != 4 || *ibuf != htonl(RX_TCP_VERSION)) {
		rxi_CancelTcpConnection(conn);
		MUTEX_EXIT(&conn->conn_call_lock);
		return;
	}

	p = pbuf;

	rxi_TcpEncodeUint32(conn->cid, p); p += sizeof(afs_uint32);
	rxi_TcpEncodeUint32(conn->serviceId, p); p += sizeof(afs_uint32);
	rxi_TcpEncodeUint32(rxi_tcp_default_window, p); p += sizeof(afs_uint32);

	if (rxi_TcpWritePacket(conn, RX_TCP_FORWARD, RX_STARTCONN, 0,
			       conn->epoch, pbuf,
			       sizeof(afs_uint32) * 3, 0, 0) < 0) {
		rxi_CancelTcpConnection(conn);
		MUTEX_EXIT(&conn->conn_call_lock);
		return;
	}

	conn->tcpPacketHeader = malloc(RX_TCP_COMM_HEADER);
	conn->tcpPacketCur = conn->tcpPacketHeader;
	conn->tcpPacketLen = 0;

	if ((length = rxi_TcpReadPacket(conn, &type, pbuf, &dummy)) <= 0) {
		rxi_CancelTcpConnection(conn);
		MUTEX_EXIT(&conn->conn_call_lock);
		return;
	}

	printf("Client packet type: %d\n", type);

	/*
	 * If we get a challenge packet, we need to handle it.  Feed it into
	 * our decoder 
	 */
	if (type == RX_CHALLENGE) {
		char *outbuf;
		int outlen;
		RXS_GetResponseStream(conn->securityObject, conn, pbuf, length,
				      (void **) &outbuf, &outlen);
		printf("Output buffer length: %d\n", outlen);

		if (rxi_TcpWritePacket(conn, RX_TCP_FORWARD, RX_RESPONSE,
				       0, 0, outbuf, outlen, 0, 0) < 0) {
			rxi_CancelTcpConnection(conn);
			MUTEX_EXIT(&conn->conn_call_lock);
			return;
		}

		osi_Free(outbuf, outlen);
	}

	rxi_TcpDecodeUint32(pbuf + 1, &defaultwindow);
	conn->tcpDefaultWindow = defaultwindow;

	rxi_TcpStartConnectionThreads(conn);
	MUTEX_EXIT(&conn->conn_call_lock);
	return;
}

/*
 * Start a new call.  Allocate all of the necessary buffers and
 * write the START message over the channel.  Note that we get called
 * with conn_call_lock locked, and at NETPRI;
 */

struct rx_call *rxi_TcpNewCall(struct rx_connection *conn)
{
	unsigned char buf[sizeof(afs_uint32)];
	afs_uint32 *ibuf = (afs_uint32 *) buf;
	struct rx_call *call;

	/*
 	 * If the connection failed (no descriptor), return NULL right away
	 */

	if (conn->tcpDescriptor == -1) {
		return NULL;
	}

	/*
	 * First off, allocate the call structure.  Right now rxi_NewCall()
	 * still uses "channels", which we don't.  So every call deals
	 * with a single channel (zero) for allocating call numbers.
	 * Note that rxi_NewCall() returns with the call locked.
	 */

	call = rxi_NewCall(conn, 0);

	/* 
	 * Setup the call number
	 */

	call->tcpCallNumber = ++conn->tcpNewCallNumber;

	ibuf[0] = htonl(call->tcpCallNumber);

	if (rxi_TcpWritePacket(call->conn, RX_TCP_FORWARD, RX_START, 0, 0,
			       buf, sizeof(afs_uint32), 0, 0) < 0) {
		rxi_CancelTcpConnection(call->conn);
		return NULL;
	}

	queue_Init(&call->tcpReadData);

#if 0
	call->tcpReadBuffer = malloc(65536);	/* XXX */
	call->tcpReadBufferSize = 65536;
	call->tcpReadBufferCur = call->tcpReadBuffer;
	call->tcpReadBufferLen = 0;
#endif
	call->tcpWriteBuffer = malloc(rxi_tcp_max_frame);
	call->tcpWriteBufferCur = call->tcpWriteBuffer;
	call->tcpWriteBufferSize = rxi_tcp_max_frame;
	call->tcpWriteBufferLen = 0;
	call->tcpWindow = call->conn->tcpDefaultWindow;
	call->tcpAdvertised = 1;

	CV_INIT(&call->reader_cv, "reader consume wakeup", CV_DEFAULT, 0);
	MUTEX_INIT(&call->reader_lock, "tcp reader lock", MUTEX_DEFAULT, 0);
	queue_Append(&conn->tcpCallQueue, call);

	MUTEX_EXIT(&call->lock);
	MUTEX_EXIT(&call->conn->conn_call_lock);

	return call;
}

#if 0
/*
 * Our per-connection write thread.
 */

void *
rxi_TcpWriteThread(void *arg)
{
	struct rx_connection *conn = (struct rx_connection *) arg;

	MUTEX_ENTER();
	MUTEX_EXIT();
}
#endif

/*
 * Our per-connection reader thread
 */

void *
rxi_TcpReaderThread(void *arg)
{
	struct rx_connection *conn = (struct rx_connection *) arg;
	int cc, i;
	afs_uint32 callid;
	unsigned char type, buf[32];
	struct rx_call *call;
	tcpReadQueueEntry *newentry;

	/*
	 * Run in a loop, continually reading data
	 */

	for (;;) {
		cc = rxi_TcpReadPacket(conn, &type, buf, &call);

		if (cc < 0) {
			printf("Connection closed (cc = %d)\n", cc);
			/*
			 * rxi_CancelTcpConnection() will take care
			 * of all of the connection cleanup (including
			 * cancelling waiting calls
			 */
			rxi_CancelTcpConnection(conn);
			return NULL;
		}

		/*
		 * Process the packet that we got
		 */

		switch (type) {
		case RX_START:
			/*
			 * A start packet means a new call.  Start
			 * by setting up the call framework.
			 */
			if (cc != sizeof(afs_uint32)) {
				printf("Call aborted, wrong size (%d)\n", cc);
				rxi_CancelTcpConnection(conn);
				return NULL;
			}

			rxi_TcpDecodeUint32(buf, &callid);
			printf("New call received (id %d)\n", callid);
			/*
			 * Allocate call (use either rxi_NewCall() or
			 * see code in rxi_ReceivePacket)
			 */
			MUTEX_ENTER(&conn->conn_call_lock);
			call = rxi_NewCall(conn, 0);	/* XXX */
			/*
			 * Note: rxi_NewCall returns with the call locked
			 */
			queue_Append(&conn->tcpCallQueue, call);
			MUTEX_EXIT(&call->lock);
			MUTEX_EXIT(&conn->conn_call_lock);
			call->tcpCallNumber = callid;
			hzero(call->bytesSent);
			hzero(call->bytesRcvd);
			queue_Init(&call->tcpReadData);
			call->tcpWindow = conn->tcpDefaultWindow;
			call->tcpAdvertised = 1;
			CV_INIT(&call->reader_cv, "reader consume wakeup",
				CV_DEFAULT, 0);
			MUTEX_INIT(&call->reader_lock, "tcp reader lock",
				   MUTEX_DEFAULT, 0);
			rx_TcpServerProc(call);
			/* XXX */
#if 0
			call->tcpReadBuffer = malloc(65536);	/* XXX */
			call->tcpReadBufferSize = 65536;
			call->tcpReadBufferCur = call->tcpReadBuffer;
			call->tcpReadBufferLen = 0;
#endif
			break;

		case RX_DATA:
			/*
			 * Data queueing is handled internally by
			 * rxi_TcpReadPacket, so this is a no-op.
			 */
			rxi_tcp_data_packets_received++;
		break;

		case RX_END:

			/*
			 * A call has completed.  Signal the top-end
			 * that the call is finished, with the return
			 * code.  If we're the server, queue a pseudo-packet
			 * with the return code in it (data == NULL,
			 * length == code).  If we're the client, mark
			 * EOF in the call mode.
			 */

			if (call->conn->type == RX_SERVER_CONNECTION) {
				tcpReadQueueEntry *entry;

				if (cc != sizeof(afs_uint32)) {
					printf("Call aborted, wrong size "
					       "(%d)\n", cc);
					rxi_CancelTcpConnection(conn);
					return NULL;
				}

				entry = rxi_TcpGetRQEntry(conn);

				entry->len = 0;
				rxi_TcpDecodeUint32(buf, &entry->size);

				MUTEX_ENTER(&call->reader_lock);
				queue_Append(&call->tcpReadData, newentry);
				MUTEX_EXIT(&call->reader_lock);
			} else {
				MUTEX_ENTER(&call->lock);
				call->mode = RX_MODE_EOF;
				MUTEX_EXIT(&call->lock);
			}
			CV_SIGNAL(&call->cv_rq);
		break;

		case RX_SEQ:
		{
			int wakeup = 0;
			/*
			 * Update the outstanding window information
			 */

			if (cc != sizeof(afs_uint32) * 2) {
				printf("call aborted, wrong size (%d)\n", cc);
				rxi_CancelTcpConnection(conn);
				return NULL;
			}
			MUTEX_ENTER(&call->lock);
			rxi_TcpDecodeUint32(buf, &call->tcpSentAck);
			rxi_TcpDecodeUint32(buf + 4, &call->tcpWindow);
			if (call->tcpWindow > 0 &&
						call->flags & RX_CALL_TQ_BUSY)
				wakeup = 1;
			MUTEX_EXIT(&call->lock);
			if (wakeup)
				CV_SIGNAL(&call->cv_twind);

			rxi_tcp_ack_packets_received++;
		}
		break;
		default:
			printf("Unknown type %d\n", type);
		}
	}
}

static int
rxi_TcpTimeoutRead(struct rx_connection *conn, unsigned char *buf, int length,
		   int fd, int timeout)
{
	struct timeval tv;
	fd_set readfd;
	int cc, cur = 0, s = conn ? conn->tcpDescriptor : fd;

	tv.tv_sec = conn ? conn->secondsUntilDead : timeout;
	tv.tv_usec = 0;

	FD_ZERO(&readfd);
	FD_SET(s, &readfd);

	do {
		cc = select(s + 1, &readfd, NULL, NULL, &tv);

		if (cc < 0) {
			if (conn)
				rxi_CancelTcpConnection(conn);
			return -1;
		}

		if (cc == 0) {
			if (conn)
				rxi_CancelTcpConnection(conn);
			return -1;
		}

		cc = read(s, buf + cur, length - cur);

		if (cc < 0) {
			return -1;
		}

		cur += cc;
	} while (cur < length);

	return cur;
}

/*
 * Write out a RxTCP packet.
 *
 * Each packet has the following format:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |D|L|                 Reserved                  |  Packet type  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Length (minus header)                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Call number (may be zero)                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Flags:
 * D - Direction (0 = "forward", 1 = "reverse")
 * L - Last packet (used to indicate that call has turned)
 */

int
rxi_TcpWritePacket(struct rx_connection *conn, int direction,
		   unsigned char type, afs_uint32 flags,
		   afs_uint32 callno, unsigned char *data, int datalen,
		   unsigned char *extradata, int extradatalen)
{

	afs_uint32 header[RX_TCP_COMM_HEADER / 4];
	struct iovec iov[3];
	int cc;

	memset(header, 0, RX_TCP_COMM_HEADER);

	header[0] = (flags & RX_TCP_FLAGS_MASK) | type | direction;
	header[1] = htonl(datalen + (extradata ? extradatalen : 0));
	header[2] = htonl(callno);

	iov[0].iov_base = (caddr_t) header;
	iov[0].iov_len = RX_TCP_COMM_HEADER;

	iov[1].iov_base = (caddr_t) data;
	iov[1].iov_len = datalen;

	if (extradata) {
		iov[2].iov_base = (caddr_t) extradata;
		iov[2].iov_len = extradatalen;
	}

	cc = writev(conn->tcpDescriptor, iov, extradata ? 3 : 2);

	if (cc < 0) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return -1;
	}

	switch (type) {
		case RX_DATA:
			rxi_tcp_data_packets_sent++;
			break;
		case RX_SEQ:
			rxi_tcp_ack_packets_sent++;
			break;
	}

	return cc;
}

/*
 * Read (and parse) a RxTCP packet
 *
 * There are two buffers we care about:
 *
 * 1) rx_connection->tcpPacketHeader
 *
 *    This contains the current packet _header_ for this connection.
 *    A complete header is always attempted to be read.  The variable
 *    tcpPacketCur points to the current position inside of the packet
 *    header buffer, and tcpPacketLen contains the current length of
 *    the packet header buffer.
 *
 * 2) call->tcpReadBuffer
 *
 *  This is the current read data buffer.  RX_DATA buffer payloads end up in
 *  this buffer (all other buffer data is returned by rxi_TcpReadPacket).
 *
 *  Buffer management is as follows;
 *
 *   -----------------------------------------
 *  |------tcpReadBufferSize----------------->|
 *  |------tcpReadBufferLen----->|            |
 *  |              |--gotnow---->|            |
 *  |              |                          |
 *   -----------------------------------------
 *  ^              ^
 *  |              |
 *  tcpReadBuffer  tcpReadBufferCur
 *
 *  tcpReadBuffer is always the start of the buffer
 *  tcpReadBufferSize is always the full size of the buffer
 *  tcpReadBufferLen is the amount of data in the buffer
 *  tcpReadBufferCur is a pointer to the data that we haven't consumed yet
 *
 *  gotnow is a local variable (amount of data left)
 *
 *  General rules:
 *  We only increment tcpReadBufferCur when we "consume" data
 *  If we consume all of the buffer, we reset Cur and Len
 *  If we get to the end of the buffer and we need more data, then
 *  reset (using memmove) everything back to the beginning.
 *
 */

static int
rxi_TcpReadPacket(struct rx_connection *conn, unsigned char *type,
		  unsigned char *buf, struct rx_call **call)
{
	afs_uint32 packetlen, callno;
	unsigned char pflags;
	int cc, data_needed, gotnow;
	struct iovec iov[2];
	tcpReadQueueEntry *newentry;

	/*
	 * Let's see if we have any data in our header buffer.  If we
	 * we have a full buffer, then break out and read in the data
	 * portion of the packet.
	 */

	while (conn->tcpPacketLen < RX_TCP_COMM_HEADER) {

		cc = read(conn->tcpDescriptor, conn->tcpPacketCur,
			  RX_TCP_COMM_HEADER - conn->tcpPacketLen);

		if (cc <= 0) {
			return -1;	/* XXX */
		}

		conn->tcpPacketLen += cc;
		conn->tcpPacketCur += cc;
	}

	*type = *(conn->tcpPacketHeader + 3);
	if (*type == 0) {
		printf("Aborting, %d data packets received\n",
		       rxi_tcp_data_packets_received);
		abort();
	}
	rxi_TcpDecodeUint32(conn->tcpPacketHeader + 4, &packetlen);
	pflags = conn->tcpPacketHeader[0];

	/*
	 * Since we've consumed the packet header, reset it now
	 */

	conn->tcpPacketLen = 0;
	conn->tcpPacketCur = conn->tcpPacketHeader;

	/*
	 * If we got a packet that has a call number, decode that now.
	 * If it's not an RX_START packet (which creates a new call)
	 * then try to find our call in the call list.
	 */

	if (*type == RX_DATA || *type == RX_START || *type == RX_END ||
	    *type == RX_SEQ) {
		rxi_TcpDecodeUint32(conn->tcpPacketHeader + 8,
				    &callno);

		if (*type != RX_START) {
			int i;
			struct rx_call *c, *np;

			for (queue_Scan(&conn->tcpCallQueue, c, np, rx_call))
				if (c->tcpCallNumber == callno) {
					*call = c;
					break;
				}

			if (*call == NULL) {
				printf("Unknown callid (%d), aborting\n",
				       callno);
				abort();
			}
		}
	}

	/*
	 * We now know how much data is available.  If we're doing a
	 * RX_DATA packet, handle that as a special case.
	 */

	if (*type == RX_DATA) {
		newentry = rxi_TcpGetRQEntry(conn);
		iov[0].iov_base = (caddr_t) newentry->data;

	} else {
		iov[0].iov_base = (caddr_t) buf;
		if (packetlen > 32)
			printf("Warning: using buf, but packetlen = %d\n",
			       packetlen);
	}

	iov[0].iov_len = data_needed = packetlen;

	iov[1].iov_base = (caddr_t) conn->tcpPacketHeader;
	iov[1].iov_len = RX_TCP_COMM_HEADER;

	/*
	 * Build a loop and read in the data.  This could get us in a
	 * bad spot, if we run into cases where there is no data to be
	 * sent and no additional packet coming for a while.  However,
	 * almost all packets have additional data after the header.  We
	 * might need to change this if this assumption changes!  This
	 * would be an easy fix: don't read more data if the packet type
	 * doesn't have any additional data (which might be inefficient,
	 * but I would suspect it's not a common case).
	 */

	while (data_needed > 0) {
		cc = readv(conn->tcpDescriptor, iov, 2);

		if (cc < 0) {
			fprintf(stderr, "Read error: %s\n", strerror(errno));
			return -1;
		}

		if (cc == 0) {
			fprintf(stderr, "Connection closed\n");
			return -1;
		}

		if (cc > data_needed) {
			conn->tcpPacketCur = conn->tcpPacketHeader +
					     (cc - data_needed);
			conn->tcpPacketLen = cc - data_needed;
			data_needed = 0;
			rxi_tcp_short_circuit_read++;
		} else {
			iov[0].iov_base = (caddr_t) iov[0].iov_base + cc;
			data_needed -= cc;
			iov[0].iov_len = data_needed;
		}

	}

	/*
	 * If we've received a RX_DATA packet, then enqueue it on the
	 * interface.
	 */

	if (*type == RX_DATA) {
		int queued, curfill;

		MUTEX_ENTER(&(*call)->reader_lock);
		newentry->len = newentry->size = packetlen;
		curfill = (*call)->tcpBufferFill += packetlen;
		queue_Append(&(*call)->tcpReadData, newentry);
		queued = (*call)->flags & RX_CALL_READER_WAIT;

		/*
		 * If we're on the last packet, then queue a zero-length
		 * "end" packet so we know that we're done
		 */

		if (pflags & (RX_CALL_LAST_PACKET >> 24)) {
			(*call)->flags |= RX_CALL_HAVE_LAST;
			newentry = rxi_TcpGetRQEntry(conn);
			newentry->len = 0;
			newentry->size = 0;
			queue_Append(&(*call)->tcpReadData, newentry);
			printf("got last packet, call %d\n",
			       (*call)->tcpCallNumber);
		}

		MUTEX_EXIT(&(*call)->reader_lock);
		if (queued)
			CV_SIGNAL(&(*call)->cv_rq);

	} 

	return packetlen;
}

/*
 * Get a free read queue entry
 */

static tcpReadQueueEntry *
rxi_TcpGetRQEntry(struct rx_connection *conn)
{
	tcpReadQueueEntry *newentry;

	MUTEX_ENTER(&tcpFreePacketLock);

	if (queue_IsNotEmpty(&tcpFreeReadPackets)) {
		newentry = queue_First(&tcpFreeReadPackets, _tcpReadQueue);
		queue_Remove(newentry);
	} else {
		newentry = (tcpReadQueueEntry *)
				osi_alloc(sizeof(tcpReadQueueEntry));
		newentry->data = NULL;
	}

	if (newentry->data == NULL)
		newentry->data = (unsigned char *)
					osi_alloc(rxi_tcp_max_frame);

	MUTEX_EXIT(&tcpFreePacketLock);

	newentry->ptr = newentry->data;

	return newentry;
}

/*
 * Return a free read queue entry back to the pool
 */

static void
rxi_TcpPutRQEntry(struct rx_connection *conn, tcpReadQueueEntry *entry)
{
	MUTEX_ENTER(&tcpFreePacketLock);

	if (queue_IsOnQueue(entry))
		queue_MoveAppend(&tcpFreeReadPackets, entry);
	else
		queue_Append(&tcpFreeReadPackets, entry);

	MUTEX_EXIT(&tcpFreePacketLock);
}

/*
 * Our TCP read glue logic.
 *
 * If we have data on our read queue, then we can consume it.  If we
 * run out of data, then wait.  If we make enough buffer space available,
 * then send a SEQ packet.
 */

int
rxi_TcpReadProc(struct rx_call *call, char *buf, int bytes)
{
	int bytesret = 0, data_copied;
	tcpReadQueueEntry *entry;

	if (call->error || call->flags & RX_CALL_RECEIVE_DONE)
		return 0;

	while (bytes > 0) {
		MUTEX_ENTER(&call->reader_lock);

		while (queue_IsEmpty(&call->tcpReadData)) {
			call->flags |= RX_CALL_READER_WAIT;
			CV_WAIT(&call->cv_rq, &call->reader_lock);
			call->flags &= ~RX_CALL_READER_WAIT;
		}

		entry = queue_First(&call->tcpReadData, _tcpReadQueue);

		MUTEX_EXIT(&call->reader_lock);

		/*
		 * If we get a special "null" packet, then that's the end of
		 * the call.  Be sure to set the error code.
		 */

		if (entry->len == 0) {
			call->error = entry->size;
			call->flags |= RX_CALL_RECEIVE_DONE;
			MUTEX_ENTER(&call->reader_lock);
			rxi_TcpPutRQEntry(call->conn, entry);
			MUTEX_EXIT(&call->reader_lock);
			return bytesret;
		}

		/*
		 * Copy data from the packet to the output buffer
		 */
		
		data_copied = bytes > entry->len ? entry->len : bytes;

		memcpy(buf, entry->ptr, data_copied);

		bytesret += data_copied;
		bytes -= data_copied;

		if (data_copied < entry->len) {
			entry->len -= data_copied;
			entry->ptr += data_copied;
		} else {
			buf += data_copied;
			MUTEX_ENTER(&call->reader_lock);
			call->tcpBufferFill -= entry->size;
			call->tcpRecvAck += entry->size;
			rxi_TcpPutRQEntry(call->conn, entry);
			MUTEX_EXIT(&call->reader_lock);
		}

	}

	/*
	 * So now we're returning "bytesret" bytes, which means we have
	 * "bytesret" more buffer space available.  Compare the available
	 * window size to the window size known to the peer (advertised
	 * window minus next packet).  Send a SEQ _if_:
	 *
	 * - The difference is at least two max segments
	 *   ... or ...
	 * - The difference is at least 50% of the available window size.
	 */ 

	if (rxi_tcp_enable_ack) {
		afs_uint32 opened;

		/*
		 * How many bytes has the window opened by?  We can
		 * calculate this by taking the remaining buffer
		 * space (rxi_tcp_default_window - call->tcpBufferFill)
		 * and subtracting the amount of data left in
		 * the window (call->tcpAdvertised - call->tcpRecvAck)
		 */

		opened = (rxi_tcp_default_window - call->tcpBufferFill) -
			 (call->tcpAdvertised - call->tcpRecvAck);

		if (/* opened >= rxi_tcp_max_frame * 2 || */
		    opened * 2 >= rxi_tcp_default_window) {
			unsigned char out[sizeof(afs_uint32) * 2];
			rxi_TcpEncodeUint32(call->tcpRecvAck, out);
			call->tcpAdvertised = call->tcpRecvAck + 1;
			rxi_TcpEncodeUint32(rxi_tcp_default_window -
					    call->tcpBufferFill, out + 4);
			rxi_TcpWritePacket(call->conn, RX_TCP_FORWARD, RX_SEQ,
					   0, call->tcpCallNumber, out,
					   sizeof(out), NULL, 0);
		}
	}

	return bytesret;
}

/*
 * Our user-side write procedure.  Queue data on the outgoing connection,
 * and wait if the remote side is full.
 */

int
rxi_TcpWriteProc(struct rx_call *call, char *buf, int bytes)
{
	int cc, obytes = bytes;

	while (bytes > 0) {

		/*
		 * If we don't have any data in the send buffer, then
		 * we can write it directly from the user's buffer, which
		 * is a reasonable optimization.
		 */

		if (bytes >= rxi_tcp_max_frame &&
					call->tcpWriteBufferLen == 0) {

			if (rxi_tcp_enable_ack) {
				MUTEX_ENTER(&call->lock);
				while (call->tcpWindow -
				       (call->tcpBytesSent - call->tcpSentAck) <
							rxi_tcp_max_frame) {
					rxi_tcp_transmit_window_closed++;
					call->flags |= RX_CALL_TQ_BUSY;
					CV_WAIT(&call->cv_twind, &call->lock);
					call->flags &= ~RX_CALL_TQ_BUSY;
				}
				MUTEX_EXIT(&call->lock);
			}

			cc = rxi_TcpWritePacket(call->conn, RX_TCP_FORWARD,
						RX_DATA, 0, call->tcpCallNumber,
						(unsigned char *) buf,
						rxi_tcp_max_frame, 0, 0);
			if (cc <= 0) {
				fprintf(stderr, "Error: %s\n", strerror(errno));
				exit(1);
			}

			bytes -= rxi_tcp_max_frame;
			buf += rxi_tcp_max_frame;
		} else if (bytes + call->tcpWriteBufferLen >=
							rxi_tcp_max_frame) {

			if (rxi_tcp_enable_ack) {
				MUTEX_ENTER(&call->lock);
				while (call->tcpWindow - (call->tcpBytesSent -
							  call->tcpSentAck) <
							rxi_tcp_max_frame) {
					rxi_tcp_transmit_window_closed++;
					call->flags |= RX_CALL_TQ_BUSY;
					CV_WAIT(&call->cv_twind, &call->lock);
					call->flags &= ~RX_CALL_TQ_BUSY;
				}
				MUTEX_EXIT(&call->lock);
			}

			cc = rxi_TcpWritePacket(call->conn, RX_TCP_FORWARD,
						RX_DATA, 0, call->tcpCallNumber,
						call->tcpWriteBuffer,
						call->tcpWriteBufferLen,
						(unsigned char *) buf,
						rxi_tcp_max_frame -
						    call->tcpWriteBufferLen);

			if (cc <= 0) {
				fprintf(stderr, "Error: %s\n", strerror(errno));
				exit(1);
			}

			bytes -= rxi_tcp_max_frame - call->tcpWriteBufferLen;
			buf += rxi_tcp_max_frame - call->tcpWriteBufferLen;

			call->tcpWriteBufferCur = call->tcpWriteBuffer;
			call->tcpWriteBufferLen = 0;
		} else {
			memcpy(call->tcpWriteBufferCur, buf, bytes);
			call->tcpWriteBufferCur += bytes;
			call->tcpWriteBufferLen += bytes;
			bytes = 0;
		}
	}

	call->tcpBytesSent += obytes;

	return obytes;
}

/*
 * Flush out the data on the write channel.  Note that this means that
 * the call has "turned", and we're not sending any more data.
 */

void
rxi_TcpFlushWrite(struct rx_call *call)
{
	int cc = 0;

	/*
	 * Sleep if we don't have enough remote buffer space available
	 */

	MUTEX_ENTER(&call->lock);
	while (call->tcpWindow - (call->tcpBytesSent - call->tcpSentAck) <
						call->tcpWriteBufferLen) {
		call->flags |= RX_CALL_TQ_BUSY;
		rxi_tcp_transmit_window_closed++;
		CV_WAIT(&call->cv_twind, &call->lock);
		call->flags &= ~RX_CALL_TQ_BUSY;
	}
	MUTEX_EXIT(&call->lock);

	/*
	 * Only send a packet if we have data, or we're the client
	 * side.
	 */

	if (call->tcpWriteBufferLen ||
		call->conn->type == RX_CLIENT_CONNECTION) {
		cc = rxi_TcpWritePacket(call->conn, RX_TCP_FORWARD, RX_DATA,
					RX_CALL_LAST_PACKET,
					call->tcpCallNumber,
					call->tcpWriteBuffer,
					call->tcpWriteBufferLen, NULL, 0);

		if (cc < 0) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			exit(1);
		}
	}

	/*
	 * Note: maybe later we'll allocate/deallocate buffers on the
	 * client side.
	 */

	return;
}

afs_int32
rxi_TcpEndCall(struct rx_call *call, afs_int32 rc)
{
	int cc;
	afs_int32 retcode = 0;

	/*
	 * Make sure we flush out the data connection
	 */

	rx_FlushWrite(call);

	/*
	 * If we're a client connection, then we should wait for
	 * the server to send us a RX_END packet
	 */

	MUTEX_ENTER(&call->lock);

	if (call->conn->type == RX_CLIENT_CONNECTION) {
		while (call->mode != RX_MODE_EOF) {
			call->flags |= RX_CALL_READER_WAIT;
			CV_WAIT(&call->cv_rq, &call->lock);
			call->flags &= ~RX_CALL_READER_WAIT;
		}
		retcode = call->error;
		MUTEX_EXIT(&call->lock);
	} else {
		retcode = htonl(rc);

		MUTEX_EXIT(&call->lock);

		cc = rxi_TcpWritePacket(call->conn, RX_TCP_FORWARD, RX_END,
					0, call->tcpCallNumber,
					(unsigned char *) &retcode,
					sizeof(retcode), NULL, 0);
		printf("Sending end packet (call = %d, cc = %d)\n",
		       call->tcpCallNumber, cc);
	}

	CALL_RELE(call, RX_CALL_REFCOUNT_BEGIN);

	MUTEX_ENTER(&call->conn->conn_call_lock);
	queue_Remove(call);
	MUTEX_EXIT(&call->conn->conn_call_lock);

	return ntoh_syserr_conv(retcode);
}


static void
rxi_TcpEncodeUint32(afs_uint32 num, unsigned char *buf)
{
	*buf++ = (num >> 24) & 0xff;
	*buf++ = (num >> 16) & 0xff;
	*buf++ = (num >> 8) & 0xff;
	*buf = num & 0xff;

	return;
}

static void
rxi_TcpDecodeUint32(unsigned char *buf, afs_uint32 *num)
{
	*num = (*buf << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	return;
}

static void
rxi_CancelTcpConnection(struct rx_connection *conn)
{
	return;
}

/*
 * Setup a new server connection
 */

void *
rxi_TcpNewServerConnection(void *arg)
{
	int s = (int) arg, cc;
	afs_uint32 epoch, cid, service, callid, defaultwindow;
	u_char buffer[RX_TCP_COMM_HEADER + sizeof(afs_uint32) * 3], *p, type;
	afs_int32 host;
	u_short port;
	struct rx_connection *conn;
	struct rx_call *call;
	struct sockaddr_storage ss;
	socklen_t namelen = sizeof(ss);

	if (getpeername(s, (struct sockaddr *) &ss, &namelen) < 0) {
		close(s);
		return NULL;
	}

	/*
	 * We don't actually have a connection yet.  Do all of the setup
	 * stuff and then call rxi_FindConnection().
	 */

	cc = rxi_TcpTimeoutRead(NULL, buffer, 4, s, 30);

	if (cc < 4) {
		close(s);
		return NULL;
	}

	*((afs_uint32 *) buffer) = htonl(RX_TCP_VERSION);

	cc = write(s, buffer, 4);

	if (cc < 4) {
		close(s);
		return NULL;
	}

	/*
	 * Right now ignore the version number (XXX).  We can't use
	 * rxi_TcpReadPacket, since it wants a rx_connection structure, and
	 * we don't have that yet.
	 */

	cc = rxi_TcpTimeoutRead(NULL, buffer, sizeof(buffer), s, 30);

	/*
	 * Make sure we have a STARTCONN packet
	 */

	if (buffer[3] != RX_STARTCONN) {
		close(s);
		return NULL;
	}

	rxi_TcpDecodeUint32(&buffer[4], &epoch);

	if (epoch != sizeof(afs_uint32) * 3) {
		close(s);
		return NULL;
	}

	rxi_TcpDecodeUint32(&buffer[8], &epoch);
	rxi_TcpDecodeUint32(&buffer[12], &cid);
	rxi_TcpDecodeUint32(&buffer[16], &service);
	rxi_TcpDecodeUint32(&buffer[20], &defaultwindow);

	printf("Epoch = %d, cid = %d, service = %d\n", epoch, cid, service);

	if ((conn = rxi_FindConnection(OSI_NULLSOCKET, &ss, namelen,
				       SOCK_STREAM, service, cid, epoch,
				       RX_SERVER_CONNECTION, 0)) == NULL) {
		close(s);
		return NULL;
	}

	printf("Got new connection structure\n");
	conn->tcpDescriptor = s;
	conn->tcpPacketHeader = malloc(RX_TCP_COMM_HEADER);
	conn->tcpPacketCur = conn->tcpPacketHeader;
	conn->tcpPacketLen = 0;

	/*
	 * Create our security challenge, and send it over the wire
	 */

	if (RXS_CheckAuthentication(conn->securityObject, conn) != 0) {
		void *outpacket;
		int outlen, error;
		unsigned char buffer[4096], type;

		RXS_CreateChallenge(conn->securityObject, conn);
		RXS_GetChallengeStream(conn->securityObject, conn,
				       &outpacket, &outlen);

		if (rxi_TcpWritePacket(conn, RX_TCP_FORWARD, RX_CHALLENGE,
				       0, 0, outpacket, outlen, 0, 0) < 0) {
			rxi_CancelTcpConnection(conn);
			return NULL;
		}

		osi_Free(outpacket, outlen);

		/*
		 * We should get a response back, including the ticket.
		 * Process that now.
		 */

		cc = rxi_TcpReadPacket(conn, &type, buffer, NULL);

		if (cc <= 0) {
			rxi_CancelTcpConnection(conn);
			return NULL;
		}

		if (type != RX_RESPONSE) {
			printf("Wrong packet type for security response (%d)\n",
			       type);
			rxi_CancelTcpConnection(conn);
			return NULL;
		}

		error = RXS_CheckResponseStream(conn->securityObject, conn,
						buffer, cc);

		if (error) {
			fprintf(stderr, "CheckResponseStream failed: %d\n",
				error);
			rxi_CancelTcpConnection(conn);
			return NULL;
		}
	}

	conn->tcpDefaultWindow = defaultwindow;

	buffer[0] = 1; /* numcalls */
	rxi_TcpEncodeUint32(rxi_tcp_default_window, &buffer[1]); /*winsize*/

	if (rxi_TcpWritePacket(conn, RX_TCP_FORWARD, RX_ACKCONN, 0, 0,
			       buffer, sizeof(afs_uint32) + 1, 0, 0) < 0) {
		rxi_CancelTcpConnection(conn);
		return NULL;
	}

	printf("Successful exchange\n");

	queue_Init(&conn->tcpCallQueue);
#if 0
	conn->tcpReadBuffer = malloc(32768);
	conn->tcpReadBufferSize = 32768;
	conn->tcpReadBufferCur = conn->tcpReadBuffer;
	conn->tcpReadBufferLen = 0;

	for (;;) {
		if ((cc = rxi_TcpReadPacket(conn, &type, &p)) <= 0)
			return NULL;

		switch (type) {
		case RX_DATA:
			break;
		default:
			printf("Unknown packet type: %d\n", type);
		}
	}

#endif
	rxi_TcpStartConnectionThreads(conn);
	return NULL;
}

osi_socket
rxi_GetHostTCPSocket(struct sockaddr_storage *saddr, int salen)
{
	osi_socket socketFd = OSI_NULLSOCKET;
	int code;

	socketFd = socket(saddr->ss_family, SOCK_STREAM, 0);

	if (socketFd < 0) {
		perror("socket");
		socketFd = OSI_NULLSOCKET;
		goto error;
	}

	code = bind(socketFd, (struct sockaddr *) saddr, salen);

	if (code) {
		perror("bind");
		goto error;
	}

	if (rxi_tcp_send_bufsize)
		setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF,
			   &rxi_tcp_send_bufsize, sizeof(rxi_tcp_send_bufsize));

	if (rxi_tcp_recv_bufsize)
		setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF,
			   &rxi_tcp_recv_bufsize, sizeof(rxi_tcp_recv_bufsize));

	if (listen(socketFd, 5) < 0) {
		perror("listen");
		goto error;
	}

	if (rxi_ListenTcp(socketFd) < 0) {
		goto error;
	}

	return socketFd;

error:
	if (socketFd != OSI_NULLSOCKET)
		close(socketFd);

	return OSI_NULLSOCKET;
}

/*
 * Invoke the necessary callback when we get a connection
 */

void *
rxi_TcpServerProc(void *p)
{
	struct rx_call *call = (struct rx_call *) p;
	afs_int32 code;
	struct rx_service *tservice = NULL;

	tservice = call->conn->service;

	if (tservice->beforeProc)
		(*tservice->beforeProc) (call);

	code = call->conn->service->executeRequestProc(call);

	if (tservice->afterProc)
		(*tservice->afterProc) (call, code);

	/*
	 * Send call termination packet (rx_EndCall does this for us).
	 */

	rx_EndCall(call, code);

	return NULL;
}

/*
 * Note: behavior undefined if this is done after the connection
 * has started
 */

void
rx_TcpSetFrameSize(int size)
{
	rxi_tcp_max_frame = size;
}

void
rx_TcpSetFlowControl(int flag)
{
	rxi_tcp_enable_ack = flag;
}

void
rx_TcpSetSendBufsize(int size)
{
	rxi_tcp_send_bufsize = size;
}

void
rx_TcpSetRecvBufsize(int size)
{
	rxi_tcp_recv_bufsize = size;
}

void
rx_TcpSetWindowSize(int size)
{
	rxi_tcp_default_window = size;
}
