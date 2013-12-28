/* AUTORIGHTS
Copyright (C) 2003 - 2010 Chaskiel Grundman
All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <afsconfig.h>
#include <afs/param.h>

#include <afs/afsutil.h>

#ifdef AFS_NT40_ENV
#include <windows.h>
#include <rpc.h>
#include <afs/cm_server.h>
#include <WINNT/syscfg.h>
#endif

#include <afs/afsutil.h>
#include "afscp.h"
#include "afscp_internal.h"

int afs_cb_inited = 0;
struct interfaceAddr afs_cb_interface;
static int afscp_maxcallbacks = 0, afscp_cballoced = 0;
struct afscp_callback *allcallbacks = NULL;

/*!
 * Initialize the callback interface structure
 */
static int
init_afs_cb(void)
{
    int cm_noIPAddr;		/* number of client network interfaces */
    int i;
#ifdef AFS_NT40_ENV
    /*
     * This Windows section was pulled in from changes to src/venus/afsio.c but is
     * untested here and may be unnecessary if rx_getAllAddr() can be used on that
     * platform.  However, there was already an ifdef here surrounding UuidCreate().
     */
    long rx_mtu = -1;
    int code;
    int cm_IPAddr[CM_MAXINTERFACE_ADDR];	/* client's IP address in host order */
    int cm_SubnetMask[CM_MAXINTERFACE_ADDR];	/* client's subnet mask in host order */
    int cm_NetMtu[CM_MAXINTERFACE_ADDR];	/* client's MTU sizes */
    int cm_NetFlags[CM_MAXINTERFACE_ADDR];	/* network flags */

    UuidCreate((UUID *) & afs_cb_interface.uuid);
    cm_noIPAddr = CM_MAXINTERFACE_ADDR;
    code = syscfg_GetIFInfo(&cm_noIPAddr,
			    cm_IPAddr, cm_SubnetMask, cm_NetMtu, cm_NetFlags);
    if (code > 0) {
	/* return all network interface addresses */
	afs_cb_interface.numberOfInterfaces = cm_noIPAddr;
	for (i = 0; i < cm_noIPAddr; i++) {
	    afs_cb_interface.addr_in[i] = cm_IPAddr[i];
	    afs_cb_interface.subnetmask[i] = cm_SubnetMask[i];
	    afs_cb_interface.mtu[i] = (rx_mtu == -1
				       || (rx_mtu != -1
					   && cm_NetMtu[i] <
					   rx_mtu)) ? cm_NetMtu[i] : rx_mtu;
	}
    } else {
	afs_cb_interface.numberOfInterfaces = 0;
    }
#else
    afs_uuid_create(&afs_cb_interface.uuid);
    cm_noIPAddr =
	rx_getAllAddr((afs_uint32 *) afs_cb_interface.addr_in,
		      AFS_MAX_INTERFACE_ADDR);
    if (cm_noIPAddr < 0)
	afs_cb_interface.numberOfInterfaces = 0;
    else {
	afs_cb_interface.numberOfInterfaces = cm_noIPAddr;
	/* we expect these in host byte order */
	for (i = 0; i < cm_noIPAddr; i++)
	    afs_cb_interface.addr_in[i] = ntohl(afs_cb_interface.addr_in[i]);
    }
#endif
    afs_cb_inited = 1;
    return 0;
}				/* init_afs_cb */

int
afscp_FindCallBack(const struct afscp_venusfid *f,
		   const struct afscp_server *server,
		   struct afscp_callback **ret)
{
    int i;
    struct afscp_callback *use = NULL, *cb;
    time_t now;
    struct afscp_venusfid fid;

    *ret = NULL;

    time(&now);
    for (i = 0; i < afscp_maxcallbacks; i++) {
	cb = &allcallbacks[i];
	if ((f->fid.Volume == cb->fid.Volume) &&
	    (f->fid.Vnode == cb->fid.Vnode) &&
	    (f->fid.Unique == cb->fid.Unique)) {
	    if (server && (cb->server != server))
		continue;
	    use = cb;
	    break;
	}
    }
    if (!use)
	return -1;

    if (use->cb.ExpirationTime + use->as_of < now) {
	if (use->valid) {
	    fid.cell = afscp_CellById(use->server->cell);
	    memcpy(&fid.fid, &use->fid, sizeof(struct AFSFid));
	    _StatInvalidate(&fid);
	}
	use->valid = 0;
    }

