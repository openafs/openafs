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

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#else
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/param.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <ubik.h>
#include <afs/bubasics.h>
#include "budb_errs.h"
#include "database.h"
#include "error_macros.h"
#include "afs/audit.h"
#include <afs/afsutil.h>

/* --------------------------------
 * interface & support code to manage text blocks within the
 * 	database
 * --------------------------------
 */

/* BUDB_GetText
 * notes: 
 *	routine mallocs storage for charListPtr, freed by stub
 */

afs_int32 GetText(), GetTextVersion(), SaveText();

afs_int32
SBUDB_GetText(call, lockHandle, textType, maxLength, offset, nextOffset,
	      charListPtr)
     struct rx_call *call;
     afs_uint32 lockHandle;
     afs_int32 textType;
     afs_int32 maxLength;
     afs_int32 offset;
     afs_int32 *nextOffset;
     charListT *charListPtr;
{
    afs_int32 code;

    code =
	GetText(call, lockHandle, textType, maxLength, offset, nextOffset,
		charListPtr);
    osi_auditU(call, BUDB_GetTxtEvent, code, AUD_LONG, textType, AUD_END);
    return code;
}

afs_int32
GetText(call, lockHandle, textType, maxLength, offset, nextOffset,
	charListPtr)
     struct rx_call *call;
     afs_uint32 lockHandle;
     afs_int32 textType;
     afs_int32 maxLength;
     afs_int32 offset;
     afs_int32 *nextOffset;
     charListT *charListPtr;
{
    struct ubik_trans *ut = 0;
    struct block block;
    afs_int32 transferSize, chunkSize;
    afs_int32 blockOffset;
    dbadr lastBlockAddr;
    afs_int32 nblocks;
    struct textBlock *tbPtr;
    afs_int32 textRemaining;
    char *textPtr;
    afs_int32 code;

    LogDebug(5, "GetText: type %d, offset %d, nextOffset %d, maxLength %d\n",
	     textType, offset, nextOffset, maxLength);

    if (callPermitted(call) == 0) {
	code = BUDB_NOTPERMITTED;
	goto no_xfer_abort;
    }

    /* check parameters */
    if ((offset < 0)
	|| (textType < 0)
	|| (textType >= TB_NUM)
	) {
	code = BUDB_BADARGUMENT;
	goto no_xfer_abort;
    }

    /* start the transaction */
    code = InitRPC(&ut, LOCKWRITE, 1);
    if (code)
	goto no_xfer_abort;

    /* fetch the lock state */
    if (checkLockHandle(ut, lockHandle) == 0) {
	code = BUDB_NOTLOCKED;
	goto no_xfer_abort;
    }

    tbPtr = &db.h.textBlock[textType];

    if ((ntohl(tbPtr->size) > 0)
	&& (offset >= ntohl(tbPtr->size))
	) {
	code = BUDB_BADARGUMENT;
	goto no_xfer_abort;
    }

    LogDebug(5, "GetText: store size is %d\n", ntohl(tbPtr->size));

    /* compute minimum of remaining text or user buffer */
    textRemaining = ntohl(tbPtr->size) - offset;
    transferSize = MIN(textRemaining, maxLength);

    /* allocate the transfer storage */
    if (transferSize <= 0) {
	charListPtr->charListT_len = 0L;
	charListPtr->charListT_val = NULL;
    } else {
	charListPtr->charListT_len = transferSize;
	charListPtr->charListT_val = (char *)malloc(transferSize);
	if (charListPtr->charListT_val == 0)
	    ABORT(BUDB_NOMEM);
    }

    textPtr = charListPtr->charListT_val;
    *nextOffset = offset + transferSize;

    /* setup the datablock. read and discard all blocks up to the one the
     * offset specifies
     */
    nblocks = offset / BLOCK_DATA_SIZE;
    lastBlockAddr = ntohl(tbPtr->textAddr);

    while (nblocks--) {
	code = dbread(ut, lastBlockAddr, (char *)&block, sizeof(block));
	if (code)
	    ABORT(BUDB_IO);
	lastBlockAddr = ntohl(block.h.next);
    }

    while (transferSize > 0) {
	code = dbread(ut, lastBlockAddr, (char *)&block, sizeof(block));
	if (code)
	    ABORT(BUDB_IO);

	LogDebug(5, "fetched block %d\n", lastBlockAddr);

	/* compute the data size to extract */
	blockOffset = offset % BLOCK_DATA_SIZE;
	textRemaining = BLOCK_DATA_SIZE - blockOffset;
	chunkSize = min(textRemaining, transferSize);

	memcpy(textPtr, &block.a[blockOffset], chunkSize);

	/* LogDebug(5, "transfering %d bytes: %s\n", chunkSize, textPtr); */

	transferSize -= chunkSize;
	offset += chunkSize;
	textPtr += chunkSize;

	if (transferSize) {
	    /* setup lastBlockAddr */
	    lastBlockAddr = ntohl(block.h.next);
	}
    }

    if (*nextOffset == ntohl(tbPtr->size)) {
	/* all done */
	*nextOffset = -1;
    }

  error_exit:
    code = ubik_EndTrans(ut);
/*  printf("in error exit, code=%ld\n", code); */
    return (code);

  no_xfer_abort:
    charListPtr->charListT_len = 0;
    charListPtr->charListT_val = (char *)malloc(0);

  abort_exit:
    if (ut)
	ubik_AbortTrans(ut);
/*  printf("in abort exit, code=%ld\n", code); */
    return (code);
}

