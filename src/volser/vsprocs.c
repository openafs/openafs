/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
#endif
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <lock.h>
#include <afs/voldefs.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/vlserver.h>
#include <afs/nfs.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include "volser.h" 
#include "volint.h"
#include "lockdata.h"
#include <afs/com_err.h>
#include <rx/rxkad.h>
#include <afs/kautils.h>
#include <afs/cmd.h>
#include <errno.h>
#define ERRCODE_RANGE 8			/* from error_table.h */
#define	CLOCKSKEW   2			/* not really skew, but resolution */

/* for UV_MoveVolume() recovery */

#include <afs/procmgmt.h>   /* signal(), kill(), wait(), etc. */
#include <setjmp.h>

afs_int32 VolumeExists(), CheckVldbRWBK(), CheckVldb();

struct ubik_client *cstruct;
int verbose = 0;
extern struct  rx_securityClass *rxnull_NewClientSecurityObject();
extern struct rx_connection *rx_NewConnection();
extern void AFSVolExecuteRequest();
extern struct rx_securityClass *rxnull_NewServerSecurityObject();
extern int VL_GetNewVolumeId();
extern int VL_SetLock();
extern int VL_ReleaseLock();
extern int VL_DeleteEntry();

struct release {
  afs_int32 time;
  afs_int32 vldbEntryIndex;
};

/*map the partition <partId> into partition name <partName>*/
MapPartIdIntoName(partId, partName)
afs_int32 partId;
char *partName;
{
    if(partId < 26) {/* what if partId > = 26 ? */
	strcpy(partName,"/vicep");
	partName[6] = partId + 'a';
	partName[7] = '\0';
	return 0;
    } else if (partId < VOLMAXPARTS) {
        strcpy(partName,"/vicep");
	partId -= 26;
	partName[6] = 'a' + (partId/26);
	partName[7] = 'a' + (partId%26);
	partName[8] = '\0';
	return 0;
    }
}

yesprompt(str)
char *str;
{
    char response, c;
    int code;

    fprintf(STDERR, "Do you want to %s? [yn](n): ", str);
    response = c = getchar();
    while (!(c==EOF || c=='\n')) c=getchar(); /*skip to end of line*/
    code = (response=='y'||response=='Y');	
    return code;
}


PrintError(msg, errcode)
    char *msg;
    afs_int32 errcode;
{
	fprintf(STDERR,msg);
	/*replace by a big switch statement*/
	switch(errcode) {
	    case 0 :	
		break;
	    case -1	:   fprintf(STDERR,"Possible communication failure\n");
		break;
	    case VSALVAGE: fprintf(STDERR,"Volume needs to be salvaged\n");
		break;
	    case VNOVNODE:  fprintf(STDERR,"Bad vnode number quoted\n");
		break;
	    case VNOVOL:    fprintf(STDERR,"Volume not attached, does not exist, or not on line\n");
		break;
	    case VVOLEXISTS:fprintf(STDERR,"Volume already exists\n");
		break;
	    case VNOSERVICE:fprintf(STDERR,"Volume is not in service\n");
		break;
	    case VOFFLINE:  fprintf(STDERR,"Volume is off line\n");
		break;
	    case VONLINE:   fprintf(STDERR,"Volume is already on line\n");
		break;
	    case VDISKFULL: fprintf(STDERR,"Partition is full\n");
		break;
	    case VOVERQUOTA:fprintf(STDERR,"Volume max quota exceeded\n");
		break;
	    case VBUSY:	fprintf(STDERR,"Volume temporarily unavailable\n");
		break;
	    case VMOVED:fprintf(STDERR,"Volume has moved to another server\n");
		break;
	    case VL_IDEXIST :  fprintf(STDERR,"VLDB: volume Id exists in the vldb\n");
		break;
	    case VL_IO:   fprintf(STDERR,"VLDB: a read terminated too early\n");
		break;
	    case VL_NAMEEXIST:   fprintf(STDERR,"VLDB: volume entry exists in the vldb\n");
		break;
	    case VL_CREATEFAIL:   fprintf(STDERR,"VLDB: internal creation failure\n");
		break;
	    case VL_NOENT:   fprintf(STDERR,"VLDB: no such entry\n");
		break;
	    case VL_EMPTY:   fprintf(STDERR,"VLDB: vldb database is empty\n");
		break;
	    case VL_ENTDELETED:   fprintf(STDERR,"VLDB: entry is deleted (soft delete)\n");
		break;
	    case VL_BADNAME:   fprintf(STDERR,"VLDB: volume name is illegal\n");
		break;
	    case VL_BADINDEX:   fprintf(STDERR,"VLDB: index was out of range\n");
		break;
	    case VL_BADVOLTYPE:   fprintf(STDERR,"VLDB: bad volume type\n");
		break;
	    case VL_BADSERVER:   fprintf(STDERR,"VLDB: illegal server number (not within limits)\n");
		break;
	    case VL_BADPARTITION:   fprintf(STDERR,"VLDB: bad partition number\n");
		break;
	    case VL_REPSFULL:   fprintf(STDERR,"VLDB: run out of space for replication sites\n");
		break;
	    case VL_NOREPSERVER:   fprintf(STDERR,"VLDB: no such repsite server exists\n");
		break;
	    case VL_DUPREPSERVER:   fprintf(STDERR,"VLDB: replication site server already exists\n");
		break;
	    case VL_RWNOTFOUND:   fprintf(STDERR,"VLDB: parent r/w entry not found\n");
		break;
	    case VL_BADREFCOUNT:   fprintf(STDERR,"VLDB: illegal reference count number\n");
		break;
	    case VL_SIZEEXCEEDED:   fprintf(STDERR,"VLDB: vldb size for attributes exceeded\n");
		break;
	    case VL_BADENTRY:   fprintf(STDERR,"VLDB: bad incoming vldb entry\n");
		break;
	    case VL_BADVOLIDBUMP:   fprintf(STDERR,"VLDB: illegal max volid increment\n");
		break;
	    case VL_IDALREADYHASHED:   fprintf(STDERR,"VLDB: (RO/BACK) Id already hashed\n");
		break;
	    case VL_ENTRYLOCKED:   fprintf(STDERR,"VLDB: vldb entry is already locked\n");
		break;
	    case VL_BADVOLOPER:   fprintf(STDERR,"VLDB: bad volume operation code\n");
		break;
	    case VL_BADRELLOCKTYPE:   fprintf(STDERR,"VLDB: bad release lock type\n");
		break;
	    case VL_RERELEASE:   fprintf(STDERR,"VLDB: status report: last release was aborted\n");
		break;
	    case VL_BADSERVERFLAG:	fprintf(STDERR,"VLDB: invalid replication site server flag\n");
		break;
	    case VL_PERM:	fprintf(STDERR,"VLDB: no permission access for call\n");
		break;
	    case VOLSERREAD_DUMPERROR:fprintf(STDERR,"VOLSER:  Problems encountered in reading the dump file !\n");
		break;
	    case VOLSERDUMPERROR:fprintf(STDERR,"VOLSER: Problems encountered in doing the dump !\n");
		break;
	    case VOLSERATTACH_ERROR: fprintf(STDERR,"VOLSER: Could not attach the volume\n");
		break;
	    case VOLSERDETACH_ERROR: fprintf(STDERR,"VOLSER: Could not detach the volume\n");
		break;
	    case VOLSERILLEGAL_PARTITION: fprintf(STDERR,"VOLSER: encountered illegal partition number\n");
		break;
	    case VOLSERBAD_ACCESS: fprintf(STDERR,"VOLSER: permission denied, not a super user\n");
		break;
	    case VOLSERVLDB_ERROR: fprintf(STDERR,"VOLSER: error detected in the VLDB\n");
		break;
	    case VOLSERBADNAME:	fprintf(STDERR,"VOLSER: error in volume name\n");
		break;
	    case VOLSERVOLMOVED: fprintf(STDERR,"VOLSER: volume has moved\n");
		break;
	    case VOLSERBADOP: fprintf(STDERR,"VOLSER: illegal operation\n");
		break;
	    case VOLSERBADRELEASE: fprintf(STDERR,"VOLSER: release could not be completed\n");
		break;
	    case VOLSERVOLBUSY: fprintf(STDERR,"VOLSER: volume is busy\n");
		break;
	      case VOLSERNO_MEMORY: fprintf(STDERR,"VOLSER: volume server is out of memory\n");
		break;
	      case VOLSERNOVOL:fprintf(STDERR,"VOLSER: no such volume - location specified incorrectly or volume does not exist\n");
		break;
	      case VOLSERMULTIRWVOL: fprintf(STDERR,"VOLSER: multiple RW volumes with same ID, one of which should be deleted\n");
		break;
	      case VOLSERFAILEDOP: fprintf(STDERR,"VOLSER: not all entries were successfully processed\n");
		break;
	    default: 
		{

		afs_int32 offset;

		initialize_ka_error_table();
		initialize_rxk_error_table();
		initialize_ktc_error_table();
		initialize_acfg_error_table();
		initialize_cmd_error_table();
		initialize_vl_error_table();
		
		offset = errcode & ((1<<ERRCODE_RANGE)-1);
		fprintf(STDERR,"%s: %s\n",error_table_name (errcode), error_message (errcode));
		break;
		}
	}
	return 0;
}


static struct rx_securityClass *uvclass=0;
static int uvindex = -1;
/* called by VLDBClient_Init to set the security module to be used in the RPC */
UV_SetSecurity(as, aindex)
register struct rx_securityClass *as;
afs_int32 aindex; {
    uvindex = aindex;
    uvclass = as;
}

/* bind to volser on <port> <aserver> */
/* takes server address in network order, port in host order.  dumb */
struct rx_connection *UV_Bind(aserver, port)
afs_int32 aserver, port; 
{
    register struct rx_connection *tc;
    
    tc = rx_NewConnection(aserver, htons(port), VOLSERVICE_ID, uvclass, uvindex);
    return tc;
}

/* if <okvol> is allright(indicated by beibg able to
 * start a transaction, delete the <delvol> */
static afs_int32 CheckAndDeleteVolume(aconn,apart,okvol,delvol)
struct rx_connection *aconn;
afs_int32 apart,okvol,delvol;
{
    afs_int32 error,code,tid,rcode;

    error = 0;
    code = 0;

    if(okvol == 0) {
	code = AFSVolTransCreate(aconn, delvol, apart, ITOffline,&tid);
	if(!error && code) error = code;
	code = AFSVolDeleteVolume(aconn,tid);
	if(!error && code) error = code;
	code = AFSVolEndTrans(aconn,tid, &rcode);
	if(!code) code = rcode;
	if(!error && code) error = code;
	return error;
    }
    else {
	code = AFSVolTransCreate(aconn, okvol, apart, ITOffline,&tid);
	if(!code) {
	    code = AFSVolEndTrans(aconn,tid, &rcode);
	    if(!code) code = rcode;
	    if(!error && code) error = code;
	    code = AFSVolTransCreate(aconn, delvol, apart, ITOffline,&tid);
	    if(!error && code) error = code;
	    code = AFSVolDeleteVolume(aconn,tid);
	    if(!error && code) error = code;
	    code = AFSVolEndTrans(aconn,tid, &rcode);
	    if(!code) code = rcode;
	    if(!error && code) error = code;
	}
	else 
	    error = code;
	return error;
    }
}

/* called by EmuerateEntry, show vldb entry in a reasonable format */
void SubEnumerateEntry(entry)
struct nvldbentry *entry;
{
    int i;
    char pname[10];
    int isMixed = 0;

#ifdef notdef
    fprintf(STDOUT,"	readWriteID %-10u ",entry->volumeId[RWVOL]);
    if(entry->flags & RW_EXISTS) fprintf(STDOUT," valid \n");else fprintf(STDOUT," invalid \n");
    fprintf(STDOUT,"	readOnlyID  %-10u ",entry->volumeId[ROVOL]);
    if(entry->flags & RO_EXISTS) fprintf(STDOUT," valid \n") ;else fprintf(STDOUT," invalid \n");
    fprintf(STDOUT,"	backUpID    %-10u ",entry->volumeId[BACKVOL]);
    if(entry->flags & BACK_EXISTS) fprintf(STDOUT," valid \n"); else fprintf(STDOUT," invalid \n");
    if((entry->cloneId != 0) && (entry->flags & RO_EXISTS))
	fprintf(STDOUT,"    releaseClone %-10u \n",entry->cloneId);
#else
    if (entry->flags & RW_EXISTS)
	fprintf(STDOUT,"    RWrite: %-10u",entry->volumeId[RWVOL]);
    if (entry->flags & RO_EXISTS)
	fprintf(STDOUT,"    ROnly: %-10u",entry->volumeId[ROVOL]);
    if (entry->flags & BACK_EXISTS)
	fprintf(STDOUT,"    Backup: %-10u",entry->volumeId[BACKVOL]);
    if ((entry->cloneId != 0) && (entry->flags & RO_EXISTS))
	fprintf(STDOUT,"    RClone: %-10u",entry->cloneId);
    fprintf(STDOUT,"\n");
#endif
    fprintf(STDOUT,"    number of sites -> %u\n",entry->nServers);
    for(i = 0; i < entry->nServers; i++) {
	if(entry->serverFlags[i] & NEW_REPSITE)
	    isMixed = 1;
    }
    for(i = 0; i < entry->nServers; i++) {
	MapPartIdIntoName(entry->serverPartition[i],pname);
	fprintf(STDOUT,"       server %s partition %s ",
		hostutil_GetNameByINet(entry->serverNumber[i]), pname);
	if(entry->serverFlags[i] & ITSRWVOL) fprintf(STDOUT,"RW Site ") ; else fprintf(STDOUT,"RO Site ");
	if (isMixed) {
	   if (entry->serverFlags[i] & NEW_REPSITE)
	      fprintf(STDOUT," -- New release");
	   else
	      fprintf(STDOUT," -- Old release");
	} else {
	   if (entry->serverFlags[i] & RO_DONTUSE)
	      fprintf(STDOUT," -- Not released");
	}
	fprintf(STDOUT,"\n");
    }
    
    return;
    
}

/*enumerate the vldb entry corresponding to <entry> */
void EnumerateEntry(entry)
struct nvldbentry *entry;
{
    int i;
    char pname[10];
    int isMixed = 0;

    fprintf(STDOUT,"\n");
    fprintf(STDOUT,"%s \n",entry->name);
    SubEnumerateEntry(entry);
    return;
}

/* forcibly remove a volume.  Very dangerous call */
UV_NukeVolume(server, partid, volid)
afs_int32 server;
afs_int32 partid, volid; {
    register struct rx_connection *tconn;
    register afs_int32 code;

    tconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    if (tconn) {
	code = AFSVolNukeVolume(tconn, partid, volid);
	rx_DestroyConnection(tconn);
    }
    else code = 0;
    return code;
}

/* like df. Return usage of <pname> on <server> in <partition> */
UV_PartitionInfo(server,pname,partition)
afs_int32 server;
char *pname;
struct diskPartition *partition;
{
    register struct rx_connection *aconn;
    afs_int32 code;

    code = 0;
    aconn = (struct rx_connection *)0;
    aconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    code = AFSVolPartitionInfo(aconn,pname,partition);
    if(code){
	fprintf(STDERR,"Could not get information on partition %s\n",pname);
	PrintError("",code);
    }
    if(aconn) rx_DestroyConnection(aconn);
    return code;
}

/* old interface to create volume */
UV_CreateVolume(aserver, apart, aname, anewid)
afs_int32 apart, aserver;
char *aname;
afs_int32 *anewid; 
{
afs_int32 code;
code = UV_CreateVolume2(aserver, apart, aname, 5000, 0, 0, 0, 0, anewid);
return code;
}

/* create a volume, given a server, partition number, volume name --> sends
* back new vol id in <anewid>*/
UV_CreateVolume2(aserver, apart, aname, aquota, aspare1, aspare2, aspare3, aspare4, anewid)
afs_int32 apart, aserver;
afs_int32 aspare1, aspare2, aspare3, aspare4;
afs_int32 aquota;
char *aname;
afs_int32 *anewid; 
{

    register struct rx_connection *aconn;
    afs_int32 tid;
    register afs_int32 code;
    afs_int32 error;
    afs_int32 rcode,vcode;
    struct nvldbentry entry,storeEntry;/*the new vldb entry */
    struct volintInfo tstatus;

    tid = 0;
    aconn = (struct rx_connection *)0;
    error = 0;
    bzero (&tstatus, sizeof(struct volintInfo));
    tstatus.dayUse = -1;
    tstatus.maxquota = aquota;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);
    /* next the next 3 available ids from the VLDB */
    vcode = ubik_Call(VL_GetNewVolumeId,cstruct, 0, 3, anewid);
    if(vcode) {
	fprintf(STDERR,"Could not get an Id for volume %s\n",aname);
	error = vcode;
	goto cfail;
    }
    code = AFSVolCreateVolume(aconn, apart, aname, volser_RW, 0, anewid, &tid);
    if (code) {
	fprintf(STDERR,"Failed to create the volume %s %u \n",aname,*anewid);
	error = code;
	goto cfail;
    }
    
    code = AFSVolSetInfo(aconn, tid, &tstatus);
    if (code) {
	fprintf(STDERR,"Could not change quota (error %d), continuing...\n", code);
    }

    code = AFSVolSetFlags(aconn, tid, 0); /* bring it online (mark it InService */
    if (code) {
	fprintf(STDERR,"Could not bring the volume %s %u online \n",aname,*anewid);
	error = code;
	goto cfail;
    }
    if(verbose) fprintf(STDOUT,"Volume %s %u created and brought online\n",aname,*anewid);
    /* set up the vldb entry for this volume */
    strncpy(entry.name, aname,VOLSER_OLDMAXVOLNAME);
    entry.nServers = 1;
    entry.serverNumber[0] = aserver;	/* this should have another 
					 level of indirection later */
    entry.serverPartition[0] = apart;	/* this should also have 
					 another indirection level */
    entry.flags = RW_EXISTS;/* this records that rw volume exists */
    entry.serverFlags[0] = ITSRWVOL;	/*this rep site has rw  vol */
    entry.volumeId[RWVOL] = *anewid;
    entry.volumeId[ROVOL] = *anewid + 1;/* rw,ro, bk id are related in the default case */
    entry.volumeId[BACKVOL] = *anewid + 2;
    entry.cloneId = 0;
    /*map into right byte order, before passing to xdr, the stuff has to be in host
      byte order. Xdr converts it into network order */
    MapNetworkToHost(&entry,&storeEntry);
    /* create the vldb entry */
    vcode = VLDB_CreateEntry(&storeEntry);
    if(vcode) {
	fprintf(STDERR,"Could not create a VLDB entry for the  volume %s %u\n", aname,*anewid);
	/*destroy the created volume*/
	if(verbose) {
	    fprintf(STDOUT,"Deleting the newly created volume %u\n",*anewid);
	}
	AFSVolDeleteVolume(aconn,tid);
	error = vcode;
	goto cfail;
    }
    if(verbose) fprintf(STDOUT,"Created the VLDB entry for the volume %s %u\n",aname,*anewid);
    /* volume created, now terminate the transaction and release the connection*/
    code = AFSVolEndTrans(aconn, tid, &rcode);/*if it crashes before this
	the volume will come online anyway when transaction timesout , so if
	    vldb entry exists then the volume is guaranteed to exist too wrt create*/
    tid = 0;
    if(code){
	fprintf(STDERR,"Failed to end the transaction on the volume %s %u\n",aname,*anewid); 
	error = code;
	goto cfail;
    }

    cfail:
      if(tid)
	{
		code= AFSVolEndTrans(aconn, tid, &rcode);
		if(code)
			fprintf(STDERR,"WARNING: could not end transaction\n");
	}
    if(aconn) rx_DestroyConnection(aconn);
    PrintError("",error);
    return error;
    

}
/* create a volume, given a server, partition number, volume name --> sends
* back new vol id in <anewid>*/
UV_AddVLDBEntry(aserver, apart, aname, aid)
  afs_int32 apart, aserver;
  char *aname;
  afs_int32 aid; 
{
    register struct rx_connection *aconn;
    register afs_int32 code;
    afs_int32 error;
    afs_int32 rcode,vcode;
    struct nvldbentry entry,storeEntry;/*the new vldb entry */

    aconn = (struct rx_connection *)0;
    error = 0;

    /* set up the vldb entry for this volume */
    strncpy(entry.name, aname,VOLSER_OLDMAXVOLNAME);
    entry.nServers = 1;
    entry.serverNumber[0] = aserver;	/* this should have another 
					 level of indirection later */
    entry.serverPartition[0] = apart;	/* this should also have 
					 another indirection level */
    entry.flags = RW_EXISTS;/* this records that rw volume exists */
    entry.serverFlags[0] = ITSRWVOL;	/*this rep site has rw  vol */
    entry.volumeId[RWVOL] = aid;
#ifdef notdef
    entry.volumeId[ROVOL] = anewid + 1;/* rw,ro, bk id are related in the default case */
    entry.volumeId[BACKVOL] = *anewid + 2;
#else
    entry.volumeId[ROVOL] = 0;
    entry.volumeId[BACKVOL] = 0;
#endif
    entry.cloneId = 0;
    /*map into right byte order, before passing to xdr, the stuff has to be in host
      byte order. Xdr converts it into network order */
    MapNetworkToHost(&entry,&storeEntry);
    /* create the vldb entry */
    vcode = VLDB_CreateEntry(&storeEntry);
    if(vcode) {
	fprintf(STDERR,"Could not create a VLDB entry for the  volume %s %u\n", aname,aid);
	error = vcode;
	goto cfail;
    }
    if(verbose) fprintf(STDOUT,"Created the VLDB entry for the volume %s %u\n",aname,aid);

  cfail:
    if(aconn) rx_DestroyConnection(aconn);
    PrintError("",error);
    return error;
}

#define ERROR_EXIT(code) {error=(code); goto error_exit;}

/* Delete the volume <volid>on <aserver> <apart>
 * the physical entry gets removed from the vldb only if the ref count 
 * becomes zero
 */
