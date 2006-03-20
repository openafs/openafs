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

#include <sys/types.h>
#include <sys/stat.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <ubik.h>
#include <afs/cmd.h>

#include <afs/com_err.h>

#include "kauth.h"
#include "kautils.h"
#include "kaserver.h"

#define UBIK_HEADERSIZE 64
#define UBIK_BUFFERSIZE 1024

char *whoami = "kadb_check";
int fd;
FILE *out;

void badEntry();

int listuheader, listkheader, listentries, verbose;

int
readUbikHeader()
{
    int offset, r;
    struct ubik_hdr uheader;

    offset = lseek(fd, 0, 0);
    if (offset != 0) {
	printf("error: lseek to 0 failed: %d %d\n", offset, errno);
	return (-1);
    }

    /* now read the info */
    r = read(fd, &uheader, sizeof(uheader));
    if (r != sizeof(uheader)) {
	printf("error: read of %d bytes failed: %d %d\n", sizeof(uheader), r,
	       errno);
	return (-1);
    }

    uheader.magic = ntohl(uheader.magic);
    uheader.size = ntohl(uheader.size);
    uheader.version.epoch = ntohl(uheader.version.epoch);
    uheader.version.counter = ntohl(uheader.version.counter);

    if (listuheader) {
	printf("Ubik Header\n");
	printf("   Magic           = 0x%x\n", uheader.magic);
	printf("   Size            = %u\n", uheader.size);
	printf("   Version.epoch   = %u\n", uheader.version.epoch);
	printf("   Version.counter = %u\n", uheader.version.counter);
    }

    if (uheader.size != UBIK_HEADERSIZE)
	printf("Ubik header size is %u (should be %u)\n", uheader.size,
	       UBIK_HEADERSIZE);
    if (uheader.magic != UBIK_MAGIC)
	printf("Ubik header magic is 0x%x (should be 0x%x)\n", uheader.magic,
	       UBIK_MAGIC);

    return (0);
}

void
PrintHeader(header)
     struct kaheader *header;
{
    printf("Version          = %d\n", header->version);
    printf("HeaderSize       = %d\n", header->headerSize);
    printf("Free Ptr         = %u\n", header->freePtr);
    printf("EOF  Ptr         = %u\n", header->eofPtr);
    printf("Kvno Ptr         = %u\n", header->kvnoPtr);
    printf("SpecialKeysVersion changed = %d\n", header->specialKeysVersion);
    printf("# admin accounts = %d\n", header->admin_accounts);
    printf("HashSize         = %d\n", header->hashsize);
    printf("Check Version    = %d\n", header->checkVersion);
    printf("stats.minorVersion     = %d\n", header->stats.minor_version);
    printf("stats.AllocBlock calls = %d\n", header->stats.allocs);
    printf("stats.FreeBlock  calls = %d\n", header->stats.frees);
    printf("stats.cpw commands     = %d\n", header->stats.cpws);
}

void
PrintEntry(index, entry)
     afs_int32 index;
     struct kaentry *entry;
{
    int i;
    char Time[100];
    struct tm *tm_p;
    time_t tt;

    printf("\n");

    i = (index - sizeof(struct kaheader)) / sizeof(struct kaentry);

    printf("Entry %5d (%u):\n", i, index);

    if (entry->flags & KAFNORMAL) {
	printf("   Name = %s", entry->userID.name);
	if (strlen(entry->userID.instance) > 0) {
	    printf(".%s", entry->userID.instance);
	}
	printf("\n");
    }

    printf("   flags = ");
    if (entry->flags & KAFNORMAL)
	printf("NORMAL ");
    if (entry->flags & KAFADMIN)
	printf("ADMIN ");
    if (entry->flags & KAFNOTGS)
	printf("NOTGS ");
    if (entry->flags & KAFNOSEAL)
	printf("NOSEAL ");
    if (entry->flags & KAFNOCPW)
	printf("NOCPW ");

    if (entry->flags & KAFNEWASSOC)
	printf("CR-ASSOC ");
    if (entry->flags & KAFFREE)
	printf("FREE ");
    if (entry->flags & KAFOLDKEYS)
	printf("OLDKEYS ");
    if (entry->flags & KAFSPECIAL)
	printf("SPECIAL ");
    if (entry->flags & KAFASSOCROOT)
	printf("ROOT-ASSOC ");
    if (entry->flags & KAFASSOC)
	printf("AN-ASSOC ");
    printf("\n");

    printf("   Next = %u\n", entry->next);

    if (entry->flags & KAFFREE)
	return;
    if (entry->flags & KAFOLDKEYS)
	return;

