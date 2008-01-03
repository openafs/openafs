/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * demand attach fs
 * fileserver state serialization
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <stdlib.h>		/* for malloc() */
#include <time.h>		/* ANSI standard location for time stuff */
#include <string.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <io.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#endif
#include <afs/assert.h>
#include <sys/stat.h>

#include <afs/stds.h>

#include <rx/xdr.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afsint.h>
#include <afs/rxgen_consts.h>
#include <afs/nfs.h>
#include <afs/errors.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/acl.h>
#include <afs/ptclient.h>
#include <afs/prs_fs.h>
#include <afs/afsutil.h>
#include <rx/rx.h>
#include <afs/cellconfig.h>
#include <stdlib.h>

#include "../viced/viced_prototypes.h"
#include "../viced/viced.h"
#include "../viced/host.h"
#include "../viced/callback.h"
#include "serialize_state.h"

/*@+fcnmacros +macrofcndecl@*/
#ifdef O_LARGEFILE
#ifdef S_SPLINT_S
extern off64_t afs_lseek(int FD, off64_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)	lseek64(FD, (off64_t)(O), F)
#define afs_stat		stat64
#define afs_fstat		fstat64
#define afs_open		open64
#define afs_fopen		fopen64
#define afs_ftruncate           ftruncate64
#define afs_mmap                mmap64
#ifdef AFS_AIX_ENV
extern void * mmap64();  /* ugly hack since aix build env appears to be somewhat broken */
#endif
#else /* !O_LARGEFILE */
#ifdef S_SPLINT_S
extern off_t afs_lseek(int FD, off_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)	lseek(FD, (off_t)(O), F)
#define afs_stat		stat
#define afs_fstat		fstat
#define afs_open		open
#define afs_fopen		fopen
#define afs_ftruncate           ftruncate
#define afs_mmap                mmap
#endif /* !O_LARGEFILE */
/*@=fcnmacros =macrofcndecl@*/


#ifdef AFS_DEMAND_ATTACH_FS

/*
 * demand attach fs
 * state dump routines
 *
 * in order to make state dump/restore as fast as possible,
 * we use memory mapped files
 *
 * if this causes problems on certain platforms, the APIs
 * have been written so that it will be very simple to go
 * back to standard I/O for just those poorly written platforms
 */
#ifndef _WIN32
#define FS_STATE_USE_MMAP
#endif

#ifdef FS_STATE_USE_MMAP
#define FS_STATE_INIT_FILESIZE (8 * 1024 * 1024)  /* truncate to 8MB initially */
#include <sys/mman.h>
#endif

static int fs_stateCreateDump(struct fs_dump_state * state);
static int fs_stateLoadDump(struct fs_dump_state * state);
static int fs_stateInvalidateDump(struct fs_dump_state * state);
static int fs_stateCommitDump(struct fs_dump_state * state);
static int fs_stateCloseDump(struct fs_dump_state * state);

#ifdef FS_STATE_USE_MMAP
static int fs_stateSizeFile(struct fs_dump_state * state);
static int fs_stateResizeFile(struct fs_dump_state * state, size_t min_add);
static int fs_stateTruncateFile(struct fs_dump_state * state);

static int fs_stateMapFile(struct fs_dump_state * state);
static int fs_stateUnmapFile(struct fs_dump_state * state);

static int fs_stateIncCursor(struct fs_dump_state * state, size_t len);
static int fs_stateCheckIOSafety(struct fs_dump_state * state,
				 size_t len);
#endif

static int fs_stateFillHeader(struct fs_state_header * hdr);
static int fs_stateCheckHeader(struct fs_state_header * hdr);

static int fs_stateAlloc(struct fs_dump_state * state);
static int fs_stateFree(struct fs_dump_state * state);

extern afsUUID FS_HostUUID;
extern char cml_version_number[];

/*
 * demand attach fs
 * save all fileserver state 
 */
