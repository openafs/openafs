/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#if defined(AFS_LINUX24_ENV)
#define _REGEX_RE_COMP
#endif
#include <sys/types.h>
#if defined(AFS_LINUX24_ENV)
#include <regex.h>
#endif
#include <afs/cmd.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <errno.h>
#include <afs/com_err.h>
#include <afs/budb.h>
#include <afs/butc.h>
#include <afs/bubasics.h>	/* PA */
#include <afs/volser.h>
#include <afs/voldefs.h>	/* PA */
#include <afs/vldbint.h>	/* PA */
#include <afs/ktime.h>		/* PA */
#include <time.h>

#include <lock.h>
#include <afs/butc.h>
#include <afs/tcdata.h>
#include <afs/butx.h>
#include "bc.h"
#include "error_macros.h"


extern struct bc_config *bc_globalConfig;
extern struct bc_dumpSchedule *bc_FindDumpSchedule();
extern struct bc_volumeSet *bc_FindVolumeSet(struct bc_config *cfg,
					     char *name);
extern struct bc_dumpTask bc_dumpTasks[BC_MAXSIMDUMPS];
extern struct ubik_client *cstruct;
extern int bc_Dumper();		/* function to do dumps */
extern int bc_Restorer();	/* function to do restores */
extern char *whoami;
extern struct ktc_token ttoken;
extern char *tailCompPtr();
extern statusP createStatusNode();

char *loadFile;
extern afs_int32 lastTaskCode;

#define HOSTADDR(sockaddr) (sockaddr)->sin_addr.s_addr

int
bc_EvalVolumeSet(aconfig, avs, avols, uclient)
     struct bc_config *aconfig;
     register struct bc_volumeSet *avs;
     struct bc_volumeDump **avols;
     struct ubik_client *uclient;
{				/*bc_EvalVolumeSet */
    int code;
    static afs_int32 use = 2;

    if (use == 2) {		/* Use EvalVolumeSet2() */
	code = EvalVolumeSet2(aconfig, avs, avols, uclient);
	if (code == RXGEN_OPCODE)
	    use = 1;
    }
    if (use == 1) {		/* Use EvalVolumeSet1() */
	code = EvalVolumeSet1(aconfig, avs, avols, uclient);
    }
    return code;
}				/*bc_EvalVolumeSet */

struct partitionsort {
    afs_int32 part;
    struct bc_volumeDump *vdlist;
    struct bc_volumeDump *lastvdlist;
    struct bc_volumeDump *dupvdlist;
    struct bc_volumeEntry *vole;
    struct partitionsort *next;
};
struct serversort {
    afs_uint32 ipaddr;
    struct partitionsort *partitions;
    struct serversort *next;
};

afs_int32
getSPEntries(server, partition, serverlist, ss, ps)
     afs_uint32 server;
     afs_int32 partition;
     struct serversort **serverlist, **ss;
     struct partitionsort **ps;
{
    if (!(*ss) || ((*ss)->ipaddr != server)) {
	*ps = 0;
	for ((*ss) = *serverlist; (*ss); *ss = (*ss)->next) {
	    if ((*ss)->ipaddr == server)
		break;
	}
    }
    /* No server entry added. Add one */
    if (!(*ss)) {
	*ss = (struct serversort *)malloc(sizeof(struct serversort));
	if (!(*ss)) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    *ss = 0;
	    return (BC_NOMEM);
	}
	memset(*ss, 0, sizeof(struct serversort));
	(*ss)->ipaddr = server;
	(*ss)->next = *serverlist;
	*serverlist = *ss;
    }


    if (!(*ps) || ((*ps)->part != partition)) {
	for (*ps = (*ss)->partitions; *ps; *ps = (*ps)->next) {
	    if ((*ps)->part == partition)
		break;
	}
    }
    /* No partition entry added. Add one */
    if (!(*ps)) {
	*ps = (struct partitionsort *)malloc(sizeof(struct partitionsort));
	if (!(*ps)) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    free(*ss);
	    *ps = 0;
	    *ss = 0;
	    return (BC_NOMEM);
	}
	memset(*ps, 0, sizeof(struct partitionsort));
	(*ps)->part = partition;
	(*ps)->next = (*ss)->partitions;
	(*ss)->partitions = *ps;
    }
    return 0;
}

afs_int32
randSPEntries(serverlist, avols)
     struct serversort *serverlist;
     struct bc_volumeDump **avols;
{
    struct serversort *ss, **pss;
    struct partitionsort *ps, **pps;
    afs_int32 r;
    afs_int32 scount, pcount;

    *avols = 0;

    /* Seed random number generator */
    r = time(0) + getpid();
    srand(r);

    /* Count number of servers, remove one at a time */
    for (scount = 0, ss = serverlist; ss; ss = ss->next, scount++);
    for (; scount; scount--) {
	/* Pick a random server in list and remove it */
	r = (rand() >> 4) % scount;
	for (pss = &serverlist, ss = serverlist; r;
	     pss = &ss->next, ss = ss->next, r--);
	*pss = ss->next;

	/* Count number of partitions, remove one at a time */
	for (pcount = 0, ps = ss->partitions; ps; ps = ps->next, pcount++);
	for (; pcount; pcount--) {
	    /* Pick a random parition in list and remove it */
	    r = (rand() >> 4) % pcount;
	    for (pps = &ss->partitions, ps = ss->partitions; r;
		 pps = &ps->next, ps = ps->next, r--);
	    *pps = ps->next;

	    ps->lastvdlist->next = *avols;
	    *avols = ps->vdlist;
	    free(ps);
	}
	free(ss);
    }
    return 0;
}

int
EvalVolumeSet2(aconfig, avs, avols, uclient)
     struct bc_config *aconfig;
     struct bc_volumeSet *avs;
     struct bc_volumeDump **avols;
     struct ubik_client *uclient;
{				/*EvalVolumeSet2 */
    struct bc_volumeEntry *tve;
    struct bc_volumeDump *tavols;
    struct VldbListByAttributes attributes;
    afs_int32 si, nsi;		/* startIndex and nextStartIndex */
    afs_int32 nentries, e, ei, et, add, l;
    nbulkentries bulkentries;
    struct nvldbentry *entries = 0;
    struct bc_volumeDump *tvd;
    afs_int32 code = 0, tcode;
    afs_int32 count = 0;
    struct serversort *servers = 0, *lastserver = 0, *ss = 0;
    struct partitionsort *ps = 0;

    *avols = (struct bc_volumeDump *)0;
    bulkentries.nbulkentries_len = 0;
    bulkentries.nbulkentries_val = 0;

    /* For each of the volume set entries - collect the volumes that match it */
    for (tve = avs->ventries; tve; tve = tve->next) {
	/* Put together a call to the vlserver for this vlentry. The 
	 * performance gain is from letting the vlserver expand the
	 * volumeset and not this routine.
	 */
	attributes.Mask = 0;
	if (tve->server.sin_addr.s_addr) {	/* The server */
	    attributes.Mask |= VLLIST_SERVER;
	    attributes.server = tve->server.sin_addr.s_addr;
	}
	if (tve->partition != -1) {	/* The partition */
	    attributes.Mask |= VLLIST_PARTITION;
	    attributes.partition = tve->partition;
	}

	/* Now make the call to the vlserver */
	for (si = 0; si != -1; si = nsi) {
	    nentries = 0;
	    bulkentries.nbulkentries_len = 0;
	    bulkentries.nbulkentries_val = 0;
	    nsi = -1;
	    tcode =
		ubik_Call(VL_ListAttributesN2, uclient, 0, &attributes,
			  tve->name, si, &nentries, &bulkentries, &nsi);
	    if (tcode)
		ERROR(tcode);

	    /* The 3.4 vlserver has a VL_ListAttributesN2() RPC call, but
	     * it is not complete. The only way to tell if it is not complete
	     * is if et == 0 (which we check for below). Also, if the call didn't
	     * match any entries, then we don't know what version the vlserver
	     * is. In both cases, we return RXGEN_OPCODE and the calling routine
	     * will switch to the EvalVolumeSet1() call.
	     */
	    if (nentries == 0)
		ERROR(RXGEN_OPCODE);	/* Use EvalVolumeSet1 */

	    /* Step through each entry and add it to the list of volumes */
	    entries = bulkentries.nbulkentries_val;
	    for (e = 0; e < nentries; e++) {
		ei = entries[e].matchindex & 0xffff;
		et = (entries[e].matchindex >> 16) & 0xffff;
		switch (et) {
		case ITSRWVOL:{
			et = RWVOL;
			break;
		    }
		case ITSBACKVOL:{
			et = BACKVOL;
			break;
		    }
		case ITSROVOL:{
			et = ROVOL;
			break;
		    }
		default:
		    ERROR(RXGEN_OPCODE);	/* Use EvalVolumeSet1 */
		}

		/* Find server and partiton structure to hang the entry off of */
		tcode =
		    getSPEntries(entries[e].serverNumber[ei],
				 entries[e].serverPartition[ei], &servers,
				 &ss, &ps);
		if (tcode) {
		    afs_com_err(whoami, tcode, "");
		    ERROR(tcode);
		}

		/* Detect if this entry should be added (not a duplicate).
		 * Use ps->dupvdlist and ps->vole to only search volumes from
		 * previous volume set entries.
		 */
		add = 1;
		if (tve != avs->ventries) {
		    l = strlen(entries[e].name);
		    if (ps->vole != tve) {
			ps->vole = tve;
			ps->dupvdlist = ps->vdlist;
		    }
		    for (tavols = ps->dupvdlist; add && tavols;
			 tavols = tavols->next) {
			if (strncmp(tavols->name, entries[e].name, l) == 0) {
			    if ((strcmp(&entries[e].name[l], ".backup") == 0)
				|| (strcmp(&entries[e].name[l], ".readonly")
				    == 0)
				|| (strcmp(&entries[e].name[l], "") == 0))
				add = 0;
			}
		    }
		}

		if (add) {
		    /* Allocate a volume dump structure and its name */
		    tvd = (struct bc_volumeDump *)
			malloc(sizeof(struct bc_volumeDump));
		    if (!tvd) {
			afs_com_err(whoami, BC_NOMEM, "");
			ERROR(BC_NOMEM);
		    }
		    memset(tvd, 0, sizeof(*tvd));

		    tvd->name = (char *)malloc(strlen(entries[e].name) + 10);
		    if (!(tvd->name)) {
			afs_com_err(whoami, BC_NOMEM, "");
			free(tvd);
			ERROR(BC_NOMEM);
		    }

		    /* Fill it in and thread onto avols list */
		    strcpy(tvd->name, entries[e].name);
		    if (et == BACKVOL)
			strcat(tvd->name, ".backup");
		    else if (et == ROVOL)
			strcat(tvd->name, ".readonly");
		    tvd->vid = entries[e].volumeId[et];
		    tvd->entry = tve;
		    tvd->volType = et;
		    tvd->partition = entries[e].serverPartition[ei];
		    tvd->server.sin_addr.s_addr = entries[e].serverNumber[ei];
		    tvd->server.sin_port = 0;	/* default FS port */
		    tvd->server.sin_family = AF_INET;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
		    tvd->server.sin_len = sizeof(struct sockaddr_in);
#endif

		    /* String tvd off of partition struct */
		    tvd->next = ps->vdlist;
		    ps->vdlist = tvd;
		    if (!tvd->next)
			ps->lastvdlist = tvd;

		    count++;
		}
	    }

	    /* Free memory allocated during VL call */
	    if (bulkentries.nbulkentries_val) {
		free((char *)bulkentries.nbulkentries_val);
		bulkentries.nbulkentries_val = 0;
		entries = 0;
	    }
	}
    }

    /* Randomly link the volumedump entries together */
    randSPEntries(servers, avols);
    fprintf(stderr, "Total number of volumes : %u\n", count);

  error_exit:
    if (bulkentries.nbulkentries_val) {
	free((char *)bulkentries.nbulkentries_val);
    }
    return (code);
}				/*EvalVolumeSet2 */

