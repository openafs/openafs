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

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <string.h>
#include <afs/stds.h>
#include <afs/com_err.h>
#include <afs/cellconfig.h>
#include "ptclient.h"
#include "pterror.h"
#include <afs/afsutil.h>


afs_int32 security = 0;
char confdir[AFSDIR_PATH_MAX];

char *whoami;

#ifndef AFS_PTHREAD_ENV
extern struct ubik_client *pruclient;
#endif

static int ignoreExist = 0;
static char line[256];
static char *lineProgress;

#define WHITESPACE " \t\n"

#ifndef AFS_PTHREAD_ENV
int
osi_audit()
{
/* OK, this REALLY sucks bigtime, but I can't tell who is calling
 * afsconf_CheckAuth easily, and only *SERVERS* should be calling osi_audit
 * anyway.  It's gonna give somebody fits to debug, I know, I know.
 */
    return 0;
}
#endif /* !AFS_PTHREAD_ENV */

int
GetToken(format, l)
     char *format;
     afs_int32 *l;
{
    int c;

    *l = 0;
    if (lineProgress == 0)
	lineProgress = line;
    c = sscanf(lineProgress, format, l);
    if (c != 1)
	return -1;
    /* skip the white space */
    lineProgress += strspn(lineProgress, WHITESPACE);
    /* skip to end of token */
    lineProgress = strpbrk(lineProgress, WHITESPACE);
    return 0;
}

#define GetInt32(l) GetToken ("%d", l)
#define GetXInt32(l) GetToken ("%x", l)

int
GetString(s, slen)
     char *s;
     int slen;
{
    char *beg;
    int l;
    int code;

    if (lineProgress == 0)
	lineProgress = line;
    /* skip the white space */
    lineProgress += strspn(lineProgress, WHITESPACE);

    /* check for quoted string and find end */
    beg = lineProgress;
    if (*beg == '"') {
	l = strcspn(++beg, "\"");
	if (l == strlen(beg))
	    return -1;		/* unbalanced quotes */
	lineProgress = beg + l + 1;
    } else {
	l = strcspn(beg, WHITESPACE);
	lineProgress += l;
    }
    if (l >= slen) {		/* don't return too much */
	code = -1;
	l = slen - 1;
    } else
	code = 0;

    strncpy(s, beg, l);
    s[l] = 0;			/* null termination */
    return code;
}

int
CodeOk(code)
     afs_int32 code;
{
    if (!ignoreExist)
	return code;
    return code && (code != PREXIST) && (code != PRIDEXIST);
}

int
PrintEntry(ea, e, indent)
     afs_int32 ea;
     struct prentry *e;
     int indent;
{
    /* handle screwed up versions of DumpEntry */
    if (e->flags & PRCONT) {
	afs_int32 id = *(afs_int32 *) (e->name);
	if ((id != PRBADID) && ((id > (1 << 24)) || (id < -(1 << 24)))) {
	    /* assume server incorrectly swapped these bytes... */
	    int i = 0;
	    while (i < sizeof(e->name)) {
		char temp;
		temp = e->name[i];
		e->name[i] = e->name[i + 3];
		e->name[i + 3] = temp;
		temp = e->name[i + 1];
		e->name[i + 1] = e->name[i + 2];
		e->name[i + 2] = temp;
		i += 4;
	    }
	}
    }
    return pr_PrintEntry(stdout, /*host order */ 1, ea, e, indent);
}

#ifndef AFS_PTHREAD_ENV

