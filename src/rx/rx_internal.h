/* Internal structures that are private to RX itself. These shouldn't be
 * modified by library callers.
 *
 * Data structures that are visible to security layers, but not to
 * customers of RX belong in rx_private.h, which is installed.
 */


/* Globals that we don't want the world to know about */
extern rx_atomic_t rx_nWaiting;
extern rx_atomic_t rx_nWaited;

/* Prototypes for internal functions */

/* rx.c */
extern void rxi_PacketsUnWait(void);
extern void rxi_SetPeerMtu(struct rx_peer *peer, afs_uint32 host,
			   afs_uint32 port, int mtu);
extern struct rx_peer *rxi_FindPeer(afs_uint32 host, u_short port,
				    struct rx_peer *origPeer, int create);
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

/* rx_packet.h */

extern int rxi_SendIovecs(struct rx_connection *conn, struct iovec *iov,
			  int iovcnt, size_t length, int istack);
extern void rxi_SendRaw(struct rx_call *call, struct rx_connection *conn,
			int type, char *data, int bytes, int istack);
