/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/nfsclient.h"
#include "../afs/afs_osidnlc.h"


/*
 * UIO routines
 */

/* routine to make copy of uio structure in ainuio, using aoutvec for space */
afsio_copy(ainuio, aoutuio, aoutvec)
struct uio *ainuio, *aoutuio;
register struct iovec *aoutvec; {
    register int i;
    register struct iovec *tvec;

    AFS_STATCNT(afsio_copy);
    if (ainuio->afsio_iovcnt > AFS_MAXIOVCNT) return EINVAL;
    bcopy((char *)ainuio, (char *)aoutuio, sizeof(struct uio));
    tvec = ainuio->afsio_iov;
    aoutuio->afsio_iov = aoutvec;
    for(i=0;i<ainuio->afsio_iovcnt;i++){
	bcopy((char *)tvec, (char *)aoutvec, sizeof(struct iovec));
	tvec++;	    /* too many compiler bugs to do this as one expr */
	aoutvec++;
    }
    return 0;
}

/* trim the uio structure to the specified size */
afsio_trim(auio, asize)
register struct uio *auio;
register afs_int32 asize; {
    register int i;
    register struct iovec *tv;

    AFS_STATCNT(afsio_trim);
    auio->afsio_resid = asize;
    tv = auio->afsio_iov;
    /* It isn't clear that multiple iovecs work ok (hasn't been tested!) */
    for(i=0;;i++,tv++) {
	if (i >= auio->afsio_iovcnt || asize <= 0) {
	    /* we're done */
	    auio->afsio_iovcnt = i;
	    break;
	}
	if (tv->iov_len	<= asize)
	    /* entire iovec is included */
	    asize -= tv->iov_len;   /* this many fewer bytes */
	else {
	    /* this is the last one */
	    tv->iov_len = asize;
	    auio->afsio_iovcnt = i+1;
	    break;
	}
    }
    return 0;
}

/* skip asize bytes in the current uio structure */
afsio_skip(auio, asize)
register struct uio *auio;
register afs_int32 asize; {
    register struct iovec *tv;	/* pointer to current iovec */
    register int cnt;

    AFS_STATCNT(afsio_skip);
   /* It isn't guaranteed that multiple iovecs work ok (hasn't been tested!) */
    while (asize > 0 && auio->afsio_resid) {
	tv = auio->afsio_iov;
	cnt = tv->iov_len;
	if (cnt == 0) {
	    auio->afsio_iov++;
	    auio->afsio_iovcnt--;
	    continue;
	}
	if (cnt > asize)
	    cnt = asize;
	tv->iov_base = (char *)(tv->iov_base) + cnt;
	tv->iov_len -= cnt;
	auio->uio_resid -= cnt;
	auio->uio_offset += cnt;
	asize -= cnt;
    }
    return 0;
}

