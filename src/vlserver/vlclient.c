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
#include <sys/types.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <string.h>

#include <afs/afsutil.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <rx/rxkad.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/cmd.h>
#include <lock.h>
#include <ubik.h>
#include "vlserver.h"
#include "vlclient.h"
#ifdef AFS_RXK5
#include <rx/rxk5errors.h>
#endif

void fill_listattributes_entry();
void display_listattributes_entry();
void display_entry();
void display_entryN();
void display_update_entry();
void dump_stats();
void GetArgs();
void print_usage();
void fill_entry();
void fill_update_entry();

#define	VL_NUMBER_OPCODESX	34
static char *opcode_names[VL_NUMBER_OPCODESX] = {
    "CreateEntry",
    "DeleteEntry",
    "GetEntryByID",
    "GetEntryByName",
    "GetNewVolumeId",
    "ReplaceEntry",
    "UpdateEntry",
    "SetLock",
    "ReleaseLock",
    "ListEntry",
    "ListAttributes",
    "LinkedList",
    "GetStats",
    "Probe",
    "GetAddrs",
    "ChangeAddr",
    "CreateEntryN",
    "GetEntryByIDN",
    "GetEntryByNameN",
    "ReplaceEntryN",
    "ListEntryN",
    "ListAttributesN",
    "LinkedListN",
    "UpdateeEntryByName",
    "CreateEntryU",
    "GetEntryByIDU",
    "GetEntryByNameU",
    "ReplaceEntryU",
    "ListEntryU",
    "ListAttributesU",
    "LinkedListU",
    "RegisterAddr",
    "GetAddrsU",
    "ListAttributesN2"
};

struct Vlent {
    struct Vlent *next;
    afs_int32 rwid;
    afs_int32 roid;
    afs_int32 baid;
    char name[64];
};

#define	NVOLS	1000
#define	ALLOCNT 50000
struct Vlent *VLa[NVOLS];
#define	VHash(avol)	((avol)&(NVOLS-1))
struct Vlent *VL, *SVL;
int VLcnt = 0;
struct ubik_client *cstruct;
struct rx_connection *serverconns[MAXSERVERS];
char confdir[AFSDIR_PATH_MAX];
char *(args[50]);

struct Vlent *
GetVolume(vol, entry)
     struct vldbentry *entry;
{
    register int i;
    register struct Vlent *vl;

    if (!vol)
	return NULL;
    i = VHash(vol);
    for (vl = VLa[i]; vl; vl = vl->next) {
	if ((vl->rwid == vol && vol != entry->volumeId[0])
	    || (vl->roid == vol && vol != entry->volumeId[1])
	    || (vl->baid == vol && vol != entry->volumeId[2])) {
	    return vl;
	}
    }
    VL->rwid = entry->volumeId[0];
    VL->roid = entry->volumeId[1];
    VL->baid = entry->volumeId[2];
    strcpy(entry->name, VL->name);
    VL->next = VLa[i];
    VLa[i] = VL;
    if (VLcnt++ > ALLOCNT) {	/* XXXX FIX XXXXXXXXX */
	printf("Too many entries (> %d)\n", ALLOCNT);
	exit(1);
    }
    VL++;
    return NULL;
}

/* Almost identical's to pr_Initialize in vlserver/pruser.c */
afs_int32
vl_Initialize(int auth, char *confDir, int server, char *cellp)
{
    return ugen_ClientInit(auth?0:1, confDir, cellp, 0,
			  &cstruct, NULL, "vl_Initialize", rxkad_clear, 
			  MAXSERVERS, AFSCONF_VLDBSERVICE, 50, server,
			  htons(AFSCONF_VLDBPORT), USER_SERVICE_ID);
}

