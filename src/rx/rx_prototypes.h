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
extern int rx_Init(u_int port);
#ifndef KERNEL
extern void rxi_StartServerProcs(int nExistingProcs);
#endif
extern void rx_StartServer(int donateMe);
extern struct rx_connection *rx_NewConnection(register afs_uint32 shost, 
        u_short sport, u_short sservice, 
        register struct rx_securityClass *securityObject, int serviceSecurityIndex);
extern void rx_SetConnDeadTime(register struct rx_connection *conn, register int seconds);
extern void rxi_CleanupConnection(struct rx_connection *conn);
extern void rxi_DestroyConnection(register struct rx_connection *conn);
extern void rx_DestroyConnection(register struct rx_connection *conn);
extern struct rx_call *rx_NewCall(register struct rx_connection *conn);
extern int rxi_HasActiveCalls(register struct rx_connection *aconn);
extern int rxi_GetCallNumberVector(register struct rx_connection *aconn, 
        register afs_int32 *aint32s);
extern int rxi_SetCallNumberVector(register struct rx_connection *aconn, 
        register afs_int32 *aint32s);
extern struct rx_service *rx_NewService(u_short port, u_short serviceId, 
        char *serviceName,
        struct rx_securityClass **securityObjects,
        int nSecurityObjects, afs_int32 (*serviceProc)());
extern void rxi_ServerProc(int threadID, struct rx_call *newcall, osi_socket *socketp);
extern void rx_WakeupServerProcs(void);
extern struct rx_call *rx_GetCall(int tno, struct rx_service *cur_service, osi_socket *socketp);
extern void rx_SetArrivalProc(register struct rx_call *call, 
        register VOID (*proc)(), register VOID *handle, register VOID *arg);
extern afs_int32 rx_EndCall(register struct rx_call *call, afs_int32 rc);
extern void rx_Finalize(void);
extern void rxi_PacketsUnWait(void);
extern struct rx_service *rxi_FindService(register osi_socket socket, 
        register u_short serviceId);
extern struct rx_call *rxi_NewCall(register struct rx_connection *conn,
        register int channel);

/* Don't like this - change source at some point to make calls identical */
#ifdef RX_ENABLE_LOCKS
extern void rxi_FreeCall(register struct rx_call *call, int haveCTLock);
#else /* RX_ENABLE_LOCKS */
extern void rxi_FreeCall(register struct rx_call *call);
#endif /* RX_ENABLE_LOCKS */

extern char *rxi_Alloc(register size_t size);
extern void rxi_Free(void *addr, register size_t size);
extern struct rx_peer *rxi_FindPeer(register afs_uint32 host, 
        register u_short port, struct rx_peer *origPeer, int create);
extern struct rx_connection *rxi_FindConnection(osi_socket socket, 
        register afs_int32 host, register u_short port, u_short serviceId, 
        afs_uint32 cid, afs_uint32 epoch, int type, u_int securityIndex);
extern struct rx_packet *rxi_ReceivePacket(register struct rx_packet *np, 
        osi_socket socket, afs_uint32 host, u_short port, 
        int *tnop, struct rx_call **newcallp);
extern int rxi_IsConnInteresting(struct rx_connection *aconn);
extern struct rx_packet *rxi_ReceiveDataPacket(register struct rx_call *call, 
        register struct rx_packet *np, int istack, osi_socket socket, 
        afs_uint32 host, u_short port, int *tnop, struct rx_call **newcallp);
extern struct rx_packet *rxi_ReceiveAckPacket(register struct rx_call *call, 
        struct rx_packet *np, int istack);
extern struct rx_packet *rxi_ReceiveResponsePacket(register struct rx_connection *conn, 
        register struct rx_packet *np, int istack);
extern struct rx_packet *rxi_ReceiveChallengePacket(register struct rx_connection *conn, 
        register struct rx_packet *np, int istack);
