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
#if 0
extern void rx_PrintTheseStats (FILE *file, struct rx_stats *s, int size, 
        afs_int32 freePackets, char version);
extern void rx_PrintStats(FILE *file);
extern void rx_PrintPeerStats(FILE *file, struct rx_peer *peer);
#endif
extern afs_int32 rx_GetServerDebug(int socket, afs_uint32 remoteAddr, 
        afs_uint16 remotePort, struct rx_debugStats *stat, afs_uint32 *supportedValues);
extern afs_int32 rx_GetServerStats(int socket, afs_uint32 remoteAddr,
        afs_uint16 remotePort, struct rx_stats *stat, afs_uint32 *supportedValues);
extern afs_int32 rx_GetServerVersion(int socket, afs_uint32 remoteAddr,
        afs_uint16 remotePort, size_t version_length, char *version);
extern afs_int32 rx_GetServerConnections(int socket, afs_uint32 remoteAddr,
        afs_uint16 remotePort, afs_int32 *nextConnection, int allConnections,
        afs_uint32 debugSupportedValues, struct rx_debugConn *conn, afs_uint32 *supportedValues);
extern afs_int32 rx_GetServerPeers(int socket, afs_uint32 remoteAddr, afs_uint16 remotePort,
        afs_int32 *nextPeer, afs_uint32 debugSupportedValues, struct rx_debugPeer *peer,
        afs_uint32 *supportedValues);
extern void shutdown_rx(void);
#ifdef RX_ENABLE_LOCKS
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
#if 0
extern int rx_RetrieveProcessRPCStats(afs_uint32 callerVersion,
        afs_uint32 *myVersion, afs_uint32 *clock_sec, afs_uint32 *clock_usec,
        size_t *allocSize, afs_uint32 *statCount, afs_uint32 **stats);
#endif
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
extern int clock_UnInit(void);
#endif

/* rx_clock_nt.c */


/* rx_conncache.c */


/* rxdebug.c */


/* rx_event.c */


/* rx_getaddr.c */


/* rx_globals.c */


/* rx_kcommon.c */
#ifdef UKERNEL
extern void rx_ServerProc(void);
#endif

/* rx_lwp.c */
extern void rx_ServerProc(void);

/* rx_misc.c */


/* rx_multi.c */


/* rx_null.c */


/* rx_packet.c */


/* rxperf.c */


/* rx_pthread.c */


/* rx_rdwr.c */


/* rx_stream.c */


/* rx_trace.c */


/* rx_user.c */


/* rx_xmit_nt.c */


/* xdr_afsuuid.c */


/* xdr_array.c */


/* xdr_arrayn.c */


/* xdr.c */


/* xdr_float.c */


/* xdr_int64.c */


/* xdr_mem.c */


/* xdr_rec.c */


/* xdr_refernce.c */


/* xdr_rx.c */


/* xdr_stdio.c */


/* xdr_update.c */



/* MISC PROTOTYPES - MOVE TO APPROPRIATE LOCATION LATER */




#endif /* _RX_PROTOTYPES_H */
