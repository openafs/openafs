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

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>
#include <errno.h>
#include <rx/rx_globals.h>

#include <osi.h>
#include <afsint.h>

#include "fs.h"
#include "fs_utils.h"
#include "cmd.h"
#include "afsd.h"
#include "cm_ioctl.h"

#define	MAXHOSTS 13
#define	OMAXHOSTS 8
#define MAXNAME 100
#define	MAXSIZE	2048
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */
#define VMSGSIZE 128      /* size of msg buf in volume hdr */

static char space[MAXSIZE];
static char tspace[1024];

#ifndef WIN32
static struct ubik_client *uclient;
#endif /* not WIN32 */

static MemDumpCmd(struct cmd_syndesc *asp);
static CSCPolicyCmd(struct cmd_syndesc *asp);

extern afs_int32 VL_GetEntryByNameO();

static char pn[] = "fs";
static int rxInitDone = 0;

static GetCellName();

#define MAXCELLCHARS		64
#define MAXHOSTCHARS		64
#define MAXHOSTSPERCELL		8

struct afsconf_cell {
	char name[MAXCELLCHARS];
        short numServers;
        short flags;
        struct sockaddr_in hostAddr[MAXHOSTSPERCELL];
        char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];
        char *linkedCell;
};


/*
 * Character to use between name and rights in printed representation for
 * DFS ACL's.
 */
#define DFS_SEPARATOR	' '

typedef char sec_rgy_name_t[1025];	/* A DCE definition */

struct Acl {
    int dfs;	/* Originally true if a dfs acl; now also the type
		   of the acl (1, 2, or 3, corresponding to object,
		   initial dir, or initial object). */
    sec_rgy_name_t cell; /* DFS cell name */
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

struct AclEntry {
    struct AclEntry *next;
    char name[MAXNAME];
    afs_int32 rights;
};

void ZapAcl (acl)
    struct Acl *acl; {
    ZapList(acl->pluslist);
    ZapList(acl->minuslist);
    free(acl);
}

foldcmp (a, b)
    register char *a;
    register char *b; {
    register char t, u;
    while (1) {
        t = *a++;
        u = *b++;
        if (t >= 'A' && t <= 'Z') t += 0x20;
        if (u >= 'A' && u <= 'Z') u += 0x20;
        if (t != u) return 1;
        if (t == 0) return 0;
    }
}

/*
 * Mods for the AFS/DFS protocol translator.
 *
 * DFS rights. It's ugly to put these definitions here, but they 
 * *cannot* change, because they're part of the wire protocol.
 * In any event, the protocol translator will guarantee these
 * assignments for AFS cache managers.
 */
#define DFS_READ          0x01
#define DFS_WRITE         0x02
#define DFS_EXECUTE       0x04
#define DFS_CONTROL       0x08
#define DFS_INSERT        0x10
#define DFS_DELETE        0x20

/* the application definable ones (backwards from AFS) */
#define DFS_USR0 0x80000000      /* "A" bit */
#define DFS_USR1 0x40000000      /* "B" bit */
#define DFS_USR2 0x20000000      /* "C" bit */
#define DFS_USR3 0x10000000      /* "D" bit */
#define DFS_USR4 0x08000000      /* "E" bit */
#define DFS_USR5 0x04000000      /* "F" bit */
#define DFS_USR6 0x02000000      /* "G" bit */
#define DFS_USR7 0x01000000      /* "H" bit */
#define DFS_USRALL	(DFS_USR0 | DFS_USR1 | DFS_USR2 | DFS_USR3 |\
			 DFS_USR4 | DFS_USR5 | DFS_USR6 | DFS_USR7)

/*
 * Offset of -id switch in command structure for various commands.
 * The -if switch is the next switch always.
 */
int parm_setacl_id, parm_copyacl_id, parm_listacl_id;

/*
 * Determine whether either the -id or -if switches are present, and
 * return 0, 1 or 2, as appropriate. Abort if both switches are present.
 */
int getidf(as, id)
    struct cmd_syndesc *as;
    int id;	/* Offset of -id switch; -if is next switch */
{
    int idf = 0;

    if (as->parms[id].items) {
	idf |= 1;
    }
    if (as->parms[id + 1].items) {
	idf |= 2;
    }
    if (idf == 3) {
	fprintf
	    (stderr,
	     "%s: you may specify either -id or -if, but not both switches\n",
	     pn);
	exit(1);
    }
    return idf;
}

void PRights (arights, dfs)
    register afs_int32 arights;
    int dfs;
{
    if (!dfs) {
	if (arights & PRSFS_READ) printf("r");
	if (arights & PRSFS_LOOKUP) printf("l");
	if (arights & PRSFS_INSERT) printf("i");
	if (arights & PRSFS_DELETE) printf("d");
	if (arights & PRSFS_WRITE) printf("w");
	if (arights & PRSFS_LOCK) printf("k");
	if (arights & PRSFS_ADMINISTER) printf("a");
	if (arights & PRSFS_USR0) printf("A");
	if (arights & PRSFS_USR1) printf("B");
	if (arights & PRSFS_USR2) printf("C");
	if (arights & PRSFS_USR3) printf("D");
	if (arights & PRSFS_USR4) printf("E");
	if (arights & PRSFS_USR5) printf("F");
	if (arights & PRSFS_USR6) printf("G");
	if (arights & PRSFS_USR7) printf("H");
    } else {
	if (arights & DFS_READ) printf("r"); else printf("-");
	if (arights & DFS_WRITE) printf("w"); else printf("-");
	if (arights & DFS_EXECUTE) printf("x"); else printf("-");
	if (arights & DFS_CONTROL) printf("c"); else printf("-");
	if (arights & DFS_INSERT) printf("i"); else printf("-");
	if (arights & DFS_DELETE) printf("d"); else printf("-");
	if (arights & (DFS_USRALL)) printf("+");
	if (arights & DFS_USR0) printf("A");
	if (arights & DFS_USR1) printf("B");
	if (arights & DFS_USR2) printf("C");
	if (arights & DFS_USR3) printf("D");
	if (arights & DFS_USR4) printf("E");
	if (arights & DFS_USR5) printf("F");
	if (arights & DFS_USR6) printf("G");
	if (arights & DFS_USR7) printf("H");
    }	
}

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
static int InAFS(apath)
register char *apath; {
    struct ViceIoctl blob;
    register afs_int32 code;

    blob.in_size = 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT)) return 0;
    }
    return 1;
}

/* return a static pointer to a buffer */
static char *Parent(apath)
char *apath; {
    register char *tp;
    strcpy(tspace, apath);
    tp = strrchr(tspace, '\\');
    if (tp) {
	*(tp+1) = 0;	/* lv trailing slash so Parent("k:\foo") is "k:\" not "k:" */
    }
    else {
	fs_ExtractDriveLetter(apath, tspace);
    	strcat(tspace, ".");
    }
    return tspace;
}

enum rtype {add, destroy, deny};

afs_int32 Convert(arights, dfs, rtypep)
    register char *arights;
    int dfs;
    enum rtype *rtypep;
{
    register int i, len;
    afs_int32 mode;
    register char tc;

    *rtypep = add;	/* add rights, by default */

    if (dfs) {
	if (!strcmp(arights, "null")) {
	    *rtypep = deny;
	    return 0;
	}
	if (!strcmp(arights,"read")) return DFS_READ | DFS_EXECUTE;
	if (!strcmp(arights, "write")) return DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE | DFS_WRITE;
	if (!strcmp(arights, "all")) return DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE | DFS_WRITE | DFS_CONTROL;
    } else {
	if (!strcmp(arights,"read")) return PRSFS_READ | PRSFS_LOOKUP;
	if (!strcmp(arights, "write")) return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK;
	if (!strcmp(arights, "mail")) return PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
	if (!strcmp(arights, "all")) return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    }
    if (!strcmp(arights, "none")) {
	*rtypep = destroy; /* Remove entire entry */
	return 0;
    }
    len = strlen(arights);
    mode = 0;
    for(i=0;i<len;i++) {
        tc = *arights++;
	if (dfs) {
	    if (tc == '-') continue;
	    else if (tc == 'r') mode |= DFS_READ;
	    else if (tc == 'w') mode |= DFS_WRITE;
	    else if (tc == 'x') mode |= DFS_EXECUTE;
	    else if (tc == 'c') mode |= DFS_CONTROL;
	    else if (tc == 'i') mode |= DFS_INSERT;
	    else if (tc == 'd') mode |= DFS_DELETE;
	    else if (tc == 'A') mode |= DFS_USR0;
	    else if (tc == 'B') mode |= DFS_USR1;
	    else if (tc == 'C') mode |= DFS_USR2;
	    else if (tc == 'D') mode |= DFS_USR3;
	    else if (tc == 'E') mode |= DFS_USR4;
	    else if (tc == 'F') mode |= DFS_USR5;
	    else if (tc == 'G') mode |= DFS_USR6;
	    else if (tc == 'H') mode |= DFS_USR7;
	    else {
		fprintf(stderr, "%s: illegal DFS rights character '%c'.\n", pn, tc);
		exit(1);
	    }
	} else {
	    if (tc == 'r') mode |= PRSFS_READ;
	    else if (tc == 'l') mode |= PRSFS_LOOKUP;
	    else if (tc == 'i') mode |= PRSFS_INSERT;
	    else if (tc == 'd') mode |= PRSFS_DELETE;
	    else if (tc == 'w') mode |= PRSFS_WRITE;
	    else if (tc == 'k') mode |= PRSFS_LOCK;
	    else if (tc == 'a') mode |= PRSFS_ADMINISTER;
	    else if (tc == 'A') mode |= PRSFS_USR0;
	    else if (tc == 'B') mode |= PRSFS_USR1;
	    else if (tc == 'C') mode |= PRSFS_USR2;
	    else if (tc == 'D') mode |= PRSFS_USR3;
	    else if (tc == 'E') mode |= PRSFS_USR4;
	    else if (tc == 'F') mode |= PRSFS_USR5;
	    else if (tc == 'G') mode |= PRSFS_USR6;
	    else if (tc == 'H') mode |= PRSFS_USR7;
	    else {
		fprintf(stderr, "%s: illegal rights character '%c'.\n", pn, tc);
		exit(1);
	    }
	}
    }
    return mode;
}

struct AclEntry *FindList (alist, aname)
    char *aname;
    register struct AclEntry *alist; {
    while (alist) {
        if (!foldcmp(alist->name, aname)) return alist;
        alist = alist->next;
    }
    return 0;
}

/* if no parm specified in a particular slot, set parm to be "." instead */
static void SetDotDefault(aitemp)
struct cmd_item **aitemp; {
    register struct cmd_item *ti;
    if (*aitemp) return;	/* already has value */
    /* otherwise, allocate an item representing "." */
    ti = (struct cmd_item *) malloc(sizeof(struct cmd_item));
    ti->next = (struct cmd_item *) 0;
    ti->data = (char *) malloc(2);
    strcpy(ti->data, ".");
    *aitemp = ti;
}

void ChangeList (al, plus, aname, arights)
    struct Acl *al;
    char *aname;
    afs_int32 arights, plus; {
    struct AclEntry *tlist;
    tlist = (plus ? al->pluslist : al->minuslist);
    tlist = FindList (tlist, aname);
    if (tlist) {
        /* Found the item already in the list. */
        tlist->rights = arights;
        if (plus)
            al->nplus -= PruneList(&al->pluslist, al->dfs);
        else
            al->nminus -= PruneList(&al->minuslist, al->dfs);
        return;
    }
    /* Otherwise we make a new item and plug in the new data. */
    tlist = (struct AclEntry *) malloc(sizeof (struct AclEntry));
    strcpy(tlist->name, aname);
    tlist->rights = arights;
    if (plus) {
        tlist->next = al->pluslist;
        al->pluslist = tlist;
        al->nplus++;
        if (arights == 0 || arights == -1)
	    al->nplus -= PruneList(&al->pluslist, al->dfs);
    }
    else {
        tlist->next = al->minuslist;
        al->minuslist = tlist;
        al->nminus++;
        if (arights == 0) al->nminus -= PruneList(&al->minuslist, al->dfs);
    }
}

void ZapList (alist)
    struct AclEntry *alist; {
    register struct AclEntry *tp, *np;
    for (tp = alist; tp; tp = np) {
        np = tp->next;
        free(tp);
    }
}

