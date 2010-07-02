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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <search.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "afscp.h"
#include "afscp_internal.h"

struct afs_venusfid *makefid(struct afs_cell *cell, afs_uint32 volume,
                             afs_uint32 vnode, afs_uint32 unique)
{
     struct afs_venusfid *ret;

     if (!cell)
          return NULL;
     ret=malloc(sizeof(struct afs_venusfid));
     if (!ret) {
          afs_errno=errno;
          return NULL;
     }
     ret->cell=cell;
     ret->fid.Volume=volume;
     ret->fid.Vnode=vnode;
     ret->fid.Unique=unique;
     return ret;
}

struct afs_venusfid *dupfid(const struct afs_venusfid *in)
{
     struct afs_venusfid *ret;

     ret=malloc(sizeof(struct afs_venusfid));
     if (!ret) {
          afs_errno=errno;
          return NULL;
     }
     ret->cell=in->cell;
     ret->fid.Volume=in->fid.Volume;
     ret->fid.Vnode=in->fid.Vnode;
     ret->fid.Unique=in->fid.Unique;
     return ret;
}


static int statcompare(const void *a, const void *b)
{
     const struct afs_statent *sa=a,*sb=b;
     if (sa->me.fid.Vnode < sb->me.fid.Vnode) return -1;
     if (sa->me.fid.Vnode > sb->me.fid.Vnode) return 1;
     if (sa->me.fid.Unique < sb->me.fid.Unique) return -1;
     if (sa->me.fid.Unique > sb->me.fid.Unique) return 1;
     return 0;
}


int afs_GetStatus(const struct afs_venusfid *fid, struct AFSFetchStatus *s)
{
     struct afs_volume *v;
     struct afs_server *server;
     struct AFSCallBack cb;
     struct AFSVolSync vs;
     struct AFSFid tf = fid->fid;
     struct afs_statent *stored,key;
     void *cached;

     int code,i,j;

     v=afs_volumebyid(fid->cell, fid->fid.Volume);
     if (!v)
          return -1;
     memset(&key, 0, sizeof(key));
     memcpy(&key.me,fid,sizeof(*fid));

     cached=tfind(&key, &v->statcache, statcompare);
     if (cached) {
          stored=*(struct afs_statent **)cached;
          memmove(s, &stored->status,sizeof(*s));
          afscp_dprintf(("Stat %u.%lu.%lu.%lu returning cached result\n",
                 fid->cell->id, fid->fid.Volume, fid->fid.Vnode, fid->fid.Unique));
          return 0;
     }


     code=ENOENT;
     for (i=0;i<v->nservers;i++) {
       server=afs_serverbyindex(v->servers[i]);
       if (server && server->naddrs > 0) {
	 for (j=0;j < server->naddrs;j++) {
	   code=RXAFS_FetchStatus(server->conns[j], &tf, s, &cb, &vs);
	   if (!code) {
             AddCallBack(server, &fid->fid, s, &cb); /* calls _StatStuff */
             afscp_dprintf(("Stat %u.%" PRIu32 ".%" PRIu32 ".%" PRIu32 " ok: type %" PRId32 " size %" PRId32 "\n",
                 fid->cell->id, fid->fid.Volume, fid->fid.Vnode, fid->fid.Unique, s->FileType, s->Length));
	     return 0;
	   }
	 }
       }
     }
     afs_errno=code;
     return -1;
}



int afs_stat(const struct afs_venusfid *fid, struct stat *s)
{

     struct AFSFetchStatus status;
     int code;

     code=afs_GetStatus(fid, &status);
     if (code)
          return code;

     if (status.FileType==File)
          s->st_mode=S_IFREG;
     else if (status.FileType==Directory)
          s->st_mode=S_IFDIR;
     else if (status.FileType==SymbolicLink)
          s->st_mode=S_IFLNK;
     else {
          afs_errno=EINVAL;
          return -1;
     }
     s->st_mode |= (status.UnixModeBits & (~S_IFMT));
     s->st_nlink = status.LinkCount;
     s->st_size =status.Length;
     s->st_uid =status.Owner;
     /*s->st_blksize=status.SegSize;*/
     s->st_atime=s->st_mtime=status.ClientModTime;
     s->st_ctime=status.ServerModTime;
     s->st_gid = status.Group;
     return 0;
}


int _StatInvalidate(const struct afs_venusfid *fid) {
     struct afs_volume *v;
     struct afs_statent key;
     void **cached;

     v=afs_volumebyid(fid->cell, fid->fid.Volume);
     if (!v)
          return -1;
     memmove(&key.me,fid,sizeof(*fid));

     cached=tfind(&key, &v->statcache, statcompare);
     if (cached) {
              free(*cached);
              tdelete(&key, &v->statcache, statcompare);
     }
     return 0;
}

int _StatStuff(const struct afs_venusfid *fid, const struct AFSFetchStatus *s) {
     struct afs_volume *v;
     struct afs_statent key, *stored;
     void **cached;

     v=afs_volumebyid(fid->cell, fid->fid.Volume);
     if (!v)
          return -1;
     memmove(&key.me,fid,sizeof(*fid));

     cached=tsearch(&key, &v->statcache, statcompare);
     if (cached) {
          stored=malloc(sizeof(struct afs_statent));
          if (stored) {
               memmove(&stored->me, fid, sizeof(*fid));
               memmove(&stored->status,s,sizeof(*s));
               *(struct afs_statent **)cached=stored;
          } else {
               tdelete(&key, &v->statcache, statcompare);
          }
     }
     return 0;
}

int afs_StoreStatus(const struct afs_venusfid *fid, struct AFSStoreStatus *s)
{
     struct afs_volume *v;
     struct afs_server *server;
     struct AFSCallBack cb;
     struct AFSVolSync vs;
     struct AFSFetchStatus fst;
     struct AFSFid tf = fid->fid;
     int code,i,j;

     v=afs_volumebyid(fid->cell, fid->fid.Volume);
     if (!v)
          return -1;


     code=ENOENT;
     for (i=0;i<v->nservers;i++) {
       server=afs_serverbyindex(v->servers[i]);
       if (server && server->naddrs > 0) {
	 for (j=0;j < server->naddrs;j++) {
	   code=RXAFS_StoreStatus(server->conns[j], &tf, s, &fst, &vs);
	   if (!code) {
             _StatStuff(fid, &fst); /* calls _StatStuff */
	     return 0;
	   }
	 }
       }
     }
     afs_errno=code;
     return -1;
}