/* return host address in network byte order */
afs_int32
GetServer(char *aname)
{
    register struct hostent *th;
    afs_int32 addr;
    int b1, b2, b3, b4;
    register afs_int32 code;

    code = sscanf(aname, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    if (code == 4) {
	addr = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
	return htonl(addr);	/* convert to network order (128 in byte 0) */
    }
    th = gethostbyname(aname);
    if (!th)
	return 0;
    memcpy(&addr, th->h_addr, sizeof(addr));
    return addr;
}


static int
handleit(struct cmd_syndesc *as, void *arock)
{
    register struct cmd_item *ti;
    register afs_int32 code, server = 0, sawserver = 0;
    afs_int32 id, voltype;
    struct vldbentry entry;
    char *cmd = 0, *cellp = 0;
    struct VldbUpdateEntry updateentry;
    struct VldbListByAttributes listbyattributes;
    int noAuth = 1;		/* Default is authenticated connections */

    if (ti = as->parms[0].items)	/* -cellpath <dir> */
	strcpy(confdir, ti->data);
    if (as->parms[1].items)	/* -server */
	strcpy(confdir, AFSDIR_SERVER_ETC_DIRPATH);
    if (as->parms[2].items)	/* -noauth */
	noAuth = 0;
    if (ti = as->parms[3].items) {	/* -host */
	server = GetServer(ti->data);
	if (server == 0) {
	    printf("server '%s' not found in host table\n", ti->data);
	    exit(1);
	}
	sawserver = 1;
    }
    if (!sawserver && noAuth && (!(ti = as->parms[4].items))) {
	printf
	    ("Must also specify the -cell' option along with -host for authenticated conns\n");
	exit(1);
    }
    if (ti = as->parms[4].items) {	/* -cell */
	cellp = ti->data;
    }
    if (code = vl_Initialize(noAuth, confdir, server, cellp)) {
	printf("Couldn't initialize vldb library (code=%d).\n", code);
	exit(1);
    }

    if (as->parms[5].items) {	/* -gstats */
	vldstats stats;
	vital_vlheader vital_header;
	code = ubik_Call(VL_GetStats, cstruct, 0, &stats, &vital_header);
	if (!code)
	    dump_stats(&stats, &vital_header);
	exit(0);
    }

    while (1) {
	char line[500];
	int nargs, releasetype;
	memset(&entry, 0, sizeof(entry));
	memset(&updateentry, 0, sizeof(updateentry));
	memset(&listbyattributes, 0, sizeof(listbyattributes));
	printf("vl> ");
	if (fgets(line, 499, stdin) == NULL) {
	    printf("\n");
	    exit(0);
	} else {
	    char *oper, *vname;
	    register char **argp = args;
	    GetArgs(line, argp, &nargs);
	    oper = &argp[0][0];
	    ++argp, --nargs;
	    if (!strcmp(oper, "cr")) {
		fill_entry(&entry, argp, nargs);
		display_entry(&entry, 0);
		code = ubik_Call(VL_CreateEntry, cstruct, 0, &entry);
		printf("return code is %d\n", code);
	    } else if (!strcmp(oper, "rm")) {
		sscanf(&(*argp)[0], "%d", &id);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &voltype);
		code = ubik_Call(VL_DeleteEntry, cstruct, 0, id, voltype);
		printf("return code is %d\n", code);
	    } else if (!strcmp(oper, "re")) {
		sscanf(&(*argp)[0], "%d", &id);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &voltype);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &releasetype);
		++argp, --nargs;
		fill_entry(&entry, argp, nargs);
		display_entry(&entry, 0);
		code =
		    ubik_Call(VL_ReplaceEntry, cstruct, 0, id, voltype,
			      &entry, releasetype);
		printf("return code is %d\n", code);
	    } else if (!strcmp(oper, "up")) {
		sscanf(&(*argp)[0], "%d", &id);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &voltype);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &releasetype);
		++argp, --nargs;
		fill_update_entry(&updateentry, argp, nargs);
		display_update_entry(&updateentry, 0);
		code =
		    ubik_Call(VL_UpdateEntry, cstruct, 0, id, voltype,
			      &updateentry, releasetype);
		printf("return code is %d\n", code);
	    } else if (!strcmp(oper, "ls")) {
		afs_int32 index, count, next_index;
		for (index = 0; 1; index = next_index) {
		    memset(&entry, 0, sizeof(entry));
		    code =
			ubik_Call(VL_ListEntry, cstruct, 0, index, &count,
				  &next_index, &entry);
		    if (code) {
			printf("VL_ListEntry returned code = %d\n", code);
			break;
		    }
		    if (!next_index)
			break;
		    display_entry(&entry, 0);
		}
	    } else if (!strcmp(oper, "ldups")) {
		afs_int32 index, count, num = 0, num1 = 0, next_index;
		struct Vlent *vl1;

		VL = SVL =
		    (struct Vlent *)malloc(ALLOCNT * sizeof(struct Vlent));
		if (VL == NULL) {
		    printf("Can't allocate memory...\n");
		    exit(1);
		}
		printf("Enumerating all entries in vldb...\n");
		for (index = 0; 1; index = next_index) {
		    memset(&entry, 0, sizeof(entry));
		    code =
			ubik_Call(VL_ListEntry, cstruct, 0, index, &count,
				  &next_index, &entry);
		    if (code) {
			printf("VL_ListEntry returned code = %d\n", code);
			break;
		    }
		    if (!next_index)
			break;
		    num++;
		    if (vl1 = GetVolume(entry.volumeId[0], &entry)) {
			num1++;
			printf
			    ("Duplicate entry is found for RW vol %u: [RW %u, RO %u, BA %u, name=%s]\n",
			     entry.volumeId[0], vl1->rwid, vl1->roid,
			     vl1->baid, vl1->name);
		    }
		    if (vl1 = GetVolume(entry.volumeId[1], &entry)) {
			num1++;
			printf
			    ("Duplicate entry is found for RO vol %u: [RW %u, RO %u, BA %u, name=%s]\n",
			     entry.volumeId[1], vl1->rwid, vl1->roid,
			     vl1->baid, vl1->name);
		    }
		    if (vl1 = GetVolume(entry.volumeId[2], &entry)) {
			num1++;
			printf
			    ("Duplicate entry is found for BA vol %u: [RW %u, RO %u, BA %u, name=%s]\n",
			     entry.volumeId[2], vl1->rwid, vl1->roid,
			     vl1->baid, vl1->name);
		    }
		    /*display_entry(&entry, 0); */
		}
		printf("(%d vldb entries found - %d duplicates)\n", num,
		       num1);
	    } else if (!strcmp(oper, "checkhash")) {
		int index, count, num = 0, num1 = 0, num2 = 0, num3 =
		    0, num31 = 0, num4 = 0, num41 = 0, next_index;
		struct vldbentry tentry;

		VL = SVL =
		    (struct Vlent *)malloc(ALLOCNT * sizeof(struct Vlent));
		if (VL == NULL) {
		    printf("Can't allocate memory...\n");
		    exit(1);
		}
		printf("Volumes not found in main hash tables in vldb...\n");
		for (index = 0; 1; index = next_index) {
		    memset(&entry, 0, sizeof(entry));
		    code =
			ubik_Call(VL_ListEntry, cstruct, 0, index, &count,
				  &next_index, &entry);
		    if (code) {
			printf("VL_ListEntry returned code = %d\n", code);
			break;
		    }
		    if (!next_index)
			break;
		    num++;
		    code =
			ubik_Call(VL_GetEntryByNameO, cstruct, 0, entry.name,
				  &tentry);
		    if (code == VL_NOENT) {
			num1++;
			printf("\tVolume %s %d (not in namehash)\n",
			       entry.name, entry.volumeId[RWVOL]);
		    }
		    code =
			ubik_Call(VL_GetEntryByID, cstruct, 0,
				  entry.volumeId[RWVOL], RWVOL, &tentry);
		    if (code == VL_NOENT) {
			num2++;
			printf("\tVolume %s %d (not in rwid hash)\n",
			       entry.name, entry.volumeId[RWVOL]);
		    }
		    if (entry.volumeId[BACKVOL]) {
			code =
			    ubik_Call(VL_GetEntryByID, cstruct, 0,
				      entry.volumeId[BACKVOL], BACKVOL,
				      &tentry);
			num31++;
			if (code == VL_NOENT) {
			    num3++;
			    printf("\tVolume %s %d (not in backup id hash)\n",
				   entry.name, entry.volumeId[BACKVOL]);
			}
		    }
		    if (entry.volumeId[ROVOL]) {
			code =
			    ubik_Call(VL_GetEntryByID, cstruct, 0,
				      entry.volumeId[ROVOL], ROVOL, &tentry);
			num41++;
			if (code == VL_NOENT) {
			    num4++;
			    printf("\tVolume %s %d (not in RO id hash)\n",
				   entry.name, entry.volumeId[ROVOL]);
			}
		    }
		}
		printf
		    ("\nTotal vldb entries %d\nTotal volumes %d (%d rw, %d backup, %d ro)\n",
		     num, num + num31 + num41, num, num31, num41);
		printf
		    ("\n\t%d didn't hash properly by name\n\t%d didn't hash properly by rw volid\n",
		     num1, num2);
		printf
		    ("\t%d didn't hash properly by backup volid (out of %d)\n\t%d didn't hash properly by ro volid (out of %d)\n",
		     num3, num31, num4, num41);
	    } else if (!strcmp(oper, "fixhash")) {
		int index, count, num = 0, num1 = 0, num2 = 0, next_index, x =
		    0;
		struct vldbentry tentry;

		VL = SVL =
		    (struct Vlent *)malloc(ALLOCNT * sizeof(struct Vlent));
		if (VL == NULL) {
		    printf("Can't allocate memory...\n");
		    exit(1);
		}
		printf
		    ("Volumes not found in main hash tables in vldb will be fixed...\n");
		memset(&updateentry, 0, sizeof(updateentry));
		for (index = 0; 1; index = next_index) {
		    /* FIXME: n2 is never changed for some reason */
		    int n1 = 0, n2 = 0, n3 = 0, n4 = 0;
		    memset(&entry, 0, sizeof(entry));
		    code =
			ubik_Call(VL_ListEntry, cstruct, 0, index, &count,
				  &next_index, &entry);
		    if (code) {
			printf("VL_ListEntry returned code = %d\n", code);
			break;
		    }
		    if (!next_index)
			break;
		    num++;
		    code =
			ubik_Call(VL_GetEntryByNameO, cstruct, 0, entry.name,
				  &tentry);
		    if (code == VL_NOENT) {
			num1++;
			n1 = 1;
			updateentry.Mask = VLUPDATE_VOLNAMEHASH;
			printf("\tVolume %s %d (not in namehash)\n",
			       entry.name, entry.volumeId[RWVOL]);
			code =
			    ubik_Call(VL_UpdateEntry, cstruct, 0,
				      entry.volumeId[RWVOL], -1, &updateentry,
				      0);
			if (code) {
			    x++;
			    printf("\tFailed to update volume %s (err=%d)\n",
				   entry.name, code);
			}
		    }
		    code =
			ubik_Call(VL_GetEntryByID, cstruct, 0,
				  entry.volumeId[RWVOL], RWVOL, &tentry);
		    if (code == VL_NOENT) {
			num1++;
			num2++;
			updateentry.Mask = VLUPDATE_RWID;
			updateentry.spares3 = entry.volumeId[RWVOL];
			printf("\tVolume %s %d (not in rw id hash)\n",
			       entry.name, entry.volumeId[RWVOL]);
			code =
			    ubik_Call(VL_UpdateEntryByName, cstruct, 0,
				      entry.name, &updateentry, 0);
			if (code) {
			    printf("\tFailed to update volume %s (err=%d)\n",
				   entry.name, code);
			    x++;
			}
			x++;
		    }
		    if (entry.volumeId[BACKVOL] && !n2) {
			code =
			    ubik_Call(VL_GetEntryByID, cstruct, 0,
				      entry.volumeId[BACKVOL], BACKVOL,
				      &tentry);
			if (code == VL_NOENT) {
			    n3 = 1;
			    num1++;
			    updateentry.Mask = VLUPDATE_BACKUPID;
			    updateentry.BackupId = entry.volumeId[BACKVOL];
			    printf("\tVolume %s %d (not in backup id hash)\n",
				   entry.name, entry.volumeId[BACKVOL]);
			    code =
				ubik_Call(VL_UpdateEntry, cstruct, 0,
					  entry.volumeId[RWVOL], -1,
					  &updateentry, 0);
			    if (code) {
				printf
				    ("\tFailed to update volume %s (err=%d)\n",
				     entry.name, code);
				x++;
			    }
			}
		    }
		    if (entry.volumeId[ROVOL && !n2]) {
			code =
			    ubik_Call(VL_GetEntryByID, cstruct, 0,
				      entry.volumeId[ROVOL], ROVOL, &tentry);
			if (code == VL_NOENT) {
			    n4 = 1;
			    num1++;
			    updateentry.Mask = VLUPDATE_READONLYID;
			    updateentry.ReadOnlyId = entry.volumeId[ROVOL];
			    printf("\tVolume %s %d (not in RO id hash)\n",
				   entry.name, entry.volumeId[ROVOL]);
			    code =
				ubik_Call(VL_UpdateEntry, cstruct, 0,
					  entry.volumeId[RWVOL], -1,
					  &updateentry, 0);
			    if (code) {
				printf
				    ("\tFailed to update volume %s (err=%d)\n",
				     entry.name, code);
				x++;
			    }
			}
		    }
		}
		printf
		    ("\nTotal vldb entries found %d:\n\t%d entries didn't hash properly and are fixed except %d that need to be handled manually\n",
		     num, num1, x);
	    } else if (!strcmp(oper, "la")) {
		int nentries = 0, i;
		bulkentries entries;
		struct vldbentry *entry;

		memset(&entries, 0, sizeof(entries));
		fill_listattributes_entry(&listbyattributes, argp, nargs);
		display_listattributes_entry(&listbyattributes, 0);
		code =
		    ubik_Call(VL_ListAttributes, cstruct, 0,
			      &listbyattributes, &nentries, &entries);
		if (code) {
		    printf("VL_ListAttributes returned code = %d\n", code);
		    continue;
		}
		entry = (struct vldbentry *)entries.bulkentries_val;
		for (i = 0; i < nentries; i++, entry++)
		    display_entry(entry, 0);
		if (entries.bulkentries_val)
		    free((char *)entries.bulkentries_val);
	    } else if (!strcmp(oper, "lan2")) {
		int nentries, i, si, nsi, t = 0;
		nbulkentries entries;
		struct nvldbentry *entry;
		char name[64];

		/* The volume name to search for (supports wildcarding) */
		if (nargs > 0) {
		    strcpy(name, argp[0]);
		    ++argp, --nargs;
		} else {
		    strcpy(name, "");
		}

		fill_listattributes_entry(&listbyattributes, argp, nargs);
		display_listattributes_entry(&listbyattributes, 0);
		printf("Wildcard VolName: '%s'\n", name);

		for (si = 0; si != -1; si = nsi) {
		    nentries = 0;
		    memset(&entries, 0, sizeof(entries));
		    code =
			ubik_Call(VL_ListAttributesN2, cstruct, 0,
				  &listbyattributes, name, si, &nentries,
				  &entries, &nsi);
		    if (code) {
			printf("VL_ListAttributesN2 returned code = %d\n",
			       code);
			break;
		    }

		    t += nentries;
		    entry = (struct nvldbentry *)entries.nbulkentries_val;
		    for (i = 0; i < nentries; i++, entry++)
			display_entryN(entry, 0);
		    if (entries.nbulkentries_val)
			free((char *)entries.nbulkentries_val);
		}
		printf("--- %d volumes ---\n", t);
	    } else if (!strcmp(oper, "ln")) {
		int netries;
		vldb_list linkedvldbs;
		vldblist vllist, vllist1;

		fill_listattributes_entry(&listbyattributes, argp, nargs);
		display_listattributes_entry(&listbyattributes, 0);
		memset(&linkedvldbs, 0, sizeof(vldb_list));
		code =
		    ubik_Call(VL_LinkedList, cstruct, 0, &listbyattributes,
			      &netries, &linkedvldbs);
		if (code) {
		    printf("VL_LinkedList returned code = %d\n", code);
		    continue;
		}
		printf("Found %d entr%s\n", netries,
		       (netries == 1 ? "y" : "ies"));
		for (vllist = linkedvldbs.node; vllist; vllist = vllist1) {
		    vllist1 = vllist->next_vldb;
		    display_entry(&vllist->VldbEntry, 0);
		    free((char *)vllist);
		}
	    } else if (!strcmp(oper, "lnn")) {
		int netries;
		nvldb_list linkedvldbs;
		nvldblist vllist, vllist1;

		fill_listattributes_entry(&listbyattributes, argp, nargs);
		display_listattributes_entry(&listbyattributes, 0);
		memset(&linkedvldbs, 0, sizeof(vldb_list));
		code =
		    ubik_Call(VL_LinkedListN, cstruct, 0, &listbyattributes,
			      &netries, &linkedvldbs);
		if (code) {
		    printf("VL_LinkedList returned code = %d\n", code);
		    continue;
		}
		printf("Found %d entr%s\n", netries,
		       (netries == 1 ? "y" : "ies"));
		for (vllist = linkedvldbs.node; vllist; vllist = vllist1) {
		    vllist1 = vllist->next_vldb;
		    display_entry(&vllist->VldbEntry, 0);
		    free((char *)vllist);
		}
	    } else if (!strcmp(oper, "di")) {
		sscanf(&(*argp)[0], "%d", &id);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &voltype);
		code =
		    ubik_Call(VL_GetEntryByID, cstruct, 0, id, voltype,
			      &entry);
		display_entry(&entry, code);
		printf("return code is %d.\n", code);
	    } else if (!strcmp(oper, "rmnh")) {
		sscanf(&(*argp)[0], "%d", &id);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &voltype);
		code =
		    ubik_Call(VL_GetEntryByID, cstruct, 0, id, voltype,
			      &entry);
		display_entry(&entry, code);
		memset(&updateentry, 0, sizeof(updateentry));
		updateentry.Mask = VLUPDATE_VOLNAMEHASH;
		printf("\tRehashing namehash table for %s (%d)\n", entry.name,
		       entry.volumeId[RWVOL]);
		code =
		    ubik_Call(VL_UpdateEntry, cstruct, 0,
			      entry.volumeId[RWVOL], -1, &updateentry, 0);
		if (code) {
		    printf("\tFailed to update volume %s (err=%d)\n",
			   entry.name, code);
		}
		printf("return code is %d.\n", code);
	    } else if (!strcmp(oper, "undelete")) {
		afs_int32 index, count, next_index;

		memset(&updateentry, 0, sizeof(updateentry));
		sscanf(&(*argp)[0], "%d", &id);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &voltype);
		if (voltype < 0 && voltype > 2) {
		    printf("Illegal voltype; must be 0, 1 or 2\n");
		    continue;
		}
		printf("Searching vldb for volume %d...\n", id);
		for (index = 0; 1; index = next_index) {
		    memset(&entry, 0, sizeof(entry));
		    code =
			ubik_Call(VL_ListEntry, cstruct, 0, index, &count,
				  &next_index, &entry);
		    if (code) {
			printf("VL_ListEntry returned code = %d\n", code);
			break;
		    }
		    if (!next_index)
			break;
		    if (entry.volumeId[voltype] == id) {
			printf("\nThe current contents of the vldb for %d:\n",
			       id);
			display_entry(&entry, 0);

			if (entry.flags & VLDELETED) {
			    updateentry.Mask = VLUPDATE_FLAGS;
			    updateentry.flags = entry.flags;
			    updateentry.flags &= ~VLDELETED;
			    printf
				("\tUndeleting vldb entry for vol %d (%s)\n",
				 id, entry.name);
			    code =
				ubik_Call(VL_UpdateEntry, cstruct, 0, id, -1,
					  &updateentry, 0);
			    if (code) {
				printf
				    ("\tFailed to update volume %s (err=%d)\n",
				     entry.name, code);
			    }
			} else {
			    printf("Entry not deleted; ignored\n");
			}
			break;
		    }
		}
	    } else if (!strcmp(oper, "dn")) {
		vname = &argp[0][0];
		code =
		    ubik_Call(VL_GetEntryByNameO, cstruct, 0, vname, &entry);
		display_entry(&entry, code);
		printf("return code is %d.\n", code);
	    } else if (!strcmp(oper, "nv")) {
		int newvolid;
		sscanf(&(*argp)[0], "%d", &id);
		code =
		    ubik_Call(VL_GetNewVolumeId, cstruct, 0, id, &newvolid);
		if (!code)
		    printf("Current Max volid is (in hex):%X\n", newvolid);
		printf("return code is %d\n", code);
	    } else if (!strcmp(oper, "gs")) {
		vldstats stats;
		vital_vlheader vital_header;
		code =
		    ubik_Call(VL_GetStats, cstruct, 0, &stats, &vital_header);
		if (!code)
		    dump_stats(&stats, &vital_header);
		printf("return code is %d.\n", code);
	    } else if (!strcmp(oper, "ga")) {
		int nentries, i;
		afs_uint32 *addrp;
		bulkaddrs addrs;
		struct VLCallBack vlcb;

		addrs.bulkaddrs_val = 0;
		addrs.bulkaddrs_len = 0;
		code = ubik_Call(VL_GetAddrs, cstruct, 0, 0 /*Handle */ ,
				 0 /*spare2 */ , &vlcb,
				 &nentries, &addrs);
		if (code) {
		    printf("VL_GetAddrs returned code = %d\n", code);
		    continue;
		}
		addrp = addrs.bulkaddrs_val;
		for (i = 0; i < nentries; i++, addrp++) {
		    if ((*addrp & 0xff000000) == 0xff000000)
			printf("[0x%x %u] (special multi-homed entry)\n",
			       *addrp, *addrp);
		    else
			printf("[0x%x %u] %s\n", *addrp, *addrp,
			       hostutil_GetNameByINet(ntohl(*addrp)));
		}
		free((char *)addrs.bulkaddrs_val);
	    } else if (!strcmp(oper, "gau")) {
		int nentries, i, j;
		afs_uint32 *addrp;
		bulkaddrs addrs;
		struct VLCallBack vlcb;

		addrs.bulkaddrs_val = 0;
		addrs.bulkaddrs_len = 0;
		code = ubik_Call(VL_GetAddrs, cstruct, 0, 0 /*Handle */ ,
				 0 /*spare2 */ , &vlcb,
				 &nentries, &addrs);
		if (code) {
		    printf("VL_GetAddrs returned code = %d\n", code);
		    continue;
		}
		addrp = addrs.bulkaddrs_val;
		for (i = 0; i < nentries; i++, addrp++) {
		    if ((*addrp & 0xff000000) == 0xff000000) {
			int mhnentries, unique;
			struct in_addr hostAddr;
			afs_uint32 *mhaddrp;
			bulkaddrs mhaddrs;
			ListAddrByAttributes attrs;
			afsUUID uuid;

			printf("[0x%x %u] (special multi-homed entry)\n",
			       *addrp, *addrp);
			attrs.Mask = VLADDR_INDEX;
			mhaddrs.bulkaddrs_val = 0;
			mhaddrs.bulkaddrs_len = 0;
			attrs.index = *addrp & 0x00ffffff;

			code =
			    ubik_Call(VL_GetAddrsU, cstruct, 0, &attrs, &uuid,
				      &unique, &mhnentries, &mhaddrs);
			if (code) {
			    printf("VL_GetAddrsU returned code = %d\n", code);
			    continue;
			}
			printf
			    ("   [%d]: uuid[%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x]\n   addrunique=%d, ip address(es):\n",
			     attrs.index, uuid.time_low, uuid.time_mid,
			     uuid.time_hi_and_version,
			     uuid.clock_seq_hi_and_reserved,
			     uuid.clock_seq_low, uuid.node[0], uuid.node[1],
			     uuid.node[2], uuid.node[3], uuid.node[4],
			     uuid.node[5], unique);
			mhaddrp = mhaddrs.bulkaddrs_val;
			for (j = 0; j < mhnentries; j++) {
			    mhaddrp[j] = ntohl(mhaddrp[j]);
			    hostAddr.s_addr = mhaddrp[j];
			    printf("\t%s (%s)\n", inet_ntoa(hostAddr),
				   hostutil_GetNameByINet(mhaddrp[j]));
			}
			if (mhaddrs.bulkaddrs_val)
			    free((char *)mhaddrs.bulkaddrs_val);
		    } else {
			printf("[0x%x %u] %s\n", *addrp, *addrp,
			       hostutil_GetNameByINet(ntohl(*addrp)));
		    }
		}
		free((char *)addrs.bulkaddrs_val);
	    } else if (!strcmp(oper, "mhc")) {
		afs_int32 serveraddrs[MAXSERVERID + 1][VL_MAXIPADDRS_PERMH];
		afs_int32 serveraddrtype[MAXSERVERID + 1];
		int nentries1, nentries2, i, j, x, y, unique, found;
		afs_uint32 *addrp1, *addrp2;
		bulkaddrs addrs1, addrs2;
		struct VLCallBack vlcb;
		ListAddrByAttributes attrs;
		afsUUID uuid;
		afs_int32 base, index;

		for (i = 0; i < MAXSERVERID + 1; i++) {
		    serveraddrtype[i] = 0;
		    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++)
			serveraddrs[i][j] = 0;
		}

		/* Collect a list of all registered IP addresses */
		addrs1.bulkaddrs_val = 0;
		addrs1.bulkaddrs_len = 0;
		code =
		    ubik_Call(VL_GetAddrs, cstruct, 0, 0, 0, &vlcb,
			      &nentries1, &addrs1);
		if (code) {
		    printf("VL_GetAddrs returned code = %d\n", code);
		    continue;
		}
		addrp1 = addrs1.bulkaddrs_val;
		for (i = 0; i < nentries1; i++, addrp1++) {
		    if ((*addrp1 & 0xff000000) != 0xff000000) {
			serveraddrs[i][0] = ntohl(*addrp1);
			serveraddrtype[i] = 1;
		    } else {
			/* It's multihomed. Get all of its addresses */
			serveraddrtype[i] = 2;
			base = (*addrp1 >> 16) & 0xff;
			index = *addrp1 & 0xffff;

			addrs2.bulkaddrs_val = 0;
			addrs2.bulkaddrs_len = 0;
			attrs.Mask = VLADDR_INDEX;
			attrs.index = (base * VL_MHSRV_PERBLK) + index;
			code =
			    ubik_Call(VL_GetAddrsU, cstruct, 0, &attrs, &uuid,
				      &unique, &nentries2, &addrs2);
			if (code) {
			    printf("VL_GetAddrsU returned code = %d\n", code);
			    break;
			}

			addrp2 = addrs2.bulkaddrs_val;
			for (j = 0; j < nentries2; j++) {
			    serveraddrs[i][j] = ntohl(addrp2[j]);
			}
			free((char *)addrs2.bulkaddrs_val);
		    }

		    if (nargs) {
			if (serveraddrtype[i] == 1) {
			    printf("%u\n", serveraddrs[i][0]);
			} else {
			    printf("[");
			    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++)
				if (serveraddrs[i][j])
				    printf(" %u", serveraddrs[i][j]);
			    printf(" ]\n");
			}
		    }
		}
		free((char *)addrs1.bulkaddrs_val);

		/* Look for any duplicates */
		for (i = 0; i < MAXSERVERID + 1; i++) {
		    if (!serveraddrtype[i])
			continue;
		    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
			if (!serveraddrs[i][j])
			    continue;

			found = 0;
			for (x = i + 1; x < MAXSERVERID + 1; x++) {
			    if (!serveraddrtype[x])
				continue;
			    for (y = 0; y < VL_MAXIPADDRS_PERMH; y++) {
				if (!serveraddrs[x][y])
				    continue;
				if (serveraddrs[i][j] == serveraddrs[x][y]) {
				    serveraddrs[x][y] = 0;
				    found++;
				}
			    }
			}
			if (found) {
			    printf
				("Found %d entries of IP address %u (0x%x)\n",
				 found + 1, serveraddrs[i][j],
				 serveraddrs[i][j]);
			}
		    }
		}

	       /*----------------------------------------*/

	    } else if (!strcmp(oper, "regaddr")) {
		int i;
		afs_uint32 *addrp, tad;
		bulkaddrs addrs;
		afsUUID uuid;

		memset(&uuid, 0, sizeof(uuid));
		sscanf(&(*argp)[0], "%d", &i);
		++argp, --nargs;
		memcpy(uuid.node, &i, sizeof(i));

		if (nargs < 0 || nargs > 16) {
		    printf("Illegal # entries = %d\n", nargs);
		    continue;
		}
		addrp = (afs_uint32 *) malloc(20 * 4);
		addrs.bulkaddrs_val = addrp;
		addrs.bulkaddrs_len = nargs;
		while (nargs > 0) {
		    sscanf(&(*argp)[0], "%d", &tad);
		    *addrp++ = tad;
		    ++argp, --nargs;
		}
		code =
		    ubik_Call(VL_RegisterAddrs, cstruct, 0, &uuid,
			      0 /*spare */ , &addrs);
		if (code) {
		    printf("VL_RegisterAddrs returned code = %d\n", code);
		    continue;
		}
	    } else if (!strcmp(oper, "ca")) {
		extern struct hostent *hostutil_GetHostByName();
		struct hostent *h1, *h2;
		afs_uint32 a1, a2;

		printf("changing %s", *argp);
		h1 = hostutil_GetHostByName(&(*argp)[0]);
		if (!h1) {
		    printf("cmdebug: can't resolve address for host %s",
			   *argp);
		    continue;
		}
		memcpy(&a1, (afs_int32 *) h1->h_addr, sizeof(afs_uint32));

		++argp, --nargs;
		printf(" to %s\n", *argp);
		h2 = hostutil_GetHostByName(&(*argp)[0]);
		if (!h2) {
		    printf("cmdebug: can't resolve address for host %s",
			   *argp);
		    continue;
		}
		memcpy(&a2, (afs_int32 *) h2->h_addr, sizeof(afs_uint32));

		printf("changing 0x%x to 0x%x\n", ntohl(a1), ntohl(a2));
		code =
		    ubik_Call(VL_ChangeAddr, cstruct, 0, ntohl(a1),
			      ntohl(a2));
		if (code) {
		    printf("VL_ChangeAddr returned code = %d\n", code);
		    continue;
		}
	    } else if (!strcmp(oper, "caid")) {
		afs_uint32 a1, a2;

		sscanf(&(*argp)[0], "%d", &a1);
		printf("changing %d (0x%x)", a1, a1);
		++argp, --nargs;
		sscanf(&(*argp)[0], "%d", &a2);
		printf(" to %d (0x%x)\n", a2, a2);
		code = ubik_Call(VL_ChangeAddr, cstruct, 0, a1, a2);
		if (code) {
		    printf("VL_ChangeAddr returned code = %d\n", code);
		    continue;
		}
	    } else if ((!strcmp(oper, "?")) || !strcmp(oper, "h"))
		print_usage();
	    else if ((!strcmp(oper, "q")) || !strcmp(oper, "quit"))
		exit(0);
	    else {
		printf("Unknown oper!\n");
	    }
	}
    }
}


