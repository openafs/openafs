#include <afsconfig.h>
#include <afs/param.h>

#include <afs/afscbint.h>	/*Callback interface defs */
#include <afs/afsutil.h>
#include <afs/afsutil_prototypes.h>
int afs_cb_inited = 0;
struct interfaceAddr afs_cb_interface;
static int
init_afs_cb(void)
{
    int count;

    afs_uuid_create(&afs_cb_interface.uuid);
    count = rx_getAllAddr((afs_uint32 *)&afs_cb_interface.addr_in, AFS_MAX_INTERFACE_ADDR);
    if (count <= 0)
	afs_cb_interface.numberOfInterfaces = 0;
    else {
	int i;
	afs_cb_interface.numberOfInterfaces = count;
	for (i = 0; i < count; i++) {
	    /* these addresss will be marshalled in XDR, so they must be in
	     * host-byte order to make sense */
	    afs_cb_interface.addr_in[i] = ntohl(afs_cb_interface.addr_in[i]);
	}
    }
    afs_cb_inited = 1;
    return 0;
}

afs_int32
SRXAFSCB_CallBack(struct rx_call *rxcall, AFSCBFids *Fids_Array,
		  AFSCBs *CallBack_Array)
{				/*SRXAFSCB_CallBack */
    return (0);

}				/*SRXAFSCB_CallBack */


afs_int32
SRXAFSCB_InitCallBackState(struct rx_call *rxcall)
{				/*SRXAFSCB_InitCallBackState */
    return (0);

}				/*SRXAFSCB_InitCallBackState */

afs_int32
SRXAFSCB_Probe(struct rx_call *rxcall)
{				/*SRXAFSCB_Probe */
    return (0);

}				/*SRXAFSCB_Probe */

afs_int32
SRXAFSCB_GetCE(struct rx_call *rxcall, afs_int32 index, AFSDBCacheEntry * ce)
{				/*SRXAFSCB_GetCE */
    return (0);
}				/*SRXAFSCB_GetCE */


afs_int32
SRXAFSCB_GetCE64(struct rx_call *rxcall, afs_int32 index, AFSDBCacheEntry64 *ce)
{				/*SRXAFSCB_GetCE64 */
    return (0);
}				/*SRXAFSCB_GetCE64 */


afs_int32
SRXAFSCB_GetLock(struct rx_call *rxcall, afs_int32 index, AFSDBLock *lock)
{				/*SRXAFSCB_GetLock */
    return (0);

}				/*SRXAFSCB_GetLock */

afs_int32
SRXAFSCB_XStatsVersion(struct rx_call *rxcall, afs_int32 *v)
{				/*SRXAFSCB_XStatsVersion */
    return (0);

}				/*SRXAFSCB_XStatsVersion */

afs_int32
SRXAFSCB_GetXStats(struct rx_call *rxcall, afs_int32 clientVersionNumber,
		   afs_int32 collectionNumber, afs_int32 * srvVersionNumberP,
		   afs_int32 * timeP, AFSCB_CollData * dataP)
{				/*SRXAFSCB_GetXStats */
    return (0);
}				/*SRXAFSCB_GetXStats */

int
SRXAFSCB_InitCallBackState2(struct rx_call *rxcall, struct interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_WhoAreYou(struct rx_call *rxcall, struct interfaceAddr *addr)
{
    if (rxcall && addr) {
	if (!afs_cb_inited)
	    init_afs_cb();
	*addr = afs_cb_interface;
    }
    return (0);
}

int
SRXAFSCB_InitCallBackState3(struct rx_call *rxcall, afsUUID *uuidp)
{
    return (0);
}

int
SRXAFSCB_ProbeUuid(struct rx_call *rxcall, afsUUID *uuidp)
{
    int code = 0;
    if (!afs_cb_inited)
	init_afs_cb();
    if (!afs_uuid_equal(uuidp, &afs_cb_interface.uuid))
	code = 1;		/* failure */
    return code;
}

afs_int32
SRXAFSCB_GetServerPrefs(struct rx_call *rxcall, afs_int32 serverIndex,
			afs_int32 *srvrAddr, afs_int32 *srvrRank)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_GetCellServDB(struct rx_call *rxcall, afs_int32 cellIndex,
		       char **cellName, serverList *cellHosts)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_GetLocalCell(struct rx_call *rxcall, char **cellName)
{
    return RXGEN_OPCODE;
}


afs_int32
SRXAFSCB_GetCacheConfig(struct rx_call *rxcall, afs_uint32 callerVersion,
			afs_uint32 *serverVersion, afs_uint32 *configCount,
			cacheConfig *config)
{
    return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_GetCellByNum(struct rx_call *rxcall, afs_int32 cellnum,
		      char **cellname, serverList *cellhosts)
{
     return RXGEN_OPCODE;
}

afs_int32
SRXAFSCB_TellMeAboutYourself(struct rx_call *rxcall,
			     struct interfaceAddr *addr, Capabilities *cap)
{
     return RXGEN_OPCODE;
}