/*-----------------------------------------------------------------------------
 * EvalVolumeSetOld
 *
 * Description:
 *	Takes the entries in a volumeset and expands them into a list of 
 *      volumes. Every VLDB volume entry is looked at and compared to the
 *      volumeset entries.
 *
 *      When matching a VLDB volume entry to a volumeset entry, 
 *       1. If the RW volume entry matches, that RW volume is used.
 *       2. Otherwise, if the BK volume entry matches, the BK volume is used.
 *       3. Finally, if the RO volume entry matches, the RO volume is used.
 *      For instance: A volumeset entry of ".* .* user.t.*" will match volume 
 *                    "user.troy" and "user.troy.backup". The rules will use 
 *                    the RW volume "user.troy".
 *
 *      When a VLDB volume entry matches a volumeset entry (be it RW, BK or RO),
 *      that volume is used and matches against any remaining volumeset entries 
 *      are not even done.
 *      For instance: A 1st volumeset entry ".* .* .*.backup" will match with
 *                    "user.troy.backup". Its 2nd volumeset entry ".* .* .*" 
 *                    would have matched its RW volume "user.troy", but the first 
 *                    match is used and the second match isn't even done.
 *
 * Arguments:
 *	aconfig	: Global configuration info.
 *	avs	: 
 *	avols	: Ptr to linked list of entries describing volumes to dump.
 *	uclient	: Ptr to Ubik client structure.
 *
 * Returns:
 *	0 on successful volume set evaluation,
 *	Lower-level codes otherwise.
 *
 * Environment:
 *	Expand only the single volume set provided (avs); don't go down its chain.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------------
 */
int
EvalVolumeSet1(aconfig, avs, avols, uclient)
     struct bc_config *aconfig;
     struct bc_volumeSet *avs;
     struct bc_volumeDump **avols;
     struct ubik_client *uclient;
{				/*EvalVolumeSet1 */
    afs_int32 code;		/*Result of various calls */
    char *errm;
    struct bc_volumeDump *tvd;	/*Ptr to new dump instance */
    struct bc_volumeEntry *tve, *ctve;	/*Ptr to new volume entry instance */
    char patt[256];		/*Composite regex; also, target string */
    int volType;		/*Type of volume that worked */
    afs_int32 index;		/*Current VLDB entry index */
    afs_int32 count;		/*Needed by VL_ListEntry() */
    afs_int32 next_index;	/*Next index to list */
    struct vldbentry entry;	/*VLDB entry */
    int srvpartpair;		/*Loop counter: server/partition pair */
    afs_int32 total = 0;
    int found, foundentry;
    struct serversort *servers = 0, *ss = 0;
    struct partitionsort *ps = 0;

    *avols = (struct bc_volumeDump *)0;
    ctve = (struct bc_volumeEntry *)0;	/* no compiled entry */

    /* For each vldb entry.
     * Variable next_index is set to the index of the next VLDB entry
     * in the enumeration.
     */
    for (index = 0; 1; index = next_index) {	/*w */
	memset(&entry, 0, sizeof(entry));
	code = ubik_Call(VL_ListEntry,	/*Routine to invoke */
			 uclient,	/*Ubik client structure */
			 0,	/*Ubik flags */
			 index,	/*Current index */
			 &count,	/*Ptr to working variable */
			 &next_index,	/*Ptr to next index value to list */
			 &entry);	/*Ptr to entry to fill */
	if (code)
	    return code;
	if (!next_index)
	    break;		/* If the next index is invalid, bail out now. */

	/* For each entry in the volume set */
	found = 0;		/* No match in volume set yet */
	for (tve = avs->ventries; tve; tve = tve->next) {	/*ve */
	    /* for each server in the vldb entry */
	    for (srvpartpair = 0; srvpartpair < entry.nServers; srvpartpair++) {	/*s */
		/* On the same server */
		if (tve->server.sin_addr.s_addr
		    && !VLDB_IsSameAddrs(tve->server.sin_addr.s_addr,
					 entry.serverNumber[srvpartpair],
					 &code)) {
		    if (code)
			return (code);
		    continue;
		}


		/* On the same partition */
		if ((tve->partition != -1)
		    && (tve->partition != entry.serverPartition[srvpartpair]))
		    continue;

		/* If the volume entry is not compiled, then compile it */
		if (ctve != tve) {
		    sprintf(patt, "^%s$", tve->name);
		    errm = (char *)re_comp(patt);
		    if (errm) {
			afs_com_err(whoami, 0,
				"Can't compile regular expression '%s': %s",
				patt, errm);
			return (-1);
		    }
		    ctve = tve;
		}

		/* If the RW name matches the volume set entry, take
		 * it and exit. First choice is to use the RW volume.
		 */
		if (entry.serverFlags[srvpartpair] & ITSRWVOL) {
		    if (entry.flags & RW_EXISTS) {
			sprintf(patt, "%s", entry.name);
			code = re_exec(patt);
			if (code == 1) {
			    found = 1;
			    foundentry = srvpartpair;
			    volType = RWVOL;
			    break;
			}
		    }

		    /* If the BK name matches the volume set entry, take 
		     * it and exit. Second choice is to use the BK volume.
		     */
		    if (entry.flags & BACK_EXISTS) {
			sprintf(patt, "%s.backup", entry.name);
			code = re_exec(patt);
			if (code == 1) {
			    found = 1;
			    foundentry = srvpartpair;
			    volType = BACKVOL;
			    break;
			}
		    }
		}

		/* If the RO name matches the volume set entry, remember
		 * it, but continue searching. Further entries may be
		 * RW or backup entries that will match.
		 */
		else if (!found && (entry.serverFlags[srvpartpair] & ITSROVOL)
			 && (entry.flags & RO_EXISTS)) {
		    sprintf(patt, "%s.readonly", entry.name);
		    code = re_exec(patt);
		    if (code == 1) {
			found = 1;
			foundentry = srvpartpair;
			volType = ROVOL;
		    }
		}

		if (code < 0)
		    afs_com_err(whoami, 0, "Internal error in regex package");
	    }			/*s */

	    /* If found a match, then create a new volume dump entry */
	    if (found) {	/*f */
		/* Find server and partition structure to hang the entry off of */
		code =
		    getSPEntries(entry.serverNumber[foundentry],
				 entry.serverPartition[foundentry], &servers,
				 &ss, &ps);
		if (code) {
		    afs_com_err(whoami, code, "");
		    return (code);
		}

		total++;
		tvd = (struct bc_volumeDump *)
		    malloc(sizeof(struct bc_volumeDump));
		if (!tvd) {
		    afs_com_err(whoami, BC_NOMEM, "");
		    return (BC_NOMEM);
		}
		memset(tvd, 0, sizeof(*tvd));

		tvd->name = (char *)malloc(strlen(entry.name) + 10);
		if (!(tvd->name)) {
		    afs_com_err(whoami, BC_NOMEM, "");
		    free(tvd);
		    return (BC_NOMEM);
		}

		strcpy(tvd->name, entry.name);
		if (volType == BACKVOL)
		    strcat(tvd->name, ".backup");
		else if (volType == ROVOL)
		    strcat(tvd->name, ".readonly");
		tvd->vid = entry.volumeId[volType];
		tvd->entry = tve;
		tvd->volType = volType;
		tvd->partition = entry.serverPartition[foundentry];
		tvd->server.sin_addr.s_addr = entry.serverNumber[foundentry];
		tvd->server.sin_port = 0;	/* default FS port */
		tvd->server.sin_family = AF_INET;

		/* String tvd off of partition struct */
		tvd->next = ps->vdlist;
		ps->vdlist = tvd;
		if (!tvd->next)
		    ps->lastvdlist = tvd;

		break;
	    }			/*f */
	}			/*ve */
    }				/*w */

    /* Randomly link the volumedump entries together */
    randSPEntries(servers, avols);

    fprintf(stderr, "Total number of volumes : %u\n", total);
    return (0);
}				/*EvalVolumeSet1 */

/* compactDateString
 *	print out a date in compact format, 16 chars, format is
 *	mm/dd/yyyy hh:mm
 * entry:
 *	date_long - ptr to a long containing the time
 * exit:
 *	ptr to a string containing a representation of the date
 */
char *
compactDateString(date_long, string, size)
     afs_int32 *date_long, size;
     char *string;
{
    struct tm *ltime;

    if (!string)
	return 0;

    if (*date_long == NEVERDATE) {
	sprintf(string, "NEVER");
    } else {
        time_t t = *date_long;
	ltime = localtime(&t);
	/* prints date in U.S. format of mm/dd/yyyy */
	strftime(string, size, "%m/%d/%Y %H:%M", ltime);
    }
    return (string);
}

afs_int32
bc_SafeATOI(anum)
     char *anum;
{
    afs_int32 total = 0;

    for (; *anum; anum++) {
	if ((*anum < '0') || (*anum > '9'))
	    return -1;
	total = (10 * total) + (afs_int32) (*anum - '0');
    }
    return total;
}

/* bc_FloatATOI:
 *    Take a string and parse it for a number (could be float) followed
 *    by a character representing the units (K,M,G,T). Default is 'K'.
 *    Return the size in KBytes.
 */
afs_int32
bc_FloatATOI(anum)
     char *anum;
{
    float total = 0;
    afs_int32 rtotal;
    afs_int32 fraction = 0;	/* > 0 if past the decimal */

    for (; *anum; anum++) {
	if ((*anum == 't') || (*anum == 'T')) {
	    total *= 1024 * 1024 * 1024;
	    break;
	}
	if ((*anum == 'g') || (*anum == 'G')) {
	    total *= 1024 * 1024;
	    break;
	}
	if ((*anum == 'm') || (*anum == 'M')) {
	    total *= 1024;
	    break;
	}
	if ((*anum == 'k') || (*anum == 'K')) {
	    break;
	}
	if (*anum == '.') {
	    fraction = 10;
	    continue;
	}
	if ((*anum < '0') || (*anum > '9'))
	    return -1;

	if (!fraction) {
	    total = (10. * total) + (float)(*anum - '0');
	} else {
	    total += ((float)(*anum - '0')) / (float)fraction;
	    fraction *= 10;
	}
    }

    total += 0.5;		/* Round up */
    if (total > 0x7fffffff)	/* Don't go over 2G */
	total = 0x7fffffff;
    rtotal = (afs_int32) total;
    return (rtotal);
}

/* make a copy of a string so that it can be freed later */
char *
bc_CopyString(astring)
     char *astring;
{
    afs_int32 tlen;
    char *tp;

    if (!astring)
	return (NULL);		/* propagate null strings easily */
    tlen = strlen(astring);
    tp = (char *)malloc(tlen + 1);	/* don't forget the terminating null */
    if (!tp) {
	afs_com_err(whoami, BC_NOMEM, "");
	return (tp);
    }
    strcpy(tp, astring);
    return tp;
}

/* concatParams
 * 
 *    Concatenates the parameters of an option and returns the string.
 *
 */

char *
concatParams(itemPtr)
     struct cmd_item *itemPtr;
{
    struct cmd_item *tempPtr;
    afs_int32 length = 0;
    char *string;

    /* compute the length of string required */
    for (tempPtr = itemPtr; tempPtr; tempPtr = tempPtr->next) {
	length += strlen(tempPtr->data);
	length++;		/* space or null terminator */
    }

    if (length == 0) {		/* no string (0 length) */
	afs_com_err(whoami, 0, "Can't have zero length date and time string");
	return (NULL);
    }

    string = (char *)malloc(length);	/* allocate the string */
    if (!string) {
	afs_com_err(whoami, BC_NOMEM, "");
	return (NULL);
    }
    string[0] = 0;

    tempPtr = itemPtr;		/* now assemble the string */
    while (tempPtr) {
	strcat(string, tempPtr->data);
	tempPtr = tempPtr->next;
	if (tempPtr)
	    strcat(string, " ");
    }

    return (string);		/* return the string */
}

/* printIfStatus
 *	print out an interface status node as received from butc
 */

printIfStatus(statusPtr)
     struct tciStatusS *statusPtr;
{
    printf("Task %d: %s: ", statusPtr->taskId, statusPtr->taskName);
    if (statusPtr->nKBytes)
	printf("%ld Kbytes transferred", statusPtr->nKBytes);
    if (strlen(statusPtr->volumeName) != 0) {
	if (statusPtr->nKBytes)
	    printf(", ");
	printf("volume %s", statusPtr->volumeName);
    }

    /* orphan */

    if (statusPtr->flags & ABORT_REQUEST)
	printf(" [abort request rcvd]");

