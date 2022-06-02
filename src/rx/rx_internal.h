/* Internal structures that are private to RX itself. These shouldn't be
 * modified by library callers.
 *
 * Data structures that are visible to security layers, but not to
 * customers of RX belong in rx_private.h, which is installed.
 */

#ifdef AFS_RXERRQ_ENV
# if defined(AFS_LINUX_ENV) || defined(AFS_USR_LINUX_ENV)
#  include <linux/types.h>
#  include <linux/errqueue.h>
#  include <linux/icmp.h>
# endif
#endif

/* Globals that we don't want the world to know about */
extern rx_atomic_t rx_nWaiting;
extern rx_atomic_t rx_nWaited;

/* How many times to retry sendmsg()-equivalent calls for AFS_RXERRQ_ENV. */
#define RXI_SENDMSG_RETRY 8

/* Prototypes for internal functions */

/* rx.c */
extern int rxi_IsRunning(void);
extern void rxi_CancelDelayedAckEvent(struct rx_call *);
extern void rxi_PacketsUnWait(void);
extern void rxi_SetPeerMtu(struct rx_peer *peer, afs_uint32 host,
			   afs_uint32 port, int mtu);
#ifdef AFS_RXERRQ_ENV
extern void rxi_ProcessNetError(struct sock_extended_err *err,
                                afs_uint32 addr, afs_uint16 port);
extern int osi_HandleSocketError(osi_socket sock, void *cmsgbuf,
				 size_t cmsgbuf_len);
extern void rxi_HandleSocketErrors(osi_socket sock);
#else
# define rxi_HandleSocketErrors(sock) do { } while (0)
#endif
extern struct rx_peer *rxi_FindPeer(afs_uint32 host, u_short port,
				    int create);
extern struct rx_packet *rxi_ReceivePacket(struct rx_packet *np,
					   osi_socket socket, afs_uint32 host,
					   u_short port, int *tnop,
					   struct rx_call **newcallp);
extern int rxi_IsConnInteresting(struct rx_connection *aconn);
extern void rxi_PostDelayedAckEvent(struct rx_call *call, struct clock *now);
extern void rxi_ConnectionError(struct rx_connection *conn, afs_int32 error);
extern void rxi_Start(struct rx_call *call, int istack);
extern void rxi_Send(struct rx_call *call, struct rx_packet *p, int istack);
extern struct rx_packet *rxi_SendAck(struct rx_call *call,
				     struct rx_packet *optionalPacket,
				     int serial, int reason, int istack);
extern struct rx_packet *rxi_SendConnectionAbort(struct rx_connection *conn,
						 struct rx_packet *packet,
						 int istack, int force);
extern void rxi_IncrementTimeAndCount(struct rx_peer *peer,
				      afs_uint32 rxInterface,
				      afs_uint32 currentFunc,
				      afs_uint32 totalFunc,
				      struct clock *queueTime,
				      struct clock *execTime,
				      afs_uint64 bytesSent,
				      afs_uint64 bytesRcvd,
				      int isServer);
#ifdef RX_ENABLE_LOCKS
extern void rxi_WaitforTQBusy(struct rx_call *call);
#else
# define rxi_WaitforTQBusy(call)
#endif

/* rx_packet.h */

extern int rxi_SendIovecs(struct rx_connection *conn, struct iovec *iov,
			  int iovcnt, size_t length, int istack);
extern void rxi_SendRaw(struct rx_call *call, struct rx_connection *conn,
			int type, char *data, int bytes, int istack);

/* rx_kcommon.c / rx_user.c */
extern void osi_Msg(const char *fmt, ...) AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);