freeOldBlockChain(ut, diskAddr)
     struct ubik_trans *ut;
     dbadr diskAddr;
{
    struct blockHeader blockHeader;
    dbadr nextDiskAddr;
    afs_int32 code;

    while (diskAddr != 0) {
	/* read in the header */
	code =
	    dbread(ut, diskAddr, (char *)&blockHeader, sizeof(blockHeader));
	if (code)
	    ABORT(code);
	nextDiskAddr = ntohl(blockHeader.next);
	code = FreeBlock(ut, &blockHeader, diskAddr);
	if (code)
	    ABORT(code);
	diskAddr = nextDiskAddr;
    }
  abort_exit:
    return (code);
}

/* BUDB_GetTextVersion
 *	get the version number for the specified text block
 */

afs_int32
SBUDB_GetTextVersion(call, textType, tversion)
     struct rx_call *call;
     afs_int32 textType;
     afs_uint32 *tversion;
{
    afs_int32 code;

    code = GetTextVersion(call, textType, tversion);
    osi_auditU(call, BUDB_GetTxVEvent, code, AUD_LONG, textType, AUD_END);
    return code;
}

afs_int32
GetTextVersion(call, textType, tversion)
     struct rx_call *call;
     afs_int32 textType;
     afs_uint32 *tversion;
{
    afs_int32 code;
    struct ubik_trans *ut;

    if (callPermitted(call) == 0)
	return (BUDB_NOTPERMITTED);

    if ((textType < 0) || (textType >= TB_NUM))
	return (BUDB_BADARGUMENT);

    code = InitRPC(&ut, LOCKREAD, 1);
    if (code)
	return (code);

    *tversion = ntohl(db.h.textBlock[textType].version);
    code = ubik_EndTrans(ut);
    return (code);
}

/* text blocks
 *	next - next disk addr
 * host/network ordering????
 */

/* BUDB_SaveText
 * notes:
 *	charListPtr storage automatically malloced and freed
 */

afs_int32
SBUDB_SaveText(call, lockHandle, textType, offset, flags, charListPtr)
     struct rx_call *call;
     afs_uint32 lockHandle;
     afs_int32 textType;
     afs_int32 offset;
     afs_int32 flags;
     charListT *charListPtr;
{
    afs_int32 code;

    code = SaveText(call, lockHandle, textType, offset, flags, charListPtr);
    osi_auditU(call, BUDB_SavTxtEvent, code, AUD_LONG, textType, AUD_END);
    return code;
}