#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register struct cmd_syndesc *ts;
    afs_int32 code;

#ifdef AFS_RXK5
    initialize_RXK5_error_table();
#endif

    strcpy(confdir, AFSDIR_CLIENT_ETC_DIRPATH);
    ts = cmd_CreateSyntax("initcmd", handleit, NULL, "initialize the program");
    cmd_AddParm(ts, "-cellpath", CMD_LIST, CMD_OPTIONAL,
		"Cell configuration directory");
    cmd_AddParm(ts, "-server", CMD_LIST, CMD_OPTIONAL,
		"Use the cell config in /usr/afs/etc (default /usr/vice/etc)");
    cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL,
		"Run it without authentication");
    cmd_AddParm(ts, "-host", CMD_LIST, CMD_OPTIONAL,
		"vldb server to talk to");
    cmd_AddParm(ts, "-cell", CMD_LIST, CMD_OPTIONAL,
		"cellname '-host' belongs to (required for auth conns)");
    cmd_AddParm(ts, "-getstats", CMD_FLAG, CMD_OPTIONAL,
		"print vldb statistics (non interactive)");
    code = cmd_Dispatch(argc, argv);
    exit(code);
}


void
fill_entry(entry, argp, nargs)
     struct vldbentry *entry;
     char **argp;
     int nargs;
{
    char *name;
    int i;

    name = &argp[0][0];
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->spares3);
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->nServers);
    strcpy(entry->name, name);
    for (i = 0; i < entry->nServers; i++) {
	++argp, --nargs;
	sscanf(&(*argp)[0], "%u", &entry->serverNumber[i]);
    }
    for (i = 0; i < entry->nServers; i++) {
	++argp, --nargs;
	sscanf(&(*argp)[0], "%d", &entry->serverPartition[i]);
    }
    for (i = 0; i < entry->nServers; i++) {
	++argp, --nargs;
	sscanf(&(*argp)[0], "%d", &entry->serverFlags[i]);
    }
    for (i = 0; i < MAXTYPES; i++) {
	++argp, --nargs;
	sscanf(&(*argp)[0], "%d", &entry->volumeId[i]);
    }
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->flags);
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->cloneId);
}

