/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_RX_PROTOTYPES_H
#define _RX_PROTOTYPES_H

/* rx.c */
#ifndef KERNEL
#ifndef AFS_PTHREAD_ENV
extern int (*registerProgram) (PROCESS, char *);
extern int (*swapNameProgram) (PROCESS, const char *, char *);
#endif
#endif
extern int (*rx_justReceived) (struct rx_packet *, struct sockaddr_in *);
extern int (*rx_almostSent) (struct rx_packet *, struct sockaddr_in *);

extern void rx_rto_setPeerTimeoutSecs(struct rx_peer *, int secs);

extern void rx_SetEpoch(afs_uint32 epoch);
extern int rx_Init(u_int port);
extern int rx_InitHost(u_int host, u_int port);
extern void rx_SetBusyChannelError(afs_int32 onoff);
#ifdef AFS_NT40_ENV
extern void rx_DebugOnOff(int on);
extern void rx_StatsOnOff(int on);
extern void rx_StartClientThread(void);
#endif
#ifndef KERNEL
extern void rxi_StartServerProcs(int nExistingProcs);
#endif
extern void rx_StartServer(int donateMe);
extern struct rx_connection *rx_NewConnection(afs_uint32 shost,
					      u_short sport, u_short sservice,
					      struct rx_securityClass
					      *securityObject,
					      int serviceSecurityIndex);
extern void rx_SetConnDeadTime(struct rx_connection *conn,
			       int seconds);
extern void rx_SetConnHardDeadTime(struct rx_connection *conn, int seconds);
extern void rx_SetConnIdleDeadTime(struct rx_connection *conn, int seconds);
extern void rxi_CleanupConnection(struct rx_connection *conn);
extern void rxi_DestroyConnection(struct rx_connection *conn);
extern void rx_GetConnection(struct rx_connection *conn);
extern void rx_DestroyConnection(struct rx_connection *conn);
extern struct rx_call *rx_NewCall(struct rx_connection *conn);
extern int rxi_HasActiveCalls(struct rx_connection *aconn);
extern int rxi_GetCallNumberVector(struct rx_connection *aconn,
				   afs_int32 * aint32s);
extern int rxi_SetCallNumberVector(struct rx_connection *aconn,
				   afs_int32 * aint32s);
extern struct rx_service *rx_NewService(u_short port, u_short serviceId,
					char *serviceName,
					struct rx_securityClass
					**securityObjects,
					int nSecurityObjects,
					afs_int32(*serviceProc) (struct
								 rx_call *
								 acall));
extern struct rx_service *rx_NewServiceHost(afs_uint32 host, u_short port,
					    u_short serviceId,
					    char *serviceName,
					    struct rx_securityClass
					    **securityObjects,
					    int nSecurityObjects,
					    afs_int32(*serviceProc) (struct
								     rx_call *
								     acall));
extern void rxi_ServerProc(int threadID, struct rx_call *newcall,
			   osi_socket * socketp);
extern void rx_WakeupServerProcs(void);
extern struct rx_call *rx_GetCall(int tno, struct rx_service *cur_service,
				  osi_socket * socketp);
extern void rx_SetArrivalProc(struct rx_call *call,
			      void (*proc) (struct rx_call *
						    call,
						    void * mh,
						    int index),
			      void * handle, int arg);
extern afs_int32 rx_EndCall(struct rx_call *call, afs_int32 rc);
extern void rx_InterruptCall(struct rx_call *call, afs_int32 error);
extern void rx_Finalize(void);
extern void rxi_PacketsUnWait(void);
extern struct rx_service *rxi_FindService(osi_socket socket,
					  u_short serviceId);
extern struct rx_call *rxi_NewCall(struct rx_connection *conn,
				   int channel);