int
fs_stateSave(void)
{
    int ret = 0, verified = 1;
    struct fs_dump_state state;

    /* save and restore need to be atomic wrt other host package operations */
    H_LOCK; 

    ViceLog(0, ("fs_stateSave: commencing fileserver state dump\n"));

    if (fs_stateAlloc(&state)) {
	ViceLog(0, ("fs_stateSave: memory allocation failed; dump aborted\n"));
	ret = 1;
	goto done;
    }

    /* XXX
     * on busy servers, these checks will inevitably fail since stuff drops H_LOCK
     * all over the place (with structs left in inconsistent states) while RPCs to
     * clients happen (grumble, grumble, the host package needs to be rewritten...)
     *
     * the current hack is to force the background threads that deal with host and
     * callback state offline early in the shutdown process, do VShutdown, come
     * back and wait for those threads to die, THEN do the state dump
     *
     * BUT, this still has one flaw -- what do we do about rx worker threads that
     * are blocked in the host package making an RPC call to a cm???
     *
     * perhaps we need a refcounter that keeps track of threads blocked in rpc calls
     * with H_LOCK dropped (and the host struct likely left in an inconsistent state)
     *
     * or better yet, we need to associate a state machine with each host object
     * (kind of like demand attach Volume structures).
     *
     * sigh. I suspect we'll need to revisit this issue
     */

    if (fs_state.options.fs_state_verify_before_save) {
	ViceLog(0, ("fs_stateSave: performing internal consistency checks before proceeding with state dump\n"));

	if (h_stateVerify(&state)) {
	    ViceLog(0, ("fs_stateSave: error: host table consistency checks failed; state dump will not be marked clean\n"));
	    verified = 0;
	    ret = 1;
	}

	if (cb_stateVerify(&state)) {
	    ViceLog(0, ("fs_stateSave: error: callback table consistency checks failed; state dump will not be marked clean\n"));
	    verified = 0;
	    ret = 1;
	}

	/* if a consistency check asserted the bail flag, reset it */
	state.bail = 0;

	ViceLog(0, ("fs_stateSave: proceeding with dump\n"));
    }

    if (fs_stateCreateDump(&state)) {
	ViceLog(0, ("fs_stateSave: error: dump create failed\n"));
	ret = 1;
	goto done;
    }

    if (h_stateSave(&state)) {
	ViceLog(0, ("fs_stateSave: error: host state dump failed\n"));
	ret = 1;
	goto done;
    }

    if (cb_stateSave(&state)) {
	ViceLog(0, ("fs_stateSave: error: callback state dump failed\n"));
	ret = 1;
	goto done;
    }

    if (!verified) {
	state.bail = 1;
    }

    if (fs_stateCommitDump(&state)) {
	ViceLog(0, ("fs_stateSave: error: dump commit failed\n"));
	ret = 1; 
	goto done;
    }

    if (verified) {
	ViceLog(0, ("fs_stateSave: fileserver state dump completed successfully\n"));
    } else {
	ViceLog(0, ("fs_stateSave: fileserver state dump completed, but not marked clean.\n"));
	ViceLog(0, ("fs_stateSave: please save a copy of '%s' for use by technical support\n",
		    state.fn));
    }

 done:
    if (state.fd >= 0)
	fs_stateCloseDump(&state);
    fs_stateFree(&state);
    H_UNLOCK;
    return ret;
}

/*
 * demand attach fs
 * restore all fileserver state
 *
 * this function must appear as one atomic operation to the host and callback
 * packages, hence H_LOCK is held for the entirety of the process.
 */
