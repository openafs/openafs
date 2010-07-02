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
#include <afs/param.h>
#include <afs/afsint.h>               /*Callback interface defs*/
#include <afs/afsutil.h>
#include <stdlib.h>
#include <string.h>
#include "afscp.h"
#include "afscp_internal.h"

int afs_cb_inited = 0;
struct interfaceAddr afs_cb_interface;
static int afs_maxcallbacks=0, afs_cballoced=0;
struct afs_callback *allcallbacks=NULL;

static int init_afs_cb() {
    int count;

    afs_uuid_create(&afs_cb_interface.uuid);
    count = rx_getAllAddr(&afs_cb_interface.addr_in, AFS_MAX_INTERFACE_ADDR);
    if ( count <= 0 )
        afs_cb_interface.numberOfInterfaces = 0;
    else
        afs_cb_interface.numberOfInterfaces = count;
    afs_cb_inited = 1;
    return 0;
}

int AddCallBack(const struct afs_server *server, const struct AFSFid *fid,
                const struct AFSFetchStatus *fst, const struct AFSCallBack *cb)
{
     int i;
     struct afs_callback *use=NULL, *newlist;
     struct afs_venusfid f;
     time_t now;

     time(&now);

     for (i=0;i<afs_maxcallbacks;i++) {
	  if (allcallbacks[i].cb.ExpirationTime < now) {
            if (allcallbacks[i].valid) {
                 f.cell=afs_cellbyid(allcallbacks[i].server->cell);
                 memcpy(&f.fid, &allcallbacks[i].fid, sizeof(struct afs_venusfid));
                 _StatInvalidate(&f);
            }
	    allcallbacks[i].valid=0;

	  }

          if (allcallbacks[i].valid == 0)
               use=&allcallbacks[i];
	  if (allcallbacks[i].server==server &&
	      fid->Volume == allcallbacks[i].fid.Volume &&
	      fid->Vnode == allcallbacks[i].fid.Vnode &&
	      fid->Unique == allcallbacks[i].fid.Unique) {
	    use=&allcallbacks[i];
	    break;
	  }
     }
     if (!use) {
          if (afs_maxcallbacks >= afs_cballoced) {
               if (afs_cballoced)
                    afs_cballoced = afs_cballoced *2;
               else
                    afs_cballoced = 4;
               newlist=realloc(allcallbacks, afs_cballoced *
                               sizeof(struct afs_callback));
               if (!newlist)
                    return -1;
               allcallbacks=newlist;
          }
          use=&allcallbacks[afs_maxcallbacks++];
     }
     use->valid=1;
     use->server=server;
     memmove(&use->fid, fid, sizeof(struct AFSFid));
     memmove(&use->cb, cb, sizeof(struct AFSCallBack));
     f.cell=afs_cellbyid(server->cell);
     memcpy(&f.fid, fid, sizeof(struct AFSFid));
     _StatStuff(&f, fst);
     return 0;
}
int RemoveCallBack(const struct afs_server *server, const struct afs_venusfid *f)
{
     struct afs_callback *cb;
     int i;

     _StatInvalidate(f);
     if (!server)
          return 0;
     for (i=0;i<afs_maxcallbacks;i++) {
          cb=&allcallbacks[i];
          if (cb->server == server &&
              f->fid.Volume == cb->fid.Volume &&
              f->fid.Vnode == cb->fid.Vnode &&
              f->fid.Unique == cb->fid.Unique) {
               cb->valid = 0;
               break;
          }
     }
     return 0;
}

