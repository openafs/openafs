/* The rxkad security object.  This contains packet processing routines that
 * are prohibited from being exported. */

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/


#ifdef KERNEL

#include "../afs/param.h"
#include "../afs/stds.h"
#ifndef UKERNEL
#include "../h/types.h"
#include "../rx/rx.h"
#include "../netinet/in.h"
#else /* !UKERNEL */
#include "../afs/sysincludes.h"
#include "../rx/rx.h"
#endif /* !UKERNEL */

#else /* KERNEL */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#include <rx/rx.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#endif /* KERNEL */

#include "private_data.h"
#define XPRT_RXKAD_CRYPT
#ifdef KERNEL
#include "../afs/permit_xprt.h"
#else
#include "../permit_xprt.h"
#endif


AFS_HIDE
afs_int32 rxkad_DecryptPacket (conn, schedule, ivec, len, packet)
  IN struct rx_connection *conn;
  IN fc_KeySchedule *schedule;
  IN fc_InitializationVector *ivec;
  IN int len;
  INOUT struct rx_packet *packet;
{
    afs_uint32 xor[2];
    struct rx_securityClass *obj;
    struct rxkad_cprivate *tp;		/* s & c have type at same offset */
    char * data;
    int i,tlen;

    if (!xprt_CryptOK (conn)) return RXKADILLEGALLEVEL;
    obj = rx_SecurityObjectOf(conn);
    tp = (struct rxkad_cprivate *)obj->privateData;
    LOCK_RXKAD_STATS
    rxkad_stats.bytesDecrypted[rxkad_TypeIndex(tp->type)] += len;
    UNLOCK_RXKAD_STATS

    bcopy ((void *)ivec, (void *)xor, sizeof(xor));
    for (i = 0; len ; i++) {
      data = rx_data(packet, i, tlen);
      if (!data || !tlen)
	break;
      tlen = MIN(len, tlen);
      fc_cbc_encrypt (data, data, tlen, schedule, xor, DECRYPT);
      len -= tlen;
    }
    /* Do this if packet checksums are ever enabled (below), but
     * current version just passes zero
    afs_int32 cksum;
    cksum = ntohl(rx_GetInt32(packet, 1));
    */
    return 0;
}

AFS_HIDE
afs_int32 rxkad_EncryptPacket (conn, schedule, ivec, len, packet)
  IN struct rx_connection *conn;
  IN fc_KeySchedule *schedule;
  IN fc_InitializationVector *ivec;
  IN int len;
  INOUT struct rx_packet *packet;
{
    afs_uint32 xor[2];
    struct rx_securityClass *obj;
    struct rxkad_cprivate *tp;		/* s & c have type at same offset */
    char *data;
    int i,tlen;

    if (!xprt_CryptOK (conn)) return RXKADILLEGALLEVEL;
    obj = rx_SecurityObjectOf(conn);
    tp = (struct rxkad_cprivate *)obj->privateData;
    LOCK_RXKAD_STATS
    rxkad_stats.bytesEncrypted[rxkad_TypeIndex(tp->type)] += len;
    UNLOCK_RXKAD_STATS

    /*
    afs_int32 cksum;
    cksum = htonl(0);		
    * Future option to add cksum here, but for now we just put 0
    */
    rx_PutInt32(packet, 1*sizeof(afs_int32), 0); 

    bcopy ((void *)ivec, (void *)xor, sizeof(xor));
    for (i = 0; len ; i++) {
      data = rx_data(packet, i, tlen);
      if (!data || !tlen)
	break;
      tlen = MIN(len, tlen);
      fc_cbc_encrypt (data, data, tlen, schedule, xor, ENCRYPT);
      len -= tlen;
    }
    return 0;
}

