/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#include <afs/cmd.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#endif
#include <lwp.h>
#include <afs/bubasics.h>
#include "bc.h"
#include <afs/com_err.h>
#include <afs/butc.h>
#include <afs/budb.h>
#include <afs/vlserver.h>
#include "error_macros.h"

extern struct bc_dumpTask bc_dumpTasks[BC_MAXSIMDUMPS];
extern char *whoami;

#define	MAXTAPESATONCE	    10

#define HOSTADDR(sockaddr) (sockaddr)->sin_addr.s_addr

/* local structure to keep track of volumes and the dumps from which
 * they should be restored 
 */
struct dumpinfo {
    struct dumpinfo *next;
    struct volinfo *volinfolist;
    struct volinfo *lastinlist;
    afs_int32 DumpId;
    afs_int32 initialDumpId;
    afs_int32 parentDumpId;
    afs_int32 level;
};

struct volinfo {
    struct voli *next;
    char *volname;
    afs_int32 server;
    afs_int32 partition;
};

/* local structure used to keep track of a tape, including a list of
 * volumes to restore from that tape (bc_tapeItem)
 */
struct bc_tapeList {
    struct bc_tapeList *next;	/* next guy in list */
    char *tapeName;		/* name of the tape */
    afs_int32 dumpID;		/* dump located on this tape */
    afs_int32 initialDumpID;	/* initial dump located on this tape */
    afs_int32 tapeNumber;	/* which tape in the dump */
    struct bc_tapeItem *restoreList;	/* volumes to restore from this tape */
};

/* each tape has a list of volumes to restore; hangs off of a struct
   bc_tapeList.  Kept sorted so that we can read the tape once to do
   everything we need to do.  */
struct bc_tapeItem {
    struct bc_tapeItem *next;
    char *volumeName;		/* volume to restore */
    afs_int32 position;		/* file slot on this tape */
    afs_int32 oid;		/* id of the volume */
    afs_int32 dumplevel;	/* level # of the containing dump (0 == top level) */
    afs_int32 vollevel;		/* level # of the containing volume (0 == full dump) */
    afs_uint32 server;
    afs_int32 partition;
    int lastdump;		/* The last incremental to restore */
};

/* strip .backup from the end of a name */
static
StripBackup(aname)
     register char *aname;
{
    int j;

    if (j = BackupName(aname)) {
	aname[j] = 0;
	return 0;
    }
    return -1;
}

int
BackupName(aname)
     char *aname;
{
    int j;

    j = strlen(aname);
    if ((j > 7) && (strcmp(".backup", aname + j - 7) == 0))
	return (j - 7);
    if ((j > 9) && (strcmp(".readonly", aname + j - 9) == 0))
	return (j - 9);
    return 0;
}

extractTapeSeq(tapename)
     char *tapename;
{
    char *sptr;

    sptr = strrchr(tapename, '.');
    if (!sptr)
	return (-1);
    sptr++;
    return (atol(sptr));
}

void
viceName(value)
     long value;
{
    char *alph;
    int r;

    alph = "abcdefjhijklmnopqrstuvwxyz";

    if (value > 25)
	viceName(value / 26 - 1);

    r = value % 26;
    printf("%c", alph[r]);
}

/* bc_DoRestore
 * entry:
 *	aindex - index into bc_dumpTasks that describes this dump.
 */
