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
#include <afs/volint.h>
#include <afs/cellconfig.h>
#ifndef AFSCONF_CLIENTNAME
#include <afs/dirpath.h>
#define AFSCONF_CLIENTNAME AFSDIR_CLIENT_ETC_DIRPATH
#endif
#include <stdlib.h>
#include <string.h>
#include <rx/rx.h>
#include "afscp.h"
#include "afscp_internal.h"

static int afs_ncells=0,afs_cellsalloced=0;
static struct afs_cell *allcells=NULL;
static int afs_nservers=0,afs_srvsalloced=0;
static struct afs_server **allservers=NULL;
static char *defcell;
int afs_errno=0;

struct afs_cell *afs_cellbyid(int id)
{
     if (id >= afs_ncells)
          return NULL;
     return &allcells[id];
}

struct afs_cell *afs_cellbyname(const char *cellname)
{
     int i;
     struct afs_cell *newlist, *thecell;

     for (i=0;i<afs_ncells;i++) {
          if (!strcmp(allcells[i].name, cellname))
               return &allcells[i];
     }
     if (afs_ncells >= afs_cellsalloced) {
          if (afs_cellsalloced)
               afs_cellsalloced = afs_cellsalloced *2;
          else
               afs_cellsalloced = 4;
          newlist=realloc(allcells, afs_cellsalloced * sizeof(struct afs_cell));
          if (!newlist)
               return NULL;
          allcells=newlist;
     }
     thecell=&allcells[afs_ncells];
     memset(thecell, 0, sizeof(struct afs_cell));
     strcpy(thecell->name, cellname);
     if (_GetSecurityObject(thecell))
       return NULL;
     if (_GetVLservers(thecell)) {
       RXS_Close(thecell->security);
       return NULL;
     }
     thecell->id=afs_ncells++;
     return thecell;
}
struct afs_cell *afs_defaultcell(void)
{
     struct afsconf_dir *dir;
     char localcell[MAXCELLCHARS+1];
     int code;


     if (defcell) {
          return afs_cellbyname(defcell);
     }

     dir=afsconf_Open(AFSCONF_CLIENTNAME);
     if (!dir) {
          afs_errno=AFSCONF_NODB;
          return NULL;
     }
     code=afsconf_GetLocalCell(dir, localcell, MAXCELLCHARS);
     if (code){
          afs_errno=code;
          return NULL;
     }
     afsconf_Close(dir);
     return afs_cellbyname(localcell);
}

int afs_setdefaultcell(const char *cellname) {
     struct afs_cell *this;
     char *newdefcell;
     if (!cellname) {
          if (defcell)
               free(defcell);
          defcell=NULL;
          return 0;
     }

     this=afs_cellbyname(cellname);
     if (!this)
          return -1;
     newdefcell=strdup(cellname);
     if (!newdefcell)
	return -1;
     if (defcell)
          free(defcell);
     defcell = newdefcell;
     return 0;
}

int afs_cellid(struct afs_cell *cell) {
	return cell->id;
}

static void _xdr_free(bool_t (*fn)(XDR *xdrs, void *obj), void *obj) {
     XDR xdrs;
     xdrs.x_op=XDR_FREE;
     fn(&xdrs, obj);
}
static bool_t _xdr_bulkaddrs(XDR *xdrs, void *objp) {
     return xdr_bulkaddrs(xdrs, objp);
}


struct afs_server *afs_serverbyid(struct afs_cell *thecell, afsUUID *u)
{
     /* impliment uniquifiers? */
     int i;
     struct afs_server **newlist;
     struct afs_server **newall;
     struct afs_server *ret;
     afsUUID tmp;
     bulkaddrs addrs;
     struct ListAddrByAttributes attrs;
     afs_int32 nentries, uniq;

     char s[512];

     afsUUID_to_string(u, s, 511);
     afscp_dprintf(("GetServerByID %s\n", s));

     for (i=0;i<thecell->nservers;i++) {
          if (afs_uuid_equal(&thecell->fsservers[i]->id, u))
               return thecell->fsservers[i];
     }

     if (thecell->nservers >= thecell->srvsalloced) {
          if (thecell->srvsalloced)
               thecell->srvsalloced = thecell->srvsalloced *2;
          else
               thecell->srvsalloced = 4;
          newlist=realloc(thecell->fsservers,
                          thecell->srvsalloced * sizeof(struct afs_server *));
          if (!newlist)
               return NULL;
          thecell->fsservers=newlist;
     }
     ret=malloc(sizeof(struct afs_server));
     if (!ret)
          return NULL;
     thecell->fsservers[thecell->nservers]=ret;
     memmove(&ret->id, u, sizeof(afsUUID));
     ret->cell=thecell->id;
     memset(&tmp, 0, sizeof(tmp));
     memset(&addrs, 0, sizeof(addrs));
     memset(&attrs, 0, sizeof(attrs));
     attrs.Mask = VLADDR_UUID;
     memmove(&attrs.uuid, u, sizeof(afsUUID));