/* main program */

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register afs_int32 code;
    char op[8];
    char name[PR_MAXNAMELEN];
    afs_int32 id, oid = ANONYMOUSID, gid;
    afs_int32 pos;
    int i;
    struct prentry entry;
    prlist alist;
    idlist lid;
    namelist lnames;
    struct hostent *hostinfo;
    struct in_addr *hostaddr;
    afs_int32 *ptr;
    char *foo;
    afs_int32 over;
    char *cell;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    whoami = argv[0];

    initialize_PT_error_table();

    strcpy(confdir, AFSDIR_CLIENT_ETC_DIRPATH);
    cell = 0;
    i = 1;
    while (i < argc) {
	int arglen = strlen(argv[i]);
	char arg[256];
	lcstring(arg, argv[i], sizeof(arg));
#define IsArg(a) (strncmp (arg,a, arglen) == 0)
	if (IsArg("-testconfdir"))
	    strncpy(confdir, argv[++i], sizeof(confdir));
	else if (IsArg("client"))
	    strncpy(confdir, AFSDIR_CLIENT_ETC_DIRPATH, sizeof(confdir));
	else if (IsArg("server"))
	    strncpy(confdir, AFSDIR_SERVER_ETC_DIRPATH, sizeof(confdir));
	else if (IsArg("0") || IsArg("1") || IsArg("2"))
	    security = atoi(argv[i]);
	else if (IsArg("-ignoreexist"))
	    ignoreExist++;
	else if (IsArg("-cell"))
	    cell = argv[++i];
	else {
	    printf
		("Usage is: 'prclient [-testconfdir <dir> | server | client] [0 | 1 | 2] [-ignoreExist] [-cell <cellname>]\n");
	    exit(1);
	}
	i++;
    }

    printf("Using CellServDB file in %s\n", confdir);
    if (security == 0)
	printf("Making unauthenticated connection to prserver\n");

    code = pr_Initialize(security, confdir, cell);
    if (code) {
	com_err(whoami, code, "Couldn't initialize protection library");
	exit(1);
    }

    while (1) {
	char *s;

	printf("pr> ");
	s = fgets(line, sizeof(line), stdin);
	if (s == NULL)
	    break;
	lineProgress = 0;

	code = GetString(op, sizeof(op));
	if (code) {
	    com_err(whoami, PRBADARG,
		    "error reading opcode in line '%s', got '%.*s'", line,
		    sizeof(op), op);
	    exit(1);
	}
	if (strlen(op) == 0)
	    continue;		/* no input */

	if (!strcmp(op, "cr")) {
	    if (GetString(name, sizeof(name)) || GetInt32(&id)
		|| GetInt32(&oid))
		code = PRBADARG;
	    /* use ubik_Call to do the work, finding an up server and handling
	     * the job of finding a sync site, if need be */
	    else
		code = ubik_PR_INewEntry(pruclient, 0, name, id, oid);
	    if (CodeOk(code))
		com_err(whoami, code, "on %s %s %d %d", op, name, id, oid);
	} else if (!strcmp(op, "sf")) {
	    afs_int32 mask, access, gq, uq;
	    if (GetInt32(&id) || GetXInt32(&mask) || GetXInt32(&access)
		|| GetInt32(&gq) || GetInt32(&uq))
		code = PRBADARG;
	    else
		code =
		    ubik_PR_SetFieldsEntry(pruclient, 0, id, mask,
			      access, gq, uq, 0, 0);
	    if (CodeOk(code))
		com_err(whoami, code, "on %s %d %x %x %d %d", op, id, mask,
			access, gq, uq);
	} else if (!strcmp(op, "ce")) {
	    char newname[PR_MAXNAMELEN];
	    afs_int32 newid;
	    if (GetInt32(&id) || GetString(newname, sizeof(newname))
		|| GetInt32(&oid) || GetInt32(&newid))
		code = PRBADARG;
	    else
		code =
		    ubik_PR_ChangeEntry(pruclient, 0, id, newname, oid,
			      newid);
	    if (CodeOk(code))
		com_err(whoami, code, "on %s %d %s %d %d", op, id, newname,
			oid, newid);
	} else if (!strcmp(op, "wh")) {
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code = ubik_PR_WhereIsIt(pruclient, 0, id, &pos);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    else
		printf("location %d\n", pos);
	} else if (!strcmp(op, "du")) {
	    memset(&entry, 0, sizeof(entry));
	    /* scanf("%d",&pos); */
	    if (GetInt32(&pos))
		code = PRBADARG;
	    else
		code = ubik_PR_DumpEntry(pruclient, 0, pos, &entry);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS) {
		PrintEntry(pos, &entry, /*indent */ 0);
#if 0
		printf("The contents of the entry for %d are:\n", entry.id);
		printf("flags %d next %d\n", entry.flags, entry.next);
		printf("Groups (or members) \n");
		for (i = 0; i < PRSIZE; i++)
		    printf("%d\n", entry.entries[i]);
		printf("nextID %d nextname %d name %s\n", entry.nextID,
		       entry.nextName, entry.name);
		printf("owner %d creator %d\n", entry.owner, entry.creator);
#endif
	    }
	} else if (!strcmp(op, "add") || !strcmp(op, "au")) {
	    /* scanf("%d %d",&id,&gid); */
	    if (GetInt32(&id) || GetInt32(&gid))
		code = PRBADARG;
	    else
		code = ubik_PR_AddToGroup(pruclient, 0, id, gid);
	    if (CodeOk(code))
		com_err(whoami, code, "on %s %d %d", op, id, gid);
	} else if (!strcmp(op, "iton")) {
	    lid.idlist_val = (afs_int32 *) malloc(20 * sizeof(afs_int32));
	    ptr = lid.idlist_val;
	    lid.idlist_len = 0;
	    foo = line;
	    skip(&foo);
	    while ((lid.idlist_len < 20) && (sscanf(foo, "%d", ptr) != EOF)) {
		lid.idlist_len++;
		skip(&foo);
		ptr++;
	    }
	    if (*foo) {
		fprintf(stderr, "too many values specified; max is %d\n", 20);
	    }
	    lnames.namelist_val = 0;
	    lnames.namelist_len = 0;
	    code = ubik_PR_IDToName(pruclient, 0, &lid, &lnames);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS) {
		for (i = 0; i < lnames.namelist_len; i++) {
		    printf("id %d name %s\n", lid.idlist_val[i],
			   lnames.namelist_val[i]);
		}
		free(lnames.namelist_val);
	    }
	    free(lid.idlist_val);
	    lid.idlist_val = 0;
	    lid.idlist_len = 0;
	} else if (!strcmp(op, "ntoi")) {
	    lnames.namelist_val =
		(prname *) malloc(PR_MAXLIST * PR_MAXNAMELEN);
	    lnames.namelist_len = 0;
	    foo = line;
	    skip(&foo);
	    for (i = 0; ((lnames.namelist_len < PR_MAXLIST)
			 && (sscanf(foo, "%s", lnames.namelist_val[i]) !=
			     EOF)); i++) {
		lnames.namelist_len++;
		skip(&foo);
	    }
	    if (*foo) {
		fprintf(stderr, "too many values specified; max is %d\n",
			PR_MAXLIST);
	    }
	    lid.idlist_val = 0;
	    lid.idlist_len = 0;
	    code = ubik_PR_NameToID(pruclient, 0, &lnames, &lid);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS) {
		for (i = 0; i < lid.idlist_len; i++)
		    printf("name %s id %d\n", lnames.namelist_val[i],
			   lid.idlist_val[i]);
		free(lid.idlist_val);
	    }
	    free(lnames.namelist_val);
	    lnames.namelist_val = 0;
	    lnames.namelist_len = 0;
	} else if (!strcmp(op, "del")) {
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code = ubik_PR_Delete(pruclient, 0, id);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	} else if (!strcmp(op, "dg")) {
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code = ubik_PR_Delete(pruclient, 0, id);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	} else if (!strcmp(op, "rm")) {
	    /* scanf("%d %d",&id,&gid); */
	    if (GetInt32(&id) || GetInt32(&gid))
		code = PRBADARG;
	    else
		code = ubik_PR_RemoveFromGroup(pruclient, 0, id, gid);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	}
