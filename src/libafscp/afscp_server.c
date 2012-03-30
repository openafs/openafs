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
#include <afs/volint.h>
#include <afs/cellconfig.h>
#ifndef AFSCONF_CLIENTNAME
#include <afs/dirpath.h>
#define AFSCONF_CLIENTNAME AFSDIR_CLIENT_ETC_DIRPATH
#endif
#include <rx/rx.h>
#ifdef HAVE_KERBEROS
# define KERBEROS_APPLE_DEPRECATED(x)
# include <krb5.h>
#endif
#include "afscp.h"
#include "afscp_internal.h"

static int afscp_ncells = 0, afscp_cellsalloced = 0;
static struct afscp_cell *allcells = NULL;
static int afscp_nservers = 0, afscp_srvsalloced = 0;
static struct afscp_server **allservers = NULL;
static char *defcell = NULL;
static char *defrealm = NULL;
int afscp_errno = 0;

void
afscp_FreeAllCells(void)
{
    int i;

    if (allcells == NULL)
	return;

    for (i = 0; i < afscp_ncells; i++) {
	if (allcells[i].realm != NULL)
	    free(allcells[i].realm);
	if (allcells[i].fsservers != NULL)
	    free(allcells[i].fsservers);
    }

    free(allcells);
    allcells = NULL;
    afscp_ncells = 0;
    afscp_cellsalloced = 0;
}

void
afscp_FreeAllServers(void)
{
    if (allservers == NULL)
	return;
    free(allservers);
    allservers = NULL;
    afscp_nservers = 0;
    afscp_srvsalloced = 0;
}

struct afscp_cell *
afscp_CellById(int id)
{
    if (id >= afscp_ncells || id < 0)
	return NULL;
    return &allcells[id];
}

struct afscp_cell *
afscp_CellByName(const char *cellname, const char *realmname)
{
    int i;
    struct afscp_cell *newlist, *thecell;

    if (cellname == NULL) {
	return NULL;
    }
    for (i = 0; i < afscp_ncells; i++) {
	if (strcmp(allcells[i].name, cellname) == 0) {
	    return &allcells[i];
	}
    }
    if (afscp_ncells >= afscp_cellsalloced) {
	if (afscp_cellsalloced)
	    afscp_cellsalloced = afscp_cellsalloced * 2;
	else
	    afscp_cellsalloced = 4;
	newlist =
	    realloc(allcells, afscp_cellsalloced * sizeof(struct afscp_cell));
	if (newlist == NULL) {
	    return NULL;
	}
	allcells = newlist;
    }
    thecell = &allcells[afscp_ncells];
    memset(thecell, 0, sizeof(struct afscp_cell));
    strlcpy(thecell->name, cellname, sizeof(thecell->name));
    if (realmname != NULL) {
	thecell->realm = strdup(realmname);
    } else {
	thecell->realm = NULL;
    }
    if (_GetSecurityObject(thecell)) {
	return NULL;
    }
    if (_GetVLservers(thecell)) {
	RXS_Close(thecell->security);
	return NULL;
    }
    thecell->id = afscp_ncells++;
    return thecell;
}

struct afscp_cell *
afscp_DefaultCell(void)
{
    struct afsconf_dir *dir;
    char localcell[MAXCELLCHARS + 1];
    int code;

    if (defcell) {
	return afscp_CellByName(defcell, defrealm);
    }

    dir = afsconf_Open(AFSCONF_CLIENTNAME);
    if (dir == NULL) {
	afscp_errno = AFSCONF_NODB;
	return NULL;
    }
    code = afsconf_GetLocalCell(dir, localcell, MAXCELLCHARS);
    if (code != 0) {
	afscp_errno = code;
	return NULL;
    }
    afsconf_Close(dir);
    return afscp_CellByName(localcell, defrealm);
}

int
afscp_SetDefaultRealm(const char *realmname)
{
    char *newdefrealm;

#ifdef HAVE_KERBEROS
    /* krb5_error_code k5ec; */
    krb5_context k5con;
    int code;

    if (realmname == NULL) {
	if (defrealm != NULL)
	    free(defrealm);
	defrealm = NULL;
	return 0;
    }

    code = krb5_init_context(&k5con);	/* see aklog.c main() */
    if (code != 0) {
	return -1;
    }
    /* k5ec = */
    krb5_set_default_realm(k5con, realmname);
    /* if (k5ec != KRB5KDC_ERR_NONE) {
     * com_err("libafscp", k5ec, "k5ec = %d (compared to KRB5KDC_ERR_NONE = %d)", k5ec, KRB5KDC_ERR_NONE);
     * return -1;
     * } */
    /* krb5_set_default_realm() is returning 0 on success, not KRB5KDC_ERR_NONE */
#endif /* HAVE_KERBEROS */

    newdefrealm = strdup(realmname);
    if (newdefrealm == NULL) {
	return -1;
    }
    if (defrealm != NULL)
	free(defrealm);
    defrealm = newdefrealm;
    return 0;
}

