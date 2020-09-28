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
#include "rx_call.h"
#include "rx_conn.h"
#include "rx_atomic.h"
#include "rx_internal.h"

struct rx_connection *
rx_ConnectionOf(struct rx_call *call)
{
    return call->conn;
}

int
rx_Error(struct rx_call *call)
{
    return call->error;
}

int
rx_GetRemoteStatus(struct rx_call *call)
{
    return call->remoteStatus;
}

void
rx_SetLocalStatus(struct rx_call *call, int status)
{
    call->localStatus = status;
}

int
rx_GetCallAbortCode(struct rx_call *call)
{
    return call->abortCode;
}

void
rx_SetCallAbortCode(struct rx_call *call, int code)
{
    call->abortCode = code;
}

void
rx_RecordCallStatistics(struct rx_call *call, unsigned int rxInterface,
			unsigned int currentFunc, unsigned int totalFunc,
			int isServer)
{
    struct clock queue;
    struct clock exec;

    clock_GetTime(&exec);
    clock_Sub(&exec, &call->startTime);
    queue = call->startTime;
    clock_Sub(&queue, &call->queueTime);

    rxi_IncrementTimeAndCount(call->conn->peer, rxInterface, currentFunc,
			     totalFunc, &queue, &exec, call->app.bytesSent,
			     call->app.bytesRcvd, isServer);
}

/*
 * Accessor for VolMonitor 'vos status'
 * note: the rx_call members are all unsigned, but our return targets
 * in wire struct transDebugInfo are all signed.  Sigh.
 */
void
rx_GetCallStatus(struct rx_call *call, afs_int32 *readNext,
			afs_int32 *transmitNext, int *lastSendTime,
			int *lastReceiveTime)
{
    if (readNext != NULL)
	*readNext = call->rnext;
    if (transmitNext != NULL)
	*transmitNext = call->tnext;
    if (lastSendTime != NULL)
	*lastSendTime = call->lastSendTime;
    if (lastReceiveTime != NULL)
	*lastReceiveTime = call->lastReceiveTime;
}