    tt = entry->user_expiration;
    tm_p = localtime(&tt);
    if (tm_p)
	strftime(Time, 100, "%m/%d/%Y %H:%M", tm_p);

    printf("   User Expiration = %s\n",
	   (entry->user_expiration == 0xffffffff) ? "never" : Time);

    printf("   Password Expiration = %u days %s\n",
	   entry->misc_auth_bytes[EXPIRES],
	   (entry->misc_auth_bytes[EXPIRES] ? "" : "(never)"));

    printf("   Password Attempts before lock = ");
    if (!entry->misc_auth_bytes[ATTEMPTS])
	printf("unlimited\n");
    else
	printf("%d\n", entry->misc_auth_bytes[ATTEMPTS]);

    printf("   Password lockout time = ");
    if (!entry->misc_auth_bytes[LOCKTIME])
	printf("unlimited\n");
    else
	printf("%.1f min\n", (entry->misc_auth_bytes[LOCKTIME] * 8.5));

    printf("   Is entry locked = %s\n",
	   (entry->misc_auth_bytes[REUSEFLAGS] ==
	    KA_ISLOCKED) ? "yes" : "no");

    printf("   Permit password reuse = %s\n",
	   (!entry->pwsums[0] && !entry->pwsums[1]) ? "yes" : "no");

    printf("   Mod Time = %u: %s", entry->modification_time,
	   ctime((time_t *) & entry->modification_time));
    printf("   Mod ID = %u\n", entry->modification_id);
    printf("   Change Password Time = %u: %s", entry->change_password_time,
	   ctime((time_t *) & entry->change_password_time));
    printf("   Ticket lifetime = %u: %s", entry->max_ticket_lifetime,
	   ctime((time_t *) & entry->max_ticket_lifetime));
    printf("   Key Version = %d\n", entry->key_version);

    printf("   Key = ");
    ka_PrintBytes((char *)&entry->key, sizeof(entry->key));
    printf("\n");

    /* What about asServer structs and such and misc_ath_bytes */
}

/* ntohEntry - convert back to host-order */
ntohEntry(struct kaentry *entryp)
{
    entryp->flags = ntohl(entryp->flags);
    entryp->next = ntohl(entryp->next);
    entryp->user_expiration = ntohl(entryp->user_expiration);
    entryp->modification_time = ntohl(entryp->modification_time);
    entryp->modification_id = ntohl(entryp->modification_id);
    entryp->change_password_time = ntohl(entryp->change_password_time);
    entryp->max_ticket_lifetime = ntohl(entryp->max_ticket_lifetime);
    entryp->key_version = ntohl(entryp->key_version);
    entryp->misc.asServer.nOldKeys = ntohl(entryp->misc.asServer.nOldKeys);
    entryp->misc.asServer.oldKeys = ntohl(entryp->misc.asServer.oldKeys);
}

char principal[64];
char *
EntryName(entryp)
     struct kaentry *entryp;
{
    char name[32], inst[32];

    ka_ConvertBytes(name, sizeof(name), entryp->userID.name,
		    strlen(entryp->userID.name));
    ka_ConvertBytes(inst, sizeof(inst), entryp->userID.instance,
		    strlen(entryp->userID.instance));

    if (strlen(entryp->userID.instance)) {
	sprintf(principal, "%s.%s", name, inst);
    } else {
	strcpy(principal, name);
    }

    return (principal);
}

void
RebuildEntry(entryp)
     struct kaentry *entryp;
{
    char key[33];
    char flags[128];
    char Time[50];

    /* Special entries are not rebuilt */
    if (entryp->flags & KAFSPECIAL)
	return;

    fprintf(out, "create    -name %s", EntryName(entryp));

    ka_ConvertBytes(key, sizeof(key), (char *)&entryp->key,
		    sizeof(entryp->key));
    fprintf(out, " -initial_password foo\n");

    strcpy(flags, "");
    if (entryp->flags & KAFADMIN)
	strcat(flags, "+ADMIN");
    if (entryp->flags & KAFNOTGS)
	strcat(flags, "+NOTGS");
    if (entryp->flags & KAFNOSEAL)
	strcat(flags, "+NOSEAL");
    if (entryp->flags & KAFNOCPW)
	strcat(flags, "+NOCPW");