UV_DeleteVolume(aserver, apart, avolid)
    afs_int32 aserver, apart, avolid; 
{
    struct rx_connection *aconn = (struct rx_connection *)0;
    afs_int32 ttid = 0;
    afs_int32 code, rcode;
    afs_int32 error = 0;
    struct nvldbentry entry,storeEntry;
    int islocked = 0;
    afs_int32 avoltype = -1, vtype;
    int notondisk = 0, notinvldb = 0;

    /* Find and read bhe VLDB entry for this volume */
    code = ubik_Call(VL_SetLock, cstruct, 0, avolid, avoltype, VLOP_DELETE);
    if (code) {
	if (code != VL_NOENT) {
	   fprintf(STDERR,"Could not lock VLDB entry for the volume %u\n", avolid);
	   ERROR_EXIT(code);
	}
	notinvldb = 1;
    } else {
       islocked = 1;

       code = VLDB_GetEntryByID(avolid, avoltype, &entry);
       if (code) {
	  fprintf(STDERR,"Could not fetch VLDB entry for volume %u\n",avolid);
	  ERROR_EXIT(code);
       }
       MapHostToNetwork(&entry);

       if (verbose)
	  EnumerateEntry(&entry);
    }

    /* Whether volume is in the VLDB or not. Delete the volume on disk */
    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);
    code = AFSVolTransCreate(aconn, avolid, apart, ITOffline, &ttid);
    if (code) {
       if (code == VNOVOL) {
	  notondisk = 1;
       } else {
	   fprintf(STDERR,"Transaction on volume %u failed\n", avolid);
	   ERROR_EXIT(code);
       }
    }
    else {
       if (verbose) {
	  fprintf(STDOUT,"Trying to delete the volume %u ...", avolid);
	  fflush(STDOUT);
       }
       code = AFSVolDeleteVolume(aconn, ttid);
       if (code) {
	  fprintf(STDERR,"Could not delete the volume %u \n", avolid);
	  ERROR_EXIT(code);
       }
       code = AFSVolEndTrans(aconn, ttid, &rcode);
       code = (code ? code : rcode);
       ttid = 0;
       if (code) {
	  fprintf(STDERR,"Could not end the transaction for the volume %u \n",avolid);
	  ERROR_EXIT(code);
       }
       if (verbose)
	  fprintf(STDOUT," done\n");
    }

    /* Now update the VLDB entry.
     * But first, verify we have a VLDB entry.
     * Whether volume is on disk or not. Delete the volume in VLDB.
     */
    if (notinvldb)
       ERROR_EXIT(0);

    if (avolid == entry.volumeId[BACKVOL]) {
	/* Its a backup volume, modify the VLDB entry. Check that the
	 * backup volume is on the server/partition we asked to delete.
	 */
        if ( !(entry.flags & BACK_EXISTS) || !Lp_Match(aserver,apart,&entry)) {
	   notinvldb = 2;         /* Not on this server and partition */
	   ERROR_EXIT(0);
	}

	if (verbose)
	   fprintf(STDOUT,"Marking the backup volume %u deleted in the VLDB\n", avolid);

	entry.flags &= ~BACK_EXISTS;
	vtype = BACKVOL;
    }

    else if (avolid == entry.volumeId[ROVOL]) {
	/* Its a read-only volume, modify the VLDB entry. Check that the
	 * readonly volume is on the server/partition we asked to delete.
	 * If flags does not have RO_EIXSTS set, then this may mean the RO 
	 * hasn't been released (and could exist in VLDB).
	 */
	if (!Lp_ROMatch(aserver,apart,&entry)) {
	   notinvldb = 2;            /* Not found on this server and partition */
	   ERROR_EXIT(0);
	}
	
	if (verbose)
	   fprintf(STDOUT,"Marking the readonly volume %u deleted in the VLDB\n", avolid);

	Lp_SetROValue(&entry, aserver, apart, 0, 0);  /* delete the site */
	entry.nServers--;
	if (!Lp_ROMatch(0,0,&entry))
	   entry.flags &= ~RO_EXISTS;    /* This was the last ro volume */
	vtype = ROVOL;
    }

    else if (avolid == entry.volumeId[RWVOL]) {
        /* It's a rw volume, delete the backup volume, modify the VLDB entry.
	 * Check that the readwrite volumes is on the server/partition we
	 * asked to delete.
	 */
        if (!(entry.flags & RW_EXISTS) || !Lp_Match(aserver,apart,&entry)) {
	   notinvldb = 2;          /* Not found on this server and partition */
	   ERROR_EXIT(0);
	}
	
	/* Delete backup if it exists */
	code = AFSVolTransCreate(aconn, entry.volumeId[BACKVOL], apart, ITOffline, &ttid);
	if (!code) {
	   if (verbose) {
	      fprintf(STDOUT,"Trying to delete the backup volume %u ...", entry.volumeId[BACKVOL]);
	      fflush(STDOUT);
	   }
	   code = AFSVolDeleteVolume(aconn, ttid);
	   if (code) {
	      fprintf(STDERR,"Could not delete the volume %u \n", entry.volumeId[BACKVOL]);
	      ERROR_EXIT(code);
	   }
	   code = AFSVolEndTrans(aconn, ttid, &rcode);
	   ttid = 0;
	   code = (code ? code : rcode);
	   if (code) {
	      fprintf(STDERR,"Could not end the transaction for the volume %u \n",
		      entry.volumeId[BACKVOL]);
	      ERROR_EXIT(code);
	   }
	   if (verbose)
	      fprintf(STDOUT," done\n");
	}

	if (verbose)
	   fprintf(STDOUT,"Marking the readwrite volume %u%s deleted in the VLDB\n", 
		   avolid, ((entry.flags & BACK_EXISTS)?", and its backup volume,":""));

	Lp_SetRWValue(&entry, aserver, apart, 0L, 0L);
	entry.nServers--;
	entry.flags &= ~(BACK_EXISTS | RW_EXISTS);
	vtype = RWVOL;

	if (entry.flags & RO_EXISTS)
	   fprintf(STDERR,"WARNING: ReadOnly copy(s) may still exist\n");
    }

    else {
       notinvldb = 2;         /* Not found on this server and partition */
       ERROR_EXIT(0);
    }

    /* Either delete or replace the VLDB entry */
    if ((entry.nServers <= 0) || !(entry.flags & (RO_EXISTS | RW_EXISTS))) {
       if (verbose)
	  fprintf(STDOUT,"Last reference to the VLDB entry for %u - deleting entry\n", avolid);
       code = ubik_Call(VL_DeleteEntry, cstruct, 0, avolid, vtype);
       if (code) {
	  fprintf(STDERR,"Could not delete the VLDB entry for the volume %u \n",avolid);
	  ERROR_EXIT(code);
       }
    } else {
       MapNetworkToHost(&entry, &storeEntry);
       code = VLDB_ReplaceEntry(avolid, vtype, &storeEntry,
				(LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
       if (code) {
	  fprintf(STDERR,"Could not update the VLDB entry for the volume %u \n", avolid);
	  ERROR_EXIT(code);
       }
    }
    islocked = 0;

  error_exit:
    if (error) PrintError("", error);

    if (notondisk && notinvldb) {
       fprintf(STDERR,"Volume %u does not exist %s\n",
	       avolid, ((notinvldb == 2)?"on server and partition":""));
       PrintError("", VOLSERNOVOL);
       if (!error) error = VOLSERNOVOL;
    }
    else if (notondisk) {
       fprintf(STDERR,"WARNING: Volume %u did not exist on the partition\n", avolid);
    }
    else if (notinvldb) {
       fprintf(STDERR,"WARNING: Volume %u does not exist in VLDB %s\n",
	       avolid, ((notinvldb == 2)?"on server and partition":""));
    }

    if (ttid) {
	code = AFSVolEndTrans(aconn, ttid, &rcode);
	code = (code ? code : rcode);
	if (code) {
	    fprintf(STDERR,"Could not end transaction on the volume %u\n", avolid);
	    PrintError("", code);
	    if (!error) error = code;
	}
    }

    if (islocked) {
	code = ubik_Call(VL_ReleaseLock,cstruct, 0, avolid, -1, 
			 (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	if (code) {
	    fprintf(STDERR,"Could not release the lock on the VLDB entry for the volume %u \n", 
		    avolid);
	    PrintError("", code);
	    if (!error) error = code;
	}
    }

    if (aconn) rx_DestroyConnection(aconn);
    return error;
}

/* add recovery to UV_MoveVolume */

#define TESTC	0	/* set to test recovery code, clear for production */

jmp_buf env;
int interrupt=0;

void sigint_handler(x)
{
	if(interrupt)
		longjmp(env,0);

	fprintf(STDOUT,
		"\nSIGINT handler: vos move operation in progress\n");
	fprintf(STDOUT,
		"WARNING: may leave AFS storage and metadata in indeterminate state\n");
	fprintf(STDOUT,
		"enter second control-c to exit\n");
	fflush(STDOUT);

	interrupt=1;
	signal(SIGINT,sigint_handler);

	return;
}

#define ONERR(ec, es, ep) if (ec) { fprintf(STDERR, (es), (ep)); PrintError("   ",ec); error = (ec); goto mfail; }

/* Move volume <afromvol> on <afromserver> <afrompart> to <atoserver>
 * <atopart>. The operation is almost idempotent 
 */

UV_MoveVolume(afromvol, afromserver, afrompart, atoserver, atopart)
     afs_int32 afromvol; 
     afs_int32 afromserver, atoserver;
     afs_int32 afrompart,   atopart;
{
    struct rx_connection *toconn, *fromconn ;
    afs_int32                fromtid, totid, clonetid;
    char                 vname[64];
    char                 *volName = 0;
    char                 tmpName[VOLSER_MAXVOLNAME +1];
    afs_int32                rcode;
    afs_int32                fromDate;
    struct restoreCookie cookie;
    register afs_int32       vcode, code;
    afs_int32                newVol, volid, backupId;
    struct volser_status tstatus;
    struct destServer    destination;

    struct nvldbentry    entry, storeEntry;
    int                  i, islocked, pntg;
    afs_int32                error;
    char                 in,lf;	                        /* for test code */
    int                  same;

#ifdef	ENABLE_BUGFIX_1165
    volEntries volumeInfo;
    struct volintInfo *infop = 0;
#endif

    islocked = 0;
    fromconn = (struct rx_connection *)0;
    toconn   = (struct rx_connection *)0;
    fromtid  = 0;
    totid    = 0;
    clonetid = 0;
    error    = 0;
    volid    = 0;
    pntg     = 0;
    backupId = 0;
    newVol   = 0;

    /* support control-c processing */
    if (setjmp(env)) goto mfail;
    (void) signal(SIGINT,sigint_handler);
 
    if (TESTC)
    {
        fprintf(STDOUT,
		"\nThere are three tests points - verifies all code paths through recovery.\n");
	fprintf(STDOUT,"First test point - operation not started.\n");
	fprintf(STDOUT,"...test here (y, n)? ");
	fflush(STDOUT);
	fscanf(stdin,"%c",&in);
	fscanf(stdin,"%c",&lf);	/* toss away */
	if (in=='y')
	{
	    fprintf(STDOUT,"type control-c\n");
	    while(1)
	    {
	        fprintf(stdout,".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }

    vcode = VLDB_GetEntryByID(afromvol, -1, &entry);
    ONERR (vcode, "Could not fetch the entry for the volume  %u from the VLDB \n", afromvol);

    if (entry.volumeId[RWVOL] != afromvol)
    {
	fprintf(STDERR,"Only RW volume can be moved\n");
	exit(1);
    }

    vcode = ubik_Call(VL_SetLock, cstruct, 0,afromvol, RWVOL, VLOP_MOVE);
    ONERR (vcode, "Could not lock entry for volume %u \n", afromvol);
    islocked = 1;

    vcode = VLDB_GetEntryByID (afromvol, RWVOL, &entry);
    ONERR (vcode, "Could not fetch the entry for the volume  %u from the VLDB \n", afromvol);

    backupId = entry.volumeId[BACKVOL];
    MapHostToNetwork(&entry);

    if ( !Lp_Match(afromserver, afrompart, &entry) )
    {
	/* the from server and partition do not exist in the vldb entry corresponding to volid */
	if ( !Lp_Match(atoserver, atopart, &entry) ) 
	{
	    /* the to server and partition do not exist in the vldb entry corresponding to volid */
	    fprintf(STDERR,"The volume %u is not on the specified site. \n", afromvol);
	    fprintf(STDERR,"The current site is :");
	    for (i=0; i<entry.nServers; i++)
	    {
		if (entry.serverFlags[i] == ITSRWVOL)
		{
		    char pname[10];
		    MapPartIdIntoName(entry.serverPartition[i],pname);
		    fprintf(STDERR," server %s partition %s \n",
			    hostutil_GetNameByINet(entry.serverNumber[i]), pname);
		}
	    }
	    vcode = ubik_Call(VL_ReleaseLock, cstruct, 0, afromvol, -1,
			      (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	    ONERR (vcode, " Could not release lock on the VLDB entry for the volume %u \n",
		   afromvol);

	    return VOLSERVOLMOVED;
	}

	/* delete the volume afromvol on src_server */
	/* from-info does not exist but to-info does =>
	 * we have already done the move, but the volume
	 * may still be existing physically on from fileserver
	 */
	fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
	fromtid = 0;
	pntg = 1;

	code = AFSVolTransCreate(fromconn, afromvol, afrompart, ITOffline, &fromtid);
	if (!code) 
	{   /* volume exists - delete it */
	    code = AFSVolSetFlags(fromconn, fromtid, VTDeleteOnSalvage | VTOutOfService);
	    ONERR (code, "Failed to set flags on the volume %u\n", afromvol);

	    code = AFSVolDeleteVolume(fromconn,fromtid);
	    ONERR (code, "Failed to delete the volume %u\n", afromvol);
	    
	    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	    fromtid = 0;
	    if (!code) code = rcode;
	    ONERR (code, "Could not end the transaction for the volume %u \n", afromvol);
	}

	/*delete the backup volume now */
	fromtid = 0;
	code = AFSVolTransCreate(fromconn, backupId, afrompart, ITOffline, &fromtid);
	if (!code) 
	{   /* backup volume exists - delete it */
	    code = AFSVolSetFlags(fromconn, fromtid, VTDeleteOnSalvage | VTOutOfService);
	    ONERR (code, "Failed to set flags on the backup volume %u\n", backupId);

	    code = AFSVolDeleteVolume(fromconn,fromtid);
	    ONERR (code, "Could not delete the backup volume %u\n", backupId);

	    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	    fromtid = 0;
	    if (!code) code = rcode;
	    ONERR (code,"Could not end the transaction for the backup volume %u \n",backupId);
	}

	fromtid = 0;
	error = 0;
	goto mfail;
    }

    /* From-info matches the vldb info about volid,
     * its ok start the move operation, the backup volume 
     * on the old site is deleted in the process 
     */
    if (afrompart == atopart) 
    {
        same = VLDB_IsSameAddrs (afromserver, atoserver, &error);
	if (error) 
	{
	    fprintf(STDERR, "Failed to get info about server's %d address(es) from vlserver (err=%d); aborting call!\n", 
		    afromserver, error);
	    goto mfail;
	}
	if (same) ONERR (VOLSERVOLMOVED, 
			 "Warning: Moving volume %u to its home partition ignored!\n", afromvol);
    }

    pntg = 1;
    toconn   = UV_Bind(atoserver,   AFSCONF_VOLUMEPORT); /* get connections to the servers */
    fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
    fromtid = totid = 0;	/* initialize to uncreated */

    /* ***
     * clone the read/write volume locally.
     * ***/

    if (verbose) fprintf(STDOUT,"Starting transaction on source volume %u ...",afromvol);
    fflush(STDOUT);
    code = AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &fromtid);
    ONERR (code, "Failed to create transaction on the volume %u\n", afromvol);
    if (verbose) fprintf(STDOUT," done\n");

    /* Get a clone id */
    newVol = 0;
    vcode = ubik_Call (VL_GetNewVolumeId, cstruct, 0, 1, &newVol);
    ONERR (vcode, "Could not get an ID for the clone of volume %u from the VLDB\n", afromvol);

    /* Do the clone. Default flags on clone are set to delete on salvage and out of service */
    if (verbose) fprintf (STDOUT,"Cloning source volume %u ...", afromvol);
    fflush(STDOUT);
    strcpy(vname, "move-clone-temp");
    code = AFSVolClone(fromconn, fromtid, 0,readonlyVolume, vname, &newVol);
    ONERR (code, "Failed to clone the source volume %u\n", afromvol);
    if (verbose) fprintf(STDOUT," done\n");

    /* lookup the name of the volume we just cloned */
    volid = afromvol;
    code = AFSVolGetName(fromconn, fromtid, &volName);
    ONERR (code, "Failed to get the name of the volume %u\n", newVol);

    if (verbose) fprintf (STDOUT,"Ending the transaction on the source volume %u ...", afromvol);
    fflush(STDOUT);
    rcode = 0;
    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!code) code = rcode;
    ONERR (code, "Failed to end the transaction on the source volume %u\n", afromvol);
    if (verbose) fprintf (STDOUT," done\n");

    /* ***
     * Create the destination volume
     * ***/

    if (verbose) fprintf(STDOUT, "Starting transaction on the cloned volume %u ...", newVol);
    fflush(STDOUT);
    code = AFSVolTransCreate (fromconn, newVol, afrompart, ITOffline, &clonetid);
    ONERR (code, "Failed to start a transaction on the cloned volume%u\n", newVol);
    if (verbose) fprintf(STDOUT," done\n");

    code = AFSVolSetFlags (fromconn, clonetid, VTDeleteOnSalvage|VTOutOfService); /*redundant */
    ONERR (code, "Could not set falgs on the cloned volume %u\n", newVol);

    /* remember time from which we've dumped the volume */
    code = AFSVolGetStatus (fromconn, clonetid, &tstatus);
    ONERR (code, "Failed to get the status of the cloned volume %u\n", newVol);

    fromDate = tstatus.creationDate-CLOCKSKEW;

#ifdef	ENABLE_BUGFIX_1165
    /*
     * Get the internal volume state from the source volume. We'll use such info (i.e. dayUse)
     * to copy it to the new volume (via AFSSetInfo later on) so that when we move volumes we
     * don't use this information...
     */
    volumeInfo.volEntries_val = (volintInfo *)0;/*this hints the stub to allocate space*/
    volumeInfo.volEntries_len = 0;
    code = AFSVolListOneVolume(fromconn, afrompart, afromvol, &volumeInfo);
    ONERR (code, "Failed to get the volint Info of the cloned volume %u\n", afromvol);

    infop = (volintInfo *) volumeInfo.volEntries_val;
    infop->maxquota = -1;			/* Else it will replace the default quota */
#endif

    /* create a volume on the target machine */
    volid = afromvol;
    code = AFSVolTransCreate (toconn, volid, atopart, ITOffline, &totid);
    if (!code) 
    {  
      /* Delete the existing volume.
       * While we are deleting the volume in these steps, the transaction
       * we started against the cloned volume (clonetid above) will be
       * sitting idle. It will get cleaned up after 600 seconds
       */
        if (verbose) fprintf(STDOUT,"Deleting pre-existing volume %u on destination ...",volid);
	fflush(STDOUT);

	code = AFSVolDeleteVolume(toconn, totid);
	ONERR (code, "Could not delete the pre-existing volume %u on destination\n", volid);
	
	code = AFSVolEndTrans(toconn, totid, &rcode);
	totid = 0;
	if (!code) code = rcode;
	ONERR (code, "Could not end the transaction on pre-existing volume %u on destination\n",
	       volid);

	if (verbose) fprintf(STDOUT," done\n");
    }

    if (verbose) fprintf(STDOUT,"Creating the destination volume %u ...",volid);
    fflush(STDOUT);
    code = AFSVolCreateVolume (toconn, atopart, volName, volser_RW, volid, &volid, &totid);
    ONERR (code, "Failed to create the destination volume %u\n", volid);
    if (verbose) fprintf(STDOUT," done\n");

    strncpy(tmpName, volName, VOLSER_OLDMAXVOLNAME);
    free(volName);
    volName = (char *) 0;

    code = AFSVolSetFlags (toconn, totid, (VTDeleteOnSalvage | VTOutOfService));
    ONERR(code, "Failed to set the flags on the destination volume %u\n", volid);

    /***
     * Now dump the clone to the new volume
     ***/

    destination.destHost = ntohl(atoserver);
    destination.destPort = AFSCONF_VOLUMEPORT;
    destination.destSSID = 1;

    /* Copy the clone to the new volume */
    if (verbose) fprintf(STDOUT, "Dumping from clone %u on source to volume %u on destination ...",
			 newVol, afromvol);
    fflush(STDOUT);
    strncpy(cookie.name,tmpName,VOLSER_OLDMAXVOLNAME);
    cookie.type   = RWVOL;
    cookie.parent = entry.volumeId[RWVOL];
    cookie.clone  = 0;
    code = AFSVolForward(fromconn, clonetid, 0, &destination, totid, &cookie);
    ONERR (code, "Failed to move data for the volume %u\n", volid);
    if (verbose) fprintf(STDOUT," done\n");

    code = AFSVolEndTrans(fromconn, clonetid, &rcode);
    if (!code) code = rcode;
    clonetid = 0;
    ONERR (code, "Failed to end the transaction on the cloned volume %u\n", newVol);

    /* ***
     * reattach to the main-line volume, and incrementally dump it.
     * ***/

    if (verbose) 
        fprintf(STDOUT,"Doing the incremental dump from source to destination for volume %u ... ", 
		afromvol);
    fflush(STDOUT);
    
    code = AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &fromtid);
    ONERR (code, "Failed to create a transaction on the source volume %u\n", afromvol);

    /* now do the incremental */
    code = AFSVolForward(fromconn, fromtid, fromDate, &destination, totid,&cookie);
    ONERR (code, "Failed to do the incremental dump from rw volume on old site to rw volume on newsite\n", 0);
    if (verbose)fprintf(STDOUT," done\n");

    /* now adjust the flags so that the new volume becomes official */
    code = AFSVolSetFlags(fromconn, fromtid, VTOutOfService);
    ONERR (code, "Failed to set the flags to make old source volume offline\n", 0);

    code = AFSVolSetFlags(toconn, totid, 0);
    ONERR (code, "Failed to set the flags to make new source volume online\n", 0);

#ifdef	ENABLE_BUGFIX_1165
    code = AFSVolSetInfo(toconn, totid, infop);
    ONERR (code, "Failed to set volume status on the destination volume %u\n", volid);
#endif

    /* put new volume online */
    code = AFSVolEndTrans(toconn, totid, &rcode);
    totid = 0;
    if (!code) code = rcode;
    ONERR (code, "Failed to end the transaction on the volume %u on the new site\n", afromvol);

    Lp_SetRWValue(&entry, afromserver, afrompart, atoserver, atopart);
    MapNetworkToHost(&entry,&storeEntry);
    storeEntry.flags &= ~BACK_EXISTS;

    if (TESTC)
    {
        fprintf(STDOUT, "Second test point - operation in progress but not complete.\n");
	fprintf(STDOUT,"...test here (y, n)? ");
	fflush(STDOUT);
	fscanf(stdin,"%c",&in);
	fscanf(stdin,"%c",&lf);	/* toss away */
	if (in=='y')
	{
	    fprintf(STDOUT,"type control-c\n");
	    while(1)
	    {
	        fprintf(stdout,".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }

    vcode = VLDB_ReplaceEntry (afromvol, -1, &storeEntry, 
			       (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
    if (vcode) 
    {
        fprintf(STDERR," Could not release the lock on the VLDB entry for the volume %s %u \n",
		storeEntry.name,afromvol);
	error = vcode;
	goto mfail;
    }
    islocked=0;

    if (TESTC)
    {
        fprintf(STDOUT, "Third test point - operation complete but no cleanup.\n");
	fprintf(STDOUT,"...test here (y, n)? ");
	fflush(STDOUT);
	fscanf(stdin,"%c",&in);
	fscanf(stdin,"%c",&lf);	/* toss away */
	if (in=='y')
	{
	    fprintf(STDOUT,"type control-c\n");
	    while(1)
	    {
	        fprintf(stdout,".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }

#ifdef notdef
    /* This is tricky.  File server is very stupid, and if you mark the volume
     * as VTOutOfService, it may mark the *good* instance (if you're moving
     * between partitions on the same machine) as out of service.  Since
     * we're cleaning this code up in DEcorum, we're just going to kludge around
     * it for now by removing this call. */
    /* already out of service, just zap it now */
    code = AFSVolSetFlags(fromconn, fromtid, VTDeleteOnSalvage | VTOutOfService);
    if (code)
    {
        fprintf(STDERR,"Failed to set the flags to make the old source volume offline\n");
	goto mfail;
    }
#endif
    if (atoserver != afromserver) 
    {
        /* set forwarding pointer for moved volumes */
        code = AFSVolSetForwarding(fromconn, fromtid, atoserver);
	ONERR (code, "Failed to set the forwarding pointer for the volume %u\n", afromvol);
    }

    if (verbose) fprintf(STDOUT,"Deleting old volume %u on source ...", afromvol);
    fflush(STDOUT);

    code = AFSVolDeleteVolume(fromconn,fromtid);	/* zap original volume */
    ONERR (code, "Failed to delete the old volume %u on source\n", afromvol); 
    
    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!code) code = rcode;
    ONERR (code, "Failed to end the transaction on the old volume %u on the source\n", afromvol);

    if (verbose) fprintf(STDOUT," done\n");

    /* Delete the backup volume on the original site */
    code = AFSVolTransCreate(fromconn, backupId, afrompart, ITOffline, &fromtid);
    if (!code) 
    {
        fprintf(STDOUT, "WARNING : Deleting the backup volume %u on the source ...",backupId);
	fflush(STDOUT);

	code = AFSVolSetFlags(fromconn, fromtid, VTDeleteOnSalvage | VTOutOfService);
	ONERR (code, "Failed to set the flags on the backup volume on source\n", 0);

	code = AFSVolDeleteVolume(fromconn,fromtid);
	ONERR (code, "Failed to delete the backup volume on source\n", 0);
	
	code = AFSVolEndTrans(fromconn,fromtid, &rcode);
	fromtid = 0;
	if (!code) code = rcode;
	ONERR (code, "Failed to end the transaction on the backup volume %u on source\n", 0);

	fprintf(STDOUT," done\n");
    }
    else code = 0;		/* no backup volume? that's okay */

    fromtid = 0;
    if (verbose) fprintf(STDOUT,"Starting transaction on the cloned volume %u ...",newVol);
    fflush(STDOUT);

    code = AFSVolTransCreate(fromconn, newVol, afrompart, ITOffline, &clonetid);
    ONERR (code, "Failed to start a transaction on the cloned volume%u\n", newVol);

    if (verbose) fprintf(STDOUT," done\n");
    
    /* now delete the clone */
    if (verbose) fprintf(STDOUT,"Deleting the clone %u ...", newVol);
    fflush(STDOUT);

    code = AFSVolDeleteVolume(fromconn, clonetid);
    ONERR (code, "Failed to delete the cloned volume %u\n", newVol);
    
    if (verbose) fprintf(STDOUT," done\n");

    code = AFSVolEndTrans(fromconn, clonetid, &rcode);
    if (!code) code = rcode;
    clonetid = 0;
    ONERR (code, "Failed to end the transaction on the cloned volume %u\n", newVol);

    /* fall through */
    /* END OF MOVE */

    if (TESTC)
    {
        fprintf(STDOUT,"Fourth test point - operation complete.\n");
	fprintf(STDOUT,"...test here (y, n)? ");
	fflush(STDOUT);
	fscanf(stdin,"%c",&in);
	fscanf(stdin,"%c",&lf);	/* toss away */
	if (in=='y')
	{
	    fprintf(STDOUT,"type control-c\n");
	    while(1)
	    {
	        fprintf(stdout,".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }

    /* normal cleanup code */

    if (entry.flags & RO_EXISTS) fprintf(STDERR,"WARNING : readOnly copies still exist \n");

    if (islocked)
    {
        vcode = ubik_Call(VL_ReleaseLock,cstruct, 0, afromvol, -1, 
			  (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
        if (vcode) 
	{
	    fprintf(STDERR," Could not release the lock on the VLDB entry for the volume %u \n",
		    afromvol);
	    if (!error) error = vcode;
	}
    }
    
    if (fromtid) 
    {
	code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (code || rcode)
	{
	    fprintf(STDERR,"Could not end transaction on the source's clone volume %u\n", newVol);
	    if (!error) error = (code ? code : rcode);
	}
    }

    if (clonetid) 
    {
	code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	if (code || rcode) 
	{
	    fprintf(STDERR,"Could not end transaction on the source's clone volume %u\n",newVol);
	    if (!error) error = (code ? code : rcode);
	}
    }

    if (totid) 
    {
	code = AFSVolEndTrans(toconn, totid, &rcode);
	if (code) 
	{
	    fprintf(STDERR,"Could not end transaction on destination volume %u\n",afromvol);
	    if (!error) error = (code ? code : rcode);
	}
    }
    if (volName) free(volName);
#ifdef	ENABLE_BUGFIX_1165
    if (infop)   free(infop);
#endif
    if (fromconn) rx_DestroyConnection(fromconn);
    if (toconn)   rx_DestroyConnection(toconn);
    PrintError("",error);
    return error;

    /* come here only when the sky falls */
mfail:

    if (pntg) 
    {
	fprintf(STDOUT,"vos move: operation interrupted, cleanup in progress...\n");
	fprintf(STDOUT,"clear transaction contexts\n");
	fflush(STDOUT);
    }

    /* unlock VLDB entry */
    if (islocked)
        ubik_Call(VL_ReleaseLock, cstruct, 0, afromvol, -1,
		  (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));

    if (clonetid) AFSVolEndTrans(fromconn, clonetid, &rcode);
    if (totid)    AFSVolEndTrans(toconn, totid, &rcode);
    if (fromtid)
    {  /* put it on-line */
        AFSVolSetFlags(fromconn,fromtid,0);
	AFSVolEndTrans(fromconn, fromtid, &rcode);
    }

    if (verbose) 
    {	/* get current VLDB entry */
	fprintf(STDOUT,"access VLDB\n");
	fflush(STDOUT);
    }
    vcode= VLDB_GetEntryByID (afromvol, -1, &entry);
    if (vcode)
    {
        fprintf(STDOUT,"FATAL: VLDB access error: abort cleanup\n");
	fflush(STDOUT);
	goto done;
    }
    MapHostToNetwork(&entry);

    /* Delete either the volume on the source location or the target location. 
     * If the vldb entry still points to the source location, then we know the
     * volume move didn't finish so we remove the volume from the target 
     * location. Otherwise, we remove the volume from the source location.
     */
    if (Lp_Match(afromserver,afrompart,&entry)) {  /* didn't move - delete target volume */
        if (pntg) {
	    fprintf(STDOUT,
		    "move incomplete - attempt cleanup of target partition - no guarantee\n");
	    fflush(STDOUT);
	}

	if (volid && toconn) {
	    code=AFSVolTransCreate(toconn,volid,atopart, ITOffline,&totid);
	    if (!code) {
	        AFSVolSetFlags(toconn,totid, VTDeleteOnSalvage | VTOutOfService);
		AFSVolDeleteVolume(toconn,totid);
		AFSVolEndTrans(toconn,totid,&rcode);
	    }
	}

	/* put source volume on-line */
	if (fromconn) {
	    code=AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &fromtid);
	    if (!code) {
	        AFSVolSetFlags(fromconn,fromtid,0);
		AFSVolEndTrans(fromconn,fromtid,&rcode);
	    }
	}
    }
    else {	/* yep, move complete */
        if (pntg) {
	    fprintf(STDOUT,
		    "move complete - attempt cleanup of source partition - no guarantee\n");
	    fflush(STDOUT);
	}

	/* delete backup volume */
	if (fromconn) {
	    code=AFSVolTransCreate (fromconn,backupId,afrompart, ITOffline,&fromtid);
	    if (!code) {
	        AFSVolSetFlags(fromconn,fromtid, VTDeleteOnSalvage | VTOutOfService);
		AFSVolDeleteVolume(fromconn,fromtid);
		AFSVolEndTrans(fromconn,fromtid,&rcode);
	    }

	    /* delete source volume */
	    code=AFSVolTransCreate (fromconn, afromvol, afrompart, ITBusy, &fromtid);
	    if (!code) {
	        AFSVolSetFlags(fromconn,fromtid, VTDeleteOnSalvage | VTOutOfService);
		if (atoserver != afromserver)
		    AFSVolSetForwarding(fromconn,fromtid,atoserver);
		AFSVolDeleteVolume(fromconn,fromtid);
		AFSVolEndTrans(fromconn,fromtid,&rcode);
	    }
	}
    }

    /* common cleanup - delete local clone */
    if (newVol) {
        code = AFSVolTransCreate (fromconn, newVol, afrompart, ITOffline, &clonetid);
	if (!code) {
	    AFSVolDeleteVolume(fromconn,clonetid);
	    AFSVolEndTrans(fromconn,clonetid,&rcode);
	}
    }

    /* unlock VLDB entry */
    ubik_Call (VL_ReleaseLock, cstruct, 0, afromvol, -1,
	       (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));

done:	/* routine cleanup */
    if (volName)  free(volName);
#ifdef	ENABLE_BUGFIX_1165
    if (infop)    free(infop);
#endif
    if (fromconn) rx_DestroyConnection(fromconn);
    if (toconn)   rx_DestroyConnection(toconn);

    if (pntg) {
	fprintf(STDOUT,"cleanup complete - user verify desired result\n");
	fflush(STDOUT);
    }
    exit(1);
}

/* Make a new backup of volume <avolid> on <aserver> and <apart> 
 * if one already exists, update it 
 */

UV_BackupVolume(aserver, apart, avolid)
    afs_int32 aserver, apart, avolid;
{
    struct rx_connection *aconn = (struct rx_connection *)0;
    afs_int32 ttid = 0, btid = 0;
    afs_int32 backupID;
    afs_int32 code = 0, rcode = 0;
    char vname[VOLSER_MAXVOLNAME +1];
    struct nvldbentry entry, storeEntry;
    afs_int32 error = 0;
    int vldblocked = 0, vldbmod = 0, backexists = 1;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    /* the calls to VLDB will succeed only if avolid is a RW volume,
     * since we are following the RW hash chain for searching */
    code = VLDB_GetEntryByID(avolid, RWVOL, &entry); 
    if (code) {
       fprintf(STDERR, "Could not fetch the entry for the volume %u from the VLDB \n", avolid);
       error = code; goto bfail;
    }
    MapHostToNetwork(&entry); 

    /* These operations require the VLDB be locked since it means the VLDB
     * will change or the vldb is already locked.
     */
    if (!(entry.flags & BACK_EXISTS)   ||               /* backup volume doesnt exist */
	 (entry.flags & VLOP_ALLOPERS) ||               /* vldb lock already held */
	 (entry.volumeId[BACKVOL] == INVALID_BID)) {    /* no assigned backup volume id */

       code = ubik_Call(VL_SetLock,cstruct, 0, avolid, RWVOL, VLOP_BACKUP);
       if (code) {
	  fprintf(STDERR,"Could not lock the VLDB entry for the volume %u\n",avolid);
	  error = code;
	  goto bfail;
       }
       vldblocked = 1;

       /* Reread the vldb entry */
       code = VLDB_GetEntryByID(avolid, RWVOL, &entry);
       if (code) {
	  fprintf(STDERR,"Could not fetch the entry for the volume %u from the VLDB \n",avolid);
	  error = code;
	  goto bfail;
       }
       MapHostToNetwork(&entry);
    }

    if (!ISNAMEVALID(entry.name)) {
       fprintf(STDERR, "Name of the volume %s exceeds the size limit\n", entry.name);
       error = VOLSERBADNAME;
       goto bfail;
    }

    backupID = entry.volumeId[BACKVOL];
    if (backupID == INVALID_BID) {
       /* Get a backup volume id from the VLDB and update the vldb
	* entry with it. 
	*/
       code = ubik_Call(VL_GetNewVolumeId, cstruct, 0, 1, &backupID);
       if (code) {
	  fprintf(STDERR,
		  "Could not allocate ID for the backup volume of  %u from the VLDB\n",
		  avolid);
	  error = code;
	  goto bfail;
       }
       entry.volumeId[BACKVOL] = backupID;
       vldbmod = 1;
    }

    /* Test to see if the backup volume exists by trying to create
     * a transaction on the backup volume. We've assumed the backup exists.
     */
    code = AFSVolTransCreate(aconn, backupID, apart, ITOffline, &btid);
    if (code) {
       if (code != VNOVOL) {
	  fprintf(STDERR,"Could not reach the backup volume %u\n", backupID);
	  error = code;
	  goto bfail;
       }
       backexists = 0;                 /* backup volume does not exist */
    }
    if (btid) {
       code = AFSVolEndTrans(aconn, btid, &rcode);
       btid = 0;
       if (code || rcode) {
	  fprintf(STDERR,
		  "Could not end transaction on the previous backup volume %u\n",
		  backupID);
	  error = (code ? code : rcode);
	  goto bfail;
       }
    }

    /* Now go ahead and try to clone the RW volume.
     * First start a transaction on the RW volume 
     */
    code = AFSVolTransCreate(aconn, avolid, apart, ITBusy, &ttid);
    if (code) {
       fprintf(STDERR,"Could not start a transaction on the volume %u\n",avolid);
       error = code;
       goto bfail;
    }

    /* Clone or reclone the volume, depending on whether the backup 
     * volume exists or not
     */
    if (backexists) {
       if(verbose) 
	  fprintf(STDOUT,"Re-cloning backup volume %u ...", backupID);
       fflush(STDOUT);

       code = AFSVolReClone(aconn, ttid, backupID);
       if (code) {
	  fprintf(STDERR,"Could not re-clone backup volume %u\n", backupID);
	  error = code;
	  goto bfail;
       }
    }
    else {
       if (verbose)
	  fprintf(STDOUT,"Creating a new backup clone %u ...", backupID);
       fflush(STDOUT);

       strcpy(vname, entry.name);
       strcat(vname,".backup");

       code = AFSVolClone(aconn, ttid, 0,backupVolume, vname, &backupID);
       if (code) {
	  fprintf(STDERR,"Failed to clone the volume %u\n",avolid);
	  error = code;
	  goto bfail;
       }
    }

    /* End the transaction on the RW volume */
    code = AFSVolEndTrans(aconn, ttid, &rcode);
    ttid = 0;
    if (code || rcode) {
	fprintf(STDERR, "Failed to end the transaction on the rw volume %u\n", avolid); 
	error = (code ? code : rcode);
	goto bfail;
    }

    /* Mork vldb as backup exists */
    if (!(entry.flags & BACK_EXISTS)) {
       entry.flags |= BACK_EXISTS;
       vldbmod = 1;
    }

    /* Now go back to the backup volume and bring it on line */
    code = AFSVolTransCreate(aconn, backupID, apart, ITOffline, &btid);
    if (code) {
	fprintf(STDERR,"Failed to start a transaction on the backup volume %u\n",backupID); 
	error = code;
	goto bfail;
    }

    code = AFSVolSetFlags(aconn, btid, 0);
    if (code) {
	fprintf(STDERR,"Could not mark the backup volume %u on line \n",backupID);
	error = code;
	goto bfail;
    }

    code = AFSVolEndTrans(aconn, btid, &rcode);
    btid = 0;
    if (code || rcode) {
	fprintf(STDERR, "Failed to end the transaction on the backup volume %u\n", backupID);
	error = (code ? code : rcode);
	goto bfail;
    }

    if (verbose)
	fprintf(STDOUT,"done\n");

    /* Will update the vldb below */

  bfail:
    if (ttid) {
       code =  AFSVolEndTrans(aconn, ttid, &rcode);
       if (code || rcode) {
	  fprintf(STDERR, "Could not end transaction on the volume %u\n", avolid);
	  if (!error)
	     error = (code ? code : rcode);
       }
    }

    if (btid) {
       code = AFSVolEndTrans(aconn, btid, &rcode);
       if (code || rcode) {
	  fprintf(STDERR,"Could not end transaction the backup volume %u\n",backupID);
	  if (!error)
	     error = (code ? code : rcode);
       }
    }

    /* Now update the vldb - if modified */
    if (vldblocked) {
       if (vldbmod) {
	  MapNetworkToHost(&entry,&storeEntry);
	  code = VLDB_ReplaceEntry(avolid, RWVOL, &storeEntry,
				    (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	  if (code) {
	     fprintf(STDERR,"Could not update the VLDB entry for the volume %u \n",avolid);
	     if (!error)
	        error = code;
	  }
       }
       else {
	  code = ubik_Call(VL_ReleaseLock,cstruct, 0, avolid, RWVOL, 
			   (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	  if (code) {
	     fprintf(STDERR,"Could not unlock the VLDB entry for the volume %u \n",avolid);
	     if (!error)
	        error = code;
	  }
       }
    }

    if (aconn)
       rx_DestroyConnection(aconn);

    PrintError("",error);
    return error;
}

static int DelVol (conn, vid, part, flags)
struct rx_connection *conn;
afs_int32 vid, part, flags;
{
  afs_int32 acode, bcode, ccode, rcode, tid;
  ccode = rcode = tid = 0;

  acode = AFSVolTransCreate(conn, vid, part, flags, &tid);
  if (!acode) {              /* It really was there */
    acode = AFSVolDeleteVolume(conn, tid);
    if (acode) {
      fprintf(STDERR, "Failed to delete volume %u.\n", vid);
      PrintError ("", acode);
    }
    ccode = AFSVolEndTrans(conn, tid, &rcode);
    if (!ccode) 
      ccode = rcode;
    if (ccode) {
      fprintf(STDERR, "Failed to end transaction on volume %u.\n", vid);
      PrintError ("", ccode);
    }
  }

return acode;
}

#define ONERROR(ec, ep, es) if (ec) { fprintf(STDERR, (es), (ep)); error = (ec); goto rfail; }
#define ERROREXIT(ec) { error = (ec); goto rfail; }

/* Get a "transaction" on this replica.  Create the volume 
 * if necessary.  Return the time from which a dump should
 * be made (0 if it's a new volume)
 */
static int GetTrans (vldbEntryPtr, index, connPtr, transPtr, timePtr)
  struct nvldbentry    *vldbEntryPtr;
  afs_int32                index;
  struct rx_connection **connPtr;
  afs_int32                *transPtr, *timePtr;
{
  afs_int32 volid;
  struct volser_status tstatus;
  int code, rcode, tcode;
  
  *connPtr  = (struct rx_connection *)0;
  *timePtr  = 0;
  *transPtr = 0;

  /* get connection to the replication site */
  *connPtr = UV_Bind(vldbEntryPtr->serverNumber[index], AFSCONF_VOLUMEPORT);
  if (!*connPtr) goto fail;                                   /* server is down */

  volid = vldbEntryPtr->volumeId[ROVOL];
  if (volid) 
    code = AFSVolTransCreate(*connPtr, volid, vldbEntryPtr->serverPartition[index], 
			     ITOffline, transPtr);

  /* If the volume does not exist, create it */
  if (!volid || code) {
      char volname[64];

      if (volid && (code != VNOVOL)){
	  PrintError("Failed to start a transaction on the RO volume.\n",
		     code);
	  goto fail;
      }

      strcpy(volname, vldbEntryPtr->name);
      strcat(volname, ".readonly");
      
      if (verbose) {
          fprintf(STDOUT,"Creating new volume %u on replication site %s: ", 
		  volid, hostutil_GetNameByINet(vldbEntryPtr->serverNumber[index]));
	  fflush(STDOUT);
      }

      code = AFSVolCreateVolume(*connPtr, vldbEntryPtr->serverPartition[index], 
				volname, volser_RO,
				vldbEntryPtr->volumeId[RWVOL], &volid, transPtr);
      if (code) {
	  PrintError("Failed to create the ro volume: ",code);
	  goto fail;
      }
      vldbEntryPtr->volumeId[ROVOL] = volid;

      if (verbose) fprintf(STDOUT,"done.\n");

      /* The following is a bit redundant, since create sets these flags by default */
      code = AFSVolSetFlags(*connPtr, *transPtr, VTDeleteOnSalvage | VTOutOfService);
      if (code) {
	  PrintError("Failed to set flags on the ro volume: ", code);
	  goto fail;
      }
  }

  /* Otherwise, the transaction did succeed, so get the creation date of the
   * latest RO volume on the replication site 
   */
  else {
      if (verbose) {
	  fprintf(STDOUT,"Updating existing ro volume %u on %s ...\n",
		  volid, hostutil_GetNameByINet(vldbEntryPtr->serverNumber[index]));
	  fflush(STDOUT);
      }

      code  = AFSVolGetStatus(*connPtr, *transPtr, &tstatus);
      if (code) {
	  PrintError("Failed to get status of volume on destination: ",code);
	  goto fail;
      }
      *timePtr = tstatus.creationDate-CLOCKSKEW;
  }
  
  return 0;

 fail:
  if (*transPtr) {
      tcode = AFSVolEndTrans(*connPtr, *transPtr, &rcode);
      *transPtr = 0;
      if (!tcode) tcode = rcode;
      if (tcode) PrintError("Could not end transaction on a ro volume: ", tcode);
  }

  return code;
}

static int SimulateForwardMultiple(fromconn, fromtid, fromdate, tr,
				   flags, cookie, results)
struct rx_connection *fromconn;
afs_int32 fromtid, fromdate, flags;
manyDests *tr;
void *cookie;
manyResults *results;
{
  int i;

  for (i=0; i<tr->manyDests_len; i++) {
     results->manyResults_val[i] = AFSVolForward(fromconn, fromtid,
				    fromdate, &(tr->manyDests_val[i].server), 
				    tr->manyDests_val[i].trans, cookie);
  }
  return 0;
}


static int rel_compar (r1, r2)
     struct release *r1, *r2;
{
  return (r1->time - r2->time);
}

/* UV_ReleaseVolume()
 *    Release volume <afromvol> on <afromserver> <afrompart> to all
 *    its RO sites (full release). Unless the previous release was
 *    incomplete: in which case we bring the remaining incomplete
 *    volumes up to date with the volumes that were released
 *    successfully.
 *    forceflag: Performs a full release.
 *
 *    Will create a clone from the RW, then dump the clone out to 
 *    the remaining replicas. If there is more than 1 RO sites,
 *    ensure that the VLDB says at least one RO is available all
 *    the time: Influences when we write back the VLDB entry.
 */

UV_ReleaseVolume(afromvol, afromserver, afrompart, forceflag)
     afs_int32 afromserver;
     afs_int32 afrompart;
     afs_int32 afromvol; 
     int forceflag;
{
  char vname[64];
  afs_int32 code, vcode, rcode, tcode;
  afs_int32 cloneVolId, roVolId;
  struct replica *replicas=0;
  struct nvldbentry entry,storeEntry;
  int i, volcount, k, m, n, fullrelease, vldbindex;
  int failure;
  struct restoreCookie cookie;
  struct rx_connection **toconns=0;
  struct release *times=0;
  int nservers = 0;
  struct rx_connection *fromconn = (struct rx_connection *)0;
  afs_int32 error = 0;
  int islocked = 0;
  afs_int32 clonetid=0, onlinetid;
  afs_int32 fromtid=0;
  afs_uint32 fromdate, thisdate;
  int ix, si, s;
  manyDests tr;
  manyResults results;
  int rwindex, roindex, roclone, roexists;
  afs_int32 rwcrdate;
  struct rtime {
    int     validtime;
    afs_uint32 time;
  } remembertime[NMAXNSERVERS];
  int releasecount = 0;
  struct volser_status volstatus;

  bzero((char *)remembertime, sizeof(remembertime));
  bzero((char *)&results, sizeof(results));

  vcode = ubik_Call(VL_SetLock, cstruct, 0, afromvol, RWVOL, VLOP_RELEASE);
  if (vcode != VL_RERELEASE) 
      ONERROR(vcode, afromvol, "Could not lock the VLDB entry for the volume %u.\n");
  islocked = 1;

  /* Get the vldb entry in readable format */
  vcode = VLDB_GetEntryByID (afromvol, RWVOL, &entry);
  ONERROR(vcode, afromvol, "Could not fetch the entry for the volume %u from the VLDB.\n");
  MapHostToNetwork(&entry);

  if (verbose)
     EnumerateEntry(&entry);

  if (!ISNAMEVALID(entry.name))
    ONERROR(VOLSERBADOP, entry.name, 
	    "Volume name %s is too long, rename before releasing.\n");
  if (entry.volumeId[RWVOL] != afromvol)
    ONERROR(VOLSERBADOP, afromvol, 
	    "The volume %u being released is not a read-write volume.\n");
  if (entry.nServers <= 1)  
    ONERROR(VOLSERBADOP, afromvol, 
	    "Volume %u has no replicas - release operation is meaningless!\n");
  if (strlen(entry.name) > (VOLSER_OLDMAXVOLNAME - 10)) 
    ONERROR(VOLSERBADOP, entry.name, 
	    "RO volume name %s exceeds (VOLSER_OLDMAXVOLNAME - 10) character limit\n");

  /* roclone is true if one of the RO volumes is on the same
   * partition as the RW volume. In this case, we make the RO volume
   * on the same partition a clone instead of a complete copy.
   */
  
  roindex = Lp_ROMatch(afromserver, afrompart, &entry) - 1;
  roclone = ((roindex == -1) ? 0 : 1);
  rwindex = Lp_GetRwIndex(&entry);
  if (rwindex < 0)
     ONERROR(VOLSERNOVOL, 0, "There is no RW volume \n");

  /* Make sure we have a RO volume id to work with */
  if (entry.volumeId[ROVOL] == INVALID_BID) {
      /* need to get a new RO volume id */
      vcode = ubik_Call(VL_GetNewVolumeId, cstruct, 0, 1, &roVolId);
      ONERROR(vcode, entry.name, "Cant allocate ID for RO volume of %s\n"); 

      entry.volumeId[ROVOL] = roVolId;
      MapNetworkToHost(&entry, &storeEntry);
      vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry, 0);
      ONERROR(vcode, entry.name, "Could not update vldb entry for %s.\n");
  }

  /* Will we be completing a previously unfinished release. -force overrides */
  for (fullrelease=1, i=0; (fullrelease && (i<entry.nServers)); i++) {
     if (entry.serverFlags[i] & NEW_REPSITE)
        fullrelease = 0;
  }
  if (forceflag && !fullrelease)
    fullrelease = 1;

  /* Determine which volume id to use and see if it exists */
  cloneVolId = ((fullrelease || (entry.cloneId == 0)) ? entry.volumeId[ROVOL] : entry.cloneId);
  code = VolumeExists(afromserver, afrompart, cloneVolId);
  roexists = ((code == ENODEV) ? 0 : 1);
  if (!roexists && !fullrelease)
     fullrelease = 1;      /* Do a full release if RO clone does not exist */

  if (verbose) {
     if (fullrelease) {
        fprintf(STDOUT,"This is a complete release of the volume %u\n", afromvol);
     } else {
        fprintf(STDOUT,"This is a completion of the previous release\n");
     }
  }
     
  fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
  if (!fromconn) 
     ONERROR(-1, afromserver, "Cannot establish connection with server 0x%x\n");
  
  if (fullrelease) {
     /* If the RO clone exists, then if the clone is a temporary
      * clone, delete it. Or if the RO clone is marked RO_DONTUSE
      * (it was recently added), then also delete it. We do not
      * want to "reclone" a temporary RO clone.
      */
     if ( roexists && 
	 (!roclone || (entry.serverFlags[roindex] & RO_DONTUSE)) ) {
	code = DelVol(fromconn, cloneVolId, afrompart, ITOffline);
	if (code && (code != VNOVOL))
	   ERROREXIT(code);
	roexists = 0;
     }

     /* Mark all the ROs in the VLDB entry as RO_DONTUSE. We don't
      * write this entry out to the vlserver until after the first
      * RO volume is released (temp RO clones don't count).
      */
     for (i=0; i<entry.nServers; i++) {
        entry.serverFlags[i] &= ~NEW_REPSITE;
        entry.serverFlags[i] |=  RO_DONTUSE;
     }
     entry.serverFlags[rwindex] |=  NEW_REPSITE;
     entry.serverFlags[rwindex] &= ~RO_DONTUSE;
      
     /* Begin transaction on RW and mark it busy while we clone it */
     code = AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &clonetid);
     ONERROR(code, afromvol, "Failed to start transaction on volume %u\n");

     /* Clone or reclone the volume */
     if (roexists) {
	if (verbose)
	   fprintf(STDERR, "Recloning RW volume ...\n");

	code = AFSVolReClone(fromconn, clonetid, cloneVolId);
	ONERROR(code, afromvol, "Failed to reclone the RW volume %u\n");
     } else {
	if (roclone) {
	   strcpy(vname, entry.name);
	   strcat(vname, ".readonly");
	   if (verbose)
	     fprintf(STDERR, "Cloning RW volume ...\n");
	} else {
	   strcpy(vname, "readonly-clone-temp");
	   if (verbose)
	     fprintf(STDERR, "Cloning RW volume to temporary RO ...\n");
	}
	code = AFSVolClone(fromconn, clonetid, 0, readonlyVolume, vname, &cloneVolId);
	ONERROR(code, afromvol, "Failed to clone the RW volume %u\n");
     }

     /* Get the time the RW was created for future information */
     code = AFSVolGetStatus(fromconn, clonetid, &volstatus);
     ONERROR(code, cloneVolId, "Failed to get the status of the RW volume %u\n");
     rwcrdate = volstatus.creationDate;

     /* End the transaction on the RW volume */
     code = AFSVolEndTrans(fromconn, clonetid, &rcode);
     clonetid = 0;
     ONERROR((code?code:rcode), cloneVolId, "Failed to end cloning transaction on RW %u\n");

     /* Remember clone volume ID in case we fail or are interrupted */
     entry.cloneId = cloneVolId;

     if (roclone) {
	/* Bring the RO clone online - though not if it's a temporary clone */
	code = AFSVolTransCreate(fromconn, cloneVolId, afrompart, ITOffline, &onlinetid);
	ONERROR(code, cloneVolId, "Failed to start transaction on volume %u\n");

	tcode = AFSVolSetFlags(fromconn, onlinetid, 0);

	code = AFSVolEndTrans(fromconn, onlinetid, &rcode);
	ONERROR((code?code:rcode), cloneVolId, "Failed to end transaction on RO clone %u\n");

	ONERROR(tcode, cloneVolId, "Could not bring volume %u on line\n");

	/* Sleep so that a client searching for an online volume won't
	 * find the clone offline and then the next RO offline while the 
	 * release brings the clone online and the next RO offline (race).
	 * There is a fix in the 3.4 client that does not need this sleep
	 * anymore, but we don't know what clients we have.
	 */
	if (entry.nServers > 2)
	   sleep(5);

        /* Mark the RO clone in the VLDB as a good site (already released)*/
        entry.serverFlags[roindex] |=  NEW_REPSITE;
        entry.serverFlags[roindex] &= ~RO_DONTUSE;
	entry.flags                |=  RO_EXISTS;

	releasecount++;

	/* Write out the VLDB entry only if the clone is not a temporary
	 * clone. If we did this to a temporary clone then we would end
	 * up marking all the ROs as "old release" making the ROs
	 * temporarily unavailable.
	 */
	MapNetworkToHost(&entry, &storeEntry);
	vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry, 0);
	ONERROR(vcode, entry.name, "Could not update vldb entry for %s.\n");
     }
  }  

  /* Now we will release from the clone to the remaining RO replicas.
   * The first 2 ROs (counting the non-temporary RO clone) are released
   * individually: releasecount. This is to reduce the race condition
   * of clients trying to find an on-line RO volume. The remaining ROs
   * are released in parallel but no more than half the number of ROs
   * (rounded up) at a time: nservers.
   */

  strcpy(vname, entry.name);
  strcat(vname, ".readonly");
  bzero(&cookie,sizeof(cookie));
  strncpy(cookie.name, vname, VOLSER_OLDMAXVOLNAME);
  cookie.type   = ROVOL;
  cookie.parent = entry.volumeId[RWVOL];
  cookie.clone  = 0;

  nservers = entry.nServers/2;           /* how many to do at once, excluding clone */
  replicas   = (struct replica *)        malloc (sizeof(struct replica)*nservers+1);
  times      = (struct release *)        malloc (sizeof(struct release)*nservers+1);
  toconns    = (struct rx_connection **) malloc (sizeof(struct rx_connection *)*nservers+1);
  results.manyResults_val = (afs_int32 *)    malloc (sizeof(afs_int32)*nservers+1);
  if ( !replicas || !times || !! !results.manyResults_val || !toconns ) 
      ONERROR(ENOMEM, 0, "Failed to create transaction on the release clone\n");

  bzero (replicas,   (sizeof(struct replica)*nservers+1));
  bzero (times,      (sizeof(struct release)*nservers+1));
  bzero (toconns,    (sizeof(struct rx_connection *)*nservers+1));
  bzero (results.manyResults_val, (sizeof(afs_int32)*nservers+1));

  /* Create a transaction on the cloned volume */
  code = AFSVolTransCreate(fromconn, cloneVolId, afrompart, ITBusy, &fromtid);
  if (!fullrelease && code)
     ONERROR(VOLSERNOVOL, afromvol,"Old clone is inaccessible. Try vos release -f %u.\n");
  ONERROR(code, 0, "Failed to create transaction on the release clone\n");

  /* For each index in the VLDB */
  for (vldbindex=0; vldbindex<entry.nServers; ) {

     /* Get a transaction on the replicas. Pick replacas which have an old release. */
     for (volcount=0; ((volcount<nservers) && (vldbindex<entry.nServers)); vldbindex++) {
	/* The first two RO volumes will be released individually.
	 * The rest are then released in parallel. This is a hack
	 * for clients not recognizing right away when a RO volume
	 * comes back on-line.
	 */
	if ((volcount == 1) && (releasecount < 2))
	   break;

        if (vldbindex == roindex) continue;              /* the clone    */
	if ( (entry.serverFlags[vldbindex] & NEW_REPSITE) &&
	    !(entry.serverFlags[vldbindex] & RO_DONTUSE) ) continue;
	if (!(entry.serverFlags[vldbindex] & ITSROVOL)) continue;  /* not a RO vol */


	/* Get a Transaction on this replica. Get a new connection if
	 * necessary.  Create the volume if necessary.  Return the
	 * time from which the dump should be made (0 if it's a new
	 * volume).  Each volume might have a different time. 
	 */
	replicas[volcount].server.destHost = ntohl(entry.serverNumber[vldbindex]);
	replicas[volcount].server.destPort = AFSCONF_VOLUMEPORT;
	replicas[volcount].server.destSSID = 1;
	times[volcount].vldbEntryIndex = vldbindex;
	
	code = GetTrans(&entry, vldbindex, &(toconns[volcount]), 
			&(replicas[volcount].trans), &(times[volcount].time));
	if (code) continue;

	/* Thisdate is the date from which we want to pick up all changes */
	if (forceflag || !fullrelease || (rwcrdate > times[volcount].time)) {
	   /* If the forceflag is set, then we want to do a full dump.
	    * If it's not a full release, we can't be sure that the creation
	    *  date is good (so we also do a full dump).
	    * If the RW volume was replaced (its creation date is newer than
	    *  the last release), then we can't be sure what has changed (so
	    *  we do a full dump).
	    */
	   thisdate = 0;
	} else if (remembertime[vldbindex].validtime) {
	   /* Trans was prev ended. Use the time from the prev trans
	    * because, prev trans may have created the volume. In which
	    * case time[volcount].time would be now instead of 0.
	    */
	   thisdate  = (remembertime[vldbindex].time < times[volcount].time) ? 
	                remembertime[vldbindex].time : times[volcount].time;
	} else {
	   thisdate = times[volcount].time;
	}	  
	remembertime[vldbindex].validtime = 1;
	remembertime[vldbindex].time = thisdate;

	if (volcount == 0) {
	   fromdate = thisdate;
	} else {
	   /* Include this volume if it is within 15 minutes of the earliest */
	   if (((fromdate>thisdate)?(fromdate-thisdate):(thisdate-fromdate)) > 900) {
	      AFSVolEndTrans(toconns[volcount], replicas[volcount].trans, &rcode);
	      replicas[volcount].trans = 0;
	      break;
	   }
	   if (thisdate < fromdate)
	      fromdate = thisdate;
	}
	volcount++;
     }
     if (!volcount) continue;

     if (verbose) {
        fprintf(STDOUT,"Starting ForwardMulti from %u to %u on %s",
		cloneVolId, entry.volumeId[ROVOL], 
		hostutil_GetNameByINet(entry.serverNumber[times[0].vldbEntryIndex]));

	for (s=1; s<volcount; s++) {
	   fprintf(STDOUT," and %s",
		   hostutil_GetNameByINet(entry.serverNumber[times[s].vldbEntryIndex]));
	}

	if (fromdate == 0)
	   fprintf(STDOUT," (full release)");
	fprintf(STDOUT,".\n");
	fflush(STDOUT);
     }

     /* Release the ones we have collected */
     tr.manyDests_val = &(replicas[0]);
     tr.manyDests_len = results.manyResults_len = volcount;
     code = AFSVolForwardMultiple(fromconn, fromtid, fromdate, &tr, 0/*spare*/, &cookie, &results);
     if (code == RXGEN_OPCODE) {               /* RPC Interface Mismatch */
        code = SimulateForwardMultiple(fromconn, fromtid, fromdate, &tr, 0/*spare*/, &cookie, &results);
	nservers = 1;
     }

     if (code) {
        PrintError("Release failed: ", code);
     } else {
        for (m=0; m<volcount; m++) {
	   if (results.manyResults_val[m]) {
	      if ((m == 0) || (results.manyResults_val[m] != ENOENT)) {
		 /* we retry timed out transaction. When it is
		  * not the first volume and the transaction wasn't found
		  * (assume it timed out and was garbage collected by volser).
		  */
		 PrintError("Failed to dump volume from clone to a ro site: ",
			    results.manyResults_val[m]);
	      }
	      continue;
	   }
	    
	   code = AFSVolSetIdsTypes(toconns[m], replicas[m].trans, 
				    vname, ROVOL, entry.volumeId[RWVOL], 0, 0);
	   if (code) {
	      if ((m == 0) || (code != ENOENT)) {
		 PrintError("Failed to set correct names and ids: ", code);
	      }
	      continue;
	   }

	   /* have to clear dest. flags to ensure new vol goes online:
	    * because the restore (forwarded) operation copied
	    * the V_inService(=0) flag over to the destination. 
	    */
	   code = AFSVolSetFlags(toconns[m], replicas[m].trans, 0);
	   if (code) {
	      if ((m == 0) || (code != ENOENT)) {
		 PrintError("Failed to set flags on ro volume: ", code);
	      }
	      continue;
	   }

	   entry.serverFlags[times[m].vldbEntryIndex] |=  NEW_REPSITE;
	   entry.serverFlags[times[m].vldbEntryIndex] &= ~RO_DONTUSE;
	   entry.flags                                |=  RO_EXISTS;
	   releasecount++;
	}
     }   

     /* End the transactions and destroy the connections */
     for (s=0; s<volcount; s++) {
        if (replicas[s].trans)
	   code = AFSVolEndTrans(toconns[s], replicas[s].trans, &rcode);
	replicas[s].trans = 0;
	if (!code) code = rcode;
	if (code) {
	   if ((s == 0) || (code != ENOENT)) {
	      PrintError("Could not end transaction on a ro volume: ", code);
	   } else {
	      PrintError("Transaction timed out on a ro volume. Will retry.\n", 0);
	      if (times[s].vldbEntryIndex < vldbindex)
		 vldbindex = times[s].vldbEntryIndex;
	   }
	}
	   
	if (toconns[s])
	   rx_DestroyConnection(toconns[s]);
	toconns[s] = 0;
     }
   
     MapNetworkToHost(&entry, &storeEntry);
     vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry, 0);
     ONERROR(vcode, afromvol, " Could not update VLDB entry for volume %u\n");
  } /* for each index in the vldb */

  /* End the transaction on the cloned volume */
  code = AFSVolEndTrans(fromconn, fromtid, &rcode);
  fromtid = 0;
  if (!code) code = rcode;
  if (code)
     PrintError("Failed to end transaction on rw volume: ", code);
  
  /* Figure out if any volume were not released and say so */
  for (failure=0, i=0; i<entry.nServers; i++) {
     if (!(entry.serverFlags[i] & NEW_REPSITE))
        failure++;
  }
  if (failure) {
     char pname[10];
     fprintf(STDERR, "The volume %u could not be released to the following %d sites:\n",
	     afromvol, failure);
     for (i=0; i<entry.nServers; i++) {
        if (!(entry.serverFlags[i] & NEW_REPSITE)) {
	   MapPartIdIntoName(entry.serverPartition[i],pname);
	   fprintf(STDERR,"\t%35s %s\n", 
		   hostutil_GetNameByINet(entry.serverNumber[i]), pname);
	}
     }

     MapNetworkToHost(&entry,&storeEntry);
     vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry, LOCKREL_TIMESTAMP);
     ONERROR(vcode, afromvol, " Could not update VLDB entry for volume %u\n");

     ERROREXIT(VOLSERBADRELEASE);
  }

  /* All the ROs were release successfully. Remove the temporary clone */
  if (!roclone) {
      if (verbose) {
	  fprintf(STDOUT,"Deleting the releaseClone %u ...", cloneVolId);
	  fflush(STDOUT);
      }
      code = DelVol (fromconn, cloneVolId, afrompart, ITOffline);
      ONERROR (code, cloneVolId, "Failed to delete volume %u.\n");
      if (verbose)
	 fprintf(STDOUT," done\n");
  }
  entry.cloneId = 0;

  for (i=0; i<entry.nServers; i++)
     entry.serverFlags[i] &= ~NEW_REPSITE;

  /* Update the VLDB */
  if (verbose) {
      fprintf(STDOUT,"updating VLDB ...");
      fflush(STDOUT);
  }
  MapNetworkToHost(&entry, &storeEntry);
  vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry,
			   LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
  ONERROR(vcode, afromvol, " Could not update VLDB entry for volume %u\n");
  if (verbose)
     fprintf(STDOUT," done\n");

 rfail:
  if (clonetid) {
      code = AFSVolEndTrans(fromconn, clonetid, &rcode);
      clonetid = 0;
      if (code) {
	  fprintf (STDERR,"Failed to end cloning transaction on the RW volume %u\n", afromvol);
	  if (!error) error = code;
      }
  }
  if (fromtid) {
      code = AFSVolEndTrans(fromconn, fromtid, &rcode);
      fromtid = 0;
      if (code) {
	  fprintf (STDERR,"Failed to end transaction on the release clone %u\n", cloneVolId);
	  if (!error) error = code;
      }
  }
  for (i=0; i<nservers; i++) {
      if (replicas && replicas[i].trans) {
	  code = AFSVolEndTrans(toconns[i], replicas[i].trans, &rcode);
	  replicas[i].trans = 0;
	  if (code) {
	      fprintf(STDERR,"Failed to end transaction on ro volume %u at server 0x%x\n",
		      entry.volumeId[ROVOL], 
		      hostutil_GetNameByINet(htonl(replicas[i].server.destHost)));
	      if (!error) error = code;
	  }
      }
      if (toconns && toconns[i]) {
	  rx_DestroyConnection(toconns[i]);
	  toconns[i] = 0;
      }
  }
  if (islocked) {
      vcode = ubik_Call(VL_ReleaseLock,cstruct, 0, afromvol, RWVOL, 
			LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
      if (vcode) {
	  fprintf(STDERR,"Could not release lock on the VLDB entry for volume %u\n", afromvol);
	  if (!error) error = vcode;
      }
  }

  PrintError("", error);

  if (fromconn)                rx_DestroyConnection(fromconn);
  if (results.manyResults_val) free (results.manyResults_val);
  if (replicas)                free (replicas);
  if (toconns)                 free (toconns);
  if (times)                   free (times);
  return error;
}


void dump_sig_handler(x)
{
   fprintf(STDERR,"\nSignal handler: vos dump operation\n");
   longjmp(env,0);
}

/* Dump the volume <afromvol> on <afromserver> and
 * <afrompart> to <afilename> starting from <fromdate>.
 * DumpFunction does the real work behind the scenes after
 * extracting parameters from the rock 
 */
UV_DumpVolume(afromvol, afromserver, afrompart, fromdate, DumpFunction, rock)
   afs_int32 afromserver;
   afs_int32 afrompart;
   afs_int32 afromvol;
   afs_int32 fromdate;
   afs_int32 (*DumpFunction)();
   char *rock;
{
   struct rx_connection *fromconn = (struct rx_connection *)0;
   struct rx_call       *fromcall = (struct rx_call *)0;
   afs_int32 fromtid=0, rxError=0, rcode=0;
   afs_int32 code, error = 0;

   if (setjmp(env)) ERROR_EXIT(EPIPE);
#ifndef AFS_NT40_ENV
   (void) signal(SIGPIPE, dump_sig_handler);
#endif
   (void) signal(SIGINT,  dump_sig_handler);

   /* get connections to the servers */
   fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
   code = AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &fromtid);
   if (code) {
      fprintf(STDERR,"Could not start transaction on the volume %u to be dumped\n", afromvol);
      ERROR_EXIT(code);
   }

   if (verbose) {
      if (!fromdate)
	fprintf(STDERR,"Full Dump ...");
      else 
	fprintf(STDERR,"Incremental Dump (as of %.24s) ...",
		ctime((time_t *)&fromdate));
      fflush(STDERR);
   }

   fromcall = rx_NewCall(fromconn);
   code = StartAFSVolDump(fromcall, fromtid, fromdate);
   if (code) {
      fprintf(STDERR,"Could not start the dump process \n");
      ERROR_EXIT(code);
   }
   if (code = DumpFunction(fromcall, rock)) {
      fprintf(STDERR,"Error while dumping volume \n");
      ERROR_EXIT(code);
   }

   if (verbose)
      fprintf(STDERR,"completed\n");

 error_exit: 
   if (fromcall) {
      code = rx_EndCall(fromcall, rxError);
      if (code) {
	 fprintf(STDERR,"Error in rx_EndCall\n");
	 if (!error) error = code;
      }	
   }
   if (fromtid) {
      code = AFSVolEndTrans(fromconn, fromtid, &rcode);
      if (code || rcode) {
	 fprintf(STDERR,"Could not end transaction on the volume %u\n", afromvol);
	 if (!error) error = (code?code:rcode);
      }
   }
   if (fromconn)
      rx_DestroyConnection(fromconn);

   PrintError("", error);
   return(error);
}


/*
 * Restore a volume <tovolid> <tovolname> on <toserver> <topart> from
 * the dump file <afilename>. WriteData does all the real work
 * after extracting params from the rock 
 */
UV_RestoreVolume(toserver, topart, tovolid, tovolname, flags, WriteData, rock)
    afs_int32 toserver, topart, tovolid;
    char tovolname[];
    int flags;
    afs_int32 (*WriteData)();
    char *rock;
{
    struct rx_connection *toconn,*tempconn;
    struct rx_call *tocall;
    afs_int32 totid, code, rcode, vcode,terror = 0;
    afs_int32 rxError = 0;
    struct volser_status tstatus;
    char partName[10];
    afs_int32 pvolid;
    afs_int32 temptid;
    int success;
    struct nvldbentry entry,storeEntry;
    afs_int32 error;
    int islocked;
    struct restoreCookie cookie;
    int reuseID;
    afs_int32 newDate, volflag;
    int index, same, errcode;
    char apartName[10];


    bzero(&cookie,sizeof(cookie));
    islocked  = 0;
    success = 0;
    error = 0;
    reuseID = 1;
    tocall = (struct rx_call *)0;
    toconn = (struct rx_connection *)0;
    tempconn = (struct rx_connection *)0;
    totid = 0;
    temptid = 0;

    pvolid = tovolid;
    toconn = UV_Bind(toserver, AFSCONF_VOLUMEPORT);
    if(pvolid == 0) {/*alot a new id if needed */
	vcode = VLDB_GetEntryByName(tovolname, &entry);
	if(vcode == VL_NOENT) {
	    vcode = ubik_Call(VL_GetNewVolumeId,cstruct, 0, 1, &pvolid);
	    if(vcode) {
		fprintf(STDERR,"Could not get an Id for the volume %s\n",tovolname);
		error = vcode;
		goto refail;
	    }
	    reuseID = 0;
        }
	else{
	    pvolid = entry.volumeId[RWVOL];
      	}
    }/* at this point we have a volume id to use/reuse for the volume to be restored */

    if(strlen(tovolname) > (VOLSER_OLDMAXVOLNAME - 1)) {
	fprintf(STDERR,"The volume name %s exceeds the maximum limit of (VOLSER_OLDMAXVOLNAME -1 ) bytes\n",tovolname);
	error = VOLSERBADOP;
	goto refail;
    }
    MapPartIdIntoName(topart, partName);
    fprintf(STDOUT,"Restoring volume %s Id %u on server %s partition %s ..", tovolname,
	    pvolid, hostutil_GetNameByINet(toserver), partName);
    fflush(STDOUT);
    /*what should the volume be restored as ? rw or ro or bk ?
      right now the default is rw always */
    code = AFSVolCreateVolume(toconn, topart, tovolname, volser_RW, 0,&pvolid, &totid);
    if (code){
	if (flags & RV_FULLRST) { /* full restore: delete then create anew */
	    if(verbose) {
		fprintf(STDOUT,"Deleting the previous volume %u ...",pvolid);
		fflush(STDOUT);
	    }
	    code = AFSVolTransCreate(toconn, pvolid, topart, ITOffline, &totid);
	    if (code) {
		fprintf(STDERR,"Failed to start transaction on %u\n",pvolid);
		error = code;
		goto refail;
	    }
	    code = AFSVolSetFlags(toconn, totid, VTDeleteOnSalvage | VTOutOfService);
	    if (code){
		fprintf(STDERR,"Could not set flags on volume %u \n",pvolid);
		error = code;
		goto refail;
	    }
	    code = AFSVolDeleteVolume(toconn,totid);
	    if (code){
		fprintf(STDERR,"Could not delete volume %u\n",pvolid); 
		error = code;
		goto refail;
	    }
	    code = AFSVolEndTrans(toconn, totid, &rcode);
	    totid = 0;
	    if (!code) code = rcode;
	    if (code) {
		fprintf(STDERR,"Could not end transaction on %u\n",pvolid);
		error = code;
		goto refail;
	    }
	    if (verbose) fprintf(STDOUT," done\n");
	    code = AFSVolCreateVolume(toconn, topart, tovolname, volser_RW, 0,&pvolid, &totid);
	    if (code){
		fprintf(STDERR,"Could not create new volume %u\n",pvolid);
		error = code;
		goto refail;}
	}
	else{
	    
	    code = AFSVolTransCreate(toconn, pvolid, topart, ITOffline, &totid);
	    if (code) {
		fprintf(STDERR,"Failed to start transaction on %u\n",pvolid);
		error = code;
		goto refail;
	    }
	}
    }
    cookie.parent = pvolid;
    cookie.type = RWVOL;
    cookie.clone = 0;
    strncpy(cookie.name,tovolname,VOLSER_OLDMAXVOLNAME);

    tocall = rx_NewCall(toconn);
    terror = StartAFSVolRestore(tocall,totid, 1,&cookie);
    if(terror) {
	fprintf(STDERR,"Volume restore Failed \n");
	error = terror;
	goto refail;
    }
    code = WriteData(tocall, rock);
    if(code) {
	fprintf(STDERR,"Could not transmit data\n");
	error = code;
	goto refail;
    }
    terror = rx_EndCall(tocall,rxError);
    tocall = (struct rx_call *) 0;
    if (terror) {
	fprintf(STDERR,"rx_EndCall Failed \n");
	error = terror;	    
	goto refail;
    }
    code = AFSVolGetStatus(toconn,totid, &tstatus);
    if(code) {
	fprintf(STDERR,"Could not get status information about the volume %u\n",tovolid);
	error = code;
	goto refail;
    }
    code = AFSVolSetIdsTypes(toconn,totid, tovolname, RWVOL, pvolid,0,0);
    if(code) {
	fprintf(STDERR,"Could not set the right type and ID on %u\n",pvolid); 
	error = code;
	goto refail;
    }
    newDate = time(0);
    code = AFSVolSetDate(toconn,totid, newDate);
    if(code) {
	fprintf(STDERR,"Could not set the date on %u\n",pvolid); 
	error = code;
	goto refail;
    }

    volflag = ((flags & RV_OFFLINE) ? VTOutOfService : 0); /* off or on-line */
    code = AFSVolSetFlags(toconn, totid, volflag);
    if (code){
	fprintf(STDERR,"Could not mark %u online\n",pvolid );
	error = code;
	goto refail;
    }
   
/* It isn't handled right in refail */
    code = AFSVolEndTrans(toconn, totid, &rcode);
    totid = 0;
    if(!code) code = rcode;
    if(code) {
	fprintf(STDERR,"Could not end transaction on %u\n",pvolid);
	error = code;
	goto refail;
    }

    success = 1;
    fprintf(STDOUT," done\n");
    fflush(STDOUT);
    if (success && (!reuseID || (flags & RV_FULLRST))) {
        /* Volume was restored on the file server, update the 
	 * VLDB to reflect the change.
	 */
	vcode = VLDB_GetEntryByID(pvolid,RWVOL, &entry);
	if(vcode && vcode != VL_NOENT && vcode != VL_ENTDELETED) {
	    fprintf(STDERR,"Could not fetch the entry for volume number %u from VLDB \n",pvolid);
	    error = vcode;
	    goto refail;
	}
	if (!vcode) MapHostToNetwork(&entry);
	if(vcode == VL_NOENT) { /* it doesnot exist already */
	    /*make the vldb return this indication specifically*/
	    if (verbose) fprintf(STDOUT,"------- Creating a new VLDB entry ------- \n");
	    strcpy(entry.name, tovolname);
	    entry.nServers = 1;
	    entry.serverNumber[0] = toserver;/*should be indirect */
	    entry.serverPartition[0] = topart;
	    entry.serverFlags[0] = ITSRWVOL;
	    entry.flags = RW_EXISTS;
	    if(tstatus.cloneID != 0){
		entry.volumeId[ROVOL] = tstatus.cloneID;/*this should come from status info on the volume if non zero */
	    }
	    else
		entry.volumeId[ROVOL] = INVALID_BID;
	    entry.volumeId[RWVOL] = pvolid;
	    entry.cloneId = 0;
	    if(tstatus.backupID != 0){
		entry.volumeId[BACKVOL] = tstatus.backupID;
		/*this should come from status info on the volume if non zero */
	    }
	    else 
		entry.volumeId[BACKVOL] = INVALID_BID;
	    MapNetworkToHost(&entry,&storeEntry);
	    vcode = VLDB_CreateEntry(&storeEntry);
	    if(vcode) {
		fprintf(STDERR,"Could not create the VLDB entry for volume number %u  \n",pvolid);
		error = vcode;
		goto refail;
	    }
	    islocked = 0;
	    if (verbose) EnumerateEntry(&entry);
	}
	else {	/*update the existing entry */
	    if(verbose) {
		fprintf(STDOUT,"Updating the existing VLDB entry\n");
		fprintf(STDOUT,"------- Old entry -------\n");
		EnumerateEntry(&entry);
		fprintf(STDOUT,"------- New entry -------\n");
	    }
	    vcode = ubik_Call(VL_SetLock,cstruct, 0, pvolid, RWVOL, VLOP_RESTORE);
	    if(vcode) {
		fprintf(STDERR,"Could not lock the entry for volume number %u \n",pvolid);
		error = vcode;
		goto refail;
	    }
	    islocked = 1;
	    strcpy(entry.name, tovolname);

	    /* Update the vlentry with the new information */
	    index = Lp_GetRwIndex(&entry);
	    if (index == -1) {
	       /* Add the rw site for the volume being restored */
	       entry.serverNumber[entry.nServers]    = toserver;
	       entry.serverPartition[entry.nServers] = topart;
	       entry.serverFlags[entry.nServers]     = ITSRWVOL;
	       entry.nServers++;
	    } else {
	       /* This volume should be deleted on the old site
		* if its different from new site.
		*/
	       same = VLDB_IsSameAddrs(toserver, entry.serverNumber[index], &errcode);
	       if (errcode) {
		  fprintf(STDERR,"Failed to get info about server's %d address(es) from vlserver (err=%d)\n", 
			  toserver, errcode);
	       }
	       if ( (!errcode && !same) || (entry.serverPartition[index] != topart) ) {
		  tempconn = UV_Bind(entry.serverNumber[index], AFSCONF_VOLUMEPORT);
		  if (verbose) {
		     MapPartIdIntoName(entry.serverPartition[index], apartName);
		     fprintf(STDOUT,"Deleting the previous volume %u on server %s, partition %s ...",
			     pvolid,
			     hostutil_GetNameByINet(entry.serverNumber[index]), apartName);
		     fflush(STDOUT);
		  }
		  code = AFSVolTransCreate(tempconn, pvolid, entry.serverPartition[index], ITOffline, &temptid);
		  if (!code){
		     code = AFSVolSetFlags(tempconn, temptid, VTDeleteOnSalvage | VTOutOfService);
		     if(code) {
		        fprintf(STDERR,"Could not set flags on volume %u on the older site\n",pvolid);
			error = code;
			goto refail;
		     }
		     code = AFSVolDeleteVolume(tempconn,temptid);
		     if(code){
		        fprintf(STDERR,"Could not delete volume %u on the older site\n",pvolid);
			error = code;
			goto refail;
		     }
		     code = AFSVolEndTrans(tempconn, temptid, &rcode);
		     temptid = 0;
		     if(!code) code = rcode;
		     if(code){
		        fprintf(STDERR,"Could not end transaction on volume %u on the older site\n",pvolid);
			error = code;
			goto refail;
		     }
		     if(verbose) fprintf(STDOUT," done\n");
		     MapPartIdIntoName(entry.serverPartition[index],partName);
		  }
	       }
	       entry.serverNumber[index]    = toserver;
	       entry.serverPartition[index] = topart;
	    }

	    entry.flags |= RW_EXISTS;
	    MapNetworkToHost(&entry,&storeEntry);
	    vcode = VLDB_ReplaceEntry(pvolid,RWVOL, &storeEntry,LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP );
	    if(vcode) {
		fprintf(STDERR,"Could not update the entry for volume number %u  \n",pvolid);
		error = vcode;
		goto refail;
	    }
	    islocked = 0;
	    if(verbose) EnumerateEntry(&entry);
	}


    }
    refail:
      if (tocall) {
	  code = rx_EndCall(tocall, rxError);
	  if (!error) error = code;
      }
      if(islocked) {
	  vcode = ubik_Call(VL_ReleaseLock,cstruct, 0, pvolid, RWVOL, LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	  if(vcode) {
	      fprintf(STDERR,"Could not release lock on the VLDB entry for the volume %u\n",pvolid);
	      if(!error) error = vcode;
	  }
      }
      if(totid) {
	  code = AFSVolEndTrans(toconn, totid, &rcode);
	  if(!code) code = rcode;
	  if(code) {
	      fprintf(STDERR,"Could not end transaction on the volume %u \n",pvolid);
	      if(!error) error = code;
	  }
      }
      if(temptid) {
	  code = AFSVolEndTrans(toconn, temptid, &rcode);
	  if(!code) code = rcode;
	  if(code) {
	      fprintf(STDERR,"Could not end transaction on the volume %u \n",pvolid);
	      if(!error) error = code;
	  }
      }
      if(tempconn) rx_DestroyConnection(tempconn);
      if(toconn) rx_DestroyConnection(toconn);
      PrintError("",error);
      return error;
}


/*unlocks the vldb entry associated with <volid> */
UV_LockRelease(volid)
afs_int32 volid;
{
	
	
    afs_int32 vcode;

    if (verbose) fprintf(STDERR,"Binding to the VLDB server\n");
    vcode = ubik_Call(VL_ReleaseLock,cstruct, 0,volid,-1,LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP );
    if(vcode) {
	fprintf(STDERR,"Could not unlock the entry for volume number %u in VLDB \n",volid);
	PrintError("",vcode);
	return (vcode);
    }
    if (verbose) fprintf(STDERR,"VLDB updated\n");
    return 0;

}

/*adds <server> and <part> as a readonly replication site for <volid>
*in vldb */
UV_AddSite(server, part, volid)
afs_int32 server, part, volid;
{
    int j, nro=0, islocked=0;
    struct nvldbentry entry,storeEntry;
    afs_int32 vcode, error=0;
    char apartName[10];

    error = ubik_Call(VL_SetLock,cstruct, 0,volid,RWVOL, VLOP_ADDSITE);
    if (error) {
	fprintf(STDERR," Could not lock the VLDB entry for the volume %u \n", volid);
	goto asfail;
    }
    islocked = 1;

    error = VLDB_GetEntryByID(volid,RWVOL, &entry);
    if (error) {
	fprintf(STDERR,"Could not fetch the VLDB entry for volume number %u  \n",volid);
	goto asfail;

    }
    if (!ISNAMEVALID(entry.name)){
	fprintf(STDERR,"Volume name %s is too long, rename before adding site\n", entry.name);
	error = VOLSERBADOP;
	goto asfail;
    }
    MapHostToNetwork(&entry);

    /* See if it's too many entries */
    if (entry.nServers >= NMAXNSERVERS){
       fprintf(STDERR,"Total number of entries will exceed %u\n", NMAXNSERVERS);
       error = VOLSERBADOP;
       goto asfail;
    }

    /* See if it's on the same server */
    for (j=0; j < entry.nServers; j++) {
       if (entry.serverFlags[j] & ITSROVOL) {
	  nro++;
	  if (VLDB_IsSameAddrs(server, entry.serverNumber[j], &error)) {
	     if (error) {
	        fprintf(STDERR,"Failed to get info about server's %d address(es) from vlserver (err=%d); aborting call!\n", 
			server, error);
	     } else {
	        MapPartIdIntoName(entry.serverPartition[j], apartName);
		fprintf(STDERR,"RO already exists on partition %s. Multiple ROs on a single server aren't allowed\n", apartName);  
		error =  VOLSERBADOP;
	     }
	     goto asfail;
	  }
       }
    }

    /* See if it's too many RO sites - leave one for the RW */
    if (nro >= NMAXNSERVERS-1){
       fprintf(STDERR,"Total number of sites will exceed %u\n", NMAXNSERVERS-1);
       error = VOLSERBADOP;
       goto asfail;
    }

    if (verbose) fprintf(STDOUT,"Adding a new site ...");
    fflush(STDOUT);
    entry.serverNumber[entry.nServers] = server;
    entry.serverPartition[entry.nServers] = part;
    entry.serverFlags[entry.nServers] = (ITSROVOL | RO_DONTUSE);
    entry.nServers++;
	
    MapNetworkToHost(&entry,&storeEntry);
    error = VLDB_ReplaceEntry(volid,RWVOL,&storeEntry,LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
    if (error) {
       fprintf(STDERR,"Could not update entry for volume %u \n",volid);
       goto asfail;
    }
    islocked = 0;
    if (verbose) fprintf(STDOUT," done\n");

  asfail:
    if (islocked) {
       vcode = ubik_Call(VL_ReleaseLock,cstruct, 0, volid, RWVOL, LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
       if (vcode) {
	  fprintf(STDERR,"Could not release lock on volume entry for %u \n",volid);
	  PrintError("", vcode);
       }
    }

    PrintError("", error);
    return error;
}

/*removes <server> <part> as read only site for <volid> from the vldb */
UV_RemoveSite(server, part, volid)
afs_int32 server, part, volid;
{
    afs_int32 vcode;
    struct nvldbentry entry,storeEntry;
    int islocked;

    vcode = ubik_Call(VL_SetLock,cstruct, 0,volid,RWVOL, VLOP_ADDSITE);
    if(vcode) {
	fprintf(STDERR," Could not lock the VLDB entry for volume %u \n", volid);
	PrintError("",vcode);
	return(vcode);
    }
    islocked = 1;
    vcode = VLDB_GetEntryByID(volid,RWVOL, &entry);
    if(vcode) {
	fprintf(STDERR,"Could not fetch the entry for volume number %u from VLDB \n",volid);
	PrintError("",vcode);
	return (vcode);
    }
    MapHostToNetwork(&entry);
    if(!Lp_ROMatch(server, part, &entry)){
	/*this site doesnot exist  */
	fprintf(STDERR,"This site is not a replication site \n");
	vcode = ubik_Call(VL_ReleaseLock,cstruct, 0, volid, RWVOL, LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if(vcode) {
	    fprintf(STDERR,"Could not update entry for volume %u \n",volid);
	    PrintError("",vcode);
	    ubik_Call(VL_ReleaseLock,cstruct, 0, volid, RWVOL, LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	    return(vcode);
	}
	return VOLSERBADOP;
    }
    else { /*remove the rep site */
	Lp_SetROValue(&entry, server, part, 0, 0);
	entry.nServers--;
	if((entry.nServers == 1) && (entry.flags & RW_EXISTS))
	    entry.flags &= ~RO_EXISTS;
	if(entry.nServers < 1) { /*this is the last ref */
	    if(verbose) fprintf(STDOUT,"Deleting the VLDB entry for %u ...",volid);
	    fflush(STDOUT);
	    vcode = ubik_Call(VL_DeleteEntry,cstruct, 0,volid, ROVOL);
	    if(vcode) {
		fprintf(STDERR,"Could not delete VLDB entry for volume %u \n",volid);
		PrintError("",vcode);
		return(vcode);
	    }
	    if (verbose) fprintf(STDOUT," done\n");
	}
	MapNetworkToHost(&entry,&storeEntry);
	fprintf(STDOUT,"Deleting the replication site for volume %u ...",volid);
	fflush(STDOUT);
	vcode = VLDB_ReplaceEntry(volid,RWVOL,&storeEntry,LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if(vcode){ 
	    fprintf(STDERR,"Could not release lock on volume entry for %u \n",volid);
	    PrintError("",vcode);
	    ubik_Call(VL_ReleaseLock,cstruct, 0, volid, RWVOL, LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	    return(vcode);
	}
	if(verbose) fprintf(STDOUT," done\n");
    }
    return 0;
}

/*list all the partitions on <aserver> */
UV_ListPartitions(aserver, ptrPartList, cntp)
    afs_int32 aserver;
    struct partList *ptrPartList;
    afs_int32 *cntp;
{
    struct rx_connection *aconn;
    struct pIDs partIds;
    struct partEntries partEnts;
    register int i, j=0, code;

    *cntp = 0;
    aconn = UV_Bind(aserver,AFSCONF_VOLUMEPORT);

    partEnts.partEntries_len = 0;
    partEnts.partEntries_val = (afs_int32 *)0;
    code = AFSVolXListPartitions(aconn, &partEnts); /* this is available only on new servers */
    if (code == RXGEN_OPCODE) 
    {
	for(i = 0; i < 26; i++)				/* try old interface */
	    partIds.partIds[i]  = -1;
	code = AFSVolListPartitions(aconn, &partIds);
	if (!code) {
	    for (i = 0;i < 26; i++) {
		if((partIds.partIds[i]) != -1) {
		    ptrPartList->partId[j] = partIds.partIds[i];
		    ptrPartList->partFlags[j] = PARTVALID;
		    j++;
		} else
		    ptrPartList->partFlags[i] = 0;
	    }
	    *cntp = j;
	}
    }
    else if (!code) 
    {
	*cntp = partEnts.partEntries_len;
	if (*cntp > VOLMAXPARTS) {
	    fprintf(STDERR,"Warning: number of partitions on the server too high %d (process only %d)\n",
		    *cntp, VOLMAXPARTS);
	    *cntp = VOLMAXPARTS;
	}
	for (i = 0;i < *cntp; i++) {
	    ptrPartList->partId[i] = partEnts.partEntries_val[i];
	    ptrPartList->partFlags[i] = PARTVALID;
	}
	free(partEnts.partEntries_val);
    }
out:
    if (code)
	fprintf(STDERR,"Could not fetch the list of partitions from the server\n");
    PrintError("",code);
    if(aconn) rx_DestroyConnection(aconn); 
    return code;
}


/*zap the list of volumes specified by volPtrArray (the volCloneId field).
 This is used by the backup system */
UV_ZapVolumeClones(aserver,apart,volPtr,arraySize)
afs_int32 aserver, apart;
afs_int32 arraySize;
struct volDescription *volPtr;
{
    struct rx_connection *aconn;
    struct volDescription *curPtr;
    int curPos;
    afs_int32 code = 0;
    afs_int32 rcode = 0;
    afs_int32 success = 1;
    afs_int32 tid;

    aconn = (struct rx_connection *)0;
    aconn = UV_Bind(aserver,AFSCONF_VOLUMEPORT);
    curPos = 0;
    for(curPtr = volPtr; curPos < arraySize; curPtr++) {
	if(curPtr->volFlags & CLONEVALID) {
	    curPtr->volFlags &= ~CLONEZAPPED;
	    success = 1;
	    code = AFSVolTransCreate(aconn, curPtr->volCloneId, apart, ITOffline, &tid);
	    if(code) success = 0;
	    else {
		code = AFSVolDeleteVolume(aconn, tid);
		if(code) success = 0;
		code = AFSVolEndTrans(aconn, tid, &rcode);
		if(code || rcode) success = 0;
	    }
	    if(success) curPtr->volFlags |= CLONEZAPPED;
	    if(!success) fprintf(STDERR,"Could not zap volume %u\n",curPtr->volCloneId);
	    if(success && verbose) fprintf(STDOUT,"Clone of %s %u deleted\n", curPtr->volName,curPtr->volCloneId);
	    curPos++;
	    tid = 0;
	}
    }
    if(aconn)rx_DestroyConnection(aconn);
    return 0;
}

/*return a list of clones of the volumes specified by volPtrArray. Used by the 
 backup system */
UV_GenerateVolumeClones(aserver,apart,volPtr,arraySize)
afs_int32 aserver, apart;
afs_int32 arraySize;
struct volDescription *volPtr;
{
    struct rx_connection *aconn;
    struct volDescription *curPtr;
    int curPos;
    afs_int32 code = 0;
    afs_int32 rcode = 0;
    afs_int32 tid;
    int reuseCloneId = 0;
    afs_int32 curCloneId = 0;
    char cloneName[256];/*max vol name */

    aconn = (struct rx_connection *)0;
    aconn = UV_Bind(aserver,AFSCONF_VOLUMEPORT);
    curPos = 0;
    if((volPtr->volFlags & REUSECLONEID) && (volPtr->volFlags & ENTRYVALID))
	reuseCloneId = 1;
    else { /*get a bunch of id's from vldb */
	code = ubik_Call(VL_GetNewVolumeId,cstruct, 0, arraySize, &curCloneId);
	if(code) {
	    fprintf(STDERR,"Could not get ID's for the clone from VLDB\n");
	    PrintError("",code);
	    return code;
	}
    }

    for(curPtr = volPtr; curPos < arraySize; curPtr++) {
	if(curPtr->volFlags & ENTRYVALID) {

	    curPtr->volFlags |= CLONEVALID; 
	    /*make a clone of curParentId and record as curPtr->volCloneId */
	    code = AFSVolTransCreate(aconn, curPtr->volId, apart, ITOffline, &tid);
	    if(verbose && code) fprintf(STDERR,"Clone for volume %s %u failed \n",curPtr->volName,curPtr->volId);
	    if(code) {
		curPtr->volFlags &= ~CLONEVALID; /*cant clone */
		curPos++;
		continue;
	    }
	    if(strlen(curPtr->volName) < (VOLSER_OLDMAXVOLNAME - 9) ){
		strcpy(cloneName, curPtr->volName);
		strcat(cloneName,"-tmpClone-");
	    }
	    else strcpy(cloneName,"-tmpClone");
	    if(reuseCloneId) {
		curPtr->volCloneId = curCloneId;
		curCloneId++;
	    }

	    code = AFSVolClone(aconn, tid, 0, readonlyVolume, cloneName,&(curPtr->volCloneId));
	    if(code){
		curPtr->volFlags &= ~CLONEVALID;
		curPos++;
		fprintf(STDERR,"Could not clone %s due to error %u\n", curPtr->volName,code);
		code=AFSVolEndTrans(aconn, tid, &rcode);
		if(code)
			fprintf(STDERR,"WARNING: could not end transaction\n");
		continue;
	    }
	    if(verbose) fprintf(STDOUT,"********** Cloned %s temporary %u\n",cloneName,curPtr->volCloneId);
	    code = AFSVolEndTrans(aconn, tid, &rcode);
	    if(code || rcode) {
		curPtr->volFlags &= ~CLONEVALID; 
		curPos++;
		continue;
	    }

	    curPos++;
	}
    }
    if (aconn) rx_DestroyConnection(aconn);
    return 0;
}

	
/*list all the volumes on <aserver> and <apart>. If all = 1, then all the
* relevant fields of the volume are also returned. This is a heavy weight operation.*/
UV_ListVolumes(aserver,apart,all,resultPtr,size)
afs_int32 aserver, apart;
int all ;
struct volintInfo **resultPtr;
afs_int32 *size;
{
    struct rx_connection *aconn;
    afs_int32 code = 0;
    volEntries volumeInfo;
    
    code = 0;
    *size = 0;
    *resultPtr = (volintInfo *)0;
    volumeInfo.volEntries_val = (volintInfo *)0;/*this hints the stub to allocate space*/
    volumeInfo.volEntries_len = 0;

    aconn = UV_Bind(aserver,AFSCONF_VOLUMEPORT);
    code = AFSVolListVolumes(aconn, apart, all, &volumeInfo);
    if(code) {
	fprintf(STDERR,"Could not fetch the list of volumes from the server\n");
    }
    else{
	*resultPtr = volumeInfo.volEntries_val;
	*size = volumeInfo.volEntries_len;
    }

    if(aconn) rx_DestroyConnection(aconn);
    PrintError("",code);
    return code;    
}

/*------------------------------------------------------------------------
 * EXPORTED UV_XListVolumes
 *
 * Description:
 *	List the extended information for all the volumes on a particular
 *	File Server and partition.  We may either return the volume's ID
 *	or all of its extended information.
 *
 * Arguments:
 *	a_serverID	   : Address of the File Server for which we want
 *				extended volume info.
 *	a_partID	   : Partition for which we want the extended
 *				volume info.
 *	a_all		   : If non-zero, fetch ALL the volume info,
 *				otherwise just the volume ID.
 *	a_resultPP	   : Ptr to the address of the area containing
 *				the returned volume info.
 *	a_numEntsInResultP : Ptr for the value we set for the number of
 *				entries returned.
 *
 * Returns:
 *	0 on success,
 *	Otherise, the return value of AFSVolXListVolumes.
 *
 * Environment:
 *	This routine is closely related to UV_ListVolumes, which returns
 *	only the standard level of detail on AFS volumes. It is a
 *	heavyweight operation, zipping through all the volume entries for
 *	a given server/partition.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

UV_XListVolumes(a_serverID, a_partID, a_all, a_resultPP, a_numEntsInResultP)
    afs_int32 a_serverID;
    afs_int32 a_partID;
    int a_all;
    struct volintXInfo **a_resultPP;
    afs_int32 *a_numEntsInResultP;

{ /*UV_XListVolumes*/

    struct rx_connection *rxConnP;	/*Ptr to the Rx connection involved*/
    afs_int32 code;				/*Error code to return*/
    volXEntries volumeXInfo;		/*Area for returned extended vol info*/

    /*
     * Set up our error code and the area for returned extended volume info.
     * We set the val field to a null pointer as a hint for the stub to
     * allocate space.
     */
    code = 0;
    *a_numEntsInResultP = 0;
    *a_resultPP = (volintXInfo *)0;
    volumeXInfo.volXEntries_val = (volintXInfo *)0;
    volumeXInfo.volXEntries_len = 0;

    /*
     * Bind to the Volume Server port on the File Server machine in question,
     * then go for it.
     */
    rxConnP = UV_Bind(a_serverID, AFSCONF_VOLUMEPORT);
    code = AFSVolXListVolumes(rxConnP, a_partID, a_all, &volumeXInfo);
    if (code)
	fprintf(STDERR,
		"[UV_XListVolumes] Couldn't fetch volume list\n");
    else {
	/*
	 * We got the info; pull out the pointer to where the results lie
	 * and how many entries are there.
	 */
	*a_resultPP = volumeXInfo.volXEntries_val;
	*a_numEntsInResultP = volumeXInfo.volXEntries_len;
    }

    /*
     * If we got an Rx connection, throw it away.
     */
    if (rxConnP)
	rx_DestroyConnection(rxConnP);

    PrintError("", code);
    return(code);
} /*UV_XListVolumes*/

/* get all the information about volume <volid> on <aserver> and <apart> */
UV_ListOneVolume(aserver,apart,volid,resultPtr)
afs_int32 aserver, apart;
afs_int32 volid;
struct volintInfo **resultPtr;
{
    struct rx_connection *aconn;
    afs_int32 code = 0;
    volEntries volumeInfo;
    
    code = 0;

    *resultPtr = (volintInfo *)0;
    volumeInfo.volEntries_val = (volintInfo *)0;/*this hints the stub to allocate space*/
    volumeInfo.volEntries_len = 0;

    aconn = UV_Bind(aserver,AFSCONF_VOLUMEPORT);
    code = AFSVolListOneVolume(aconn, apart, volid, &volumeInfo);
    if(code) {
	fprintf(STDERR,"Could not fetch the information about volume %u from the server\n",volid);
    }
    else{
	*resultPtr = volumeInfo.volEntries_val;
	
    }

    if(aconn) rx_DestroyConnection(aconn);
    PrintError("",code);
    return code;    
}

/*------------------------------------------------------------------------
 * EXPORTED UV_XListOneVolume
 *
 * Description:
 *	List the extended information for a volume on a particular File
 *	Server and partition.
 *
 * Arguments:
 *	a_serverID	   : Address of the File Server for which we want
 *				extended volume info.
 *	a_partID	   : Partition for which we want the extended
 *				volume info.
 *	a_volID		   : Volume ID for which we want the info.
 *	a_resultPP	   : Ptr to the address of the area containing
 *				the returned volume info.
 *
 * Returns:
 *	0 on success,
 *	Otherise, the return value of AFSVolXListOneVolume.
 *
 * Environment:
 *	This routine is closely related to UV_ListOneVolume, which returns
 *	only the standard level of detail on the chosen AFS volume.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

UV_XListOneVolume(a_serverID, a_partID, a_volID, a_resultPP)
    afs_int32 a_serverID;
    afs_int32 a_partID;
    afs_int32 a_volID;
    struct volintXInfo **a_resultPP;

{ /*UV_XListOneVolume*/
    struct rx_connection *rxConnP;	/*Rx connection to Volume Server*/
    afs_int32 code;				/*Error code*/
    volXEntries volumeXInfo;		/*Area for returned info*/

    /*
     * Set up our error code, and the area we're in which we are returning
     * the info.  Setting the val field to a null pointer tells the stub
     * to allocate space for us.
     */
    code = 0;
    *a_resultPP = (volintXInfo *)0;
    volumeXInfo.volXEntries_val = (volintXInfo *)0;
    volumeXInfo.volXEntries_len = 0;

    /*
     * Bind to the Volume Server port on the File Server machine in question,
     * then go for it.
     */
    rxConnP = UV_Bind(a_serverID, AFSCONF_VOLUMEPORT);
    code = AFSVolXListOneVolume(rxConnP, a_partID, a_volID, &volumeXInfo);
    if(code)
	fprintf(STDERR,
		"[UV_XListOneVolume] Couldn't fetch the volume information\n");
    else
	/*
	 * We got the info; pull out the pointer to where the results lie.
	 */
	*a_resultPP = volumeXInfo.volXEntries_val;

    /*
     * If we got an Rx connection, throw it away.
     */
    if (rxConnP)
	rx_DestroyConnection(rxConnP);

    PrintError("",code);
    return code;    
} /*UV_XListOneVolume*/

/* CheckVolume()
 *    Given a volume we read from a partition, check if it is 
 *    represented in the VLDB correctly.
 * 
 *    The VLDB is looked up by the RW volume id (not its name).
 *    The RW contains the true name of the volume (BK and RO set
 *       the name in the VLDB only on creation of the VLDB entry).
 *    We want rules strict enough that when we check all volumes
 *       on one partition, it does not need to be done again. IE:
 *       two volumes on different partitions won't constantly 
 *       change a VLDB entry away from what the other set.
 *    For RW and BK volumes, we will always check the VLDB to see 
 *       if the two exist on the server/partition. May seem redundant,
 *       but this is an easy check of the VLDB. IE: if the VLDB entry
 *       says the BK exists but no BK volume is there, we will detect
 *       this when we check the RW volume.
 *    VLDB entries are locked only when a change needs to be done.
 *    Output changed to look a lot like the "vos syncserv" otuput.
 */
static afs_int32 CheckVolume(volumeinfo, aserver, apart, modentry, maxvolid)
  volintInfo *volumeinfo;
  afs_int32 aserver, apart;
  afs_int32 *modentry;
  afs_uint32 *maxvolid;
{   
   int   idx, j;
   afs_int32 code, error = 0;
   struct nvldbentry entry, storeEntry;
   int sameserver;
   char pname[10];
   int pass=0, islocked=0, createentry, addvolume, modified, mod;
   afs_int32 rwvolid;

   if (modentry) *modentry = 0;
   rwvolid = ((volumeinfo->type == RWVOL) ? volumeinfo->volid : volumeinfo->parentID);

 retry:
   /* Check to see if the VLDB is ok without locking it (pass 1).
    * If it will change, then lock the VLDB entry, read it again,
    * then make the changes to it (pass 2).
    */
   if (++pass == 2) {
      code = ubik_Call(VL_SetLock, cstruct, 0, rwvolid, RWVOL, VLOP_DELETE);
      if (code) {
	 fprintf(STDERR, "Could not lock VLDB entry for %u\n", rwvolid);
	 ERROR_EXIT(code);
      }
      islocked = 1;
   }

   createentry = 0;       /* Do we need to create a VLDB entry */
   addvolume   = 0;       /* Add this volume to the VLDB entry */
   modified    = 0;       /* The VLDB entry was modified */

   /* Read the entry from VLDB by its RW volume id */
   code = VLDB_GetEntryByID(rwvolid, RWVOL, &entry);
   if (code) {
      if (code != VL_NOENT) {
	 fprintf(STDOUT,"Could not retreive the VLDB entry for volume %u \n", rwvolid);
	 ERROR_EXIT(code);
      }

      bzero(&entry, sizeof(entry));
      vsu_ExtractName(entry.name, volumeinfo->name); /* Store name of RW */

      createentry = 1;
   } else {
      MapHostToNetwork(&entry);
   }

   if (verbose && (pass == 1)) {
      fprintf(STDOUT,"_______________________________\n");
      fprintf(STDOUT,"\n-- status before -- \n");
      if (createentry) {
	 fprintf(STDOUT,"\n**does not exist**\n");
      } else {
	 if ((entry.flags & RW_EXISTS) ||
	     (entry.flags & RO_EXISTS) || 
	     (entry.flags & BACK_EXISTS))
	   EnumerateEntry(&entry);
      }
      fprintf(STDOUT,"\n");
   }

   if (volumeinfo->type == RWVOL) {                      /* RW volume exists */
      if (createentry) {
	 idx = 0;
	 entry.nServers = 1;
	 addvolume++;
      } else {
	 /* Check existence of RW and BK volumes */
	 code = CheckVldbRWBK(&entry, &mod);
	 if (code) ERROR_EXIT(code);
	 if (mod) modified++;

	 idx = Lp_GetRwIndex(&entry);
	 if (idx == -1) {            /* RW index not found in the VLDB entry */
	    idx = entry.nServers;       /* put it into next index */
	    entry.nServers++;
	    addvolume++;
	 } else {                        /* RW index found in the VLDB entry. */
	    /* Verify if this volume's location matches where the VLDB says it is */
	    if (!Lp_Match(aserver, apart, &entry)) {
	       if (entry.flags & RW_EXISTS) {
		  /* The RW volume exists elsewhere - report this one a duplicate */
		  if (pass == 1) {
		     MapPartIdIntoName(apart, pname);
		     fprintf(STDERR,"*** Warning: Orphaned RW volume %u exists on %s %s\n",
			     rwvolid, hostutil_GetNameByINet(aserver), pname);
		     MapPartIdIntoName(entry.serverPartition[idx], pname);
		     fprintf(STDERR,"    VLDB reports RW volume %u exists on %s %s\n",
			     rwvolid,
			     hostutil_GetNameByINet(entry.serverNumber[idx]), pname);
		  }
	       } else {
		  /* The RW volume does not exist - have VLDB point to this one */
		  addvolume++;

		  /* Check for orphaned BK volume on old partition */
		  if (entry.flags & BACK_EXISTS) {
		     if (pass == 1) {
			MapPartIdIntoName(entry.serverPartition[idx], pname);
		        fprintf(STDERR,"*** Warning: Orphaned BK volume %u exists on %s %s\n",
				entry.volumeId[BACKVOL],
				hostutil_GetNameByINet(entry.serverNumber[idx]), pname);
			MapPartIdIntoName(apart, pname);
		        fprintf(STDERR,"    VLDB reports its RW volume %u exists on %s %s\n",
				rwvolid, hostutil_GetNameByINet(aserver), pname);
		     }
		  }
	       }
	    } else {
	       /* Volume location matches the VLDB location */
	       if ( (volumeinfo->backupID && !entry.volumeId[BACKVOL]) ||
		    (volumeinfo->cloneID  && !entry.volumeId[ROVOL])   ||
		    (strncmp(entry.name,volumeinfo->name,VOLSER_OLDMAXVOLNAME) != 0) ) {
		  addvolume++;
	       }
	    }
	 }
      }

      if (addvolume) {
	 entry.flags                   |= RW_EXISTS;
	 entry.volumeId[RWVOL]          = rwvolid;
	 if (!entry.volumeId[BACKVOL])
	    entry.volumeId[BACKVOL]     = volumeinfo->backupID;
	 if (!entry.volumeId[ROVOL])
	    entry.volumeId[ROVOL]       = volumeinfo->cloneID;

	 entry.serverFlags[idx]         = ITSRWVOL;
	 entry.serverNumber[idx]        = aserver;
	 entry.serverPartition[idx]     = apart;
	 strncpy(entry.name, volumeinfo->name, VOLSER_OLDMAXVOLNAME);
	 
	 modified++;

	 /* One last check - to update BK if need to */
	 code = CheckVldbRWBK(&entry, &mod);
	 if (code) ERROR_EXIT(code);
	 if (mod) modified++;
      }
   }

   else if (volumeinfo->type == BACKVOL) {             /* A BK volume */
      if (createentry) {
	 idx = 0;
	 entry.nServers = 1;
	 addvolume++;
      } else {
	 /* Check existence of RW and BK volumes */
	 code = CheckVldbRWBK(&entry, &mod);
	 if (code) ERROR_EXIT(code);
	 if (mod) modified++;

	 idx = Lp_GetRwIndex(&entry);
	 if (idx == -1) {     /* RW index not found in the VLDB entry */
	    idx = entry.nServers;    /* Put it into next index */
	    entry.nServers++;
	    addvolume++;
	 } else {             /* RW index found in the VLDB entry */
	    /* Verify if this volume's location matches where the VLDB says it is */
	    if (!Lp_Match(aserver, apart, &entry)) {
	       /* VLDB says RW and/or BK is elsewhere - report this BK volume orphaned */
	       if (pass == 1) {
		  MapPartIdIntoName(apart, pname);
		  fprintf(STDERR,"*** Warning: Orphaned BK volume %u exists on %s %s\n",
			  volumeinfo->volid, hostutil_GetNameByINet(aserver), pname);
		  MapPartIdIntoName(entry.serverPartition[idx], pname);
		  fprintf(STDERR,"    VLDB reports its RW/BK volume %u exists on %s %s\n",
			  rwvolid,
			  hostutil_GetNameByINet(entry.serverNumber[idx]), pname);
	      }
	    } else {
	       if (volumeinfo->volid != entry.volumeId[BACKVOL]) {
		  if (!(entry.flags & BACK_EXISTS)) {
		     addvolume++;
		  }
		  else if (volumeinfo->volid > entry.volumeId[BACKVOL]) {
		     addvolume++;

		     if (pass == 1) {
			MapPartIdIntoName(entry.serverPartition[idx], pname);
			fprintf(STDERR,"*** Warning: Orphaned BK volume %u exists on %s %s\n",
				entry.volumeId[BACKVOL], hostutil_GetNameByINet(aserver), pname);
			fprintf(STDERR,"    VLDB reports its BK volume ID is %u\n",
				volumeinfo->volid);
		     }
		  } else {
		     if (pass == 1) {
			MapPartIdIntoName(entry.serverPartition[idx], pname);
			fprintf(STDERR,"*** Warning: Orphaned BK volume %u exists on %s %s\n",
				volumeinfo->volid, hostutil_GetNameByINet(aserver), pname);
			fprintf(STDERR,"    VLDB reports its BK volume ID is %u\n",
				entry.volumeId[BACKVOL]);
		     }
		  }
	       }
	       else if (!entry.volumeId[BACKVOL]) {
		  addvolume++;
	       }
	    }
	 }
      }
      if (addvolume) {
	 entry.flags               |= BACK_EXISTS;
	 entry.volumeId[RWVOL]      = rwvolid;
	 entry.volumeId[BACKVOL]    = volumeinfo->volid;

	 entry.serverNumber[idx]    = aserver;
	 entry.serverPartition[idx] = apart;
	 entry.serverFlags[idx]     = ITSRWVOL;

	 modified++;
      }
   }

   else if (volumeinfo->type == ROVOL) {       /* A RO volume */
      if (volumeinfo->volid == entry.volumeId[ROVOL]) {
	 /* This is a quick check to see if the RO entry exists in the 
	  * VLDB so we avoid the CheckVldbRO() call (which checks if each
	  * RO volume listed in the VLDB exists).
	  */
	 idx = Lp_ROMatch(aserver, apart, &entry) - 1;
	 if (idx == -1) {
	    idx = entry.nServers;
	    entry.nServers++;
	    addvolume++;
	 } else {
	    if (!(entry.flags & RO_EXISTS)) {
	       addvolume++;
	    }
	 }
      } else {
	 /* Before we correct the VLDB entry, make sure all the
	  * ROs listed in the VLDB exist.
	  */
	 code = CheckVldbRO(&entry, &mod);
	 if (code) ERROR_EXIT(code);
	 if (mod) modified++;

	 if (!(entry.flags & RO_EXISTS)) {
	    /* No RO exists in the VLDB entry - add this one */
	    idx = entry.nServers;
	    entry.nServers++;
	    addvolume++;
	 }
	 else if (volumeinfo->volid > entry.volumeId[ROVOL]) {
	    /* The volume headers's RO ID does not match that in the VLDB entry,
	     * and the vol hdr's ID is greater (implies more recent). So delete
	     * all the RO volumes listed in VLDB entry and add this volume.
	     */
	    for (j=0; j<entry.nServers; j++) {
	       if (entry.serverFlags[j] & ITSROVOL) {
		  /* Verify this volume exists and print message we are orphaning it */
		  if (pass == 1) {
		     MapPartIdIntoName(apart, entry.serverPartition[j]);
		     fprintf(STDERR,"*** Warning: Orphaned RO volume %u exists on %s %s\n",
			     entry.volumeId[ROVOL],
			     hostutil_GetNameByINet(entry.serverNumber[j]), pname);
		     fprintf(STDERR,"    VLDB reports its RO volume ID is %u\n",
			     volumeinfo->volid);
		  }
		  
		  Lp_SetRWValue(entry, entry.serverNumber[idx],
				entry.serverPartition[idx], 0L, 0L);
		  entry.nServers--;
		  modified++;
		  j--;
	       }
	    }
	    
	    idx = entry.nServers;
	    entry.nServers++;
	    addvolume++;
	 }
	 else if (volumeinfo->volid < entry.volumeId[ROVOL]) {
	    /* The volume headers's RO ID does not match that in the VLDB entry,
	     * and the vol hdr's ID is lower (implies its older). So orphan it.
	     */
	    if (pass == 1) {
	       MapPartIdIntoName(apart, pname);
	       fprintf(STDERR,"*** Warning: Orphaned RO volume %u exists on %s %s\n",
		       volumeinfo->volid, hostutil_GetNameByINet(aserver), pname);
	       fprintf(STDERR,"    VLDB reports its RO volume ID is %u\n",
		       entry.volumeId[ROVOL]);
	    }
	 }
	 else {
	    /* The RO volume ID in the volume header match that in the VLDB entry,
	     * and there exist RO volumes in the VLDB entry. See if any of them
	     * are this one. If not, then we add it.
	  */
	    idx = Lp_ROMatch(aserver, apart, &entry) - 1; 
	    if (idx == -1) {
	       idx = entry.nServers;
	       entry.nServers++;
	       addvolume++;
	    }
	 }
      }

      if (addvolume) {
	 entry.flags               |= RO_EXISTS;
	 entry.volumeId[RWVOL]      = rwvolid;
	 entry.volumeId[ROVOL]      = volumeinfo->volid;

	 entry.serverNumber[idx]    = aserver;
	 entry.serverPartition[idx] = apart;
	 entry.serverFlags[idx]     = ITSROVOL;

	 modified++;
      }
   }

   /* Remember largest volume id */
   if (entry.volumeId[ROVOL]   > *maxvolid) *maxvolid = entry.volumeId[ROVOL];
   if (entry.volumeId[BACKVOL] > *maxvolid) *maxvolid = entry.volumeId[BACKVOL];
   if (entry.volumeId[RWVOL]   > *maxvolid) *maxvolid = entry.volumeId[RWVOL];

   if (modified) {
      MapNetworkToHost(&entry, &storeEntry);

      if (createentry) {
	 code = VLDB_CreateEntry(&storeEntry);
	 if (code) {
	    fprintf(STDOUT,"Could not create a VLDB entry for the volume %u\n", rwvolid);
	    ERROR_EXIT(code);
	 }
      }
      else {
	 if (pass == 1) goto retry;
	 code = VLDB_ReplaceEntry(rwvolid, RWVOL, &storeEntry,
				  LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	 if (code) {
	    fprintf(STDERR,"Could not update entry for %u\n", rwvolid);
	    ERROR_EXIT(code);
	 }
      }
      if (modentry) *modentry = modified;
   } else if (pass == 2) {
      code = ubik_Call(VL_ReleaseLock,cstruct, 0, rwvolid, RWVOL,
		       LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
      if (code) {
	 PrintError("Could not unlock VLDB entry ", code);
      }
   }

   if (verbose) {
      fprintf(STDOUT,"-- status after --\n");
      if (modified)
	 EnumerateEntry(&entry);
      else
	 fprintf(STDOUT,"\n**no change**\n");
   }

 error_exit:
   if (verbose) {
      fprintf(STDOUT,"\n_______________________________\n");
   }
   return(error);
}

int sortVolumes(v1, v2)
   volintInfo *v1, *v2;
{
   afs_int32 rwvolid1, rwvolid2;

   rwvolid1 = ((v1->type == RWVOL) ? v1->volid : v1->parentID);
   rwvolid2 = ((v2->type == RWVOL) ? v2->volid : v2->parentID);

   if (rwvolid1 > rwvolid2) return -1;    /* lower RW id goes first */
   if (rwvolid1 < rwvolid2) return  1;

   if (v1->type == RWVOL) return -1;      /* RW vols go first */
   if (v2->type == RWVOL) return  1;

   if ((v1->type == BACKVOL) && (v2->type == ROVOL  )) return -1;    /* BK vols next */
   if ((v1->type == ROVOL  ) && (v2->type == BACKVOL)) return  1;

   if (v1->volid < v2->volid) return  1;           /* larger volids first */
   if (v1->volid > v2->volid) return -1;
   return 0;
}

/* UV_SyncVolume()
 *      Synchronise <aserver> <apart>(if flags = 1) <avolid>.
 *      Synchronize an individual volume against a sever and partition.
 *      Checks the VLDB entry (similar to syncserv) as well as checks
 *      if the volume exists on specified servers (similar to syncvldb).
 */
UV_SyncVolume(aserver, apart, avolname, flags)
    afs_int32 aserver, apart;
    char *avolname;
    int flags;
{
    struct rx_connection *aconn = 0;
    afs_int32 j, k, code, vcode, error = 0;
    afs_int32 tverbose, mod, modified = 0;
    struct nvldbentry vldbentry;
    afs_int32 volumeid = 0;
    volEntries volumeInfo;
    struct partList PartList;
    afs_int32 pcnt, rv;
    afs_int32 maxvolid = 0;

    volumeInfo.volEntries_val = (volintInfo *)0;
    volumeInfo.volEntries_len = 0;

    if (!aserver && flags) {
       /* fprintf(STDERR,"Partition option requires a server option\n"); */
       ERROR_EXIT(EINVAL);
    }

    /* Turn verbose logging off and do our own verbose logging */
    tverbose = verbose;
    verbose  = 0;

    /* Read the VLDB entry */
    vcode = VLDB_GetEntryByName(avolname, &vldbentry);
    if (vcode && (vcode != VL_NOENT)) {
       fprintf(STDERR,"Could not access the VLDB for volume %s\n", avolname);
       ERROR_EXIT(vcode);
    } else if (!vcode) {
       MapHostToNetwork(&vldbentry);
    }

    if (tverbose) {
       fprintf(STDOUT,"Processing VLDB entry %s ...\n", avolname);
       fprintf(STDOUT,"_______________________________\n");
       fprintf(STDOUT,"\n-- status before -- \n");
       if (vcode) {
	  fprintf(STDOUT,"\n**does not exist**\n");
       } else {
	  if ((vldbentry.flags & RW_EXISTS) ||
	      (vldbentry.flags & RO_EXISTS) || 
	      (vldbentry.flags & BACK_EXISTS))
	    EnumerateEntry(&vldbentry);
       }
       fprintf(STDOUT,"\n");
    }

    /* Verify that all of the VLDB entries exist on the repective servers 
     * and partitions (this does not require that avolname be a volume ID).
     * Equivalent to a syncserv.
     */
    if (!vcode) {
       code = CheckVldb(&vldbentry, &mod);
       if (code) {
	  fprintf(STDERR,"Could not process VLDB entry for volume %s\n", vldbentry.name);
	  ERROR_EXIT(code);
       }
       if (mod) modified++;
    }

    /* If aserver is given, we will search for the desired volume on it */
    if (aserver) {
       /* Generate array of partitions on the server that we will check */
       if (!flags) {
	  code = UV_ListPartitions(aserver, &PartList, &pcnt);
	  if (code) {
	     fprintf(STDERR,"Could not fetch the list of partitions from the server\n");
	     ERROR_EXIT(code);
	  }
       } else {
	  PartList.partId[0] = apart;
	  pcnt = 1;
       }

       aconn = UV_Bind(aserver,AFSCONF_VOLUMEPORT);

       /* If a volume ID were given, search for it on each partition */
       if (volumeid = atol(avolname)) {
	  for (j=0; j<pcnt; j++) {
	     code = AFSVolListOneVolume(aconn, PartList.partId[j], volumeid, &volumeInfo);
	     if (code) {
	        if (code != ENODEV) {
		   fprintf(STDERR,"Could not query server\n");
		   ERROR_EXIT(code);
		}
	     } else {
	        /* Found one, sync it with VLDB entry */
	        code = CheckVolume(volumeInfo.volEntries_val, aserver, 
				   PartList.partId[j], &mod, &maxvolid);
		if (code) ERROR_EXIT(code);
		if (mod) modified++;
	     }

	     if (volumeInfo.volEntries_val)
	        free(volumeInfo.volEntries_val);
	     volumeInfo.volEntries_val = (volintInfo *)0;
	     volumeInfo.volEntries_len = 0;
	  }
       }

       /* Check to see if the RW, BK, and RO IDs exist on any
	* partitions. We get the volume IDs from the VLDB.
	*/
       rv = 1;                      /* Read the VLDB entry ? */
       for (j=0; j<MAXTYPES; j++) { /* for RW, RO, and BK IDs */
	  if (rv) {
	     vcode = VLDB_GetEntryByName(avolname, &vldbentry);
	     if (vcode) {
	        if (vcode == VL_NOENT) break;
		fprintf(STDERR,"Could not access the VLDB for volume %s\n", avolname);
		ERROR_EXIT(vcode);
	     }
	     rv = 0;
	  }

	  if (vldbentry.volumeId[j] == 0) continue;

	  for (k=0; k<pcnt; k++) {      /* For each partition */
	     volumeInfo.volEntries_val = (volintInfo *)0;
	     volumeInfo.volEntries_len = 0;
	     code = AFSVolListOneVolume(aconn, PartList.partId[k],
					vldbentry.volumeId[j], &volumeInfo);
	     if (code) {
	        if (code != ENODEV) {
		   fprintf(STDERR,"Could not query server\n");
		   ERROR_EXIT(code);
		}
	     } else {
	        /* Found one, sync it with VLDB entry */
	        code = CheckVolume(volumeInfo.volEntries_val, aserver, 
				   PartList.partId[k], &mod, &maxvolid);
		if (code) ERROR_EXIT(code);
		if (mod) modified++, rv++;
	     }

	     if (volumeInfo.volEntries_val)
	        free(volumeInfo.volEntries_val);
	     volumeInfo.volEntries_val = (volintInfo *)0;
	     volumeInfo.volEntries_len = 0;
	  }
       }
    } /* if (aserver) */

    /* If verbose output, print a summary of what changed */
    if (tverbose) {
       fprintf(STDOUT,"-- status after --\n");
       code = VLDB_GetEntryByName(avolname, &vldbentry);
       if (code && (code != VL_NOENT)) {
	  fprintf(STDERR,"Could not access the VLDB for volume %s\n", avolname);
	  ERROR_EXIT(code);
       }
       if (modified && (code == VL_NOENT)) {
	  fprintf(STDOUT,"\n**entry deleted**\n");
       } else if (modified) {
	  EnumerateEntry(vldbentry);
       } else {
	  fprintf(STDOUT,"\n**no change**\n");
       }
       fprintf(STDOUT,"\n_______________________________\n");
    }

  error_exit:    
    /* Now check if the maxvolid is larger than that stored in the VLDB */
    if (maxvolid) {
       afs_int32 maxvldbid = 0;
       code = ubik_Call(VL_GetNewVolumeId,cstruct, 0, 0, &maxvldbid);
       if (code) {
	  fprintf(STDERR, "Could not get the highest allocated volume id from the VLDB\n");
	  if (!error) error = code;
       } else if (maxvolid > maxvldbid) {
	  afs_uint32 id, nid;
	  id = maxvolid - maxvldbid + 1;
	  code = ubik_Call(VL_GetNewVolumeId, cstruct, 0, id, &nid);
	  if (code) {
	     fprintf(STDERR,"Error in increasing highest allocated volume id in VLDB\n");
	     if (!error) error = code;
	  }
       }
    }

    verbose = tverbose;
    if (verbose) {
       if (error) fprintf(STDOUT,"...error encountered");
       else       fprintf(STDOUT,"...done entry\n");
    }
    if (aconn) rx_DestroyConnection(aconn);
    if (volumeInfo.volEntries_val) free(volumeInfo.volEntries_val);

    PrintError("",error);
    return error;
}

/* UV_SyncVldb()
 *      Synchronise vldb with the file server <aserver> and,
 *      optionally, <apart>.
 */
UV_SyncVldb(aserver, apart, flags, force)
    afs_int32 aserver, apart;
    int flags, force;
{
    struct rx_connection *aconn;
    afs_int32 code, error=0;
    int i, j, pfail;
    volEntries volumeInfo;
    struct partList PartList;
    afs_int32 pcnt;
    char pname[10];
    volintInfo *vi;
    afs_int32 failures = 0, modifications = 0, tentries = 0;
    afs_int32 modified;
    afs_uint32 maxvolid = 0;

    volumeInfo.volEntries_val = (volintInfo *)0;
    volumeInfo.volEntries_len = 0;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    /* Generate array of partitions to check */
    if (!flags) {
       code = UV_ListPartitions(aserver, &PartList, &pcnt);
       if (code) {
	  fprintf(STDERR,"Could not fetch the list of partitions from the server\n");
	  ERROR_EXIT(code);
       }
    } else {
       PartList.partId[0] = apart;
       pcnt = 1;
    }

    if (verbose) {
       fprintf(STDOUT,"Processing volume entries ...\n");
       fflush(STDOUT);
    }

    /* Step through the array of partitions */
    for (i = 0; i < pcnt; i++) {
       apart = PartList.partId[i];
       MapPartIdIntoName(apart, pname);

       volumeInfo.volEntries_val = (volintInfo *)0;
       volumeInfo.volEntries_len = 0;
       code = AFSVolListVolumes(aconn, apart, 1, &volumeInfo);
       if (code) {
	  fprintf(STDERR,"Could not fetch the list of volumes from the server\n");
	  ERROR_EXIT(code);
       }

       /* May want to sort the entries: RW, BK (high to low), RO (high to low) */
       qsort((char *)volumeInfo.volEntries_val, volumeInfo.volEntries_len, 
	     sizeof(volintInfo), sortVolumes);

       pfail = 0;
       for (vi=volumeInfo.volEntries_val, j=0; j < volumeInfo.volEntries_len; j++, vi++) {
	  if (!vi->status)
	     continue;

	  tentries++;

	  if (verbose) {
	     fprintf(STDOUT,"Processing volume entry %d: %s (%u) on server %s %s...\n",
		     j+1, vi->name, vi->volid,
		     hostutil_GetNameByINet(aserver), pname);
	     fflush(STDOUT);
	  }

	  code = CheckVolume(vi, aserver, apart, &modified, &maxvolid);
	  if (code) {
	     PrintError("",code);
	     failures++;
	     pfail++;
	  }
	  else if (modified) {
	     modifications++;
	  }

	  if (verbose) {
	     if (code) {
		fprintf(STDOUT,"...error encountered\n\n");
	     } else {
		fprintf(STDOUT,"...done entry %d\n\n", j+1);
	     }
	  }
       }

       if (pfail) {
	  fprintf(STDERR,"Could not process entries on server %s partition %s\n",
		  hostutil_GetNameByINet(aserver), pname);
       }
       if (volumeInfo.volEntries_val) {
	  free(volumeInfo.volEntries_val);
	  volumeInfo.volEntries_val = 0;
       }

    }/* thru all partitions */

    if (verbose) {
       fprintf(STDOUT, "Total entries: %u, Failed to process %d, Changed %d\n",
	       tentries, failures, modifications);
    }

  error_exit:
    /* Now check if the maxvolid is larger than that stored in the VLDB */
    if (maxvolid) {
       afs_uint32 maxvldbid = 0;
       code = ubik_Call(VL_GetNewVolumeId,cstruct, 0, 0, &maxvldbid);
       if (code) {
	  fprintf(STDERR, "Could not get the highest allocated volume id from the VLDB\n");
	  if (!error) error = code;
       } else if (maxvolid > maxvldbid) {
	  afs_uint32 id, nid;
	  id = maxvolid - maxvldbid + 1;
	  code = ubik_Call(VL_GetNewVolumeId, cstruct, 0, id, &nid);
	  if (code) {
	     fprintf(STDERR,"Error in increasing highest allocated volume id in VLDB\n");
	     if (!error) error = code;
	  }
       }
    }

    if (aconn) rx_DestroyConnection(aconn);
    if (volumeInfo.volEntries_val)
       free(volumeInfo.volEntries_val);
    PrintError("",error);
    return(error);
}

/* VolumeExists()
 *      Determine if a volume exists on a server and partition.
 *      Try creating a transaction on the volume. If we can,
 *      the volume exists, if not, then return the error code.
 *      Some error codes mean the volume is unavailable but
 *      still exists - so we catch these error codes.
 */
afs_int32 VolumeExists(server, partition, volumeid)
     afs_int32 server, partition, volumeid;
{
   struct rx_connection *conn=(struct rx_connection *)0;
   afs_int32                code = -1;
   volEntries           volumeInfo;

   conn = UV_Bind(server, AFSCONF_VOLUMEPORT);
   if (conn) {
      volumeInfo.volEntries_val = (volintInfo *)0;
      volumeInfo.volEntries_len = 0;
      code = AFSVolListOneVolume(conn, partition, volumeid, &volumeInfo);
      if (volumeInfo.volEntries_val)
	 free(volumeInfo.volEntries_val);
      if (code == VOLSERILLEGAL_PARTITION)
	 code = ENODEV;
      rx_DestroyConnection(conn);
   }
   return code;
}

/* CheckVldbRWBK()
 *
 */
afs_int32 CheckVldbRWBK(entry, modified)
   struct nvldbentry *entry;
   afs_int32             *modified;
{
   int modentry = 0;
   int idx;
   afs_int32 code, error = 0;
   char pname[10];

   if (modified) *modified = 0;
   idx = Lp_GetRwIndex(entry);

   /* Check to see if the RW volume exists and set the RW_EXISTS
    * flag accordingly.
    */
   if (idx == -1) {                          /* Did not find a RW entry */
      if (entry->flags & RW_EXISTS) {        /* ... yet entry says RW exists */
	 entry->flags &= ~RW_EXISTS;         /* ... so say RW does not exist */
	 modentry++;
      }
   } else {
      code = VolumeExists(entry->serverNumber[idx],
			  entry->serverPartition[idx],
			  entry->volumeId[RWVOL]);
      if (code == 0) {                          /* RW volume exists */
	 if (!(entry->flags & RW_EXISTS)) {     /* ... yet entry says RW does not exist */
	    entry->flags |= RW_EXISTS;          /* ... so say RW does exist */
	    modentry++;
	 }
      } 
      else if (code == ENODEV) {                /* RW volume does not exist */
	 if (entry->flags & RW_EXISTS) {        /* ... yet entry says RW exists */
	    entry->flags &= ~RW_EXISTS;         /* ... so say RW does not exist */
	    modentry++;
	 }
      }
      else {
	 /* If VLDB says it didn't exist, then ignore error */
	 if (entry->flags & RW_EXISTS) {
	    MapPartIdIntoName(entry->serverPartition[idx], pname);
	    fprintf(STDERR,"Transaction call failed for RW volume %u on server %s %s\n",
		    entry->volumeId[RWVOL], 
		    hostutil_GetNameByINet(entry->serverNumber[idx]), pname);
	    ERROR_EXIT(code);
	 }
      }
   }

   /* Check to see if the BK volume exists and set the BACK_EXISTS
    * flag accordingly. idx already ponts to the RW entry.
    */
   if (idx == -1) {                         /* Did not find a RW entry */
      if (entry->flags & BACK_EXISTS) {     /* ... yet entry says BK exists */
	 entry->flags &= ~BACK_EXISTS;      /* ... so say BK does not exist */
	 modentry++;
      }
   } 
   else {                                            /* Found a RW entry */
      code = VolumeExists(entry->serverNumber[idx],
			  entry->serverPartition[idx],
			  entry->volumeId[BACKVOL]);
      if (code == 0) {                           /* BK volume exists */
	 if (!(entry->flags & BACK_EXISTS)) {    /* ... yet entry says BK does not exist */	 
	    entry->flags |= BACK_EXISTS;         /* ... so say BK does exist */
	    modentry++;
	 }
      }
      else if (code == ENODEV) {                 /* BK volume does not exist */
	 if (entry->flags & BACK_EXISTS) {       /* ... yet entry says BK exists */
	    entry->flags &= ~BACK_EXISTS;        /* ... so say BK does not exist */
	    modentry++;
	 }
      } 
      else {
	 /* If VLDB says it didn't exist, then ignore error */
	 if (entry->flags & BACK_EXISTS) {
	    MapPartIdIntoName(entry->serverPartition[idx], pname);
	    fprintf(STDERR,"Transaction call failed for BK volume %u on server %s %s\n",
		    entry->volumeId[BACKVOL],
		    hostutil_GetNameByINet(entry->serverNumber[idx]), pname);
	    ERROR_EXIT(code);
	 }
      }
   }

   /* If there is an idx but the BK and RW volumes no
    * longer exist, then remove the RW entry.
    */
   if ((idx != -1) && !(entry->flags & RW_EXISTS) &&
                      !(entry->flags & BACK_EXISTS)) {
      Lp_SetRWValue(entry, entry->serverNumber[idx],
		    entry->serverPartition[idx], 0L, 0L);
      entry->nServers--;
      modentry++;
   }
   
 error_exit:
   if (modified) *modified = modentry;
   return(error);
}

CheckVldbRO(entry, modified)
   struct nvldbentry *entry;
   afs_int32             *modified;
{
   int idx;
   int foundro = 0, modentry = 0;
   afs_int32 code, error = 0;
   char pname[10];

   if (modified) *modified = 0;

   /* Check to see if the RO volumes exist and set the RO_EXISTS
    * flag accordingly. 
    */
   for (idx=0; idx < entry->nServers; idx++) {
      if (!(entry->serverFlags[idx] & ITSROVOL)) {
	 continue;   /* not a RO */
      }

      code = VolumeExists(entry->serverNumber[idx],
			  entry->serverPartition[idx],
			  entry->volumeId[ROVOL]);
      if (code == 0) {                          /* RO volume exists */
	 foundro++;
      } 
      else if (code == ENODEV) {                /* RW volume does not exist */
	 Lp_SetROValue(entry, entry->serverNumber[idx],
		       entry->serverPartition[idx], 0L, 0L);
	 entry->nServers--;
	 idx--;
	 modentry++;
      }
      else {
	 MapPartIdIntoName(entry->serverPartition[idx], pname);
	 fprintf(STDERR,"Transaction call failed for RO %u on server %s %s\n",
		 entry->volumeId[ROVOL],
		 hostutil_GetNameByINet(entry->serverNumber[idx]), pname);
	 ERROR_EXIT(code);
      }
   }

   if (foundro) {                            /* A RO volume exists */
      if (!(entry->flags & RO_EXISTS)) {     /* ... yet entry says RW does not exist */
	 entry->flags |= RO_EXISTS;          /* ... so say RW does exist */
	 modentry++;
      }
   } else {                                  /* A RO volume does not exist */
      if (entry->flags & RO_EXISTS) {        /* ... yet entry says RO exists */
	 entry->flags &= ~RO_EXISTS;         /* ... so say RO does not exist */
	 modentry++;
      }
   }

 error_exit:
   if (modified) *modified = modentry;
   return(error);
}

/* CheckVldb()
 *      Ensure that <entry> matches with the info on file servers
 */
afs_int32 CheckVldb(entry, modified)
   struct nvldbentry *entry;
   afs_int32             *modified;
{
   afs_int32 code, error=0;
   int idx;
   struct nvldbentry storeEntry;
   int islocked=0, mod, modentry, delentry=0;
   int foundro, pass=0;

   if (modified) *modified = 0;
   if (verbose) {
      fprintf(STDOUT,"_______________________________\n");
      fprintf(STDOUT,"\n-- status before -- \n");
      if ((entry->flags & RW_EXISTS) ||
	  (entry->flags & RO_EXISTS) || 	
	  (entry->flags & BACK_EXISTS))
	EnumerateEntry(entry);
      fprintf(STDOUT,"\n");
   }

   if (strlen(entry->name) > (VOLSER_OLDMAXVOLNAME - 10)) {
      fprintf(STDERR,"Volume name %s exceeds limit of %d characters\n",
	      entry->name, VOLSER_OLDMAXVOLNAME-10);
   }

 retry:
   /* Check to see if the VLDB is ok without locking it (pass 1).
    * If it will change, then lock the VLDB entry, read it again,
    * then make the changes to it (pass 2).
    */
   if (++pass == 2) {
      code = ubik_Call(VL_SetLock,cstruct, 0, entry->volumeId[RWVOL], RWVOL, VLOP_DELETE);
      if (code) {
	 fprintf(STDERR, "Could not lock VLDB entry for %u \n",entry->volumeId[RWVOL] );
	 ERROR_EXIT(code);
      }
      islocked = 1;

      code = VLDB_GetEntryByID(entry->volumeId[RWVOL], RWVOL, entry);
      if (code) {
	 fprintf(STDERR,"Could not read VLDB entry for volume %s\n", entry->name);
	 ERROR_EXIT(code);
      }
      else {
	 MapHostToNetwork(entry);
      }
   }

   modentry = 0;

   /* Check if the RW and BK entries are ok */
   code = CheckVldbRWBK(entry, &mod);
   if (code) ERROR_EXIT(code);
   if (mod && (pass == 1)) goto retry;
   if (mod) modentry++;

   /* Check if the RO volumes entries are ok */
   code = CheckVldbRO(entry, &mod);
   if (code) ERROR_EXIT(code);
   if (mod && (pass == 1)) goto retry;
   if (mod) modentry++;

   /* The VLDB entry has been updated. If it as been modified, then 
    * write the entry back out the the VLDB.
    */
   if (modentry) {
      if (pass == 1) goto retry;

      if (!(entry->flags & RW_EXISTS)   && 
	  !(entry->flags & BACK_EXISTS) && 
	  !(entry->flags & RO_EXISTS)) {
	 /* The RW, BK, nor RO volumes do not exist. Delete the VLDB entry */
	 code = ubik_Call(VL_DeleteEntry, cstruct, 0, entry->volumeId[RWVOL], RWVOL);
	 if (code) {
	    fprintf(STDERR,"Could not delete VLDB entry for volume %u \n",
		    entry->volumeId[RWVOL]);
	    ERROR_EXIT(code);
	 }
	 delentry = 1;
      }
      else {
	 /* Replace old entry with our new one */
	 MapNetworkToHost(entry,&storeEntry);
	 code = VLDB_ReplaceEntry(entry->volumeId[RWVOL], RWVOL, &storeEntry,
				  (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP)); 
	 if (code) {
	    fprintf(STDERR,"Could not update VLDB entry for volume %u\n",
		    entry->volumeId[RWVOL] );
	    ERROR_EXIT(code);
	 }
      }
      if (modified) *modified = 1;
      islocked = 0;
   }

   if (verbose) {
      fprintf(STDOUT,"-- status after --\n");
      if (delentry)
	 fprintf(STDOUT,"\n**entry deleted**\n");
      else if (modentry)
	 EnumerateEntry(entry);
      else
	 fprintf(STDOUT,"\n**no change**\n");
   }

 error_exit:
   if (verbose) {
      fprintf(STDOUT,"\n_______________________________\n");
   }

   if (islocked) {
      code = ubik_Call(VL_ReleaseLock, cstruct, 0, entry->volumeId[RWVOL], RWVOL,
		       (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
      if (code) {
	 fprintf(STDERR,"Could not release lock on VLDB entry for volume %u\n",
		 entry->volumeId[RWVOL]);
	 if (!error) error = code;
      }
   }
   return error;
}

/* UV_SyncServer()
 *      Synchronise <aserver> <apart>(if flags = 1) with the VLDB.
 */
UV_SyncServer(aserver, apart, flags, force)
    afs_int32 aserver, apart;
    int flags, force;
{
    struct rx_connection *aconn;
    afs_int32 code, error = 0;
    afs_int32 nentries, tentries=0;
    struct VldbListByAttributes attributes;
    nbulkentries arrayEntries;
    afs_int32 failures=0, modified, modifications=0;
    struct nvldbentry *vlentry;
    afs_int32 si, nsi, j;

    aconn = UV_Bind(aserver,AFSCONF_VOLUMEPORT);

    /* Set up attributes to search VLDB  */
    attributes.server    = ntohl(aserver);
    attributes.Mask      = VLLIST_SERVER;
    if (flags) {
       attributes.partition  = apart;
       attributes.Mask      |= VLLIST_PARTITION;
    }

    if (verbose) {
       fprintf(STDOUT,"Processing VLDB entries ...\n");
       fflush(STDOUT);
    }

    /* While we need to collect more VLDB entries */
    for (si=0; si != -1; si=nsi) {
       bzero(&arrayEntries, sizeof(arrayEntries));

       /* Collect set of VLDB entries */
       code = VLDB_ListAttributesN2(&attributes, 0, si,
				     &nentries, &arrayEntries, &nsi);
       if (code == RXGEN_OPCODE) {
	  code = VLDB_ListAttributes(&attributes, &nentries, &arrayEntries);
	  nsi = -1;
       }
       if (code) {
	  fprintf(STDERR,"Could not access the VLDB for attributes\n");
	  ERROR_EXIT(code);
       }
       tentries += nentries;

       for (j=0; j<nentries; j++) {
	  vlentry = &arrayEntries.nbulkentries_val[j];
	  MapHostToNetwork(vlentry);

	  if (verbose) {
	     fprintf(STDOUT,"Processing VLDB entry %d ...\n", j+1);
	     fflush(STDOUT);
	  }

	  code = CheckVldb(vlentry, &modified);
	  if (code) {
	     PrintError("",code);
	     fprintf(STDERR,"Could not process VLDB entry for volume %s\n",
		     vlentry->name);
	     failures++;
	  } else if (modified) {
	     modifications++;
	  }

	  if (verbose) {
	     if (code) {
		fprintf(STDOUT,"...error encountered\n\n");
	     } else {
		fprintf(STDOUT,"...done entry %d\n\n", j+1);
	     }
	  }
       }

       if (arrayEntries.nbulkentries_val) {
	  free(arrayEntries.nbulkentries_val);
	  arrayEntries.nbulkentries_val = 0;
       }
    }

    if (verbose) {
       fprintf(STDOUT,"Total entries: %u, Failed to process %d, Changed %d\n",
	       tentries, failures, modifications);
    }

  error_exit:    
    if (aconn)
       rx_DestroyConnection(aconn);
    if (arrayEntries.nbulkentries_val)
       free(arrayEntries.nbulkentries_val);

    if (failures)
       error = VOLSERFAILEDOP;
    return error;
}

/*rename volume <oldname> to <newname>, changing the names of the related 
 *readonly and backup volumes. This operation is also idempotent.
 *salvager is capable of recovering from rename operation stopping halfway.
 *to recover run syncserver on the affected machines,it will force renaming to completion. name clashes should have been detected before calling this proc */
UV_RenameVolume(entry,oldname,newname)
struct nvldbentry *entry;
char oldname[],newname[];
{
    struct nvldbentry storeEntry;
    afs_int32 vcode,code,rcode,error;
    int i,index;
    char nameBuffer[256];
    afs_int32 tid;
    struct rx_connection *aconn;
    int islocked;

    error = 0;
    aconn = (struct rx_connection *)0;
    tid = 0;
    islocked = 0;

    vcode = ubik_Call(VL_SetLock,cstruct, 0,entry->volumeId[RWVOL], RWVOL, VLOP_ADDSITE);/*last param is dummy*/
    if(vcode){
	fprintf(STDERR," Could not lock the VLDB entry for the  volume %u \n",entry->volumeId[RWVOL] );
	error = vcode;
	goto rvfail;
    }
    islocked = 1;
    strncpy(entry->name,newname,VOLSER_OLDMAXVOLNAME);
    MapNetworkToHost(entry,&storeEntry);
    vcode = VLDB_ReplaceEntry(entry->volumeId[RWVOL],RWVOL, &storeEntry,0 );
    if (vcode) {
	fprintf(STDERR,"Could not update VLDB entry for %u\n",entry->volumeId[RWVOL]);
	error = vcode;
	goto rvfail;
    }
    if(verbose) fprintf(STDOUT,"Recorded the new name %s in VLDB\n",newname);
    /*at this stage the intent to rename is recorded in the vldb, as far as the vldb 
      is concerned, oldname is lost */
    if(entry->flags & RW_EXISTS) {
	index = Lp_GetRwIndex(entry);
	if(index == -1){ /* there is a serious discrepancy */
	    fprintf(STDERR,"There is a serious discrepancy in VLDB entry for volume %u\n",entry->volumeId[RWVOL]);
	    fprintf(STDERR,"try building VLDB from scratch\n");
	    error = VOLSERVLDB_ERROR;
	    goto rvfail;
	}
	aconn = UV_Bind(entry->serverNumber[index],AFSCONF_VOLUMEPORT);
	code = AFSVolTransCreate(aconn,entry->volumeId[RWVOL],entry->serverPartition[index],  ITOffline, &tid);
	if(code) { /*volume doesnot exist */
	    fprintf(STDERR,"Could not start transaction on the rw volume %u\n",entry->volumeId[RWVOL]);
	    error = code;
	    goto rvfail;
	}
	else {/*volume exists, process it */

	    code = AFSVolSetIdsTypes(aconn, tid, newname,RWVOL, entry->volumeId[RWVOL],entry->volumeId[ROVOL],entry->volumeId[BACKVOL]);
	    if(!code) {
		if(verbose) printf("Renamed rw volume %s to %s\n",oldname,newname);
		code = AFSVolEndTrans(aconn, tid, &rcode);
		tid = 0;
		if(code) {
		    fprintf(STDERR,"Could not  end transaction on volume %s %u\n",entry->name,entry->volumeId[RWVOL]);
		    error = code;
		    goto rvfail;
		}
	    }
	    else {
		fprintf(STDERR,"Could not  set parameters on volume %s %u\n",entry->name,entry->volumeId[RWVOL]);
		error = code;
		goto rvfail;
	    }
	}
	if(aconn) rx_DestroyConnection(aconn);
	aconn = (struct rx_connection *)0;
    } /*end rw volume processing */

    if(entry->flags & BACK_EXISTS) {/*process the backup volume */
	index = Lp_GetRwIndex(entry);
	if(index == -1){ /* there is a serious discrepancy */
	    fprintf(STDERR,"There is a serious discrepancy in the VLDB entry for the backup volume %u\n",entry->volumeId[BACKVOL]);
	    fprintf(STDERR,"try building VLDB from scratch\n");
	    error = VOLSERVLDB_ERROR;
	    goto rvfail;
	}
	aconn = UV_Bind(entry->serverNumber[index],AFSCONF_VOLUMEPORT);
	code = AFSVolTransCreate(aconn,entry->volumeId[BACKVOL],entry->serverPartition[index],  ITOffline, &tid);
	if(code) { /*volume doesnot exist */
	    fprintf(STDERR,"Could not start transaction on the backup volume  %u\n",entry->volumeId[BACKVOL]);
	    error = code;
	    goto rvfail;
	}
	else {/*volume exists, process it */
	    if(strlen(newname) > (VOLSER_OLDMAXVOLNAME - 8)){
		fprintf(STDERR,"Volume name %s.backup exceeds the limit of %u characters\n",newname,VOLSER_OLDMAXVOLNAME);
		error = code;
		goto rvfail;
	    }
	    strcpy(nameBuffer,newname);
	    strcat(nameBuffer,".backup");

	    code = AFSVolSetIdsTypes(aconn, tid,nameBuffer ,BACKVOL, entry->volumeId[RWVOL],0,0);
	    if(!code) {
		if(verbose) fprintf(STDOUT,"Renamed backup volume to %s \n",nameBuffer);
		code = AFSVolEndTrans(aconn, tid, &rcode);
		tid = 0;
		if(code) {
		    fprintf(STDERR,"Could not  end transaction on the backup volume %u\n",entry->volumeId[BACKVOL]);
		    error = code;
		    goto rvfail;
		}
	    }
	    else {
		fprintf(STDERR,"Could not  set parameters on the backup volume %u\n",entry->volumeId[BACKVOL]);
		error = code;
		goto rvfail;
	    }
	}
    } /* end backup processing */
    if(aconn) rx_DestroyConnection(aconn);
    aconn = (struct rx_connection *)0;
    if(entry->flags & RO_EXISTS){  /*process the ro volumes */
	for(i = 0; i < entry->nServers; i++){
	    if(entry->serverFlags[i] & ITSROVOL) {
		aconn = UV_Bind(entry->serverNumber[i],AFSCONF_VOLUMEPORT);
		code = AFSVolTransCreate(aconn,entry->volumeId[ROVOL],entry->serverPartition[i],  ITOffline, &tid);
		if(code) { /*volume doesnot exist */
		    fprintf(STDERR,"Could not start transaction on the ro volume %u\n",entry->volumeId[ROVOL]);
		    error = code;
		    goto rvfail;
		}
		else {/*volume exists, process it */
		    strcpy(nameBuffer,newname);
		    strcat(nameBuffer,".readonly");
		    if(strlen(nameBuffer) > (VOLSER_OLDMAXVOLNAME - 1)){
			fprintf(STDERR,"Volume name %s exceeds the limit of %u characters\n",nameBuffer,VOLSER_OLDMAXVOLNAME);
			error = code;
			goto rvfail;
		    }
		    code = AFSVolSetIdsTypes(aconn, tid, nameBuffer,ROVOL, entry->volumeId[RWVOL],0,0);
		    if(!code) {
			if(verbose) fprintf(STDOUT,"Renamed RO volume %s on host %s\n",
					    nameBuffer,
					    hostutil_GetNameByINet(entry->serverNumber[i]));
			code = AFSVolEndTrans(aconn, tid, &rcode);
			tid = 0;
			if (code) {
			    fprintf(STDERR,"Could not  end transaction on volume %u\n",entry->volumeId[ROVOL]);
			    error = code;
			    goto rvfail;
			}
		    }
		    else {
			fprintf(STDERR,"Could not  set parameters on the ro volume %u\n",entry->volumeId[ROVOL]);
			error = code;
			goto rvfail;
		    }
		}
		if(aconn) rx_DestroyConnection(aconn);
		aconn = (struct rx_connection *)0;
	    }
	}
    }
rvfail:
    if(islocked) {
	vcode = ubik_Call(VL_ReleaseLock,cstruct, 0,entry->volumeId[RWVOL] , RWVOL, LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if(vcode){
	    fprintf(STDERR,"Could not unlock the VLDB entry for the volume %s %u\n",entry->name,entry->volumeId[RWVOL]);
	    if(!error) error = vcode;
	}
    }
    if(tid) {
	code =  AFSVolEndTrans(aconn, tid, &rcode);
	if(!code) code = rcode;
	if(code) {
	    fprintf(STDERR,"Failed to end transaction on a volume \n");
	    if(!error) error = code;
	}
    }
    if(aconn) rx_DestroyConnection(aconn);
    PrintError("",error);
    return error;
    
}

/*report on all the active transactions on volser */
UV_VolserStatus(server,rpntr,rcount)
afs_int32 server;
transDebugInfo **rpntr;
afs_int32 *rcount;
{
    struct rx_connection *aconn;
    transDebugEntries transInfo;
    afs_int32 code = 0;
    
    aconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    transInfo.transDebugEntries_val = (transDebugInfo *) 0;
    transInfo.transDebugEntries_len = 0;
    code = AFSVolMonitor(aconn,&transInfo);
    if(code) {
	fprintf(STDERR,"Could not access status information about the server\n");
	PrintError("",code);
	if (transInfo.transDebugEntries_val) free(transInfo.transDebugEntries_val);
	if(aconn) rx_DestroyConnection(aconn);
	return code;
    }
    else {
	*rcount = transInfo.transDebugEntries_len;
	*rpntr = transInfo.transDebugEntries_val;
	if(aconn) rx_DestroyConnection(aconn);
	return 0;
    }
    

}	    
/*delete the volume without interacting with the vldb */
UV_VolumeZap(server,part,volid)
afs_int32 volid,server,part;

{
    afs_int32 rcode,ttid,error,code;
    struct rx_connection *aconn;

    code = 0;
    error = 0;
    ttid = 0;

    aconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    code = AFSVolTransCreate(aconn, volid, part, ITOffline, &ttid);
    if(code){
	fprintf(STDERR,"Could not start transaction on volume %u\n",volid);
	error = code;
	goto zfail;
    }
    code = AFSVolDeleteVolume(aconn, ttid);
    if(code){
	fprintf(STDERR,"Could not delete volume %u\n",volid);
	error = code;
	goto zfail;
    }
    code = AFSVolEndTrans(aconn, ttid, &rcode);
    ttid = 0;
    if(!code) code = rcode;
    if(code){
	fprintf(STDERR,"Could not end transaction on volume %u\n",volid);
	error = code;
	goto zfail;
    }
zfail:    
if(ttid){
    code = AFSVolEndTrans(aconn,ttid,&rcode);
    if(!code) code = rcode;
    if(!error) error = code;
}
PrintError("",error);
if(aconn) rx_DestroyConnection(aconn);
return error;
}

UV_SetVolume(server, partition, volid, transflag, setflag, sleeptime)
  afs_int32 server, partition, volid, transflag, setflag;
{
  struct rx_connection *conn = 0;
  afs_int32 tid=0;
  afs_int32 code, error=0, rcode;

  conn = UV_Bind(server, AFSCONF_VOLUMEPORT);
  if (!conn) {
     fprintf(STDERR, "SetVolumeStatus: Bind Failed");
     ERROR_EXIT(-1);
  }

  code = AFSVolTransCreate(conn, volid, partition, transflag, &tid);
  if (code) {
     fprintf(STDERR, "SetVolumeStatus: TransCreate Failed\n");
     ERROR_EXIT(code);
  }
  
  code = AFSVolSetFlags(conn, tid, setflag);
  if (code) {
     fprintf(STDERR, "SetVolumeStatus: SetFlags Failed\n");
     ERROR_EXIT(code);
  }
  
  if (sleeptime) {
#ifdef AFS_PTHREAD_ENV
     sleep(sleeptime);
#else
     IOMGR_Sleep(sleeptime);
#endif
  }

 error_exit:
  if (tid) {
     rcode = 0;
     code = AFSVolEndTrans(conn, tid, &code);
     if (code || rcode) {
        fprintf(STDERR, "SetVolumeStatus: EndTrans Failed\n");
	if (!error) error = (code ? code : rcode);
     }
  }

  if (conn) rx_DestroyConnection(conn);
  return(error);
}

/*maps the host addresses in <old > (present in network byte order) to
 that in< new> (present in host byte order )*/
MapNetworkToHost(old, new)
struct nvldbentry *old, *new;
{
    int i,count;

    /*copy all the fields */
    strcpy(new->name,old->name);
/*    new->volumeType = old->volumeType;*/
    new->nServers = old->nServers;
    count = old->nServers;
    if(count < NMAXNSERVERS) count++;
    for(i = 0; i < count; i++) {
	new->serverNumber[i] = ntohl(old->serverNumber[i]);
	new->serverPartition[i] = old->serverPartition[i];
	new->serverFlags[i] = old->serverFlags[i];
    }
    new->volumeId[RWVOL]= old->volumeId[RWVOL];
    new->volumeId[ROVOL] = old->volumeId[ROVOL];
    new->volumeId[BACKVOL] = old->volumeId[BACKVOL];
    new->cloneId = old->cloneId;
    new->flags = old->flags;

}

/*maps the host entries in <entry> which are present in host byte order to network byte order */
MapHostToNetwork(entry)
struct nvldbentry *entry;
{
    int i,count;
    
    count = entry->nServers;
    if(count < NMAXNSERVERS) count++;
    for(i = 0; i < count; i++) {
	entry->serverNumber[i] = htonl(entry->serverNumber[i]);
    }
}