/* Don't like this - change source at some point to make calls identical */
#ifdef RX_ENABLE_LOCKS
extern void rxi_FreeCall(struct rx_call *call, int haveCTLock);
#else /* RX_ENABLE_LOCKS */
extern void rxi_FreeCall(struct rx_call *call);
#endif /* RX_ENABLE_LOCKS */

extern char *rxi_Alloc(size_t size);
extern void rxi_Free(void *addr, size_t size);
extern void rxi_SetPeerMtu(struct rx_peer *peer, afs_uint32 host,
			   afs_uint32 port, int mtu);
extern struct rx_peer *rxi_FindPeer(afs_uint32 host,
				    u_short port,
				    struct rx_peer *origPeer, int create);
extern struct rx_connection *rxi_FindConnection(osi_socket socket,
						afs_uint32 host,
						u_short port,
						u_short serviceId,
						afs_uint32 cid,
						afs_uint32 epoch, int type,
						u_int securityIndex,
						int *unknownService);
extern struct rx_packet *rxi_ReceivePacket(struct rx_packet *np,
					   osi_socket socket, afs_uint32 host,
					   u_short port, int *tnop,
					   struct rx_call **newcallp);
extern int rxi_IsConnInteresting(struct rx_connection *aconn);
extern struct rx_packet *rxi_ReceiveDataPacket(struct rx_call *call,
					       struct rx_packet *np,
					       int istack, osi_socket socket,
					       afs_uint32 host, u_short port,
					       int *tnop,
					       struct rx_call **newcallp);
extern struct rx_packet *rxi_ReceiveAckPacket(struct rx_call *call,
					      struct rx_packet *np,
					      int istack);
extern struct rx_packet *rxi_ReceiveResponsePacket(struct
						   rx_connection *conn, struct rx_packet
						   *np, int istack);
extern struct rx_packet *rxi_ReceiveChallengePacket(struct
						    rx_connection *conn, struct rx_packet
						    *np, int istack);
extern void rxi_AttachServerProc(struct rx_call *call,
				 osi_socket socket,
				 int *tnop,
				 struct rx_call **newcallp);
extern void rxi_AckAll(struct rxevent *event, struct rx_call *call,
		       char *dummy);
extern void rxi_SendDelayedAck(struct rxevent *event,
			       void *call /* struct rx_call *call */, void *dummy);
extern void rxi_ClearTransmitQueue(struct rx_call *call,
				   int force);
extern void rxi_ClearReceiveQueue(struct rx_call *call);
extern struct rx_packet *rxi_SendCallAbort(struct rx_call *call,
					   struct rx_packet *packet,
					   int istack, int force);
extern struct rx_packet *rxi_SendConnectionAbort(struct rx_connection
						 *conn,
						 struct rx_packet *packet,
						 int istack, int force);
extern void rxi_ConnectionError(struct rx_connection *conn,
				afs_int32 error);
extern void rxi_CallError(struct rx_call *call, afs_int32 error);
extern void rxi_ResetCall(struct rx_call *call,
			  int newcall);
extern struct rx_packet *rxi_SendAck(struct rx_call *call, struct rx_packet
				     *optionalPacket, int serial, int reason,
				     int istack);
extern void rxi_Start(struct rx_call *call, int istack);
extern void rxi_Send(struct rx_call *call,
		     struct rx_packet *p, int istack);
#ifdef RX_ENABLE_LOCKS
extern int rxi_CheckCall(struct rx_call *call, int haveCTLock);
#else /* RX_ENABLE_LOCKS */
extern int rxi_CheckCall(struct rx_call *call);
#endif /* RX_ENABLE_LOCKS */
extern void rxi_KeepAliveEvent(struct rxevent *event,
			       void *call /* struct rx_call *call */,
			       void *dummy);
extern void rxi_GrowMTUEvent(struct rxevent *event,
			     void *call /* struct rx_call *call */,
			     void *dummy);