int ReturnCallBacks(const struct afs_server *server)
{
     struct AFSCBFids theFids;
     struct AFSCBs theCBs;
     struct afs_callback *cb;
     struct afs_venusfid f;
     int inited=0;
     int ncallbacks=0;
     int i,j;
     time_t now;

     time(&now);

     for (i=0;i<afs_maxcallbacks;i++) {
          cb=&allcallbacks[i];
/*          printf("%d %x %x %ld %d", i, cb, cb->server, cb->cb.ExpirationTime,
            cb->valid);*/
          if (cb->server != server)
               continue;
	  if (cb->cb.ExpirationTime < now) {
            if (cb->valid) {
                 f.cell=afs_cellbyid(cb->server->cell);
                 memcpy(&f.fid, &cb->fid, sizeof(struct afs_venusfid));
                 _StatInvalidate(&f);
            }

	    cb->valid=0;
	    continue;
	  }
          if (!inited) {
               theFids.AFSCBFids_val=malloc(sizeof(struct AFSCallBack) * AFSCBMAX);
               if (!theFids.AFSCBFids_val)
                    return -1;
               theCBs.AFSCBs_val=malloc(sizeof(struct AFSFid) * AFSCBMAX);
               if (!theCBs.AFSCBs_val) {
                    free(theFids.AFSCBFids_val);
                    return -1;
               }
          }

          if (ncallbacks == AFSCBMAX) {
               theFids.AFSCBFids_len=ncallbacks;
               theCBs.AFSCBs_len=ncallbacks;
	       for (j=0;j<server->naddrs;j++) {
		 if (!RXAFS_GiveUpCallBacks(server->conns[j], &theFids,
					   &theCBs))
		   break;
	       }
               ncallbacks=0;
          }
          memmove(&theFids.AFSCBFids_val[ncallbacks], &cb->fid,
                  sizeof(struct AFSFid));
          memmove(&theCBs.AFSCBs_val[ncallbacks], &cb->cb,
                  sizeof(struct AFSCallBack));

          theCBs.AFSCBs_val[ncallbacks].CallBackType = CB_DROPPED;
	  ncallbacks++;
          if (cb->valid) {
               f.cell=afs_cellbyid(cb->server->cell);
               memcpy(&f.fid, &cb->fid, sizeof(struct afs_callback));
               _StatInvalidate(&f);
          }

	  cb->valid=0;
     }
     if (ncallbacks) {
       theFids.AFSCBFids_len=ncallbacks;
       theCBs.AFSCBs_len=ncallbacks;
       for (j=0;j<server->naddrs;j++) {
	 if (!RXAFS_GiveUpCallBacks(server->conns[j], &theFids,
				   &theCBs))
	   break;
       }
       free(theFids.AFSCBFids_val);
       free(theCBs.AFSCBs_val);
     }
     return 0;
}

int ReturnAllCallBacks(void)
{
     struct afs_server *s;
     int i;

     for (i=0;(s=afs_serverbyindex(i));i++)
          ReturnCallBacks(s);
     return 0;
}



afs_int32 SRXAFSCB_CallBack(rxcall, Fids_Array, CallBack_Array)
    struct rx_call *rxcall;
    AFSCBFids *Fids_Array;
    AFSCBs *CallBack_Array;

{ /*SRXAFSCB_CallBack*/
     struct rx_connection *rxconn=rx_ConnectionOf(rxcall);
     struct rx_peer *rxpeer=rx_PeerOf(rxconn);
     struct afs_server *server=afs_anyserverbyaddr(rxpeer->host);
     struct afs_callback *cb;
     struct afs_venusfid f;
     struct AFSFid *fid;
     int i,j;

     if (!server)
          return 0;
     for (i=0;i<afs_maxcallbacks;i++) {
          cb=&allcallbacks[i];
          if (cb->server != server)
               continue;
          for (j=0;j<Fids_Array->AFSCBFids_len;j++) {
               fid=&Fids_Array->AFSCBFids_val[j];
               if (fid->Volume == cb->fid.Volume &&
                   fid->Vnode == cb->fid.Vnode &&
                   fid->Unique == cb->fid.Unique)
                    cb->valid = 0;
               f.cell=afs_cellbyid(cb->server->cell);
               memcpy(&f.fid, &cb->fid, sizeof(struct afs_venusfid));
               _StatInvalidate(&f);
          }
     }

    return(0);

} /*SRXAFSCB_CallBack*/


afs_int32 SRXAFSCB_InitCallBackState(rxcall)
    struct rx_call *rxcall;

{ /*SRXAFSCB_InitCallBackState*/
     struct rx_connection *rxconn=rx_ConnectionOf(rxcall);
     struct rx_peer *rxpeer=rx_PeerOf(rxconn);
     struct afs_server *server=afs_anyserverbyaddr(rxpeer->host);
     struct afs_callback *cb;
     struct afs_venusfid f;
     int i;

     if (!server)
          return 0;
     for (i=0;i<afs_maxcallbacks;i++) {
          cb=&allcallbacks[i];
          if (cb->server != server)
               continue;
          if (cb->valid) {
               f.cell=afs_cellbyid(cb->server->cell);
               memcpy(&f.fid, &cb->fid, sizeof(struct afs_callback));
               _StatInvalidate(&f);
          }
          cb->valid = 0;
     }
     return(0);

} /*SRXAFSCB_InitCallBackState*/

afs_int32 SRXAFSCB_Probe(rxcall)
        struct rx_call *rxcall;

{ /*SRXAFSCB_Probe*/
    return(0);

} /*SRXAFSCB_Probe*/


afs_int32 SRXAFSCB_GetCE(rxcall)
    struct rx_call *rxcall;

{ /*SRXAFSCB_GetCE*/
     return(0);
} /*SRXAFSCB_GetCE*/