extern void rxi_AttachServerProc(register struct rx_call *call, 
        register osi_socket socket, register int *tnop, register struct rx_call **newcallp);
extern void rxi_AckAll(struct rxevent *event, register struct rx_call *call, char *dummy);
extern void rxi_SendDelayedAck(struct rxevent *event, register struct rx_call *call, char *dummy);
extern void rxi_ClearTransmitQueue(register struct rx_call *call, register int force);
extern void rxi_ClearReceiveQueue(register struct rx_call *call);
extern struct rx_packet *rxi_SendCallAbort(register struct rx_call *call, 
        struct rx_packet *packet, int istack, int force);
extern struct rx_packet *rxi_SendConnectionAbort(register struct rx_connection *conn,
        struct rx_packet *packet, int istack, int force);
extern void rxi_ConnectionError(register struct rx_connection *conn, 
        register afs_int32 error);
extern void rxi_CallError(register struct rx_call *call, afs_int32 error);
extern void rxi_ResetCall(register struct rx_call *call, register int newcall);
extern struct rx_packet *rxi_SendAck(register struct rx_call *call, 
        register struct rx_packet *optionalPacket, int seq, int serial, 
        int pflags, int reason, int istack);
extern void rxi_StartUnlocked(struct rxevent *event, register struct rx_call *call, 
        int istack);
extern void rxi_Start(struct rxevent *event, register struct rx_call *call,
        int istack);
extern void rxi_Send(register struct rx_call *call, register struct rx_packet *p, 
        int istack);
#ifdef RX_ENABLE_LOCKS
extern int rxi_CheckCall(register struct rx_call *call, int haveCTLock);
#else /* RX_ENABLE_LOCKS */
extern int rxi_CheckCall(register struct rx_call *call);
#endif /* RX_ENABLE_LOCKS */
extern void rxi_KeepAliveEvent(struct rxevent *event, register struct rx_call *call, char *dummy);
extern void rxi_ScheduleKeepAliveEvent(register struct rx_call *call);
extern void rxi_KeepAliveOn(register struct rx_call *call);
extern void rxi_SendDelayedConnAbort(struct rxevent *event, register struct rx_connection *conn, 
        char *dummy);
extern void rxi_SendDelayedCallAbort(struct rxevent *event, register struct rx_call *call,
        char *dummy);
extern void rxi_ChallengeEvent(struct rxevent *event, register struct rx_connection *conn, 
        void *atries);
extern void rxi_ChallengeOn(register struct rx_connection *conn);
extern void rxi_ComputeRoundTripTime(register struct rx_packet *p, 
        register struct clock *sentp, register struct rx_peer *peer);
extern void rxi_ReapConnections(void);
extern int rxs_Release (struct rx_securityClass *aobj);
#ifndef KERNEL
extern void rx_PrintTheseStats (FILE *file, struct rx_stats *s, int size, 
        afs_int32 freePackets, char version);
extern void rx_PrintStats(FILE *file);
extern void rx_PrintPeerStats(FILE *file, struct rx_peer *peer);
#endif
extern afs_int32 rx_GetServerDebug(osi_socket socket, afs_uint32 remoteAddr, 
        afs_uint16 remotePort, struct rx_debugStats *stat, afs_uint32 *supportedValues);
extern afs_int32 rx_GetServerStats(osi_socket socket, afs_uint32 remoteAddr,
        afs_uint16 remotePort, struct rx_stats *stat, afs_uint32 *supportedValues);
extern afs_int32 rx_GetServerVersion(osi_socket socket, afs_uint32 remoteAddr,
        afs_uint16 remotePort, size_t version_length, char *version);
extern afs_int32 rx_GetServerConnections(osi_socket socket, afs_uint32 remoteAddr,
        afs_uint16 remotePort, afs_int32 *nextConnection, int allConnections,
        afs_uint32 debugSupportedValues, struct rx_debugConn *conn, afs_uint32 *supportedValues);