    fprintf(out, "setfields -name %s", principal);
    if (strcmp(flags, "") != 0)
	fprintf(out, " -flags %s", &flags[1]);
    if (entryp->user_expiration != 0xffffffff) {
	time_t tt = entryp->user_expiration;
	strftime(Time, 50, "%m/%d/%Y %H:%M",localtime(&tt));
	fprintf(out, " -expiration '%s'", Time);
    }
    fprintf(out, " -lifetime %u", entryp->max_ticket_lifetime);
    if (entryp->misc_auth_bytes[EXPIRES])
	fprintf(out, " -pwexpires %u", entryp->misc_auth_bytes[EXPIRES]);
    if (entryp->pwsums[0] || entryp->pwsums[1])
	fprintf(out, " -reuse no");
    if (entryp->misc_auth_bytes[ATTEMPTS])
	fprintf(out, " -attempts %u", entryp->misc_auth_bytes[ATTEMPTS]);
    if (entryp->misc_auth_bytes[LOCKTIME])
	fprintf(out, " -locktime %d",
		(int)(entryp->misc_auth_bytes[LOCKTIME] * 8.5));
    fprintf(out, "\n");

    fprintf(out, "setkey    -name %s -new_key %s -kvno %d\n", principal, key,
	    ntohl(entryp->key_version));
}

CheckHeader(header)
     struct kaheader *header;
{
    afs_int32 i, code = 0;

    header->version = ntohl(header->version);
    header->headerSize = ntohl(header->headerSize);
    header->freePtr = ntohl(header->freePtr);
    header->eofPtr = ntohl(header->eofPtr);
    header->kvnoPtr = ntohl(header->kvnoPtr);
    header->stats.minor_version = ntohl(header->stats.minor_version);
    header->stats.allocs = ntohl(header->stats.allocs);
    header->stats.frees = ntohl(header->stats.frees);
    header->stats.cpws = ntohl(header->stats.cpws);
    header->admin_accounts = ntohl(header->admin_accounts);
    header->specialKeysVersion = ntohl(header->specialKeysVersion);
    header->hashsize = ntohl(header->hashsize);
    for (i = 0; i < HASHSIZE; i++) {
	header->nameHash[i] = ntohl(header->nameHash[i]);
    }
    header->checkVersion = ntohl(header->checkVersion);

    if (header->version != header->checkVersion) {
	code++;
	fprintf(stderr, "HEADER VERSION MISMATCH: initial %d, final %d\n",
		header->version, header->checkVersion);
    }
    if (header->headerSize != sizeof(struct kaheader)) {
	code++;
	fprintf(stderr,
		"HEADER SIZE WRONG: file indicates %d, should be %d\n",
		header->headerSize, sizeof(struct kaheader));
    }
    if (header->hashsize != HASHSIZE) {
	code++;
	fprintf(stderr, "HASH SIZE WRONG: file indicates %d, should be %d\n",
		header->hashsize, HASHSIZE);
    }
    if ((header->kvnoPtr && ((header->kvnoPtr < header->headerSize)
			     || (header->eofPtr < header->freePtr)))
	|| (header->freePtr && ((header->freePtr < header->headerSize)
				|| (header->eofPtr < header->kvnoPtr)))) {
	code++;
	fprintf(stderr,
		"DATABASE POINTERS BAD: header size = %d, freePtr = %d, kvnoPtr = %d, eofPtr = %d\n",
		header->headerSize, header->freePtr, header->kvnoPtr,
		header->eofPtr);
    }

/*
 *  fprintf(stderr, "DB Version %d, %d possible entries\n", header->version,
 *          (header->eofPtr-header->headerSize) / sizeof(struct kaentry));
 */
    return code;
}

afs_int32
NameHash(entryp)
     struct kaentry *entryp;
{
    unsigned int hash;
    int i;
    char *aname = entryp->userID.name;
    char *ainstance = entryp->userID.instance;

    /* stolen directly from the HashString function in the vol package */
    hash = 0;
    for (i = strlen(aname), aname += i - 1; i--; aname--)
	hash = (hash * 31) + (*((unsigned char *)aname) - 31);
    for (i = strlen(ainstance), ainstance += i - 1; i--; ainstance--)
	hash = (hash * 31) + (*((unsigned char *)ainstance) - 31);
    return (hash % HASHSIZE);
}

readDB(offset, buffer, size)
     afs_int32 offset;
     char *buffer;
     afs_int32 size;
{
    afs_int32 code;

    offset += UBIK_HEADERSIZE;
    code = lseek(fd, offset, SEEK_SET);
    if (code != offset) {
	com_err(whoami, errno, "skipping Ubik header");
	exit(2);
    }
    code = read(fd, buffer, size);
    if (code != size) {
	com_err(whoami, errno, "reading db got %d bytes", code);
	exit(3);
    }
}

#include "AFS_component_version_number.c"

