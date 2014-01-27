/*
 * Copyright (c) 2014 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Secure Endpoints Inc. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software without
 *   specific prior written permission from Secure Endpoints, Inc. and
 *   Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <afs/stds.h>
#include "afsd.h"
#include "cm_getaddrs.h"

/*
 * cm_GetAddrsU takes as input a uuid and a unique value which
 * represent the set of addresses that are required.  These values
 * are used as input to the VL_GetAddrsU RPC which returns a list
 * of addresses.  For each returned address a bucket in the provided
 * arrays (serverFlags, serverNumber, serverUUID, serverUnique)
 * are populated.  The serverFlags array entries are filled with the
 * 'Flags' value provided as input.  'serverNumber' is the server's
 * IP address.
 */

afs_uint32
cm_GetAddrsU(cm_cell_t *cellp, cm_user_t *userp, cm_req_t *reqp,
	     afsUUID *Uuid, afs_int32 Unique, afs_int32 Flags,
	     int *index,
	     afs_int32 serverFlags[],
	     afs_int32 serverNumber[],
	     afsUUID serverUUID[],
	     afs_int32 serverUnique[])
{
    afs_uint32 code = 0;
    cm_conn_t *connp;
    struct rx_connection *rxconnp;
    afs_uint32 * addrp, nentries;
    afs_int32 unique;
    bulkaddrs  addrs;
    ListAddrByAttributes attrs;
    afsUUID uuid;
    int i;

    memset(&uuid, 0, sizeof(uuid));
    memset(&addrs, 0, sizeof(addrs));
    memset(&attrs, 0, sizeof(attrs));

    attrs.Mask = VLADDR_UUID;
    attrs.uuid = *Uuid;

    do {
	code = cm_ConnByMServers(cellp->vlServersp, 0, userp, reqp, &connp);
	if (code)
	    continue;
	rxconnp = cm_GetRxConn(connp);
	code = VL_GetAddrsU(rxconnp, &attrs, &uuid, &unique, &nentries,
			    &addrs);
	rx_PutConnection(rxconnp);
    } while (cm_Analyze(connp, userp, reqp, NULL, cellp, 0, NULL, NULL,
			&cellp->vlServersp, NULL, code));

    code = cm_MapVLRPCError(code, reqp);

    if (afsd_logp->enabled) {
	char uuidstr[128];
	afsUUID_to_string(Uuid, uuidstr, sizeof(uuidstr));

	if (code)
	    osi_Log2(afsd_logp,
		     "CALL VL_GetAddrsU serverNumber %s FAILURE, code 0x%x",
		     osi_LogSaveString(afsd_logp, uuidstr),
		     code);
	else
	    osi_Log1(afsd_logp, "CALL VL_GetAddrsU serverNumber %s SUCCESS",
		     osi_LogSaveString(afsd_logp, uuidstr));
    }

    if (code)
	return CM_ERROR_RETRY;

    if (nentries == 0) {
	code = CM_ERROR_INVAL;
    } else {
	addrp = addrs.bulkaddrs_val;
	for (i = 0; i < nentries && (*index) < NMAXNSERVERS; (*index)++, i++) {
	    serverFlags[*index] = Flags;
	    serverNumber[*index] = addrp[i];
	    serverUUID[*index] = uuid;
	    serverUnique[*index] = unique;
	}
    }
    xdr_free((xdrproc_t) xdr_bulkaddrs, &addrs);

    return code;
}