extern afs_int32 rx_GetServerPeers(osi_socket socket, afs_uint32 remoteAddr, afs_uint16 remotePort,
        afs_int32 *nextPeer, afs_uint32 debugSupportedValues, struct rx_debugPeer *peer,
        afs_uint32 *supportedValues);
extern void shutdown_rx(void);
#ifndef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t *lockaddr, char *msg);
#endif
#ifndef KERNEL
extern int rx_KeyCreate(rx_destructor_t rtn);
#endif
extern void rx_SetSpecific(struct rx_connection *conn, int key, void *ptr);
extern void *rx_GetSpecific(struct rx_connection *conn, int key);
extern void rx_IncrementTimeAndCount(struct rx_peer *peer, afs_uint32 rxInterface,
        afs_uint32 currentFunc, afs_uint32 totalFunc, struct clock *queueTime,
        struct clock *execTime, afs_hyper_t *bytesSent, afs_hyper_t *bytesRcvd, int isServer);
extern void rx_MarshallProcessRPCStats(afs_uint32 callerVersion,
        int count, rx_function_entry_v1_t *stats, afs_uint32 **ptrP);
extern int rx_RetrieveProcessRPCStats(afs_uint32 callerVersion,
        afs_uint32 *myVersion, afs_uint32 *clock_sec, afs_uint32 *clock_usec,
        size_t *allocSize, afs_uint32 *statCount, afs_uint32 **stats);
extern int rx_RetrievePeerRPCStats(afs_uint32 callerVersion,
        afs_uint32 *myVersion, afs_uint32 *clock_sec, afs_uint32 *clock_usec,
        size_t *allocSize, afs_uint32 *statCount, afs_uint32 **stats);
extern void rx_FreeRPCStats(afs_uint32 *stats, size_t allocSize);
extern int rx_queryProcessRPCStats(void);
extern int rx_queryPeerRPCStats(void);
extern void rx_enableProcessRPCStats(void);
extern void rx_enablePeerRPCStats(void);
extern void rx_disableProcessRPCStats(void);
extern void rx_disablePeerRPCStats(void);
extern void rx_clearProcessRPCStats(afs_uint32 clearFlag);
extern void rx_clearPeerRPCStats(afs_uint32 clearFlag);
extern void rx_SetRxStatUserOk(int (*proc)(struct rx_call *call));
extern int rx_RxStatUserOk(struct rx_call *call);

/* old style till varargs */
void rxi_DebugPrint();

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

/* rx_clock_nt.c */


/* rx_conncache.c */
extern void rxi_DeleteCachedConnections(void);
extern struct rx_connection *rx_GetCachedConnection(unsigned int remoteAddr,
        unsigned short port, unsigned short service, struct rx_securityClass *securityObject,
        int securityIndex);
extern void rx_ReleaseCachedConnection(struct rx_connection *conn);


/* rxdebug.c */


/* rx_event.c */
extern struct rxevent *rxevent_Post(struct clock *when, void (*func)(),
                             void *arg, void *arg1);
extern void shutdown_rxevent(void);
extern struct rxepoch *rxepoch_Allocate(struct clock *when);


/* rx_getaddr.c */


/* rx_globals.c */


/* rx_kcommon.c */
#ifdef UKERNEL
extern void rx_ServerProc(void);
#endif
extern void osi_AssertFailK(const char *expr, const char *file, int line);


/* rx_knet.c */
extern struct osi_socket *rxk_NewSocket(short aport);
extern int rxk_FreeSocket(register osi_socket asocket);
#if 0
/* definitions are all different at the moment */
#ifdef KERNEL
extern int osi_NetSend(struct socket *sop, struct sockaddr_in *to,
                struct iovec *iov, int iovcnt, int size, int istack);
#endif
#endif
#if 0
extern int osi_NetReceive(osi_socket so, struct sockaddr_in *from,
                   struct iovec *iov, int iovcnt, int *lengthp);
