/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif /* AFS_NT40_ENV */
#include <sys/stat.h>
#ifdef AFS_AIX_ENV
#include <sys/statfs.h>
#endif
#include <errno.h>
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/nfs.h>
#include <afs/vlserver.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/cmd.h>
#include <rx/rxkad.h>
#include "volser.h"
#include "volint.h"
#include "lockdata.h"

struct ubik_client *cstruct;
extern int VL_CreateEntry(), VL_CreateEntryN();
extern int VL_GetEntryByID(), VL_GetEntryByIDN();
extern int VL_GetEntryByNameO(), VL_GetEntryByNameN();
extern int VL_ReplaceEntry(), VL_ReplaceEntryN();
extern int VL_ListAttributes(), VL_ListAttributesN(), VL_ListAttributesN2();

static void ovlentry_to_nvlentry(oentryp, nentryp)
    struct vldbentry *oentryp;
    struct nvldbentry *nentryp;
{
    register int i;

    bzero(nentryp, sizeof(struct nvldbentry));
    strncpy(nentryp->name, oentryp->name, sizeof(nentryp->name));
    for (i=0; i < oentryp->nServers; i++) {
	nentryp->serverNumber[i] = oentryp->serverNumber[i];
	nentryp->serverPartition[i] = oentryp->serverPartition[i];
	nentryp->serverFlags[i] = oentryp->serverFlags[i];
    }
    nentryp->nServers = oentryp->nServers;
    for (i=0; i<MAXTYPES; i++)
	nentryp->volumeId[i] = oentryp->volumeId[i];
    nentryp->cloneId = oentryp->cloneId;
    nentryp->flags = oentryp->flags;
}

static nvlentry_to_ovlentry(nentryp, oentryp)
    struct nvldbentry *nentryp;
    struct vldbentry *oentryp;
{
    register int i;

    bzero(oentryp, sizeof(struct vldbentry));
    strncpy(oentryp->name, nentryp->name, sizeof(oentryp->name));
    if (nentryp->nServers > OMAXNSERVERS) {
	/*
	 * The alternative is to store OMAXSERVERS but it's always better
	 * to know what's going on...
	 */
	return VL_BADSERVER;
    }
    for (i=0; i < nentryp->nServers; i++) {
	oentryp->serverNumber[i] = nentryp->serverNumber[i];
	oentryp->serverPartition[i] = nentryp->serverPartition[i];
	oentryp->serverFlags[i] = nentryp->serverFlags[i];
    }
    oentryp->nServers = i;
    for (i=0; i<MAXTYPES; i++)
	oentryp->volumeId[i] = nentryp->volumeId[i];
    oentryp->cloneId = nentryp->cloneId;
    oentryp->flags = nentryp->flags;
    return 0;
}

static int newvlserver=0;