#if defined(SUPERGROUPS)
	else if (!strcmp(op, "lsg")) {
	    alist.prlist_len = 0;
	    alist.prlist_val = 0;
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code =
		    ubik_PR_ListSuperGroups(pruclient, 0, id, &alist,
			      &over);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS) {
		ptr = alist.prlist_val;
		if (over) {
		    printf("Number of groups greater than PR_MAXGROUPS!\n");
		    printf("Excess of %d.\n", over);
		}
		for (i = 0; i < alist.prlist_len; i++, ptr++)
		    printf("%d\n", *ptr);
		free(alist.prlist_val);
		alist.prlist_len = 0;
		alist.prlist_val = 0;
	    }
	}
#endif /* SUPERGROUPS */
	else if (!strcmp(op, "l")) {
	    alist.prlist_len = 0;
	    alist.prlist_val = 0;
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code = ubik_PR_GetCPS(pruclient, 0, id, &alist, &over);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS) {
		ptr = alist.prlist_val;
		if (over) {
		    printf("Number of groups greater than PR_MAXGROUPS!\n");
		    printf("Excess of %d.\n", over);
		}
		for (i = 0; i < alist.prlist_len; i++, ptr++)
		    printf("%d\n", *ptr);
		free(alist.prlist_val);
		alist.prlist_len = 0;
		alist.prlist_val = 0;
	    }
	} else if (!strcmp(op, "lh")) {
	    alist.prlist_len = 0;
	    alist.prlist_val = 0;
	    /* scanf("%d",&id); */
	    if (GetString(name, sizeof(name)))
		code = PRBADARG;
	    else if (!(hostinfo = gethostbyname(name)))
		code = PRBADARG;
	    else {
		hostaddr = hostinfo->h_addr_list[0];
		id = ntohl(hostaddr->s_addr);
		code =
		    ubik_PR_GetHostCPS(pruclient, 0, id, &alist, &over);
	    }
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS) {
		ptr = alist.prlist_val;
		if (over) {
		    printf("Number of groups greater than PR_MAXGROUPS!\n");
		    printf("Excess of %d.\n", over);
		}
		for (i = 0; i < alist.prlist_len; i++, ptr++)
		    printf("%d\n", *ptr);
		free(alist.prlist_val);
		alist.prlist_len = 0;
		alist.prlist_val = 0;
	    }
	}