bc_Restorer(aindex)
     afs_int32 aindex;
{
    struct bc_dumpTask *dumpTaskPtr;

    char vname[BU_MAXNAMELEN];
    struct budb_dumpEntry *dumpDescr, dumpDescr1, dumpDescr2;
    struct budb_volumeEntry *volumeEntries;
    struct bc_volumeDump *tvol;
    afs_int32 code = 0, tcode;
    afs_int32 tapedumpid, parent;

    afs_int32 nentries = 0;
    afs_int32 last, next, ve, vecount;
    struct bc_tapeItem *ti, *pti, *nti;
    struct bc_tapeList *tapeList = (struct bc_tapeList *)0;
    struct bc_tapeList *tle, *ptle, *ntle;
    afs_int32 tlid;
    afs_int32 tapeid, tid;

    struct tc_restoreArray rpcArray;	/* the rpc structure we use */
    struct tc_restoreDesc *tcarray = (struct tc_restoreDesc *)0;

    afs_int32 i, startentry, todump;
    afs_int32 port, nextport;
    afs_int32 level, tapeseq;
    afs_int32 foundvolume, voldumplevel;
    struct rx_connection *aconn = (struct rx_connection *)0;
    statusP statusPtr, newStatusPtr;

    struct dumpinfo *dumpinfolist = NULL;
    struct dumpinfo *pdi, *ndi, *di, *dlevels;
    struct volinfo *pvi, *nvi, *vi;
    afs_int32 lvl, lv;
    int num_dlevels = 20;

    afs_int32 serverAll;	/* The server to which all volumes are to be restore to */
    afs_int32 partitionAll;	/* Likewise for partition */
    struct hostent *hostPtr;
    long haddr;
    time_t did;
    int foundtape, c;

    extern statusP createStatusNode();
    extern statusP findStatus();

    dlevels = (struct dumpinfo *) malloc(num_dlevels * sizeof(*dlevels));

    dumpTaskPtr = &bc_dumpTasks[aindex];
    serverAll = HOSTADDR(&dumpTaskPtr->destServer);
    partitionAll = dumpTaskPtr->destPartition;

    volumeEntries = (struct budb_volumeEntry *)
	malloc(MAXTAPESATONCE * sizeof(struct budb_volumeEntry));
    if (!volumeEntries) {
	afs_com_err(whoami, BC_NOMEM, "");
	ERROR(BC_NOMEM);
    }

    /* For each volume to restore, find which dump it's most recent full or 
     * incremental is on and thread onto our dump list (from oldest to newest
     * dump). Also hang the volume off of the dump (no particular order).
     */
    for (tvol = dumpTaskPtr->volumes; tvol; tvol = tvol->next) {	/*tvol */
	strcpy(vname, tvol->name);
	dumpDescr = &dumpDescr1;
	if (dumpTaskPtr->parentDumpID > 0) /* Told which dump to try */
	  {
	    /* Right now, this assumes that all volumes listed will be
	     * from the given dumpID.  FIXME
	     */
	    code = bcdb_FindDumpByID(dumpTaskPtr->parentDumpID, dumpDescr);
	    if (code)
	      {
		afs_com_err(whoami, code, "Couldn't look up info for dump %d\n",
			dumpTaskPtr->parentDumpID);
		continue;
	      }
	    code = bcdb_FindVolumes(dumpTaskPtr->parentDumpID, vname, volumeEntries,
				    last, &next, MAXTAPESATONCE, &vecount);
	    if (code)
	      {
		if (!BackupName(vname))
		  {
		    strcat(vname, ".backup");
		    code = bcdb_FindVolumes(dumpTaskPtr->parentDumpID, vname, volumeEntries,
					    last, &next, MAXTAPESATONCE, &vecount);
		  }
	      }
	  }
	else
	  {
	    code = bcdb_FindDump(vname, dumpTaskPtr->fromDate, dumpDescr);
	    if (!BackupName(vname)) {	/* See if backup volume is there */
	      strcat(vname, ".backup");
	      dumpDescr = &dumpDescr2;
	      tcode = code;
	      code = bcdb_FindDump(vname, dumpTaskPtr->fromDate, dumpDescr);

	      if (code) {	/* Can't find backup, go with first results */
		strcpy(vname, tvol->name);
		dumpDescr = &dumpDescr1;
		code = tcode;
	      } else if (!tcode) {	/* Both found an entry, go with latest result */
		if (dumpDescr1.created > dumpDescr2.created) {
		  strcpy(vname, tvol->name);
		  dumpDescr = &dumpDescr1;
		  code = tcode;
		}
	      }
	    }
	  }

	if (code) {		/* If FindDump took an error */
	    afs_com_err(whoami, code, "; Can't find any dump for volume %s",
		    tvol->name);
	    continue;
	}

	/* look to see if this dump has already been found */
	for (pdi = 0, di = dumpinfolist; di; pdi = di, di = di->next) {
	    if (di->DumpId < dumpDescr->id) {
		di = 0;
		break;
	    } else if (di->DumpId == dumpDescr->id) {
		break;
	    }
	}

	/* If didn't find it, create one and thread into list */
	if (!di) {
	    di = (struct dumpinfo *)malloc(sizeof(struct dumpinfo));
	    if (!di) {
		afs_com_err(whoami, BC_NOMEM, "");
		ERROR(BC_NOMEM);
	    }
	    memset(di, 0, sizeof(struct dumpinfo));

	    di->DumpId = dumpDescr->id;
	    di->initialDumpId = dumpDescr->initialDumpID;
	    di->parentDumpId = dumpDescr->parent;
	    di->level = dumpDescr->level;

	    if (!pdi) {
		di->next = dumpinfolist;
		dumpinfolist = di;
	    } else {
		di->next = pdi->next;
		pdi->next = di;
	    }
	}

	/* Create one and thread into list */
	vi = (struct volinfo *)malloc(sizeof(struct volinfo));
	if (!vi) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    ERROR(BC_NOMEM);
	}
	memset(vi, 0, sizeof(struct volinfo));

	vi->volname = (char *)malloc(strlen(vname) + 1);
	if (!vi->volname) {
	    free(vi);
	    afs_com_err(whoami, BC_NOMEM, "");
	    ERROR(BC_NOMEM);
	}

	strcpy(vi->volname, vname);
	if (serverAll) {
	    vi->server = serverAll;
	    vi->partition = partitionAll;
	} else {
	    vi->server = HOSTADDR(&tvol->server);
	    vi->partition = tvol->partition;
	}

	/* thread onto end of list */
	if (!di->lastinlist)
	    di->volinfolist = vi;
	else
	    di->lastinlist->next = vi;
	di->lastinlist = vi;
    }				/*tvol */

    /* For each of the above dumps we found (they could be increments), find 
     * the dump's lineage (up to the full dump). 
     */
    for (di = dumpinfolist; di; di = di->next) {
	/* Find each of the parent dumps */
	memcpy(&dlevels[0], di, sizeof(struct dumpinfo));
	for (lvl = 1, parent = dlevels[0].parentDumpId; parent;
	     parent = dlevels[lvl].parentDumpId, lvl++) {
	    if (lvl >= num_dlevels) {		/* running out of dump levels */
		struct dumpinfo *tdl = dlevels;

		num_dlevels += num_dlevels;	/* double */
		dlevels = (struct dumpinfo *) malloc(num_dlevels * sizeof(*dlevels));
		memcpy(dlevels, tdl, (num_dlevels/2) * sizeof(*dlevels));
		free(tdl);
	    }
	    code = bcdb_FindDumpByID(parent, &dumpDescr1);
	    if (code) {
		for (vi = di->volinfolist; vi; vi = vi->next) {
		    afs_com_err(whoami, code,
			    "; Can't find parent DumpID %u for volume %s",
			    parent, vi->volname);
		}
		break;
	    }

	    dlevels[lvl].DumpId = dumpDescr1.id;
	    dlevels[lvl].initialDumpId = dumpDescr1.initialDumpID;
	    dlevels[lvl].parentDumpId = dumpDescr1.parent;
	    dlevels[lvl].level = dumpDescr1.level;
	}

	/* For each of the volumes that has a dump in this lineage (vi),
	 * find where it is in each dump level (lv) starting at level 0 and
	 * going to higest. Each dump level could contain one or more
	 * fragments (vecount) of the volume (volume fragments span tapes). 
	 * Each volume fragment is sorted by tapeid, tape sequence, and tape 
	 * position.
	 */
	for (vi = di->volinfolist; vi; vi = vi->next) {
	    tle = tapeList;	/* Use for searching the tapelist */
	    ptle = 0;		/* The previous tape list entry */
	    tlid = 0;		/* tapeid of first entry */
	    /* Volume's dump-level. May not be same as dump's dump-level. This
	     * value gets incremented everytime the volume is found on a dump.
	     */
	    voldumplevel = 0;

	    /* Do from level 0 dump to highest level dump */
	    for (lv = (lvl - 1); lv >= 0; lv--) {
		foundvolume = 0;	/* If found volume on this dump */
		tapeid =
		    (dlevels[lv].initialDumpId ? dlevels[lv].
		     initialDumpId : dlevels[lv].DumpId);

		/* Cycle through the volume fragments for this volume on the tape */
		for (last = 0, c = 0; last >= 0; last = next, c++) {
		    code =
			bcdb_FindVolumes(dlevels[lv].DumpId, vi->volname,
					 volumeEntries, last, &next,
					 MAXTAPESATONCE, &vecount);
		    if (code) {
			/* Volume wasn't found on the tape - so continue.
			 * This can happen when volume doesn't exist during level 0 dump
			 * but exists for incremental dump.
			 */
			if (code == BUDB_ENDOFLIST) {
			    code = 0;
			    break;
			}

			afs_com_err(whoami, code,
				"; Can't find volume %s in DumpID %u",
				vi->volname, dlevels[lv].DumpId);
			ERROR(code);
		    }

		    /* If we have made the Findvolumes call more than once for the same
		     * volume, then begin the search at the top. This sort assumes that
		     * we are collecting information from lowest level dump (full dump)
		     * to the highest level dump (highest incremental)
		     * and from first fragment to last fragment at each level. If
		     * there is more than one call to FindVolumes, this is not true
		     * anymore.
		     */
		    if (c) {
			tle = tapeList;
			ptle = 0;
			tlid = 0;
		    }

		    /* For each of the volume fragments that we read, sort them into
		     * the list of tapes they are on. Sort by tapeid, then tape sequence.
		     * Do from first fragment (where volume begins) to last fragment.
		     */
		    for (ve = (vecount - 1); ve >= 0; ve--) {
			foundvolume = 1;	/* Found the volume on this dump */
			foundtape = 0;
			tapeseq = volumeEntries[ve].tapeSeq;

			/* Determine where in the list of tapes this should go */
			for (; tle; ptle = tle, tle = tle->next) {
			    tid =
				(tle->initialDumpID ? tle->
				 initialDumpID : tle->dumpID);

			    /* Sort by tapeids. BUT, we don't want add an entry in the middle 
			     * of a dumpset (might split a volume fragmented across tapes). 
			     * So make sure we step to next dumpset. tlid is the tapeid of 
			     * the last tape we added a volume to. This can happen when an
			     * incremental was appended to a tape created prior its parent-
			     * dump's tape, and needs to be restored after it.
			     */
			    if (tapeid < tid) {
				if (tlid != tid) {
				    tle = 0;
				    break;
				}	/* Allocate and insert a tape entry */
			    }

			    /* Found the tapeid (the dumpset). Check if its the correct 
			     * tape sequence
			     */
			    else if (tapeid == tid) {
				if (tapeseq < tle->tapeNumber) {
				    tle = 0;
				    break;
				}
				/* Allocate and insert a tape entry */
				if (tapeseq == tle->tapeNumber) {
				    break;
				}
				/* Don't allocate tape entry */
				foundtape = 1;	/* Found dumpset but not the tape */
			    }

			    /* Prevously found the tapeid (the dumpset) but this tape 
			     * sequence not included (the tapeid has changed). So add 
			     * this tape entry to the end of its dumpset.
			     */
			    else if (foundtape) {
				tle = 0;
				break;
			    }
			    /* Allocate and insert a tape entry */
			}

			/* Allocate a new tapelist entry if one not found */
			if (!tle) {
			    tle = (struct bc_tapeList *)
				malloc(sizeof(struct bc_tapeList));
			    if (!tle) {
				afs_com_err(whoami, BC_NOMEM, "");
				return (BC_NOMEM);
			    }
			    memset(tle, 0, sizeof(struct bc_tapeList));

			    tle->tapeName =
				(char *)malloc(strlen(volumeEntries[ve].tape)
					       + 1);
			    if (!tle->tapeName) {
				free(tle);
				afs_com_err(whoami, BC_NOMEM, "");
				return (BC_NOMEM);
			    }

			    strcpy(tle->tapeName, volumeEntries[ve].tape);
			    tle->dumpID = dlevels[lv].DumpId;
			    tle->initialDumpID = dlevels[lv].initialDumpId;
			    tle->tapeNumber = tapeseq;

			    /* Now thread the tape onto the list */
			    if (!ptle) {
				tle->next = tapeList;
				tapeList = tle;
			    } else {
				tle->next = ptle->next;
				ptle->next = tle;
			    }
			}
			tlid =
			    (tle->initialDumpID ? tle->initialDumpID : tle->
			     dumpID);

			/* Now place the volume fragment into the correct position on 
			 * this tapelist entry. Duplicate entries are ignored.
			 */
			for (pti = 0, ti = tle->restoreList; ti;
			     pti = ti, ti = ti->next) {
			    if (volumeEntries[ve].position < ti->position) {
				ti = 0;
				break;
			    }
			    /* Allocate an entry */
			    else if (volumeEntries[ve].position ==
				     ti->position) {
				break;
			    }	/* Duplicate entry */
			}

			/* If didn't find one, allocate and thread onto list.
			 * Remember the server and partition.
			 */
			if (!ti) {
			    ti = (struct bc_tapeItem *)
				malloc(sizeof(struct bc_tapeItem));
			    if (!ti) {
				afs_com_err(whoami, BC_NOMEM, "");
				return (BC_NOMEM);
			    }
			    memset(ti, 0, sizeof(struct bc_tapeItem));

			    ti->volumeName =
				(char *)malloc(strlen(volumeEntries[ve].name)
					       + 1);
			    if (!ti->volumeName) {
				free(ti);
				afs_com_err(whoami, BC_NOMEM, "");
				return (BC_NOMEM);
			    }

			    strcpy(ti->volumeName, volumeEntries[ve].name);
			    ti->server = vi->server;
			    ti->partition = vi->partition;
			    ti->oid = volumeEntries[ve].id;
			    ti->position = volumeEntries[ve].position;
			    ti->vollevel = voldumplevel;
			    ti->dumplevel = dlevels[lv].level;
			    ti->lastdump = ((lv == 0) ? 1 : 0);

			    if (!pti) {
				ti->next = tle->restoreList;
				tle->restoreList = ti;
			    } else {
				ti->next = pti->next;
				pti->next = ti;
			    }

			    nentries++;
			}	/* !ti */
		    }		/* ve: for each returned volume fragment by bcdb_FindVolumes() */
		}		/* last: Multiple calls to bcdb_FindVolumes() */

		if (foundvolume)
		    voldumplevel++;
	    }			/* lv: For each dump level */
	}			/* vi: For each volume to search for in the dump hierarchy */
    }				/* di: For each dump */

    if (!nentries) {
	afs_com_err(whoami, 0, "No volumes to restore");
	ERROR(0);
    }

    if (dumpTaskPtr->dontExecute) {
	for (tle = tapeList; tle; tle = tle->next) {
	    for (ti = tle->restoreList; ti; ti = ti->next) {
		tapedumpid =
		    (tle->initialDumpID ? tle->initialDumpID : tle->dumpID);
		strcpy(vname, ti->volumeName);
		StripBackup(vname);
		if (dumpTaskPtr->newExt)
		    strcat(vname, dumpTaskPtr->newExt);

		/* print volume to restore and the tape and position its on */
		if (serverAll) {
		    printf
			("Restore volume %s (ID %d) from tape %s (%u), position %d",
			 ti->volumeName, ti->oid, tle->tapeName, tapedumpid,
			 ti->position);

		    if (strcmp(ti->volumeName, vname) != 0)
			printf(" as %s", vname);

		    printf(".\n");
		}

		/* print in format recognized by volsetrestore */
		else {
		    haddr = ti->server;
		    hostPtr =
			gethostbyaddr((char *)&haddr, sizeof(haddr), AF_INET);
		    if (hostPtr)
			printf("%s", hostPtr->h_name);
		    else
			printf("%u", ti->server);

		    printf(" /vicep");
		    viceName(ti->partition);

		    printf(" %s # as %s; %s (%u); pos %d;", ti->volumeName,
			   vname, tle->tapeName, tapedumpid, ti->position);

		    did = tle->dumpID;
		    printf(" %s", ctime(&did));
		}
	    }
	}

	ERROR(0);
    }

    /* Allocate a list of volumes to restore */
    tcarray =
	(struct tc_restoreDesc *)malloc(nentries *
					sizeof(struct tc_restoreDesc));
    if (!tcarray) {
	afs_com_err(whoami, BC_NOMEM, "");
	ERROR(BC_NOMEM);
    }
    memset(tcarray, 0, nentries * sizeof(struct tc_restoreDesc));

    /* Fill in the array with the list above */
    i = 0;
    for (tle = tapeList; tle; tle = tle->next) {
	for (ti = tle->restoreList; ti; ti = ti->next) {
	    tcarray[i].origVid = ti->oid;	/* means unknown */
	    tcarray[i].vid = 0;
	    tcarray[i].flags = 0;
	    if (ti->vollevel == 0)
		tcarray[i].flags |= RDFLAG_FIRSTDUMP;
	    if (ti->lastdump)
		tcarray[i].flags |= RDFLAG_LASTDUMP;
	    tcarray[i].position = ti->position;
	    tcarray[i].dbDumpId = tle->dumpID;
	    tcarray[i].initialDumpId = tle->initialDumpID;
	    tcarray[i].hostAddr = ti->server;	/* just the internet address */
	    tcarray[i].partition = ti->partition;

	    strcpy(tcarray[i].tapeName, tle->tapeName);
	    strcpy(tcarray[i].oldName, ti->volumeName);
	    strcpy(tcarray[i].newName, ti->volumeName);
	    StripBackup(tcarray[i].newName);
	    if (dumpTaskPtr->newExt)
		strcat(tcarray[i].newName, dumpTaskPtr->newExt);

	    tcarray[i].dumpLevel = ti->dumplevel;
	    i++;
	}
    }

    printf("Starting restore\n");

    /* Loop until all of the tape entries are used */
    for (startentry = 0; startentry < nentries; startentry += todump) {
	/* Get all the next tape entries with the same port offset */
	if (dumpTaskPtr->portCount == 0) {
	    port = 0;
	    todump = nentries - startentry;
	} else if (dumpTaskPtr->portCount == 1) {
	    port = dumpTaskPtr->portOffset[0];
	    todump = nentries - startentry;
	} else {
	    level = tcarray[startentry].dumpLevel;
	    port =
		dumpTaskPtr->
		portOffset[(level <=
			    dumpTaskPtr->portCount -
			    1 ? level : dumpTaskPtr->portCount - 1)];

	    for (todump = 1; ((startentry + todump) < nentries); todump++) {
		level = tcarray[startentry + todump].dumpLevel;
		nextport =
		    dumpTaskPtr->
		    portOffset[(level <=
				dumpTaskPtr->portCount -
				1 ? level : dumpTaskPtr->portCount - 1)];
		if (port != nextport)
		    break;	/* break if we change ports */
	    }
	}

	rpcArray.tc_restoreArray_len = todump;
	rpcArray.tc_restoreArray_val = &tcarray[startentry];

	code = ConnectButc(dumpTaskPtr->config, port, &aconn);
	if (code)
	    return (code);

	if (tcarray[startentry].dumpLevel == 0)
	    printf("\nFull restore being processed on port %d\n", port);
	else
	    printf("\nIncremental restore being processed on port %d\n",
		   port);

	code =
	    TC_PerformRestore(aconn, "DumpSetName", &rpcArray,
			      &dumpTaskPtr->dumpID);
	if (code) {
	    afs_com_err(whoami, code, "; Failed to start restore");
	    break;
	}

	/* setup status monitor node */
	statusPtr = createStatusNode();
	lock_Status();
	statusPtr->port = port;
	statusPtr->taskId = dumpTaskPtr->dumpID;
	statusPtr->jobNumber = bc_jobNumber();
	statusPtr->flags &= ~STARTING;	/* clearStatus to examine */
	if (tcarray[startentry].dumpLevel == 0)
	    sprintf(statusPtr->taskName, "Full Restore");
	else
	    sprintf(statusPtr->taskName, "Incremental Restore");
	unlock_Status();

	/* Wait until this restore pass is complete before starting the next
	 * pass. Query the statusWatcher for the status
	 */
	while (1) {
	    lock_Status();
	    newStatusPtr = findStatus(dumpTaskPtr->dumpID);
	    if (!newStatusPtr
		|| (newStatusPtr->
		    flags & (ABORT_REQUEST | ABORT_SENT | ABORT_DONE |
			     TASK_ERROR))) {
		unlock_Status();
		ERROR(0);
	    } else if (newStatusPtr->flags & (TASK_DONE)) {
		unlock_Status();
		break;
	    }

	    unlock_Status();
	    IOMGR_Sleep(2);
	}

	if (aconn)
	    rx_DestroyConnection(aconn);
	aconn = (struct rx_connection *)0;
    }				/* while */

    /* free up everything */
  error_exit:
    if (aconn)
	rx_DestroyConnection(aconn);

    if (tcarray)
	free(tcarray);

    /* Free the dumpinfo list and its volumes */
    for (di = dumpinfolist; di; di = ndi) {
	for (vi = di->volinfolist; vi; vi = nvi) {
	    if (vi->volname)
		free(vi->volname);
	    nvi = vi->next;
	    free(vi);
	}
	ndi = di->next;
	free(di);
    }

    /* Free the tape lists and items */
    for (tle = tapeList; tle; tle = ntle) {
	for (ti = tle->restoreList; ti; ti = nti) {
	    nti = ti->next;
	    if (ti->volumeName)
		free(ti->volumeName);
	    free(ti);
	}
	ntle = tle->next;
	if (tle->tapeName)
	    free(tle->tapeName);
	free(tle);
    }

    /* free local-like things we alloacted to save stack space */
    if (volumeEntries)
	free(volumeEntries);

    free(dlevels);

    return code;
}