WorkerBee(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 code;
    char *dbFile;
    char *outFile;
    afs_int32 index;
    struct stat info;
    struct kaheader header;
    int nentries, i, j, count;
    int *entrys;
    struct kaentry entry;

    dbFile = as->parms[0].items->data;	/* -database */
    listuheader = (as->parms[1].items ? 1 : 0);	/* -uheader  */
    listkheader = (as->parms[2].items ? 1 : 0);	/* -kheader  */
    listentries = (as->parms[3].items ? 1 : 0);	/* -entries  */
    verbose = (as->parms[4].items ? 1 : 0);	/* -verbose  */
    outFile = (as->parms[5].items ? as->parms[5].items->data : NULL);	/* -rebuild  */

    if (outFile) {
	out = fopen(outFile, "w");
	if (!out) {
	    com_err(whoami, errno, "opening output file %s", outFile);
	    exit(7);
	}
    } else
	out = 0;

    fd = open(dbFile, O_RDONLY, 0);
    if (fd < 0) {
	com_err(whoami, errno, "opening database file %s", dbFile);
	exit(6);
    }
    code = fstat(fd, &info);
    if (code) {
	com_err(whoami, errno, "stat'ing file %s", dbFile);
	exit(6);
    }
    if ((info.st_size - UBIK_HEADERSIZE) % UBIK_BUFFERSIZE)
	fprintf(stderr,
		"DATABASE SIZE INCONSISTENT: was %d, should be (n*%d + %d), for integral n\n",
		info.st_size, UBIK_BUFFERSIZE, UBIK_HEADERSIZE);

    readUbikHeader();

    readDB(0, &header, sizeof(header));
    code = CheckHeader(&header);
    if (listkheader)
	PrintHeader(&header);

    nentries =
	(info.st_size -
	 (UBIK_HEADERSIZE + header.headerSize)) / sizeof(struct kaentry);
    entrys = (int *)malloc(nentries * sizeof(int));
    memset(entrys, 0, nentries * sizeof(int));

    for (i = 0, index = sizeof(header); i < nentries;
	 i++, index += sizeof(struct kaentry)) {
	readDB(index, &entry, sizeof(entry));

	if (index >= header.eofPtr) {
	    entrys[i] |= 0x8;
	} else if (listentries) {
	    PrintEntry(index, &entry);
	}

	if (entry.flags & KAFNORMAL) {
	    entrys[i] |= 0x1;	/* user entry */

	    if (strlen(entry.userID.name) == 0) {
		if (verbose)
		    printf("Entry %d has zero length name\n", i);
		continue;
	    }
	    if (!des_check_key_parity(&entry.key)
		|| des_is_weak_key(&entry.key)) {
		fprintf(stderr, "Entry %d, %s, has bad key\n", i,
			EntryName(&entry));
		continue;
	    }

	    if (out) {
		RebuildEntry(&entry);
	    }

	} else if (entry.flags & KAFFREE) {
	    entrys[i] |= 0x2;	/* free entry */

	} else if (entry.flags & KAFOLDKEYS) {
	    entrys[i] |= 0x4;	/* old keys block */
	    /* Should check the structure of the oldkeys block? */

	} else {
	    if (index < header.eofPtr) {
		fprintf(stderr, "Entry %d is unrecognizable\n", i);
	    }
	}
    }

    /* Follow the hash chains */
    for (j = 0; j < HASHSIZE; j++) {
	for (index = header.nameHash[j]; index; index = entry.next) {
	    readDB(index, &entry, sizeof(entry));

	    /* check to see if the name is hashed correctly */
	    i = NameHash(&entry);
	    if (i != j) {
		fprintf(stderr,
			"Entry %d, %s, found in hash chain %d (should be %d)\n",
			((index -
			  sizeof(struct kaheader)) / sizeof(struct kaentry)),
			EntryName(&entry), j, i);
	    }

	    /* Is it on another hash chain or circular hash chain */
	    i = (index - header.headerSize) / sizeof(entry);
	    if (entrys[i] & 0x10) {
		fprintf(stderr,
			"Entry %d, %s, hash index %d, was found on another hash chain\n",
			i, EntryName(&entry), j);
		if (entry.next)
		    fprintf(stderr, "Skipping rest of hash chain %d\n", j);
		else
		    fprintf(stderr, "No next entry in hash chain %d\n", j);
		code++;
		break;
	    }
	    entrys[i] |= 0x10;	/* On hash chain */
	}
    }

    /* Follow the free pointers */
    count = 0;
    for (index = header.freePtr; index; index = entry.next) {
	readDB(index, &entry, sizeof(entry));

	/* Is it on another chain or circular free chain */
	i = (index - header.headerSize) / sizeof(entry);
	if (entrys[i] & 0x20) {
	    fprintf(stderr, "Entry %d, %s, already found on free chain\n", i,
		    EntryName(&entry));
	    fprintf(stderr, "Skipping rest of free chain\n");
	    code++;
	    break;
	}
	entrys[i] |= 0x20;	/* On free chain */

	count++;
    }
    if (verbose)
	printf("Found %d free entries\n", count);

    /* Follow the oldkey blocks */
    count = 0;
    for (index = header.kvnoPtr; index; index = entry.next) {
	readDB(index, &entry, sizeof(entry));

	/* Is it on another chain or circular free chain */
	i = (index - header.headerSize) / sizeof(entry);
	if (entrys[i] & 0x40) {
	    fprintf(stderr, "Entry %d, %s, already found on olkeys chain\n",
		    i, EntryName(&entry));
	    fprintf(stderr, "Skipping rest of oldkeys chain\n");
	    code++;
	    break;
	}
	entrys[i] |= 0x40;	/* On free chain */

	count++;
    }
    if (verbose)
	printf("Found %d oldkey blocks\n", count);

    /* Now recheck all the blocks and see if they are allocated correctly
     * 0x1 --> User Entry           0x10 --> On hash chain
     * 0x2 --> Free Entry           0x20 --> On Free chain
     * 0x4 --> OldKeys Entry        0x40 --> On Oldkeys chain
     * 0x8 --> Past EOF
     */
    for (i = 0; i < nentries; i++) {
	j = entrys[i];
	if (j & 0x1) {		/* user entry */
	    if (!(j & 0x10))
		badEntry(j, i);	/* on hash chain? */
	    else if (j & 0xee)
		badEntry(j, i);	/* anything else? */
	} else if (j & 0x2) {	/* free entry */
	    if (!(j & 0x20))
		badEntry(j, i);	/* on free chain? */
	    else if (j & 0xdd)
		badEntry(j, i);	/* anything else? */
	} else if (j & 0x4) {	/* oldkeys entry */
	    if (!(j & 0x40))
		badEntry(j, i);	/* on oldkeys chain? */
	    else if (j & 0xbb)
		badEntry(j, i);	/* anything else? */
	} else if (j & 0x8) {	/* past eof */
	    if (j & 0xf7)
		badEntry(j, i);	/* anything else? */
	} else
	    badEntry(j, i);	/* anything else? */
    }

    exit(code != 0);
}