int PruneList (ae, dfs)
    struct AclEntry **ae;
    int dfs;
{
    struct AclEntry **lp;
    struct AclEntry *te, *ne;
    afs_int32 ctr;
    ctr = 0;
    lp = ae;
    for(te = *ae;te;te=ne) {
        if ((!dfs && te->rights == 0) || te->rights == -1) {
            *lp = te->next;
            ne = te->next;
            free(te);
            ctr++;
	}
        else {
            ne = te->next;
            lp = &te->next;
	}
    }
    return ctr;
}

char *SkipLine (astr)
    register char *astr; {
    while (*astr !='\n') astr++;
    astr++;
    return astr;
}

/*
 * Create an empty acl, taking into account whether the acl pointed
 * to by astr is an AFS or DFS acl. Only parse this minimally, so we
 * can recover from problems caused by bogus ACL's (in that case, always
 * assume that the acl is AFS: for DFS, the user can always resort to
 * acl_edit, but for AFS there may be no other way out).
 */
struct Acl  *EmptyAcl(astr)
    char *astr;
{
    register struct Acl *tp;
    int junk;

    tp = (struct Acl *)malloc(sizeof (struct Acl));
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
    sscanf(astr, "%d dfs:%d %s", &junk, &tp->dfs, tp->cell);
    return tp;
}

struct Acl *ParseAcl (astr)
    char *astr; {
    int nplus, nminus, i, trights;
    char tname[MAXNAME];
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;

    ta = (struct Acl *) malloc (sizeof (struct Acl));
    ta->dfs = 0;
    sscanf(astr, "%d dfs:%d %s", &ta->nplus, &ta->dfs, ta->cell);
    astr = SkipLine(astr);
    sscanf(astr, "%d", &ta->nminus);
    astr = SkipLine(astr);

    nplus = ta->nplus;
    nminus = ta->nminus;

    last = 0;
    first = 0;
    for(i=0;i<nplus;i++) {
        sscanf(astr, "%100s %d", tname, &trights);
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        if (!first) first = tl;
        strcpy(tl->name, tname);
        tl->rights = trights;
        tl->next = 0;
        if (last) last->next = tl;
        last = tl;
    }
    ta->pluslist = first;

    last = 0;
    first = 0;
    for(i=0;i<nminus;i++) {
        sscanf(astr, "%100s %d", tname, &trights);
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        if (!first) first = tl;
        strcpy(tl->name, tname);
        tl->rights = trights;
        tl->next = 0;
        if (last) last->next = tl;
        last = tl;
    }
    ta->minuslist = first;

    return ta;
}

void PrintStatus(status, name, motd, offmsg)
    VolumeStatus *status;
    char *name;
    char *motd;
    char *offmsg; {
    printf("Volume status for vid = %u named %s\n",status->Vid, name);
    if (*offmsg != 0)
	printf("Current offline message is %s\n",offmsg);
    if (*motd != 0)
	printf("Current message of the day is %s\n",motd);
    printf("Current disk quota is ");
    if (status->MaxQuota != 0) printf("%d\n", status->MaxQuota);
    else printf("unlimited\n");
    printf("Current blocks used are %d\n",status->BlocksInUse);
    printf("The partition has %d blocks available out of %d\n\n",status->PartBlocksAvail, status->PartMaxBlocks);
}

void QuickPrintStatus(status, name)
VolumeStatus *status;
char *name; {
    double QuotaUsed =0.0;
    double PartUsed =0.0;
    int WARN = 0;
    printf("%-25.25s",name);

    if (status->MaxQuota != 0) {
	printf("%10d%10d", status->MaxQuota, status->BlocksInUse);
	QuotaUsed = ((((double)status->BlocksInUse)/status->MaxQuota) * 100.0);
    } else {
	printf("no limit%10d", status->BlocksInUse);
    }
    if (QuotaUsed > 90.0){
	printf(" %5.0f%%<<", QuotaUsed);
	WARN = 1;
    }
    else printf(" %5.0f%%  ", QuotaUsed);
    PartUsed = (100.0 - ((((double)status->PartBlocksAvail)/status->PartMaxBlocks) * 100.0));
    if (PartUsed > 97.0){
	printf(" %9.0f%%<<", PartUsed);
	WARN = 1;
    }
    else printf(" %9.0f%%  ", PartUsed);
    if (WARN){
	printf("\t<<WARNING\n");
    }
    else printf("\n");
}

void QuickPrintSpace(status, name)
VolumeStatus *status;
char *name; {
    double PartUsed =0.0;
    int WARN = 0;
    printf("%-25.25s",name);

    printf("%10d%10d%10d", status->PartMaxBlocks, status->PartMaxBlocks - status->PartBlocksAvail, status->PartBlocksAvail);
	
    PartUsed = (100.0 - ((((double)status->PartBlocksAvail)/status->PartMaxBlocks) * 100.0));
    if (PartUsed > 90.0){
	printf(" %4.0f%%<<", PartUsed);
	WARN = 1;
    }
    else printf(" %4.0f%%  ", PartUsed);
    if (WARN){
	printf("\t<<WARNING\n");
    }
    else printf("\n");
}

char *AclToString(acl)
    struct Acl *acl; {
    static char mydata[MAXSIZE];
    char tstring[MAXSIZE];
    char dfsstring[30];
    struct AclEntry *tp;
    
    if (acl->dfs) sprintf(dfsstring, " dfs:%d %s", acl->dfs, acl->cell);
    else dfsstring[0] = '\0';
    sprintf(mydata, "%d%s\n%d\n", acl->nplus, dfsstring, acl->nminus);
    for(tp = acl->pluslist;tp;tp=tp->next) {
        sprintf(tstring, "%s %d\n", tp->name, tp->rights);
        strcat(mydata, tstring);
    }
    for(tp = acl->minuslist;tp;tp=tp->next) {
        sprintf(tstring, "%s %d\n", tp->name, tp->rights);
        strcat(mydata, tstring);
    }
    return mydata;
}

#define AFSCLIENT_ADMIN_GROUPNAME "AFS Client Admins"

BOOL IsAdmin (void)
{
    static BOOL fAdmin = FALSE;
    static BOOL fTested = FALSE;

    if (!fTested)
    {
        /* Obtain the SID for the AFS client admin group.  If the group does
         * not exist, then assume we have AFS client admin privileges.
         */
        PSID psidAdmin = NULL;
        DWORD dwSize, dwSize2;
        char pszAdminGroup[ MAX_COMPUTERNAME_LENGTH + sizeof(AFSCLIENT_ADMIN_GROUPNAME) + 2 ];
        char *pszRefDomain = NULL;
        SID_NAME_USE snu = SidTypeGroup;

        dwSize = sizeof(pszAdminGroup);

        if (!GetComputerName(pszAdminGroup, &dwSize)) {
            /* Can't get computer name.  We return false in this case.
               Retain fAdmin and fTested. This shouldn't happen.*/
            return FALSE;
        }

        fTested = TRUE;

        dwSize = 0;
        dwSize2 = 0;

        strcat(pszAdminGroup,"\\");
        strcat(pszAdminGroup, AFSCLIENT_ADMIN_GROUPNAME);

        LookupAccountName(NULL, pszAdminGroup, NULL, &dwSize, NULL, &dwSize2, &snu);
        /* that should always fail. */

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            /* if we can't find the group, then we allow the operation */
            fAdmin = TRUE;
            return TRUE;
        }

        if (dwSize == 0 || dwSize2 == 0) {
            /* Paranoia */
            fAdmin = TRUE;
            return TRUE;
        }

        psidAdmin = (PSID)malloc(dwSize); memset(psidAdmin,0,dwSize);
        pszRefDomain = (char *)malloc(dwSize2);

        if (!LookupAccountName(NULL, pszAdminGroup, psidAdmin, &dwSize, pszRefDomain, &dwSize2, &snu)) {
            /* We can't lookup the group now even though we looked it up earlier.  
               Could this happen? */
            fAdmin = TRUE;
        } else {
            /* Then open our current ProcessToken */
            HANDLE hToken;

            if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {
                /* We'll have to allocate a chunk of memory to store the list of
                 * groups to which this user belongs; find out how much memory
                 * we'll need.
                 */
                DWORD dwSize = 0;
                PTOKEN_GROUPS pGroups;
                
                GetTokenInformation (hToken, TokenGroups, NULL, dwSize, &dwSize);
            
                pGroups = (PTOKEN_GROUPS)malloc(dwSize);
                
                /* Allocate that buffer, and read in the list of groups. */
                if (GetTokenInformation (hToken, TokenGroups, pGroups, dwSize, &dwSize))
                {
                    /* Look through the list of group SIDs and see if any of them
                     * matches the AFS Client Admin group SID.
                     */
                    size_t iGroup = 0;
                    for (; (!fAdmin) && (iGroup < pGroups->GroupCount); ++iGroup)
                    {
                        if (EqualSid (psidAdmin, pGroups->Groups[ iGroup ].Sid)) {
                            fAdmin = TRUE;
                        }
                    }
                }

                if (pGroups)
                    free(pGroups);
            }
        }

        free(psidAdmin);
        free(pszRefDomain);
    }

    return fAdmin;
}

static SetACLCmd(as)
struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    struct Acl *ta;
    register struct cmd_item *ti, *ui;
    int plusp;
    afs_int32 rights;
    int clear;
    int idf = getidf(as, parm_setacl_id);

    if (as->parms[2].items) clear=1;
    else clear=0;
    plusp = !(as->parms[3].items);
    for(ti=as->parms[0].items; ti;ti=ti->next) {
	blob.out_size = MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    return 1;
	}
	ta = ParseAcl(space);
	if (!plusp && ta->dfs) {
	    fprintf(stderr,
		    "fs: %s: you may not use the -negative switch with DFS acl's.\n%s",
		    ti->data,
		    "(you may specify \"null\" to revoke all rights, however)\n");
	    return 1;
	}
	if (clear) ta = EmptyAcl(space);
	else ta = ParseAcl(space);
	CleanAcl(ta);
	for(ui=as->parms[1].items; ui; ui=ui->next->next) {
	    enum rtype rtype;
	    if (!ui->next) {
		fprintf(stderr,"%s: Missing second half of user/access pair.\n", pn);
		return 1;
	    }
	    rights = Convert(ui->next->data, ta->dfs, &rtype);
	    if (rtype == destroy && !ta->dfs) {
		if (!FindList(ta->pluslist, ui->data)) {
		    fprintf(stderr,"%s: Invalid arg (%s doesn't exist in the current acl)\n", pn, ui->data);
		    return 1;
		}
	    }
	    if (rtype == deny && !ta->dfs) plusp = 0;
	    if (rtype == destroy && ta->dfs) rights = -1;
	    ChangeList(ta, plusp, ui->data, rights);
	}
	blob.in = AclToString(ta);
	blob.out_size=0;
	blob.in_size = 1+strlen(blob.in);
	code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	if (code) {
	    if (errno == EINVAL) {
		if (ta->dfs) {
		    static char *fsenv = 0;
		    if (!fsenv) {
			fsenv = (char *)getenv("FS_EXPERT");
		    }
		    fprintf(stderr, "fs: \"Invalid argument\" was returned when you tried to store a DFS access list.\n");
		    if (!fsenv) {
			fprintf(stderr,
    "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
    "\nPossible reasons for this include:\n\n",			    
    " -You may have specified an inappropriate combination of rights.\n",
    "  For example, some DFS-supported filesystems may not allow you to\n",
    "  drop the \"c\" right from \"user_obj\".\n\n",
    " -A mask_obj may be required (it is likely required by the underlying\n",
    "  filesystem if you try to set anything other than the basic \"user_obj\"\n",
    "  \"mask_obj\", or \"group_obj\" entries). Unlike acl_edit, the fs command\n",
    "  does not automatically create or update the mask_obj. Try setting\n",
    "  the rights \"mask_obj all\" with \"fs sa\" before adding any explicit\n",
    "  users or groups. You can do this with a single command, such as\n",
    "  \"fs sa mask_obj all user:somename read\"\n\n",
    " -A specified user or group may not exist.\n\n",
    " -You may have tried to delete \"user_obj\", \"group_obj\", or \"other_obj\".\n",
    "  This is probably not allowed by the underlying file system.\n\n",
    " -If you add a user or group to a DFS ACL, remember that it must be\n",
    "  fully specified as \"user:username\" or \"group:groupname\". In addition, there\n",
    "  may be local requirements on the format of the user or group name.\n",
    "  Check with your cell administrator.\n\n",			    
    " -Or numerous other possibilities. It would be great if we could be more\n",
    "  precise about the actual problem, but for various reasons, this is\n",
    "  impractical via this interface.  If you can't figure it out, you\n",
    "  might try logging into a DCE-equipped machine and use acl_edit (or\n",
    "  whatever is provided). You may get better results. Good luck!\n\n",
    " (You may inhibit this message by setting \"FS_EXPERT\" in your environment)\n");
		    }
		} else {
		    fprintf(stderr,"%s: Invalid argument, possible reasons include:\n", pn);
		    fprintf(stderr,"\t-File not in AFS\n");
		    fprintf(stderr,"\t-Too many users on access control list\n");
		    fprintf(stderr,"\t-Tried to add non-existent user to access control list\n");
		}
		return 1;
	    }
	    else {
		Die(errno, ti->data);
		continue;
	    }
	}
    }
    return 0;
}