afs_int32
SaveText(call, lockHandle, textType, offset, flags, charListPtr)
     struct rx_call *call;
     afs_uint32 lockHandle;
     afs_int32 textType;
     afs_int32 offset;
     afs_int32 flags;
     charListT *charListPtr;
{
    struct ubik_trans *ut;
    struct block diskBlock;
    dbadr diskBlockAddr;
    afs_int32 remainingInBlock, chunkSize;
    struct textBlock *tbPtr;
    afs_int32 textLength = charListPtr->charListT_len;
    char *textptr = charListPtr->charListT_val;
    afs_int32 code;

    LogDebug(5, "SaveText: type %d, offset %d, length %d\n", textType, offset,
	     textLength);

    if (callPermitted(call) == 0)
	return (BUDB_NOTPERMITTED);

    if ((textLength > BLOCK_DATA_SIZE) || (offset < 0))
	return (BUDB_BADARGUMENT);

    code = InitRPC(&ut, LOCKWRITE, 1);
    if (code)
	return (code);

    /* fetch the lock state */
    if (checkLockHandle(ut, lockHandle) == 0)
	ABORT(BUDB_NOTLOCKED);

    if ((textType < 0) || (textType >= TB_NUM))
	ABORT(BUDB_BADARGUMENT);

    tbPtr = &db.h.textBlock[textType];

    LogDebug(5,
	     "SaveText: lockHandle %d textType %d offset %d flags %d txtlength %d\n",
	     lockHandle, textType, offset, flags, textLength);

    if (offset == 0) {
	/* release any blocks from previous transactions */
	diskBlockAddr = ntohl(tbPtr->newTextAddr);
	freeOldBlockChain(ut, diskBlockAddr);

	if (textLength) {
	    code = AllocBlock(ut, &diskBlock, &diskBlockAddr);
	    if (code)
		ABORT(code);

	    LogDebug(5, "allocated block %d\n", diskBlockAddr);

	    /* set block type */
	    diskBlock.h.type = text_BLOCK;

	    /* save it in the database header */
	    tbPtr->newsize = 0;
	    tbPtr->newTextAddr = htonl(diskBlockAddr);
	    dbwrite(ut, (char *)tbPtr - (char *)&db.h, (char *)tbPtr,
		    sizeof(struct textBlock));
	} else {
	    tbPtr->newsize = 0;
	    tbPtr->newTextAddr = 0;
	}
    } else {
	/* non-zero offset */
	int nblocks;

	if (offset != ntohl(tbPtr->newsize))
	    ABORT(BUDB_BADARGUMENT);

	/* locate the block to which offset refers */
	nblocks = offset / BLOCK_DATA_SIZE;

	diskBlockAddr = ntohl(tbPtr->newTextAddr);
	if (diskBlockAddr == 0)
	    ABORT(BUDB_BADARGUMENT);

	code =
	    dbread(ut, diskBlockAddr, (char *)&diskBlock, sizeof(diskBlock));
	if (code)
	    ABORT(code);

	while (nblocks--) {
	    diskBlockAddr = ntohl(diskBlock.h.next);
	    code =
		dbread(ut, diskBlockAddr, (char *)&diskBlock,
		       sizeof(diskBlock));
	    if (code)
		ABORT(code);
	}
    }

    /* diskBlock and diskBlockAddr now point to the last block in the chain */

    while (textLength) {
	/* compute the transfer size */
	remainingInBlock = (BLOCK_DATA_SIZE - (offset % BLOCK_DATA_SIZE));
	chunkSize = MIN(remainingInBlock, textLength);

	/* copy in the data */
	memcpy(&diskBlock.a[offset % BLOCK_DATA_SIZE], textptr, chunkSize);

	/* LogDebug(5, "text is %s\n", textptr); */

	textLength -= chunkSize;
	textptr += chunkSize;
	offset += chunkSize;
	tbPtr->newsize = htonl(ntohl(tbPtr->newsize) + chunkSize);

	if (textLength > 0) {
	    afs_int32 prevBlockAddr;
	    afs_int32 linkOffset;
	    afs_int32 linkValue;

	    /* have to add another block to the chain */

	    code =
		dbwrite(ut, diskBlockAddr, (char *)&diskBlock,
			sizeof(diskBlock));
	    if (code)
		ABORT(code);

	    prevBlockAddr = (afs_int32) diskBlockAddr;
	    code = AllocBlock(ut, &diskBlock, &diskBlockAddr);
	    if (code)
		ABORT(code);

	    LogDebug(5, "allocated block %d\n", diskBlockAddr);

	    /* set block type */
	    diskBlock.h.type = text_BLOCK;

	    /* now have to update the previous block's link */
	    linkOffset =
		(afs_int32) ((char*)& diskBlock.h.next - (char*)& diskBlock);
	    linkValue = htonl(diskBlockAddr);

	    code =
		dbwrite(ut, (afs_int32) prevBlockAddr + linkOffset,
			(char *)&linkValue, sizeof(afs_int32));
	    if (code)
		ABORT(code);
	} else {
	    /* just write the old block */
	    code =
		dbwrite(ut, diskBlockAddr, (char *)&diskBlock,
			sizeof(diskBlock));
	    if (code)
		ABORT(code);
	}
    }

    if (flags & BUDB_TEXT_COMPLETE) {	/* done */
	/* this was the last chunk of text */
	diskBlockAddr = ntohl(tbPtr->textAddr);
	freeOldBlockChain(ut, diskBlockAddr);

	tbPtr->textAddr = tbPtr->newTextAddr;
	tbPtr->newTextAddr = 0;
	tbPtr->size = tbPtr->newsize;
	tbPtr->newsize = 0;
	tbPtr->version = htonl(ntohl(tbPtr->version) + 1);

	/* saveTextToFile(ut, tbPtr); */
    }

    /* update size and other text header info */
    code =
	dbwrite(ut, (char *)tbPtr - (char *)&db.h, (char *)tbPtr,
		sizeof(struct textBlock));
    if (code)
	ABORT(code);

  error_exit:
    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}

