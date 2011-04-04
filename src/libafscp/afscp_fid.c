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

#include <search.h>

#include <afs/vlserver.h>
#include <afs/vldbint.h>
#include "afscp.h"
#include "afscp_internal.h"

/*
 * Allocate and populate an afscp_venusfid struct from component parts
 */
struct afscp_venusfid *
afscp_MakeFid(struct afscp_cell *cell, afs_uint32 volume,
	      afs_uint32 vnode, afs_uint32 unique)
{
    struct afscp_venusfid *ret;

    if (cell == NULL) {
	return NULL;
    }
    ret = malloc(sizeof(struct afscp_venusfid));
    if (ret == NULL) {
	afscp_errno = errno;
	return NULL;
    }
    ret->cell = cell;
    ret->fid.Volume = volume;
    ret->fid.Vnode = vnode;
    ret->fid.Unique = unique;
    return ret;
}

/*
 * Duplicate an existing afscp_venusfid struct
 */
struct afscp_venusfid *
afscp_DupFid(const struct afscp_venusfid *in)
{
    struct afscp_venusfid *ret;

    ret = malloc(sizeof(struct afscp_venusfid));
    if (ret == NULL) {
	afscp_errno = errno;
	return NULL;
    }
    ret->cell = in->cell;
    ret->fid.Volume = in->fid.Volume;
    ret->fid.Vnode = in->fid.Vnode;
    ret->fid.Unique = in->fid.Unique;
    return ret;
}

void
afscp_FreeFid(struct afscp_venusfid *avfp)
{
    if (avfp != NULL)
	free(avfp);
}

static int
statcompare(const void *a, const void *b)
{
    const struct afscp_statent *sa = a, *sb = b;
    if (sa->me.fid.Vnode < sb->me.fid.Vnode)
	return -1;
    if (sa->me.fid.Vnode > sb->me.fid.Vnode)
	return 1;
    if (sa->me.fid.Unique < sb->me.fid.Unique)
	return -1;
    if (sa->me.fid.Unique > sb->me.fid.Unique)
	return 1;
    return 0;
}

int
afscp_GetStatus(const struct afscp_venusfid *fid, struct AFSFetchStatus *s)
{
    struct afscp_volume *v;
    struct afscp_server *server;
    struct AFSCallBack cb;
    struct AFSVolSync vs;
    struct AFSFid tf = fid->fid;
    struct afscp_statent *stored, key;
    void *cached;
    int code, i, j;
    time_t now;

    v = afscp_VolumeById(fid->cell, fid->fid.Volume);
    if (v == NULL) {
	return -1;
    }
    memset(&key, 0, sizeof(key));
    memcpy(&key.me, fid, sizeof(*fid));

    cached = tfind(&key, &v->statcache, statcompare);
    if (cached != NULL) {
	stored = *(struct afscp_statent **)cached;
	memmove(s, &stored->status, sizeof(*s));
	afs_dprintf(("Stat %u.%lu.%lu.%lu returning cached result\n",
		     fid->cell->id, fid->fid.Volume, fid->fid.Vnode,
		     fid->fid.Unique));
	return 0;
    }

    code = ENOENT;
    for (i = 0; i < v->nservers; i++) {
	server = afscp_ServerByIndex(v->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		time(&now);
		code = RXAFS_FetchStatus(server->conns[j], &tf, s, &cb, &vs);
		if (code == 0) {
		    afscp_AddCallBack(server, &fid->fid, s, &cb, now);	/* calls _StatStuff */
		    afs_dprintf(("Stat %d.%lu.%lu.%lu"
				 " ok: type %ld size %ld\n",
				 fid->cell->id,
				 afs_printable_uint32_lu(fid->fid.Volume),
				 afs_printable_uint32_lu(fid->fid.Vnode),
				 afs_printable_uint32_lu(fid->fid.Unique),
				 afs_printable_int32_ld(s->FileType),
				 afs_printable_int32_ld(s->Length)));
		    return 0;
		}
	    }
	}
    }
    afscp_errno = code;
    return -1;
}

int
afscp_Stat(const struct afscp_venusfid *fid, struct stat *s)
{

    struct AFSFetchStatus status;
    int code;


    if (s == NULL || fid == NULL) {
	fprintf(stderr, "NULL args given to afscp_Stat, cannot continue\n");
	return -1;
    }

    code = afscp_GetStatus(fid, &status);
    if (code != 0) {
	return code;
    }

    if (status.FileType == File)
	s->st_mode = S_IFREG;
    else if (status.FileType == Directory)
	s->st_mode = S_IFDIR;
#ifndef AFS_NT40_ENV
    else if (status.FileType == SymbolicLink)
	s->st_mode = S_IFLNK;
    /* a behavior needs to be defined on Windows */
#endif
    else {
	afscp_errno = EINVAL;
	return -1;
    }
    s->st_mode |= (status.UnixModeBits & (~S_IFMT));
    s->st_nlink = status.LinkCount;
    s->st_size = status.Length;
    s->st_uid = status.Owner;
    /*s->st_blksize=status.SegSize; */
    s->st_atime = s->st_mtime = status.ClientModTime;
    s->st_ctime = status.ServerModTime;
    s->st_gid = status.Group;
    return 0;
}

int
_StatInvalidate(const struct afscp_venusfid *fid)
{
    struct afscp_volume *v;
    struct afscp_statent key;
    void **cached;

    v = afscp_VolumeById(fid->cell, fid->fid.Volume);
    if (v == NULL) {
	return -1;
    }
    memmove(&key.me, fid, sizeof(*fid));

    cached = tfind(&key, &v->statcache, statcompare);
    if (cached != NULL) {
	free(*cached);
	tdelete(&key, &v->statcache, statcompare);
    }
    return 0;
}

int
_StatStuff(const struct afscp_venusfid *fid, const struct AFSFetchStatus *s)
{
    struct afscp_volume *v;
    struct afscp_statent key, *stored;
    void **cached;

    v = afscp_VolumeById(fid->cell, fid->fid.Volume);
    if (v == NULL) {
	return -1;
    }
    memmove(&key.me, fid, sizeof(*fid));

    cached = tsearch(&key, &v->statcache, statcompare);
    if (cached != NULL) {
	stored = malloc(sizeof(struct afscp_statent));
	if (stored != NULL) {
	    memmove(&stored->me, fid, sizeof(*fid));
	    memmove(&stored->status, s, sizeof(*s));
	    *(struct afscp_statent **)cached = stored;
	} else {
	    tdelete(&key, &v->statcache, statcompare);
	}
    }
    return 0;
}

int
afscp_StoreStatus(const struct afscp_venusfid *fid, struct AFSStoreStatus *s)
{
    struct afscp_volume *v;
    struct afscp_server *server;
    struct AFSVolSync vs;
    struct AFSFetchStatus fst;
    struct AFSFid tf = fid->fid;
    int code, i, j;

    v = afscp_VolumeById(fid->cell, fid->fid.Volume);
    if (v == NULL) {
	return -1;
    }

    code = ENOENT;
    for (i = 0; i < v->nservers; i++) {
	server = afscp_ServerByIndex(v->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		code = RXAFS_StoreStatus(server->conns[j], &tf, s, &fst, &vs);
		if (code == 0) {
		    _StatStuff(fid, &fst);	/* calls _StatStuff */
		    return 0;
		}
	    }
	}
    }
    afscp_errno = code;
    return -1;
}