static CopyACLCmd(as)
struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    struct Acl *fa, *ta;
    struct AclEntry *tp;
    register struct cmd_item *ti;
    int clear;
    int idf = getidf(as, parm_copyacl_id);

    if (as->parms[2].items) clear=1;
    else clear=0;
    blob.out_size = MAXSIZE;
    blob.in_size = idf;
    blob.in = blob.out = space;
    code = pioctl(as->parms[0].items->data, VIOCGETAL, &blob, 1);
    if (code) {
	Die(errno, as->parms[0].items->data);
	return 1;
    }
    fa = ParseAcl(space);
    CleanAcl(fa);
    for (ti=as->parms[1].items; ti;ti=ti->next) {
	blob.out_size = MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    return 1;
	}
	if (clear) ta = EmptyAcl(space);
	else ta = ParseAcl(space);
	CleanAcl(ta);
	if (ta->dfs != fa->dfs) {
	    fprintf(stderr, "fs: incompatible file system types: acl not copied to %s; aborted\n", ti->data);
	    goto next;
	}
	if (ta->dfs) {
	    if (! clear && strcmp(ta->cell, fa->cell) != 0) {
		fprintf(stderr, "fs: default DCE cell differs for file %s: use \"-clear\" switch; acl not merged\n", ti->data);
		goto next;
	    }
	    strcpy(ta->cell, fa->cell);
	}
	for (tp = fa->pluslist;tp;tp=tp->next) 
	    ChangeList(ta, 1, tp->name, tp->rights);
	for (tp = fa->minuslist;tp;tp=tp->next) 
	    ChangeList(ta, 0, tp->name, tp->rights);
	blob.in = AclToString(ta);
	blob.out_size=0;
	blob.in_size = 1+strlen(blob.in);
	code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	if (code) {
	    if (errno == EINVAL) {
		fprintf(stderr,"%s: Invalid argument, possible reasons include:\n", pn);
		fprintf(stderr,"\t-File not in AFS\n");
		return 1;
	    }
	    else {
		Die(errno, ti->data);
		goto next;
	    }
	}
      next:
	ZapAcl(ta);
    }
    return 0;
}



/* tell if a name is 23 or -45 (digits or minus digits), which are bad names we must prune */
static BadName(aname)
register char *aname; {
    register int tc;
    while(tc = *aname++) {
	/* all must be '-' or digit to be bad */
	if (tc != '-' && (tc < '0' || tc > '9')) return 0;
    }
    return 1;
}


/* clean up an access control list of its bad entries; return 1 if we made
   any changes to the list, and 0 otherwise */
int CleanAcl(aa)
struct Acl *aa; {
    register struct AclEntry *te, **le, *ne;
    int changes;

    /* Don't correct DFS ACL's for now */
    if (aa->dfs)
	return 0;

    /* prune out bad entries */
    changes = 0;	    /* count deleted entries */
    le = &aa->pluslist;
    for(te = aa->pluslist; te; te=ne) {
	ne = te->next;
	if (BadName(te->name)) {
	    /* zap this dude */
	    *le = te->next;
	    aa->nplus--;
	    free(te);
	    changes++;
	}
	else {
	    le = &te->next;
	}
    }
    le = &aa->minuslist;
    for(te = aa->minuslist; te; te=ne) {
	ne = te->next;
	if (BadName(te->name)) {
	    /* zap this dude */
	    *le = te->next;
	    aa->nminus--;
	    free(te);
	    changes++;
	}
	else {
	    le = &te->next;
	}
    }
    return changes;
}


/* clean up an acl to not have bogus entries */
static CleanACLCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    register struct Acl *ta;
    struct ViceIoctl blob;
    int changes;
    register struct cmd_item *ti;
    register struct AclEntry *te;

    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    continue;
	}
	ta = ParseAcl(space);

	if (ta->dfs) {
	    fprintf(stderr,
		    "%s: cleanacl is not supported for DFS access lists.\n",
		    pn);
	    return 1;
	}

	changes = CleanAcl(ta);

	if (changes) {
	    /* now set the acl */
	    blob.in=AclToString(ta);
	    blob.in_size = strlen(blob.in)+1;
	    blob.out_size = 0;
	    code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	    if (code) {
		if (errno == EINVAL) {
		    fprintf(stderr,"%s: Invalid argument, possible reasons include\n", pn);
		    fprintf(stderr,"%s: File not in vice or\n", pn);
		    fprintf(stderr,"%s: Too many users on access control list or\n", pn);
		    return 1;
		}
		else {
		    Die(errno, ti->data);
		    continue;
		}
	    }

	    /* now list the updated acl */
	    printf("Access list for %s is now\n", ti->data);
	    if (ta->nplus > 0) {
		if (!ta->dfs) printf("Normal rights:\n");
		for(te = ta->pluslist;te;te=te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
		}
	    }
	    if (ta->nminus > 0) {
		printf("Negative rights:\n");
		for(te = ta->minuslist;te;te=te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
		}
	    }
	    if (ti->next) printf("\n");
	}
	else
	    printf("Access list for %s is fine.\n", ti->data);
    }
    return 0;
}

static ListACLCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    register struct Acl *ta;
    struct ViceIoctl blob;
    struct AclEntry *te;
    register struct cmd_item *ti;
    int idf = getidf(as, parm_listacl_id);

    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	char separator;
	blob.out_size = MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    continue;
	}
	ta = ParseAcl(space);
	switch (ta->dfs) {
	  case 0:
	    printf("Access list for %s is\n", ti->data);
	    break;
	  case 1:
	    printf("DFS access list for %s is\n", ti->data);
	    break;
	  case 2:
	    printf("DFS initial directory access list of %s is\n", ti->data);
	    break;
	  case 3:
	    printf("DFS initial file access list of %s is\n", ti->data);
	    break;
	}
	if (ta->dfs) {
	    printf("  Default cell = %s\n", ta->cell);
	}
	separator = ta->dfs? DFS_SEPARATOR : ' ';
	if (ta->nplus > 0) {
	    if (!ta->dfs) printf("Normal rights:\n");
	    for(te = ta->pluslist;te;te=te->next) {
		printf("  %s%c", te->name, separator);
		PRights(te->rights, ta->dfs);
		printf("\n");
	    }
	}
	if (ta->nminus > 0) {
	    printf("Negative rights:\n");
	    for(te = ta->minuslist;te;te=te->next) {
		printf("  %s ", te->name);
		PRights(te->rights, ta->dfs);
		printf("\n");
	    }
	}
	if (ti->next) printf("\n");
    }
    return 0;
}

static FlushVolumeCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;

    for(ti=as->parms[0].items; ti; ti=ti->next) {
	blob.in_size = blob.out_size = 0;
	code = pioctl(ti->data, VIOC_FLUSHVOLUME, &blob, 0);
	if (code) {
	    fprintf(stderr, "Error flushing volume\n");
	    Die(errno,ti->data);
	    continue;
	}
    }
    return 0;
}

static FlushCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    int error;
    register struct cmd_item *ti;

    error = 0;
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	blob.in_size = blob.out_size = 0;
	code = pioctl(ti->data, VIOCFLUSH, &blob, 0);
	if (code) {
	    error = 1;
	    if (errno == EMFILE) {
		fprintf(stderr,"%s: Can't flush active file %s\n", pn, ti->data);
		continue;
	    }
	    else {
		fprintf(stderr, "Error flushing file\n");
		Die(errno,ti->data);
		continue;
	    }
	}
    }
    return error;
}

/* all this command does is repackage its args and call SetVolCmd */
static SetQuotaCmd(as)
register struct cmd_syndesc *as; {
    struct cmd_syndesc ts;

    /* copy useful stuff from our command slot; we may later have to reorder */
    memcpy(&ts, as, sizeof(ts));	/* copy whole thing */
    return SetVolCmd(&ts);
}

static SetVolCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    struct VolumeStatus *status;
    char *motd, *offmsg, *input;

    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = sizeof(*status) + 3;	/* for the three terminating nulls */
	blob.out = space;
	blob.in = space;
	status = (VolumeStatus *)space;
	status->MinQuota = status->MaxQuota = -1;
	motd = offmsg = (char *) 0;
	if (as->parms[1].items) {
	    code = util_GetInt32(as->parms[1].items->data, &status->MaxQuota);
	    if (code) {
		fprintf(stderr,"fs: bad integer specified for quota.\n");
		return code;
	    }
	}
	if (as->parms[2].items) motd = as->parms[2].items->data;
	if (as->parms[3].items) offmsg = as->parms[3].items->data;
	input = (char *)status + sizeof(*status);
	*(input++) = '\0';	/* never set name: this call doesn't change vldb */
	if(offmsg) {
	  if (strlen(offmsg) >= VMSGSIZE) {
	    fprintf(stderr,"fs: message must be shorter than %d characters\n",
		    VMSGSIZE);
	    return code;
	  }
	    strcpy(input,offmsg);
	    blob.in_size += strlen(offmsg);
	    input += strlen(offmsg) + 1;
	}
	else *(input++) = '\0';
	if(motd) {
	  if (strlen(motd) >= VMSGSIZE) {
	    fprintf(stderr,"fs: message must be shorter than %d characters\n",
		    VMSGSIZE);
	    return code;
	  }
	    strcpy(input,motd);
	    blob.in_size += strlen(motd);
	    input += strlen(motd) + 1;
	}
	else *(input++) = '\0';
	code = pioctl(ti->data,VIOCSETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    return 1;
	}
    }
    return 0;
}

static ExamineCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name, *offmsg, *motd;
    int error;
    
    error = 0;
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	status = (VolumeStatus *)space;
	name = (char *)status + sizeof(*status);
	offmsg = name + strlen(name) + 1;
	motd = offmsg + strlen(offmsg) + 1;
	PrintStatus(status, name, motd, offmsg);
    }
    return error;
}

static ListQuotaCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name;
    
    printf("%-25s%-10s%-10s%-7s%-13s\n", 
           "Volume Name", "     Quota", "      Used", "  %Used", "    Partition");
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    continue;
	}
	status = (VolumeStatus *)space;
	name = (char *)status + sizeof(*status);
	QuickPrintStatus(status, name);
    }
    return 0;
}

static WhereIsCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    register int j;
    afs_int32 *hosts;
    char *tp;
    
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	memset(space, 0, sizeof(space));
	code = pioctl(ti->data, VIOCWHEREIS, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    continue;
	}
	hosts = (afs_int32 *) space;
	printf("File %s is on host%s ", ti->data, (hosts[0] && !hosts[1]) ? "": "s");
	for(j=0; j<MAXHOSTS; j++) {
	    if (hosts[j] == 0) break;
	    tp = hostutil_GetNameByINet(hosts[j]);
	    printf("%s ", tp);
	}
	printf("\n");
    }
    return 0;
}


static DiskFreeCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    char *name;
    struct VolumeStatus *status;
    
    printf("%-25s%-10s%-10s%-10s%-6s\n", "Volume Name", "    kbytes",
	   "      used", "     avail", " %used");
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    continue;
	}
	status = (VolumeStatus *)space;
	name = (char *)status + sizeof(*status);
	QuickPrintSpace(status, name);
    }
    return 0;
}

static QuotaCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    double quotaPct;
    struct VolumeStatus *status;
    
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    continue;
	}
	status = (VolumeStatus *)space;
	if (status->MaxQuota) quotaPct = ((((double)status->BlocksInUse)/status->MaxQuota) * 100.0);
	else quotaPct = 0.0;
	printf("%2.0f%% of quota used.\n", quotaPct);
    }
    return 0;
}

static ListMountCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    int error;
    register struct cmd_item *ti;
    char orig_name[1024];		/*Original name, may be modified*/
    char true_name[1024];		/*``True'' dirname (e.g., symlink target)*/
    char parent_dir[1024];		/*Parent directory of true name*/
    register char *last_component;	/*Last component of true name*/
#ifndef WIN32
    struct stat statbuff;		/*Buffer for status info*/
#endif /* not WIN32 */
#ifndef WIN32
    int link_chars_read;		/*Num chars read in readlink()*/
#endif /* not WIN32 */
    int	thru_symlink;			/*Did we get to a mount point via a symlink?*/
    
    error = 0;
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	thru_symlink = 0;
#ifdef WIN32
	strcpy(orig_name, ti->data);
#else /* not WIN32 */
	sprintf(orig_name, "%s%s",
		(ti->data[0] == '/') ? "" : "./",
		ti->data);
#endif /* not WIN32 */

#ifndef WIN32
	if (lstat(orig_name, &statbuff) < 0) {
	    /* if lstat fails, we should still try the pioctl, since it
		may work (for example, lstat will fail, but pioctl will
		    work if the volume of offline (returning ENODEV). */
	    statbuff.st_mode = S_IFDIR; /* lie like pros */
	}

	/*
	 * The lstat succeeded.  If the given file is a symlink, substitute
	 * the file name with the link name.
	 */
	if ((statbuff.st_mode & S_IFMT) == S_IFLNK) {
	    thru_symlink = 1;
	    /*
	     * Read name of resolved file.
	     */
	    link_chars_read = readlink(orig_name, true_name, 1024);
	    if (link_chars_read <= 0) {
		fprintf(stderr,"%s: Can't read target name for '%s' symbolic link!\n",
		       pn, orig_name);
		exit(1);
	    }

	    /*
	     * Add a trailing null to what was read, bump the length.
	     */
	    true_name[link_chars_read++] = 0;

	    /*
	     * If the symlink is an absolute pathname, we're fine.  Otherwise, we
	     * have to create a full pathname using the original name and the
	     * relative symlink name.  Find the rightmost slash in the original
	     * name (we know there is one) and splice in the symlink value.
	     */
	    if (true_name[0] != '\\') {
		last_component = (char *) strrchr(orig_name, '\\');
		strcpy(++last_component, true_name);
		strcpy(true_name, orig_name);
	    }
	}
	else
	    strcpy(true_name, orig_name);
#else	/* WIN32 */
	strcpy(true_name, orig_name);
#endif /* WIN32 */

	/*
	 * Find rightmost slash, if any.
	 */
	last_component = (char *) strrchr(true_name, '\\');
	if (!last_component)
	    last_component = (char *) strrchr(true_name, '/');
	if (last_component) {
	    /*
	     * Found it.  Designate everything before it as the parent directory,
	     * everything after it as the final component.
	     */
	    strncpy(parent_dir, true_name, last_component - true_name + 1);
	    parent_dir[last_component - true_name + 1] = 0;
	    last_component++;   /*Skip the slash*/
	}
	else {
	    /*
	     * No slash appears in the given file name.  Set parent_dir to the current
	     * directory, and the last component as the given name.
	     */
	    fs_ExtractDriveLetter(true_name, parent_dir);
	    strcat(parent_dir, ".");
	    last_component = true_name;
            fs_StripDriveLetter(true_name, true_name, sizeof(true_name));
	}

	if (strcmp(last_component, ".") == 0 || strcmp(last_component, "..") == 0) {
	    fprintf(stderr,"fs: you may not use '.' or '..' as the last component\n");
	    fprintf(stderr,"fs: of a name in the 'fs lsmount' command.\n");
	    error = 1;
	    continue;
	}

	blob.in = last_component;
	blob.in_size = strlen(last_component)+1;
	blob.out_size = MAXSIZE;
	blob.out = space;
	memset(space, 0, MAXSIZE);

	code = pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, &blob, 1);

	if (code == 0)
	    printf("'%s' is a %smount point for volume '%s'\n",
		   ti->data,
		   (thru_symlink ? "symbolic link, leading to a " : ""),
		   space);

	else {
	    error = 1;
	    if (errno == EINVAL)
		fprintf(stderr,"'%s' is not a mount point.\n",
		       ti->data);
	    else {
		Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	}
    }
    return error;
}

static MakeMountCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    register char *cellName, *volName, *tmpName;
#ifdef WIN32
    char localCellName[1000];
#else /* not WIN32 */
    struct afsconf_cell info;
    struct vldbentry vldbEntry;
#endif /* not WIN32 */
    struct ViceIoctl blob;

/*

defect #3069

    if (as->parms[5].items && !as->parms[2].items) {
	fprintf(stderr,"fs: must provide cell when creating cellular mount point.\n");
	return 1;
    }
*/

    if (as->parms[2].items)	/* cell name specified */
	cellName = as->parms[2].items->data;
    else
	cellName = (char *) 0;
    volName = as->parms[1].items->data;

    if (strlen(volName) >= 64) {
	fprintf(stderr,"fs: volume name too long (length must be < 64 characters)\n");
	return 1;
    }

    /* Check for a cellname in the volume specification, and complain
     * if it doesn't match what was specified with -cell */
    if (tmpName = strchr(volName, ':')) {
      *tmpName = '\0';
      if (cellName) {
	if (foldcmp(cellName,volName)) {
	  fprintf(stderr,"fs: cellnames do not match.\n");
	  return 1;
	}
      }
      cellName = volName;
      volName = ++tmpName;
    }

    if (!InAFS(Parent(as->parms[0].items->data))) {
	fprintf(stderr,"fs: mount points must be created within the AFS file system\n");
	return 1;
    }

    if (!cellName) {
	blob.in_size = 0;
	blob.out_size = MAXSIZE;
	blob.out = space;
	code = pioctl(Parent(as->parms[0].items->data), VIOC_FILE_CELL_NAME, &blob, 1);
    }

#ifdef WIN32
    strcpy(localCellName, (cellName? cellName : space));
#else /* not win32 */
    code = GetCellName(cellName?cellName:space, &info);
    if (code) {
	return 1;
    }
    if (!(as->parms[4].items)) {
      /* not fast, check which cell the mountpoint is being created in */
      code = 0;
	/* not fast, check name with VLDB */
      if (!code)
	code = VLDBInit(1, &info);
      if (code == 0) {
	  /* make the check.  Don't complain if there are problems with init */
	  code = ubik_Call(VL_GetEntryByNameO, uclient, 0, volName, &vldbEntry);
	  if (code == VL_NOENT) {
	      fprintf(stderr,"fs: warning, volume %s does not exist in cell %s.\n",
		      volName, cellName ? cellName : space);
	  }
      }
    }
#endif /* not WIN32 */

    if (as->parms[3].items)	/* if -rw specified */
	strcpy(space, "%");
    else
	strcpy(space, "#");
    if (cellName) {
	/* cellular mount point, prepend cell prefix */
#ifdef WIN32
	strcat(space, localCellName);
#else /* not WIN32 */
	strcat(space, info.name);
#endif /* not WIN32 */
	strcat(space, ":");
    }
    strcat(space, volName);	/* append volume name */
    strcat(space, ".");		/* stupid convention; these end with a period */
#ifdef WIN32
    /* create symlink with a special pioctl for Windows NT, since it doesn't
     * have a symlink system call.
     */
    blob.out_size = 0;
    blob.in_size = 1 + strlen(space);
    blob.in = space;
    blob.out = NULL;
    code = pioctl(as->parms[0].items->data, VIOC_AFS_CREATE_MT_PT, &blob, 0);
#else /* not WIN32 */
    code = symlink(space, as->parms[0].items->data);
#endif /* not WIN32 */
    if (code) {
	Die(errno, as->parms[0].items->data);
	return 1;
    }
    return 0;
}

/*
 * Delete AFS mount points.  Variables are used as follows:
 *       tbuffer: Set to point to the null-terminated directory name of the mount point
 *	    (or ``.'' if none is provided)
 *      tp: Set to point to the actual name of the mount point to nuke.
 */
static RemoveMountCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code=0;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    char tbuffer[1024];
    char lsbuffer[1024];
    register char *tp;
    
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	tp = (char *) strrchr(ti->data, '\\');
	if (!tp)
	    tp = (char *) strrchr(ti->data, '/');
	if (tp) {
	    strncpy(tbuffer, ti->data, code=tp-ti->data+1);  /* the dir name */
            tbuffer[code] = 0;
	    tp++;   /* skip the slash */
	}
	else {
	    fs_ExtractDriveLetter(ti->data, tbuffer);
	    strcat(tbuffer, ".");
	    tp = ti->data;
            fs_StripDriveLetter(tp, tp, 0);
	}
	blob.in = tp;
	blob.in_size = strlen(tp)+1;
	blob.out = lsbuffer;
	blob.out_size = sizeof(lsbuffer);
	code = pioctl(tbuffer, VIOC_AFS_STAT_MT_PT, &blob, 0);
	if (code) {
	    if (errno == EINVAL)
		fprintf(stderr,"fs: '%s' is not a mount point.\n", ti->data);
	    else {
		Die(errno, ti->data);
	    }
	    continue;	/* don't bother trying */
	}
	blob.out_size = 0;
	blob.in = tp;
	blob.in_size = strlen(tp)+1;
	code = pioctl(tbuffer, VIOC_AFS_DELETE_MT_PT, &blob, 0);
	if (code) {
	    Die(errno, ti->data);
	}

    }
    return code;
}

/*
*/

static CheckServersCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register afs_int32 j;
    afs_int32 temp;
    register char *tp;
    struct afsconf_cell info;
    struct chservinfo checkserv;

    memset(&checkserv, 0, sizeof(struct chservinfo));
    blob.in_size=sizeof(struct chservinfo);
    blob.in=(caddr_t)&checkserv;

    blob.out_size = MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(afs_int32));	/* so we assure zero when nothing is copied back */

    /* prepare flags for checkservers command */
    temp = 2;	/* default to checking local cell only */
    if (as->parms[2].items) temp |= 1;	/* set fast flag */
    if (as->parms[1].items) temp &= ~2;	/* turn off local cell check */
    
    checkserv.magic = 0x12345678;	/* XXX */
    checkserv.tflags=temp;

    /* now copy in optional cell name, if specified */
    if (as->parms[0].items) {
	code = GetCellName(as->parms[0].items->data, &info);
	if (code) {
	    return 1;
	}
	strcpy(checkserv.tbuffer,info.name);
	checkserv.tsize=strlen(info.name)+1;
    }
	else
	{
		strcpy(checkserv.tbuffer,"\0");
		checkserv.tsize=0;
	}

	if(as->parms[3].items)
	{
		checkserv.tinterval=atol(as->parms[3].items->data);

		/* sanity check */
		if(checkserv.tinterval<0) {
		    printf("Warning: The negative -interval is ignored; treated as an inquiry\n");
		    checkserv.tinterval=0;
		} else if(checkserv.tinterval> 600) {
		    printf("Warning: The maximum -interval value is 10 mins (600 secs)\n");
		    checkserv.tinterval=600;	/* 10 min max interval */
        }
	}
	else {
        checkserv.tinterval = -1;	/* don't change current interval */
	}

    if ( checkserv.tinterval != 0 ) {
#ifdef WIN32
        if ( !IsAdmin() ) {
            fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
            return EACCES;
        }
#else /* WIN32 */
        if (geteuid()) {
            fprintf (stderr,"Permission denied: requires root access.\n");
            return EACCES;
        }
#endif /* WIN32 */
    }

    code = pioctl(0, VIOCCKSERV, &blob, 1);
    if (code) {
	if ((errno == EACCES) && (checkserv.tinterval > 0)) {
	    printf("Must be root to change -interval\n");
	    return code;
	}
	Die(errno, 0);
        return code;
    }
    memcpy(&temp, space, sizeof(afs_int32));
    if (checkserv.tinterval >= 0) {
	if (checkserv.tinterval > 0) 
	    printf("The new down server probe interval (%d secs) is now in effect (old interval was %d secs)\n", 
		   checkserv.tinterval, temp);
	else 
	    printf("The current down server probe interval is %d secs\n", temp);
	return 0;
    }
    if (temp == 0) {
	printf("All servers are running.\n");
    }
    else {
	printf("These servers unavailable due to network or server problems: ");
	for(j=0; j < MAXHOSTS; j++) {
	    memcpy(&temp, space + j*sizeof(afs_int32), sizeof(afs_int32));
	    if (temp == 0) break;
	    tp = hostutil_GetNameByINet(temp);
	    printf(" %s", tp);
	}
	printf(".\n");
	code = 1;	/* XXX */
    }
    return code;
}

static GagCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code=0;
    struct ViceIoctl blob;
    struct gaginfo gagflags;
    struct cmd_item *show;
    
    memset(&gagflags, 0, sizeof(struct gaginfo));
    blob.in_size = sizeof(struct gaginfo);
    blob.in = (caddr_t ) &gagflags;
    blob.out_size = MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(afs_int32));	/* so we assure zero when nothing is copied back */

    if (show = as->parms[0].items)
       if (!foldcmp (show->data, "user"))
          gagflags.showflags |= GAGUSER;
       else if (!foldcmp (show->data, "console"))
          gagflags.showflags |= GAGCONSOLE;
       else if (!foldcmp (show->data, "all"))
          gagflags.showflags |= GAGCONSOLE | GAGUSER;
       else if (!foldcmp (show->data, "none"))
          /* do nothing */ ;
       else {
	 fprintf(stderr, 
	  "unrecognized flag %s: must be in {user,console,all,none}\n", 
	  show->data);
	 code = EINVAL;
       }
 
    if (code)
      return code;

    code = pioctl(0, VIOC_GAG, &blob, 1);
    if (code) {
	Die(errno, 0);
        return code;
    }
    return code;
}

static CheckVolumesCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    
    blob.in_size = 0;
    blob.out_size = 0;
    code = pioctl(0, VIOCCKBACK, &blob, 1);
    if (code) {
	Die(errno, 0);
	return 1;
    }
    else printf("All volumeID/name mappings checked.\n");
    
    return 0;
}

static SetCacheSizeCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 temp;
    
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    if (!as->parms[0].items && !as->parms[1].items) {
	fprintf(stderr,"%s: syntax error in set cache size cmd.\n", pn);
	return 1;
    }
    if (as->parms[0].items) {
	code = util_GetInt32(as->parms[0].items->data, &temp);
	if (code) {
	    fprintf(stderr,"fs: bad integer specified for cache size.\n");
	    return code;
	}
    }
    else
	temp = 0;
    blob.in = (char *) &temp;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;
    code = pioctl(0, VIOCSETCACHESIZE, &blob, 1);
    if (code)
	Die(errno, (char *) 0);
    else
	printf("New cache size set.\n");
    return 0;
}

#define MAXGCSIZE	16
static GetCacheParmsCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 parms[MAXGCSIZE];
    
    blob.in = (char *) 0;
    blob.in_size = 0;
    blob.out_size = sizeof(parms);
    blob.out = (char *) parms;
    code = pioctl(0, VIOCGETCACHEPARMS, &blob, 1);
    if (code)
	Die(errno, (char *) 0);
    else {
	printf("AFS using %d of the cache's available %d 1K byte blocks.\n",
	       parms[1], parms[0]);
	if (parms[1] > parms[0])
	    printf("[Cache guideline temporarily deliberately exceeded; it will be adjusted down but you may wish to increase the cache size.]\n");
    }
    return 0;
}

static ListCellsCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    afs_int32 i, j, *lp, magic, size;
    char *tcp, *tp;
    afs_int32 clear, maxa = OMAXHOSTS;
    struct ViceIoctl blob;
    
    for(i=0;i<1000;i++) {
	tp = space;
	memcpy(tp, &i, sizeof(afs_int32));
	tp = (char *)(space + sizeof(afs_int32));
	lp = (afs_int32 *)tp;
	*lp++ = 0x12345678;
	size = sizeof(afs_int32) + sizeof(afs_int32);
	blob.out_size = MAXSIZE;
	blob.in_size = sizeof(afs_int32);
	blob.in = space;
	blob.out = space;
	code = pioctl(0, VIOCGETCELL, &blob, 1);
	if (code < 0) {
	    if (errno == EDOM) break;	/* done with the list */
	    else {
		Die(errno, 0);
		return 1;
	    }
	}
	tp = space;
	memcpy(&magic, tp, sizeof(afs_int32));	
	if (magic == 0x12345678) {
	    maxa = MAXHOSTS;
	    tp += sizeof(afs_int32);
	}
	printf("Cell %s on hosts", tp+maxa*sizeof(afs_int32));
	for(j=0; j < maxa; j++) {
	    memcpy(&clear, tp + j*sizeof(afs_int32), sizeof(afs_int32));
	    if (clear == 0) break;
	    tcp = hostutil_GetNameByINet(clear);
	    printf(" %s", tcp);
	}
	printf(".\n");
    }
    return 0;
}

static NewCellCmd(as)
register struct cmd_syndesc *as; {
#ifndef WIN32
    register afs_int32 code, linkedstate=0, size=0, *lp;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    register char *tp, *cellname=0;
    register struct hostent *thp;
    afs_int32 fsport = 0, vlport = 0;

#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    memset(space, 0, MAXHOSTS * sizeof(afs_int32));
    tp = space;
    lp = (afs_int32 *)tp;
    *lp++ = 0x12345678;
    tp += sizeof(afs_int32);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	thp = hostutil_GetHostByName(ti->data);
	if (!thp) {
	    fprintf(stderr,"%s: Host %s not found in host table, skipping it.\n",
		   pn, ti->data);
	}
	else {
	    memcpy(tp, thp->h_addr, sizeof(afs_int32));
	    tp += sizeof(afs_int32);
	}
    }
    if (as->parms[2].items) {
	/*
	 * Link the cell, for the purposes of volume location, to the specified
	 * cell.
	 */
	cellname = as->parms[2].items->data;
	linkedstate = 1;
    }
#ifdef FS_ENABLE_SERVER_DEBUG_PORTS
    if (as->parms[3].items) {
	code = util_GetInt32(as->parms[3].items->data, &vlport);
	if (code) {
	    fprintf(stderr,"fs: bad integer specified for the fileserver port.\n");
	    return code;
	}
    }
    if (as->parms[4].items) {
	code = util_GetInt32(as->parms[4].items->data, &fsport);
	if (code) {
	    fprintf(stderr,"fs: bad integer specified for the vldb server port.\n");
	    return code;
	}
    }
#endif
    tp = (char *)(space + (MAXHOSTS+1) *sizeof(afs_int32));
    lp = (afs_int32 *)tp;    
    *lp++ = fsport;
    *lp++ = vlport;
    *lp = linkedstate;
    strcpy(space +  ((MAXHOSTS+4) * sizeof(afs_int32)), as->parms[0].items->data);
    size = ((MAXHOSTS+4) * sizeof(afs_int32)) + strlen(as->parms[0].items->data) + 1 /* for null */;
    tp = (char *)(space + size);
    if (linkedstate) {
	strcpy(tp, cellname);
	size += strlen(cellname) + 1;
    }
    blob.in_size = size;
    blob.in = space;
    blob.out_size = 0;
    code = pioctl(0, VIOCNEWCELL, &blob, 1);
    if (code < 0)
	Die(errno, 0);
    return 0;
#else /* WIN32 */
    register afs_int32 code;
    struct ViceIoctl blob;
    
    blob.in_size = 0;
    blob.in = (char *) 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl((char *) 0, VIOCNEWCELL, &blob, 1);

    if (code) {
       Die(errno, (char *) 0);
    }
    else
       printf("Cell servers information refreshed\n");
    return 0;
#endif /* WIN32 */
}

static WhichCellCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    int error;
    
    error = 0;	/* no error occurred */
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.in_size = 0;
	blob.out_size = MAXSIZE;
	blob.out = space;

	code = pioctl(ti->data, VIOC_FILE_CELL_NAME, &blob, 1);
	if (code) {
	    if (errno == ENOENT)
		fprintf(stderr,"%s: no such cell as '%s'\n", pn, ti->data);
	    else
		Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	else
	    printf("File %s lives in cell '%s'\n", ti->data, space);
    }
    return error;
}

static WSCellCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    
    blob.in_size = 0;
    blob.in = (char *) 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl((char *) 0, VIOC_GET_WS_CELL, &blob, 1);

    if (code) {
	Die(errno, (char *) 0);
    }
    else
	printf("This workstation belongs to cell '%s'\n", space);
    return 0;
}

static PrimaryCellCmd(as)
register struct cmd_syndesc *as; {
/*
    fprintf(stderr,"This command is obsolete, as is the concept of a primary token.\n");
*/
    return 0;
}

static MonitorCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    afs_int32 hostAddr;
    register struct hostent *thp;
    char *tp;
    int setp;
    
    ti = as->parms[0].items;
    setp = 1;
    if (ti) {
	/* set the host */
	if (!strcmp(ti->data, "off"))
	    hostAddr = 0xffffffff;
	else {
	    thp = hostutil_GetHostByName(ti->data);
	    if (!thp) {
		if (!strcmp(ti->data, "localhost")) {
		    fprintf(stderr,"localhost not in host table, assuming 127.0.0.1\n");
		    hostAddr = htonl(0x7f000001);
		}
		else {
		    fprintf(stderr,"host %s not found in host table.\n", ti->data);
		    return 1;
		}
	    }
	    else memcpy(&hostAddr, thp->h_addr, sizeof(afs_int32));
	}
    }
    else {
	hostAddr = 0;	/* means don't set host */
	setp = 0;	/* aren't setting host */
    }

    /* now do operation */
    blob.in_size = sizeof(afs_int32);
    blob.out_size = sizeof(afs_int32);
    blob.in = (char *) &hostAddr;
    blob.out = (char *) &hostAddr;
    code = pioctl(0, VIOC_AFS_MARINER_HOST, &blob, 1);
    if (code) {
	Die(errno, 0);
	exit(1);
    }
    if (setp) {
	printf("%s: new monitor host set.\n", pn);
    }
    else {
	/* now decode old address */
	if (hostAddr == 0xffffffff) {
	    printf("Cache monitoring is currently disabled.\n");
	}
	else {
	    tp = hostutil_GetNameByINet(hostAddr);
	    printf("Using host %s for monitor services.\n", tp);
	}
    }
    return 0;
}

static SysNameCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *input = space;
    afs_int32 setp = 0;
    
    ti = as->parms[0].items;
    if (ti) {
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */
    }

    blob.in = space;
    blob.out = space;
    blob.out_size = MAXSIZE;
    blob.in_size = sizeof(afs_int32);
    memcpy(input, &setp, sizeof(afs_int32));
    input += sizeof(afs_int32);
    for (; ti; ti = ti->next) {
        setp++;
        blob.in_size += strlen(ti->data) + 1;
        if (blob.in_size > MAXSIZE) {
            fprintf(stderr, "%s: sysname%s too long.\n", pn,
                     setp > 1 ? "s" : "");
            return 1;
        }
        strcpy(input, ti->data);
        input += strlen(ti->data);
        *(input++) = '\0';
    }
    memcpy(space, &setp, sizeof(afs_int32));
    code = pioctl(0, VIOC_AFS_SYSNAME, &blob, 1);
    if (code) {
        Die(errno, 0);
        return 1;
    }    
    if (setp) {
        printf("%s: new sysname%s set.\n", pn, setp > 1 ? "s" : "");
        return 0;
    }

    input = space;
    memcpy(&setp, input, sizeof(afs_int32));
    input += sizeof(afs_int32);
    if (!setp) {
        fprintf(stderr,"No sysname name value was found\n");
        return 1;
    } 
    
    printf("Current sysname%s", setp > 1 ? "s are" : " is");
    for (; setp > 0; --setp ) {
        printf(" \'%s\'", input);
        input += strlen(input) + 1;
    }
    printf("\n");
    return 0;
}

