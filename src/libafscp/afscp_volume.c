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
#include <afs/vlserver.h>
#include <afs/vldbint.h>
#include <afs/volint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include "afscp.h"
#include "afscp_internal.h"

#include <search.h>
#include <time.h>

static int icompare(const void *pa, const void *pb) {
  const struct afs_volume *va=pa,*vb=pb;

  if (va->id > vb->id) return 1;
  if (va->id < vb->id) return -1;
  return 0;
}


static int ncompare(const void *pa, const void *pb) {
  const struct afs_volume *va=pa,*vb=pb;

  if (va->voltype > vb->voltype) return 1;
  if (vb->voltype < va->voltype) return -1;
  return strcmp(va->name, vb->name);
}


union allvldbentry
{
     struct uvldbentry u;
     struct nvldbentry n;
     struct vldbentry o;
};

struct afs_volume *afs_volumebyname(struct afs_cell *cell, const char *vname, afs_int32 intype)
{
     union allvldbentry u;
     struct afs_volume *ret,key;
     struct afs_server *server;
     afs_int32 code,vtype,type,srv;
     void *s;
     struct in_addr i;

     if (intype == VOLTYPE_RW)
          vtype=VLSF_RWVOL;
     else if (intype == VOLTYPE_RO)
          vtype=VLSF_ROVOL;
     else if (intype == VOLTYPE_BK)
          vtype=VLSF_BACKVOL;
     else {
          afs_errno=EINVAL;
          return NULL;
     }

     memset(&key,0, sizeof(key));
     strcpy(key.name,vname);
     key.voltype=vtype;
     s=tfind(&key, &cell->volsbyname, ncompare);
     if (s) {
       ret=*(struct afs_volume **)s;
       return ret;
     }

     type=0;
     if ((code=ubik_VL_GetEntryByNameU(cell->vlservers, 0, (char *)vname, &u.u))
         == RXGEN_OPCODE) {
          type=1;
          if ((code=ubik_VL_GetEntryByNameN(cell->vlservers, 0, (char *)vname, &u.n))
              == RXGEN_OPCODE) {
               type=2;
               code=ubik_VL_GetEntryByNameO(cell->vlservers, 0, (char *)vname, &u.o);
          }
     }
     if (code) {
          afs_errno=code;
          return NULL;
     }
     ret=malloc(sizeof(struct afs_volume));
     if (!ret) {
          afs_errno=ENOMEM;
          return NULL;
     }
     memset(ret,0,sizeof(struct afs_volume));
     strcpy(ret->name, u.u.name);
     ret->nservers=0;
     ret->cell=cell;
     switch (type) {
     case 0:
       ret->id=u.u.volumeId[intype];
          for (srv=0;srv < u.u.nServers;srv++) {
               if ((u.u.serverFlags[srv] & vtype) == 0)
                    continue;
               //printf("uvldbentry server %d flags: %x\n",srv,  u.u.serverFlags[srv]);

               if ((u.u.serverFlags[srv] & VLSERVER_FLAG_UUID) == 0)
                    server=afs_serverbyaddr(cell, u.u.serverNumber[srv].time_low);
               else
                    server=afs_serverbyid(cell, &u.u.serverNumber[srv]);
               if (!server)
                    continue;
               ret->servers[ret->nservers++]=server->index;
          }
          break;
     case 1:
       ret->id=u.n.volumeId[intype];
          for (srv=0;srv < u.n.nServers;srv++) {
               if ((u.n.serverFlags[srv] & vtype) == 0)
                    continue;
               server=afs_serverbyaddr(cell, u.n.serverNumber[srv]);
               if (!server)
                    continue;
               ret->servers[ret->nservers++]=server->index;
          }
          break;
     case 2:
       ret->id=u.o.volumeId[intype];
          for (srv=0;srv < u.o.nServers;srv++) {
               if ((u.o.serverFlags[srv] & vtype) == 0)
                    continue;
               server=afs_serverbyaddr(cell, u.o.serverNumber[srv]);
               if (!server)
                    continue;
               ret->servers[ret->nservers++]=server->index;
          }
          break;
     }
     if (!ret->nservers || !ret->id) {
          free(ret);
          return NULL;
     }

     ret->voltype=intype;
     server=afs_serverbyindex(ret->servers[0]);
     if (server)
       i.s_addr=server->addrs[0];
     else
       i.s_addr=0;
     afscp_dprintf(("New volume BYNAME %s (%lu) on %s (%d)\n", ret->name, ret->id, inet_ntoa(i),ret->servers[0]));
     s=tsearch(&key, &cell->volsbyname, ncompare);
     if (s)
       *(struct afs_volume **)s=ret;
     key.id=ret->id;
     s=tsearch(&key, &cell->volsbyid, icompare);
     if (s)
       *(struct afs_volume **)s=ret;
     return ret;
}