    if (statusPtr->flags & ABORT_DONE)
	printf(" [abort complete]");

    if (statusPtr->flags & OPR_WAIT)
	printf(" [operator wait]");

    if (statusPtr->flags & CALL_WAIT)
	printf(" [callout in progress]");

    if (statusPtr->flags & DRIVE_WAIT)
	printf(" [drive wait]");

    if (statusPtr->flags & TASK_DONE)
	printf(" [done]");
    printf("\n");
}

afs_int32
getPortOffset(port)
     char *port;
{
    afs_int32 portOffset;

    portOffset = bc_SafeATOI(port);

    if (portOffset < 0) {
	afs_com_err(whoami, 0, "Can't decode port offset '%s'", port);
	return (-1);
    } else if (portOffset > BC_MAXPORTOFFSET) {
	afs_com_err(whoami, 0, "%u exceeds max port offset %u", portOffset,
		BC_MAXPORTOFFSET);
	return (-1);
    }
    return (portOffset);
}

/* bc_GetTapeStatusCmd
 *	display status of all tasks on a particular tape coordinator
 */

bc_GetTapeStatusCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 code;
    struct rx_connection *tconn;
    afs_int32 portOffset = 0;

    int ntasks;
    afs_uint32 flags;
    afs_uint32 taskId;
    struct tciStatusS status;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    if (as->parms[0].items) {
	portOffset = getPortOffset(as->parms[0].items->data);
	if (portOffset < 0)
	    return (BC_BADARG);
    }

    code = ConnectButc(bc_globalConfig, portOffset, &tconn);
    if (code)
	return (code);

    flags = TSK_STAT_FIRST;
    taskId = 0;
    ntasks = 0;

    while ((flags & TSK_STAT_END) == 0) {
	code = TC_ScanStatus(tconn, &taskId, &status, &flags);
	if (code) {
	    if (code == TC_NOTASKS)
		break;
	    afs_com_err(whoami, code, "; Can't get status from butc");
	    return (-1);
	}
	if ((flags & TSK_STAT_NOTFOUND))
	    break;		/* Can't find the task id */
	flags &= ~TSK_STAT_FIRST;	/* turn off flag */

	printIfStatus(&status);
	ntasks++;
    }

    if (ntasks == 0)
	printf("Tape coordinator is idle\n");

    if (flags & TSK_STAT_ADSM)
	printf("TSM Tape coordinator\n");
    else if (flags & TSK_STAT_XBSA)
	printf("XBSA Tape coordinator\n");

    return (0);
}

extern struct Lock dispatchLock;

/* bc_WaitForNoJobs
 *	wait for all jobs to terminate
 */
bc_WaitForNoJobs()
{
    afs_int32 wcode = 0;
    int i;
    int waitmsg = 1;
    int usefulJobRunning = 1;

    extern dlqlinkT statusHead;

    afs_com_err(whoami, 0, "waiting for job termination");

    while (usefulJobRunning) {
	usefulJobRunning = (dlqEmpty(&statusHead) ? 0 : 1);
	if (dispatchLock.excl_locked)
	    usefulJobRunning = 1;
	for (i = 0; (!usefulJobRunning && (i < BC_MAXSIMDUMPS)); i++) {
	    if (bc_dumpTasks[i].flags & BC_DI_INUSE)
		usefulJobRunning = 1;
	}

	/* Wait 5 seconds and check again */
	if (usefulJobRunning)
	    IOMGR_Sleep(5);
    }
    return (lastTaskCode);
}

/* bc_JobsCmd
 *	print status on running jobs
 * parameters
 *	ignored - a null "as" prints only jobs.
 */

bc_JobsCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 prevTime;
    dlqlinkP ptr;
    statusP statusPtr;
    char ds[50];

    statusP youngest;
    dlqlinkT atJobsHead;
    extern dlqlinkT statusHead;

    dlqInit(&atJobsHead);

    lock_Status();
    ptr = (&statusHead)->dlq_next;
    while (ptr != &statusHead) {
	statusPtr = (statusP) ptr;
	ptr = ptr->dlq_next;

	if (statusPtr->scheduledDump) {
	    dlqUnlink((dlqlinkP) statusPtr);
	    dlqLinkb(&atJobsHead, (dlqlinkP) statusPtr);
	} else {
	    printf("Job %d:", statusPtr->jobNumber);

	    printf(" %s", statusPtr->taskName);

	    if (statusPtr->dbDumpId)
		printf(": DumpID %u", statusPtr->dbDumpId);
	    if (statusPtr->nKBytes)
		printf(", %ld Kbytes", statusPtr->nKBytes);
	    if (strlen(statusPtr->volumeName) != 0)
		printf(", volume %s", statusPtr->volumeName);

	    if (statusPtr->flags & CONTACT_LOST)
		printf(" [butc contact lost]");

	    if (statusPtr->flags & ABORT_REQUEST)
		printf(" [abort request]");

	    if (statusPtr->flags & ABORT_SENT)
		printf(" [abort sent]");

	    if (statusPtr->flags & OPR_WAIT)
		printf(" [operator wait]");

	    if (statusPtr->flags & CALL_WAIT)
		printf(" [callout in progress]");

	    if (statusPtr->flags & DRIVE_WAIT)
		printf(" [drive wait]");
	    printf("\n");
	}
    }

    /* 
     * Now print the scheduled dumps.
     */
    if (!dlqEmpty(&statusHead) && as)
	printf("\n");		/* blank line between running and scheduled dumps */

    prevTime = 0;
    while (!dlqEmpty(&atJobsHead)) {
	ptr = (&atJobsHead)->dlq_next;
	youngest = (statusP) ptr;

	ptr = ptr->dlq_next;
	while (ptr != &atJobsHead) {	/* Find the dump that starts the earliest */
	    statusPtr = (statusP) ptr;
	    if (statusPtr->scheduledDump < youngest->scheduledDump)
		youngest = statusPtr;
	    ptr = ptr->dlq_next;
	}

	/* Print token expiration time */
	if ((ttoken.endTime > prevTime)
	    && (ttoken.endTime <= youngest->scheduledDump) && as
	    && (ttoken.endTime != NEVERDATE)) {
	    if (ttoken.endTime > time(0)) {
		compactDateString(&ttoken.endTime, ds, 50);
		printf("       %16s: TOKEN EXPIRATION\n", ds);
	    } else {
		printf("       TOKEN HAS EXPIRED\n");
	    }
	}
	prevTime = youngest->scheduledDump;

	/* Print the info */
	compactDateString(&youngest->scheduledDump, ds, 50);
	printf("Job %d:", youngest->jobNumber);
	printf(" %16s: %s", ds, youngest->cmdLine);
	printf("\n");

	/* return to original list */
	dlqUnlink((dlqlinkP) youngest);
	dlqLinkb(&statusHead, (dlqlinkP) youngest);
    }

    /* Print token expiration time if havn't already */
    if ((ttoken.endTime == NEVERDATE) && as)
	printf("     : TOKEN NEVER EXPIRES\n");
    else if ((ttoken.endTime > prevTime) && as) {
	if (ttoken.endTime > time(0)) {
	    compactDateString(&ttoken.endTime, ds, 50);
	    printf("       %16s: TOKEN EXPIRATION\n", ds);
	} else {
	    printf("     : TOKEN HAS EXPIRED\n");
	}
    }

    unlock_Status();
    return 0;
}

bc_KillCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 i;
    afs_int32 slot;
    struct bc_dumpTask *td;
    char *tp;
    char tbuffer[256];

    dlqlinkP ptr;
    statusP statusPtr;

    extern dlqlinkT statusHead;
    extern statusP findStatus();


    tp = as->parms[0].items->data;
    if (strchr(tp, '.') == 0) {
	slot = bc_SafeATOI(tp);
	if (slot == -1) {
	    afs_com_err(whoami, 0, "Bad syntax for number '%s'", tp);
	    return -1;
	}

	lock_Status();
	ptr = (&statusHead)->dlq_next;
	while (ptr != &statusHead) {
	    statusPtr = (statusP) ptr;
	    if (statusPtr->jobNumber == slot) {
		statusPtr->flags |= ABORT_REQUEST;
		unlock_Status();
		return (0);
	    }
	    ptr = ptr->dlq_next;
	}
	unlock_Status();

	fprintf(stderr, "Job %d not found\n", slot);
	return (-1);
    } else {
	/* vol.dump */
	td = bc_dumpTasks;
	for (i = 0; i < BC_MAXSIMDUMPS; i++, td++) {
	    if (td->flags & BC_DI_INUSE) {
		/* compute name */
		strcpy(tbuffer, td->volSetName);
		strcat(tbuffer, ".");
		strcat(tbuffer, tailCompPtr(td->dumpName));
		if (strcmp(tbuffer, tp) == 0)
		    break;
	    }
	}
	if (i >= BC_MAXSIMDUMPS) {
	    afs_com_err(whoami, 0, "Can't find job %s", tp);
	    return -1;
	}

	lock_Status();
	statusPtr = findStatus(td->dumpID);

	if (statusPtr == 0) {
	    afs_com_err(whoami, 0, "Can't locate status - internal error");
	    unlock_Status();
	    return (-1);
	}
	statusPtr->flags |= ABORT_REQUEST;
	unlock_Status();
	return (0);
    }
    return 0;
}

/* restore a volume or volumes */
bc_VolRestoreCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    /*
     * parm 0 is the new server to restore to
     * parm 1 is the new partition to restore to
     * parm 2 is volume(s) to restore
     * parm 3 is the new extension, if any, for the volume name.
     * parm 4 gives the new volume # to restore this volume as (removed).
     * parm 4 date is a string representing the date
     *
     * We handle four types of restores.  If old is set, then we restore the
     * volume with the same name and ID.  If old is not set, we allocate
     * a new volume ID for the restored volume.  If a new extension is specified,
     * we add that extension to the volume name of the restored volume.
     */
    struct bc_volumeEntry tvolumeEntry;	/* entry within the volume set */
    struct bc_volumeDump *volsToRestore = (struct bc_volumeDump *)0;
    struct bc_volumeDump *lastVol = (struct bc_volumeDump *)0;
    struct bc_volumeDump *tvol;	/* temp for same */
    struct sockaddr_in destServ;	/* machine to which to restore volumes */
    afs_int32 destPartition;	/* partition to which to restore volumes */
    char *tp;
    struct cmd_item *ti;
    afs_int32 code;
    int oldFlag;
    afs_int32 fromDate;
    afs_int32 dumpID = 0;
    char *newExt, *timeString;
    afs_int32 i;
    afs_int32 *ports = NULL;
    afs_int32 portCount = 0;
    int dontExecute;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    /* specified other destination host */
    if (as->parms[0].items) {
	tp = as->parms[0].items->data;
	if (bc_ParseHost(tp, &destServ)) {
	    afs_com_err(whoami, 0, "Failed to locate destination host '%s'", tp);
	    return -1;
	}
    }

    /* specified other destination partition */
    if (as->parms[1].items) {
	tp = as->parms[1].items->data;
	if (bc_GetPartitionID(tp, &destPartition)) {
	    afs_com_err(whoami, 0, "Can't parse destination partition '%s'", tp);
	    return -1;
	}
    }

    for (ti = as->parms[2].items; ti; ti = ti->next) {
	/* build list of volume items */
	tvol = (struct bc_volumeDump *)malloc(sizeof(struct bc_volumeDump));
	if (!tvol) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    return BC_NOMEM;
	}
	memset(tvol, 0, sizeof(struct bc_volumeDump));

	tvol->name = (char *)malloc(VOLSER_MAXVOLNAME + 1);
	if (!tvol->name) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    return BC_NOMEM;
	}
	strncpy(tvol->name, ti->data, VOLSER_OLDMAXVOLNAME);
	tvol->entry = &tvolumeEntry;

	if (lastVol)
	    lastVol->next = tvol;	/* thread onto end of list */
	else
	    volsToRestore = tvol;
	lastVol = tvol;
    }

    if (as->parms[4].items) {
	timeString = concatParams(as->parms[4].items);
	if (!timeString)
	    return (-1);

	code = ktime_DateToLong(timeString, &fromDate);
	free(timeString);
	if (code) {
	    afs_com_err(whoami, 0, "Can't parse restore date and time");
	    afs_com_err(whoami, 0, "%s", ktime_GetDateUsage());
	    return code;
	}
    } else {
	fromDate = 0x7fffffff;	/* latest one */
    }

    newExt = (as->parms[3].items ? as->parms[3].items->data : NULL);
    oldFlag = 0;

    /* Read all the port offsets into the ports array. The first element in the
     * array is for full restore and the rest are for incremental restores 
     */
    if (as->parms[5].items) {
	for (ti = as->parms[5].items; ti; ti = ti->next)
	    portCount++;
	ports = (afs_int32 *) malloc(portCount * sizeof(afs_int32));
	if (!ports) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    return BC_NOMEM;
	}

	for (ti = as->parms[5].items, i = 0; ti; ti = ti->next, i++) {
	    ports[i] = getPortOffset(ti->data);
	    if (ports[i] < 0)
		return (BC_BADARG);
	}
    }

    dontExecute = (as->parms[6].items ? 1 : 0);	/* -n */

    if (as->parms[7].items)
      {
	dumpID = atoi(as->parms[7].items->data);
	if (dumpID <= 0)
	  dumpID = 0;
      }
    
    /*
     * Perform the call to start the restore.
     */
    code =
	bc_StartDmpRst(bc_globalConfig, "volume", "restore", volsToRestore,
		       &destServ, destPartition, fromDate, newExt, oldFlag,
		       /*parentDump */ dumpID, /*dumpLevel */ 0,
		       bc_Restorer, ports, portCount,
		       /*dumpSched */ NULL, /*append */ 0, dontExecute);
    if (code)
	afs_com_err(whoami, code, "; Failed to queue restore");

    return (code);
}