    if (use->valid)
	*ret = use;
    else
	return -1;

    return 0;
}

int
afscp_AddCallBack(const struct afscp_server *server,
		  const struct AFSFid *fid,
		  const struct AFSFetchStatus *fst,
		  const struct AFSCallBack *cb, const time_t as_of)
{
    int i;
    struct afscp_callback *use = NULL, *newlist;
    struct afscp_venusfid f;
    time_t now;
    
    time(&now);
    
    for (i = 0; i < afscp_maxcallbacks; i++) {
	if (allcallbacks[i].cb.ExpirationTime + allcallbacks[i].as_of < now) {
            if (allcallbacks[i].valid) {
                f.cell = afscp_CellById(allcallbacks[i].server->cell);
                memcpy(&f.fid, &allcallbacks[i].fid, sizeof(struct AFSFid));
                _StatInvalidate(&f);
            }
            allcallbacks[i].valid = 0;
        }
	
        if (allcallbacks[i].valid == 0)
            use = &allcallbacks[i];
        if ((allcallbacks[i].server == server) &&
            (fid->Volume == allcallbacks[i].fid.Volume) &&
            (fid->Vnode == allcallbacks[i].fid.Vnode) &&
            (fid->Unique == allcallbacks[i].fid.Unique)) {
            use = &allcallbacks[i];
            break;
        }
    }
    if (use == NULL) {
        if (afscp_maxcallbacks >= afscp_cballoced) {
            if (afscp_cballoced != 0)
                afscp_cballoced = afscp_cballoced * 2;
            else
                afscp_cballoced = 4;
            newlist = realloc(allcallbacks, afscp_cballoced *
			      sizeof(struct afscp_callback));
	    if (newlist == NULL) {
		return -1;
	    }
	    allcallbacks = newlist;
	}
	use = &allcallbacks[afscp_maxcallbacks++];
    }
    use->valid = 1;
    use->server = server;
    memmove(&use->fid, fid, sizeof(struct AFSFid));
    memmove(&use->cb, cb, sizeof(struct AFSCallBack));
    use->as_of = as_of;
    f.cell = afscp_CellById(server->cell);
    memcpy(&f.fid, fid, sizeof(struct AFSFid));
    _StatStuff(&f, fst);
    return 0;
}				/* afscp_AddCallBack */

int
afscp_RemoveCallBack(const struct afscp_server *server,
		     const struct afscp_venusfid *f)
{
    struct afscp_callback *cb;
    int i;

    _StatInvalidate(f);
    if (server == NULL) {
	return 0;
    }
    for (i = 0; i < afscp_maxcallbacks; i++) {
	cb = &allcallbacks[i];
	if ((cb->server == server) &&
	    (f->fid.Volume == cb->fid.Volume) &&
	    (f->fid.Vnode == cb->fid.Vnode) &&
	    (f->fid.Unique == cb->fid.Unique)) {
	    cb->valid = 0;
	    break;
	}
    }
    return 0;
}				/* afscp_ReturnCallBacks */