int
fs_stateRestore(void)
{
    int ret = 0;
    struct fs_dump_state state;

    /* save and restore need to be atomic wrt other host package operations */
    H_LOCK;

    ViceLog(0, ("fs_stateRestore: commencing fileserver state restore\n"));

    if (fs_stateAlloc(&state)) {
	ViceLog(0, ("fs_stateRestore: memory allocation failed\n"));
	ret = 1;
	goto done;
    }

    if (fs_stateLoadDump(&state)) {
	ViceLog(0, ("fs_stateRestore: failed to load dump file '%s'\n", state.fn));
	ret = 1;
	goto done;
    }

    if (fs_stateInvalidateDump(&state)) {
	ViceLog(0, ("fs_stateRestore: failed to invalidate dump file '%s'\n", state.fn));
	ret = 1;
	goto done;
    }


    if (state.flags.do_host_restore) {
	if (h_stateRestore(&state)) {
	    ViceLog(0, ("fs_stateRestore: error: host state restore failed. exiting avoid further corruption\n"));
	    exit(0);
	}
	ViceLog(0, ("fs_stateRestore: host table restored\n"));

	if (cb_stateRestore(&state)) {
	    ViceLog(0, ("fs_stateRestore: error: callback state restore failed. exiting to avoid further corruption\n"));
	    exit(0);
	}
	ViceLog(0, ("fs_stateRestore: FileEntry and CallBack tables restored\n"));

	if (h_stateRestoreIndices(&state)) {
	    ViceLog(0, ("fs_stateRestore: error: host index remapping failed. exiting to avoid further corruption\n"));
	    exit(0);
	}
	ViceLog(0, ("fs_stateRestore: host table indices remapped\n"));

	if (cb_stateRestoreIndices(&state)) {
	    ViceLog(0, ("fs_stateRestore: error: callback index remapping failed. exiting to avoid further corruption\n"));
	    exit(0);
	}
	ViceLog(0, ("fs_stateRestore: FileEntry and CallBack indices remapped\n"));
    }

    ViceLog(0, ("fs_stateRestore: restore phase complete\n"));

    if (fs_state.options.fs_state_verify_after_restore) {
	ViceLog(0, ("fs_stateRestore: beginning state verification phase\n"));

	if (state.flags.do_host_restore) {
	    if (h_stateVerify(&state)) {
		ViceLog(0, ("fs_stateRestore: error: host table consistency checks failed; exiting to avoid further corruption\n"));
		exit(0);
	    }

	    if (cb_stateVerify(&state)) {
		ViceLog(0, ("fs_stateRestore: error: callback table consistency checks failed; exiting to avoid further corruption\n"));
		exit(0);
	    }
	}

	ViceLog(0, ("fs_stateRestore: fileserver state verification complete\n"));
    }

    ViceLog(0, ("fs_stateRestore: restore was successful\n"));

 done:
    if (state.fd >= 0) {
	fs_stateInvalidateDump(&state);
	fs_stateCloseDump(&state);
    }
    fs_stateFree(&state);
    H_UNLOCK;
    return ret;
}