char *exported_types[] = {"null", "nfs", ""};
static ExportAfsCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    int export=0, type=0, mode = 0, exp = 0, gstat = 0, exportcall, pwsync=0, smounts=0;
    
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    ti = as->parms[0].items;
    if (strcmp(ti->data, "nfs")	== 0) type = 0x71; /* NFS */
    else {
	fprintf(stderr,"Invalid exporter type, '%s', Only the 'nfs' exporter is currently supported\n", ti->data);
	return 1;
    }
    ti = as->parms[1].items;
    if (ti) {
	if (strcmp(ti->data, "on") == 0) export = 3;
	else if (strcmp(ti->data, "off") == 0) export = 2;
	else {
	    printf("Illegal argument %s\n", ti->data);
	    return 1;
	}
	exp = 1;
    }
    if (ti = as->parms[2].items) {	/* -noconvert */
	if (strcmp(ti->data, "on") == 0) mode = 2;
	else if (strcmp(ti->data, "off") == 0) mode = 3;
	else {
	    printf("Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if (ti = as->parms[3].items) {	/* -uidcheck */
	if (strcmp(ti->data, "on") == 0) pwsync = 3;
	else if (strcmp(ti->data, "off") == 0) pwsync = 2;
	else {
	    printf("Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if (ti = as->parms[4].items) {	/* -submounts */
	if (strcmp(ti->data, "on") == 0) smounts = 3;
	else if (strcmp(ti->data, "off") == 0) smounts = 2;
	else {
	    printf("Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    exportcall =  (type << 24) | (mode << 6) | (pwsync << 4) | (smounts << 2) | export;
    type &= ~0x70;
    /* make the call */
    blob.in = (char *) &exportcall;
    blob.in_size = sizeof(afs_int32);
    blob.out = (char *) &exportcall;
    blob.out_size = sizeof(afs_int32);
    code = pioctl(0, VIOC_EXPORTAFS, &blob, 1);
    if (code) {
	if (errno == ENODEV) {
	    fprintf(stderr,"Sorry, the %s-exporter type is currently not supported on this AFS client\n", exported_types[type]);
	} else {
	    Die(errno, 0);
	    exit(1);
	}
    } else {
	if (!gstat) {
	    if (exportcall & 1) {
		printf("'%s' translator is enabled with the following options:\n\tRunning in %s mode\n\tRunning in %s mode\n\t%s\n", 
		       exported_types[type], (exportcall & 2 ? "strict unix" : "convert owner mode bits to world/other"),
		       (exportcall & 4 ? "strict 'passwd sync'" : "no 'passwd sync'"),
		       (exportcall & 8 ? "Allow mounts of /afs/.. subdirs" : "Only mounts to /afs allowed"));
	    } else {
		printf("'%s' translator is disabled\n", exported_types[type]);
	    }
	}
    }
    return(0);
}


static GetCellCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    struct afsconf_cell info;
    register struct cmd_item *ti;
    struct a {
	afs_int32 stat;
	afs_int32 junk;
    } args;
    
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per cell */
	blob.out_size = sizeof(args);
	blob.out = (caddr_t) &args;
	code = GetCellName(ti->data, &info);
	if (code) {
	    continue;
	}
	blob.in_size = 1+strlen(info.name);
	blob.in = info.name;
	code = pioctl(0, VIOC_GETCELLSTATUS, &blob, 1);
	if (code) {
	    if (errno == ENOENT)
		fprintf(stderr,"fs: the cell named '%s' does not exist\n", info.name);
	    else
		Die(errno, info.name);
	    return 1;
	}
	printf("Cell %s status: ", info.name);
#ifdef notdef
	if (args.stat & 1) printf("primary ");
#endif
	if (args.stat & 2) printf("no setuid allowed");
	else printf("setuid allowed");
	if (args.stat & 4) printf(", using old VLDB");
	printf("\n");
    }
    return 0;
}

static SetCellCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    struct afsconf_cell info;
    register struct cmd_item *ti;
    struct a {
	afs_int32 stat;
	afs_int32 junk;
	char cname[64];
    } args;

    /* figure stuff to set */
    args.stat = 0;
    args.junk = 0;

#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    if (! as->parms[1].items) args.stat |= CM_SETCELLFLAG_SUID; /* default to -nosuid */

    /* set stat for all listed cells */
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per cell */
	code = GetCellName(ti->data, &info);
	if (code) {
	    continue;
	}
	strcpy(args.cname, info.name);
	blob.in_size = sizeof(args);
	blob.in = (caddr_t) &args;
	blob.out_size = 0;
	blob.out = (caddr_t) 0;
	code = pioctl(0, VIOC_SETCELLSTATUS, &blob, 1);
	if (code) {
	    Die(errno, 0);
	    exit(1);
	}
    }
    return 0;
}

#ifdef WIN32
static GetCellName(char *cellNamep, struct afsconf_cell *infop)
{
	strcpy(infop->name, cellNamep);
        return 0;
}

static VLDBInit(int noAuthFlag, struct afsconf_cell *infop)
{
	return 0;
}

#else /* not WIN32 */

static GetCellName(cellName, info)
char *cellName;
struct afsconf_cell *info;
{
    struct afsconf_dir *tdir;
    register int code;

    tdir = afsconf_Open(AFSCONF_CLIENTNAME);
    if (!tdir) {
	fprintf(stderr,"Could not process files in configuration directory (%s).\n",AFSCONF_CLIENTNAME);
	return -1;
    }

    code = afsconf_GetCellInfo(tdir, cellName, AFSCONF_VLDBSERVICE, info);
    if (code) {
	fprintf(stderr,"fs: cell %s not in %s/CellServDB\n", cellName, AFSCONF_CLIENTNAME);
	return code;
    }

    return 0;
}


static VLDBInit(noAuthFlag,  info)
int noAuthFlag;
struct afsconf_cell *info;
{   afs_int32 code;
    struct ktc_principal sname;
    struct ktc_token ttoken;
    afs_int32 scIndex;
    struct rx_securityClass *sc;
    struct rx_connection *serverconns[VLDB_MAXSERVERS];
    afs_int32 i;

    code = rx_Init(0);
    if (code) {
	fprintf(stderr,"fs: could not initialize rx.\n");
	return code;
    }
    rxInitDone = 1;
    rx_SetRxDeadTime(50);
    if (!noAuthFlag) {   /* we don't need tickets for null */
	strcpy(sname.cell, info->name);
	sname.instance[0] = 0;
	strcpy(sname.name, "afs");
	code = ktc_GetToken(&sname,&ttoken, sizeof(ttoken), (char *)0);
	if (code) {
	    fprintf(stderr,"fs: Could not get afs tokens, running unauthenticated.\n");
	    scIndex = 0;
	}
	else {
	    /* got a ticket */
	    if (ttoken.kvno >= 0 && ttoken.kvno	<= 255)	scIndex	= 2;	/* kerberos */
	    else {
		fprintf (stderr, "fs: funny kvno (%d) in ticket, proceeding\n",
			 ttoken.kvno);
		scIndex = 2;
	    }
	}
    }
    else scIndex = 0;	    /* don't authenticate */
    switch (scIndex) {
	case 0 :
	    sc = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
	    break;
	
	case 1 :
	    break;
	case 2:
	    sc = (struct rx_securityClass *)
		rxkad_NewClientSecurityObject(rxkad_clear, &ttoken.sessionKey,
					      ttoken.kvno, ttoken.ticketLen,
					      ttoken.ticket);
	    break;
    }
    if (info->numServers > VLDB_MAXSERVERS) {
	fprintf(stderr, "fs: info.numServers=%d (> VLDB_MAXSERVERS=%d)\n",
		info->numServers, VLDB_MAXSERVERS);
	exit(1);
    }
    memset(serverconns, 0, sizeof(serverconns));
    for (i = 0;i<info->numServers;i++) 
	serverconns[i] = rx_NewConnection(info->hostAddr[i].sin_addr.s_addr,
					  info->hostAddr[i].sin_port, USER_SERVICE_ID,
					  sc, scIndex);

    if (sc)
        rxs_Release(sc);    /* Decrement the initial refCount */
    code = ubik_ClientInit(serverconns, &uclient);

    if (code) {
	fprintf(stderr,"fs: ubik client init failed.\n");
	return code;
    }
    return 0;
}
#endif /* not WIN32 */

static    struct ViceIoctl gblob;
static int debug = 0;
/* 
 * here follow some routines in suport of the setserverprefs and
 * getserverprefs commands.  They are:
 * SetPrefCmd  "top-level" routine
 * addServer   adds a server to the list of servers to be poked into the
 *             kernel.  Will poke the list into the kernel if it threatens
 *             to get too large.
 * pokeServers pokes the existing list of servers and ranks into the kernel
 * GetPrefCmd  reads the Cache Manager's current list of server ranks
 */

static pokeServers()
{
int code;

    cm_SSetPref_t *ssp;
    code = pioctl(0, VIOC_SETSPREFS, &gblob, 1);

    ssp = (cm_SSetPref_t *)space;
    gblob.in_size = ((char *)&(ssp->servers[0])) - (char *)ssp;
    gblob.in = space;
    return code;
}

static addServer(name, rank)
	char *name;
	unsigned short rank;
{  
	int code;
	cm_SSetPref_t *ssp;
	cm_SPref_t *sp;
	struct hostent *thostent;

#ifndef MAXUSHORT
#ifdef MAXSHORT
#define MAXUSHORT ((unsigned short) 2*MAXSHORT+1)  /* assumes two's complement binary system */
#else
#define MAXUSHORT ((unsigned short) ~0)
#endif
#endif

	code = 0;
	thostent = hostutil_GetHostByName(name);
	if (!thostent) {
		fprintf (stderr, "%s: couldn't resolve name.\n", name);
		return EINVAL;
	}

	ssp = (cm_SSetPref_t *)(gblob.in);

	if (gblob.in_size > MAXINSIZE - sizeof(cm_SPref_t)) {
		code = pokeServers();
		ssp->num_servers = 0;
	}

	sp = (cm_SPref_t *)((char*)gblob.in + gblob.in_size);
	memcpy (&(sp->host.s_addr), thostent->h_addr, sizeof(afs_uint32));
	sp->rank = (rank > MAXUSHORT ? MAXUSHORT : rank);
	gblob.in_size += sizeof(cm_SPref_t);
	ssp->num_servers++;

	if (debug) fprintf(stderr, "adding server %s, rank %d, ip addr 0x%lx\n",name,sp->rank,sp->host.s_addr);
	
	return code;
}

static BOOL IsWindowsNT (void)
{
    static BOOL fChecked = FALSE;
    static BOOL fIsWinNT = FALSE;

    if (!fChecked)
    {
        OSVERSIONINFO Version;

        fChecked = TRUE;

        memset (&Version, 0x00, sizeof(Version));
        Version.dwOSVersionInfoSize = sizeof(Version);

        if (GetVersionEx (&Version))
        {
            if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT)
                fIsWinNT = TRUE;
        }
    }
    return fIsWinNT;
}


static SetPrefCmd(as)
register struct cmd_syndesc *as; {
  FILE *infd;
  afs_int32 code;
  struct cmd_item *ti;
  char name[80];
  afs_int32 rank;
  cm_SSetPref_t *ssp;
    
  ssp = (cm_SSetPref_t *)space;
  ssp->flags = 0;
  ssp->num_servers = 0;
  gblob.in_size = ((char*)&(ssp->servers[0])) - (char *)ssp;
  gblob.in = space;
  gblob.out = space;
  gblob.out_size = MAXSIZE;

#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

  code = 0;

  ti = as->parms[2].items;  /* -file */
  if (ti) {
    if (debug) fprintf(stderr,"opening file %s\n",ti->data);
    if (!(infd = fopen(ti->data,"r" ))) {
      code = errno;
      Die(errno,ti->data);
    }
    else
      while ( fscanf(infd, "%79s%ld", name, &rank) != EOF) {
	code = addServer (name, (unsigned short) rank);
      }
  }

  ti = as->parms[3].items;  /* -stdin */
  if (ti) {
    while ( scanf("%79s%ld", name, &rank) != EOF) {
      code = addServer (name, (unsigned short) rank);
    }
  }
  
  for (ti = as->parms[0].items;ti;ti=ti->next) {/*list of servers, ranks */
    if (ti) {
      if (!ti->next) {
	break;
      }
      code = addServer (ti->data, (unsigned short) atol(ti->next->data));
      if (debug)
	printf("set fs prefs %s %s\n", ti->data, ti->next->data);
      ti=ti->next;
    }
  }
  code = pokeServers();
  if (debug) 
    printf("now working on vlservers, code=%d, errno=%d\n",code,errno);

  ssp = (cm_SSetPref_t *)space;
  gblob.in_size = ((char*)&(ssp->servers[0])) - (char *)ssp;
  gblob.in = space;
  ssp->flags = CM_SPREF_VLONLY;
  ssp->num_servers = 0;

  for (ti = as->parms[1].items;ti;ti=ti->next) { /* list of dbservers, ranks */
    if (ti) {
      if (!ti->next) {
	break;
      }
      code = addServer (ti->data, (unsigned short) atol(ti->next->data));
      if (debug) 
	printf("set vl prefs %s %s\n", ti->data, ti->next->data);
      ti=ti->next;
    }
  }

  if (as->parms[1].items) {
    if (debug) 
      printf("now poking vlservers\n");
    code = pokeServers();
  }

if (code) 
  Die(errno,0);

return code;
}


static GetPrefCmd(as)
register struct cmd_syndesc *as; {
  afs_int32 code;
  struct cmd_item *ti;
  char *name, tbuffer[20];
  afs_int32 addr;
  FILE * outfd;
  int resolve;
  int vlservers;
  struct ViceIoctl blob;
  struct cm_SPrefRequest *in;
  struct cm_SPrefInfo *out;
  int i;
    
  code = 0;
  ti = as->parms[0].items;  /* -file */
  if (ti) {
    if (debug) fprintf(stderr,"opening file %s\n",ti->data);
    if (!(outfd = freopen(ti->data,"w",stdout))) {
      Die(errno,ti->data);
      return errno;
    }
  }

  ti = as->parms[1].items;  /* -numeric */
  resolve = !(ti);
  ti = as->parms[2].items;  /* -vlservers */
  vlservers = (ti ? CM_SPREF_VLONLY : 0);
/*  ti = as->parms[3].items;   -cell */

  in = (struct cm_SPrefRequest *)space;
  in->offset = 0;

  do {
    blob.in_size=sizeof(struct cm_SPrefRequest);
    blob.in = (char *)in;
    blob.out = space;
    blob.out_size = MAXSIZE;

    in->num_servers = (MAXSIZE - 2*sizeof(short))/sizeof(struct cm_SPref);
    in->flags = vlservers; 

    code = pioctl(0, VIOC_GETSPREFS, &blob, 1);
    if (code){
      perror("getserverprefs pioctl");
      Die (errno,0);
    }
    else {
      out = (struct cm_SPrefInfo *) blob.out;

      for (i=0;i<out->num_servers;i++) {
	if (resolve) {
	  name = hostutil_GetNameByINet(out->servers[i].host.s_addr);
	}
	else {
	  addr = ntohl(out->servers[i].host.s_addr);
	  sprintf(tbuffer, "%d.%d.%d.%d", (addr>>24) & 0xff, (addr>>16) & 0xff,
		  (addr>>8) & 0xff, addr & 0xff);
	  name=tbuffer;
	}
	printf ("%-50s %5u\n",name,out->servers[i].rank);      
      }
  
      in->offset = out->next_offset;
    }
  } while (!code && out->next_offset > 0);

    return code;
}

static TraceCmd(struct cmd_syndesc *asp)
{
	long code;
    struct ViceIoctl blob;
    long inValue;
    long outValue;
    
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
        if (geteuid()) {
            fprintf (stderr,"Permission denied: requires root access.\n");
            return EACCES;
        }
#endif /* WIN32 */

    if ((asp->parms[0].items && asp->parms[1].items)) {
		fprintf(stderr, "fs trace: must use at most one of '-off' or '-on'\n");
        return EINVAL;
    }
        
	/* determine if we're turning this tracing on or off */
	inValue = 0;
    if (asp->parms[0].items)
        inValue = 3;		/* enable */
	else if (asp->parms[1].items) inValue = 2;	/* disable */
    if (asp->parms[2].items) inValue |= 4;		/* do reset */
	if (asp->parms[3].items) inValue |= 8;		/* dump */
        
    blob.in_size = sizeof(long);
    blob.in = (char *) &inValue;
    blob.out_size = sizeof(long);
    blob.out = (char *) &outValue;
        
	code = pioctl(NULL, VIOC_TRACECTL, &blob, 1);
	if (code) {
		Die(errno, NULL);
		return code;
	}
        
    if (outValue) printf("AFS tracing enabled.\n");
    else printf("AFS tracing disabled.\n");

    return 0;
}

static void sbusage()
{
	fprintf(stderr, "example usage: fs storebehind -files *.o -kb 99999 -default 0\n");
	fprintf(stderr, "               fs sb 50000 *.[ao] -default 10\n");
}

static StoreBehindCmd(as)  /* fs sb -kbytes 9999 -files *.o -default 64 */
struct cmd_syndesc *as; {
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct sbstruct tsb;
    int kb;
    
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");  
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    if ((as->parms[0].items && as->parms[1].items) ||   
        (!as->parms[0].items && !as->parms[1].items)) /* same as logical xor */
      ;
    else {
      sbusage();
      return EINVAL;
    }

    ti=as->parms[2].items;
    if (ti && ti->data) {
      kb = atoi (ti->data);
    }
    else kb = -1;
    tsb.sb_default = kb;

    ti=as->parms[1].items;
    if (ti && ti->data) {
      kb = atoi (ti->data);
    }
    else kb = -1;
    tsb.sb_thisfile = kb;
    
    ti=as->parms[0].items;
    do { 
      /* once per file */
      blob.in = &tsb;
      blob.out = &tsb;
      blob.in_size = sizeof(tsb);
      blob.out_size = sizeof(tsb);
      fprintf (stderr, "storbehind %s %d %d\n", (ti?ti->data:0), 
	       tsb.sb_thisfile, tsb.sb_default);
      code = pioctl((ti ? ti->data : 0) , VIOC_STOREBEHIND, &blob, 1);
      if (code) {
	Die(errno, (ti ? ti->data : 0));
	continue;
      }
      if (blob.out_size == sizeof(tsb)) {
	fprintf (stderr, "storbehind %s is now %d (default %d)\n", (ti?ti->data:0), 
		 tsb.sb_thisfile, tsb.sb_default);
      }
      ti = (ti ? ti->next : ti);
    } while (ti);

    return 0;
}

static afs_int32 SetCryptCmd(as)
    struct cmd_syndesc *as;
{
    afs_int32 code = 0, flag;
    struct ViceIoctl blob;
    char *tp;
 
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    tp = as->parms[0].items->data;
    if (strcmp(tp, "on") == 0)
      flag = 1;
    else if (strcmp(tp, "off") == 0)
      flag = 0;
    else {
      fprintf (stderr, "%s: %s must be \"on\" or \"off\".\n", pn, tp);
      return EINVAL;
    }

    blob.in = (char *) &flag;
    blob.in_size = sizeof(flag);
    blob.out_size = 0;
    code = pioctl(0, VIOC_SETRXKCRYPT, &blob, 1);
    if (code)
      Die(code, (char *) 0);
    return 0;
}

static afs_int32 GetCryptCmd(as)
    struct cmd_syndesc *as;
{
    afs_int32 code = 0, flag;
    struct ViceIoctl blob;
    char *tp;
 
    blob.in = (char *) 0;
    blob.in_size = 0;
    blob.out_size = sizeof(flag);
    blob.out = space;

    code = pioctl(0, VIOC_GETRXKCRYPT, &blob, 1);

    if (code) Die(code, (char *) 0);
    else {
      tp = space;
      memcpy(&flag, tp, sizeof(afs_int32));
      printf("Security level is currently ");
      if (flag == 1)
        printf("crypt (data security).\n");
      else
        printf("clear.\n");
    }
    return 0;
}

main(argc, argv)
int argc;
char **argv; {
    register afs_int32 code;
    register struct cmd_syndesc *ts;

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

#ifdef WIN32
    WSADATA WSAjunk;
    WSAStartup(0x0101, &WSAjunk);
#endif /* WIN32 */

    /* try to find volume location information */
    

    osi_Init();

    ts = cmd_CreateSyntax("setserverprefs", SetPrefCmd, 0, "set server ranks");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL|CMD_EXPANDS, "fileserver names and ranks");
    cmd_AddParm(ts, "-vlservers", CMD_LIST, CMD_OPTIONAL|CMD_EXPANDS, "VL server names and ranks");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "input from named file");
    cmd_AddParm(ts, "-stdin", CMD_FLAG, CMD_OPTIONAL, "input from stdin");
    cmd_CreateAlias(ts, "sp");

    ts = cmd_CreateSyntax("getserverprefs", GetPrefCmd, 0, "get server ranks");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "output to named file");
    cmd_AddParm(ts, "-numeric", CMD_FLAG, CMD_OPTIONAL, "addresses only");
    cmd_AddParm(ts, "-vlservers", CMD_FLAG, CMD_OPTIONAL, "VL servers");
    /* cmd_AddParm(ts, "-cell", CMD_FLAG, CMD_OPTIONAL, "cellname"); */
    cmd_CreateAlias(ts, "gp");

    ts = cmd_CreateSyntax("setacl", SetACLCmd, 0, "set access control list");
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");
    cmd_AddParm(ts, "-acl", CMD_LIST, 0, "access list entries");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "clear access list");
    cmd_AddParm(ts, "-negative", CMD_FLAG, CMD_OPTIONAL, "apply to negative rights");
    parm_setacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl (DFS only)");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl (DFS only)");
    cmd_CreateAlias(ts, "sa");
    
    ts = cmd_CreateSyntax("listacl", ListACLCmd, 0, "list access control list");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    parm_listacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl");
    cmd_CreateAlias(ts, "la");
    
    ts = cmd_CreateSyntax("cleanacl", CleanACLCmd, 0, "clean up access control list");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    
    ts = cmd_CreateSyntax("copyacl", CopyACLCmd, 0, "copy access control list");
    cmd_AddParm(ts, "-fromdir", CMD_SINGLE, 0, "source directory (or DFS file)");
    cmd_AddParm(ts, "-todir", CMD_LIST, 0, "destination directory (or DFS file)");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "first clear dest access list");
    parm_copyacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl");
    
    cmd_CreateAlias(ts, "ca");

    ts = cmd_CreateSyntax("flush", FlushCmd, 0, "flush file from cache");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    
    ts = cmd_CreateSyntax("setvol", SetVolCmd, 0, "set volume status");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-max", CMD_SINGLE, CMD_OPTIONAL, "disk space quota in 1K units");
