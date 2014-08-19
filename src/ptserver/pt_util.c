/* $Id$ */

/*
 *
 * pt_util: Program to dump the AFS protection server database
 *         into an ascii file.
 *
 *	Assumptions: We *cheat* here and read the datafile directly, ie.
 *	             not going through the ubik distributed data manager.
 *		     therefore the database must be quiescent for the
 *		     output of this program to be valid.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <sys/types.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/file.h>
#else
#include <fcntl.h>
#include <io.h>
#define L_SET SEEK_SET
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <afs/com_err.h>


#include <afs/cmd.h>		/*Command line parsing */
#include <afs/afsutil.h>
#include <errno.h>
#include <lock.h>
#ifndef _WIN32
#include <netinet/in.h>
#endif
#define UBIK_INTERNALS
#include <ubik.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/com_err.h>
#include "ptint.h"
#include "ptserver.h"
#include "pterror.h"
#include "ptprototypes.h"

#define IDHash(x) (abs(x) % HASHSIZE)
#define print_id(x) ( ((flags&DO_SYS)==0 && (x<-32767 || x>97536)) || \
		      ((flags&DO_OTR)==0 && (x>-32768 && x<97537)))

extern char *optarg;
extern int optind;

int restricted = 0;

static int display_entry(int);
static void add_group(long);
static void display_groups(void);
static void display_group(int);
static void fix_pre(struct prentry *);
static char *id_to_name(int);
static char *checkin(struct prentry *);
static char *check_core(int);
static int CommandProc(struct cmd_syndesc *, void *);

struct hash_entry {
    char h_name[PR_MAXNAMELEN];
    int h_id;
    struct hash_entry *next;
};
struct hash_entry *hat[HASHSIZE];

static struct contentry prco;
static struct prentry pre;
static struct prheader prh;
static struct ubik_version uv;

struct grp_list {
    struct grp_list *next;
    long groups[1024];
};
static struct grp_list *grp_head = 0;
static long grp_count = 0;

struct usr_list {
    struct usr_list *next;
    char name[PR_MAXNAMELEN];
    long uid;
};
static struct usr_list *usr_head = 0;

char buffer[1024];
int dbase_fd;
FILE *dfp;

#define FMT_BASE "%-10s %d/%d %d %d %d\n"
#define FMT_MEM  "   %-8s %d\n"

#define DO_USR 1
#define DO_GRP 2
#define DO_MEM 4
#define DO_SYS 8
#define DO_OTR 16

int nflag = 0;
int wflag = 0;
int flags = 0;

int
main(int argc, char **argv)
{

    struct cmd_syndesc *cs;	/*Command line syntax descriptor */
    afs_int32 code;	/*Return code */

    cs = cmd_CreateSyntax(NULL, CommandProc, NULL,
			  "access protection database");
    cmd_AddParm(cs, "-w", CMD_FLAG, CMD_OPTIONAL,
		"update prdb with contents of data file");
    cmd_AddParm(cs, "-user", CMD_FLAG, CMD_OPTIONAL, "display users");
    cmd_AddParm(cs, "-group", CMD_FLAG, CMD_OPTIONAL, "display groups");
    cmd_AddParm(cs, "-members", CMD_FLAG, CMD_OPTIONAL,
		"display group members");
    cmd_AddParm(cs, "-name", CMD_FLAG, CMD_OPTIONAL,
		"follow name hash chains (not id hashes)");
    cmd_AddParm(cs, "-system", CMD_FLAG, CMD_OPTIONAL,
		"display only system data");
    cmd_AddParm(cs, "-xtra", CMD_FLAG, CMD_OPTIONAL,
		"display extra users/groups");
    cmd_AddParm(cs, "-prdb", CMD_SINGLE, CMD_OPTIONAL, "prdb file");
    cmd_AddParm(cs, "-datafile", CMD_SINGLE, CMD_OPTIONAL, "data file");
    code = cmd_Dispatch(argc, argv);

    exit(code);

}