static int
fs_stateCreateDump(struct fs_dump_state * state)
{
    int fd, ret = 0;
    char savedump[MAXPATHLEN];
    struct afs_stat status;

    afs_snprintf(savedump, sizeof(savedump), "%s.old", state->fn);

    if (afs_stat(state->fn, &status) == 0) {
	renamefile(state->fn, savedump);
    }

    if (((fd = afs_open(state->fn, 
			O_RDWR | O_CREAT | O_TRUNC, 
			S_IRUSR | S_IWUSR)) == -1) ||
	(afs_fstat(fd, &status) == -1)) {
	ViceLog(0, ("fs_stateCreateDump: failed to create state dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    state->fd = fd;
    state->mode = FS_STATE_DUMP_MODE;
    memset(state->hdr, 0, sizeof(struct fs_state_header));
    fs_stateIncEOF(state, sizeof(struct fs_state_header));

#ifdef FS_STATE_USE_MMAP
    if (fs_stateSizeFile(state)) {
	ViceLog(0, ("fs_stateCreateDump: failed to resize state dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    if (fs_stateMapFile(state)) {
	ViceLog(0, ("fs_stateCreateDump: failed to memory map state dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }
#endif

    ret = fs_stateInvalidateDump(state);

 done:
    return ret;
}

static int
fs_stateInvalidateDump(struct fs_dump_state * state)
{
    afs_uint64 z;
    int ret = 0;
    struct fs_state_header hdr;

#ifdef FS_STATE_USE_MMAP
    if (state->mmap.map == NULL) {
	return 1;
    }
#endif

    memcpy(&hdr, state->hdr, sizeof(hdr));
    hdr.valid = 0;
    ZeroInt64(z);

    /* write a bogus header to flag dump in progress */
    if (fs_stateWriteHeader(state, &z, &hdr, sizeof(hdr))) {
	ViceLog(0, ("fs_stateInvalidateDump: failed to invalidate old dump file header '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }
    if (fs_stateSync(state)) {
	ViceLog(0, ("fs_stateInvalidateDump: failed to sync changes to disk\n"));
	ret = 1;
	goto done;
    }

 done:
    return ret;
}

static int
fs_stateCommitDump(struct fs_dump_state * state)
{
    afs_uint64 z;
    int ret = 0;

    ZeroInt64(z);

#ifdef FS_STATE_USE_MMAP
    if (fs_stateTruncateFile(state)) {
	ViceLog(0, ("fs_stateCommitDump: failed to truncate dump file to proper size\n"));
	ret = 1;
	goto done;
    }
#endif

    /* ensure that all pending data I/Os for the state file have been committed 
     * _before_ we make the metadata I/Os */
    if (fs_stateSync(state)) {
	ViceLog(0, ("fs_stateCommitDump: failed to sync changes to disk\n"));
	ret = 1;
	goto done;
    }

#ifdef FS_STATE_USE_MMAP
    /* XXX madvise may not exist on all platforms, so
     * we may need to add some ifdefs at some point... */
    {
	madvise((((char *)state->mmap.map) + sizeof(struct fs_state_header)), 
		state->mmap.size - sizeof(struct fs_state_header), 
		MADV_DONTNEED);
    }
#endif

    /* build the header, and write it to disk */
    fs_stateFillHeader(state->hdr);
    if (state->bail) {
	state->hdr->valid = 0;
    }
    if (fs_stateWriteHeader(state, &z, state->hdr, sizeof(struct fs_state_header))) {
	ViceLog(0, ("fs_stateCommitDump: failed to write header to dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }
    if (fs_stateSync(state)) {
	ViceLog(0, ("fs_stateCommitDump: failed to sync new header to disk\n"));
	ret = 1;
	goto done;
    }

 done:
    return ret;
}

static int
fs_stateLoadDump(struct fs_dump_state * state)
{
    afs_uint64 z;
    int fd, ret = 0;
    struct afs_stat status;
    afs_int32 now = FT_ApproxTime();

    ZeroInt64(z);

    if ((fd = afs_open(state->fn, O_RDWR)) == -1 ||
	(afs_fstat(fd, &status) == -1)) {
	ViceLog(0, ("fs_stateLoadDump: failed to load state dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }
    state->fd = fd;
    state->mode = FS_STATE_LOAD_MODE;
    state->file_len = status.st_size;

#ifdef FS_STATE_USE_MMAP
    if (fs_stateMapFile(state)) {
	ViceLog(0, ("fs_stateLoadDump: failed to memory map state dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }
#endif

    if (fs_stateReadHeader(state, &z, state->hdr, sizeof(struct fs_state_header))) {
	ViceLog(0, ("fs_stateLoadDump: failed to read header from dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    /* check the validity of the header */
    if (fs_stateCheckHeader(state->hdr)) {
	ViceLog(1, ("fs_stateLoadDump: header failed validity checks; not restoring '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    if ((state->hdr->timestamp + HOST_STATE_VALID_WINDOW) >= now) {
	state->flags.do_host_restore = 1;
    } else {
	ViceLog(0, ("fs_stateLoadDump: warning: dump is too old for host and callback restore; skipping those steps\n"));
    }

 done:
    return ret;
}

static int
fs_stateCloseDump(struct fs_dump_state * state)
{
#ifdef FS_STATE_USE_MMAP
    fs_stateUnmapFile(state);
#endif
    close(state->fd);
    return 0;
}

int
fs_stateWrite(struct fs_dump_state * state,
	      void * buf, size_t len)
{
    int ret = 0;

#ifdef FS_STATE_USE_MMAP
    if (fs_stateCheckIOSafety(state, len)) {
	if (fs_stateResizeFile(state, len)) {
	    ViceLog(0, ("fs_stateWrite: could not resize dump file '%s'\n",
			state->fn));
	    ret = 1;
	    goto done;
	}
    }
	    
    memcpy(state->mmap.cursor, buf, len);
    fs_stateIncCursor(state, len);
#else
    if (write(state->fd, buf, len) != len) {
	ViceLog(0, ("fs_stateWrite: write failed\n"));
	ret = 1;
	goto done;
    }
#endif

 done:
    return ret;
}

int
fs_stateRead(struct fs_dump_state * state,
	     void * buf, size_t len)
{
    int ret = 0;

#ifdef FS_STATE_USE_MMAP
    if (fs_stateCheckIOSafety(state, len)) {
	ViceLog(0, ("fs_stateRead: read beyond EOF for dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    memcpy(buf, state->mmap.cursor, len);
    fs_stateIncCursor(state, len);
#else
    if (read(state->fd, buf, len) != len) {
	ViceLog(0, ("fs_stateRead: read failed\n"));
	ret = 1;
	goto done;
    }
#endif

 done:
    return ret;
}

int
fs_stateWriteV(struct fs_dump_state * state,
	       struct iovec * iov, int niov)
{
    int i, ret = 0;
    size_t len = 0;

    for (i=0; i < niov; i++) {
	len += iov[i].iov_len;
    }

#ifdef FS_STATE_USE_MMAP
    if (fs_stateCheckIOSafety(state, len)) {
	if (fs_stateResizeFile(state, len)) {
	    ViceLog(0, ("fs_stateWrite: could not resize dump file '%s'\n",
			state->fn));
	    ret = 1;
	    goto done;
	}
    }

    for (i=0; i < niov; i++) {
	memcpy(state->mmap.cursor, iov[i].iov_base, iov[i].iov_len);
	fs_stateIncCursor(state, iov[i].iov_len);
    }
#else
    if (writev(state->fd, iov, niov) != len) {
	ViceLog(0, ("fs_stateWriteV: write failed\n"));
	ret = 1;
	goto done;
    }
#endif

 done:
    return ret;
}

int
fs_stateReadV(struct fs_dump_state * state,
	      struct iovec * iov, int niov)
{
    int i, ret = 0;
    size_t len = 0;

    for (i=0; i < niov; i++) {
	len += iov[i].iov_len;
    }

#ifdef FS_STATE_USE_MMAP
    if (fs_stateCheckIOSafety(state, len)) {
	ViceLog(0, ("fs_stateRead: read beyond EOF for dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    for (i=0; i < niov; i++) {
	memcpy(iov[i].iov_base, state->mmap.cursor, iov[i].iov_len);
	fs_stateIncCursor(state, iov[i].iov_len);
    }
#else
    if (readv(state->fd, iov, niov) != len) {
	ViceLog(0, ("fs_stateReadV: read failed\n"));
	ret = 1;
	goto done;
    }
#endif

 done:
    return ret;
}

int
fs_stateWriteHeader(struct fs_dump_state * state,
		    afs_uint64 * offset,
		    void * hdr, size_t len)
{
    int ret = 0;

    if (fs_stateSeek(state, offset)) {
	ViceLog(0, ("fs_stateWriteHeader: could not seek to correct position in dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    if (fs_stateWrite(state, hdr, len)) {
	ViceLog(0, ("fs_stateWriteHeader: write failed\n"));
	ret = 1;
	goto done;
    }

 done:
    return ret;
}

int
fs_stateReadHeader(struct fs_dump_state * state,
		   afs_uint64 * offset,
		   void * hdr, size_t len)
{
    int ret = 0;

    if (fs_stateSeek(state, offset)) {
	ViceLog(0, ("fs_stateReadHeader: could not seek to correct position in dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    if (fs_stateRead(state, hdr,len)) {
	ViceLog(0, ("fs_stateReadHeader: read failed\n"));
	ret = 1;
	goto done;
    }

 done:
    return ret;
}

#ifdef FS_STATE_USE_MMAP
static int
fs_stateSizeFile(struct fs_dump_state * state)
{
    int ret = 0;
    state->file_len = FS_STATE_INIT_FILESIZE;
    if (afs_ftruncate(state->fd, state->file_len) != 0)
	ret = 1;
    return ret;
}

static int
fs_stateResizeFile(struct fs_dump_state * state, size_t min_add)
{
    int ret = 0;
    afs_foff_t inc;

#ifdef FS_STATE_USE_MMAP
    fs_stateUnmapFile(state);
#endif

    inc = ((min_add / FS_STATE_INIT_FILESIZE)+1) * FS_STATE_INIT_FILESIZE;
    state->file_len += inc;

    if (afs_ftruncate(state->fd, state->file_len) != 0) {
	ViceLog(0, ("fs_stateResizeFile: truncate failed\n"));
	ret = 1;
	goto done;
    }

#ifdef FS_STATE_USE_MMAP
    if (fs_stateMapFile(state)) {
	ViceLog(0, ("fs_stateResizeFile: remapping memory mapped file failed\n"));
	ret = 1;
	goto done;
    }
#endif

 done:
    return ret;
}

static int
fs_stateTruncateFile(struct fs_dump_state * state)
{
    int ret = 0;

#ifdef AFS_LARGEFILE_ENV
    if (afs_ftruncate(state->fd, state->eof_offset) != 0) {
	ret = 1;
    }
#else
    afs_uint32 hi, lo;
    SplitInt64(state->eof_offset, hi, lo);
    if (afs_ftruncate(state->fd, lo) != 0) {
	ret = 1;
    }
#endif

    return ret;
}
#endif

#ifdef FS_STATE_USE_MMAP
static int
fs_stateMapFile(struct fs_dump_state * state)
{
    int ret = 0, flags;

    switch(state->mode) {
    case FS_STATE_LOAD_MODE:
	flags = PROT_READ | PROT_WRITE;   /* loading involves a header invalidation */
	break;
    case FS_STATE_DUMP_MODE:
	flags = PROT_WRITE;
	break;
    default:
	ViceLog(0, ("fs_stateMapFile: invalid dump state mode\n"));
	return 1;
    }

    state->mmap.map = afs_mmap(NULL, 
			       state->file_len, 
			       flags, 
			       MAP_SHARED,
			       state->fd, 
			       0);

    if (state->mmap.map == MAP_FAILED) {
	state->mmap.size = 0;
	state->mmap.map = NULL;
	ViceLog(0, ("fs_stateMapFile: failed to memory map file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

    state->mmap.size = state->file_len;
    state->mmap.cursor = state->mmap.map;
    state->mmap.offset = 0;

    /* for state loading, accesses will be sequential, so let's give
     * the VM subsystem a heads up */
    if (state->mode == FS_STATE_LOAD_MODE) {
	/* XXX madvise may not exist on all platforms, so
	 * we may need to add some ifdefs at some point... */
	flags = MADV_SEQUENTIAL | MADV_WILLNEED;
#ifdef AFS_SUN510_ENV
	flags |= MADV_ACCESS_LWP;   /* added in solaris 9 12/02 */
#endif
	madvise(state->mmap.map, state->mmap.size, flags);
    }

 done:
    return ret;
}

static int
fs_stateUnmapFile(struct fs_dump_state * state)
{
    int ret = 0;

    if (munmap(state->mmap.map, state->mmap.size) == -1) {
	ViceLog(0, ("fs_stateUnmapFile: failed to unmap dump file '%s'\n",
		    state->fn));
	ret = 1;
	goto done;
    }

 done:
    return ret;
}
#endif /* FS_STATE_USE_MMAP */

#ifdef FS_STATE_USE_MMAP
int
fs_stateSync(struct fs_dump_state * state)
{
    int ret = 0;

    msync(state->mmap.map, state->mmap.size, MS_SYNC);

 done:
    return ret;
}
#else /* !FS_STATE_USE_MMAP */
int
fs_stateSync(struct fs_dump_state * state)
{
    int ret = 0;

    if (fsync(state->fd) == -1)
	ret = 1;

 done:
    return ret;
}
#endif /* !FS_STATE_USE_MMAP */

int
fs_stateIncEOF(struct fs_dump_state * state, afs_int32 len)
{
    afs_uint64 temp;
    FillInt64(temp, 0, len);
    AddUInt64(state->eof_offset, temp, &state->eof_offset);
    return 0;
}

#ifdef FS_STATE_USE_MMAP
static int
fs_stateIncCursor(struct fs_dump_state * state, size_t len)
{
    char * p;

    state->mmap.offset += len;

    p = (char *) state->mmap.cursor;
    p += len;
    state->mmap.cursor = (void *) p;

    return 0;
}

static int
fs_stateCheckIOSafety(struct fs_dump_state * state, size_t len)
{
    int ret = 0;

    if ((state->mmap.offset + len) > state->mmap.size) {
	ret = 1;
    }
    return ret;
}
#endif /* FS_STATE_USE_MMAP */

#ifdef FS_STATE_USE_MMAP
int
fs_stateSeek(struct fs_dump_state * state, afs_uint64 * offset)
{
    int ret = 0;
    char * p;
    afs_uint32 hi, lo;

    SplitInt64(*offset, hi, lo);

    /* update cursor */
    p = (char *) state->mmap.map;
#ifdef AFS_64BIT_ENV
    p += *offset;
#else
    p += lo;
#endif
    state->mmap.cursor = (void *) p;

    /* update offset */
#ifdef AFS_LARGEFILE_ENV
    state->mmap.offset = *offset;
#else
    if (hi)
	ret = 1;
    state->mmap.offset = lo;
#endif

    return ret;
}
#else /* !FS_STATE_USE_MMAP */
int
fs_stateSeek(struct fs_dump_state * state, afs_uint64 * offset)
{
    int ret = 0;
#ifndef AFS_LARGEFILE_ENV
    afs_uint32 high, low;
    
    SplitInt64(*offset, high, low);
    if (high) {
	ret = 1;
	goto done;
    }
    
    if (afs_lseek(state->fd, low, SEEK_SET) == -1)
	ret = 1;
#else
    if (afs_lseek(state->fd, *offset, SEEK_SET) == -1)
	ret = 1;
#endif
    return ret;
}
#endif /* !FS_STATE_USE_MMAP */

static int
fs_stateFillHeader(struct fs_state_header * hdr)
{
    hdr->stamp.magic = FS_STATE_MAGIC;
    hdr->stamp.version = FS_STATE_VERSION;
#ifdef SYS_NAME_ID
    hdr->sys_name = SYS_NAME_ID;
#else
    hdr->sys_name = 0xFFFFFFFF;
#endif
    hdr->timestamp = FT_ApproxTime();
    hdr->server_uuid = FS_HostUUID;
    hdr->valid = 1;
#ifdef WORDS_BIGENDIAN
    hdr->endianness = 1;
#else
    hdr->endianness = 0;
#endif
#ifdef FS_STATS_DETAILED
    hdr->stats_detailed = 1;
#else
    hdr->stats_detailed = 0;
#endif
    if (strlcpy(hdr->server_version_string, cml_version_number, sizeof(hdr->server_version_string))
	>= sizeof(hdr->server_version_string)) {
	ViceLog(0, ("fs_stateFillHeader: WARNING -- cml_version_number field truncated\n"));
    }
    return 0;
}

static int
fs_stateCheckHeader(struct fs_state_header * hdr)
{
    int ret = 0;

    if (!hdr->valid) {
	ViceLog(0, ("fs_stateCheckHeader: dump was previously flagged invalid\n"));
	ret = 1;
    }
#ifdef WORDS_BIGENDIAN
    else if (!hdr->endianness) {
	ViceLog(0, ("fs_stateCheckHeader: wrong endianness\n"));
	ret = 1;
    }
#else /* WORDS_BIGENDIAN */
    else if (hdr->endianness) {
	ViceLog(0, ("fs_stateCheckHeader: wrong endianness\n"));
	ret = 1;
    }
#endif /* WORDS_BIGENDIAN */

    else if (hdr->stamp.magic != FS_STATE_MAGIC) {
	ViceLog(0, ("fs_stateCheckHeader: invalid dump header\n"));
	ret = 1;
    }
    else if (hdr->stamp.version != FS_STATE_VERSION) {
	ViceLog(0, ("fs_stateCheckHeader: unknown dump format version number\n"));
	ret = 1;
    }

#ifdef FS_STATS_DETAILED
    else if (!hdr->stats_detailed) {
	ViceLog(0, ("fs_stateCheckHeader: wrong config flags\n"));
	ret = 1;
    }
#else /* FS_STATS_DETAILED */
    else if (hdr->stats_detailed) {
	ViceLog(0, ("fs_stateCheckHeader: wrong config flags\n"));
	ret = 1;
    }
#endif /* FS_STATS_DETAILED */

    else if (!afs_uuid_equal(&hdr->server_uuid, &FS_HostUUID)) {
	ViceLog(0, ("fs_stateCheckHeader: server UUID does not match this server's UUID\n"));
	ret = 1;
    }

    /* the cml_version_string is included for informational purposes only.  If someone ever
     * wants to limit state dump reloading based upon the contents of this string, just
     * uncomment the following code.  uncommenting this code is _strongly discouraged_ because
     * we already make use of the version stamps in the various dump headers to deal with
     * data structure version incompatabilities.
    else if (strncmp(hdr->server_version_string, cml_version_number, 
		     sizeof(hdr->server_version_string)) != 0) {
	ViceLog(0, ("fs_stateCheckHeader: dump from different server version\n"));
	ret = 1;
    }
    */

    else if (strncmp(hdr->server_version_string, cml_version_number, 
		     sizeof(hdr->server_version_string)) != 0) {
	ViceLog(0, ("fs_stateCheckHeader: dump from different server version ; attempting state reload anyway\n"));
    }


    return ret;
}

static int
fs_stateAlloc(struct fs_dump_state * state)
{
    int ret = 0;
    memset(state, 0, sizeof(struct fs_dump_state));
    state->fd = -1;
    state->fn = AFSDIR_SERVER_FSSTATE_FILEPATH;
    state->hdr = (struct fs_state_header *)malloc(sizeof(struct fs_state_header));
    state->h_hdr = (struct host_state_header *)malloc(sizeof(struct host_state_header));
    state->cb_hdr = (struct callback_state_header *)malloc(sizeof(struct callback_state_header));
    state->cb_timeout_hdr = (struct callback_state_timeout_header *)
      malloc(sizeof(struct callback_state_timeout_header));
    state->cb_fehash_hdr = (struct callback_state_fehash_header *)
      malloc(sizeof(struct callback_state_fehash_header));
    if ((state->hdr == NULL) || (state->h_hdr == NULL) || (state->cb_hdr == NULL) ||
	(state->cb_timeout_hdr == NULL) || (state->cb_fehash_hdr == NULL))
	ret = 1;
    return ret;
}

static int
fs_stateFree(struct fs_dump_state * state)
{
    if (state->hdr)
	free(state->hdr);
    if (state->h_hdr)
	free(state->h_hdr);
    if (state->cb_hdr)
	free(state->cb_hdr);
    if (state->cb_timeout_hdr)
	free(state->cb_timeout_hdr);
    if (state->cb_fehash_hdr)
	free(state->cb_fehash_hdr);
    if (state->h_map.entries)
	free(state->h_map.entries);
    if (state->fe_map.entries)
	free(state->fe_map.entries);
    if (state->cb_map.entries)
	free(state->cb_map.entries);
    return 0;
}

#endif /* AFS_DEMAND_ATTACH_FS */