int
afscp_ReturnCallBacks(const struct afscp_server *server)
{
    struct AFSCBFids theFids;
    struct AFSCBs theCBs;
    struct afscp_callback *cb;
    struct afscp_venusfid f;
    struct rx_connection *c;
    int inited = 0;
    int ncallbacks = 0;
    int i, j, code;
    time_t now;

    time(&now);

    for (i = 0; i < afscp_maxcallbacks; i++) {
	cb = &allcallbacks[i];
	if (cb->server != server) {
	    continue;
	}
	if (cb->cb.ExpirationTime + cb->as_of < now) {
	    if (cb->valid) {
		f.cell = afscp_CellById(cb->server->cell);
		memcpy(&f.fid, &cb->fid, sizeof(struct AFSFid));
		_StatInvalidate(&f);
	    }
	    cb->valid = 0;
	    continue;
	}
	if (!inited) {
	    theFids.AFSCBFids_val = malloc(sizeof(struct AFSFid) * AFSCBMAX);
	    if (!theFids.AFSCBFids_val) {
		return -1;
	    }
	    memset(theFids.AFSCBFids_val, 0,
		   sizeof(struct AFSFid) * AFSCBMAX);
	    theCBs.AFSCBs_val = malloc(sizeof(struct AFSCallBack) * AFSCBMAX);
	    if (!theCBs.AFSCBs_val) {
		free(theFids.AFSCBFids_val);
		return -1;
	    }
	    memset(theCBs.AFSCBs_val, 0,
		   sizeof(struct AFSCallBack) * AFSCBMAX);
	    inited = 1;
	}

	if (ncallbacks == AFSCBMAX) {
	    theFids.AFSCBFids_len = ncallbacks;
	    theCBs.AFSCBs_len = ncallbacks;
	    for (j = 0; j < server->naddrs; j++) {
		c = afscp_ServerConnection(server, j);
		if (c == NULL)
		    break;
		code = RXAFS_GiveUpCallBacks(c, &theFids, &theCBs);
		if (code == 0)
		    break;
	    }
	    ncallbacks = 0;
	}
	memmove(&theFids.AFSCBFids_val[ncallbacks], &cb->fid,
		sizeof(struct AFSFid));
	memmove(&theCBs.AFSCBs_val[ncallbacks], &cb->cb,
		sizeof(struct AFSCallBack));

	theCBs.AFSCBs_val[ncallbacks].CallBackType = CB_DROPPED;
	ncallbacks++;
	if (cb->valid) {
	    f.cell = afscp_CellById(cb->server->cell);
	    memcpy(&f.fid, &cb->fid, sizeof(struct AFSFid));
	    _StatInvalidate(&f);
	}

	cb->valid = 0;
    }
    if (ncallbacks > 0) {
	theFids.AFSCBFids_len = ncallbacks;
	theCBs.AFSCBs_len = ncallbacks;
	for (j = 0; j < server->naddrs; j++) {
	    c = afscp_ServerConnection(server, j);
	    if (c == NULL)
		break;
	    code = RXAFS_GiveUpCallBacks(c, &theFids, &theCBs);
	    if (code == 0)
		break;
	}
	free(theFids.AFSCBFids_val);
	free(theCBs.AFSCBs_val);
    }
    return 0;
}				/* afscp_ReturnCallBacks */

int
afscp_ReturnAllCallBacks(void)
{
    struct afscp_server *s;
    int i;

    if (allcallbacks == NULL)
	return 0;
    for (i = 0; (s = afscp_ServerByIndex(i)); i++) {
	afscp_ReturnCallBacks(s);
    }
    free(allcallbacks);
    allcallbacks = NULL;
    afscp_maxcallbacks = 0;
    afscp_cballoced = 0;
    return 0;
}				/* afscp_ReturnAllCallBacks */

/*!
 * Handle a set of callbacks from the File Server.
 *
 * \param[in]	rxcall		Ptr to the associated Rx call structure.
 * \param[in]	Fids_Array	Ptr to the set of Fids.
 * \param[in]	CallBacks_Array	Ptr to the set of callbacks.
 *
 * \post Returns RXGEN_SUCCESS on success, Error value otherwise.
 *
 */
afs_int32
SRXAFSCB_CallBack(struct rx_call * rxcall, AFSCBFids * Fids_Array,
		  AFSCBs * CallBack_Array)
{
    struct rx_connection *rxconn = rx_ConnectionOf(rxcall);
    struct rx_peer *rxpeer = rx_PeerOf(rxconn);
    struct afscp_server *server = afscp_AnyServerByAddr(rx_HostOf(rxpeer));
    struct afscp_callback *cb;
    struct afscp_venusfid f;
    struct AFSFid *fid;
    int i;
    unsigned int j;

    if (server == NULL) {
	return 0;
    }
    for (i = 0; i < afscp_maxcallbacks; i++) {
	cb = &allcallbacks[i];
	if (cb->server != server)
	    continue;
	for (j = 0; j < Fids_Array->AFSCBFids_len; j++) {
	    fid = &Fids_Array->AFSCBFids_val[j];
	    if ((fid->Volume == cb->fid.Volume) &&
		(fid->Vnode == cb->fid.Vnode) &&
		(fid->Unique == cb->fid.Unique))
		cb->valid = 0;
	    f.cell = afscp_CellById(cb->server->cell);
	    memcpy(&f.fid, &cb->fid, sizeof(struct AFSFid));
	    _StatInvalidate(&f);
	}
    }

    return RXGEN_SUCCESS;
}				/*SRXAFSCB_CallBack */

