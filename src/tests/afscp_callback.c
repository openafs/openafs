#include <afs/param.h>
#include <afs/afscbint.h>               /*Callback interface defs*/
int afs_cb_inited = 0;
struct interfaceAddr afs_cb_interface;
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
afs_int32 SRXAFSCB_CallBack(rxcall, Fids_Array, CallBack_Array)
    struct rx_call *rxcall;
    AFSCBFids *Fids_Array;
    AFSCBs *CallBack_Array;

{ /*SRXAFSCB_CallBack*/
    return(0);

} /*SRXAFSCB_CallBack*/


afs_int32 SRXAFSCB_InitCallBackState(rxcall)
    struct rx_call *rxcall;

{ /*SRXAFSCB_InitCallBackState*/
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

{ /*SRXAFSCB_GetCE64*/
     return(0);
} /*SRXAFSCB_GetCE64*/


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
    if ( rxcall && addr )
    {
        if (!afs_cb_inited) init_afs_cb();
        *addr = afs_cb_interface;
    }
    return(0);
}

int SRXAFSCB_InitCallBackState3(rxcall, uuidp)
struct rx_call *rxcall;
afsUUID *uuidp;
{
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
    return code;
}

afs_int32 SRXAFSCB_GetServerPrefs(rxcall, serverIndex, srvrAddr, srvrRank)
struct rx_call *rxcall;
afs_int32 serverIndex;
afs_int32 *srvrAddr;
afs_int32 *srvrRank;
{
    return RXGEN_OPCODE;
}


afs_int32 SRXAFSCB_GetCellServDB(rxcall, cellIndex, cellName, cellHosts)
struct rx_call *rxcall;
afs_int32 cellIndex;
char *cellName;
afs_int32 *cellHosts;
{
    return RXGEN_OPCODE;
}


afs_int32 SRXAFSCB_GetLocalCell(rxcall, cellName)
struct rx_call *rxcall;
char *cellName;
{
    return RXGEN_OPCODE;
}


afs_int32 SRXAFSCB_GetCacheConfig(rxcall, callerVersion, serverVersion,
                                  configCount, config)
struct rx_call *rxcall;
afs_uint32 callerVersion;
afs_uint32 *serverVersion;
afs_uint32 *configCount;
cacheConfig *config;
{
    return RXGEN_OPCODE;
}