static int
CommandProc(struct cmd_syndesc *a_as, void *arock)
{
    int i;
    long code = 0;
    long upos;
    long gpos = 0;
    struct prentry uentry, gentry;
    struct ubik_hdr *uh;
    char *dfile = 0;
    const char *pbase = AFSDIR_SERVER_PRDB_FILEPATH;
    char *pfile = NULL;
    char pbuffer[1028];
    struct cmd_parmdesc *tparm;

    tparm = a_as->parms;

    if (tparm[0].items) {
	wflag++;
    }
    if (tparm[1].items) {
	flags |= DO_USR;
    }
    if (tparm[2].items) {
	flags |= DO_GRP;
    }
    if (tparm[3].items) {
	flags |= (DO_GRP | DO_MEM);
    }
    if (tparm[4].items) {
	nflag++;
    }
    if (tparm[5].items) {
	flags |= DO_SYS;
    }
    if (tparm[6].items) {
	flags |= DO_OTR;
    }
    if (tparm[7].items) {
	pfile = tparm[7].items->data;
    }
    if (tparm[8].items) {
	dfile = tparm[8].items->data;
    }

    if (pfile == NULL) {
        afs_snprintf(pbuffer, sizeof(pbuffer), "%s.DB0", pbase);
        pfile = pbuffer;
    }
    if ((dbase_fd = open(pfile, (wflag ? O_RDWR : O_RDONLY) | O_CREAT, 0600))
	< 0) {
	fprintf(stderr, "pt_util: cannot open %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }
    if (read(dbase_fd, buffer, HDRSIZE) < 0) {
	fprintf(stderr, "pt_util: error reading %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }

    if (dfile) {
	if ((dfp = fopen(dfile, wflag ? "r" : "w")) == 0) {
	    fprintf(stderr, "pt_util: error opening %s: %s\n", dfile,
		    strerror(errno));
	    exit(1);
	}
    } else
	dfp = (wflag ? stdin : stdout);

    uh = (struct ubik_hdr *)buffer;
    if (ntohl(uh->magic) != UBIK_MAGIC)
	fprintf(stderr, "pt_util: %s: Bad UBIK_MAGIC. Is %x should be %x\n",
		pfile, ntohl(uh->magic), UBIK_MAGIC);
    memcpy(&uv, &uh->version, sizeof(struct ubik_version));

    if (wflag && ntohl(uv.epoch) == 0 && ntohl(uv.counter) == 0) {
	uv.epoch = htonl(2); /* a ubik version of 0 or 1 has special meaning */
	memcpy(&uh->version, &uv, sizeof(struct ubik_version));
	lseek(dbase_fd, 0, SEEK_SET);
	if (write(dbase_fd, buffer, HDRSIZE) < 0) {
	    fprintf(stderr, "pt_util: error writing ubik version to %s: %s\n",
		    pfile, strerror(errno));
	    exit(1);
	}
    }

    /* Now that any writeback is done, swap these */
    uv.epoch = ntohl(uv.epoch);
    uv.counter = ntohl(uv.counter);

    fprintf(stderr, "Ubik Version is: %d.%d\n", uv.epoch, uv.counter);
    if (read(dbase_fd, &prh, sizeof(struct prheader)) < 0) {
	fprintf(stderr, "pt_util: error reading %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }

    Initdb();
    initialize_PT_error_table();

    if (wflag) {
	struct usr_list *u;
	int id = 0, flags = 0;
	int seenGroup = 0;

	while (fgets(buffer, sizeof(buffer), dfp)) {
	    int oid, cid, quota, uid;
	    char name[PR_MAXNAMELEN], mem[PR_MAXNAMELEN];

	    if (isspace(*buffer)) {
		code = sscanf(buffer, "%s %d", mem, &uid);
		if (code != 2) {
		    fprintf(stderr,
			    "Insuffient data provided for group membership\n");
		    exit(1);
		}

		if (!seenGroup) {
		    fprintf(stderr,
			    "Group member %s listed outside of group\n",
			    mem);
		    exit(1);
		}

		for (u = usr_head; u; u = u->next)
		    if (u->uid && u->uid == uid)
			break;
		if (u) {
		    /* Add user - deferred because it is probably foreign */
		    u->uid = 0;
		    if (FindByID(0, uid))
			code = PRIDEXIST;
		    else {
			if (!code
			    && (flags & (PRGRP | PRQUOTA)) ==
			    (PRGRP | PRQUOTA)) {
			    gentry.ngroups++;
			    code = pr_WriteEntry(0, 0, gpos, &gentry);
			    if (code)
				fprintf(stderr,
					"Error setting group count on %s: %s\n",
					name, afs_error_message(code));
			}
			code = CreateEntry(0, u->name, &uid, 1 /*idflag */ ,
					   1 /*gflag */ ,
					   SYSADMINID /*oid */ ,
					   SYSADMINID /*cid */ );
		    }
		    if (code)
			fprintf(stderr, "Error while creating %s: %s\n",
				u->name, afs_error_message(code));
		    continue;
		}
		/* Add user to group */
		if (id == ANYUSERID || id == AUTHUSERID || uid == ANONYMOUSID) {
		    code = PRPERM;
		} else if ((upos = FindByID(0, uid))
			   && (gpos = FindByID(0, id))) {
		    code = pr_ReadEntry(0, 0, upos, &uentry);
		    if (!code)
			code = pr_ReadEntry(0, 0, gpos, &gentry);
		    if (!code)
			code = AddToEntry(0, &gentry, gpos, uid);
		    if (!code)
			code = AddToEntry(0, &uentry, upos, id);
		} else
		    code = PRNOENT;

		if (code)
		    fprintf(stderr, "Error while adding %s to %s: %s\n", mem,
			    name, afs_error_message(code));
	    } else {
		code = sscanf(buffer, "%s %d/%d %d %d %d", name, &flags, &quota, &id,
			      &oid, &cid);
		if (code != 6) {
		    fprintf(stderr,
			    "Insufficient data provided for user/group\n");
		    exit(1);
		}

		seenGroup = 1;

		if (FindByID(0, id))
		    code = PRIDEXIST;
		else
		    code = CreateEntry(0, name, &id, 1 /*idflag */ ,
				       flags & PRGRP, oid, cid);
		if (code == PRBADNAM) {
		    u = (struct usr_list *)malloc(sizeof(struct usr_list));
		    u->next = usr_head;
		    u->uid = id;
		    strcpy(u->name, name);
		    usr_head = u;
		} else if (code) {
		    fprintf(stderr, "Error while creating %s: %s\n", name,
			    afs_error_message(code));
		} else if ((flags & PRACCESS)
			   || (flags & (PRGRP | PRQUOTA)) ==
			   (PRGRP | PRQUOTA)) {
		    gpos = FindByID(0, id);
		    code = pr_ReadEntry(0, 0, gpos, &gentry);
		    if (!code) {
			gentry.flags = flags;
			gentry.ngroups = quota;
			code = pr_WriteEntry(0, 0, gpos, &gentry);
		    }
		    if (code)
			fprintf(stderr,
				"Error while setting flags on %s: %s\n", name,
				afs_error_message(code));
		}
	    }
	}
	for (u = usr_head; u; u = u->next)
	    if (u->uid)
		fprintf(stderr, "Error while creating %s: %s\n", u->name,
			afs_error_message(PRBADNAM));
    } else {
	for (i = 0; i < HASHSIZE; i++) {
	    upos = nflag ? ntohl(prh.nameHash[i]) : ntohl(prh.idHash[i]);
	    while (upos) {
		long newpos;
		newpos = display_entry(upos);
		if (newpos == upos) {
		    fprintf(stderr, "pt_util: hash error in %s chain %d\n",
			    nflag ? "name":"id", i);
		    exit(1);
		} else
		    upos = newpos;
	    }
	}
	if (flags & DO_GRP)
	    display_groups();
    }

    lseek(dbase_fd, 0, L_SET);	/* rewind to beginning of file */
    if (read(dbase_fd, buffer, HDRSIZE) < 0) {
	fprintf(stderr, "pt_util: error reading %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }
    uh = (struct ubik_hdr *)buffer;

    uh->version.epoch = ntohl(uh->version.epoch);
    uh->version.counter = ntohl(uh->version.counter);

    if ((uh->version.epoch != uv.epoch)
	|| (uh->version.counter != uv.counter)) {
	fprintf(stderr,
		"pt_util: Ubik Version number changed during execution.\n");
	fprintf(stderr, "Old Version = %d.%d, new version = %d.%d\n",
		uv.epoch, uv.counter, uh->version.epoch, uh->version.counter);
    }
    close(dbase_fd);
    exit(0);
}

static int
display_entry(int offset)
{
    lseek(dbase_fd, offset + HDRSIZE, L_SET);
    read(dbase_fd, &pre, sizeof(struct prentry));

    fix_pre(&pre);

    if ((pre.flags & PRFREE) == 0) {
	if (pre.flags & PRGRP) {
	    if (flags & DO_GRP)
		add_group(pre.id);
	} else {
	    if (print_id(pre.id) && (flags & DO_USR))
		fprintf(dfp, FMT_BASE, pre.name, pre.flags, pre.ngroups,
			pre.id, pre.owner, pre.creator);
	    checkin(&pre);
	}
    }
    return (nflag ? pre.nextName : pre.nextID);
}

static void
add_group(long id)
{
    struct grp_list *g;
    long i;

    i = grp_count++ % 1024;
    if (i == 0) {
	g = (struct grp_list *)malloc(sizeof(struct grp_list));
	g->next = grp_head;
	grp_head = g;
    }
    g = grp_head;
    g->groups[i] = id;
}

static void
display_groups(void)
{
    int i, id;
    struct grp_list *g;

    g = grp_head;
    while (grp_count--) {
	i = grp_count % 1024;
	id = g->groups[i];
	display_group(id);
	if (i == 0) {
	    grp_head = g->next;
	    free(g);
	    g = grp_head;
	}
    }
}

static void
display_group(int id)
{
    int i, offset;
    int print_grp = 0;

    offset = ntohl(prh.idHash[IDHash(id)]);
    while (offset) {
	lseek(dbase_fd, offset + HDRSIZE, L_SET);
	if (read(dbase_fd, &pre, sizeof(struct prentry)) < 0) {
	    fprintf(stderr, "pt_util: read i/o error: %s\n", strerror(errno));
	    exit(1);
	}
	fix_pre(&pre);
	if (pre.id == id)
	    break;
	offset = pre.nextID;
    }

    if (print_id(id)) {
	fprintf(dfp, FMT_BASE, pre.name, pre.flags, pre.ngroups, pre.id,
		pre.owner, pre.creator);
	print_grp = 1;
    }

    if ((flags & DO_MEM) == 0)
	return;

    for (i = 0; i < PRSIZE; i++) {
	if ((id = pre.entries[i]) == 0)
	    break;
	if (id == PRBADID)
	    continue;
	if (print_id(id) || print_grp == 1) {
	    if (print_grp == 0) {
		fprintf(dfp, FMT_BASE, pre.name, pre.flags, pre.ngroups,
			pre.id, pre.owner, pre.creator);
		print_grp = 2;
	    }
	    fprintf(dfp, FMT_MEM, id_to_name(id), id);
	}
    }
    if (i == PRSIZE) {
	offset = pre.next;
	while (offset) {
	    lseek(dbase_fd, offset + HDRSIZE, L_SET);
	    read(dbase_fd, &prco, sizeof(struct contentry));
	    prco.next = ntohl(prco.next);
	    for (i = 0; i < COSIZE; i++) {
		prco.entries[i] = ntohl(prco.entries[i]);
		if ((id = prco.entries[i]) == 0)
		    break;
		if (id == PRBADID)
		    continue;
		if (print_id(id) || print_grp == 1) {
		    if (print_grp == 0) {
			fprintf(dfp, FMT_BASE, pre.name, pre.flags,
				pre.ngroups, pre.id, pre.owner, pre.creator);
			print_grp = 2;
		    }
		    fprintf(dfp, FMT_MEM, id_to_name(id), id);
		}
	    }
	    if ((i == COSIZE) && prco.next)
		offset = prco.next;
	    else
		offset = 0;
	}
    }
}

static void
fix_pre(struct prentry *pre)
{
    int i;

    pre->flags = ntohl(pre->flags);
    pre->id = ntohl(pre->id);
    pre->cellid = ntohl(pre->cellid);
    pre->next = ntohl(pre->next);
    pre->nextID = ntohl(pre->nextID);
    pre->nextName = ntohl(pre->nextName);
    pre->owner = ntohl(pre->owner);
    pre->creator = ntohl(pre->creator);
    pre->ngroups = ntohl(pre->ngroups);
    pre->nusers = ntohl(pre->nusers);
    pre->count = ntohl(pre->count);
    pre->instance = ntohl(pre->instance);
    pre->owned = ntohl(pre->owned);
    pre->nextOwned = ntohl(pre->nextOwned);
    pre->parent = ntohl(pre->parent);
    pre->sibling = ntohl(pre->sibling);
    pre->child = ntohl(pre->child);
    for (i = 0; i < PRSIZE; i++) {
	pre->entries[i] = ntohl(pre->entries[i]);
    }
}

static char *
id_to_name(int id)
{
    int offset;
    static struct prentry pre;
    char *name;

    name = check_core(id);
    if (name)
	return (name);
    offset = ntohl(prh.idHash[IDHash(id)]);
    while (offset) {
	lseek(dbase_fd, offset + HDRSIZE, L_SET);
	if (read(dbase_fd, &pre, sizeof(struct prentry)) < 0) {
	    fprintf(stderr, "pt_util: read i/o error: %s\n", strerror(errno));
	    exit(1);
	}
	pre.id = ntohl(pre.id);
	if (pre.id == id) {
	    name = checkin(&pre);
	    return (name);
	}
	offset = ntohl(pre.nextID);
    }
    return 0;
}

static char *
checkin(struct prentry *pre)
{
    struct hash_entry *he, *last;
    int id;

    id = pre->id;
    last = (struct hash_entry *)0;
    he = hat[IDHash(id)];
    while (he) {
	if (id == he->h_id)
	    return (he->h_name);
	last = he;
	he = he->next;
    }
    he = (struct hash_entry *)malloc(sizeof(struct hash_entry));
    if (he == 0) {
	fprintf(stderr, "pt_util: No Memory for internal hash table.\n");
	exit(1);
    }
    he->h_id = id;
    he->next = (struct hash_entry *)0;
    strncpy(he->h_name, pre->name, PR_MAXNAMELEN);
    if (last == (struct hash_entry *)0)
	hat[IDHash(id)] = he;
    else
	last->next = he;
    return (he->h_name);
}

static char *
check_core(int id)
{
    struct hash_entry *he;
    he = hat[IDHash(id)];
    while (he) {
	if (id == he->h_id)
	    return (he->h_name);
	he = he->next;
    }
    return 0;
}