extern void rxi_ScheduleKeepAliveEvent(struct rx_call *call);
extern void rxi_ScheduleNatKeepAliveEvent(struct rx_connection *conn);
extern void rxi_ScheduleGrowMTUEvent(struct rx_call *call, int secs);
extern void rxi_KeepAliveOn(struct rx_call *call);
extern void rxi_NatKeepAliveOn(struct rx_connection *conn);
extern void rxi_GrowMTUOn(struct rx_call *call);
extern void rx_SetConnSecondsUntilNatPing(struct rx_connection *conn, afs_int32 seconds);
extern void rxi_SendDelayedConnAbort(struct rxevent *event,
				     void *conn, /* struct rx_connection *conn */
				     void *dummy);
extern void rxi_SendDelayedCallAbort(struct rxevent *event,
				     void *call, /* struct rx_call *call */
				     void *dummy);
extern void rxi_ChallengeEvent(struct rxevent *event,
			       void *conn, /* struct rx_connection *conn */
			       void *arg1, int atries);
extern void rxi_ChallengeOn(struct rx_connection *conn);
extern void rxi_ReapConnections(struct rxevent *unused, void *unused1,
				void *unused2);
extern void rx_KeepAliveOn(struct rx_call *call);
extern void rx_KeepAliveOff(struct rx_call *call);
extern int rxs_Release(struct rx_securityClass *aobj);
#ifndef KERNEL
extern void rx_PrintTheseStats(FILE * file, struct rx_statistics *s, int size,
			       afs_int32 freePackets, char version);
extern void rx_PrintStats(FILE * file);
extern void rx_PrintPeerStats(FILE * file, struct rx_peer *peer);
#endif
extern afs_int32 rx_GetServerDebug(osi_socket socket, afs_uint32 remoteAddr,
				   afs_uint16 remotePort,
				   struct rx_debugStats *stat,
				   afs_uint32 * supportedValues);
extern afs_int32 rx_GetServerStats(osi_socket socket, afs_uint32 remoteAddr,
				   afs_uint16 remotePort,
				   struct rx_statistics *stat,
				   afs_uint32 * supportedValues);
extern afs_int32 rx_GetServerVersion(osi_socket socket, afs_uint32 remoteAddr,
				     afs_uint16 remotePort,
				     size_t version_length, char *version);
extern afs_int32 rx_GetServerConnections(osi_socket socket,
					 afs_uint32 remoteAddr,
					 afs_uint16 remotePort,
					 afs_int32 * nextConnection,
					 int allConnections,
					 afs_uint32 debugSupportedValues,
					 struct rx_debugConn *conn,
					 afs_uint32 * supportedValues);
extern afs_int32 rx_GetServerPeers(osi_socket socket, afs_uint32 remoteAddr,
				   afs_uint16 remotePort,
				   afs_int32 * nextPeer,
				   afs_uint32 debugSupportedValues,
				   struct rx_debugPeer *peer,
				   afs_uint32 * supportedValues);
extern afs_int32 rx_GetLocalPeers(afs_uint32 peerHost, afs_uint16 peerPort,
				      struct rx_debugPeer * peerStats);
extern void shutdown_rx(void);
#ifndef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t * lockaddr, char *msg);
#endif
#ifndef KERNEL
extern int rx_KeyCreate(rx_destructor_t rtn);
#endif
extern void rx_SetSpecific(struct rx_connection *conn, int key, void *ptr);
extern void *rx_GetSpecific(struct rx_connection *conn, int key);
extern void rx_SetServiceSpecific(struct rx_service *svc, int key, void *ptr);
extern void * rx_GetServiceSpecific(struct rx_service *svc, int key);
extern void rx_IncrementTimeAndCount(struct rx_peer *peer,
				     afs_uint32 rxInterface,
				     afs_uint32 currentFunc,
				     afs_uint32 totalFunc,
				     struct clock *queueTime,
				     struct clock *execTime,
				     afs_hyper_t * bytesSent,
				     afs_hyper_t * bytesRcvd, int isServer);