/* restore a whole partition or server */

/* bc_DiskRestoreCmd
 *	restore a whole partition
 * params:
 *	first, reqd - machine (server) to restore
 *	second, reqd - partition to restore
 *	various optional
 */

bc_DiskRestoreCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    struct bc_volumeSet tvolumeSet;	/* temporary volume set for EvalVolumeSet call */
    struct bc_volumeEntry tvolumeEntry;	/* entry within the volume set */
    struct bc_volumeDump *volsToRestore = (struct bc_volumeDump *)0;
    struct sockaddr_in destServ;	/* machine to which to restore volumes */
    afs_int32 destPartition;	/* partition to which to restore volumes */
    char *tp;
    afs_int32 code;
    int oldFlag;
    afs_int32 fromDate;
    char *newExt;
    afs_int32 *ports = NULL;
    afs_int32 portCount = 0;
    int dontExecute;
    struct bc_volumeDump *prev, *tvol, *nextvol;
    struct cmd_item *ti;
    afs_int32 i;

    /* parm 0 is the server to restore
     * parm 1 is the partition to restore
     
     * parm 8 and above as in VolRestoreCmd:
     * parm 8 is the new server to restore to
     * parm 9 is the new partition to restore to
     */

    code = bc_UpdateVolumeSet();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve volume sets");
	return (code);
    }
    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    /* create a volume set corresponding to the volume pattern we've been given */
    memset(&tvolumeSet, 0, sizeof(tvolumeSet));
    memset(&tvolumeEntry, 0, sizeof(tvolumeEntry));
    tvolumeSet.name = "TempVolumeSet";
    tvolumeSet.ventries = &tvolumeEntry;
    tvolumeEntry.serverName = as->parms[0].items->data;
    tvolumeEntry.partname = as->parms[1].items->data;

    if (bc_GetPartitionID(tvolumeEntry.partname, &tvolumeEntry.partition)) {
	afs_com_err(whoami, 0, "Can't parse partition '%s'",
		tvolumeEntry.partname);
	return -1;
    }

    if (bc_ParseHost(tvolumeEntry.serverName, &tvolumeEntry.server)) {
	afs_com_err(whoami, 0, "Can't locate host '%s'", tvolumeEntry.serverName);
	return -1;
    }

    /* specified other destination host */
    if (as->parms[8].items) {
	tp = as->parms[8].items->data;
	if (bc_ParseHost(tp, &destServ)) {
	    afs_com_err(whoami, 0, "Can't locate destination host '%s'", tp);
	    return -1;
	}
    } else			/* use destination host == original host */
	memcpy(&destServ, &tvolumeEntry.server, sizeof(destServ));

    /* specified other destination partition */
    if (as->parms[9].items) {
	tp = as->parms[9].items->data;
	if (bc_GetPartitionID(tp, &destPartition)) {
	    afs_com_err(whoami, 0, "Can't parse destination partition '%s'", tp);
	    return -1;
	}
    } else			/* use original partition */
	destPartition = tvolumeEntry.partition;

    tvolumeEntry.name = ".*";	/* match all volumes (this should be a parameter) */

    if (as->parms[2].items) {
	for (ti = as->parms[2].items; ti; ti = ti->next)
	    portCount++;
	ports = (afs_int32 *) malloc(portCount * sizeof(afs_int32));
	if (!ports) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    return BC_NOMEM;
	}

	for (ti = as->parms[2].items, i = 0; ti; ti = ti->next, i++) {
	    ports[i] = getPortOffset(ti->data);
	    if (ports[i] < 0)
		return (BC_BADARG);
	}
    }

    newExt = (as->parms[10].items ? as->parms[10].items->data : NULL);
    dontExecute = (as->parms[11].items ? 1 : 0);	/* -n */

    /*
     * Expand out the volume set into its component list of volumes, by calling VLDB server.
     */
    code =
	bc_EvalVolumeSet(bc_globalConfig, &tvolumeSet, &volsToRestore,
			 cstruct);
    if (code) {
	afs_com_err(whoami, code, "; Failed to evaluate volume set");
	return (-1);
    }

    /* Since we want only RW volumes, remove any 
     * BK or RO volumes from the list.
     */
    for (prev = 0, tvol = volsToRestore; tvol; tvol = nextvol) {
	nextvol = tvol->next;
	if (BackupName(tvol->name)) {
	    if (tvol->name)
		fprintf(stderr,
			"Will not restore volume %s (its RW does not reside on the partition)\n",
			tvol->name);

	    if (tvol == volsToRestore) {
		volsToRestore = nextvol;
	    } else {
		prev->next = nextvol;
	    }
	    if (tvol->name)
		free(tvol->name);
	    free(tvol);
	} else {
	    prev = tvol;
	}
    }

    fromDate = 0x7fffffff;	/* last one before this date */
    oldFlag = 1;		/* do restore same volid (and name) */

    /*
     * Perform the call to start the dump.
     */
    code =
	bc_StartDmpRst(bc_globalConfig, "disk", "restore", volsToRestore,
		       &destServ, destPartition, fromDate, newExt, oldFlag,
		       /*parentDump */ 0, /*dumpLevel */ 0,
		       bc_Restorer, ports, portCount,
		       /*dumpSched */ NULL, /*append */ 0, dontExecute);
    if (code)
	afs_com_err(whoami, code, "; Failed to queue restore");

    return (code);
}

/* bc_VolsetRestoreCmd
 *	restore a volumeset or list of volumes.
 */

bc_VolsetRestoreCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    int oldFlag;
    long fromDate;
    char *newExt;

    int dontExecute;
    afs_int32 *ports = NULL;
    afs_int32 portCount = 0;
    afs_int32 code = 0;
    afs_int32 portoffset = 0;
    char *volsetName;
    struct bc_volumeSet *volsetPtr;	/* Ptr to list of generated volume info */
    struct bc_volumeDump *volsToRestore = (struct bc_volumeDump *)0;
    struct bc_volumeDump *lastVol = (struct bc_volumeDump *)0;
    struct sockaddr_in destServer;	/* machine to which to restore volume */
    afs_int32 destPartition;	/* partition to which to restore volumes */
    struct cmd_item *ti;
    afs_int32 i;

    code = bc_UpdateVolumeSet();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve volume sets");
	return (code);
    }
    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    if (as->parms[0].items) {
	if (as->parms[1].items) {
	    afs_com_err(whoami, 0, "Can't have both -name and -file options");
	    return (-1);
	}

	volsetName = as->parms[0].items->data;
	volsetPtr = bc_FindVolumeSet(bc_globalConfig, volsetName);
	if (!volsetPtr) {
	    afs_com_err(whoami, 0,
		    "Can't find volume set '%s' in backup database",
		    volsetName);
	    return (-1);
	}

	/* Expand out the volume set into its component list of volumes. */
	code =
	    bc_EvalVolumeSet(bc_globalConfig, volsetPtr, &volsToRestore,
			     cstruct);
	if (code) {
	    afs_com_err(whoami, code, "; Failed to evaluate volume set");
	    return (-1);
	}
    } else if (as->parms[1].items) {
	FILE *fd;
	char line[256];
	char server[50], partition[50], volume[50], rest[256];
	long count;
	struct bc_volumeDump *tvol;

	fd = fopen(as->parms[1].items->data, "r");
	if (!fd) {
	    afs_com_err(whoami, errno, "; Cannot open file '%s'",
		    as->parms[1].items->data);
	    return (-1);
	}

	while (fgets(line, 255, fd)) {
	    count =
		sscanf(line, "%s %s %s %s", server, partition, volume, rest);

	    if (count <= 0)
		continue;
	    if (count < 3) {
		fprintf(stderr,
			"Invalid volumeset file format: Wrong number of arguments: Ignoring\n");
		fprintf(stderr, "     %s", line);
		continue;
	    }

	    if (bc_ParseHost(server, &destServer)) {
		afs_com_err(whoami, 0, "Failed to locate host '%s'", server);
		continue;
	    }

	    if (bc_GetPartitionID(partition, &destPartition)) {
		afs_com_err(whoami, 0,
			"Failed to parse destination partition '%s'",
			partition);
		continue;
	    }

	    /* Allocate a volumeDump structure and link it in */
	    tvol =
		(struct bc_volumeDump *)malloc(sizeof(struct bc_volumeDump));
	    memset(tvol, 0, sizeof(struct bc_volumeDump));

	    tvol->name = (char *)malloc(VOLSER_MAXVOLNAME + 1);
	    if (!tvol->name) {
		afs_com_err(whoami, BC_NOMEM, "");
		return BC_NOMEM;
	    }
	    strncpy(tvol->name, volume, VOLSER_OLDMAXVOLNAME);
	    memcpy(&tvol->server, &destServer, sizeof(destServer));
	    tvol->partition = destPartition;

	    if (lastVol)
		lastVol->next = tvol;	/* thread onto end of list */
	    else
		volsToRestore = tvol;
	    lastVol = tvol;
	}
	fclose(fd);
    } else {
	afs_com_err(whoami, 0, "-name or -file option required");
	return (-1);
    }


    /* Get the port offset for the restore */
    if (as->parms[2].items) {
	for (ti = as->parms[2].items; ti; ti = ti->next)
	    portCount++;
	ports = (afs_int32 *) malloc(portCount * sizeof(afs_int32));
	if (!ports) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    return BC_NOMEM;
	}

	for (ti = as->parms[2].items, i = 0; ti; ti = ti->next, i++) {
	    ports[i] = getPortOffset(ti->data);
	    if (ports[i] < 0)
		return (BC_BADARG);
	}
    }

    newExt = (as->parms[3].items ? as->parms[3].items->data : NULL);
    dontExecute = (as->parms[4].items ? 1 : 0);

    fromDate = 0x7fffffff;	/* last one before this date */
    oldFlag = 1;		/* do restore same volid (and name) */

    /* Perform the call to start the restore */
    code = bc_StartDmpRst(bc_globalConfig, "disk", "restore", volsToRestore,
			  /*destserver */ NULL, /*destpartition */ 0, fromDate,
			  newExt, oldFlag,
			  /*parentDump */ 0, /*dumpLevel */ 0,
			  bc_Restorer, ports, portCount,
			  /*dumpSched */ NULL, /*append */ 0, dontExecute);
    if (code)
	afs_com_err(whoami, code, "; Failed to queue restore");

    return code;
}

/*-----------------------------------------------------------------------------
 * bc_DumpCmd
 *
 * Description:
 *	Perform a dump of the set of volumes provided.
 *	user specifies: -volumeset .. -dump .. [-portoffset ..] [-n]
 *
 * Arguments:
 *	as	: Parsed command line information.
 *	arock	: Ptr to misc stuff; not used.
 *
 * Returns:
 *	-1 on errors,
 *	The result of bc_StartDump() otherwise
 *
 * Environment:
 *	Nothing special.
 *
 * Side Effects:
 *	As advertised.
 *---------------------------------------------------------------------------
 */
