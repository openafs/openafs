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

#include <afs/vlserver.h>
#include <afs/vldbint.h>
#include <afs/dir.h>
#include "afscp.h"
#include "afscp_internal.h"
#include <afs/unified_afs.h>

int
afscp_CreateFile(const struct afscp_venusfid *dir, char *name,
		 struct AFSStoreStatus *sst, struct afscp_venusfid **ret)
{
    int code, i, j;
    struct AFSFid df = dir->fid;
    struct afscp_volume *vol;
    struct AFSFetchStatus dfst, fst;
    struct AFSVolSync vs;
    struct AFSCallBack cb;
    struct AFSFid ff;
    struct afscp_server *server;
    struct rx_connection *c;
    time_t now;

    if (dir == NULL || name == NULL || sst == NULL) {
	fprintf(stderr,
		"afscp_CreateFile called with NULL args, cannot continue\n");
	return -1;
    }
    vol = afscp_VolumeById(dir->cell, dir->fid.Volume);
    if (vol == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }
    code = ENOENT;
    for (i = 0; i < vol->nservers; i++) {
	server = afscp_ServerByIndex(vol->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		c = afscp_ServerConnection(server, j);
		if (c == NULL) {
		    break;
		}
		time(&now);
		code = RXAFS_CreateFile(c, &df, name, sst, &ff,
					&fst, &dfst, &cb, &vs);
		if (code >= 0) {
		    break;
		}
	    }
	}
	if (code >= 0) {
	    break;
	}
    }
    if (code != 0) {
	_StatInvalidate(dir);
	afscp_errno = code;
	return -1;
    }
    _StatStuff(dir, &dfst);
    afscp_AddCallBack(server, &ff, &fst, &cb, now);
    if (ret != NULL)
	*ret = afscp_MakeFid(vol->cell, ff.Volume, ff.Vnode, ff.Unique);
    return 0;
}

int
afscp_MakeDir(const struct afscp_venusfid *dir, char *name,
	      struct AFSStoreStatus *sst, struct afscp_venusfid **ret)
{
    int code, i, j;
    struct AFSFid df = dir->fid;
    struct afscp_volume *vol;
    struct AFSFetchStatus dfst, fst;
    struct AFSVolSync vs;
    struct AFSCallBack cb;
    struct AFSFid ff;
    struct afscp_server *server;
    struct rx_connection *c;
    time_t now;

    vol = afscp_VolumeById(dir->cell, dir->fid.Volume);
    if (vol == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }
    code = ENOENT;
    for (i = 0; i < vol->nservers; i++) {
	server = afscp_ServerByIndex(vol->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		c = afscp_ServerConnection(server, j);
		if (c == NULL)
		    break;
		time(&now);
		code = RXAFS_MakeDir(c, &df, name, sst, &ff,
				     &fst, &dfst, &cb, &vs);
		if (code >= 0)
		    break;
	    }
	}
	if (code >= 0)
	    break;
    }
    if (code != 0) {
	_StatInvalidate(dir);
	afscp_errno = code;
	return -1;
    }
    _StatStuff(dir, &dfst);
    afscp_AddCallBack(server, &ff, &fst, &cb, now);
    if (ret != NULL)
	*ret = afscp_MakeFid(vol->cell, ff.Volume, ff.Vnode, ff.Unique);
    return 0;
}

int
afscp_Symlink(const struct afscp_venusfid *dir, char *name,
	      char *target, struct AFSStoreStatus *sst)
{
    int code, i, j;
    struct AFSFid df = dir->fid;
    struct afscp_volume *vol;
    struct AFSFetchStatus dfst, fst;
    struct AFSVolSync vs;
    struct AFSFid ff;
    struct afscp_server *server;
    struct rx_connection *c;

    vol = afscp_VolumeById(dir->cell, dir->fid.Volume);
    if (vol == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }
    code = ENOENT;
    for (i = 0; i < vol->nservers; i++) {
	server = afscp_ServerByIndex(vol->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		c = afscp_ServerConnection(server, j);
		if (c == NULL)
		    break;
		code = RXAFS_Symlink(c, &df, name, target, sst,
				     &ff, &fst, &dfst, &vs);
		if (code >= 0)
		    break;
	    }
	}
	if (code >= 0)
	    break;
    }
    if (code != 0) {
	_StatInvalidate(dir);
	afscp_errno = code;
	return -1;
    }
    _StatStuff(dir, &dfst);
    return 0;
}


int
afscp_Lock(const struct afscp_venusfid *fid, int locktype)
{
    int code, i, j;
    struct AFSFid ff = fid->fid;
    struct afscp_volume *vol;
    struct AFSVolSync vs;
    struct afscp_server *server;
    struct rx_connection *c;

    vol = afscp_VolumeById(fid->cell, fid->fid.Volume);
    if (vol == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }
    code = ENOENT;
    for (i = 0; i < vol->nservers; i++) {
	server = afscp_ServerByIndex(vol->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		c = afscp_ServerConnection(server, j);
		if (c == NULL)
		    break;
		if (locktype == LockRelease)
		    code = RXAFS_ReleaseLock(c, &ff, &vs);
		/* read, write, extend */
		else if (locktype < LockRelease)
		    code = RXAFS_SetLock(c, &ff, locktype, &vs);
		if (code >= 0)
		    break;
	    }
	}
	if (code >= 0)
	    break;
    }
    if (code != 0) {
	if ((code == EAGAIN) || (code == UAEWOULDBLOCK) || (code == UAEAGAIN))
	    code = EWOULDBLOCK;
	afscp_errno = code;
	return -1;
    }
    return 0;
}


int
afscp_RemoveFile(const struct afscp_venusfid *dir, char *name)
{
    int code, i, j;
    struct AFSFid df = dir->fid;
    struct afscp_volume *vol;
    struct AFSFetchStatus dfst;
    struct AFSVolSync vs;
    struct afscp_server *server;
    struct rx_connection *c;

    vol = afscp_VolumeById(dir->cell, dir->fid.Volume);
    if (vol == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }
    code = ENOENT;
    for (i = 0; i < vol->nservers; i++) {
	server = afscp_ServerByIndex(vol->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		c = afscp_ServerConnection(server, j);
		if (c == NULL)
		    break;
		code = RXAFS_RemoveFile(c, &df, name, &dfst, &vs);
		if (code >= 0)
		    break;
	    }
	}
	if (code >= 0)
	    break;
    }
    if (code != 0) {
	_StatInvalidate(dir);
	afscp_errno = code;
	return -1;
    }
    _StatStuff(dir, &dfst);
    return 0;
}

int
afscp_RemoveDir(const struct afscp_venusfid *dir, char *name)
{
    int code, i, j;
    struct AFSFid df = dir->fid;
    struct afscp_volume *vol;
    struct AFSFetchStatus dfst;
    struct AFSVolSync vs;
    struct afscp_server *server;
    struct rx_connection *c;

    vol = afscp_VolumeById(dir->cell, dir->fid.Volume);
    if (vol == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }
    code = ENOENT;
    for (i = 0; i < vol->nservers; i++) {
	server = afscp_ServerByIndex(vol->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		c = afscp_ServerConnection(server, j);
		if (c == NULL)
		    break;
		code = RXAFS_RemoveDir(c, &df, name, &dfst, &vs);
		if (code >= 0)
		    break;
	    }
	}
	if (code >= 0)
	    break;
    }
    if (code != 0) {
	_StatInvalidate(dir);
	afscp_errno = code;
	return -1;
    }
    _StatStuff(dir, &dfst);
    return 0;
}