void
fill_update_entry(entry, argp, nargs)
     struct VldbUpdateEntry *entry;
     char **argp;
     int nargs;
{
    int i, Mask;
    char *name;

    entry->Mask = 0;
    name = &argp[0][0];
    if (strcmp(name, "null")) {
	strcpy(entry->name, name);
	entry->Mask |= VLUPDATE_VOLUMENAME;
    }
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->flags);
    if (entry->flags != -1)
	entry->Mask |= VLUPDATE_FLAGS;
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->cloneId);
    if (entry->flags != -1)
	entry->Mask |= VLUPDATE_CLONEID;
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->ReadOnlyId);
    if (entry->ReadOnlyId != -1)
	entry->Mask |= VLUPDATE_READONLYID;
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->BackupId);
    if (entry->BackupId != -1)
	entry->Mask |= VLUPDATE_BACKUPID;
    ++argp, --nargs;
    sscanf(&(*argp)[0], "%d", &entry->nModifiedRepsites);
    if (entry->nModifiedRepsites != -1)
	entry->Mask |= VLUPDATE_REPSITES;
    for (i = 0; i < entry->nModifiedRepsites; i++) {
	++argp, --nargs;
	sscanf(&(*argp)[0], "%x", &Mask);
	++argp, --nargs;
	sscanf(&(*argp)[0], "%u", &entry->RepsitesTargetServer[i]);
	++argp, --nargs;
	sscanf(&(*argp)[0], "%d", &entry->RepsitesTargetPart[i]);
	if (Mask & VLUPDATE_REPS_DELETE)
	    entry->RepsitesMask[i] |= VLUPDATE_REPS_DELETE;
	if (Mask & VLUPDATE_REPS_MODSERV) {
	    ++argp, --nargs;
	    sscanf(&(*argp)[0], "%u", &entry->RepsitesNewServer[i]);
	    entry->RepsitesMask[i] |= VLUPDATE_REPS_MODSERV;
	} else if (Mask & VLUPDATE_REPS_MODPART) {
	    ++argp, --nargs;
	    sscanf(&(*argp)[0], "%d", &entry->RepsitesNewPart[i]);
	    entry->RepsitesMask[i] |= VLUPDATE_REPS_MODPART;
	} else if (Mask & VLUPDATE_REPS_MODFLAG) {
	    ++argp, --nargs;
	    sscanf(&(*argp)[0], "%d", &entry->RepsitesNewFlags[i]);
	    entry->RepsitesMask[i] |= VLUPDATE_REPS_MODFLAG;
	} else if (Mask & VLUPDATE_REPS_ADD) {
	    ++argp, --nargs;
	    sscanf(&(*argp)[0], "%u", &entry->RepsitesNewServer[i]);
	    ++argp, --nargs;
	    sscanf(&(*argp)[0], "%d", &entry->RepsitesNewPart[i]);
	    ++argp, --nargs;
	    sscanf(&(*argp)[0], "%d", &entry->RepsitesNewFlags[i]);
	    entry->RepsitesMask[i] |= VLUPDATE_REPS_ADD;
	}
    }
}