     if (ubik_VL_GetAddrsU(thecell->vlservers, 0, &attrs, &tmp,
                      &uniq, &nentries, &addrs))
          return NULL;
     if (nentries > MAXADDRS) {
          nentries=MAXADDRS;
          /* XXX I don't want to do *that* much dynamic allocation */
          abort();
     }

     ret->naddrs=nentries;
     for (i=0; i< nentries;i++) {
          ret->addrs[i]=htonl(addrs.bulkaddrs_val[i]);
	  ret->conns[i]=rx_NewConnection(ret->addrs[i],
					 htons(AFSCONF_FILEPORT),
					 1, thecell->security,
					 thecell->scindex);
     }
     _xdr_free(_xdr_bulkaddrs, &addrs);
     thecell->nservers++;

     if (afs_nservers >= afs_srvsalloced) {
          if (afs_srvsalloced)
               afs_srvsalloced = afs_srvsalloced *2;
          else
               afs_srvsalloced = 4;
          newall=realloc(allservers,
                          afs_srvsalloced * sizeof(struct afs_server *));
          if (!newall)
               return ret;
          allservers=newall;
     }
     ret->index=afs_nservers;
     allservers[afs_nservers++]=ret;
     return ret;
}

struct afs_server *afs_serverbyaddr(struct afs_cell *thecell, afs_uint32 addr)
{
     /* impliment uniquifiers? */
     int i,j;
     struct afs_server **newlist;
     struct afs_server **newall;
     struct afs_server *ret;
     afsUUID uuid;
     bulkaddrs addrs;
     struct ListAddrByAttributes attrs;
     afs_int32 nentries, code, uniq;


     for (i=0;i<thecell->nservers;i++) {
          ret=thecell->fsservers[i];
          for (j=0;j<ret->naddrs;j++)
               if (ret->addrs[j] == htonl(addr))
                    return ret;
     }

     if (thecell->nservers >= thecell->srvsalloced) {
          if (thecell->srvsalloced)
               thecell->srvsalloced = thecell->srvsalloced *2;
          else
               thecell->srvsalloced = 4;
          newlist=realloc(thecell->fsservers,
                          thecell->srvsalloced * sizeof(struct afs_server));
          if (!newlist)
               return NULL;
          thecell->fsservers=newlist;
     }
     ret=malloc(sizeof(struct afs_server));
     if (!ret)
          return NULL;
     thecell->fsservers[thecell->nservers]=ret;
     ret->cell=thecell->id;
     memset(&uuid, 0, sizeof(uuid));
     memset(&addrs, 0, sizeof(addrs));
     memset(&attrs, 0, sizeof(attrs));
     attrs.Mask = VLADDR_IPADDR;
     attrs.ipaddr=addr;


     if ((code=ubik_VL_GetAddrsU(thecell->vlservers, 0, &attrs, &uuid,
                      &uniq, &nentries, &addrs))) {
       memset(&ret->id, 0, sizeof(uuid));
       ret->naddrs=1;
       ret->addrs[0]=htonl(addr);
       ret->conns[0]=rx_NewConnection(ret->addrs[0],
				      htons(AFSCONF_FILEPORT),
				      1, thecell->security,
				      thecell->scindex);
     } else {
       char s[512];

       afsUUID_to_string(&uuid, s, 511);
       afscp_dprintf(("GetServerByAddr 0x%x -> uuid %s\n", addr, s));

       if (nentries > MAXADDRS) {
	 nentries=MAXADDRS;
	 /* XXX I don't want to do *that* much dynamic allocation */
	 abort();
       }
       memmove(&ret->id, &uuid, sizeof(afsUUID));

       ret->naddrs=nentries;
       for (i=0; i< nentries;i++) {
	 ret->addrs[i]=htonl(addrs.bulkaddrs_val[i]);
	 ret->conns[i]=rx_NewConnection(ret->addrs[i],
					htons(AFSCONF_FILEPORT),
					1, thecell->security,
					thecell->scindex);
       }
       _xdr_free(_xdr_bulkaddrs, &addrs);
     }

     thecell->nservers++;
     if (afs_nservers >= afs_srvsalloced) {
          if (afs_srvsalloced)
               afs_srvsalloced = afs_srvsalloced *2;
          else
               afs_srvsalloced = 4;
          newall=realloc(allservers,
                          afs_srvsalloced * sizeof(struct afs_server *));
          if (!newall)
               return ret;
          allservers=newall;
     }
     ret->index=afs_nservers;
     allservers[afs_nservers++]=ret;
     return ret;
}

struct afs_server *afs_anyserverbyaddr(afs_uint32 addr)
{
     /* no idea what this means: "impliment uniquifiers?" */
     int i,j;
     struct afs_server *ret;

     for (i=0;i<afs_nservers;i++) {
          ret=allservers[i];
          for (j=0;j<ret->naddrs;j++)
               if (ret->addrs[j] == htonl(addr))
                    return ret;
     }
     return NULL;
}

struct afs_server *afs_serverbyindex(int i)
{

     if (i>= afs_nservers)
          return NULL;
     return allservers[i];
}

struct rx_connection *afs_serverconnection(const struct afs_server *srv, int i) {
  if (i >= srv->naddrs)
    return NULL;
  return srv->conns[i];
}
