/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <rx/xdr.h>
#include <afs/kauth.h>

#define MAXBS	2048		/* try to avoid horrible allocs */

int
xdr_ka_CBS(XDR * x, struct ka_CBS *abbs)
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
    } else {
	xdr_afs_int32(x, &len);
	if (len < 0 || len > MAXBS)
	    return FALSE;
	if (abbs->SeqBody != NULL) {
	    if (len > abbs->SeqLen) {
		/*
		 * We've been given a preallocated buffer to decode into, but
		 * we're decoding 'len' bytes, which is larger than the
		 * provided buffer (only abbs->SeqLen bytes large). This won't
		 * work.
		 */
		return FALSE;
	    }
	} else
	    abbs->SeqBody = (char *)malloc(len);
	abbs->SeqLen = len;
	xdr_opaque(x, abbs->SeqBody, len);
	return TRUE;
    }
}

int
xdr_ka_BBS(XDR * x, struct ka_BBS *abbs)
{
    afs_int32 maxLen, len;
    if (x->x_op == XDR_FREE) {
	free(abbs->SeqBody);
	return TRUE;
    }

    if (x->x_op == XDR_ENCODE) {
	if (!xdr_afs_int32(x, &abbs->MaxSeqLen)
	    || !xdr_afs_int32(x, &abbs->SeqLen)
	    || !xdr_opaque(x, abbs->SeqBody, abbs->SeqLen))
	    return FALSE;
	return TRUE;
    } else {
	if (!xdr_afs_int32(x, &maxLen) || !xdr_afs_int32(x, &len) || (len < 0)
	    || (len > MAXBS) || (len > maxLen))
	    return FALSE;
	if (abbs->SeqBody != NULL) {
	    if (len > abbs->MaxSeqLen) {
		/*
		 * We've been given a preallocated buffer to decode into, but
		 * we're decoding 'len' bytes, which is larger than the
		 * provided buffer (only abbs->MaxSeqLen bytes large). This
		 * won't work.
		 */
		return FALSE;
	    }
	    if (maxLen > abbs->MaxSeqLen) {
		/*
		 * Our preallocated buffer only has space for MaxSeqLen bytes.
		 * Don't let the peer change 'abbs' so that it looks like we
		 * have space for more bytes than that; that could cause us to
		 * access memory beyond what we've actually allocated.
		 */
		return FALSE;
	    }
	} else
	    abbs->SeqBody = (char *)malloc(maxLen);
	abbs->MaxSeqLen = maxLen;
	abbs->SeqLen = len;
	if (!xdr_opaque(x, abbs->SeqBody, len))
	    return FALSE;
	return TRUE;
    }
}