void
fill_listattributes_entry(entry, argp, nargs)
     struct VldbListByAttributes *entry;
     char **argp;
     int nargs;
{
    entry->Mask = 0;

    if (nargs <= 0)
	return;
    entry->server = ntohl(GetServer(argp[0]));
    sscanf(&(*argp)[0], "%d", &entry->server);
    if (entry->server != 0)
	entry->Mask |= VLLIST_SERVER;
    ++argp, --nargs;

    if (nargs <= 0)
	return;
    sscanf(&(*argp)[0], "%d", &entry->partition);
    if (entry->partition != -1)
	entry->Mask |= VLLIST_PARTITION;
    ++argp, --nargs;

    if (nargs <= 0)
	return;
    sscanf(&(*argp)[0], "%d", &entry->volumeid);
    if (entry->volumeid != -1)
	entry->Mask |= VLLIST_VOLUMEID;
    ++argp, --nargs;

    if (nargs <= 0)
	return;
    sscanf(&(*argp)[0], "%d", &entry->flag);
    if (entry->flag != -1)
	entry->Mask |= VLLIST_FLAG;
}

void
display_listattributes_entry(entry, error)
     struct VldbListByAttributes *entry;
     int error;
{
    if (error)
	return;
    printf("\nList entry values (Mask=%x)\n", entry->Mask);
    if (entry->Mask & VLLIST_SERVER)
	printf("\tServer: %d.%d.%d.%d\n", (entry->server >> 24) & 0xff,
	       (entry->server >> 16) & 0xff, (entry->server >> 8) & 0xff,
	       (entry->server) & 0xff);
    if (entry->Mask & VLLIST_PARTITION)
	printf("\tPartition: %d\n", entry->partition);
    if (entry->Mask & VLLIST_VOLUMEID)
	printf("\tVolumeId: %u\n", entry->volumeid);
    if (entry->Mask & VLLIST_FLAG)
	printf("\tFlag: %x\n", entry->flag);
}


