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
#include <afs/param.h>
#include <afs/afsint.h>
#include <afs/vlserver.h>
#include <afs/vldbint.h>
#include <afs/dir.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "afscp.h"
#include "afscp_internal.h"


int afs_FetchACL(const struct afs_venusfid *dir,
		 struct AFSOpaque *acl) {
  int code, i, j;
  struct AFSFid df = dir->fid;
  struct afs_volume *vol;
  struct AFSFetchStatus dfst;
  struct AFSVolSync vs;
  struct afs_volume *volume;
  struct afs_server *server;

  vol=afs_volumebyid(dir->cell, dir->fid.Volume);
  if (!vol) {
    afs_errno=ENOENT;
    return -1;
  }
  code=ENOENT;
  for (i=0;i<vol->nservers;i++) {
    server=afs_serverbyindex(vol->servers[i]);
    if (server && server->naddrs > 0) {
      for (j=0;j < server->naddrs;j++) {
	code=RXAFS_FetchACL(server->conns[j], &df, acl, &dfst, &vs);
	if (code >= 0)
	  break;
      }
    }
    if (code >= 0)
      break;
  }
  if (code) {
    _StatInvalidate(dir);
    afs_errno=code;
    return -1;
  }
  _StatStuff(dir, &dfst);
  return 0;
}


int afs_StoreACL(const struct afs_venusfid *dir,
		 struct AFSOpaque *acl) {
  int code, i, j;
  struct AFSFid df = dir->fid;
  struct afs_volume *vol;
  struct AFSFetchStatus dfst;
  struct AFSVolSync vs;
  struct afs_volume *volume;
  struct afs_server *server;

  vol=afs_volumebyid(dir->cell, dir->fid.Volume);
  if (!vol) {
    afs_errno=ENOENT;
    return -1;
  }
  code=ENOENT;
  for (i=0;i<vol->nservers;i++) {
    server=afs_serverbyindex(vol->servers[i]);
    if (server && server->naddrs > 0) {
      for (j=0;j < server->naddrs;j++) {
	code=RXAFS_StoreACL(server->conns[j], &df, acl, &dfst, &vs);
	if (code >= 0)
	  break;
      }
    }
    if (code >= 0)
      break;
  }
  if (code) {
    _StatInvalidate(dir);
    afs_errno=code;
    return -1;
  }
  _StatStuff(dir, &dfst);
  return 0;
}