#endif
extern void osi_StopListener(void);



/* rx_lwp.c */
extern void rx_ServerProc(void);
extern void rxi_Sleep(void *addr);
extern void rxi_Delay(int seconds);
extern void rxi_InitializeThreadSupport(void);
extern void rxi_Wakeup(void *addr);
extern void rxi_StopListener(void);
#ifndef KERNEL
extern void rxi_ReScheduleEvents(void);
#endif
extern void rxi_InitializeThreadSupport(void);
extern void rxi_StartServerProc(void (*proc)(void), int stacksize);
extern void rxi_StartListener(void);
extern void rx_ServerProc(void);
extern int rxi_Listen(osi_socket sock);
extern int rxi_Recvmsg(int socket, struct msghdr *msg_p, int flags);
extern int rxi_Sendmsg(osi_socket socket, struct msghdr *msg_p, int flags);


/* rx_misc.c */
#ifndef osi_alloc
extern char *osi_alloc(afs_int32 x);
#endif
#ifndef osi_free
extern int osi_free(char *x, afs_int32 size);
#endif

/* rx_multi.c */


/* rx_null.c */
extern struct rx_securityClass *rxnull_NewServerSecurityObject(void);
extern struct rx_securityClass *rxnull_NewClientSecurityObject(void);

/* rx_packet.c */
extern afs_int32 rx_SlowGetInt32(struct rx_packet *packet, size_t offset);
extern afs_int32 rx_SlowPutInt32(struct rx_packet *packet, size_t offset, afs_int32 data);
extern afs_int32 rx_SlowReadPacket(struct rx_packet *packet, unsigned int offset,
	int resid, char *out);
extern afs_int32 rx_SlowWritePacket(struct rx_packet *packet, int offset,
	int resid, char *in);
extern void rxi_freeCBuf(struct rx_packet *c);
extern int rxi_RoundUpPacket(struct rx_packet *p, unsigned int nb);
extern int rxi_AllocDataBuf(struct rx_packet *p, int nb, int class);
extern void rxi_MorePackets(int apackets);
extern void rxi_MorePacketsNoLock(int apackets);
extern void rxi_FreeAllPackets(void);
extern void rx_CheckPackets(void);
extern void rxi_FreePacketNoLock(struct rx_packet *p);
extern int rxi_FreeDataBufsNoLock(struct rx_packet *p, int first);
extern void rxi_RestoreDataBufs(struct rx_packet *p);
extern int rxi_TrimDataBufs(struct rx_packet *p, int first);
extern void rxi_FreePacket(struct rx_packet *p);
extern struct rx_packet *rxi_AllocPacketNoLock(int class);
extern struct rx_packet *rxi_AllocPacket(int class);
extern struct rx_packet *rxi_AllocSendPacket(register struct rx_call *call, int want);
extern int rxi_ReadPacket(int socket, register struct rx_packet *p, afs_uint32 *host, u_short *port);
extern struct rx_packet *rxi_SplitJumboPacket(register struct rx_packet *p, afs_int32 host, 
        short port, int first);
#ifndef KERNEL
extern int osi_NetSend(osi_socket socket, char *addr, struct iovec *dvec, int nvecs, 
        int length, int istack);
#endif
extern struct rx_packet *rxi_ReceiveDebugPacket(register struct rx_packet *ap, 
        osi_socket asocket, afs_int32 ahost, short aport, int istack);
extern struct rx_packet *rxi_ReceiveVersionPacket(register struct rx_packet *ap, 
        osi_socket asocket, afs_int32 ahost, short aport, int istack);
extern void rxi_SendPacket(struct rx_connection * conn, struct rx_packet *p,
                    int istack);
extern void rxi_SendPacketList(struct rx_connection * conn, struct rx_packet **list,
        int len, int istack);