#ifdef notdef
    cmd_AddParm(ts, "-min", CMD_SINGLE, CMD_OPTIONAL, "disk space guaranteed");
#endif
    cmd_AddParm(ts, "-motd", CMD_SINGLE, CMD_OPTIONAL, "message of the day");
    cmd_AddParm(ts, "-offlinemsg", CMD_SINGLE, CMD_OPTIONAL, "offline message");
    cmd_CreateAlias(ts, "sv");
    
    ts = cmd_CreateSyntax("messages", GagCmd, 0, "control Cache Manager messages");
    cmd_AddParm(ts, "-show", CMD_SINGLE, CMD_OPTIONAL, "[user|console|all|none]");

    ts = cmd_CreateSyntax("examine", ExamineCmd, 0, "display volume status");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "lv");
    cmd_CreateAlias(ts, "listvol");
    
    ts = cmd_CreateSyntax("listquota", ListQuotaCmd, 0, "list volume quota");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "lq");
    
    ts = cmd_CreateSyntax("diskfree", DiskFreeCmd, 0, "show server disk space usage");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "df");
    
    ts = cmd_CreateSyntax("quota", QuotaCmd, 0, "show volume quota usage");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    
    ts = cmd_CreateSyntax("lsmount", ListMountCmd, 0, "list mount point");    
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");
    
    ts = cmd_CreateSyntax("mkmount", MakeMountCmd, 0, "make mount point");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, 0, "directory");
    cmd_AddParm(ts, "-vol", CMD_SINGLE, 0, "volume name");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-rw", CMD_FLAG, CMD_OPTIONAL, "force r/w volume");
    cmd_AddParm(ts, "-fast", CMD_FLAG, CMD_OPTIONAL, "don't check name with VLDB");

    /*
     *
     * defect 3069
     * 
    cmd_AddParm(ts, "-root", CMD_FLAG, CMD_OPTIONAL, "create cellular mount point");
    */

    
    ts = cmd_CreateSyntax("rmmount", RemoveMountCmd, 0, "remove mount point");
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");
    
    ts = cmd_CreateSyntax("checkservers", CheckServersCmd, 0, "check local cell's servers");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell to check");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "check all cells");
    cmd_AddParm(ts, "-fast", CMD_FLAG, CMD_OPTIONAL, "just list, don't check");
	cmd_AddParm(ts,"-interval",CMD_SINGLE,CMD_OPTIONAL,"seconds between probes");
    
    ts = cmd_CreateSyntax("checkvolumes", CheckVolumesCmd,0, "check volumeID/name mappings");
    cmd_CreateAlias(ts, "checkbackups");

    
    ts = cmd_CreateSyntax("setcachesize", SetCacheSizeCmd, 0, "set cache size");
    cmd_AddParm(ts, "-blocks", CMD_SINGLE, CMD_OPTIONAL, "size in 1K byte blocks (0 => reset)");
    cmd_CreateAlias(ts, "cachesize");

    cmd_AddParm(ts, "-reset", CMD_FLAG, CMD_OPTIONAL, "reset size back to boot value");
    
    ts = cmd_CreateSyntax("getcacheparms", GetCacheParmsCmd, 0, "get cache usage info");

    ts = cmd_CreateSyntax("listcells", ListCellsCmd, 0, "list configured cells");
    
    ts = cmd_CreateSyntax("setquota", SetQuotaCmd, 0, "set volume quota");
    cmd_AddParm(ts, "-path", CMD_SINGLE, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-max", CMD_SINGLE, 0, "max quota in kbytes");
#ifdef notdef
    cmd_AddParm(ts, "-min", CMD_SINGLE, CMD_OPTIONAL, "min quota in kbytes");
#endif
    cmd_CreateAlias(ts, "sq");

    ts = cmd_CreateSyntax("newcell", NewCellCmd, 0, "configure new cell");
#ifndef WIN32
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "cell name");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_REQUIRED, "primary servers");
    cmd_AddParm(ts, "-linkedcell", CMD_SINGLE, CMD_OPTIONAL, "linked cell name");