extern void rx_MarshallProcessRPCStats(afs_uint32 callerVersion, int count,
				       rx_function_entry_v1_t * stats,
				       afs_uint32 ** ptrP);
extern int rx_RetrieveProcessRPCStats(afs_uint32 callerVersion,
				      afs_uint32 * myVersion,
				      afs_uint32 * clock_sec,
				      afs_uint32 * clock_usec,
				      size_t * allocSize,
				      afs_uint32 * statCount,
				      afs_uint32 ** stats);
extern int rx_RetrievePeerRPCStats(afs_uint32 callerVersion,
				   afs_uint32 * myVersion,
				   afs_uint32 * clock_sec,
				   afs_uint32 * clock_usec,
				   size_t * allocSize, afs_uint32 * statCount,
				   afs_uint32 ** stats);
extern void rx_FreeRPCStats(afs_uint32 * stats, size_t allocSize);
extern int rx_queryProcessRPCStats(void);
extern int rx_queryPeerRPCStats(void);
extern void rx_enableProcessRPCStats(void);
extern void rx_enablePeerRPCStats(void);
extern void rx_disableProcessRPCStats(void);
extern void rx_disablePeerRPCStats(void);
extern void rx_clearProcessRPCStats(afs_uint32 clearFlag);
extern void rx_clearPeerRPCStats(afs_uint32 clearFlag);
extern void rx_SetRxStatUserOk(int (*proc) (struct rx_call * call));
extern int rx_RxStatUserOk(struct rx_call *call);
extern afs_int32 rx_SetSecurityConfiguration(struct rx_service *service,
					     rx_securityConfigVariables type,
					     void *value);

void rxi_DebugInit(void);
void rxi_DebugPrint(char *format, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);

/* rx_clock.c */
#if !defined(clock_Init)
extern void clock_Init(void);
#endif
#if !defined(clock_UnInit)
extern int clock_UnInit(void);
#endif
#if !defined(clock_UpdateTime)
extern void clock_UpdateTime(void);
#endif
#if defined(UKERNEL) && !defined(osi_GetTime)
extern int osi_GetTime(struct timeval *tv);
#endif

/* rx_clock_nt.c */


/* rx_conncache.c */
extern void rxi_DeleteCachedConnections(void);
extern struct rx_connection *rx_GetCachedConnection(unsigned int remoteAddr,
						    unsigned short port,
						    unsigned short service,
						    struct rx_securityClass
						    *securityObject,
						    int securityIndex);
extern void rx_ReleaseCachedConnection(struct rx_connection *conn);


/* rxdebug.c */


/* rx_event.c */
extern int rxevent_nFree;
extern int rxevent_nPosted;
#if 0
extern struct rxevent *rxevent_Post(struct clock *when,
				    void (*func) (struct rxevent * event,
						  struct rx_connection * conn,
						  struct rx_call * acall),
				    void *arg, void *arg1);
/* this func seems to be called with tons of different style routines, need to look
at another time. */
#else
extern struct rxevent *rxevent_Post(struct clock *when,
				    void (*func) (struct rxevent *, void *, void *),
				    void *arg, void *arg1);
extern struct rxevent *rxevent_Post2(struct clock *when,
				    void (*func) (struct rxevent *, void *, void *, int),
				    void *arg, void *arg1, int arg2);
extern struct rxevent *rxevent_PostNow(struct clock *when, struct clock *now,
				       void (*func) (struct rxevent *, void *, void *), void *arg, void *arg1);
extern struct rxevent *rxevent_PostNow2(struct clock *when, struct clock *now,
					void (*func) (struct rxevent *, void *, void *, int), void *arg,
					void *arg1, int arg2);
#endif
extern void shutdown_rxevent(void);
extern struct rxepoch *rxepoch_Allocate(struct clock *when);
extern void rxevent_Init(int nEvents, void (*scheduler) (void));
extern void rxevent_Cancel_1(struct rxevent *ev,
			     struct rx_call *call,
			     int type);