int dontExecute;

int
bc_DumpCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{				/*bc_DumpCmd */
    static char rn[] = "bc_DumpCmd";	/*Routine name */
    char *dumpPath, *vsName;	/*Ptrs to various names */
    struct bc_volumeSet *tvs;	/*Ptr to list of generated volume info */
    struct bc_dumpSchedule *tds, *baseds;	/*Ptr to dump schedule node */
    struct bc_volumeDump *tve, *volsToDump;	/*Ptr to individual vols to be dumped */
    struct budb_dumpEntry dumpEntry, de, fde;	/* dump entry */
    afs_uint32 d;

    afs_int32 parent;		/* parent dump */
    afs_int32 level;		/* this dump's level # */
    afs_int32 problemFindingDump;	/* can't find parent(s) */

    afs_int32 *portp = NULL;
    afs_int32 portCount = 0;
    afs_int32 doAt, atTime;	/* Time a timed-dump is to start at */
    afs_int32 length;
    char *timeString;
    int doAppend;		/* Append the dump to dump set */
    afs_int32 code;		/* Return code */
    int loadfile;		/* whether to load a file or not */

    statusP statusPtr;

    extern struct bc_dumpTask bc_dumpTasks[];
    extern afs_int32 bcdb_FindLastVolClone();
    extern afs_int32 volImageTime();

    code = bc_UpdateDumpSchedule();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve dump schedule");
	return (code);
    }
    code = bc_UpdateVolumeSet();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve volume sets");
	return (code);
    }
    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    /* 
     * Some parameters cannot be specified together
     * The "-file" option cannot exist with the "-volume", "-dump", 
     * "-portoffset", or "-append" option 
     */
    if (as->parms[6].items) {
	loadfile = 1;
	if (as->parms[0].items || as->parms[1].items || as->parms[2].items
	    || as->parms[4].items) {
	    afs_com_err(whoami, 0, "Invalid option specified with -file option");
	    return -1;
	}
    } else {
	loadfile = 0;
	if (!as->parms[0].items || !as->parms[1].items) {
	    afs_com_err(whoami, 0,
		    "Must specify volume set name and dump level name");
	    return -1;
	}
    }

    /* 
     * Get the time we are to perform this dump
     */
    if (as->parms[3].items) {
	doAt = 1;

	timeString = concatParams(as->parms[3].items);
	if (!timeString)
	    return (-1);

	/*
	 * Now parse this string for the time to start.
	 */
	code = ktime_DateToLong(timeString, &atTime);
	free(timeString);
	if (code) {
	    afs_com_err(whoami, 0, "Can't parse dump start date and time");
	    afs_com_err(whoami, 0, "%s", ktime_GetDateUsage());
	    return (1);
	}
    } else
	doAt = 0;

    dontExecute = (as->parms[5].items ? 1 : 0);	/* -n */

    /* 
     * If this dump is not a load file, then check the parameters.
     */
    if (!loadfile) {		/*6 */
	vsName = as->parms[0].items->data;	/* get volume set name */
	dumpPath = as->parms[1].items->data;	/* get dump path */

	/* get the port number, if one was specified */
	if (as->parms[2].items) {
	    portCount = 1;
	    portp = (afs_int32 *) malloc(sizeof(afs_int32));
	    if (!portp) {
		afs_com_err(whoami, BC_NOMEM, "");
		return BC_NOMEM;
	    }

	    *portp = getPortOffset(as->parms[2].items->data);
	    if (*portp < 0)
		return (BC_BADARG);
	}

	doAppend = (as->parms[4].items ? 1 : 0);	/* -append */

	/*
	 * Get a hold of the given volume set and dump set.
	 */
	tvs = bc_FindVolumeSet(bc_globalConfig, vsName);
	if (!tvs) {
	    afs_com_err(whoami, 0,
		    "Can't find volume set '%s' in backup database", vsName);
	    return (-1);
	}
	baseds = bc_FindDumpSchedule(bc_globalConfig, dumpPath);
	if (!baseds) {
	    afs_com_err(whoami, 0,
		    "Can't find dump schedule '%s' in backup database",
		    dumpPath);
	    return (-1);
	}

    }

    /*6 */
    /*
     * If given the "-at" option, then add this to the jobs list and return 
     * with no error.
     *
     * Create a status node for this timed dump.
     * Fill in the time to dump and the cmd line for the dump leaving off
     * the -at option.  If the -n option is there, it is scheduled with 
     * the Timed dump as opposed to not scheduling the time dump at all.
     */
    if (doAt) {
	if (atTime < time(0)) {
	    afs_com_err(whoami, 0,
		    "Time of dump is earlier then current time - not added");
	} else {
	    statusPtr = createStatusNode();
	    lock_Status();
	    statusPtr->scheduledDump = atTime;

	    /* Determine length of the dump command */
	    length = 0;
	    length += 4;	/* "dump" */
	    if (loadfile) {
		length += 6;	/* " -file" */
		length += 1 + strlen(as->parms[6].items->data);	/* " <file>" */
	    } else {
		/* length += 11; *//* " -volumeset" */
		length += 1 + strlen(as->parms[0].items->data);	/* " <volset> */

		/* length += 6; *//* " -dump" */
		length += 1 + strlen(as->parms[1].items->data);	/* " <dumpset> */

		if (as->parms[2].items) {
		    /* length += 12; *//* " -portoffset" */
		    length += 1 + strlen(as->parms[2].items->data);	/* " <port>" */
		}

		if (as->parms[4].items)
		    length += 8;	/* " -append" */
	    }

	    if (dontExecute)
		length += 3;	/* " -n" */
	    length++;		/* end-of-line */

	    /* Allocate status block for this timed dump */
	    sprintf(statusPtr->taskName, "Scheduled Dump");
	    statusPtr->jobNumber = bc_jobNumber();
	    statusPtr->scheduledDump = atTime;
	    statusPtr->cmdLine = (char *)malloc(length);
	    if (!statusPtr->cmdLine) {
		afs_com_err(whoami, BC_NOMEM, "");
		return BC_NOMEM;
	    }

	    /* Now reconstruct the dump command */
	    statusPtr->cmdLine[0] = 0;
	    strcat(statusPtr->cmdLine, "dump");
	    if (loadfile) {
		strcat(statusPtr->cmdLine, " -file");
		strcat(statusPtr->cmdLine, " ");
		strcat(statusPtr->cmdLine, as->parms[6].items->data);
	    } else {
		/* strcat(statusPtr->cmdLine, " -volumeset"); */
		strcat(statusPtr->cmdLine, " ");
		strcat(statusPtr->cmdLine, as->parms[0].items->data);

		/* strcat(statusPtr->cmdLine, " -dump"); */
		strcat(statusPtr->cmdLine, " ");
		strcat(statusPtr->cmdLine, as->parms[1].items->data);

		if (as->parms[2].items) {
		    /* strcat(statusPtr->cmdLine, " -portoffset"); */
		    strcat(statusPtr->cmdLine, " ");
		    strcat(statusPtr->cmdLine, as->parms[2].items->data);
		}

		if (as->parms[4].items)
		    strcat(statusPtr->cmdLine, " -append");
	    }
	    if (dontExecute)
		strcat(statusPtr->cmdLine, " -n");

	    printf("Add scheduled dump as job %d\n", statusPtr->jobNumber);
	    if ((atTime > ttoken.endTime) && (ttoken.endTime != NEVERDATE))
		afs_com_err(whoami, 0,
			"Warning: job %d starts after expiration of AFS token",
			statusPtr->jobNumber);

	    unlock_Status();
	}

	return (0);
    }

    /* 
     * Read and execute the load file if specified.  The work of reading is done
     * in the main routine prior the dispatch call. loadFile and dontExecute are
     * global variables so this can take place in main.
     */
    if (loadfile) {
	loadFile = (char *)malloc(strlen(as->parms[6].items->data) + 1);
	if (!loadFile) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    return BC_NOMEM;
	}
	strcpy(loadFile, as->parms[6].items->data);
	return 0;
    }

    /* 
     * We are doing a real dump (no load file or timed dump).
     */
    printf("Starting dump of volume set '%s' (dump level '%s')\n", vsName,
	   dumpPath);

    /* For each dump-level above this one, see if the volumeset was dumped
     * at the level. We search all dump-levels since a higher dump-level
     * may have actually been done AFTER a lower dump level.
     */
    parent = level = problemFindingDump = 0;
    for (tds = baseds->parent; tds; tds = tds->parent) {
	/* Find the most recent dump of the volume-set at this dump-level.
	 * Raise problem flag if didn't find a dump and a parent not yet found.
	 */
	code = bcdb_FindLatestDump(vsName, tds->name, &dumpEntry);
	if (code) {
	    if (!parent)
		problemFindingDump = 1;	/* skipping a dump level */
	    continue;
	}

	/* We found the most recent dump at this level. Now check
	 * if we should use it by seeing if its full dump hierarchy 
	 * exists. If it doesn't, we don't want to base our incremental
	 * off of this dump. 
	 */
	if (!parent || (dumpEntry.id > parent)) {
	    /* Follow the parent dumps to see if they are all there */
	    for (d = dumpEntry.parent; d; d = de.parent) {
		code = bcdb_FindDumpByID(d, &de);
		if (code)
		    break;
	    }

	    /* If we found the entire level, remember it. Otherwise raise flag.
	     * If we had already found a dump, raise the problem flag.
	     */
	    if (!d && !code) {
		if (parent)
		    problemFindingDump = 1;
		parent = dumpEntry.id;
		level = dumpEntry.level + 1;
		memcpy(&fde, &dumpEntry, sizeof(dumpEntry));
	    } else {
		/* Dump hierarchy not complete so can't base off the latest */
		problemFindingDump = 1;
	    }
	}
    }

    /* If the problemflag was raise, it means we are not doing the 
     * dump at the level we requested it be done at.
     */
    if (problemFindingDump) {
	afs_com_err(whoami, 0,
		"Warning: Doing level %d dump due to missing higher-level dumps",
		level);
	if (parent) {
	    printf("Parent dump: dump %s (DumpID %u)\n", fde.name, parent);
	}
    } else if (parent) {
	printf("Found parent: dump %s (DumpID %u)\n", fde.name, parent);
    }

    /* Expand out the volume set into its component list of volumes. */
    code = bc_EvalVolumeSet(bc_globalConfig, tvs, &volsToDump, cstruct);
    if (code) {
	afs_com_err(whoami, code, "; Failed to evaluate volume set");
	return (-1);
    }
    if (!volsToDump) {
	printf("No volumes to dump\n");
	return (0);
    }

    /* Determine what the clone time of the volume was when it was
     * last dumped (tve->date). This is the time from when an
     * incremental should be done (remains zero if a full dump).
     */
    if (parent) {
	for (tve = volsToDump; tve; tve = tve->next) {
	    code = bcdb_FindClone(parent, tve->name, &tve->date);
	    if (code)
		tve->date = 0;

	    /* Get the time the volume was last cloned and see if the volume has
	     * changed since then. Only do this when the "-n" flag is specified
	     * because butc will get the cloneDate at time of dump.
	     */
	    if (dontExecute) {
		code =
		    volImageTime(tve->server.sin_addr.s_addr, tve->partition,
				 tve->vid, tve->volType, &tve->cloneDate);
		if (code)
		    tve->cloneDate = 0;

		if (tve->cloneDate && (tve->cloneDate == tve->date)) {
		    afs_com_err(whoami, 0,
			    "Warning: Timestamp on volume %s unchanged from previous dump",
			    tve->name);
		}
	    }
	}
    }

    if (dontExecute)
	printf("Would have dumped the following volumes:\n");
    else
	printf("Preparing to dump the following volumes:\n");
    for (tve = volsToDump; tve; tve = tve->next) {
	printf("\t%s (%u)\n", tve->name, tve->vid);
    }
    if (dontExecute)
	return (0);

    code = bc_StartDmpRst(bc_globalConfig, dumpPath, vsName, volsToDump,
			  /*destServer */ NULL, /*destPartition */ 0,
			  /*fromDate */ 0,
			  /*newExt */ NULL, /*oldFlag */ 0,
			  parent, level, bc_Dumper, portp, /*portCount */ 1,
			  baseds, doAppend, dontExecute);
    if (code)
	afs_com_err(whoami, code, "; Failed to queue dump");

    return (code);
}				/*bc_DumpCmd */