#define	volumetype_string(type) (type == RWVOL? "read/write":type == ROVOL? "readonly":type == BACKVOL? "backup":"unknown")

void
display_entry(entry, error)
     struct vldbentry *entry;
     int error;
{
    int i;

    if (error)
	return;
    printf("\nEntry for volume name: %s, volid=%u (flags=%X) are:\n",
	   entry->name, entry->volumeId[RWVOL], entry->flags);
    printf("ParentID=%u, ReadOnlyID=%u, backupID=%u, CloneId=%u ",
	   entry->volumeId[0], entry->volumeId[1], entry->volumeId[2],
	   entry->cloneId);
    printf("nServers=%d\n", entry->nServers);
    printf("ServerNumber\tServerPart\tserverFlag\n");
    for (i = 0; i < entry->nServers; i++)
	printf("%12u\t%10d\t%10x\n", entry->serverNumber[i],
	       entry->serverPartition[i], entry->serverFlags[i]);
}

void
display_entryN(entry, error)
     struct nvldbentry *entry;
     int error;
{
    int i, et, ei;

    if (error)
	return;
    printf("\nEntry for volume name: %s, volid=%u (flags=%X) are:\n",
	   entry->name, entry->volumeId[RWVOL], entry->flags);
    printf("ParentID=%u, ReadOnlyID=%u, backupID=%u, CloneId=%u ",
	   entry->volumeId[0], entry->volumeId[1], entry->volumeId[2],
	   entry->cloneId);
    printf("nServers=%d\n", entry->nServers);
    printf("ServerNumber\tServerPart\tserverFlag\n");
    ei = entry->matchindex & 0xffff;
    et = (entry->matchindex >> 16) & 0xffff;
    for (i = 0; i < entry->nServers; i++) {
	printf("%12u\t%10d\t%10x", entry->serverNumber[i],
	       entry->serverPartition[i], entry->serverFlags[i]);
	if (i == ei) {
	    printf(" <--- %s", (et == 4) ? "RW" : ((et == 8) ? "BK" : "RO"));
	}
	printf("\n");
    }
}

