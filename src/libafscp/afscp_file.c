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
#include "afscp.h"
#include "afscp_internal.h"

/* this is not yet 64-bit clean */
ssize_t
afscp_PRead(const struct afscp_venusfid * fid, void *buffer,
	    size_t count, off_t offset)
{
    struct AFSFetchStatus fst;
    struct AFSVolSync vs;
    struct AFSCallBack cb;
    struct AFSFid tf = fid->fid;
    struct afscp_volume *vol;
    struct afscp_server *server;
    struct rx_call *c = NULL;
    int code, code2 = 0;
    int i, j, bytes, totalbytes = 0;
    int bytesremaining;
    char *p;
    time_t now;

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
		c = rx_NewCall(server->conns[j]);
		if (c != 0) {
		    p = buffer;
		    code = StartRXAFS_FetchData(c, &tf, offset, count);
		    if (code != 0) {
			code = rx_EndCall(c, code);
			continue;
		    }
		    bytes =
			rx_Read(c, (char *)&bytesremaining,
				sizeof(afs_int32));
		    if (bytes != sizeof(afs_int32)) {
			code = rx_EndCall(c, bytes);
			continue;
		    }
		    bytesremaining = ntohl(bytesremaining);
		    totalbytes = 0;
		    while (bytesremaining > 0) {
			bytes = rx_Read(c, p, bytesremaining);
			if (bytes <= 0)
			    break;
			p += bytes;
			totalbytes += bytes;
			bytesremaining -= bytes;
		    }
		    if (bytesremaining == 0) {
			time(&now);
			code2 = EndRXAFS_FetchData(c, &fst, &cb, &vs);
			if (code2 == 0)
			    afscp_AddCallBack(server, &fid->fid, &fst, &cb,
					      now);
		    }
		    code = rx_EndCall(c, code2);
		}
		if (code == 0) {
		    return totalbytes;
		}
	    }
	}
    }
    afscp_errno = code;
    return -1;
}

/* this is not yet 64-bit clean */
ssize_t
afscp_PWrite(const struct afscp_venusfid * fid, const void *buffer,
	     size_t count, off_t offset)
{
    struct AFSFetchStatus fst;
    struct AFSStoreStatus sst;
    struct AFSVolSync vs;
    struct AFSCallBack cb;
    struct AFSFid tf = fid->fid;
    struct afscp_volume *vol;
    struct afscp_server *server;
    struct rx_call *c = NULL;
    int code, code2 = 0;
    int i, j, bytes, totalbytes = 0;
    int bytesremaining;
    const char *p;
    off_t filesize;
    time_t now;

    memset(&sst, 0, sizeof(sst));
    vol = afscp_VolumeById(fid->cell, fid->fid.Volume);
    if (vol == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }
    if (vol->voltype != RWVOL) {
	afscp_errno = EROFS;
	return -1;
    }

    code = ENOENT;
    for (i = 0; i < vol->nservers; i++) {
	server = afscp_ServerByIndex(vol->servers[i]);
	if (server && server->naddrs > 0) {
	    for (j = 0; j < server->naddrs; j++) {
		code =
		    RXAFS_FetchStatus(server->conns[j], &tf, &fst, &cb, &vs);
		if (code != 0)
		    continue;
		sst.Mask = AFS_SETMODTIME;
		time(&now);
		sst.ClientModTime = now;
		filesize = fst.Length;
		if (offset + count > filesize)
		    filesize = offset + count;
		c = rx_NewCall(server->conns[j]);
		if (c != 0) {
		    p = buffer;
		    code =
			StartRXAFS_StoreData(c, &tf, &sst, offset, count,
					     filesize);
		    if (code != 0) {
			code = rx_EndCall(c, code);
			continue;
		    }
		    /*
		     * seems to write file length to beginning of file -- why?
		     */
		    /*
		     * bytesremaining = htonl(count);
		     * bytes = rx_Write(c, (char *)&bytesremaining,
		     *                  sizeof(afs_int32));
		     * if (bytes != sizeof(afs_int32)) {
		     *  code = rx_EndCall(c, bytes);
		     *  continue;
		     * }
		     */
		    bytesremaining = count;
		    totalbytes = 0;
		    while (bytesremaining > 0) {
			bytes = rx_Write(c, (char *)p, bytesremaining);
			if (bytes <= 0)
			    break;
			p += bytes;
			totalbytes += bytes;
			bytesremaining -= bytes;
		    }
		    if (bytesremaining == 0) {
			code2 = EndRXAFS_StoreData(c, &fst, &vs);
		    }
		    code = rx_EndCall(c, code2);
		}
		if (code == 0) {
		    return totalbytes;
		}
	    }
	}
    }
    afscp_errno = code;
    return -1;
}