#if defined(SUPERGROUPS)
	else if (!strcmp(op, "m")) {
	    alist.prlist_len = 0;
	    alist.prlist_val = 0;
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code =
		    ubik_PR_ListElements(pruclient, 0, id, &alist,
			      &over);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS) {
		ptr = alist.prlist_val;
		if (over) {
		    printf("Number of groups greater than PR_MAXGROUPS!\n");
		    printf("Excess of %d.\n", over);
		}
		for (i = 0; i < alist.prlist_len; i++, ptr++)
		    printf("%d\n", *ptr);
		free(alist.prlist_val);
		alist.prlist_len = 0;
		alist.prlist_val = 0;
	    }
	}
#endif /* SUPERGROUPS */
	else if (!strcmp(op, "nu")) {
	    /* scanf("%s",name); */
	    if (GetString(name, sizeof(name)))
		code = PRBADARG;
	    else
		code = pr_CreateUser(name, &id);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS)
		printf("Id is %d.\n", id);
	} else if (!strcmp(op, "ng")) {
	    /* scanf("%s",name); */
	    if (GetString(name, sizeof(name)))
		code = PRBADARG;
	    else
		code = ubik_PR_NewEntry(pruclient, 0, name, 1, oid, &id);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS)
		printf("Id is %d.\n", id);
	} else if (!strcmp(op, "lm")) {
	    code = ubik_PR_ListMax(pruclient, 0, &id, &gid);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS)
		printf("Max user id is %d, max (really min) group is %d.\n",
		       id, gid);
	} else if (!strcmp(op, "smu")) {
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code = ubik_PR_SetMax(pruclient, 0, id, 0);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	} else if (!strcmp(op, "smg")) {
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code = ubik_PR_SetMax(pruclient, 0, id, 1);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	} else if (!strcmp(op, "sin")) {
	    /* scanf("%d",&id); */
	    if (GetInt32(&id))
		code = PRBADARG;
	    else
		code = pr_SIdToName(id, name);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS)
		printf("id %d name %s\n", id, name);
	} else if (!strcmp(op, "sni")) {
	    /* scanf("%s",name); */
	    if (GetString(name, sizeof(name)))
		code = PRBADARG;
	    else
		code = pr_SNameToId(name, &id);
	    if (CodeOk(code))
		printf("%s\n", pr_ErrorMsg(code));
	    if (code == PRSUCCESS)
		printf("name %s id %d\n", name, id);
	} else if (!strcmp(op, "fih")) {
	    char tname[128];
	    struct PrUpdateEntry uentry;
	    memset(&uentry, 0, sizeof(uentry));
	    /* scanf("%s",name); */
	    if (GetString(name, sizeof(name))) {
		code = PRBADARG;
		continue;
	    }
	    code = pr_SNameToId(name, &id);
	    if (CodeOk(code)) {
		printf("%s\n", pr_ErrorMsg(code));
		continue;
	    }
	    code = pr_SIdToName(id, tname);
	    if (code == PRSUCCESS) {
		printf
		    ("Warning: Id hash for %s (id %d) seems correct at the db; rehashing it anyway\n",
		     name, id);
/*		continue;*/
	    }
	    uentry.Mask = PRUPDATE_IDHASH;
	    code = ubik_PR_UpdateEntry(pruclient, 0, 0, name, &uentry);
	    if (code) {
		printf("Failed to update entry %s (err=%d)\n", name, code);
		continue;
	    }
	} else if (!strcmp(op, "fnh")) {
	    int tid;
	    struct PrUpdateEntry uentry;
	    memset(&uentry, 0, sizeof(uentry));
	    /* scanf("%d", &id); */
	    if (GetInt32(&id)) {
		code = PRBADARG;
		continue;
	    }
	    code = pr_SIdToName(id, name);
	    if (CodeOk(code)) {
		printf("%s\n", pr_ErrorMsg(code));
		continue;
	    }
	    code = pr_SNameToId(name, &tid);
	    if (code == PRSUCCESS) {
		printf
		    ("Name hash for %d (name is %s) seems correct at the db; rehashing it anyway\n",
		     id, name);
/*		continue;*/
	    }
	    uentry.Mask = PRUPDATE_NAMEHASH;
	    code =
		ubik_PR_UpdateEntry(pruclient, 0, id, "_foo_", &uentry);
	    if (code) {
		printf("Failed to update entry with id %d (err=%d)\n", id,
		       code);
		continue;
	    }
	}