extern struct rx_packet *rxi_SendSpecial(register struct rx_call *call,
        register struct rx_connection *conn, struct rx_packet *optionalPacket,
        int type, char *data, int nbytes, int istack);
extern void rxi_EncodePacketHeader(register struct rx_packet *p);
extern void rxi_DecodePacketHeader(register struct rx_packet *p);
extern void rxi_PrepareSendPacket(register struct rx_call *call, register struct rx_packet *p, 
        register int last);
extern int rxi_AdjustIfMTU(int mtu);
extern int rxi_AdjustMaxMTU(int mtu, int peerMaxMTU);
extern int rxi_AdjustDgramPackets(int frags, int mtu);


/* rxperf.c */


/* rx_pthread.c */
extern void rxi_Delay(int sec);
extern void rxi_InitializeThreadSupport(void);
extern void rxi_StartServerProc(void (*proc)(void), int stacksize);
#ifndef rxi_ReScheduleEvents
extern void rxi_ReScheduleEvents(void);
#endif
extern void rx_ServerProc(void);
extern void rxi_StartListener(void);
extern int rxi_Listen(osi_socket sock);
extern int rxi_Recvmsg(int socket, struct msghdr *msg_p, int flags);
extern int rxi_Sendmsg(osi_socket socket, struct msghdr *msg_p, int flags);


/* rx_rdwr.c */
extern int rxi_ReadProc(register struct rx_call *call, register char *buf, 
        register int nbytes);
extern int rx_ReadProc(struct rx_call *call, char *buf, int nbytes);
extern int rx_ReadProc32(struct rx_call *call, afs_int32 *value);
extern int rxi_FillReadVec(struct rx_call *call, afs_uint32 seq, 
        afs_uint32 serial, afs_uint32 flags);
extern int rxi_ReadvProc(struct rx_call *call, struct iovec *iov, int *nio, 
        int maxio, int nbytes);
extern int rx_ReadvProc(struct rx_call *call, struct iovec *iov, 
        int *nio, int maxio, int nbytes);
extern int rxi_WriteProc(register struct rx_call *call, register char *buf,    
        register int nbytes);
extern int rx_WriteProc(struct rx_call *call, char *buf, int nbytes);
extern int rx_WriteProc32(register struct rx_call *call, register afs_int32 *value);
extern int rxi_WritevAlloc(struct rx_call *call, struct iovec *iov, 
        int *nio, int maxio, int nbytes);
extern int rx_WritevAlloc(struct rx_call *call, struct iovec *iov, int *nio, 
        int maxio, int nbytes);
extern int rx_WritevInit(struct rx_call *call);
extern int rxi_WritevProc(struct rx_call *call, struct iovec *iov, 
        int nio, int nbytes);
extern int rx_WritevProc(struct rx_call *call, struct iovec *iov, int nio, int nbytes);
extern void rxi_FlushWrite(register struct rx_call *call);
extern void rx_FlushWrite(struct rx_call *call);

/* rx_stream.c */


/* rx_trace.c */


/* rx_user.c */
#ifdef AFS_PTHREAD_ENV
extern pthread_mutex_t rx_if_init_mutex;
extern pthread_mutex_t rx_if_mutex;
#endif
extern osi_socket rxi_GetUDPSocket(u_short port);
extern void osi_AssertFailU(const char *expr, const char *file, int line);
extern int rx_getAllAddr (afs_int32 *buffer, int maxSize);
extern void osi_Panic(); /* leave without args till stdarg rewrite */
extern void rxi_InitPeerParams(struct rx_peer *pp);


/* rx_xmit_nt.c */


/* MISC PROTOTYPES - MOVE TO APPROPRIATE LOCATION LATER */

/* keep here for now, 64 bit issues */
extern void *afs_osi_Alloc(size_t x);
extern void *afs_osi_Alloc_NoSleep(size_t x);
extern void afs_osi_Free(void *x, size_t asize);

#endif /* _RX_PROTOTYPES_H */