/*!
 * Initialize callback state on this ``Cache Manager''.
 *
 * \param[in]	rxcall	Ptr to the associated Rx call structure.
 *
 * \post Returns RXGEN_SUCCESS on success, Error value otherwise.
 *
 * \note This will definitely be called by the File Server (exactly once),
 *       since it will think we are another new ``Cache Manager''.
 */
afs_int32
SRXAFSCB_InitCallBackState(struct rx_call * rxcall)
{
    struct rx_connection *rxconn = rx_ConnectionOf(rxcall);
    struct rx_peer *rxpeer = rx_PeerOf(rxconn);
    struct afscp_server *server = afscp_AnyServerByAddr(rx_HostOf(rxpeer));
    struct afscp_callback *cb;
    struct afscp_venusfid f;
    int i;

    if (server == NULL) {
	return 0;
    }
    for (i = 0; i < afscp_maxcallbacks; i++) {
	cb = &allcallbacks[i];
	if (cb->server != server)
	    continue;
	if (cb->valid) {
	    f.cell = afscp_CellById(cb->server->cell);
	    memcpy(&f.fid, &cb->fid, sizeof(struct AFSFid));
	    _StatInvalidate(&f);
	}
	cb->valid = 0;
    }
    return RXGEN_SUCCESS;
}				/* SRXAFSCB_InitCallBackState */

/*!
 * Respond to a probe from the File Server.
 *
 * \param[in] rxcall	Ptr to the associated Rx call structure.
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 * \note If a File Server doesn't hear from you every so often, it will
 *       send you a probe to make sure you're there, just like any other
 * 	 ``Cache Manager'' it's keeping track of.
 *
 */
afs_int32
SRXAFSCB_Probe(struct rx_call * rxcall)
{
    return RXGEN_SUCCESS;
}				/* SRXAFSCB_Probe */

/*!
 * Respond minimally to a request for returning the contents of
 * a cache lock, since someone out there thinks you're a Cache
 * Manager.
 *
 * \param[in]	rxcall	Ptr to the associated Rx call structure.
 * \param[in]	index
 * \param[out]	lock
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_GetLock(struct rx_call * rxcall, afs_int32 index, AFSDBLock * lock)
{
    return RXGEN_SUCCESS;

}				/*SRXAFSCB_GetLock */

/*!
 * Respond minimally to a request for returning the contents of
 * a cache entry, since someone out there thinks you're a Cache
 * Manager.
 *
 * \param[in]	rxcall	Ptr to the associated Rx call structure.
 * \param[in]	index
 * \param[out]	ce	Ptr to cache entry
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_GetCE(struct rx_call * rxcall, afs_int32 index, AFSDBCacheEntry * ce)
{
    return RXGEN_SUCCESS;
}				/* SRXAFSCB_GetCE */

/*!
 * Respond minimally to a request for returning the contents of
 * a cache entry, since someone out there thinks you're a Cache
 * Manager. (64-bit version, though same as SRXAFSCB_GetCE())
 *
 * \param[in]	rxcall	Ptr to the associated Rx call structure.
 * \param[in]	index
 * \param[out]	ce	Ptr to cache entry
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_GetCE64(struct rx_call * rxcall, afs_int32 index,
		 AFSDBCacheEntry64 * ce)
{
    return RXGEN_SUCCESS;
}				/*SRXAFSCB_GetCE */

/*!
 * Respond minimally to a request for fetching the version of
 * extended Cache Manager statistics offered, since someone out
 * there thinks you're a Cache Manager.
 *
 * \param[in]	rxcall		Ptr to the associated Rx call structure
 * \param[out]	versionNumberP
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_XStatsVersion(struct rx_call * rxcall, afs_int32 * versionNumberP)
{
    return RXGEN_SUCCESS;
}				/*SRXAFSCB_XStatsVersion */

/*!
 * Respond minimally to a request for returning extended
 * statistics for a Cache Manager, since someone out there thinks
 * you're a Cache Manager.
 *
 * \param[in]	z_call			Ptr to the associated Rx call structure
 * \param[in]	clientVersionNumber
 * \param[in]	collectionNumber
 * \param[out]	srvVersionNumberP
 * \param[out]	timeP
 * \param[out]	dataP
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_GetXStats(struct rx_call * z_call, afs_int32 clientVersionNumber,
		   afs_int32 collectionNumber, afs_int32 * srvVersionNumberP,
		   afs_int32 * timeP, AFSCB_CollData * dataP)
{
    return RXGEN_SUCCESS;
}				/*SRXAFSCB_GetXStats */

