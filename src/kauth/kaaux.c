/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 1.5  89/03/14  13:19:57
 * Rename CBS and BBS to ka_* to avoid collisions with afsint.h.
 * 
 * Revision 1.4  88/12/09  14:35:57
 * Fixed a bug where BBS code didn't check error code which caused
 *   problems when Ubik retransmitted packets after previous call
 *   aborted and didn't return output parameters.
 * 
 * Revision 1.3  88/11/18  09:07:47
 * Conversion to Rx.
 * 
 * Revision 1.2  88/09/20  11:38:13
 * Added IBM Copyright
 * 
 * Revision 1.1  88/08/29  12:48:27
 * Initial revision
 *  */

#if defined(UKERNEL)
#include "../rx/xdr.h"
#include "../afsint/kauth.h"
#else /* defined(UKERNEL) */
#include <rx/xdr.h>
#include "kauth.h"
#endif /* defined(UKERNEL) */

#define MAXBS	2048		/* try to avoid horrible allocs */

int xdr_ka_CBS(
  XDR *x,
  struct ka_CBS *abbs)
{
    afs_int32 len;
    if (x->x_op == XDR_FREE) {
	free(abbs->SeqBody);
	return TRUE;
    }

    if (x->x_op == XDR_ENCODE) {
	xdr_afs_int32(x, &abbs->SeqLen);
	xdr_opaque(x, abbs->SeqBody, abbs->SeqLen);
	return TRUE;
    }
    else {
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS) return FALSE;
	if (!abbs->SeqBody) abbs->SeqBody = (char *) malloc(len);
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}

int xdr_ka_BBS(
  XDR *x,
  struct ka_BBS *abbs)
{
    afs_int32 maxLen, len;
    if (x->x_op == XDR_FREE) {
	free(abbs->SeqBody);
	return TRUE;
    }

    if (x->x_op == XDR_ENCODE) {
	if (!xdr_afs_int32(x, &abbs->MaxSeqLen) ||
	    !xdr_afs_int32(x, &abbs->SeqLen) ||
	    !xdr_opaque(x, abbs->SeqBody, abbs->SeqLen))
	    return FALSE;
	return TRUE;
    }
    else {
	if (!xdr_afs_int32(x, &maxLen) ||
	    !xdr_afs_int32(x, &len) ||
	    (len < 0) || (len > MAXBS) || (len > maxLen))
	    return FALSE;
	if (!abbs->SeqBody) abbs->SeqBody = (char *) malloc(maxLen);
	abbs->MaxSeqLen = maxLen;
	abbs->SeqLen = len;
	if (!xdr_opaque(x, abbs->SeqBody, len)) return FALSE;
	return TRUE;
    }
}