int
afscp_SetDefaultCell(const char *cellname)
{
    struct afscp_cell *this;
    char *newdefcell;
    if (cellname == NULL) {
	if (defcell != NULL)
	    free(defcell);
	defcell = NULL;
	return 0;
    }

    this = afscp_CellByName(cellname, defrealm);
    if (this == NULL) {
	return -1;
    }
    newdefcell = strdup(cellname);
    if (newdefcell == NULL) {
	return -1;
    }
    if (defcell != NULL)
	free(defcell);
    defcell = newdefcell;
    return 0;
}

int
afscp_CellId(struct afscp_cell *cell)
{
    if (cell == NULL)
	return -1;
    return cell->id;
}

static void
_xdr_free(bool_t(*fn) (XDR * xdrs, void *obj), void *obj)
{
    XDR xdrs;
    xdrs.x_op = XDR_FREE;
    fn(&xdrs, obj);
}

static bool_t
_xdr_bulkaddrs(XDR * xdrs, void *objp)
{
    return xdr_bulkaddrs(xdrs, objp);
}

/* takes server in host byte order */
struct afscp_server *
afscp_ServerById(struct afscp_cell *thecell, afsUUID * u)
{
    /* impliment uniquifiers? */
    int i, code;
    struct afscp_server **newlist;
    struct afscp_server **newall;
    struct afscp_server *ret = NULL;
    afsUUID tmp;
    bulkaddrs addrs;
    struct ListAddrByAttributes attrs;
    afs_int32 nentries, uniq;
    char s[512];
    afsUUID_to_string(u, s, 511);
    afs_dprintf(("GetServerByID %s\n", s));

    for (i = 0; i < thecell->nservers; i++) {
	if (afs_uuid_equal(&thecell->fsservers[i]->id, u)) {
	    return thecell->fsservers[i];
	}
    }

    if (thecell->nservers >= thecell->srvsalloced) {
	if (thecell->srvsalloced)
	    thecell->srvsalloced = thecell->srvsalloced * 2;
	else
	    thecell->srvsalloced = 4;
	newlist = realloc(thecell->fsservers,
			  thecell->srvsalloced *
			  sizeof(struct afscp_server *));
	if (newlist == NULL) {
	    return NULL;
	}
	thecell->fsservers = newlist;
    }
    ret = malloc(sizeof(struct afscp_server));
    if (ret == NULL) {
	return NULL;
    }
    memset(ret, 0, sizeof(struct afscp_server));
    thecell->fsservers[thecell->nservers] = ret;
    memmove(&ret->id, u, sizeof(afsUUID));
    ret->cell = thecell->id;
    memset(&tmp, 0, sizeof(tmp));
    memset(&addrs, 0, sizeof(addrs));
    memset(&attrs, 0, sizeof(attrs));
    attrs.Mask = VLADDR_UUID;
    memmove(&attrs.uuid, u, sizeof(afsUUID));

    code = ubik_VL_GetAddrsU(thecell->vlservers, 0, &attrs, &tmp,
			     &uniq, &nentries, &addrs);
    if (code != 0) {
	return NULL;
    }
    if (nentries > AFS_MAXHOSTS) {
	nentries = AFS_MAXHOSTS;
	/* XXX I don't want to do *that* much dynamic allocation */
	abort();
    }

    ret->naddrs = nentries;
    for (i = 0; i < nentries; i++) {
	ret->addrs[i] = htonl(addrs.bulkaddrs_val[i]);
	ret->conns[i] = rx_NewConnection(ret->addrs[i],
					 htons(AFSCONF_FILEPORT),
					 1, thecell->security,
					 thecell->scindex);
    }
    _xdr_free(_xdr_bulkaddrs, &addrs);
    thecell->nservers++;

    if (afscp_nservers >= afscp_srvsalloced) {
	if (afscp_srvsalloced)
	    afscp_srvsalloced = afscp_srvsalloced * 2;
	else
	    afscp_srvsalloced = 4;
	newall = realloc(allservers,
			 afscp_srvsalloced * sizeof(struct afscp_server *));
	if (newall == NULL) {
	    return ret;
	}
	allservers = newall;
    }
    ret->index = afscp_nservers;
    allservers[afscp_nservers++] = ret;
    return ret;
}