/*!
 * This routine was used in the AFS 3.5 beta release, but not anymore.
 * It has since been replaced by SRXAFSCB_InitCallBackState3.
 *
 * \param[in]	rxcall	Ptr to the associated Rx call structure.
 * \param[out]	addr	Ptr to return the list of interfaces for this client
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_InitCallBackState2(struct rx_call * rxcall,
			    struct interfaceAddr * addr)
{
    return RXGEN_OPCODE;
}				/* SRXAFSCB_InitCallBackState2 */

/*!
 *
 * \param	rxcall	Ptr to the associated Rx call structure.
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_TellMeAboutYourself(struct rx_call * a_call,
			     struct interfaceAddr * addr,
			     Capabilities * capabilities)
{
#ifdef AFS_NT40_ENV
    int code;
    int cm_noIPAddr;		/* number of client network interfaces */
    int cm_IPAddr[CM_MAXINTERFACE_ADDR];	/* client's IP address in host order */
    int cm_SubnetMask[CM_MAXINTERFACE_ADDR];	/* client's subnet mask in host order */
    int cm_NetMtu[CM_MAXINTERFACE_ADDR];	/* client's MTU sizes */
    int cm_NetFlags[CM_MAXINTERFACE_ADDR];	/* network flags */
    int i;

    cm_noIPAddr = CM_MAXINTERFACE_ADDR;
    code = syscfg_GetIFInfo(&cm_noIPAddr,
			    cm_IPAddr, cm_SubnetMask, cm_NetMtu, cm_NetFlags);
    if (code > 0) {
	/* return all network interface addresses */
	addr->numberOfInterfaces = cm_noIPAddr;
	for (i = 0; i < cm_noIPAddr; i++) {
	    addr->addr_in[i] = cm_IPAddr[i];
	    addr->subnetmask[i] = cm_SubnetMask[i];
	    addr->mtu[i] = cm_NetMtu[i];
	}
    } else {
	addr->numberOfInterfaces = 0;
    }
#else
    if (a_call && addr) {
	if (!afs_cb_inited)
	    init_afs_cb();
	*addr = afs_cb_interface;
    }
#endif
    if (capabilities != NULL) {
	afs_uint32 *dataBuffP;
	afs_int32 dataBytes;

	dataBytes = 1 * sizeof(afs_uint32);
	dataBuffP = (afs_uint32 *) xdr_alloc(dataBytes);
	dataBuffP[0] = CLIENT_CAPABILITY_ERRORTRANS;
	capabilities->Capabilities_len = dataBytes / sizeof(afs_uint32);
	capabilities->Capabilities_val = dataBuffP;
    }
    return RXGEN_SUCCESS;
}				/* SRXAFSCB_TellMeAboutYourself */

/*!
 * Routine called by the server-side callback RPC interface to
 * obtain a unique identifier for the client. The server uses
 * this identifier to figure out whether or not two RX connections
 * are from the same client, and to find out which addresses go
 * with which clients.
 *
 * \param[in]	rxcall	Ptr to the associated Rx call structure.
 * \param[out]	addr	Ptr to return the list of interfaces for this client
 *
 * \post Returns output of TellMeAboutYourself (which
 *       should be RXGEN_SUCCESS).
 *
 */
afs_int32
SRXAFSCB_WhoAreYou(struct rx_call * rxcall, struct interfaceAddr * addr)
{
    return SRXAFSCB_TellMeAboutYourself(rxcall, addr, NULL);
}				/* SRXAFSCB_WhoAreYou */

