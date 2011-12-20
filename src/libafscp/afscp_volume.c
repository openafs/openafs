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

#ifdef HAVE_SEARCH_H
#include <search.h>
#else
#include "afscp_search.h"
#endif

#include <afs/vlserver.h>
#include <afs/vldbint.h>
#include <afs/volint.h>
#include "afscp.h"
#include "afscp_internal.h"

static int
icompare(const void *pa, const void *pb)
{
    const struct afscp_volume *va = pa, *vb = pb;

    if (va->id > vb->id)
	return 1;
    if (va->id < vb->id)
	return -1;
    return 0;
}

static int
ncompare(const void *pa, const void *pb)
{
    const struct afscp_volume *va = pa, *vb = pb;

    if (va->voltype > vb->voltype)
	return 1;
    if (vb->voltype < va->voltype)
	return -1;
    return strcmp(va->name, vb->name);
}

union allvldbentry {
    struct uvldbentry u;
    struct nvldbentry n;
    struct vldbentry o;
};

struct afscp_volume *
afscp_VolumeByName(struct afscp_cell *cell, const char *vname,
		   afs_int32 intype)
{
    union allvldbentry u;
    struct afscp_volume *ret, key;
    struct afscp_server *server;
    afs_int32 code, vtype, type, srv;
    void *s;
#ifdef AFSCP_DEBUG
    struct in_addr i;
#endif
    if (intype == RWVOL)
	vtype = VLSF_RWVOL;
    else if (intype == ROVOL)
	vtype = VLSF_ROVOL;
    else if (intype == BACKVOL)
	vtype = VLSF_BACKVOL;
    else {
	afscp_errno = EINVAL;
	return NULL;
    }

    memset(&key, 0, sizeof(key));
    strlcpy(key.name, vname, sizeof(key.name));
    key.voltype = vtype;
    s = tfind(&key, &cell->volsbyname, ncompare);
    if (s) {
	ret = *(struct afscp_volume **)s;
	return ret;
    }

    type = 0;
    code = ubik_VL_GetEntryByNameU(cell->vlservers, 0, (char *)vname, &u.u);
    if (code == RXGEN_OPCODE) {
	type = 1;
	code =
	    ubik_VL_GetEntryByNameN(cell->vlservers, 0, (char *)vname, &u.n);
	if (code == RXGEN_OPCODE) {
	    type = 2;
	    code = ubik_VL_GetEntryByNameO(cell->vlservers, 0, (char *)vname,
					   &u.o);
	}
    }
    if (code != 0) {
	afscp_errno = code;
	return NULL;
    }
    ret = malloc(sizeof(struct afscp_volume));
    if (ret == NULL) {
	afscp_errno = ENOMEM;
	return NULL;
    }
    memset(ret, 0, sizeof(struct afscp_volume));
    strlcpy(ret->name, u.u.name, sizeof(ret->name));
    ret->nservers = 0;
    ret->cell = cell;
    switch (type) {
    case 0:
	ret->id = u.u.volumeId[intype];
	for (srv = 0; srv < u.u.nServers; srv++) {
	    if ((u.u.serverFlags[srv] & vtype) == 0)
		continue;
	    afs_dprintf(("uvldbentry server %d flags: %x\n", srv,
			 u.u.serverFlags[srv]));

	    if ((u.u.serverFlags[srv] & VLSERVER_FLAG_UUID) == 0)
		server =
		    afscp_ServerByAddr(cell, u.u.serverNumber[srv].time_low);
	    else
		server = afscp_ServerById(cell, &u.u.serverNumber[srv]);
	    if (!server)
		continue;
	    ret->servers[ret->nservers++] = server->index;
	}
	break;
    case 1:
	ret->id = u.n.volumeId[intype];
	for (srv = 0; srv < u.n.nServers; srv++) {
	    if ((u.n.serverFlags[srv] & vtype) == 0)
		continue;
	    server = afscp_ServerByAddr(cell, u.n.serverNumber[srv]);
	    if (!server)
		continue;
	    ret->servers[ret->nservers++] = server->index;
	}
	break;
    case 2:
	ret->id = u.o.volumeId[intype];
	for (srv = 0; srv < u.o.nServers; srv++) {
	    if ((u.o.serverFlags[srv] & vtype) == 0)
		continue;
	    server = afscp_ServerByAddr(cell, u.o.serverNumber[srv]);
	    if (!server)
		continue;
	    ret->servers[ret->nservers++] = server->index;
	}
	break;
    }
    if (!ret->nservers || !ret->id) {
	free(ret);
	return NULL;
    }

    ret->voltype = intype;
    server = afscp_ServerByIndex(ret->servers[0]);
#ifdef AFSCP_DEBUG
    if (server != NULL)
	i.s_addr = server->addrs[0];
    else
	i.s_addr = 0;
#endif
    afs_dprintf(("New volume BYNAME %s (%lu) on %s (%d)\n", ret->name,
		 afs_printable_uint32_lu(ret->id),
		 inet_ntoa(i), ret->servers[0]));
    s = tsearch(&key, &cell->volsbyname, ncompare);
    if (s)
	*(struct afscp_volume **)s = ret;
    key.id = ret->id;
    s = tsearch(&key, &cell->volsbyid, icompare);
    if (s)
	*(struct afscp_volume **)s = ret;
    return ret;
}

