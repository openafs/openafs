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


#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <stdio.h>
#include "ptserver.h"

#include "display.h"

# include <time.h>


static char *
pr_TimeToString(time_t clock)
{
    struct tm *tm;
    static char buffer[32];
    static int this_year = 0;

    if (clock == 0)
	return "time-not-set  ";
    if (!this_year) {
	time_t now = time(0);
	tm = localtime(&now);
	this_year = tm->tm_year;
    }
    tm = localtime(&clock);
    if (tm->tm_year != this_year)
	strftime(buffer, 32, "%m/%d/%Y %H:%M:%S", tm);
    else
	strftime(buffer, 32, "%m/%d %H:%M:%S", tm);
    return buffer;
}

#define host(a) (hostOrder ? (a) : ntohl(a))

#define PRINT_COMMON_FIELDS(e)                                             \
do {                                                                       \
    int i;                                                                 \
    if ((e)->cellid)                                                       \
	fprintf(f, "cellid == %d\n", host((e)->cellid));                   \
    for (i = 0; i < sizeof((e)->reserved) / sizeof((e)->reserved[0]); i++) \
	if ((e)->reserved[i])                                              \
	    fprintf(f, "reserved field [%d] not zero: %d\n", i,            \
		    host((e)->reserved[i]));                               \
    fprintf(f, "%*s", indent, "");                                         \
    fprintf(f, "Entry at %d: flags 0x%x, id %di, next %d.\n", ea,          \
	    host((e)->flags), host((e)->id), host((e)->next));             \
} while (0)

#define PRINT_IDS(e)                                                       \
do {                                                                       \
    int i;                                                                 \
    int newline = 0;                                                       \
    for (i = 0; i < sizeof((e)->entries)/sizeof((e)->entries[0]); i++) {   \
	if ((e)->entries[i] == 0)                                          \
	    break;                                                         \
	if (i == 0)                                                        \
	    fprintf(f, "%*sids ", indent, "");                             \
	else if (newline == 0)                                             \
	    fprintf(f, "%*s", indent + 4, "");                             \
	if (host((e)->entries[i]) == PRBADID)                              \
	    fprintf(f, " EMPTY");                                          \
	else                                                               \
	    fprintf(f, "%6d", host((e)->entries[i]));                      \
	newline = 1;                                                       \
	if (i % 10 == 9) {                                                 \
	    fprintf(f, "\n");                                              \
	    newline = 0;                                                   \
	} else                                                             \
	    fprintf(f, " ");                                               \
    }                                                                      \
    if (newline)                                                           \
	fprintf(f, "\n");                                                  \
} while (0)

/* regular entry */
int
pr_PrintEntry(FILE *f, int hostOrder, afs_int32 ea, struct prentry *e,
	      int indent)
{
    /* In case we are given the wrong type of entry. */
    if ((host(e->flags) & PRTYPE) == PRCONT) {
	struct contentry c;
	memcpy(&c, e, sizeof(c));
	return pr_PrintContEntry(f, hostOrder, ea, &c, indent);
    }

    PRINT_COMMON_FIELDS(e);
    fprintf(f, "%*s", indent, "");
    fprintf(f, "c:%s ", pr_TimeToString(host(e->createTime)));
    fprintf(f, "a:%s ", pr_TimeToString(host(e->addTime)));
    fprintf(f, "r:%s ", pr_TimeToString(host(e->removeTime)));
    fprintf(f, "n:%s\n", pr_TimeToString(host(e->changeTime)));
    PRINT_IDS(e);
    fprintf(f, "%*s", indent, "");
    fprintf(f, "hash (id %d name %d).  Owner %di, creator %di\n",
	    host(e->nextID), host(e->nextName), host(e->owner),
	    host(e->creator));
    fprintf(f, "%*s", indent, "");
#if defined(SUPERGROUPS)
    fprintf(f, "quota groups %d, foreign users %d.  Mem: %d, cntsg: %d\n",
	    host(e->ngroups), host(e->nusers), host(e->count),
	    host(e->instance));
#else
    fprintf(f, "quota groups %d, foreign users %d.  Mem: %d, inst: %d\n",
	    host(e->ngroups), host(e->nusers), host(e->count),
	    host(e->instance));
#endif
    fprintf(f, "%*s", indent, "");
#if defined(SUPERGROUPS)
    fprintf(f, "Owned chain %d, next owned %d, nextsg %d, sg (%d %d).\n",
	    host(e->owned), host(e->nextOwned), host(e->parent),
	    host(e->sibling), host(e->child));
#else
    fprintf(f, "Owned chain %d, next owned %d, inst ptrs(%d %d %d).\n",
	    host(e->owned), host(e->nextOwned), host(e->parent),
	    host(e->sibling), host(e->child));
#endif
    fprintf(f, "%*s", indent, "");
    if (strlen(e->name) >= PR_MAXNAMELEN)
	fprintf(f, "NAME TOO LONG: ");
    fprintf(f, "Name is '%.*s'\n", PR_MAXNAMELEN, e->name);
    return 0;
}

int
pr_PrintContEntry(FILE *f, int hostOrder, afs_int32 ea, struct contentry *c, int indent)
{
    PRINT_COMMON_FIELDS(c);
    /* Print the reserved fields for compatibility with older versions.
     * They should always be zero, checked in PRINT_COMMON_FIELDS(). */
    fprintf(f, "%*s", indent, "");
    fprintf(f, "c:%s ", pr_TimeToString(host((c)->reserved[0])));
    fprintf(f, "a:%s ", pr_TimeToString(host((c)->reserved[1])));
    fprintf(f, "r:%s ", pr_TimeToString(host((c)->reserved[2])));
    fprintf(f, "n:%s\n", pr_TimeToString(host((c)->reserved[3])));
    PRINT_IDS(c);
    return 0;
}