/* bc_QuitCmd
 *	terminate the backup process. Insists that that all running backup
 *	jobs be terminated before it will quit
 * parameters:
 *	ignored
 */

bc_QuitCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    int i;
    struct bc_dumpTask *td;
    extern dlqlinkT statusHead;
    dlqlinkP ptr;
    statusP statusPtr;

    /* Check the status list for outstanding jobs */
    lock_Status();
    for (ptr = (&statusHead)->dlq_next; ptr != &statusHead;
	 ptr = ptr->dlq_next) {
	statusPtr = (statusP) ptr;
	if (!(statusPtr->flags & ABORT_REQUEST)) {
	    unlock_Status();
	    afs_com_err(whoami, 0, "Job %d still running (and not aborted)",
		    statusPtr->jobNumber);
	    afs_com_err(whoami, 0,
		    "You must at least 'kill' all running jobs before quitting");
	    return -1;
	}
    }
    unlock_Status();

    /* A job still being initialized (but no status structure or job number since it
     * has not been handed to a butc process yet)
     */
    for (td = bc_dumpTasks, i = 0; i < BC_MAXSIMDUMPS; i++, td++) {
	if (td->flags & BC_DI_INUSE) {
	    afs_com_err(whoami, 0, "A job is still running");
	    afs_com_err(whoami, 0,
		    "You must at least 'kill' all running jobs before quitting");
	    return -1;
	}
    }

#ifdef AFS_NT40_ENV
    /* close the all temp text files before quitting */
    for (i = 0; i < TB_NUM; i++)
	bc_closeTextFile(&bc_globalConfig->configText[i],
			 &bc_globalConfig->tmpTextFileNames[i][0]);
#endif
    exit(lastTaskCode);
}

/* bc_LabelTapeCmd
 *	Labels a tape i.e. request the tape coordinator to perform this
 *	operation
 */

bc_LabelTapeCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    char *tapename = 0, *pname = 0;
    afs_int32 size;
    afs_int32 code;
    afs_int32 port = 0;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    if (as->parms[0].items) {	/* -name */
	tapename = as->parms[0].items->data;
	if (strlen(tapename) >= TC_MAXTAPELEN) {
	    afs_com_err(whoami, 0, "AFS tape name '%s' is too long", tapename);
	    return -1;
	}
    }

    if (as->parms[3].items) {	/* -pname */
	if (tapename) {
	    afs_com_err(whoami, 0, "Can only specify -name or -pname");
	    return -1;
	}
	pname = as->parms[3].items->data;
	if (strlen(pname) >= TC_MAXTAPELEN) {
	    afs_com_err(whoami, 0, "Permanent tape name '%s' is too long", pname);
	    return -1;
	}
    }

    if (as->parms[1].items) {
	size = bc_FloatATOI(as->parms[1].items->data);
	if (size == -1) {
	    afs_com_err(whoami, 0, "Bad syntax for tape size '%s'",
		    as->parms[1].items->data);
	    return -1;
	}
    } else
	size = 0;

    if (as->parms[2].items) {
	port = getPortOffset(as->parms[2].items->data);
	if (port < 0)
	    return (BC_BADARG);
    }

    code = bc_LabelTape(tapename, pname, size, bc_globalConfig, port);
    if (code)
	return code;
    return 0;
}

/* bc_ReadLabelCmd
 *	read the label on a tape
 * params:
 *	optional port number
 */

bc_ReadLabelCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 code;
    afs_int32 port = 0;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    if (as->parms[0].items) {
	port = getPortOffset(as->parms[0].items->data);
	if (port < 0)
	    return (BC_BADARG);
    }

    code = bc_ReadLabel(bc_globalConfig, port);
    if (code)
	return code;
    return 0;
}

/* bc_ScanDumpsCmd
 *	read content information from dump tapes, and if user desires,
 *	add it to the database
 */

bc_ScanDumpsCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 port = 0;
    afs_int32 dbAddFlag = 0;
    afs_int32 code;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    /* check for flag */
    if (as->parms[0].items != 0) {	/* add scan to database */
	dbAddFlag++;
    }

    /* check for port */
    if (as->parms[1].items) {
	port = getPortOffset(as->parms[1].items->data);
	if (port < 0)
	    return (BC_BADARG);
    }

    code = bc_ScanDumps(bc_globalConfig, dbAddFlag, port);
    return (code);
}

/* bc_ParseExpiration
 *
 * Notes:
 *	dates are specified as absolute or relative, the syntax is:
 *	absolute:	at %d/%d/%d [%d:%d]	where [..] is optional
 *	relative:	in [%dy][%dm][%dd]	where at least one component
 *						must be specified
 */

afs_int32
bc_ParseExpiration(paramPtr, expType, expDate)
     struct cmd_parmdesc *paramPtr;
     afs_int32 *expType;
     afs_int32 *expDate;
{
    struct cmd_item *itemPtr;
    struct ktime_date kt;
    char *dateString = 0;
    afs_int32 code = 0;

    *expType = BC_NO_EXPDATE;
    *expDate = 0;

    if (!paramPtr->items)
	ERROR(0);		/* no expiration specified */

    /* some form of expiration date specified. First validate the prefix */
    itemPtr = paramPtr->items;

    if (strcmp(itemPtr->data, "at") == 0) {
	*expType = BC_ABS_EXPDATE;

	dateString = concatParams(itemPtr->next);
	if (!dateString)
	    ERROR(1);

	code = ktime_DateToLong(dateString, expDate);
	if (code)
	    ERROR(1);
    } else if (strcmp(itemPtr->data, "in") == 0) {
	*expType = BC_REL_EXPDATE;

	dateString = concatParams(itemPtr->next);
	if (!dateString)
	    ERROR(1);

	code = ParseRelDate(dateString, &kt);
	if (code)
	    ERROR(1);
	*expDate = ktimeRelDate_ToLong(&kt);
    } else {
	dateString = concatParams(itemPtr);
	if (!dateString)
	    ERROR(1);

	if (ktime_DateToLong(dateString, expDate) == 0) {
	    *expType = BC_ABS_EXPDATE;
	    code = ktime_DateToLong(dateString, expDate);
	    if (code)
		ERROR(1);
	} else if (ParseRelDate(dateString, &kt) == 0) {
	    *expType = BC_REL_EXPDATE;
	    *expDate = ktimeRelDate_ToLong(&kt);
	} else {
	    ERROR(1);
	}
    }

  error_exit:
    if (dateString)
	free(dateString);
    return (code);
}

/* database lookup command and routines */

/* bc_dblookupCmd
 *	Currently a single option, volumename to search for. Reports
 *	all dumps containing the specified volume
 */

bc_dblookupCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    struct cmd_item *ciptr;
    afs_int32 code;

    ciptr = as->parms[0].items;
    if (ciptr == 0)		/* no argument specified */
	return (-1);

    code = DBLookupByVolume(ciptr->data);
    return (code);
}



/* for ubik version */

bc_dbVerifyCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 status;
    afs_int32 orphans;
    afs_int32 host;

    struct hostent *hostPtr;
    int detail;
    afs_int32 code = 0;

    extern struct udbHandleS udbHandle;

    detail = (as->parms[0].items ? 1 : 0);	/* print more details */

    code =
	ubik_Call(BUDB_DbVerify, udbHandle.uh_client, 0, &status, &orphans,
		  &host);

    if (code) {
	afs_com_err(whoami, code, "; Unable to verify database");
	return (-1);
    }

    /* verification call succeeded */

    if (status == 0)
	printf("Database OK\n");
    else
	afs_com_err(whoami, status, "; Database is NOT_OK");

    if (detail) {
	printf("Orphan blocks %d\n", orphans);

	if (!host)
	    printf("Unable to lookup host id\n");
	else {
	    hostPtr = gethostbyaddr((char *)&host, sizeof(host), AF_INET);
	    if (hostPtr == 0)
		printf("Database checker was %d.%d.%d.%d\n",
		       ((host & 0xFF000000) >> 24), ((host & 0xFF0000) >> 16),
		       ((host & 0xFF00) >> 8), (host & 0xFF));
	    else
		printf("Database checker was %s\n", hostPtr->h_name);
	}
    }
    return ((status ? -1 : 0));
}

/* deleteDump:
 * Delete a dump. If port is >= 0, it means try to delete from XBSA server
 */
deleteDump(dumpid, port, force)
     afs_uint32 dumpid;		/* The dumpid to delete */
     afs_int32 port;		/* port==-1 means don't go to butc */
     afs_int32 force;
{
    afs_int32 code = 0, tcode;
    struct budb_dumpEntry dumpEntry;
    struct rx_connection *tconn = 0;
    afs_int32 i, taskflag, xbsadump;
    statusP statusPtr = 0;
    budb_dumpsList dumps;
    afs_uint32 taskId;

    /* If the port is set, we will try to send a delete request to the butc */
    if (port >= 0) {
	tcode = bc_UpdateHosts();
	if (tcode) {
	    afs_com_err(whoami, tcode, "; Can't retrieve tape hosts");
	    ERROR(tcode);
	}

	/* Find the dump in the backup database */
	tcode = bcdb_FindDumpByID(dumpid, &dumpEntry);
	if (tcode) {
	    afs_com_err(whoami, tcode, "; Unable to locate dumpID %u in database",
		    dumpid);
	    ERROR(tcode);
	}
	xbsadump = (dumpEntry.flags & (BUDB_DUMP_ADSM | BUDB_DUMP_BUTA));

	/* If dump is to an XBSA server, connect to butc and send it
	 * the dump to delete. Butc will contact the XBSA server.
	 * The dump will not be an appended dump because XBSA butc 
	 * does not support the append option.
	 */
	if (xbsadump && dumpEntry.nVolumes) {
	    tcode = ConnectButc(bc_globalConfig, port, &tconn);
	    if (tcode)
		ERROR(tcode);

	    tcode = TC_DeleteDump(tconn, dumpid, &taskId);
	    if (tcode) {
		if (tcode == RXGEN_OPCODE)
		    tcode = BC_VERSIONFAIL;
		afs_com_err(whoami, tcode,
			"; Unable to delete dumpID %u via butc", dumpid);
		ERROR(tcode);
	    }

	    statusPtr = createStatusNode();
	    lock_Status();
	    statusPtr->taskId = taskId;
	    statusPtr->port = port;
	    statusPtr->jobNumber = bc_jobNumber();
	    statusPtr->flags |= (SILENT | NOREMOVE);	/* No msg & keep statusPtr */
	    statusPtr->flags &= ~STARTING;	/* clearstatus to examine */
	    sprintf(statusPtr->taskName, "DeleteDump");
	    unlock_Status();

	    /* Wait for task to finish */
	    taskflag = waitForTask(taskId);
	    if (taskflag & (TASK_ERROR | ABORT_DONE)) {
		afs_com_err(whoami, BUTX_DELETEOBJFAIL,
			"; Unable to delete dumpID %u via butc", dumpid);
		ERROR(BUTX_DELETEOBJFAIL);	/* the task failed */
	    }
	}
    }

  error_exit:
    if (statusPtr)
	deleteStatusNode(statusPtr);	/* Clean up statusPtr - because NOREMOVE */
    if (tconn)
	rx_DestroyConnection(tconn);	/* Destroy the connection */

    /* Remove the dump from the backup database */
    if (!code || force) {
	dumps.budb_dumpsList_len = 0;
	dumps.budb_dumpsList_val = 0;

	tcode = bcdb_deleteDump(dumpid, 0, 0, &dumps);
	if (tcode) {
	    afs_com_err(whoami, tcode,
		    "; Unable to delete dumpID %u from database", dumpid);
	    dumps.budb_dumpsList_len = 0;
	    if (!code)
		code = tcode;
	}

	/* Display the dumps that were deleted - includes appended dumps */
	for (i = 0; i < dumps.budb_dumpsList_len; i++)
	    printf("     %u%s\n", dumps.budb_dumpsList_val[i],
		   (i > 0) ? " Appended Dump" : "");
	if (dumps.budb_dumpsList_val)
	    free(dumps.budb_dumpsList_val);
    }

    return code;
}

