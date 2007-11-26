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

#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <afs/vice.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <afs/afsint.h>
#include <afs/venus.h>
#include <rx/xdr.h>
#include "rmtsys.h"


/*
 * We deal with converting pioctl parameters between host and network order.
 * Painful but somebody has to do this and pioctl is the best place rather than
 * leaving it to the calling application programs!
 */

#define	MAXNAME		100
#define	MAXSIZE		2048
#define	MAXHOSTS	8	/* XXX HARD Limit limitation XXX */
#define	MAXGCSIZE	16

struct Acl {
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

struct ClearToken {
    afs_int32 AuthHandle;
    char HandShakeKey[8];
    afs_int32 ViceId;
    afs_int32 BeginTimestamp;
    afs_int32 EndTimestamp;
};

char *
RSkipLine(astr)
     register char *astr;
{
    while (*astr != '\n')
	astr++;
    astr++;
    return astr;
}


struct Acl *
RParseAcl(astr)
     char *astr;
{
    int nplus, nminus, i, trights;
    char tname[MAXNAME];
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;
    sscanf(astr, "%d", &nplus);
    astr = RSkipLine(astr);
    sscanf(astr, "%d", &nminus);
    astr = RSkipLine(astr);

    ta = (struct Acl *)malloc(sizeof(struct Acl));
    ta->nplus = nplus;
    ta->nminus = nminus;

    last = 0;
    first = 0;
    for (i = 0; i < nplus; i++) {
	sscanf(astr, "%100s %d", tname, &trights);
	astr = RSkipLine(astr);
	tl = (struct AclEntry *)malloc(sizeof(struct AclEntry));
	if (!first)
	    first = tl;
	strcpy(tl->name, tname);
	tl->rights = trights;
	tl->next = 0;
	if (last)
	    last->next = tl;
	last = tl;
    }
    ta->pluslist = first;

    last = 0;
    first = 0;
    for (i = 0; i < nminus; i++) {
	sscanf(astr, "%100s %d", tname, &trights);
	astr = RSkipLine(astr);
	tl = (struct AclEntry *)malloc(sizeof(struct AclEntry));
	if (!first)
	    first = tl;
	strcpy(tl->name, tname);
	tl->rights = trights;
	tl->next = 0;
	if (last)
	    last->next = tl;
	last = tl;
    }
    ta->minuslist = first;

    return ta;
}


int
RAclToString(struct Acl *acl, char *mydata, int ntoh_conv)
{
    char tstring[MAXSIZE];
    struct AclEntry *tp;

/* No conversion needed since they're in network order in the first place */
    sprintf(mydata, "%d\n%d\n", acl->nplus, acl->nminus);
    for (tp = acl->pluslist; tp; tp = tp->next) {
	sprintf(tstring, "%s %d\n", tp->name, tp->rights);
	strcat(mydata, tstring);
    }
    for (tp = acl->minuslist; tp; tp = tp->next) {
	sprintf(tstring, "%s %d\n", tp->name, tp->rights);
	strcat(mydata, tstring);
    }
    return 0;
}


/* Free all malloced stuff */
int
RCleanAcl(struct Acl *aa)
{
    register struct AclEntry *te, *ne;

    for (te = aa->pluslist; te; te = ne) {
	ne = te->next;
	free(te);
    }
    for (te = aa->minuslist; te; te = ne) {
	ne = te->next;
	free(te);
    }
    free(aa);
    return 0;
}


int
RFetchVolumeStatus_conversion(char *data, int ntoh_conv)
{
    struct AFSFetchVolumeStatus *status = (AFSFetchVolumeStatus *) data;

    if (ntoh_conv) {		/* Network -> Host */
	status->Vid = ntohl(status->Vid);
	status->ParentId = ntohl(status->ParentId);
#ifdef	notdef
/* save cycles */
	status->OnLine = status->OnLine;
	status->InService = status->InService;
	status->Blessed = status->Blessed;
	status->NeedsSalvage = status->NeedsSalvage;
#endif
	status->Type = ntohl(status->Type);
	status->MinQuota = ntohl(status->MinQuota);
	status->MaxQuota = ntohl(status->MaxQuota);
	status->BlocksInUse = ntohl(status->BlocksInUse);
	status->PartBlocksAvail = ntohl(status->PartBlocksAvail);
	status->PartMaxBlocks = ntohl(status->PartMaxBlocks);
    } else {			/* Host -> Network */
	status->Vid = htonl(status->Vid);
	status->ParentId = htonl(status->ParentId);
#ifdef	notdef
/* save cycles */
	status->OnLine = status->OnLine;
	status->InService = status->InService;
	status->Blessed = status->Blessed;
	status->NeedsSalvage = status->NeedsSalvage;
#endif
	status->Type = htonl(status->Type);
	status->MinQuota = htonl(status->MinQuota);
	status->MaxQuota = htonl(status->MaxQuota);
	status->BlocksInUse = htonl(status->BlocksInUse);
	status->PartBlocksAvail = htonl(status->PartBlocksAvail);
	status->PartMaxBlocks = htonl(status->PartMaxBlocks);
    }
    return 0;
}

int
RClearToken_convert(char *ptr, int ntoh_conv)
{
    struct ClearToken *ticket = (struct ClearToken *)ptr;

    if (ntoh_conv) {		/* Network -> host */
	ticket->AuthHandle = ntohl(ticket->AuthHandle);
	ticket->ViceId = ntohl(ticket->ViceId);
	ticket->BeginTimestamp = ntohl(ticket->BeginTimestamp);
	ticket->EndTimestamp = ntohl(ticket->EndTimestamp);
    } else {			/* Host -> Network */
	ticket->AuthHandle = htonl(ticket->AuthHandle);
	ticket->ViceId = htonl(ticket->ViceId);
	ticket->BeginTimestamp = htonl(ticket->BeginTimestamp);
	ticket->EndTimestamp = htonl(ticket->EndTimestamp);
    }
    return 0;
}

int
inparam_conversion(afs_int32 cmd, char *buffer, afs_int32 ntoh_conv)
{
    struct Acl *acl;
    afs_int32 *lptr, i;
    char *ptr;

    switch (cmd & 0xffff) {
    case VIOCSETAL & 0xffff:
	acl = RParseAcl(buffer);
	RAclToString(acl, buffer, ntoh_conv);
	RCleanAcl(acl);
	break;
    case VIOCSETTOK & 0xffff:
	lptr = (afs_int32 *) buffer;
	/* i is sizeof of the secret ticket */
	if (ntoh_conv) {
	    i = ntohl(*lptr);
	    *lptr = i;
	} else {
	    i = *lptr;
	    *lptr = htonl(i);
	}
	lptr++;
	ptr = (char *)lptr;
	ptr += i;		/* skip over the ticket */
	lptr = (afs_int32 *) ptr;
	/* i is now size of the clear token */
	if (ntoh_conv) {
	    i = ntohl(*lptr);
	    *lptr = i;
	} else {
	    i = *lptr;
	    *lptr = htonl(i);
	}
	lptr++;
	ptr = (char *)lptr;
	RClearToken_convert(ptr, ntoh_conv);
	ptr += i;		/* sizeof(struct ClearToken) */
	lptr = (afs_int32 *) ptr;
	if (ntoh_conv)
	    *lptr = ntohl(*lptr);
	else
	    *lptr = htonl(*lptr);
	lptr++;			/* primary flag */
	break;
    case VIOCSETVOLSTAT & 0xffff:
	RFetchVolumeStatus_conversion(buffer, ntoh_conv);
	break;
    case VIOCGETTOK & 0xffff:
    case VIOCCKSERV & 0xffff:
    case VIOCACCESS & 0xffff:
    case VIOCSETCACHESIZE & 0xffff:
    case VIOCGETCELL & 0xffff:
    case VIOC_AFS_MARINER_HOST & 0xffff:
    case VIOC_VENUSLOG & 0xffff:
    case VIOC_AFS_SYSNAME & 0xffff:
    case VIOC_EXPORTAFS & 0xffff:
	lptr = (afs_int32 *) buffer;
	if (ntoh_conv)
	    *lptr = ntohl(*lptr);
	else
	    *lptr = htonl(*lptr);
	break;
    case VIOC_SETCELLSTATUS & 0xffff:
	lptr = (afs_int32 *) buffer;
	if (ntoh_conv)
	    *lptr = ntohl(*lptr);
	else
	    *lptr = htonl(*lptr);
	lptr++;
	if (ntoh_conv)
	    *lptr = ntohl(*lptr);
	else
	    *lptr = htonl(*lptr);
	break;
    case VIOCGETAL & 0xffff:
    case VIOCGETVOLSTAT & 0xffff:
    case VIOCGETCACHEPARMS & 0xffff:
    case VIOCFLUSH & 0xffff:
    case VIOCSTAT & 0xffff:
    case VIOCUNLOG & 0xffff:
    case VIOCCKBACK & 0xffff:
    case VIOCCKCONN & 0xffff:
    case VIOCGETTIME & 0xffff:	/* Obsolete */
    case VIOCWHEREIS & 0xffff:
    case VIOCPREFETCH & 0xffff:
    case VIOCNOP & 0xffff:	/* Obsolete */
    case VIOCENGROUP & 0xffff:	/* Obsolete */
    case VIOCDISGROUP & 0xffff:	/* Obsolete */
    case VIOCLISTGROUPS & 0xffff:	/* Obsolete */
    case VIOCUNPAG & 0xffff:
    case VIOCWAITFOREVER & 0xffff:	/* Obsolete */
    case VIOCFLUSHCB & 0xffff:
    case VIOCNEWCELL & 0xffff:
    case VIOC_AFS_DELETE_MT_PT & 0xffff:
    case VIOC_AFS_STAT_MT_PT & 0xffff:
    case VIOC_FILE_CELL_NAME & 0xffff:
    case VIOC_GET_WS_CELL & 0xffff:
    case VIOC_GET_PRIMARY_CELL & 0xffff:
    case VIOC_GETCELLSTATUS & 0xffff:
    case VIOC_FLUSHVOLUME & 0xffff:
    case VIOCGETFID & 0xffff:	/* nothing yet */
	break;
    default:
	/* Note that new pioctls are supposed to be in network order! */
	break;
    }
    return 0;
}


int
outparam_conversion(afs_int32 cmd, char *buffer, afs_int32 ntoh_conv)
{
    struct Acl *acl;
    afs_int32 *lptr, i;
    char *ptr;

    switch (cmd & 0xffff) {
    case VIOCGETAL & 0xffff:
	acl = RParseAcl(buffer);
	RAclToString(acl, buffer, ntoh_conv);
	RCleanAcl(acl);
	break;
    case VIOCGETVOLSTAT & 0xffff:
	RFetchVolumeStatus_conversion(buffer, ntoh_conv);
	break;
    case VIOCSETVOLSTAT & 0xffff:
	RFetchVolumeStatus_conversion(buffer, ntoh_conv);
	break;
    case VIOCGETTOK & 0xffff:
	lptr = (afs_int32 *) buffer;
	/* i is set to sizeof secret ticket */
	if (ntoh_conv) {
	    i = ntohl(*lptr);
	    *lptr = i;
	} else {
	    i = *lptr;
	    *lptr = htonl(i);
	}
	lptr++;
	ptr = (char *)lptr;
	ptr += i;		/* skip over the ticket */
	lptr = (afs_int32 *) ptr;
	/* i is set to sizeof clear ticket */
	if (ntoh_conv) {
	    i = ntohl(*lptr);
	    *lptr = i;
	} else {
	    i = *lptr;
	    *lptr = htonl(i);
	}
	lptr++;
	ptr = (char *)lptr;
	RClearToken_convert(ptr, ntoh_conv);
	ptr += i;		/* sizeof(struct ClearToken) */
	lptr = (afs_int32 *) ptr;
	if (ntoh_conv)
	    *lptr = ntohl(*lptr);
	else
	    *lptr = htonl(*lptr);
	lptr++;			/* primary flag */
	break;
    case VIOCCKCONN & 0xffff:
    case VIOC_AFS_MARINER_HOST & 0xffff:
    case VIOC_VENUSLOG & 0xffff:
    case VIOC_GETCELLSTATUS & 0xffff:
    case VIOC_AFS_SYSNAME & 0xffff:
    case VIOC_EXPORTAFS & 0xffff:
	lptr = (afs_int32 *) buffer;
	if (ntoh_conv)
	    *lptr = ntohl(*lptr);
	else
	    *lptr = htonl(*lptr);
	break;
    case VIOCGETCACHEPARMS & 0xffff:
	lptr = (afs_int32 *) buffer;
	for (i = 0; i < MAXGCSIZE; i++, lptr++) {
	    if (ntoh_conv)
		*lptr = ntohl(*lptr);
	    else
		*lptr = htonl(*lptr);
	}
	break;
    case VIOCUNLOG & 0xffff:
    case VIOCCKSERV & 0xffff:	/* Already in network order */
    case VIOCCKBACK & 0xffff:
    case VIOCGETTIME & 0xffff:	/* Obsolete */
    case VIOCWHEREIS & 0xffff:	/* Already in network order */
    case VIOCPREFETCH & 0xffff:
    case VIOCNOP & 0xffff:	/* Obsolete */
    case VIOCENGROUP & 0xffff:	/* Obsolete */
    case VIOCDISGROUP & 0xffff:	/* Obsolete */
    case VIOCLISTGROUPS & 0xffff:	/* Obsolete */
    case VIOCACCESS & 0xffff:
    case VIOCUNPAG & 0xffff:
    case VIOCWAITFOREVER & 0xffff:	/* Obsolete */
    case VIOCSETCACHESIZE & 0xffff:
    case VIOCFLUSHCB & 0xffff:
    case VIOCNEWCELL & 0xffff:
    case VIOCGETCELL & 0xffff:	/* Already in network order */
    case VIOC_AFS_DELETE_MT_PT & 0xffff:
    case VIOC_AFS_STAT_MT_PT & 0xffff:
    case VIOC_FILE_CELL_NAME & 0xffff:	/* no need to convert */
    case VIOC_GET_WS_CELL & 0xffff:	/* no need to convert */
    case VIOCGETFID & 0xffff:	/* nothing yet */
    case VIOCSETAL & 0xffff:
    case VIOCFLUSH & 0xffff:
    case VIOCSTAT & 0xffff:
    case VIOCSETTOK & 0xffff:
    case VIOC_GET_PRIMARY_CELL & 0xffff:	/* The cell is returned here */
    case VIOC_SETCELLSTATUS & 0xffff:
    case VIOC_FLUSHVOLUME & 0xffff:
	break;
    default:
	/* Note that new pioctls are supposed to be in network order! */
	break;
    }
    return 0;
}