#endif

#ifdef FS_ENABLE_SERVER_DEBUG_PORTS
    /*
     * Turn this on only if you wish to be able to talk to a server which is listening
     * on alternative ports. This is not intended for general use and may not be
     * supported in the cache manager. It is not a way to run two servers at the
     * same host, since the cache manager cannot properly distinguish those two hosts.
     */
    cmd_AddParm(ts, "-fsport", CMD_SINGLE, CMD_OPTIONAL, "cell's fileserver port");
    cmd_AddParm(ts, "-vlport", CMD_SINGLE, CMD_OPTIONAL, "cell's vldb server port");
#endif

    ts = cmd_CreateSyntax("whichcell", WhichCellCmd, 0, "list file's cell");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("whereis", WhereIsCmd, 0, "list file's location");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("wscell", WSCellCmd, 0, "list workstation's cell");
    
    /*
     ts = cmd_CreateSyntax("primarycell", PrimaryCellCmd, 0, "obsolete (listed primary cell)");
     */
    
    ts = cmd_CreateSyntax("monitor", MonitorCmd, 0, "set cache monitor host address");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "host name or 'off'");
    cmd_CreateAlias(ts, "mariner");
    
   
    ts = cmd_CreateSyntax("getcellstatus", GetCellCmd, 0, "get cell status");
    cmd_AddParm(ts, "-cell", CMD_LIST, 0, "cell name");
    
    ts = cmd_CreateSyntax("setcell", SetCellCmd, 0, "set cell status");
    cmd_AddParm(ts, "-cell", CMD_LIST, 0, "cell name");
    cmd_AddParm(ts, "-suid", CMD_FLAG, CMD_OPTIONAL, "allow setuid programs");
    cmd_AddParm(ts, "-nosuid", CMD_FLAG, CMD_OPTIONAL, "disallow setuid programs");
    
    ts = cmd_CreateSyntax("flushvolume", FlushVolumeCmd, 0, "flush all data in volume");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("sysname", SysNameCmd, 0, "get/set sysname (i.e. @sys) value");
    cmd_AddParm(ts, "-newsys", CMD_LIST, CMD_OPTIONAL, "new sysname");

    ts = cmd_CreateSyntax("exportafs", ExportAfsCmd, 0, "enable/disable translators to AFS");
    cmd_AddParm(ts, "-type", CMD_SINGLE, 0, "exporter name");
    cmd_AddParm(ts, "-start", CMD_SINGLE, CMD_OPTIONAL, "start/stop translator ('on' or 'off')");
    cmd_AddParm(ts, "-convert", CMD_SINGLE, CMD_OPTIONAL, "convert from afs to unix mode ('on or 'off')");
    cmd_AddParm(ts, "-uidcheck", CMD_SINGLE, CMD_OPTIONAL, "run on strict 'uid check' mode ('on' or 'off')");
    cmd_AddParm(ts, "-submounts", CMD_SINGLE, CMD_OPTIONAL, "allow nfs mounts to subdirs of /afs/.. ('on' or 'off')");


    ts = cmd_CreateSyntax("storebehind", StoreBehindCmd, 0, 
			  "store to server after file close");
    cmd_AddParm(ts, "-kbytes", CMD_SINGLE, CMD_OPTIONAL, "asynchrony for specified names");
    cmd_AddParm(ts, "-files", CMD_LIST, CMD_OPTIONAL, "specific pathnames");
    cmd_AddParm(ts, "-allfiles", CMD_SINGLE, CMD_OPTIONAL, "new default (KB)");
    cmd_CreateAlias(ts, "sb");

    ts = cmd_CreateSyntax("setcrypt", SetCryptCmd, 0, "set cache manager encryption flag");
    cmd_AddParm(ts, "-crypt", CMD_SINGLE, 0, "on or off");

    ts = cmd_CreateSyntax("getcrypt", GetCryptCmd, 0, "get cache manager encryption flag");

    ts = cmd_CreateSyntax("trace", TraceCmd, 0, "enable or disable CM tracing");
    cmd_AddParm(ts, "-on", CMD_FLAG, CMD_OPTIONAL, "enable tracing");
    cmd_AddParm(ts, "-off", CMD_FLAG, CMD_OPTIONAL, "disable tracing");
    cmd_AddParm(ts, "-reset", CMD_FLAG, CMD_OPTIONAL, "reset log contents");
    cmd_AddParm(ts, "-dump", CMD_FLAG, CMD_OPTIONAL, "dump log contents");
    cmd_CreateAlias(ts, "tr");

    ts = cmd_CreateSyntax("memdump", MemDumpCmd, 0, "dump memory allocs in debug builds");
    cmd_AddParm(ts, "-begin", CMD_FLAG, CMD_OPTIONAL, "set a memory checkpoint");
    cmd_AddParm(ts, "-end", CMD_FLAG, CMD_OPTIONAL, "dump memory allocs");
    
    ts = cmd_CreateSyntax("cscpolicy", CSCPolicyCmd, 0, "change client side caching policy for AFS shares");
    cmd_AddParm(ts, "-share", CMD_SINGLE, CMD_OPTIONAL, "AFS share");
    cmd_AddParm(ts, "-manual", CMD_FLAG, CMD_OPTIONAL, "manual caching of documents");
    cmd_AddParm(ts, "-programs", CMD_FLAG, CMD_OPTIONAL, "automatic caching of programs and documents");
    cmd_AddParm(ts, "-documents", CMD_FLAG, CMD_OPTIONAL, "automatic caching of documents");
    cmd_AddParm(ts, "-disable", CMD_FLAG, CMD_OPTIONAL, "disable caching");

    code = cmd_Dispatch(argc, argv);

#ifndef WIN32
    if (rxInitDone) rx_Finalize();
#endif /* not WIN32 */
    
    return code;
}

void Die(code, filename)
    int   code;
    char *filename;
{ /*Die*/

    if (code == EINVAL) {
	if (filename)
	    fprintf(stderr,"%s: Invalid argument; it is possible that %s is not in AFS.\n", pn, filename);
	else fprintf(stderr,"%s: Invalid argument.\n", pn);
    }
    else if (code == ENOENT) {
	if (filename) fprintf(stderr,"%s: File '%s' doesn't exist\n", pn, filename);
	else fprintf(stderr,"%s: no such file returned\n", pn);
    }
    else if (code == EROFS)  fprintf(stderr,"%s: You can not change a backup or readonly volume\n", pn);
    else if (code == EACCES || code == EPERM) {
	if (filename) fprintf(stderr,"%s: You don't have the required access rights on '%s'\n", pn, filename);
	else fprintf(stderr,"%s: You do not have the required rights to do this operation\n", pn);
    }
    else if (code == ENODEV) {
	fprintf(stderr,"%s: AFS service may not have started.\n", pn);
    }
    else if (code == ESRCH) {
	fprintf(stderr,"%s: Cell name not recognized.\n", pn);
    }
    else if (code == EPIPE) {
	fprintf(stderr,"%s: Volume name or ID not recognized.\n", pn);
    }
    else if (code == EFBIG) {
	fprintf(stderr,"%s: Cache size too large.\n", pn);
    }
    else if (code == ETIMEDOUT) {
	if (filename)
	    fprintf(stderr,"%s:'%s': Connection timed out", pn, filename);
	else
	    fprintf(stderr,"%s: Connection timed out", pn);
    }
    else {
	if (filename) fprintf(stderr,"%s:'%s'", pn, filename);
	else fprintf(stderr,"%s", pn);
#ifdef WIN32
	fprintf(stderr, ": code 0x%x\n", code);
#else /* not WIN32 */
	fprintf(stderr,": %s\n", error_message(code));
#endif /* not WIN32 */
    }
} /*Die*/

static MemDumpCmd(struct cmd_syndesc *asp)
{
    long code;
    struct ViceIoctl blob;
    long inValue;
    long outValue;
  
    if ((asp->parms[0].items && asp->parms[1].items)) {
        fprintf(stderr, "fs trace: must use at most one of '-begin' or '-end'\n");
        return EINVAL;
    }
  
    /* determine if we're turning this tracing on or off */
    inValue = 0;
    if (asp->parms[0].items)
        inValue = 1;            /* begin */
    else if (asp->parms[1].items) 
        inValue = 0;            /* end */
  
    blob.in_size = sizeof(long);
    blob.in = (char *) &inValue;
    blob.out_size = sizeof(long);
    blob.out = (char *) &outValue;

    code = pioctl(NULL, VIOC_TRACEMEMDUMP, &blob, 1);
    if (code) {
        Die(errno, NULL);
        return code;
    }

    if (outValue) printf("AFS memdump begin.\n");
    else printf("AFS memdump end.\n");

    return 0;
}

static CSCPolicyCmd(struct cmd_syndesc *asp)
{
    struct cmd_item *ti;
    char *share = NULL;
    HKEY hkCSCPolicy;

    for(ti=asp->parms[0].items; ti;ti=ti->next) {
        share = ti->data;
        if (share)
        {
            break;
        }
    }

    if (share)
    {
        char *policy;

        RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                        "SOFTWARE\\OpenAFS\\Client\\CSCPolicy",
                        0, 
                        "AFS", 
                        REG_OPTION_NON_VOLATILE,
                        KEY_WRITE,
                        NULL, 
                        &hkCSCPolicy,
                        NULL );

        if ( hkCSCPolicy == NULL ) {
            fprintf (stderr,"Permission denied: requires Administrator access.\n");
            return EACCES;
        }

        if ( !IsAdmin() ) {
            fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
            RegCloseKey(hkCSCPolicy);
            return EACCES;
        }

        policy = "manual";
		
        if (asp->parms[1].items)
            policy = "manual";
        if (asp->parms[2].items)
            policy = "programs";
        if (asp->parms[3].items)
            policy = "documents";
        if (asp->parms[4].items)
            policy = "disable";
		
        RegSetValueEx( hkCSCPolicy, share, 0, REG_SZ, policy, strlen(policy)+1);
		
        printf("CSC policy on share \"%s\" changed to \"%s\".\n\n", share, policy);
        printf("Close all applications that accessed files on this share or restart AFS Client for the change to take effect.\n"); 
    }
    else
    {
        DWORD dwIndex, dwPolicies;
        char policyName[256];
        DWORD policyNameLen;
        char policy[256];
        DWORD policyLen;
        DWORD dwType;

        /* list current csc policies */

        RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                        "SOFTWARE\\OpenAFS\\Client\\CSCPolicy",
                        0, 
                        "AFS", 
                        REG_OPTION_NON_VOLATILE,
                        KEY_READ|KEY_QUERY_VALUE,
                        NULL, 
                        &hkCSCPolicy,
                        NULL );

        RegQueryInfoKey( hkCSCPolicy,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwPolicies, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );
		
        printf("Current CSC policies:\n");
        for ( dwIndex = 0; dwIndex < dwPolicies; dwIndex ++ ) {

            policyNameLen = sizeof(policyName);
            policyLen = sizeof(policy);
            RegEnumValue( hkCSCPolicy, dwIndex, policyName, &policyNameLen, NULL,
                          &dwType, policy, &policyLen);

            printf("  %s = %s\n", policyName, policy);
        }
    }

    RegCloseKey(hkCSCPolicy);
    return (0);
}