struct afscp_volume *
afscp_VolumeById(struct afscp_cell *cell, afs_uint32 id)
{
    union allvldbentry u;
    struct afscp_volume *ret, key;
    struct afscp_server *server;
    afs_int32 code, vtype, type, srv;
    int voltype = -1;
    char idbuffer[16];
    void *s;
#ifdef AFSCP_DEBUG
    struct in_addr i;
#endif

    memset(&key, 0, sizeof(key));
    key.id = id;
    s = tfind(&key, &cell->volsbyid, icompare);
    if (s) {
	ret = *(struct afscp_volume **)s;
	return ret;
    }

    snprintf(idbuffer, sizeof(idbuffer), "%lu", afs_printable_uint32_lu(id));
    type = 0;
    code = ubik_VL_GetEntryByNameU(cell->vlservers, 0, idbuffer, &u.u);
    if (code == RXGEN_OPCODE) {
	type = 1;
	code = ubik_VL_GetEntryByIDN(cell->vlservers, 0, id, -1, &u.n);
	if (code == RXGEN_OPCODE) {
	    type = 2;
	    code = ubik_VL_GetEntryByID(cell->vlservers, 0, id, -1, &u.o);
	}
    }
    if (code != 0) {
	afscp_errno = code;
	return NULL;
    }
    ret = malloc(sizeof(struct afscp_volume));
    if (ret == NULL) {
	afscp_errno = ENOMEM;
	return NULL;
    }
    memset(ret, 0, sizeof(struct afscp_volume));
    strlcpy(ret->name, u.u.name, sizeof(ret->name));
    ret->nservers = 0;
    ret->cell = cell;

    switch (type) {
    case 0:
	if (id == u.u.volumeId[RWVOL]) {
	    vtype = VLSF_RWVOL;
	    voltype = RWVOL;
	} else if (id == u.u.volumeId[ROVOL]) {
	    vtype = VLSF_ROVOL;
	    voltype = ROVOL;
	} else if (id == u.u.volumeId[BACKVOL]) {
	    vtype = VLSF_BACKVOL;
	    voltype = BACKVOL;
	} else {
	    vtype = 0;
	    voltype = -1;
	}
	for (srv = 0; srv < u.u.nServers; srv++) {
	    if ((u.u.serverFlags[srv] & vtype) == 0)
		continue;
	    if ((u.u.serverFlags[srv] & VLSERVER_FLAG_UUID) == 0)
		server =
		    afscp_ServerByAddr(cell, u.u.serverNumber[srv].time_low);
	    else
		server = afscp_ServerById(cell, &u.u.serverNumber[srv]);
	    if (!server)
		continue;
	    ret->servers[ret->nservers++] = server->index;
	}
	break;
    case 1:
	if (id == u.n.volumeId[RWVOL]) {
	    vtype = VLSF_RWVOL;
	    voltype = RWVOL;
	} else if (id == u.n.volumeId[ROVOL]) {
	    vtype = VLSF_ROVOL;
	    voltype = ROVOL;
	} else if (id == u.n.volumeId[BACKVOL]) {
	    vtype = VLSF_BACKVOL;
	    voltype = BACKVOL;
	} else {
	    vtype = 0;
	    voltype = -1;
	}
	for (srv = 0; srv < u.n.nServers; srv++) {
	    if ((u.n.serverFlags[srv] & vtype) == 0)
		continue;
	    server = afscp_ServerByAddr(cell, u.n.serverNumber[srv]);
	    if (server == NULL)
		continue;
	    ret->servers[ret->nservers++] = server->index;
	}
	break;
    case 2:
	if (id == u.o.volumeId[RWVOL]) {
	    vtype = VLSF_RWVOL;
	    voltype = RWVOL;
	} else if (id == u.o.volumeId[ROVOL]) {
	    vtype = VLSF_ROVOL;
	    voltype = ROVOL;
	} else if (id == u.o.volumeId[BACKVOL]) {
	    vtype = VLSF_BACKVOL;
	    voltype = BACKVOL;
	} else {
	    vtype = 0;
	    voltype = -1;
	}
	for (srv = 0; srv < u.o.nServers; srv++) {
	    if ((u.o.serverFlags[srv] & vtype) == 0)
		continue;
	    server = afscp_ServerByAddr(cell, u.o.serverNumber[srv]);
	    if (server == NULL)
		continue;
	    ret->servers[ret->nservers++] = server->index;
	}
	break;
    }
    ret->voltype = voltype;
    server = afscp_ServerByIndex(ret->servers[0]);
#ifdef AFSCP_DEBUG
    if (server)
	i.s_addr = server->addrs[0];
    else
	i.s_addr = 0;
#endif
    afs_dprintf(("New volume BYID %s (%lu) on %s (%d)\n", ret->name,
		 afs_printable_uint32_lu(ret->id), inet_ntoa(i),
		 ret->servers[0]));
    s = tsearch(&key, &cell->volsbyid, icompare);
    if (s)
	*(struct afscp_volume **)s = ret;
    strlcpy(key.name, ret->name, sizeof(key.name));
    s = tsearch(&key, &cell->volsbyname, ncompare);
    if (s)
	*(struct afscp_volume **)s = ret;
    return ret;
}