afs_int32 SRXAFSCB_GetCE64(rxcall)
    struct rx_call *rxcall;

{ /*SRXAFSCB_GetCE*/
     return(0);
} /*SRXAFSCB_GetCE*/


afs_int32 SRXAFSCB_GetLock(rxcall)
    struct rx_call *rxcall;

{ /*SRXAFSCB_GetLock*/
    return(0);

} /*SRXAFSCB_GetLock*/
afs_int32 SRXAFSCB_XStatsVersion(rxcall)
    struct rx_call *rxcall;

{ /*SRXAFSCB_XStatsVersion*/
    return(0);

} /*SRXAFSCB_XStatsVersion*/

afs_int32 SRXAFSCB_GetXStats(rxcall)
    struct rx_call *rxcall;

{ /*SRXAFSCB_GetXStats*/
     return(0);
} /*SRXAFSCB_GetXStats*/

int SRXAFSCB_InitCallBackState2(rxcall, addr)
struct rx_call *rxcall;
struct interfaceAddr * addr;
{
    return RXGEN_OPCODE;
}

int SRXAFSCB_WhoAreYou(rxcall, addr)
struct rx_call *rxcall;
struct interfaceAddr *addr;
{
    XDR x;
    if ( rxcall && addr )
    {
        if (!afs_cb_inited) init_afs_cb();
        *addr = afs_cb_interface;
    }
#ifdef STRANGEDEBUG
    xdrrx_create(&x, rxcall, XDR_ENCODE);
    xdr_interfaceAddr(&x, addr);
    rx_Write(rxcall,"",0);
    rxi_FlushWrite(rxcall);
    rx_EndCall(rxcall, 0);
    IOMGR_Sleep(10);
    IOMGR_Sleep(600);
#endif
    return(0);
}

int SRXAFSCB_InitCallBackState3(rxcall, uuidp)
struct rx_call *rxcall;
afsUUID *uuidp;
{
     struct rx_connection *rxconn=rx_ConnectionOf(rxcall);
     struct rx_peer *rxpeer=rx_PeerOf(rxconn);
     struct afs_server *server=afs_anyserverbyaddr(rxpeer->host);
     struct afs_callback *cb;
     struct afs_venusfid f;
     int i;

     if (!server)
          return 0;
     for (i=0;i<afs_maxcallbacks;i++) {
          cb=&allcallbacks[i];
          if (cb->server != server)
               continue;
          if (cb->valid) {
               f.cell=afs_cellbyid(cb->server->cell);
               memcpy(&f.fid, &cb->fid, sizeof(struct afs_callback));
               _StatInvalidate(&f);
          }
          cb->valid = 0;
     }
     return(0);
}
int SRXAFSCB_ProbeUuid(rxcall, uuidp)
struct rx_call *rxcall;
afsUUID *uuidp;
{
    int code = 0;
    if (!afs_cb_inited) init_afs_cb();
    if (!afs_uuid_equal(uuidp, &afs_cb_interface.uuid))
        code = 1; /* failure */
#ifdef STRANGEDEBUG
    rx_EndCall(rxcall,code);
    IOMGR_Sleep(600);
#endif
    return code;
}
int SRXAFSCB_GetServerPrefs(
    struct rx_call *a_call,
    afs_int32 a_index,
    afs_int32 *a_srvr_addr,
    afs_int32 *a_srvr_rank)
{
    *a_srvr_addr = 0xffffffff;
    *a_srvr_rank = 0xffffffff;
    return 0;
}

int SRXAFSCB_GetCellServDB(
    struct rx_call *a_call,
    afs_int32 a_index,
    char **a_name,
    afs_int32 *a_hosts)
{
    return RXGEN_OPCODE;
}

int SRXAFSCB_GetLocalCell(
    struct rx_call *a_call,
    char **a_name)
{
    return RXGEN_OPCODE;
}

int SRXAFSCB_GetCacheConfig(
    struct rx_call *a_call,
    afs_uint32 callerVersion,
    afs_uint32 *serverVersion,
    afs_uint32 *configCount,
    cacheConfig *config)
{
    return RXGEN_OPCODE;
}
int SRXAFSCB_GetCellByNum(
    struct rx_call *a_call,
    afs_int32 a_index,
    char **a_name,
    afs_int32 *a_hosts)
{
    return RXGEN_OPCODE;
}
#ifdef AFS_64BIT_CLIENT
afs_int32
SRXAFSCB_TellMeAboutYourself(struct rx_call * rxcall,
                             struct interfaceAddr * addr,
                             Capabilities * capabilities)
{
    if ( rxcall && addr )
    {
        if (!afs_cb_inited) init_afs_cb();
        *addr = afs_cb_interface;
    }
    return(0);
}
#endif