#if defined(SUPERGROUPS)
	else if (!strcmp(op, "fih")) {
	    char tname[128];
	    struct PrUpdateEntry uentry;
	    bzero(&uentry, sizeof(uentry));
	    /* scanf("%s",name); */
	    if (GetString(name, sizeof(name))) {
		code = PRBADARG;
		continue;
	    }
	    code = pr_SNameToId(name, &id);
	    if (CodeOk(code)) {
		printf("%s\n", pr_ErrorMsg(code));
		continue;
	    }
	    code = pr_SIdToName(id, tname);
	    if (code == PRSUCCESS) {
		printf
		    ("Warning: Id hash for %s (id %d) seems correct at the db; rehashing it anyway\n",
		     name, id);
/*		continue;*/
	    }
	    uentry.Mask = PRUPDATE_IDHASH;
	    code = ubik_PR_UpdateEntry(pruclient, 0, 0, name, &uentry);
	    if (code) {
		printf("Failed to update entry %s (err=%d)\n", name, code);
		continue;
	    }
	} else if (!strcmp(op, "fnh")) {
	    int tid;
	    struct PrUpdateEntry uentry;
	    bzero(&uentry, sizeof(uentry));
	    /* scanf("%d", &id); */
	    if (GetInt32(&id)) {
		code = PRBADARG;
		continue;
	    }
	    code = pr_SIdToName(id, name);
	    if (CodeOk(code)) {
		printf("%s\n", pr_ErrorMsg(code));
		continue;
	    }
	    code = pr_SNameToId(name, &tid);
	    if (code == PRSUCCESS) {
		printf
		    ("Name hash for %d (name is %s) seems correct at the db; rehashing it anyway\n",
		     id, name);
/*		continue;*/
	    }
	    uentry.Mask = PRUPDATE_NAMEHASH;
	    code =
		ubik_PR_UpdateEntry(pruclient, 0, id, "_foo_", &uentry);
	    if (code) {
		printf("Failed to update entry with id %d (err=%d)\n", id,
		       code);
		continue;
	    }
	}
#endif /* SUPERGROUPS */
	else if (!strcmp(op, "?"))
	    PrintHelp();
	else if (!strcmp(op, "q"))
	    exit(0);
	else
	    printf("Unknown op: '%s'! ? for help\n", op);
    }
}


PrintHelp()
{
    printf("cr name id owner - create entry with name and id.\n");
    printf("wh id  - what is the offset into database for id?\n");
    printf("du offset - dump the contents of the entry at offset.\n");
    printf("add uid gid - add user uid to group gid.\n");
    printf("iton id* - translate the list of id's to names.\n");
    printf("ntoi name* - translate the list of names to ids.\n");
    printf("del id - delete the entry for id.\n");
    printf("dg gid - delete the entry for group gid.\n");
    printf("rm id gid - remove user id from group gid.\n");
    printf("l id - get the CPS for id.\n");
    printf("lh host - get the host CPS for host.\n");
#if defined(SUPERGROUPS)
    printf("lsg id - get the supergroups for id.\n");
    printf("m id - list elements for id.\n");
#endif
    printf("nu name - create new user with name - returns an id.\n");
    printf("ng name - create new group with name - returns an id.\n");
    printf("lm  - list max user id and max (really min) group id.\n");
    printf("smu - set max user id.\n");
    printf("smg - set max group id.\n");
    printf("sin id - single iton.\n");
    printf("sni name - single ntoi.\n");
    printf("fih name - fix id hash for <name>.\n");
    printf("fnh id - fix name hash for <id>.\n");
    printf("q - quit.\n?- this message.\n");
}


skip(s)
     char **s;
{
    while (**s != ' ' && **s != '\0')
	(*s)++;
    while (**s == ' ')
	(*s)++;
}
#endif /* !AFS_PTHREAD_ENV */