/* takes server in host byte order */
struct afscp_server *
afscp_ServerByAddr(struct afscp_cell *thecell, afs_uint32 addr)
{
    /* implement uniquifiers? */
    int i, j;
    struct afscp_server **newlist;
    struct afscp_server **newall;
    struct afscp_server *ret = NULL;
    afsUUID uuid;
    bulkaddrs addrs;
    struct ListAddrByAttributes attrs;
    afs_int32 nentries, code, uniq;

    if (thecell == NULL)
	return ret;		/* cannot continue without thecell */

    for (i = 0; i < thecell->nservers; i++) {
	ret = thecell->fsservers[i];
	for (j = 0; j < ret->naddrs; j++)
	    if (ret->addrs[j] == htonl(addr)) {
		return ret;
	    }
    }

    if (thecell->nservers >= thecell->srvsalloced) {
	if (thecell->srvsalloced)
	    thecell->srvsalloced = thecell->srvsalloced * 2;
	else
	    thecell->srvsalloced = 4;
	newlist = realloc(thecell->fsservers,
			  thecell->srvsalloced * sizeof(struct afscp_server));
	if (newlist == NULL) {
	    return NULL;
	}
	thecell->fsservers = newlist;
    }
    ret = malloc(sizeof(struct afscp_server));
    if (ret == NULL) {
	return NULL;
    }
    memset(ret, 0, sizeof(struct afscp_server));
    thecell->fsservers[thecell->nservers] = ret;
    ret->cell = thecell->id;
    memset(&uuid, 0, sizeof(uuid));
    memset(&addrs, 0, sizeof(addrs));
    memset(&attrs, 0, sizeof(attrs));
    attrs.Mask = VLADDR_IPADDR;
    attrs.ipaddr = addr;

    code = ubik_VL_GetAddrsU(thecell->vlservers, 0, &attrs, &uuid,
			     &uniq, &nentries, &addrs);
    if (code != 0) {
	memset(&ret->id, 0, sizeof(uuid));
	ret->naddrs = 1;
	ret->addrs[0] = htonl(addr);
	ret->conns[0] = rx_NewConnection(ret->addrs[0],
					 htons(AFSCONF_FILEPORT),
					 1, thecell->security,
					 thecell->scindex);
    } else {
	char s[512];

	afsUUID_to_string(&uuid, s, 511);
	afs_dprintf(("GetServerByAddr 0x%x -> uuid %s\n", addr, s));

	if (nentries > AFS_MAXHOSTS) {
	    nentries = AFS_MAXHOSTS;
	    /* XXX I don't want to do *that* much dynamic allocation */
	    abort();
	}
	memmove(&ret->id, &uuid, sizeof(afsUUID));

	ret->naddrs = nentries;
	for (i = 0; i < nentries; i++) {
	    ret->addrs[i] = htonl(addrs.bulkaddrs_val[i]);
	    ret->conns[i] = rx_NewConnection(ret->addrs[i],
					     htons(AFSCONF_FILEPORT),
					     1, thecell->security,
					     thecell->scindex);
	}
	_xdr_free(_xdr_bulkaddrs, &addrs);
    }

    thecell->nservers++;
    if (afscp_nservers >= afscp_srvsalloced) {
	if (afscp_srvsalloced)
	    afscp_srvsalloced = afscp_srvsalloced * 2;
	else
	    afscp_srvsalloced = 4;
	newall = realloc(allservers,
			 afscp_srvsalloced * sizeof(struct afscp_server *));
	if (newall == NULL) {
	    return ret;
	}
	allservers = newall;
    }
    ret->index = afscp_nservers;
    allservers[afscp_nservers++] = ret;
    return ret;
}

/* takes server in host byte order */
struct afscp_server *
afscp_AnyServerByAddr(afs_uint32 addr)
{
    /* implement uniquifiers? */
    int i, j;
    struct afscp_server *ret = NULL;

    if (allservers == NULL)
	return ret;
    for (i = 0; i < afscp_nservers; i++) {
	ret = allservers[i];
	for (j = 0; j < ret->naddrs; j++)
	    if (ret->addrs[j] == htonl(addr)) {
		return ret;
	    }
    }
    return NULL;
}

/* takes server in host byte order */
struct afscp_server *
afscp_ServerByIndex(int i)
{
    if (i >= afscp_nservers || i < 0)
	return NULL;
    return allservers[i];
}

struct rx_connection *
afscp_ServerConnection(const struct afscp_server *srv, int i)
{
    if (srv == NULL)
	return NULL;
    if (i >= srv->naddrs || i < 0)
	return NULL;
    return srv->conns[i];
}