void
badEntry(e, i)
     afs_int32 e, i;
{
    int offset;
    struct kaentry entry;

    offset = i * sizeof(struct kaentry) + sizeof(struct kaheader);
    readDB(offset, &entry, sizeof(entry));

    fprintf(stderr, "Entry %d, %s, hash index %d, is bad: [", i,
	    EntryName(&entry), NameHash(&entry));
    if (e & 0x1)
	fprintf(stderr, " UserEntry");
    if (e & 0x2)
	fprintf(stderr, " FreeEntry");
    if (e & 0x4)
	fprintf(stderr, " OldkeysEntry");
    if (e & 0x8)
	fprintf(stderr, " PastEOF");
    if (!(e & 0xf))
	fprintf(stderr, " <NULL>");
    fprintf(stderr, " ] [");
    if (e & 0x10)
	fprintf(stderr, " UserChain");
    if (e & 0x20)
	fprintf(stderr, " FreeChain");
    if (e & 0x40)
	fprintf(stderr, " OldkeysChain");
    if (!(e & 0xf0))
	fprintf(stderr, " <NULL>");
    fprintf(stderr, " ]\n");
}

main(argc, argv)
     int argc;
     char *argv[];
{
    struct cmd_syndesc *ts;

    setlinebuf(stdout);

    ts = cmd_CreateSyntax(NULL, WorkerBee, NULL, "KADB check");
    cmd_AddParm(ts, "-database", CMD_SINGLE, CMD_REQUIRED, "kadb_file");
    cmd_AddParm(ts, "-uheader", CMD_FLAG, CMD_OPTIONAL,
		"Display UBIK header");
    cmd_AddParm(ts, "-kheader", CMD_FLAG, CMD_OPTIONAL,
		"Display KADB header");
    cmd_AddParm(ts, "-entries", CMD_FLAG, CMD_OPTIONAL, "Display entries");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose");
    cmd_AddParm(ts, "-rebuild", CMD_SINGLE, CMD_OPTIONAL, "out_file");

    return cmd_Dispatch(argc, argv);
}