/* debug support */

saveTextToFile(ut, tbPtr)
     struct ubik_trans *ut;
     struct textBlock *tbPtr;

{
    afs_int32 blockAddr;
    struct block block;
    char filename[128];
    afs_int32 size, chunkSize;
    int fid;

    sprintf(filename, "%s/%s", gettmpdir(), "dbg_XXXXXX");

    fid = mkstemp(filename);
    size = ntohl(tbPtr->size);
    blockAddr = ntohl(tbPtr->textAddr);
    while (size) {
	chunkSize = MIN(BLOCK_DATA_SIZE, size);
	dbread(ut, blockAddr, (char *)&block, sizeof(block));
	write(fid, &block.a[0], chunkSize);
	blockAddr = ntohl(block.h.next);
	size -= chunkSize;
    }
    close(fid);
    printf("wrote debug file %s\n", filename);
}


#if (defined(AFS_HPUX_ENV)) || defined(AFS_NT40_ENV)

/* mkstemp
 * entry:
 *	st - string containing template for a tmp file name
 * exit:
 *	-1 - failed
 *	0-n - open file descriptor
 * notes:
 *	1) missing in Ultrix, HP/UX and AIX 221 environment
 *      2) iterate some number of times to alleviate the race?
 */

int
mkstemp(st)
     char *st;
{
    int retval = -1;

#ifdef AFS_LINUX20_ENV
    retval = open(mkstemp(st), O_RDWR | O_CREAT | O_EXCL, 0600);
#else
    retval = open(mktemp(st), O_RDWR | O_CREAT | O_EXCL, 0600);
#endif

    return (retval);
}
#endif