struct afs_volume *afs_volumebyid(struct afs_cell *cell, afs_uint32 id)
{
     union allvldbentry u;
     struct afs_volume *ret,key;
     struct afs_server *server;
     afs_int32 code,vtype,type,srv;
     int voltype;
     char idbuffer[16];
     void *s;
     struct in_addr i;

     memset(&key,0, sizeof(key));
     key.id=id;
     s=tfind(&key, &cell->volsbyid, icompare);
     if (s) {
       ret=*(struct afs_volume **)s;
       return ret;
     }

     sprintf(idbuffer,"%" PRIu32, id);
     type=0;
     if ((code=ubik_VL_GetEntryByNameU(cell->vlservers, 0, idbuffer, &u.u))
         == RXGEN_OPCODE) {
          type=1;
          if ((code=ubik_VL_GetEntryByIDN(cell->vlservers, 0, id, -1, &u.n))
              == RXGEN_OPCODE) {
               type=2;
               code=ubik_VL_GetEntryByID(cell->vlservers, 0, id, -1,  &u.o);
          }
     }
     if (code) {
          afs_errno=code;
          return NULL;
     }
     ret=malloc(sizeof(struct afs_volume));
     if (!ret) {
          afs_errno=ENOMEM;
          return NULL;
     }

     memset(ret,0,sizeof(struct afs_volume));
     strcpy(ret->name, u.u.name);
     ret->nservers=0;
     ret->cell=cell;

     switch (type) {
     case 0:
          if (id == u.u.volumeId[RWVOL]) {
               vtype=VLSF_RWVOL;
               voltype=VOLTYPE_RW;
          } else if (id == u.u.volumeId[ROVOL]) {
               vtype=VLSF_ROVOL;
               voltype=VOLTYPE_RO;
          } else if (id == u.u.volumeId[BACKVOL]) {
               vtype=VLSF_BACKVOL;
               voltype=VOLTYPE_BK;
          } else {
               vtype=0;
               voltype=-1;
          }
          for (srv=0;srv < u.u.nServers;srv++) {
               if ((u.u.serverFlags[srv] & vtype) == 0)
                    continue;
               if ((u.u.serverFlags[srv] & VLSERVER_FLAG_UUID) == 0)
                    server=afs_serverbyaddr(cell, u.u.serverNumber[srv].time_low);
               else
                    server=afs_serverbyid(cell, &u.u.serverNumber[srv]);
               if (!server)
                    continue;
               ret->servers[ret->nservers++]=server->index;
          }
          break;
     case 1:
          if (id == u.n.volumeId[RWVOL]) {
               vtype=VLSF_RWVOL;
               voltype=VOLTYPE_RW;
          } else if (id == u.n.volumeId[ROVOL]) {
               vtype=VLSF_ROVOL;
               voltype=VOLTYPE_RO;
          } else if (id == u.n.volumeId[BACKVOL]) {
               vtype=VLSF_BACKVOL;
               voltype=VOLTYPE_BK;
          } else {
               vtype=0;
               voltype=-1;
          }
          for (srv=0;srv < u.n.nServers;srv++) {
               if ((u.n.serverFlags[srv] & vtype) == 0)
                    continue;
               server=afs_serverbyaddr(cell, u.n.serverNumber[srv]);
               if (!server)
                    continue;
               ret->servers[ret->nservers++]=server->index;
          }
          break;
     case 2:
          if (id == u.o.volumeId[RWVOL]) {
               vtype=VLSF_RWVOL;
               voltype=VOLTYPE_RW;
          } else if (id == u.o.volumeId[ROVOL]) {
               vtype=VLSF_ROVOL;
               voltype=VOLTYPE_RO;
          } else if (id == u.o.volumeId[BACKVOL]) {
               vtype=VLSF_BACKVOL;
               voltype=VOLTYPE_BK;
          } else {
               vtype=0;
               voltype=-1;
          }
          for (srv=0;srv < u.o.nServers;srv++) {
               if ((u.o.serverFlags[srv] & vtype) == 0)
                    continue;
               server=afs_serverbyaddr(cell, u.o.serverNumber[srv]);
               if (!server)
                    continue;
               ret->servers[ret->nservers++]=server->index;
          }
          break;
     }
     ret->voltype=voltype;
     server=afs_serverbyindex(ret->servers[0]);
     if (server)
       i.s_addr=server->addrs[0];
     else
       i.s_addr=0;
     afscp_dprintf(("New volume BYID %s (%lu) on %s (%d)\n", ret->name, ret->id, inet_ntoa(i), ret->servers[0]));
     s=tsearch(&key, &cell->volsbyid, icompare);
     if (s)
       *(struct afs_volume **)s=ret;
     strcpy(key.name, ret->name);
     s=tsearch(&key, &cell->volsbyname, ncompare);
     if (s)
       *(struct afs_volume **)s=ret;
     return ret;
}