extern int rxevent_RaiseEvents(struct clock *next);




/* rx_getaddr.c */
extern void rxi_setaddr(afs_uint32 x);
extern afs_uint32 rxi_getaddr(void);
extern int rx_getAllAddr(afs_uint32 * buffer, int maxSize);
extern int rx_getAllAddrMaskMtu(afs_uint32 addrBuffer[],
			  	 afs_uint32 maskBuffer[],
				 afs_uint32 mtuBuffer[],
				 int maxSize);

/* rx_globals.c */
extern int rx_GetMaxReceiveWindow(void);
extern int rx_GetMaxSendWindow(void);
extern void rx_SetMaxReceiveWindow(int packets);
extern void rx_SetMaxSendWindow(int packets);
extern int rx_GetMinPeerTimeout(void);
extern void rx_SetMinPeerTimeout(int msecs);

#ifdef KERNEL
/* rx_kcommon.c */
struct socket;
extern int (*rxk_PacketArrivalProc) (struct rx_packet * ahandle,
				     struct sockaddr_in * afrom,
				     struct socket *arock,
				     afs_int32 asize);
extern int (*rxk_GetPacketProc) (struct rx_packet **ahandle, int asize);
extern afs_int32 afs_termState;
extern int rxk_initDone;

extern int rxk_DelPort(u_short aport);
extern void rxk_shutdownPorts(void);
extern osi_socket rxi_GetUDPSocket(u_short port);
extern osi_socket rxi_GetHostUDPSocket(u_int host, u_short port);
# if defined(AFS_LINUX26_ENV)
# define osi_Panic(msg...) do { printk(KERN_CRIT "openafs: " msg); BUG(); } while (0)
# undef osi_Assert
# define osi_Assert(expr) \
    do { if (!(expr)) osi_Panic("assertion failed: %s, file: %s, line: %d\n", #expr, __FILE__, __LINE__); } while (0)
# elif defined(AFS_AIX_ENV)
extern void osi_Panic(char *fmt, void *a1, void *a2, void *a3);
# else
extern void osi_Panic(char *fmt, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2)
    AFS_NORETURN;
#endif
extern int osi_utoa(char *buf, size_t len, unsigned long val);
extern void rxi_InitPeerParams(struct rx_peer *pp);
extern void shutdown_rxkernel(void);
# ifdef AFS_USERSPACE_IP_ADDR
extern int rxi_GetcbiInfo(void);
extern afs_int32 rxi_Findcbi(afs_uint32 addr);
# else
extern int rxi_GetIFInfo(void);
# endif
# ifndef UKERNEL
extern int rxk_FreeSocket(struct socket *asocket);
extern osi_socket *rxk_NewSocket(short aport);
# endif
extern int rxk_ReadPacket(osi_socket so, struct rx_packet *p, int *host,
			  int *port);
# ifdef UKERNEL
extern void *rx_ServerProc(void *);
# endif
# ifndef AFS_LINUX26_ENV
extern void osi_AssertFailK(const char *expr, const char *file, int line) AFS_NORETURN;
# endif
extern void rxk_ListenerProc(void);
extern void rxk_Listener(void);
# ifndef UKERNEL
extern void afs_rxevent_daemon(void);
# endif
extern rx_ifnet_t rxi_FindIfnet(afs_uint32 addr, afs_uint32 * maskp);
extern void osi_StopListener(void);

/* ARCH/rx_kmutex.c */
# if defined(AFS_LINUX20_ENV)
extern void afs_mutex_init(afs_kmutex_t * l);
extern void afs_mutex_enter(afs_kmutex_t * l);
extern int afs_mutex_tryenter(afs_kmutex_t * l);
extern void afs_mutex_exit(afs_kmutex_t * l);
extern int afs_cv_wait(afs_kcondvar_t * cv, afs_kmutex_t * l, int sigok);
extern void afs_cv_timedwait(afs_kcondvar_t * cv, afs_kmutex_t * l,
			     int waittime);
# endif



/* ARCH/rx_knet.c */
# if !defined(AFS_SGI_ENV)
extern int osi_NetSend(osi_socket asocket, struct sockaddr_in *addr,
		       struct iovec *dvec, int nvecs, afs_int32 asize,
		       int istack);
# endif
# ifdef RXK_UPCALL_ENV
extern void rx_upcall(socket_t so, void *arg, __unused int waitflag);
# else
extern int osi_NetReceive(osi_socket so, struct sockaddr_in *addr,
			  struct iovec *dvec, int nvecs, int *lengthp);
# endif
# if defined(AFS_SUN510_ENV)
extern void osi_StartNetIfPoller(void);
extern void osi_NetIfPoller(void);
extern void osi_StopNetIfPoller(void);
extern struct afs_ifinfo afsifinfo[ADDRSPERSITE];
# endif
extern void osi_StopListener(void);
extern int rxi_FindIfMTU(afs_uint32 addr);
# if defined(UKERNEL)
extern void rxi_ListenerProc(osi_socket usockp, int *tnop,
			     struct rx_call **newcallp);
# endif

# if !defined(RXK_LISTENER_ENV) && !defined(RXK_UPCALL_ENV)
extern void rxk_init(void);
# endif

/* UKERNEL/rx_knet.c */
# ifdef UKERNEL
extern void afs_rxevent_daemon(void);
# endif
#endif

/* rx_lwp.c */
extern void rxi_Sleep(void *addr);
extern void rxi_Delay(int seconds);
extern void rxi_InitializeThreadSupport(void);
extern void rxi_Wakeup(void *addr);
extern void rxi_StopListener(void);
#ifndef KERNEL
extern void rxi_ReScheduleEvents(void);
#endif
extern void rxi_InitializeThreadSupport(void);
extern void rxi_StartServerProc(void *(*proc) (void *), int stacksize);
extern void rxi_StartListener(void);
extern void *rx_ServerProc(void *);
extern int rxi_Listen(osi_socket sock);
extern int rxi_Recvmsg(osi_socket socket, struct msghdr *msg_p, int flags);
extern int rxi_Sendmsg(osi_socket socket, struct msghdr *msg_p, int flags);


/* rx_misc.c */
#ifndef osi_alloc
extern char *osi_alloc(afs_int32 x);
#endif
#ifndef osi_free
extern int osi_free(char *x, afs_int32 size);
#endif
extern int hton_syserr_conv(afs_int32 code);
extern int ntoh_syserr_conv(int code);


/* rx_multi.c */
extern struct multi_handle *multi_Init(struct rx_connection **conns,
				       int nConns);
extern int multi_Select(struct multi_handle *mh);
extern void multi_Ready(struct rx_call *call,
			void *mh, int index);
extern void multi_Finalize(struct multi_handle *mh);
extern void multi_Finalize_Ignore(struct multi_handle *mh);



/* rx_null.c */
extern struct rx_securityClass *rxnull_NewServerSecurityObject(void);
extern struct rx_securityClass *rxnull_NewClientSecurityObject(void);

/* rx_packet.c */
extern afs_int32 rx_SlowGetInt32(struct rx_packet *packet, size_t offset);
extern afs_int32 rx_SlowPutInt32(struct rx_packet *packet, size_t offset,
				 afs_int32 data);
extern afs_int32 rx_SlowReadPacket(struct rx_packet *packet,
				   unsigned int offset, int resid, char *out);
extern afs_int32 rx_SlowWritePacket(struct rx_packet *packet, int offset,
				    int resid, char *in);
extern int rxi_RoundUpPacket(struct rx_packet *p, unsigned int nb);
extern int rxi_AllocDataBuf(struct rx_packet *p, int nb, int cla_ss);
extern void rxi_MorePackets(int apackets);
extern void rxi_MorePacketsNoLock(int apackets);
#if defined(AFS_PTHREAD_ENV)
extern void rxi_MorePacketsTSFPQ(int apackets, int flush_global, int num_keep_local); /* more flexible packet alloc function */
extern void rxi_AdjustLocalPacketsTSFPQ(int num_keep_local, int allow_overcommit); /* adjust thread-local queue length, for places where we know how many packets we will need a priori */
extern void rxi_FlushLocalPacketsTSFPQ(void); /* flush all thread-local packets to global queue */
#endif
extern void rxi_FreeAllPackets(void);
extern void rx_CheckPackets(void);
extern void rxi_FreePacketNoLock(struct rx_packet *p);
extern int rxi_FreeDataBufsNoLock(struct rx_packet *p, afs_uint32 first);
extern void rxi_RestoreDataBufs(struct rx_packet *p);
extern int rxi_TrimDataBufs(struct rx_packet *p, int first);
extern void rxi_FreePacket(struct rx_packet *p);
extern struct rx_packet *rxi_AllocPacketNoLock(int cla_ss);
extern struct rx_packet *rxi_AllocPacket(int cla_ss);
extern int rxi_AllocPackets(int cla_ss, int num_pkts, struct rx_queue *q);
extern int rxi_FreePackets(int num_pkts, struct rx_queue *q);
extern struct rx_packet *rxi_AllocSendPacket(struct rx_call *call,
					     int want);
extern int rxi_ReadPacket(osi_socket socket, struct rx_packet *p,
			  afs_uint32 * host, u_short * port);
extern struct rx_packet *rxi_SplitJumboPacket(struct rx_packet *p,
					      afs_uint32 host, short port,
					      int first);
#ifndef KERNEL
extern int osi_NetSend(osi_socket socket, void *addr, struct iovec *dvec,
		       int nvecs, int length, int istack);
#endif
extern struct rx_packet *rxi_ReceiveDebugPacket(struct rx_packet *ap,
						osi_socket asocket,
						afs_uint32 ahost, short aport,
						int istack);
extern struct rx_packet *rxi_ReceiveVersionPacket(struct rx_packet
						  *ap, osi_socket asocket,
						  afs_uint32 ahost,
						  short aport, int istack);
extern void rxi_SendPacket(struct rx_call *call, struct rx_connection *conn,
			   struct rx_packet *p, int istack);
extern void rxi_SendPacketList(struct rx_call *call,
			       struct rx_connection *conn,
			       struct rx_packet **list, int len, int istack);
extern void rxi_SendRawAbort(osi_socket socket, afs_uint32 host, u_short port,
			     afs_int32 error, struct rx_packet *source,
			     int istack);
extern struct rx_packet *rxi_SendSpecial(struct rx_call *call,
					 struct rx_connection *conn,
					 struct rx_packet *optionalPacket,
					 int type, char *data, int nbytes,
					 int istack);
extern void rxi_EncodePacketHeader(struct rx_packet *p);
extern void rxi_DecodePacketHeader(struct rx_packet *p);
extern void rxi_PrepareSendPacket(struct rx_call *call,
				  struct rx_packet *p,
				  int last);
extern int rxi_AdjustIfMTU(int mtu);
extern int rxi_AdjustMaxMTU(int mtu, int peerMaxMTU);
extern int rxi_AdjustDgramPackets(int frags, int mtu);
#ifdef  AFS_GLOBAL_RXLOCK_KERNEL
extern void rxi_WaitforTQBusy(struct rx_call *call);
#endif

/* rxperf.c */


/* rx_pthread.c */
extern void rxi_Delay(int sec);
extern void rxi_InitializeThreadSupport(void);
extern void rxi_StartServerProc(void *(*proc) (void *), int stacksize);
#ifndef rxi_ReScheduleEvents
extern void rxi_ReScheduleEvents(void);
#endif
extern void *rx_ServerProc(void *);
extern void rxi_StartListener(void);
extern int rxi_Listen(osi_socket sock);
extern int rxi_Recvmsg(osi_socket socket, struct msghdr *msg_p, int flags);
extern int rxi_Sendmsg(osi_socket socket, struct msghdr *msg_p, int flags);

/* rx_rdwr.c */
extern int rxi_ReadProc(struct rx_call *call, char *buf,
			int nbytes);
extern int rx_ReadProc(struct rx_call *call, char *buf, int nbytes);
extern int rx_ReadProc32(struct rx_call *call, afs_int32 * value);
extern int rxi_FillReadVec(struct rx_call *call, afs_uint32 serial);
extern int rxi_ReadvProc(struct rx_call *call, struct iovec *iov, int *nio,
			 int maxio, int nbytes);
extern int rx_ReadvProc(struct rx_call *call, struct iovec *iov, int *nio,
			int maxio, int nbytes);
extern int rxi_WriteProc(struct rx_call *call, char *buf,
			 int nbytes);
extern int rx_WriteProc(struct rx_call *call, char *buf, int nbytes);
extern int rx_WriteProc32(struct rx_call *call,
			  afs_int32 * value);
extern int rxi_WritevAlloc(struct rx_call *call, struct iovec *iov, int *nio,
			   int maxio, int nbytes);
extern int rx_WritevAlloc(struct rx_call *call, struct iovec *iov, int *nio,
			  int maxio, int nbytes);
extern int rxi_WritevProc(struct rx_call *call, struct iovec *iov, int nio,
			  int nbytes);
extern int rx_WritevProc(struct rx_call *call, struct iovec *iov, int nio,
			 int nbytes);
extern void rxi_FlushWrite(struct rx_call *call);
extern void rx_FlushWrite(struct rx_call *call);

/* rx_trace.c */


/* rx_user.c */
#ifdef AFS_PTHREAD_ENV
extern afs_kmutex_t rx_if_init_mutex;
extern afs_kmutex_t rx_if_mutex;
#endif
extern osi_socket rxi_GetUDPSocket(u_short port);
extern void osi_AssertFailU(const char *expr, const char *file, int line) AFS_NORETURN;
extern void rxi_InitPeerParams(struct rx_peer *pp);
extern int rxi_HandleSocketError(int socket);

#if defined(AFS_AIX32_ENV) && !defined(KERNEL)
#ifndef osi_Alloc
extern void *osi_Alloc(afs_int32 x);
#endif
#ifndef osi_Free
extern void osi_Free(void *x, afs_int32 size);
#endif
#endif /* defined(AFS_AIX32_ENV) && !defined(KERNEL) */
#ifndef KERNEL
extern void osi_Panic(char *fmt, ...) AFS_NORETURN;
#endif

extern void rx_GetIFInfo(void);
extern void rx_SetNoJumbo(void);
extern void rx_SetMaxMTU(int mtu);

/* rx_xmit_nt.c */


/* MISC PROTOTYPES - MOVE TO APPROPRIATE LOCATION LATER */

/* EXTERNAL PROTOTYPES - include here cause it causes too many issues to
   include the afs_prototypes.h file - just make sure they match */
#ifndef afs_osi_Alloc
extern void *afs_osi_Alloc(size_t x);
#endif
#ifndef afs_osi_Alloc_NoSleep
extern void *afs_osi_Alloc_NoSleep(size_t x);
#endif
#ifndef afs_osi_Free
extern void afs_osi_Free(void *x, size_t asize);
#endif
#ifndef afs_osi_Wakeup
extern int afs_osi_Wakeup(void *event);
#endif
#ifndef afs_osi_Sleep
extern void afs_osi_Sleep(void *event);
#endif
extern unsigned int afs_random(void);
extern void osi_linux_rxkreg(void);

#endif /* _RX_PROTOTYPES_H */