/* bc_deleteDumpCmd
 *	Delete a specified dump from the database
 * entry:
 *	dump id - single required arg as param 0.
 */

bc_deleteDumpCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_uint32 dumpid;
    afs_int32 code = 0;
    afs_int32 rcode = 0;
    afs_int32 groupId = 0, havegroupid, sflags, noexecute;
    struct cmd_item *ti;
    afs_uint32 fromTime = 0, toTime = 0, havetime = 0;
    char *timeString;
    budb_dumpsList dumps, flags;
    int i;
    afs_int32 port = -1, dbonly = 0, force;

    /* Must specify at least one of -dumpid, -from, or -to */
    if (!as->parms[0].items && !as->parms[1].items && !as->parms[2].items
	&& !as->parms[4].items) {
	afs_com_err(whoami, 0, "Must specify at least one field");
	return (-1);
    }

    /* Must have -to option with -from option */
    if (as->parms[1].items && !as->parms[2].items) {
	afs_com_err(whoami, 0, "Must specify '-to' field with '-from' field");
	return (-1);
    }

    /* Get the time to delete from */
    if (as->parms[1].items) {	/* -from */
	timeString = concatParams(as->parms[1].items);
	if (!timeString)
	    return (-1);

	/*
	 * Now parse this string for the time to start.
	 */
	code = ktime_DateToLong(timeString, &fromTime);
	free(timeString);
	if (code) {
	    afs_com_err(whoami, 0, "Can't parse 'from' date and time");
	    afs_com_err(whoami, 0, "%s", ktime_GetDateUsage());
	    return (-1);
	}
	havetime = 1;
    }

    port = (as->parms[3].items ? getPortOffset(as->parms[3].items->data) : 0);	/* -port */
    if (as->parms[5].items)	/* -dbonly */
	port = -1;

    force = (as->parms[6].items ? 1 : 0);

    havegroupid = (as->parms[4].items ? 1 : 0);
    if (havegroupid)
	groupId = atoi(as->parms[4].items->data);

    noexecute = (as->parms[7].items ? 1 : 0);

    /* Get the time to delete to */
    if (as->parms[2].items) {	/* -to */
	timeString = concatParams(as->parms[2].items);
	if (!timeString)
	    return (-1);

	/*
	 * Now parse this string for the time to start. Simce
	 * times are at minute granularity, add 59 seconds.
	 */
	code = ktime_DateToLong(timeString, &toTime);
	free(timeString);
	if (code) {
	    afs_com_err(whoami, 0, "Can't parse 'to' date and time");
	    afs_com_err(whoami, 0, "%s", ktime_GetDateUsage());
	    return (-1);
	}
	toTime += 59;
	havetime = 1;
    }

    if (fromTime > toTime) {
	afs_com_err(whoami, 0,
		"'-from' date/time cannot be later than '-to' date/time");
	return (-1);
    }

    /* Remove speicific dump ids - if any */
    printf("The following dumps %s deleted:\n",
	   (noexecute ? "would have been" : "were"));
    for (ti = as->parms[0].items; ti != 0; ti = ti->next) {	/* -dumpid */
	dumpid = atoi(ti->data);
	if (!noexecute) {
	    code = deleteDump(dumpid, port, force);
	} else {
	    printf("     %u\n", dumpid);
	}
    }

    /*
     * Now remove dumps between to and from dates.
     */
    if (havegroupid || havetime) {
	dumps.budb_dumpsList_len = 0;
	dumps.budb_dumpsList_val = 0;
	flags.budb_dumpsList_len = 0;
	flags.budb_dumpsList_val = 0;
	sflags = 0;
	if (havegroupid)
	    sflags |= BUDB_OP_GROUPID;
	if (havetime)
	    sflags |= BUDB_OP_DATES;

	code =
	    bcdb_listDumps(sflags, groupId, fromTime, toTime, &dumps, &flags);
	if (code) {
	    afs_com_err(whoami, code,
		    "; Error while deleting dumps from %u to %u", fromTime,
		    toTime);
	    rcode = -1;
	}

	for (i = 0; i < dumps.budb_dumpsList_len; i++) {
	    if (flags.budb_dumpsList_val[i] & BUDB_OP_DBDUMP)
		continue;

	    if (!noexecute) {
		if (flags.budb_dumpsList_val[i] & BUDB_OP_APPDUMP)
		    continue;
		code = deleteDump(dumps.budb_dumpsList_val[i], port, force);
		/* Ignore code and continue */
	    } else {
		printf("     %u%s%s\n", dumps.budb_dumpsList_val[i],
		       (flags.
			budb_dumpsList_val[i] & BUDB_OP_APPDUMP) ?
		       " Appended Dump" : "",
		       (flags.
			budb_dumpsList_val[i] & BUDB_OP_DBDUMP) ?
		       " Database Dump" : "");

	    }
	}

	if (dumps.budb_dumpsList_val)
	    free(dumps.budb_dumpsList_val);
	dumps.budb_dumpsList_len = 0;
	dumps.budb_dumpsList_val = 0;
	if (flags.budb_dumpsList_val)
	    free(flags.budb_dumpsList_val);
	flags.budb_dumpsList_len = 0;
	flags.budb_dumpsList_val = 0;
    }

    return (rcode);
}

bc_saveDbCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    struct rx_connection *tconn;
    afs_int32 portOffset = 0;
    statusP statusPtr;
    afs_uint32 taskId;
    afs_int32 code;
    afs_uint32 toTime;
    char *timeString;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    if (as->parms[0].items) {
	portOffset = getPortOffset(as->parms[0].items->data);
	if (portOffset < 0)
	    return (BC_BADARG);
    }

    /* Get the time to delete to */
    if (as->parms[1].items) {
	timeString = concatParams(as->parms[1].items);
	if (!timeString)
	    return (-1);

	/*
	 * Now parse this string for the time. Since
	 * times are at minute granularity, add 59 seconds.
	 */
	code = ktime_DateToLong(timeString, &toTime);
	free(timeString);
	if (code) {
	    afs_com_err(whoami, 0, "Can't parse '-archive' date and time");
	    afs_com_err(whoami, 0, "%s", ktime_GetDateUsage());
	    return (-1);
	}
	toTime += 59;
    } else
	toTime = 0;

    code = ConnectButc(bc_globalConfig, portOffset, &tconn);
    if (code)
	return (code);

    code = TC_SaveDb(tconn, toTime, &taskId);
    if (code) {
	afs_com_err(whoami, code, "; Failed to save database");
	goto exit;
    }

    /* create status monitor block */
    statusPtr = createStatusNode();
    lock_Status();
    statusPtr->taskId = taskId;
    statusPtr->port = portOffset;
    statusPtr->jobNumber = bc_jobNumber();
    statusPtr->flags &= ~STARTING;	/* clearstatus to examine */
    sprintf(statusPtr->taskName, "SaveDb");
    unlock_Status();

  exit:
    rx_DestroyConnection(tconn);
    return (code);
}

bc_restoreDbCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    struct rx_connection *tconn;
    afs_int32 portOffset = 0;
    statusP statusPtr;
    afs_uint32 taskId;
    afs_int32 code;

    code = bc_UpdateHosts();
    if (code) {
	afs_com_err(whoami, code, "; Can't retrieve tape hosts");
	return (code);
    }

    if (as->parms[0].items) {
	portOffset = getPortOffset(as->parms[0].items->data);
	if (portOffset < 0)
	    return (BC_BADARG);
    }

    code = ConnectButc(bc_globalConfig, portOffset, &tconn);
    if (code)
	return (code);

    code = TC_RestoreDb(tconn, &taskId);
    if (code) {
	afs_com_err(whoami, code, "; Failed to restore database");
	goto exit;
    }

    /* create status monitor block */
    statusPtr = createStatusNode();
    lock_Status();
    statusPtr->taskId = taskId;
    statusPtr->port = portOffset;
    statusPtr->jobNumber = bc_jobNumber();
    statusPtr->flags &= ~STARTING;	/* clearstatus to examine */
    sprintf(statusPtr->taskName, "RestoreDb");
    unlock_Status();

  exit:
    rx_DestroyConnection(tconn);
    return (code);
}

/* ----------------------------------
 * supporting routines for database examination 
 * ----------------------------------
 */

/* structures and defines for DBLookupByVolume */

#define DBL_MAX_VOLUMES	20	/* max. for each dump */

/* dumpedVol - saves interesting information so that we can print it out
 *	later
 */

struct dumpedVol {
    struct dumpedVol *next;
    afs_int32 dumpID;
    afs_int32 initialDumpID;
    char tapeName[BU_MAXTAPELEN];
    afs_int32 level;
    afs_int32 parent;
    afs_int32 createTime;
    afs_int32 incTime;		/* actually the clone time */
};

/* -----------------------------------------
 * routines for examining the database
 * -----------------------------------------
 */

/* DBLookupByVolume
 *	Lookup the volumename in the backup database and print the results
 * entry:
 *	volumeName - volume to lookup
 */

DBLookupByVolume(volumeName)
     char *volumeName;
{
    struct budb_dumpEntry dumpEntry;
    struct budb_volumeEntry volumeEntry[DBL_MAX_VOLUMES];
    afs_int32 numEntries;
    afs_int32 tapedumpid;
    afs_int32 last, next;

    struct dumpedVol *dvptr = 0;
    struct dumpedVol *tempPtr = 0;
    afs_int32 code = 0;
    int i, pass;
    char vname[BU_MAXNAMELEN];
    char ds[50];

    for (pass = 0; pass < 2; pass++) {
	/*p */
	/* On second pass, search for backup volume */
	if (pass == 1) {
	    if (!BackupName(volumeName)) {
		strcpy(vname, volumeName);
		strcat(vname, ".backup");
		volumeName = vname;
	    } else {
		continue;
	    }
	}

	last = next = 0;
	while (next != -1) {	/*w */
	    code =
		bcdb_LookupVolume(volumeName, &volumeEntry[0], last, &next,
				  DBL_MAX_VOLUMES, &numEntries);
	    if (code)
		break;

	    /* add the volumes to the list */
	    for (i = 0; i < numEntries; i++) {	/*f */
		struct dumpedVol *insPtr, **prevPtr;

		tempPtr =
		    (struct dumpedVol *)malloc(sizeof(struct dumpedVol));
		if (!tempPtr)
		    ERROR(BC_NOMEM);

		memset(tempPtr, 0, sizeof(*tempPtr));
		tempPtr->incTime = volumeEntry[i].clone;
		tempPtr->dumpID = volumeEntry[i].dump;
		strncpy(tempPtr->tapeName, volumeEntry[i].tape,
			BU_MAXTAPELEN);

		/* check if we need to null terminate it - just for safety */
		if (strlen(volumeEntry[i].tape) >= BU_MAXTAPELEN)
		    tempPtr->tapeName[BU_MAXTAPELEN - 1] = 0;

		code = bcdb_FindDumpByID(tempPtr->dumpID, &dumpEntry);
		if (code) {
		    free(tempPtr);
		    ERROR(code);
		}

		tempPtr->initialDumpID = dumpEntry.initialDumpID;
		tempPtr->parent = dumpEntry.parent;
		tempPtr->level = dumpEntry.level;
		tempPtr->createTime = dumpEntry.created;

		/* add volume to list in reverse chronological order */
		prevPtr = &dvptr;
		insPtr = dvptr;

		while ((insPtr != 0)
		       && (insPtr->createTime > tempPtr->createTime)
		    ) {
		    prevPtr = &insPtr->next;
		    insPtr = insPtr->next;
		}

		/* now at the right place - insert the block */
		tempPtr->next = *prevPtr;
		*prevPtr = tempPtr;
	    }			/*f */

	    last = next;
	}			/*w */
    }				/*p */

    if (dvptr) {
	printf
	    ("DumpID    lvl parentID creation date     clone date       tape name\n");
	for (tempPtr = dvptr; tempPtr; tempPtr = tempPtr->next) {
	    /* For the user, the tape name is its name and initial dump id */
	    tapedumpid =
		(tempPtr->initialDumpID ? tempPtr->initialDumpID : tempPtr->
		 dumpID);

	    /* beware the static items in compactDateString */
	    compactDateString(&tempPtr->createTime, ds, 50);
	    printf("%-9d %-2d %-9d %16s", tempPtr->dumpID, tempPtr->level,
		   tempPtr->parent, ds);
	    compactDateString(&tempPtr->incTime, ds, 50);
	    printf("  %16s %s (%u)\n", ds, tempPtr->tapeName, tapedumpid);
	}
	code = 0;
    }

  error_exit:
    for (tempPtr = dvptr; tempPtr; tempPtr = dvptr) {
	dvptr = dvptr->next;
	free(tempPtr);
    }

    if (code)
	afs_com_err(whoami, code, "");
    return (code);
}