/*!
 * Routine called by the server-side callback RPC interface to
 * implement clearing all callbacks from this host.
 *
 * \param[in]	rxcall		Ptr to the associated Rx call structure.
 * \param[in]	serverUuid	Ptr to UUID
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_InitCallBackState3(struct rx_call * rxcall, afsUUID * serverUuid)
{
    struct rx_connection *rxconn = rx_ConnectionOf(rxcall);
    struct rx_peer *rxpeer = rx_PeerOf(rxconn);
    struct afscp_server *server = afscp_AnyServerByAddr(rx_HostOf(rxpeer));
    struct afscp_callback *cb;
    struct afscp_venusfid f;
    int i;

    if (server == NULL) {
	return 0;
    }
    for (i = 0; i < afscp_maxcallbacks; i++) {
	cb = &allcallbacks[i];
	if (cb->server != server)
	    continue;
	if (cb->valid) {
	    f.cell = afscp_CellById(cb->server->cell);
	    memcpy(&f.fid, &cb->fid, sizeof(struct AFSFid));
	    _StatInvalidate(&f);
	}
	cb->valid = 0;
    }
    return RXGEN_SUCCESS;
}				/* SRXAFSCB_InitCallBackState3 */

/*!
 * Routine called by the server-side callback RPC interface to
 * implement ``probing'' the Cache Manager, just making sure it's
 * still there is still the same client it used to be.
 *
 * \param	rxcall		Ptr to the associated Rx call structure.
 * \param	clientUuid	Ptr to UUID that must match the client's UUID
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_ProbeUuid(struct rx_call * rxcall, afsUUID * clientUuid)
{
    int code = 0;
    if (!afs_cb_inited)
	init_afs_cb();
    if (!afs_uuid_equal(clientUuid, &afs_cb_interface.uuid))
	code = 1;		/* failure */
    return code;
}				/* SRXAFSCB_ProbeUuid */

/*!
 * Routine to list server preferences used by this client.
 *
 * \param[in]	a_call		Ptr to Rx call on which this request came in.
 * \param[in]	a_index		Input server index
 * \param[out]	a_srvr_addr	Output server address (0xffffffff on last server)
 * \param[out]	a_srvr_rank	Output server rank
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
afs_int32
SRXAFSCB_GetServerPrefs(struct rx_call * a_call, afs_int32 a_index,
			afs_int32 * a_srvr_addr, afs_int32 * a_srvr_rank)
{
    *a_srvr_addr = 0xffffffff;
    *a_srvr_rank = 0xffffffff;
    return RXGEN_SUCCESS;
}				/* SRXAFSCB_GetServerPrefs */

/*!
 * Routine to list cells configured for this client
 *
 * \param[in]	a_call	Ptr to Rx call on which this request came in.
 * \param[in]	a_index	Input cell index
 * \param[out]	a_name	Output cell name ("" on last cell)
 * \param[out]	a_hosts	Output cell database servers
 *
 * \post Returns RXGEN_OPCODE (always)
 *
 */
afs_int32
SRXAFSCB_GetCellServDB(struct rx_call * a_call, afs_int32 a_index,
		       char **a_name, afs_int32 * a_hosts)
{
    return RXGEN_OPCODE;
}				/* SRXAFSCB_GetCellServDB */

/*!
 * Routine to return name of client's local cell
 *
 * \param[in]	a_call	Ptr to Rx call on which this request came in.
 * \param[out]	a_name	Output cell name
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
int
SRXAFSCB_GetLocalCell(struct rx_call *a_call, char **a_name)
{
    return RXGEN_OPCODE;
}				/* SRXAFSCB_GetLocalCell */

/*!
 * Routine to return parameters used to initialize client cache.
 * Client may request any format version. Server may not return
 * format version greater than version requested by client.
 *
 * \param[in]	a_call		Ptr to Rx call on which this request came in.
 * \param[in]	callerVersion	Data format version desired by the client.
 * \param[out]	serverVersion	Data format version of output data.
 * \param[out]	configCount	Number bytes allocated for output data.
 * \param[out]	config		Client cache configuration.
 *
 * \post Returns RXGEN_SUCCESS (always)
 *
 */
int
SRXAFSCB_GetCacheConfig(struct rx_call *a_call, afs_uint32 callerVersion,
			afs_uint32 * serverVersion, afs_uint32 * configCount,
			cacheConfig * config)
{
    return RXGEN_OPCODE;
}				/* SRXAFSCB_GetCacheConfig */

/*!

 *
 * \param[in]	rxcall	Ptr to the associated Rx call structure.
 *
 * \post Returns RXGEN_OPCODE (always)
 *
 */
int
SRXAFSCB_GetCellByNum(struct rx_call *a_call, afs_int32 a_index,
		      char **a_name, afs_int32 * a_hosts)
{
    return RXGEN_OPCODE;
}				/* SRXAFSCB_GetCellByNum */
