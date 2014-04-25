/*
 * Copyright 2000, International Business Machines Corporation and others.
 * Copyright 2011, Your File System Inc
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include "rx.h"
#include "rx_conn.h"

void
rx_SetConnectionEpoch(struct rx_connection *conn, afs_uint32 epoch) {
    conn->epoch = epoch;
}

afs_uint32
rx_GetConnectionEpoch(struct rx_connection *conn) {
    return conn->epoch;
}

void
rx_SetConnectionId(struct rx_connection *conn, afs_uint32 cid) {
    conn->cid = cid;
}

afs_uint32
rx_GetConnectionId(struct rx_connection *conn) {
    return conn->cid;
}

void
rx_SetSecurityData(struct rx_connection *conn, void *data) {
    conn->securityData = data;
}

void *
rx_GetSecurityData(struct rx_connection *conn)
{
    return conn->securityData;
}

int
rx_IsUsingPktCksum(struct rx_connection *conn)
{
    return conn->flags & RX_CONN_USING_PACKET_CKSUM;
}

void
rx_SetSecurityHeaderSize(struct rx_connection *conn, afs_uint32 size)
{
    conn->securityHeaderSize = size;
}

afs_uint32
rx_GetSecurityHeaderSize(struct rx_connection *conn)
{
    return conn->securityHeaderSize;
}

void
rx_SetSecurityMaxTrailerSize(struct rx_connection *conn, afs_uint32 size)
{
    conn->securityMaxTrailerSize = size;
}

afs_uint32
rx_GetSecurityMaxTrailerSize(struct rx_connection *conn)
{
    return conn->securityMaxTrailerSize;
}

void
rx_SetMsgsizeRetryErr(struct rx_connection *conn, int err)
{
    conn->msgsizeRetryErr = err;
}

int
rx_IsServerConn(struct rx_connection *conn)
{
    return conn->type == RX_SERVER_CONNECTION;
}

int
rx_IsClientConn(struct rx_connection *conn)
{
    return conn->type == RX_CLIENT_CONNECTION;
}

struct rx_peer *
rx_PeerOf(struct rx_connection *conn)
{
    return conn->peer;
}

u_short
rx_ServiceIdOf(struct rx_connection *conn)
{
    return conn->serviceId;
}

int
rx_SecurityClassOf(struct rx_connection *conn)
{
    return conn->securityIndex;
}

struct rx_securityClass *
rx_SecurityObjectOf(const struct rx_connection *conn)
{
    return conn->securityObject;
}

struct rx_service *
rx_ServiceOf(struct rx_connection *conn)
{
    return conn->service;
}

int
rx_ConnError(struct rx_connection *conn)
{
    return conn->error;
}