void
display_update_entry(entry, error)
     struct VldbUpdateEntry *entry;
     int error;
{
    int i;

    if (error)
	return;
    printf("\nUpdate entry values (Mask=%x)\n", entry->Mask);
    if (entry->Mask & VLUPDATE_VOLUMENAME)
	printf("\tNew name: %s\n", entry->name);
    if (entry->Mask & VLUPDATE_FLAGS)
	printf("\tNew flags: %X\n", entry->flags);
    if (entry->Mask & VLUPDATE_CLONEID)
	printf("\tNew CloneId: %X\n", entry->cloneId);
    if (entry->Mask & VLUPDATE_READONLYID)
	printf("\tNew RO id: %D\n", entry->ReadOnlyId);
    if (entry->Mask & VLUPDATE_BACKUPID)
	printf("\tNew BACKUP id: %D\n", entry->BackupId);
    if (entry->Mask & VLUPDATE_REPSITES) {
	printf("\tRepsites info:\n");
	printf("\tFlag\tTServer\tTPart\tNServer\tNPart\tNFlag\n");
	for (i = 0; i < entry->nModifiedRepsites; i++) {
	    printf("\t%4x\t%7U\t%5d", entry->RepsitesMask[i],
		   entry->RepsitesTargetServer[i],
		   entry->RepsitesTargetPart[i]);
	    if ((entry->RepsitesMask[i] & VLUPDATE_REPS_ADD)
		|| (entry->RepsitesMask[i] & VLUPDATE_REPS_MODSERV))
		printf("\t%7U", entry->RepsitesNewServer[i]);
	    else
		printf("\t-------");
	    if ((entry->RepsitesMask[i] & VLUPDATE_REPS_ADD)
		|| (entry->RepsitesMask[i] & VLUPDATE_REPS_MODPART))
		printf("\t%5d", entry->RepsitesNewPart[i]);
	    else
		printf("\t-----");
	    if ((entry->RepsitesMask[i] & VLUPDATE_REPS_ADD)
		|| (entry->RepsitesMask[i] & VLUPDATE_REPS_MODFLAG))
		printf("\t%5x\n", entry->RepsitesNewFlags[i]);
	    else
		printf("\t-----\n");
	}
    }
}