VLDB_CreateEntry(entryp)
    struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code, (*nproc)();

    if (newvlserver == 1) {
tryold:
	code = nvlentry_to_ovlentry(entryp, &oentry);
	if (code)
	    return code;
	code = ubik_Call(VL_CreateEntry, cstruct, 0, &oentry);
	return code;
    }
    code = ubik_Call(VL_CreateEntryN, cstruct, 0, entryp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_GetEntryByID(volid, voltype, entryp)
    afs_int32 volid, voltype;
    struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code, (*nproc)();

    if (newvlserver == 1) {
tryold:
	code = ubik_Call(VL_GetEntryByID, cstruct, 0, volid, voltype, &oentry);
	if (!code)
	    ovlentry_to_nvlentry(&oentry, entryp);
	return code;
    }
    code = ubik_Call(VL_GetEntryByIDN, cstruct, 0, volid, voltype, entryp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_GetEntryByName(namep, entryp)
    char *namep;
    struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code, (*nproc)();

    if (newvlserver == 1) {
tryold:
	code = ubik_Call(VL_GetEntryByNameO, cstruct, 0, namep, &oentry);
	if (!code)
	    ovlentry_to_nvlentry(&oentry, entryp);
	return code;
    }
    code = ubik_Call(VL_GetEntryByNameN, cstruct, 0, namep, entryp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_ReplaceEntry(volid, voltype, entryp, releasetype)
    afs_int32 volid, voltype, releasetype;
    struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code, (*nproc)();

    if (newvlserver == 1) {
tryold:
	code = nvlentry_to_ovlentry(entryp, &oentry);
	if (code)
	    return code;
	code = ubik_Call(VL_ReplaceEntry, cstruct, 0, volid, voltype, &oentry, releasetype);
	return code;
    }
    code = ubik_Call(VL_ReplaceEntryN, cstruct, 0, volid, voltype, entryp, releasetype);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}



VLDB_ListAttributes(attrp, entriesp, blkentriesp)
    VldbListByAttributes *attrp;     
    afs_int32 *entriesp;
    nbulkentries *blkentriesp;
{
    bulkentries arrayEntries;
    register int code, i;

    if (newvlserver == 1) {
tryold:
	bzero(&arrayEntries, sizeof(arrayEntries)); /*initialize to hint the stub  to alloc space */
	code = ubik_Call(VL_ListAttributes, cstruct, 0, attrp, entriesp, &arrayEntries);
	if (!code) {
	    blkentriesp->nbulkentries_val = (nvldbentry *)malloc(*entriesp * sizeof(struct nvldbentry));
	    for (i = 0; i < *entriesp; i++) {	/* process each entry */
		ovlentry_to_nvlentry(&arrayEntries.bulkentries_val[i], &blkentriesp->nbulkentries_val[i]);		
	    }
	}
	if (arrayEntries.bulkentries_val) free(arrayEntries.bulkentries_val);
	return code;
    }
    code = ubik_Call(VL_ListAttributesN, cstruct, 0, attrp, entriesp, blkentriesp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_ListAttributesN2(attrp, name, thisindex, nentriesp, blkentriesp, nextindexp)
  VldbListByAttributes *attrp;
  char                 *name;
  afs_int32                thisindex;
  afs_int32                *nentriesp;
  nbulkentries         *blkentriesp;
  afs_int32                *nextindexp;
{
  afs_int32 code;

  code = ubik_Call(VL_ListAttributesN2, cstruct, 0, 
		   attrp, (name?name:""), thisindex, 
		   nentriesp, blkentriesp, nextindexp);
  return code;
}


static int vlserverv4=-1;
struct cacheips {
    afs_int32 server;
    afs_int32 count;
    afs_uint32 addrs[16];
};
struct cacheips cacheips[16];
int cacheip_index=0;
extern int VL_GetAddrsU();
VLDB_IsSameAddrs(serv1, serv2, errorp)
    afs_int32 serv1, serv2, *errorp;
{
    struct vldbentry oentry;
    register int code, (*nproc)();
    ListAddrByAttributes attrs;
    bulkaddrs addrs;
    afs_uint32 *addrp, nentries,  unique, i, j, f1, f2;
    afsUUID uuid;
    static int initcache = 0;

    *errorp = 0;

    if (serv1 == serv2)   
	return 1;
    if (vlserverv4 == 1) {
	return 0;
    }
    if (!initcache) {
	for (i=0; i<16; i++) {
	   cacheips[i].server = cacheips[i].count = 0;
	}
	initcache = 1;
    }

    /* See if it's cached */
    for (i=0; i<16; i++) {
       f1 = f2 = 0;
       for (j=0; j < cacheips[i].count; j++) {
	  if      (serv1 == cacheips[i].addrs[j]) f1 = 1;
	  else if (serv2 == cacheips[i].addrs[j]) f2 = 1;

	  if (f1 && f2)
	     return 1;
       }	
       if (f1 || f2)
	  return 0;
  }

    bzero(&attrs, sizeof(attrs));
    attrs.Mask = VLADDR_IPADDR;
    attrs.ipaddr = serv1;
    bzero(&addrs, sizeof(addrs));
    bzero(&uuid, sizeof(uuid));
    code = ubik_Call(VL_GetAddrsU, cstruct, 0, &attrs, &uuid, &unique, &nentries, &addrs);
    if (vlserverv4 == -1) {
	if (code == RXGEN_OPCODE) {
	    vlserverv4 = 1;	/* Doesn't support new interface */
	    return 0;
	} else if (!code) {
	    vlserverv4 = 2;
	}
    }
    if (code == VL_NOENT)
	return 0;
    if (code) {
	*errorp = code;
	return 0;
    }

    code = 0;
    if (++cacheip_index >= 16) cacheip_index = 0;
    cacheips[cacheip_index].server = serv1;
    cacheips[cacheip_index].count = nentries;
    addrp = addrs.bulkaddrs_val;
    for (i=0; i<nentries; i++, addrp++) {
	cacheips[cacheip_index].addrs[i] = *addrp;
	if (serv2 == *addrp) {
	    code = 1;
	}
    }
    return code;
}


#ifdef	notdef
afs_int32 subik_Call(aproc, aclient, aflags, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)
    register struct ubik_client *aclient;
    int (*aproc)();
    afs_int32 aflags;
    afs_int32 p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16;
{
    struct vldbentry vldbentry;
    register int code, (*nproc)();

    if (newvlserver == 1) {
    }
    code = ubik_Call(aproc, aclient, aflags, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	} else if (!code) {
	    newvlserver = 2;
	}
    }
}
#endif	notdef


/*
  Get the appropriate type of ubik client structure out from the system.
*/
afs_int32 vsu_ClientInit(noAuthFlag, confDir, cellName, sauth, uclientp, secproc)
    int noAuthFlag;
    int (*secproc)();
    char *cellName;
    struct ubik_client **uclientp;
    char *confDir;
    afs_int32 sauth;
{
    afs_int32 code, scIndex, i;
    struct afsconf_cell info;
    struct afsconf_dir *tdir;
    struct ktc_principal sname;
    struct ktc_token ttoken;
    struct rx_securityClass *sc;
    static struct rx_connection *serverconns[VLDB_MAXSERVERS];
    char cellstr[64];


    code = rx_Init(0);
    if (code) {
        fprintf(STDERR,"vsu_ClientInit: could not initialize rx.\n");
        return code;
    }
    rx_SetRxDeadTime(90);

    if (sauth) {  /* -localauth */
        tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
        if (!tdir) {
            fprintf(STDERR, "vsu_ClientInit: Could not process files in configuration directory (%s).\n",
                    AFSDIR_SERVER_ETC_DIRPATH);
            return -1;
        }
        code = afsconf_ClientAuth(tdir, &sc, &scIndex); /* sets sc,scIndex */
        if (code) {
            fprintf(STDERR, "vsu_ClientInit: Could not get security object for -localAuth %\n");
            return -1;
        }
        code = afsconf_GetCellInfo(tdir, tdir->cellName, AFSCONF_VLDBSERVICE,
               &info);
        if (code) {
            fprintf(STDERR, "vsu_ClientInit: can't find cell %s's hosts in %s/%s\n",
                   cellName, AFSDIR_SERVER_ETC_DIRPATH,AFSDIR_CELLSERVDB_FILE);
            exit(1);
        }
    }
    else {  /* not -localauth */
        tdir = afsconf_Open(confDir);
        if (!tdir) {
            fprintf(STDERR, "vsu_ClientInit: Could not process files in configuration directory (%s).\n",
                    confDir);
            return -1;
        }

        if (!cellName) {
            code = afsconf_GetLocalCell(tdir, cellstr, sizeof(cellstr));
            if (code) {
                fprintf(STDERR, "vsu_ClientInit: can't get local cellname, check %s/%s\n",
                        confDir, AFSDIR_THISCELL_FILE);
                exit(1);
            }
            cellName = cellstr;
        }

        code = afsconf_GetCellInfo(tdir, cellName, AFSCONF_VLDBSERVICE, &info);
        if (code) {
            fprintf(STDERR, "vsu_ClientInit: can't find cell %s's hosts in %s/%s\n",
                    cellName, confDir,AFSDIR_CELLSERVDB_FILE);
            exit(1);
        }
        if (noAuthFlag)    /* -noauth */
            scIndex = 0;
        else {             /* not -noauth */
            strcpy(sname.cell, info.name);
            sname.instance[0] = 0;
            strcpy(sname.name, "afs");
            code = ktc_GetToken(&sname, &ttoken, sizeof(ttoken), (char *)0);
            if (code) {   /* did not get ticket */
                fprintf(STDERR, "vsu_ClientInit: Could not get afs tokens, running unauthenticated.\n");
                scIndex = 0;
            }
            else {     /* got a ticket */
                scIndex = 2;
                if ((ttoken.kvno < 0) || (ttoken.kvno > 255)) {
                    fprintf(STDERR, "vsu_ClientInit: funny kvno (%d) in ticket, proceeding\n",
                            ttoken.kvno);
                }
            }
        }

        switch (scIndex) {
          case 0 :
            sc = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
            break;
          case 2:
            sc = (struct rx_securityClass *)rxkad_NewClientSecurityObject(
                 rxkad_clear, &ttoken.sessionKey, ttoken.kvno,
                 ttoken.ticketLen, ttoken.ticket);
            break;
          default:
            fprintf(STDERR, "vsu_ClientInit: unsupported security index %d\n",
                     scIndex);
            exit(1);
            break;
        }
    }

    if (secproc)     /* tell UV module about default authentication */
        (*secproc) (sc, scIndex);
    if (info.numServers > VLDB_MAXSERVERS) {
        fprintf(STDERR, "vsu_ClientInit: info.numServers=%d (> VLDB_MAXSERVERS=%d)\n",
                info.numServers, VLDB_MAXSERVERS);
        exit(1);
    }
    for (i=0; i<info.numServers; i++) {
        serverconns[i] = rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
                         info.hostAddr[i].sin_port, USER_SERVICE_ID,
                         sc, scIndex);
    }
    *uclientp = 0;
    code = ubik_ClientInit(serverconns, uclientp);
    if (code) {
        fprintf(STDERR, "vsu_ClientInit: ubik client init failed.\n");
        return code;
    }
    return 0;
}


/*extract the name of volume <name> without readonly or backup suffixes
 * and return the result as <rname>.
 */
vsu_ExtractName(rname,name)
char rname[],name[];
{   char sname[32];
    int total;

    strcpy(sname,name);
    total = strlen(sname);
    if(!strcmp(&sname[total - 9],".readonly")) {
	/*discard the last 8 chars */
	sname[total - 9] = '\0';
	strcpy(rname,sname);
	return 0;
    }
    else if(!strcmp(&sname[total - 7 ],".backup")) {
	/*discard last 6 chars */
	sname[total - 7] = '\0';
	strcpy(rname,sname);
	return 0;
    }
    else {
	strncpy(rname,name,VOLSER_OLDMAXVOLNAME);
	return -1;
    }
}


/* returns 0 if failed */
afs_uint32 vsu_GetVolumeID(astring, acstruct, errp)
struct ubik_client *acstruct;
afs_int32 *errp;
char *astring; {
    afs_uint32 tc, value;

    char *str,*ptr, volname[VOLSER_OLDMAXVOLNAME+1];
    int tryname, curval;
    struct nvldbentry entry;
    afs_int32 vcode;
    int total;

    *errp = 0;
    total = strlen(astring);
    str = astring;
    ptr = astring;
    tryname = 0;
    while (curval = *str++){
	if(curval < '0' || curval > '9')
	    tryname = 1;
    }

    if(tryname) {
	vsu_ExtractName(volname,astring);
	vcode = VLDB_GetEntryByName(volname, &entry);
	if(!vcode) {
	    if(!strcmp(&astring[total - 9],".readonly"))
		return entry.volumeId[ROVOL];
	    else if ((!strcmp(&astring[total - 7 ],".backup")))
		return entry.volumeId[BACKVOL];
	    else
		return (entry.volumeId[RWVOL]);   
	} else {
	    *errp = vcode;
	    return 0;	/* can't find volume */
	}
    }

    value = 0;
    while (tc = *astring++) {
	if (tc & 0x80) {
	    if(!tryname) fprintf(STDERR,"goofed in volid \n");
	    else {fprintf(STDERR,"Could not get entry from vldb for %s\n",ptr);PrintError("",vcode);}
	    *errp = EINVAL;
	    return 0;
	}
	if (tc < '0' || tc > '9'){
	    if(!tryname) fprintf(STDERR,"internal error: out of range char in vol ID\n");
	    else {fprintf(STDERR,"Could not get entry from vldb for %s\n",ptr);PrintError("",vcode);}
	    *errp = ERANGE;
	    return 0;
	}
	value *= 10;
	value += (tc - '0');
    }
    return value;
}