/* structures for dumpInfo */

struct volumeLink {
    struct volumeLink *nextVolume;
    struct budb_volumeEntry volumeEntry;
};

struct tapeLink {
    struct tapeLink *nextTape;
    struct budb_tapeEntry tapeEntry;
    struct volumeLink *firstVolume;
};


/* dumpInfo
 *	print information about a dump and all its tapes and volumes.
 */

afs_int32
dumpInfo(dumpid, detailFlag)
     afs_int32 dumpid;
     afs_int32 detailFlag;
{
    struct budb_dumpEntry dumpEntry;
    struct tapeLink *head = 0;
    struct tapeLink *tapeLinkPtr, *lastTapeLinkPtr;
    struct volumeLink **link, *volumeLinkPtr, *lastVolumeLinkPtr;

    budb_volumeList vl;
    afs_int32 last, next, dbTime;
    afs_int32 tapedumpid;

    int tapeNumber;
    int i;
    int dbDump;
    afs_int32 code = 0;
    char ds[50];

    extern struct udbHandleS udbHandle;

    tapeLinkPtr = 0;
    lastTapeLinkPtr = 0;
    volumeLinkPtr = 0;
    lastVolumeLinkPtr = 0;

    /* first get information about the dump */

    code = bcdb_FindDumpByID(dumpid, &dumpEntry);
    if (code)
	ERROR(code);

    /* For the user, the tape name is its name and initial dump id */
    tapedumpid = (dumpEntry.initialDumpID ? dumpEntry.initialDumpID : dumpid);

    /* Is this a database dump id or not */
    if (strcmp(dumpEntry.name, DUMP_TAPE_NAME) == 0)
	dbDump = 1;
    else
	dbDump = 0;

    /* print out the information about the dump */
    if (detailFlag) {
	printf("\nDump\n");
	printf("----\n");
	printDumpEntry(&dumpEntry);
    } else {
        time_t t = dumpEntry.created;
	if (dbDump)
	    printf("Dump: id %u, created: %s\n", dumpEntry.id,
		   ctime(&t));
 	else
	    printf("Dump: id %u, level %d, volumes %d, created: %s\n",
		   dumpEntry.id, dumpEntry.level, dumpEntry.nVolumes,
		   ctime(&t));
    }

    if (!detailFlag && (strlen(dumpEntry.tapes.tapeServer) > 0)
	&& (dumpEntry.flags & (BUDB_DUMP_ADSM | BUDB_DUMP_BUTA))) {
	printf("Backup Service: TSM: Server: %s\n",
	       dumpEntry.tapes.tapeServer);
    }

    /* now get the list of tapes */
    for (tapeNumber = dumpEntry.tapes.b; tapeNumber <= dumpEntry.tapes.maxTapes; tapeNumber++) {	/*f */
	tapeLinkPtr = (struct tapeLink *)malloc(sizeof(struct tapeLink));
	if (!tapeLinkPtr) {
	    afs_com_err(whoami, BC_NOMEM, "");
	    ERROR(BC_NOMEM);
	}

	memset(tapeLinkPtr, 0, sizeof(*tapeLinkPtr));
	code = bcdb_FindTapeSeq(dumpid, tapeNumber, &tapeLinkPtr->tapeEntry);
	if (code) {
	    code = 0;
	    free(tapeLinkPtr);
	    continue;
	}

	/* add this tape to  previous chain */
	if (lastTapeLinkPtr) {
	    lastTapeLinkPtr->nextTape = tapeLinkPtr;
	    lastTapeLinkPtr = tapeLinkPtr;
	}

	if (head == 0) {
	    head = tapeLinkPtr;
	    lastTapeLinkPtr = head;
	}

	next = 0;
	while (next != -1) {	/*wn */
	    vl.budb_volumeList_len = 0;
	    vl.budb_volumeList_val = 0;
	    last = next;

	    /* now get all the volumes in this dump. */
	    code = ubik_Call_SingleServer(BUDB_GetVolumes, udbHandle.uh_client, UF_SINGLESERVER, BUDB_MAJORVERSION, BUDB_OP_DUMPID | BUDB_OP_TAPENAME, tapeLinkPtr->tapeEntry.name,	/* tape name */
					  dumpid,	/* dumpid (not initial dumpid) */
					  0,	/* end */
					  last,	/* last */
					  &next,	/* nextindex */
					  &dbTime,	/* update time */
					  &vl);

	    if (code) {
		if (code == BUDB_ENDOFLIST) {	/* 0 volumes on tape */
		    code = 0;
		    break;
		}
		ERROR(code);
	    }

	    for (i = 0; i < vl.budb_volumeList_len; i++) {
		link = &tapeLinkPtr->firstVolume;

		volumeLinkPtr =
		    (struct volumeLink *)malloc(sizeof(struct volumeLink));
		if (!volumeLinkPtr) {
		    afs_com_err(whoami, BC_NOMEM, "");
		    ERROR(BC_NOMEM);
		}
		memset(volumeLinkPtr, 0, sizeof(*volumeLinkPtr));

		memcpy(&volumeLinkPtr->volumeEntry,
		       &vl.budb_volumeList_val[i],
		       sizeof(struct budb_volumeEntry));

		/* now insert it onto the right place */
		while ((*link != 0)
		       && (volumeLinkPtr->volumeEntry.position >
			   (*link)->volumeEntry.position)) {
		    link = &((*link)->nextVolume);
		}

		/* now link it in */
		volumeLinkPtr->nextVolume = *link;
		*link = volumeLinkPtr;
	    }

	    if (vl.budb_volumeList_val)
		free(vl.budb_volumeList_val);
	}			/*wn */
    }				/*f */

    for (tapeLinkPtr = head; tapeLinkPtr; tapeLinkPtr = tapeLinkPtr->nextTape) {
	if (detailFlag) {
	    printf("\nTape\n");
	    printf("----\n");
	    printTapeEntry(&tapeLinkPtr->tapeEntry);
	} else {
	    printf("Tape: name %s (%u)\n", tapeLinkPtr->tapeEntry.name,
		   tapedumpid);
	    printf("nVolumes %d, ", tapeLinkPtr->tapeEntry.nVolumes);
	    compactDateString(&tapeLinkPtr->tapeEntry.written, ds, 50);
	    printf("created %16s", ds);
	    if (tapeLinkPtr->tapeEntry.expires != 0) {
		compactDateString(&tapeLinkPtr->tapeEntry.expires, ds, 50);
		printf(", expires %16s", ds);
	    }
	    printf("\n\n");
	}

	/* print out all the volumes */

	/* print header for volume listing - db dumps have no volumes */
	if ((detailFlag == 0) && !dbDump)
	    printf("%4s %16s %9s %-s\n", "Pos", "Clone time", "Nbytes",
		   "Volume");

	for (volumeLinkPtr = tapeLinkPtr->firstVolume; volumeLinkPtr;
	     volumeLinkPtr = volumeLinkPtr->nextVolume) {
	    if (detailFlag) {
		printf("\nVolume\n");
		printf("------\n");
		printVolumeEntry(&volumeLinkPtr->volumeEntry);
	    } else {
		compactDateString(&volumeLinkPtr->volumeEntry.clone, ds, 50),
		    printf("%4d %s %10u %16s\n",
			   volumeLinkPtr->volumeEntry.position, ds,
			   volumeLinkPtr->volumeEntry.nBytes,
			   volumeLinkPtr->volumeEntry.name);
	    }
	}
    }

  error_exit:
    if (code)
	afs_com_err("dumpInfo", code, "; Can't get dump information");

    /* free all allocated structures */
    tapeLinkPtr = head;
    while (tapeLinkPtr) {
	volumeLinkPtr = tapeLinkPtr->firstVolume;
	while (volumeLinkPtr) {
	    lastVolumeLinkPtr = volumeLinkPtr;
	    volumeLinkPtr = volumeLinkPtr->nextVolume;
	    free(lastVolumeLinkPtr);
	}

	lastTapeLinkPtr = tapeLinkPtr;
	tapeLinkPtr = tapeLinkPtr->nextTape;
	free(lastTapeLinkPtr);
    }
    return (code);
}

int
compareDump(ptr1, ptr2)
     struct budb_dumpEntry *ptr1, *ptr2;
{
    if (ptr1->created < ptr2->created)
	return (-1);
    else if (ptr1->created > ptr2->created)
	return (1);
    return (0);
}

afs_int32
printRecentDumps(ndumps)
     int ndumps;
{
    afs_int32 code = 0;
    afs_int32 nextindex, index = 0;
    afs_int32 dbTime;
    budb_dumpList dl;
    struct budb_dumpEntry *dumpPtr;
    int i;
    char ds[50];

    extern struct udbHandleS udbHandle;
    extern compareDump();

    do {			/* while (nextindex != -1) */
	/* initialize the dump list */
	dl.budb_dumpList_len = 0;
	dl.budb_dumpList_val = 0;

	/* outline algorithm */
	code = ubik_Call(BUDB_GetDumps, udbHandle.uh_client, 0, BUDB_MAJORVERSION, BUDB_OP_NPREVIOUS, "",	/* no name */
			 0,	/* start */
			 ndumps,	/* end */
			 index,	/* index */
			 &nextindex, &dbTime, &dl);
	if (code) {
	    if (code == BUDB_ENDOFLIST)
		return 0;
	    afs_com_err("dumpInfo", code, "; Can't get dump information");
	    return (code);
	}

	/* No need to sort, it's already sorted */

	if (dl.budb_dumpList_len && (index == 0))
	    printf("%10s %10s %2s %-16s %2s %5s dump name\n", "dumpid",
		   "parentid", "lv", "created", "nt", "nvols");

	dumpPtr = dl.budb_dumpList_val;
	for (i = 1; i <= dl.budb_dumpList_len; i++) {
	    compactDateString(&dumpPtr->created, ds, 50),
		printf("%10u %10u %-2d %16s %2d %5d %s", dumpPtr->id,
		       dumpPtr->parent, dumpPtr->level, ds,
		       dumpPtr->tapes.maxTapes - dumpPtr->tapes.b + 1,
		       dumpPtr->nVolumes, dumpPtr->name);
	    if (dumpPtr->initialDumpID)	/* an appended dump */
		printf(" (%u)", dumpPtr->initialDumpID);
	    else if (dumpPtr->appendedDumpID)	/* has appended dumps */
		printf(" (%u)", dumpPtr->id);
	    printf("\n");

	    dumpPtr++;
	}

	if (dl.budb_dumpList_val)
	    free(dl.budb_dumpList_val);
	index = nextindex;
    } while (nextindex != -1);

    return (code);
}

/* bc_dumpInfoCmd 
 *	list the dumps and contens of the dumps.
 * params:
 *	as - name of tape
 *	arock -
 */
bc_dumpInfoCmd(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 dumpid;
    afs_int32 detailFlag;
    afs_int32 ndumps;
    afs_int32 code = 0;

    afs_int32 dumpInfo();

    if (as->parms[0].items) {
	if (as->parms[1].items) {
	    afs_com_err(whoami, 0,
		    "These options are exclusive - select only one");
	    return (BC_BADARG);
	}
	ndumps = atoi(as->parms[0].items->data);
	if (ndumps <= 0) {
	    afs_com_err(whoami, 0, "Must provide a positive number");
	    return -1;
	}

	code = printRecentDumps(ndumps);
    } else if (as->parms[1].items) {
	detailFlag = (as->parms[2].items ? 1 : 0);	/* 1 = detailed listing */
	dumpid = atoi(as->parms[1].items->data);
	code = dumpInfo(dumpid, detailFlag);
    } else {
	code = printRecentDumps(10);
    }

    return (code);
}