void
dump_stats(stats, vital_header)
     vldstats *stats;
     vital_vlheader *vital_header;
{
    int i;
    char strg[30];
    time_t start_time = stats->start_time;

    strncpy(strg, ctime(&start_time), sizeof(strg));
    strg[strlen(strg) - 1] = 0;
    printf("Dynamic statistics stats (starting time: %s):\n", strg);
    printf("OpcodeName\t# Requests\t# Aborts\n");
    for (i = 0; i < VL_NUMBER_OPCODESX; i++)
	printf("%10s\t%10d\t%8d\n", opcode_names[i], stats->requests[i],
	       stats->aborts[i]);
    printf("\nVldb header stats (version=%d)\n",
	   ntohl(vital_header->vldbversion));
    printf("headersize=%d, allocs=%d, frees=%d, MaxVolid=%X\n",
	   ntohl(vital_header->headersize), ntohl(vital_header->allocs),
	   ntohl(vital_header->frees), ntohl(vital_header->MaxVolumeId));
    for (i = 0; i < MAXTYPES; i++)
	printf("total %s entries=%d\n", volumetype_string(i),
	       ntohl(vital_header->totalEntries[i]));
}

void
GetArgs(line, args, nargs)
     register char *line;
     register char **args;
     register int *nargs;
{
    *nargs = 0;
    while (*line) {
	register char *last = line;
	while (*line == ' ')
	    line++;
	if (*last == ' ')
	    *last = 0;
	if (!*line)
	    break;
	*args++ = line, (*nargs)++;
	while (*line && *line != ' ')
	    line++;
    }
}

void
print_usage()
{
    printf("Valid Commands:\n");

    printf("   CreateEntry:\n");
    printf
	("\tcr <vname> <vtype> <#S> <Saddr1>.<Saddrn> <Spart1>.<Spartn> <Sflag1>.<Sflagn> <Volid1-3> <flag>\n");

    printf("   DeleteEntry:\n");
    printf("\trm <volid> <voltype>\n");

    printf("   ReplaceEntry:\n");
    printf("\tre <volid> <voltype> <New vldb entry ala 'cr'>\n");

    printf("   UpdateEntry:\n");
    printf
	("\tup <volid> <voltype> <vname> <vtype> <#AddSer> [<Saddr1>.<Saddrn> <Spart1>.<Spartn> <Sflag1>.<Sflagn>] <Volid1-3> <flag>\n");

    printf("   ListEntry:\n");
    printf("\tls\n");

    printf("   Find duplicate entries of a volume\n");
    printf("\tldups\n");

    printf("   For each vlentry, find it by name, RW id, BK id, and RO id\n");
    printf("\tcheckhash\n");

    printf
	("   UpdateEntry (update the volname, RW id, BK id, RO id hashes):\n");
    printf("\tfixhash\n");

    printf("   ListAttributes:\n");
    printf("\tla [server] [partition] [volumeid] [flag]\n");

    printf("   ListAttributesN2:\n");
    printf("\tlan2 [volname] [server] [partition] [volumeid] [flag]\n");

    printf("   GetEntryByID:\n");
    printf("\tdi <volid> <voltype>\n");

    printf("   UpdateEntry (refresh namehash table):\n");
    printf("\trmnh <volid> <voltype>\n");

    printf("   GetEntryByName:\n");
    printf("\tdn <volname> <voltype>\n");

    printf("   UpdateEntry (undelete a vol entry):\n");
    printf("\tundelete <volid> <voltype>\n");
/*
 *  printf("  LinkedList\n");
 *  printf("\t:ln [server] [partition] [volumeid] [flag]\n");
 *
 *  printf("  LinkedListN\n");
 *  printf("\t:lnn [server] [partition] [volumeid] [flag]\n");
 */
    printf("   GetNewVoumeId:\n");
    printf("\tnv <bump-count>\n");

    printf("   GetStats:\n");
    printf("\tgs\n");

    printf("   ChangeAddr:\n");
    printf("\tca <oldmachname> <newmachname>\n");

/*
 *  printf("   ChangeAddr\n");
 *  printf("\t:caid <oldaddr> <newaddr>\n");
 */
    printf("   GetAddrs:\n");
    printf("\tga\n");

    printf("   GetAddrsU:\n");
    printf("\tgau\n");

    printf("   RegisterAddrs:\n");
    printf("\tregaddr uuidNumber <ip1 .. ipn>\n");

    printf("\tmisc: q, quit, ?, h\n");
}
