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
#include <string.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <io.h>
#include <winsock2.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <sys/stat.h>
#ifdef AFS_AIX_ENV
#include <sys/statfs.h>
#endif
#include <errno.h>

#include <lock.h>
#include <afs/stds.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/nfs.h>
#include <afs/vlserver.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/afsutil.h>
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/cmd.h>
#include <afs/usd.h>
#include "volser.h"
#include "volint.h"
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include "dump.h"
#include "lockdata.h"

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include "volser_prototypes.h"

#ifdef HAVE_POSIX_REGEX
#include <regex.h>
#endif

struct tqElem {
    afs_int32 volid;
    struct tqElem *next;
};

struct tqHead {
    afs_int32 count;
    struct tqElem *next;
};

#define COMMONPARMS     cmd_Seek(ts, 12);\
cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");\
cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL, "don't authenticate");\
cmd_AddParm(ts, "-localauth",CMD_FLAG,CMD_OPTIONAL,"use server tickets");\
cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose");\
cmd_AddParm(ts, "-encrypt", CMD_FLAG, CMD_OPTIONAL, "encrypt commands");\

#define ERROR_EXIT(code) {error=(code); goto error_exit;}

extern int verbose;
int rxInitDone = 0;
struct rx_connection *tconn;
afs_int32 tserver;
extern struct ubik_client *cstruct;
const char *confdir;

static struct tqHead busyHead, notokHead;

static void
qInit(struct tqHead *ahead)
{
    memset((char *)ahead, 0, sizeof(struct tqHead));
    return;
}


static void
qPut(struct tqHead *ahead, afs_int32 volid)
{
    struct tqElem *elem;

    elem = (struct tqElem *)malloc(sizeof(struct tqElem));
    elem->next = ahead->next;
    elem->volid = volid;
    ahead->next = elem;
    ahead->count++;
    return;
}

static void
qGet(struct tqHead *ahead, afs_int32 *volid)
{
    struct tqElem *tmp;

    if (ahead->count <= 0)
	return;
    *volid = ahead->next->volid;
    tmp = ahead->next;
    ahead->next = tmp->next;
    ahead->count--;
    free(tmp);
    return;
}

/* returns 1 if <filename> exists else 0 */
static int
FileExists(char *filename)
{
    usd_handle_t ufd;
    int code;
    afs_hyper_t size;

    code = usd_Open(filename, USD_OPEN_RDONLY, 0, &ufd);
    if (code) {
	return 0;
    }
    code = USD_IOCTL(ufd, USD_IOCTL_GETSIZE, &size);
    USD_CLOSE(ufd);
    if (code) {
	return 0;
    }
    return 1;
}

/* returns 1 if <name> doesnot end in .readonly or .backup, else 0 */
static int
VolNameOK(char *name)
{
    int total;


    total = strlen(name);
    if (!strcmp(&name[total - 9], ".readonly")) {
	return 0;
    } else if (!strcmp(&name[total - 7], ".backup")) {
	return 0;
    } else {
	return 1;
    }
}

/* return 1 if name is a number else 0 */
static int
IsNumeric(char *name)
{
    int result, len, i;
    char *ptr;

    result = 1;
    ptr = name;
    len = strlen(name);
    for (i = 0; i < len; i++) {
	if (*ptr < '0' || *ptr > '9') {
	    result = 0;
	    break;
	}
	ptr++;

    }
    return result;
}


/*
 * Parse a server name/address and return the address in HOST BYTE order
 */
afs_int32
GetServer(char *aname)
{
    register struct hostent *th;
    afs_int32 addr;
    int b1, b2, b3, b4;
    register afs_int32 code;
    char hostname[MAXHOSTCHARS];

    code = sscanf(aname, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    if (code == 4) {
	addr = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
	addr = ntohl(addr);	/* convert to host order */
    } else {
	th = gethostbyname(aname);
	if (!th)
	    return 0;
	memcpy(&addr, th->h_addr, sizeof(addr));
    }

    if (addr == htonl(0x7f000001)) {	/* local host */
	code = gethostname(hostname, MAXHOSTCHARS);
	if (code)
	    return 0;
	th = gethostbyname(hostname);	/* returns host byte order */
	if (!th)
	    return 0;
	memcpy(&addr, th->h_addr, sizeof(addr));
    }

    return (addr);
}

afs_int32
GetVolumeType(char *aname)
{

    if (!strcmp(aname, "ro"))
	return (ROVOL);
    else if (!strcmp(aname, "rw"))
	return (RWVOL);
    else if (!strcmp(aname, "bk"))
	return (BACKVOL);
    else
	return (-1);
}

int
IsPartValid(afs_int32 partId, afs_int32 server, afs_int32 *code)
{
    struct partList dummyPartList;
    int i, success, cnt;

    success = 0;
    *code = 0;

    *code = UV_ListPartitions(server, &dummyPartList, &cnt);
    if (*code)
	return success;
    for (i = 0; i < cnt; i++) {
	if (dummyPartList.partFlags[i] & PARTVALID)
	    if (dummyPartList.partId[i] == partId)
		success = 1;
    }
    return success;
}



 /*sends the contents of file associated with <fd> and <blksize>  to Rx Stream 
  * associated  with <call> */
int 
SendFile(usd_handle_t ufd, register struct rx_call *call, long blksize)
{
    char *buffer = (char *)0;
    afs_int32 error = 0;
    int done = 0;
    afs_uint32 nbytes;

    buffer = (char *)malloc(blksize);
    if (!buffer) {
	fprintf(STDERR, "malloc failed\n");
	return -1;
    }

    while (!error && !done) {
#ifndef AFS_NT40_ENV		/* NT csn't select on non-socket fd's */
	fd_set in;
	FD_ZERO(&in);
	FD_SET((int)(ufd->handle), &in);
	/* don't timeout if read blocks */
	IOMGR_Select(((int)(ufd->handle)) + 1, &in, 0, 0, 0);
#endif
	error = USD_READ(ufd, buffer, blksize, &nbytes);
	if (error) {
	    fprintf(STDERR, "File system read failed\n");
	    break;
	}
	if (nbytes == 0) {
	    done = 1;
	    break;
	}
	if (rx_Write(call, buffer, nbytes) != nbytes) {
	    error = -1;
	    break;
	}
    }
    if (buffer)
	free(buffer);
    return error;
}

/* function invoked by UV_RestoreVolume, reads the data from rx_trx_stream and
 * writes it out to the volume. */
afs_int32
WriteData(struct rx_call *call, char *rock)
{
    char *filename;
    usd_handle_t ufd;
    long blksize;
    afs_int32 error, code;
    int ufdIsOpen = 0;
    afs_hyper_t filesize, currOffset; 
    afs_uint32 buffer;		
    afs_uint32 got; 		

    error = 0;

    filename = rock;
    if (!filename || !*filename) {
	usd_StandardInput(&ufd);
	blksize = 4096;
	ufdIsOpen = 1;
    } else {
	code = usd_Open(filename, USD_OPEN_RDONLY, 0, &ufd);
	if (code == 0) {
	    ufdIsOpen = 1;
	    code = USD_IOCTL(ufd, USD_IOCTL_GETBLKSIZE, &blksize);
	}
	if (code) {
	    fprintf(STDERR, "Could not access file '%s'\n", filename);
	    error = VOLSERBADOP;
	    goto wfail;
	}
	/* test if we have a valid dump */
	hset64(filesize, 0, 0);
	USD_SEEK(ufd, filesize, SEEK_END, &currOffset);
	hset64(filesize, hgethi(currOffset), hgetlo(currOffset)-sizeof(afs_uint32));
	USD_SEEK(ufd, filesize, SEEK_SET, &currOffset);
	USD_READ(ufd, &buffer, sizeof(afs_uint32), &got);
	if ((got != sizeof(afs_uint32)) || (ntohl(buffer) != DUMPENDMAGIC)) {
	    fprintf(STDERR, "Signature missing from end of file '%s'\n", filename);
	    error = VOLSERBADOP;
	    goto wfail;
	}
	/* rewind, we are done */
	hset64(filesize, 0, 0);
	USD_SEEK(ufd, filesize, SEEK_SET, &currOffset);
    }
    code = SendFile(ufd, call, blksize);
    if (code) {
	error = code;
	goto wfail;
    }
  wfail:
    if (ufdIsOpen) {
	code = USD_CLOSE(ufd);
	if (code) {
	    fprintf(STDERR, "Could not close dump file %s\n",
		    (filename && *filename) ? filename : "STDOUT");
	    if (!error)
		error = code;
	}
    }
    return error;
}

/* Receive data from <call> stream into file associated
 * with <fd> <blksize>
 */
int
ReceiveFile(usd_handle_t ufd, struct rx_call *call, long blksize)
{
    char *buffer = NULL;
    afs_int32 bytesread;
    afs_uint32 bytesleft, w;
    afs_int32 error = 0;

    buffer = (char *)malloc(blksize);
    if (!buffer) {
	fprintf(STDERR, "memory allocation failed\n");
	ERROR_EXIT(-1);
    }

    while ((bytesread = rx_Read(call, buffer, blksize)) > 0) {
	for (bytesleft = bytesread; bytesleft; bytesleft -= w) {
#ifndef AFS_NT40_ENV		/* NT csn't select on non-socket fd's */
	    fd_set out;
	    FD_ZERO(&out);
	    FD_SET((int)(ufd->handle), &out);
	    /* don't timeout if write blocks */
	    IOMGR_Select(((int)(ufd->handle)) + 1, 0, &out, 0, 0);
#endif
	    error =
		USD_WRITE(ufd, &buffer[bytesread - bytesleft], bytesleft, &w);
	    if (error) {
		fprintf(STDERR, "File system write failed\n");
		ERROR_EXIT(-1);
	    }
	}
    }

  error_exit:
    if (buffer)
	free(buffer);
    return (error);
}

afs_int32
DumpFunction(struct rx_call *call, char *filename)
{
    usd_handle_t ufd;		/* default is to stdout */
    afs_int32 error = 0, code;
    afs_hyper_t size;
    long blksize;
    int ufdIsOpen = 0;

    /* Open the output file */
    if (!filename || !*filename) {
	usd_StandardOutput(&ufd);
	blksize = 4096;
	ufdIsOpen = 1;
    } else {
	code =
	    usd_Open(filename, USD_OPEN_CREATE | USD_OPEN_RDWR, 0666, &ufd);
	if (code == 0) {
	    ufdIsOpen = 1;
	    hzero(size);
	    code = USD_IOCTL(ufd, USD_IOCTL_SETSIZE, &size);
	}
	if (code == 0) {
	    code = USD_IOCTL(ufd, USD_IOCTL_GETBLKSIZE, &blksize);
	}
	if (code) {
	    fprintf(STDERR, "Could not create file '%s'\n", filename);
	    ERROR_EXIT(VOLSERBADOP);
	}
    }

    code = ReceiveFile(ufd, call, blksize);
    if (code)
	ERROR_EXIT(code);

  error_exit:
    /* Close the output file */
    if (ufdIsOpen) {
	code = USD_CLOSE(ufd);
	if (code) {
	    fprintf(STDERR, "Could not close dump file %s\n",
		    (filename && *filename) ? filename : "STDIN");
	    if (!error)
		error = code;
	}
    }

    return (error);
}

#if SIZEOF_TIME_T!=4
static char *
vos_ctime(afs_int32 *timep)
{
    time_t foo = *timep;
    return ctime(&foo);
}
#else
#define vos_ctime ctime
#endif

static void
DisplayFormat(pntr, server, part, totalOK, totalNotOK, totalBusy, fast,
	      longlist, disp)
     volintInfo *pntr;
     afs_int32 server, part;
     int *totalOK, *totalNotOK, *totalBusy;
     int fast, longlist, disp;
{
    char pname[10];

    if (fast) {
	fprintf(STDOUT, "%-10lu\n", (unsigned long)pntr->volid);
    } else if (longlist) {
	if (pntr->status == VOK) {
	    fprintf(STDOUT, "%-32s ", pntr->name);
	    fprintf(STDOUT, "%10lu ", (unsigned long)pntr->volid);
	    if (pntr->type == 0)
		fprintf(STDOUT, "RW ");
	    if (pntr->type == 1)
		fprintf(STDOUT, "RO ");
	    if (pntr->type == 2)
		fprintf(STDOUT, "BK ");
	    fprintf(STDOUT, "%10d K  ", pntr->size);
	    if (pntr->inUse == 1) {
		fprintf(STDOUT, "On-line");
		*totalOK += 1;
	    } else {
		fprintf(STDOUT, "Off-line");
		*totalNotOK++;
	    }
	    if (pntr->needsSalvaged == 1)
		fprintf(STDOUT, "**needs salvage**");
	    fprintf(STDOUT, "\n");
	    MapPartIdIntoName(part, pname);
	    fprintf(STDOUT, "    %s %s \n", hostutil_GetNameByINet(server),
		    pname);
	    fprintf(STDOUT, "    RWrite %10lu ROnly %10lu Backup %10lu \n",
		    (unsigned long)pntr->parentID,
		    (unsigned long)pntr->cloneID,
		    (unsigned long)pntr->backupID);
	    fprintf(STDOUT, "    MaxQuota %10d K \n", pntr->maxquota);
	    fprintf(STDOUT, "    Creation    %s",
		    vos_ctime(& pntr->creationDate));
#ifdef FULL_LISTVOL_SWITCH
	    fprintf(STDOUT, "    Copy        %s",
		    vos_ctime( & pntr->copyDate));
	    if (!pntr->backupDate)
		fprintf(STDOUT, "    Backup      Never\n");
	    else
		fprintf(STDOUT, "    Backup      %s",
			vos_ctime( & pntr->backupDate));
	    if (pntr->accessDate)
		fprintf(STDOUT, "    Last Access %s",
			vos_ctime( & pntr->accessDate));
#endif
	    if (!pntr->updateDate)
		fprintf(STDOUT, "    Last Update Never\n");
	    else {
		fprintf(STDOUT, "    Last Update %s",
			vos_ctime( & pntr->updateDate));
		fprintf(STDOUT,
			"    %d accesses in the past day (i.e., vnode references)\n",
			pntr->dayUse);
	    }
	} else if (pntr->status == VBUSY) {
	    *totalBusy += 1;
	    qPut(&busyHead, pntr->volid);
	    if (disp)
		fprintf(STDOUT, "**** Volume %lu is busy ****\n",
			(unsigned long)pntr->volid);
	} else {
	    *totalNotOK += 1;
	    qPut(&notokHead, pntr->volid);
	    if (disp)
		fprintf(STDOUT, "**** Could not attach volume %lu ****\n",
			(unsigned long)pntr->volid);
	}
	fprintf(STDOUT, "\n");
    } else {			/* default listing */
	if (pntr->status == VOK) {
	    fprintf(STDOUT, "%-32s ", pntr->name);
	    fprintf(STDOUT, "%10lu ", (unsigned long)pntr->volid);
	    if (pntr->type == 0)
		fprintf(STDOUT, "RW ");
	    if (pntr->type == 1)
		fprintf(STDOUT, "RO ");
	    if (pntr->type == 2)
		fprintf(STDOUT, "BK ");
	    fprintf(STDOUT, "%10d K ", pntr->size);
	    if (pntr->inUse == 1) {
		fprintf(STDOUT, "On-line");
		*totalOK += 1;
	    } else {
		fprintf(STDOUT, "Off-line");
		*totalNotOK += 1;
	    }
	    if (pntr->needsSalvaged == 1)
		fprintf(STDOUT, "**needs salvage**");
	    fprintf(STDOUT, "\n");
	} else if (pntr->status == VBUSY) {
	    *totalBusy += 1;
	    qPut(&busyHead, pntr->volid);
	    if (disp)
		fprintf(STDOUT, "**** Volume %lu is busy ****\n",
			(unsigned long)pntr->volid);
	} else {
	    *totalNotOK += 1;
	    qPut(&notokHead, pntr->volid);
	    if (disp)
		fprintf(STDOUT, "**** Could not attach volume %lu ****\n",
			(unsigned long)pntr->volid);
	}
    }
}

/*------------------------------------------------------------------------
 * PRIVATE XDisplayFormat
 *
 * Description:
 *	Display the contents of one extended volume info structure.
 *
 * Arguments:
 *	a_xInfoP	: Ptr to extended volume info struct to print.
 *	a_servID	: Server ID to print.
 *	a_partID        : Partition ID to print.
 *	a_totalOKP	: Ptr to total-OK counter.
 *	a_totalNotOKP	: Ptr to total-screwed counter.
 *	a_totalBusyP	: Ptr to total-busy counter.
 *	a_fast		: Fast listing?
 *	a_int32		: Int32 listing?
 *	a_showProblems	: Show volume problems?
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void
XDisplayFormat(a_xInfoP, a_servID, a_partID, a_totalOKP, a_totalNotOKP,
	       a_totalBusyP, a_fast, a_int32, a_showProblems)
     volintXInfo *a_xInfoP;
     afs_int32 a_servID;
     afs_int32 a_partID;
     int *a_totalOKP;
     int *a_totalNotOKP;
     int *a_totalBusyP;
     int a_fast;
     int a_int32;
     int a_showProblems;

{				/*XDisplayFormat */

    char pname[10];

    if (a_fast) {
	/*
	 * Short & sweet.
	 */
	fprintf(STDOUT, "%-10lu\n", (unsigned long)a_xInfoP->volid);
    } else if (a_int32) {
	/*
	 * Fully-detailed listing.
	 */
	if (a_xInfoP->status == VOK) {
	    /*
	     * Volume's status is OK - all the fields are valid.
	     */
	    fprintf(STDOUT, "%-32s ", a_xInfoP->name);
	    fprintf(STDOUT, "%10lu ", (unsigned long)a_xInfoP->volid);
	    if (a_xInfoP->type == 0)
		fprintf(STDOUT, "RW ");
	    if (a_xInfoP->type == 1)
		fprintf(STDOUT, "RO ");
	    if (a_xInfoP->type == 2)
		fprintf(STDOUT, "BK ");
	    fprintf(STDOUT, "%10d K used ", a_xInfoP->size);
	    fprintf(STDOUT, "%d files ", a_xInfoP->filecount);
	    if (a_xInfoP->inUse == 1) {
		fprintf(STDOUT, "On-line");
		(*a_totalOKP)++;
	    } else {
		fprintf(STDOUT, "Off-line");
		(*a_totalNotOKP)++;
	    }
	    fprintf(STDOUT, "\n");
	    MapPartIdIntoName(a_partID, pname);
	    fprintf(STDOUT, "    %s %s \n", hostutil_GetNameByINet(a_servID),
		    pname);
	    fprintf(STDOUT, "    RWrite %10lu ROnly %10lu Backup %10lu \n",
		    (unsigned long)a_xInfoP->parentID,
		    (unsigned long)a_xInfoP->cloneID,
		    (unsigned long)a_xInfoP->backupID);
	    fprintf(STDOUT, "    MaxQuota %10d K \n", a_xInfoP->maxquota);
	    fprintf(STDOUT, "    Creation    %s",
		    vos_ctime( & a_xInfoP->creationDate));
#ifdef FULL_LISTVOL_SWITCH
	    fprintf(STDOUT, "    Copy        %s",
		    vos_ctime( & a_xInfoP->copyDate));
	    if (!a_xInfoP->backupDate)
		fprintf(STDOUT, "    Backup      Never\n");
	    else
		fprintf(STDOUT, "    Backup      %s",
			vos_ctime( & a_xInfoP->backupDate));
	    if (a_xInfoP->accessDate)
		fprintf(STDOUT, "    Last Access %s",
			vos_ctime( & a_xInfoP->accessDate));
#endif
	    if (!a_xInfoP->updateDate)
		fprintf(STDOUT, "    Last Update Never\n");
	    else {
		fprintf(STDOUT, "    Last Update %s",
			vos_ctime( & a_xInfoP->updateDate));
		fprintf(STDOUT,
			"    %d accesses in the past day (i.e., vnode references)\n",
			a_xInfoP->dayUse);
	    }

	    /*
	     * Print all the read/write and authorship stats.
	     */
	    fprintf(STDOUT, "\n                      Raw Read/Write Stats\n");
	    fprintf(STDOUT,
		    "          |-------------------------------------------|\n");
	    fprintf(STDOUT,
		    "          |    Same Network     |    Diff Network     |\n");
	    fprintf(STDOUT,
		    "          |----------|----------|----------|----------|\n");
	    fprintf(STDOUT,
		    "          |  Total   |   Auth   |   Total  |   Auth   |\n");
	    fprintf(STDOUT,
		    "          |----------|----------|----------|----------|\n");
	    fprintf(STDOUT, "Reads     | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_reads[VOLINT_STATS_SAME_NET],
		    a_xInfoP->stat_reads[VOLINT_STATS_SAME_NET_AUTH],
		    a_xInfoP->stat_reads[VOLINT_STATS_DIFF_NET],
		    a_xInfoP->stat_reads[VOLINT_STATS_DIFF_NET_AUTH]);
	    fprintf(STDOUT, "Writes    | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_writes[VOLINT_STATS_SAME_NET],
		    a_xInfoP->stat_writes[VOLINT_STATS_SAME_NET_AUTH],
		    a_xInfoP->stat_writes[VOLINT_STATS_DIFF_NET],
		    a_xInfoP->stat_writes[VOLINT_STATS_DIFF_NET_AUTH]);
	    fprintf(STDOUT,
		    "          |-------------------------------------------|\n\n");

	    fprintf(STDOUT,
		    "                   Writes Affecting Authorship\n");
	    fprintf(STDOUT,
		    "          |-------------------------------------------|\n");
	    fprintf(STDOUT,
		    "          |   File Authorship   | Directory Authorship|\n");
	    fprintf(STDOUT,
		    "          |----------|----------|----------|----------|\n");
	    fprintf(STDOUT,
		    "          |   Same   |   Diff   |    Same  |   Diff   |\n");
	    fprintf(STDOUT,
		    "          |----------|----------|----------|----------|\n");
	    fprintf(STDOUT, "0-60 sec  | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_fileSameAuthor[VOLINT_STATS_TIME_IDX_0],
		    a_xInfoP->stat_fileDiffAuthor[VOLINT_STATS_TIME_IDX_0],
		    a_xInfoP->stat_dirSameAuthor[VOLINT_STATS_TIME_IDX_0],
		    a_xInfoP->stat_dirDiffAuthor[VOLINT_STATS_TIME_IDX_0]);
	    fprintf(STDOUT, "1-10 min  | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_fileSameAuthor[VOLINT_STATS_TIME_IDX_1],
		    a_xInfoP->stat_fileDiffAuthor[VOLINT_STATS_TIME_IDX_1],
		    a_xInfoP->stat_dirSameAuthor[VOLINT_STATS_TIME_IDX_1],
		    a_xInfoP->stat_dirDiffAuthor[VOLINT_STATS_TIME_IDX_1]);
	    fprintf(STDOUT, "10min-1hr | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_fileSameAuthor[VOLINT_STATS_TIME_IDX_2],
		    a_xInfoP->stat_fileDiffAuthor[VOLINT_STATS_TIME_IDX_2],
		    a_xInfoP->stat_dirSameAuthor[VOLINT_STATS_TIME_IDX_2],
		    a_xInfoP->stat_dirDiffAuthor[VOLINT_STATS_TIME_IDX_2]);
	    fprintf(STDOUT, "1hr-1day  | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_fileSameAuthor[VOLINT_STATS_TIME_IDX_3],
		    a_xInfoP->stat_fileDiffAuthor[VOLINT_STATS_TIME_IDX_3],
		    a_xInfoP->stat_dirSameAuthor[VOLINT_STATS_TIME_IDX_3],
		    a_xInfoP->stat_dirDiffAuthor[VOLINT_STATS_TIME_IDX_3]);
	    fprintf(STDOUT, "1day-1wk  | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_fileSameAuthor[VOLINT_STATS_TIME_IDX_4],
		    a_xInfoP->stat_fileDiffAuthor[VOLINT_STATS_TIME_IDX_4],
		    a_xInfoP->stat_dirSameAuthor[VOLINT_STATS_TIME_IDX_4],
		    a_xInfoP->stat_dirDiffAuthor[VOLINT_STATS_TIME_IDX_4]);
	    fprintf(STDOUT, "> 1wk     | %8d | %8d | %8d | %8d |\n",
		    a_xInfoP->stat_fileSameAuthor[VOLINT_STATS_TIME_IDX_5],
		    a_xInfoP->stat_fileDiffAuthor[VOLINT_STATS_TIME_IDX_5],
		    a_xInfoP->stat_dirSameAuthor[VOLINT_STATS_TIME_IDX_5],
		    a_xInfoP->stat_dirDiffAuthor[VOLINT_STATS_TIME_IDX_5]);
	    fprintf(STDOUT,
		    "          |-------------------------------------------|\n");
	} /*Volume status OK */
	else if (a_xInfoP->status == VBUSY) {
	    (*a_totalBusyP)++;
	    qPut(&busyHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "**** Volume %lu is busy ****\n",
			(unsigned long)a_xInfoP->volid);
	} /*Busy volume */
	else {
	    (*a_totalNotOKP)++;
	    qPut(&notokHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "**** Could not attach volume %lu ****\n",
			(unsigned long)a_xInfoP->volid);
	}			/*Screwed volume */
	fprintf(STDOUT, "\n");
    } /*Long listing */
    else {
	/*
	 * Default listing.
	 */
	if (a_xInfoP->status == VOK) {
	    fprintf(STDOUT, "%-32s ", a_xInfoP->name);
	    fprintf(STDOUT, "%10lu ", (unsigned long)a_xInfoP->volid);
	    if (a_xInfoP->type == 0)
		fprintf(STDOUT, "RW ");
	    if (a_xInfoP->type == 1)
		fprintf(STDOUT, "RO ");
	    if (a_xInfoP->type == 2)
		fprintf(STDOUT, "BK ");
	    fprintf(STDOUT, "%10d K ", a_xInfoP->size);
	    if (a_xInfoP->inUse == 1) {
		fprintf(STDOUT, "On-line");
		(*a_totalOKP)++;
	    } else {
		fprintf(STDOUT, "Off-line");
		(*a_totalNotOKP)++;
	    }
	    fprintf(STDOUT, "\n");
	} /*Volume OK */
	else if (a_xInfoP->status == VBUSY) {
	    (*a_totalBusyP)++;
	    qPut(&busyHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "**** Volume %lu is busy ****\n",
			(unsigned long)a_xInfoP->volid);
	} /*Busy volume */
	else {
	    (*a_totalNotOKP)++;
	    qPut(&notokHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "**** Could not attach volume %lu ****\n",
			(unsigned long)a_xInfoP->volid);
	}			/*Screwed volume */
    }				/*Default listing */
}				/*XDisplayFormat */

#ifdef FULL_LISTVOL_SWITCH
/*------------------------------------------------------------------------
 * PRIVATE XDisplayFormat2
 *
 * Description:
 *	Display the formated contents of one extended volume info structure.
 *
 * Arguments:
 *	a_xInfoP	: Ptr to extended volume info struct to print.
 *	a_servID	: Server ID to print.
 *	a_partID        : Partition ID to print.
 *	a_totalOKP	: Ptr to total-OK counter.
 *	a_totalNotOKP	: Ptr to total-screwed counter.
 *	a_totalBusyP	: Ptr to total-busy counter.
 *	a_fast		: Fast listing?
 *	a_int32		: Int32 listing?
 *	a_showProblems	: Show volume problems?
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void
XDisplayFormat2(a_xInfoP, a_servID, a_partID, a_totalOKP, a_totalNotOKP,
	       a_totalBusyP, a_fast, a_int32, a_showProblems)
     volintXInfo *a_xInfoP;
     afs_int32 a_servID;
     afs_int32 a_partID;
     int *a_totalOKP;
     int *a_totalNotOKP;
     int *a_totalBusyP;
     int a_fast;
     int a_int32;
     int a_showProblems;

{				/*XDisplayFormat */
    if (a_fast) {
	/*
	 * Short & sweet.
	 */
	fprintf(STDOUT, "vold_id\t%-10lu\n", (unsigned long)a_xInfoP->volid);
    } else if (a_int32) {
	/*
	 * Fully-detailed listing.
	 */
	if (a_xInfoP->status == VOK) {
	    /*
	     * Volume's status is OK - all the fields are valid.
	     */

                static long server_cache = -1, partition_cache = -1;
                static char hostname[256], address[32], pname[16];
                int i,ai[] = {VOLINT_STATS_TIME_IDX_0,VOLINT_STATS_TIME_IDX_1,VOLINT_STATS_TIME_IDX_2,
                              VOLINT_STATS_TIME_IDX_3,VOLINT_STATS_TIME_IDX_4,VOLINT_STATS_TIME_IDX_5};

		if (a_servID != server_cache) {
			struct in_addr s;

			s.s_addr = a_servID;
			strcpy(hostname, hostutil_GetNameByINet(a_servID));
			strcpy(address, inet_ntoa(s));
			server_cache = a_servID;
		}
		if (a_partID != partition_cache) {
			MapPartIdIntoName(a_partID, pname);
			partition_cache = a_partID;
		}

		fprintf(STDOUT, "name\t\t%s\n", a_xInfoP->name);
		fprintf(STDOUT, "id\t\t%lu\n", a_xInfoP->volid);
		fprintf(STDOUT, "serv\t\t%s\t%s\n", address, hostname);
		fprintf(STDOUT, "part\t\t%s\n", pname);
		switch (a_xInfoP->status) {
		case VOK:
			fprintf(STDOUT, "status\t\tOK\n");
			break;
		case VBUSY:
			fprintf(STDOUT, "status\t\tBUSY\n");
			return;
		default:
			fprintf(STDOUT, "status\t\tUNATTACHABLE\n");
			return;
		}
		fprintf(STDOUT, "backupID\t%lu\n", a_xInfoP->backupID);
		fprintf(STDOUT, "parentID\t%lu\n", a_xInfoP->parentID);
		fprintf(STDOUT, "cloneID\t\t%lu\n", a_xInfoP->cloneID);
		fprintf(STDOUT, "inUse\t\t%s\n", a_xInfoP->inUse ? "Y" : "N");
		switch (a_xInfoP->type) {
		case 0:
			fprintf(STDOUT, "type\t\tRW\n");
			break;
		case 1:
			fprintf(STDOUT, "type\t\tRO\n");
			break;
		case 2:
			fprintf(STDOUT, "type\t\tBK\n");
			break;
		default:
			fprintf(STDOUT, "type\t\t?\n");
			break;
		}
		fprintf(STDOUT, "creationDate\t%-9lu\t%s", a_xInfoP->creationDate,
			vos_ctime(&a_xInfoP->creationDate));
		fprintf(STDOUT, "accessDate\t%-9lu\t%s", a_xInfoP->accessDate,
			vos_ctime(&a_xInfoP->accessDate));
		fprintf(STDOUT, "updateDate\t%-9lu\t%s", a_xInfoP->updateDate,
			vos_ctime(&a_xInfoP->updateDate));
		fprintf(STDOUT, "backupDate\t%-9lu\t%s", a_xInfoP->backupDate,
			vos_ctime(&a_xInfoP->backupDate));
		fprintf(STDOUT, "copyDate\t%-9lu\t%s", a_xInfoP->copyDate,
			vos_ctime(&a_xInfoP->copyDate));
		
		fprintf(STDOUT, "diskused\t%u\n", a_xInfoP->size);
		fprintf(STDOUT, "maxquota\t%u\n", a_xInfoP->maxquota);

		fprintf(STDOUT, "filecount\t%u\n", a_xInfoP->filecount);
		fprintf(STDOUT, "dayUse\t\t%u\n", a_xInfoP->dayUse);



		fprintf(STDOUT,"reads_same_net\t%8d\n",a_xInfoP->stat_reads[VOLINT_STATS_SAME_NET]);
		fprintf(STDOUT,"reads_same_net_auth\t%8d\n",a_xInfoP->stat_reads[VOLINT_STATS_SAME_NET_AUTH]);
		fprintf(STDOUT,"reads_diff_net\t%8d\n",a_xInfoP->stat_reads[VOLINT_STATS_DIFF_NET]);
		fprintf(STDOUT,"reads_diff_net_auth\t%8d\n",a_xInfoP->stat_reads[VOLINT_STATS_DIFF_NET_AUTH]);

		fprintf(STDOUT,"writes_same_net\t%8d\n",a_xInfoP->stat_writes[VOLINT_STATS_SAME_NET]);
		fprintf(STDOUT,"writes_same_net_auth\t%8d\n",a_xInfoP->stat_writes[VOLINT_STATS_SAME_NET_AUTH]);
		fprintf(STDOUT,"writes_diff_net\t%8d\n",a_xInfoP->stat_writes[VOLINT_STATS_DIFF_NET]);
		fprintf(STDOUT,"writes_diff_net_auth\t%8d\n",a_xInfoP->stat_writes[VOLINT_STATS_DIFF_NET_AUTH]);

		for(i=0;i<5;i++)
		{
			fprintf(STDOUT,"file_same_author_idx_%d\t%8d\n",i+1,a_xInfoP->stat_fileSameAuthor[ai[i]]);
			fprintf(STDOUT,"file_diff_author_idx_%d\t%8d\n",i+1,a_xInfoP->stat_fileDiffAuthor[ai[i]]);
			fprintf(STDOUT,"dir_same_author_idx_%d\t%8d\n",i+1,a_xInfoP->stat_dirSameAuthor[ai[i]]);
			fprintf(STDOUT,"dir_dif_author_idx_%d\t%8d\n",i+1,a_xInfoP->stat_dirDiffAuthor[ai[i]]);
		}

	} /*Volume status OK */
	else if (a_xInfoP->status == VBUSY) {
	    (*a_totalBusyP)++;
	    qPut(&busyHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "BUSY_VOL\t%lu\n",
			(unsigned long)a_xInfoP->volid);
	} /*Busy volume */
	else {
	    (*a_totalNotOKP)++;
	    qPut(&notokHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "COULD_NOT_ATTACH\t%lu\n",
			(unsigned long)a_xInfoP->volid);
	}			/*Screwed volume */
    } /*Long listing */
    else {
	/*
	 * Default listing.
	 */
	if (a_xInfoP->status == VOK) {
	    fprintf(STDOUT, "name\t%-32s\n", a_xInfoP->name);
	    fprintf(STDOUT, "volID\t%10lu\n", (unsigned long)a_xInfoP->volid);
	    if (a_xInfoP->type == 0)
		fprintf(STDOUT, "type\tRW\n");
	    if (a_xInfoP->type == 1)
		fprintf(STDOUT, "type\tRO\n");
	    if (a_xInfoP->type == 2)
		fprintf(STDOUT, "type\tBK\n");
	    fprintf(STDOUT, "size\t%10dK\n", a_xInfoP->size);

	    fprintf(STDOUT, "inUse\t%d\n",a_xInfoP->inUse);
	    if (a_xInfoP->inUse == 1)
	    	(*a_totalOKP)++;
	    else
		(*a_totalNotOKP)++;

	} /*Volume OK */
	else if (a_xInfoP->status == VBUSY) {
	    (*a_totalBusyP)++;
	    qPut(&busyHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "VOLUME_BUSY\t%lu\n",
			(unsigned long)a_xInfoP->volid);
	} /*Busy volume */
	else {
	    (*a_totalNotOKP)++;
	    qPut(&notokHead, a_xInfoP->volid);
	    if (a_showProblems)
		fprintf(STDOUT, "COULD_NOT_ATTACH_VOLUME\t%lu\n",
			(unsigned long)a_xInfoP->volid);
	}			/*Screwed volume */
    }				/*Default listing */
}				/*XDisplayFormat */
#endif /*FULL_LISTVOL_SWITCH*/

#ifdef FULL_LISTVOL_SWITCH
static void
DisplayFormat2(server, partition, pntr)
     long server, partition;
     volintInfo *pntr;
{
    static long server_cache = -1, partition_cache = -1;
    static char hostname[256], address[32], pname[16];

    if (server != server_cache) {
	struct in_addr s;

	s.s_addr = server;
	strcpy(hostname, hostutil_GetNameByINet(server));
	strcpy(address, inet_ntoa(s));
	server_cache = server;
    }
    if (partition != partition_cache) {
	MapPartIdIntoName(partition, pname);
	partition_cache = partition;
    }
    fprintf(STDOUT, "name\t\t%s\n", pntr->name);
    fprintf(STDOUT, "id\t\t%lu\n", pntr->volid);
    fprintf(STDOUT, "serv\t\t%s\t%s\n", address, hostname);
    fprintf(STDOUT, "part\t\t%s\n", pname);
    switch (pntr->status) {
    case VOK:
	fprintf(STDOUT, "status\t\tOK\n");
	break;
    case VBUSY:
	fprintf(STDOUT, "status\t\tBUSY\n");
	return;
    default:
	fprintf(STDOUT, "status\t\tUNATTACHABLE\n");
	return;
    }
    fprintf(STDOUT, "backupID\t%lu\n", pntr->backupID);
    fprintf(STDOUT, "parentID\t%lu\n", pntr->parentID);
    fprintf(STDOUT, "cloneID\t\t%lu\n", pntr->cloneID);
    fprintf(STDOUT, "inUse\t\t%s\n", pntr->inUse ? "Y" : "N");
    fprintf(STDOUT, "needsSalvaged\t%s\n", pntr->needsSalvaged ? "Y" : "N");
    /* 0xD3 is from afs/volume.h since I had trouble including the file */
    fprintf(STDOUT, "destroyMe\t%s\n", pntr->destroyMe == 0xD3 ? "Y" : "N");
    switch (pntr->type) {
    case 0:
	fprintf(STDOUT, "type\t\tRW\n");
	break;
    case 1:
	fprintf(STDOUT, "type\t\tRO\n");
	break;
    case 2:
	fprintf(STDOUT, "type\t\tBK\n");
	break;
    default:
	fprintf(STDOUT, "type\t\t?\n");
	break;
    }
    fprintf(STDOUT, "creationDate\t%-9lu\t%s", pntr->creationDate,
	    vos_ctime(&pntr->creationDate));
    fprintf(STDOUT, "accessDate\t%-9lu\t%s", pntr->accessDate,
	    vos_ctime(&pntr->accessDate));
    fprintf(STDOUT, "updateDate\t%-9lu\t%s", pntr->updateDate,
	    vos_ctime(&pntr->updateDate));
    fprintf(STDOUT, "backupDate\t%-9lu\t%s", pntr->backupDate,
	    vos_ctime(&pntr->backupDate));
    fprintf(STDOUT, "copyDate\t%-9lu\t%s", pntr->copyDate,
	    vos_ctime(&pntr->copyDate));
    fprintf(STDOUT, "flags\t\t%#lx\t(Optional)\n", pntr->flags);
    fprintf(STDOUT, "diskused\t%u\n", pntr->size);
    fprintf(STDOUT, "maxquota\t%u\n", pntr->maxquota);
    fprintf(STDOUT, "minquota\t%lu\t(Optional)\n", pntr->spare0);
    fprintf(STDOUT, "filecount\t%u\n", pntr->filecount);
    fprintf(STDOUT, "dayUse\t\t%u\n", pntr->dayUse);
    fprintf(STDOUT, "weekUse\t\t%lu\t(Optional)\n", pntr->spare1);
    fprintf(STDOUT, "spare2\t\t%lu\t(Optional)\n", pntr->spare2);
    fprintf(STDOUT, "spare3\t\t%lu\t(Optional)\n", pntr->spare3);
    return;
}

static void
DisplayVolumes2(server, partition, pntr, count)
     volintInfo *pntr;
     long server, partition, count;
{
    long i;

    for (i = 0; i < count; i++) {
	fprintf(STDOUT, "BEGIN_OF_ENTRY\n");
	DisplayFormat2(server, partition, pntr);
	fprintf(STDOUT, "END_OF_ENTRY\n\n");
	pntr++;
    }
    return;
}
#endif /* FULL_LISTVOL_SWITCH */

static void
DisplayVolumes(server, part, pntr, count, longlist, fast, quiet)
     afs_int32 server, part;
     volintInfo *pntr;
     afs_int32 count, longlist, fast;
     int quiet;
{
    int totalOK, totalNotOK, totalBusy, i;
    afs_int32 volid;

    totalOK = 0;
    totalNotOK = 0;
    totalBusy = 0;
    qInit(&busyHead);
    qInit(&notokHead);
    for (i = 0; i < count; i++) {
	DisplayFormat(pntr, server, part, &totalOK, &totalNotOK, &totalBusy,
		      fast, longlist, 0);
	pntr++;
    }
    if (totalBusy) {
	while (busyHead.count) {
	    qGet(&busyHead, &volid);
	    fprintf(STDOUT, "**** Volume %lu is busy ****\n",
		    (unsigned long)volid);
	}
    }
    if (totalNotOK) {
	while (notokHead.count) {
	    qGet(&notokHead, &volid);
	    fprintf(STDOUT, "**** Could not attach volume %lu ****\n",
		    (unsigned long)volid);
	}
    }
    if (!quiet) {
	fprintf(STDOUT, "\n");
	if (!fast) {
	    fprintf(STDOUT,
		    "Total volumes onLine %d ; Total volumes offLine %d ; Total busy %d\n\n",
		    totalOK, totalNotOK, totalBusy);
	}
    }
}
/*------------------------------------------------------------------------
 * PRIVATE XDisplayVolumes
 *
 * Description:
 *	Display extended volume information.
 *
 * Arguments:
 *	a_servID : Pointer to the Rx call we're performing.
 *	a_partID : Partition for which we want the extended list.
 *	a_xInfoP : Ptr to extended volume info.
 *	a_count  : Number of volume records contained above.
 *	a_int32   : Int32 listing generated?
 *	a_fast   : Fast listing generated?
 *	a_quiet  : Quiet listing generated?
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void
XDisplayVolumes(a_servID, a_partID, a_xInfoP, a_count, a_int32, a_fast,
		a_quiet)
     afs_int32 a_servID;
     afs_int32 a_partID;
     volintXInfo *a_xInfoP;
     afs_int32 a_count;
     afs_int32 a_int32;
     afs_int32 a_fast;
     int a_quiet;

{				/*XDisplayVolumes */

    int totalOK;		/*Total OK volumes */
    int totalNotOK;		/*Total screwed volumes */
    int totalBusy;		/*Total busy volumes */
    int i;			/*Loop variable */
    afs_int32 volid;		/*Current volume ID */

    /*
     * Initialize counters and (global!!) queues.
     */
    totalOK = 0;
    totalNotOK = 0;
    totalBusy = 0;
    qInit(&busyHead);
    qInit(&notokHead);

    /*
     * Display each volume in the list.
     */
    for (i = 0; i < a_count; i++) {
	XDisplayFormat(a_xInfoP, a_servID, a_partID, &totalOK, &totalNotOK,
		       &totalBusy, a_fast, a_int32, 0);
	a_xInfoP++;
    }

    /*
     * If any volumes were found to be busy or screwed, display them.
     */
    if (totalBusy) {
	while (busyHead.count) {
	    qGet(&busyHead, &volid);
	    fprintf(STDOUT, "**** Volume %lu is busy ****\n",
		    (unsigned long)volid);
	}
    }
    if (totalNotOK) {
	while (notokHead.count) {
	    qGet(&notokHead, &volid);
	    fprintf(STDOUT, "**** Could not attach volume %lu ****\n",
		    (unsigned long)volid);
	}
    }

    if (!a_quiet) {
	fprintf(STDOUT, "\n");
	if (!a_fast) {
	    fprintf(STDOUT,
		    "Total volumes: %d on-line, %d off-line, %d  busyd\n\n",
		    totalOK, totalNotOK, totalBusy);
	}
    }

}				/*XDisplayVolumes */
#ifdef FULL_LISTVOL_SWITCH
/*------------------------------------------------------------------------
 * PRIVATE XDisplayVolumes2
 *
 * Description:
 *	Display extended formated volume information.
 *
 * Arguments:
 *	a_servID : Pointer to the Rx call we're performing.
 *	a_partID : Partition for which we want the extended list.
 *	a_xInfoP : Ptr to extended volume info.
 *	a_count  : Number of volume records contained above.
 *	a_int32   : Int32 listing generated?
 *	a_fast   : Fast listing generated?
 *	a_quiet  : Quiet listing generated?
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void
XDisplayVolumes2(a_servID, a_partID, a_xInfoP, a_count, a_int32, a_fast,
		a_quiet)
     afs_int32 a_servID;
     afs_int32 a_partID;
     volintXInfo *a_xInfoP;
     afs_int32 a_count;
     afs_int32 a_int32;
     afs_int32 a_fast;
     int a_quiet;

{				/*XDisplayVolumes */

    int totalOK;		/*Total OK volumes */
    int totalNotOK;		/*Total screwed volumes */
    int totalBusy;		/*Total busy volumes */
    int i;			/*Loop variable */
    afs_int32 volid;		/*Current volume ID */

    /*
     * Initialize counters and (global!!) queues.
     */
    totalOK = 0;
    totalNotOK = 0;
    totalBusy = 0;
    qInit(&busyHead);
    qInit(&notokHead);

    /*
     * Display each volume in the list.
     */
    for (i = 0; i < a_count; i++) {
	fprintf(STDOUT, "BEGIN_OF_ENTRY\n");
	XDisplayFormat2(a_xInfoP, a_servID, a_partID, &totalOK, &totalNotOK,
		       &totalBusy, a_fast, a_int32, 0);
	fprintf(STDOUT, "END_OF_ENTRY\n");
	a_xInfoP++;
    }

    /*
     * If any volumes were found to be busy or screwed, display them.
     */
    if (totalBusy) {
	while (busyHead.count) {
	    qGet(&busyHead, &volid);
	    fprintf(STDOUT, "BUSY_VOL\t%lu\n",
		    (unsigned long)volid);
	}
    }
    if (totalNotOK) {
	while (notokHead.count) {
	    qGet(&notokHead, &volid);
	    fprintf(STDOUT, "COULD_NOT_ATTACH\t%lu\n",
		    (unsigned long)volid);
	}
    }

    if (!a_quiet) {
	fprintf(STDOUT, "\n");
	if (!a_fast) {
	    fprintf(STDOUT,
		    "VOLUMES_ONLINE\t%d\nVOLUMES_OFFLINE\t%d\nVOLUMES_BUSY\t%d\n",
		    totalOK, totalNotOK, totalBusy);
	}
    }

}				/*XDisplayVolumes2 */
#endif /* FULL_LISTVOL_SWITCH */


/* set <server> and <part> to the correct values depending on 
 * <voltype> and <entry> */
static void
GetServerAndPart(entry, voltype, server, part, previdx)
     struct nvldbentry *entry;
     afs_int32 *server, *part;
     int voltype;
     int *previdx;
{
    int i, istart, vtype;

    *server = -1;
    *part = -1;

    /* Doesn't check for non-existance of backup volume */
    if ((voltype == RWVOL) || (voltype == BACKVOL)) {
	vtype = ITSRWVOL;
	istart = 0;		/* seach the entire entry */
    } else {
	vtype = ITSROVOL;
	/* Seach from beginning of entry or pick up where we left off */
	istart = ((*previdx < 0) ? 0 : *previdx + 1);
    }

    for (i = istart; i < entry->nServers; i++) {
	if (entry->serverFlags[i] & vtype) {
	    *server = entry->serverNumber[i];
	    *part = entry->serverPartition[i];
	    *previdx = i;
	    return;
	}
    }

    /* Didn't find any, return -1 */
    *previdx = -1;
    return;
}

static void
PostVolumeStats(entry)
     struct nvldbentry *entry;
{
    SubEnumerateEntry(entry);
    /* Check for VLOP_ALLOPERS */
    if (entry->flags & VLOP_ALLOPERS)
	fprintf(STDOUT, "    Volume is currently LOCKED  \n");
    return;
}

/*------------------------------------------------------------------------
 * PRIVATE XVolumeStats
 *
 * Description:
 *	Display extended volume information.
 *
 * Arguments:
 *	a_xInfoP  : Ptr to extended volume info.
 *	a_entryP  : Ptr to the volume's VLDB entry.
 *	a_srvID   : Server ID.
 *	a_partID  : Partition ID.
 *	a_volType : Type of volume to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void
XVolumeStats(a_xInfoP, a_entryP, a_srvID, a_partID, a_volType)
     volintXInfo *a_xInfoP;
     struct nvldbentry *a_entryP;
     afs_int32 a_srvID;
     afs_int32 a_partID;
     int a_volType;

{				/*XVolumeStats */

    int totalOK, totalNotOK, totalBusy;	/*Dummies - we don't really count here */

    XDisplayFormat(a_xInfoP,	/*Ptr to extended volume info */
		   a_srvID,	/*Server ID to print */
		   a_partID,	/*Partition ID to print */
		   &totalOK,	/*Ptr to total-OK counter */
		   &totalNotOK,	/*Ptr to total-screwed counter */
		   &totalBusy,	/*Ptr to total-busy counter */
		   0,		/*Don't do a fast listing */
		   1,		/*Do a long listing */
		   1);		/*Show volume problems */
    return;

}				/*XVolumeStats */

static void
VolumeStats(pntr, entry, server, part, voltype)
     volintInfo *pntr;
     struct nvldbentry *entry;
     int voltype;
     afs_int32 server, part;
{
    int totalOK, totalNotOK, totalBusy;

    DisplayFormat(pntr, server, part, &totalOK, &totalNotOK, &totalBusy, 0, 1,
		  1);
    return;
}

/* command to forcibly remove a volume */
static int
NukeVolume(register struct cmd_syndesc *as)
{
    register afs_int32 code;
    afs_int32 volID, err;
    afs_int32 partID;
    afs_int32 server;
    register char *tp;

    server = GetServer(tp = as->parms[0].items->data);
    if (!server) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n", tp);
	return 1;
    }

    partID = volutil_GetPartitionID(tp = as->parms[1].items->data);
    if (partID == -1) {
	fprintf(STDERR, "vos: could not parse '%s' as a partition name", tp);
	return 1;
    }

    volID = vsu_GetVolumeID(tp = as->parms[2].items->data, cstruct, &err);
    if (volID == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR,
		    "vos: could not parse '%s' as a numeric volume ID", tp);
	return 1;
    }

    fprintf(STDOUT,
	    "vos: forcibly removing all traces of volume %d, please wait...",
	    volID);
    fflush(STDOUT);
    code = UV_NukeVolume(server, partID, volID);
    if (code == 0)
	fprintf(STDOUT, "done.\n");
    else
	fprintf(STDOUT, "failed with code %d.\n", code);
    return code;
}


/*------------------------------------------------------------------------
 * PRIVATE ExamineVolume
 *
 * Description:
 *	Routine used to examine a single volume, contacting the VLDB as
 *	well as the Volume Server.
 *
 * Arguments:
 *	as : Ptr to parsed command line arguments.
 *
 * Returns:
 *	0 for a successful operation,
 *	Otherwise, one of the ubik or VolServer error values.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------
 */
static int
ExamineVolume(register struct cmd_syndesc *as, void *arock)
{
    struct nvldbentry entry;
    afs_int32 vcode = 0;
    volintInfo *pntr = (volintInfo *) 0;
    volintXInfo *xInfoP = (volintXInfo *) 0;
    afs_int32 volid;
    afs_int32 code, err, error = 0;
    int voltype, foundserv = 0, foundentry = 0;
    afs_int32 aserver, apart;
    int previdx = -1;
    int wantExtendedInfo;	/*Do we want extended vol info? */

    wantExtendedInfo = (as->parms[1].items ? 1 : 0);	/* -extended */

    volid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);	/* -id */
    if (volid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "Unknown volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return -1;
    }

    if (verbose) {
	fprintf(STDOUT, "Fetching VLDB entry for %lu .. ",
		(unsigned long)volid);
	fflush(STDOUT);
    }
    vcode = VLDB_GetEntryByID(volid, -1, &entry);
    if (vcode) {
	fprintf(STDERR,
		"Could not fetch the entry for volume number %lu from VLDB \n",
		(unsigned long)volid);
	return (vcode);
    }
    if (verbose)
	fprintf(STDOUT, "done\n");
    MapHostToNetwork(&entry);

    if (entry.volumeId[RWVOL] == volid)
	voltype = RWVOL;
    else if (entry.volumeId[BACKVOL] == volid)
	voltype = BACKVOL;
    else			/* (entry.volumeId[ROVOL] == volid) */
	voltype = ROVOL;

    do {			/* do {...} while (voltype == ROVOL) */
	/* Get the entry for the volume. If its a RW vol, get the RW entry.
	 * It its a BK vol, get the RW entry (even if VLDB may say the BK doen't exist).
	 * If its a RO vol, get the next RO entry.
	 */
	GetServerAndPart(&entry, ((voltype == ROVOL) ? ROVOL : RWVOL),
			 &aserver, &apart, &previdx);
	if (previdx == -1) {	/* searched all entries */
	    if (!foundentry) {
		fprintf(STDERR, "Volume %s does not exist in VLDB\n\n",
			as->parms[0].items->data);
		error = ENOENT;
	    }
	    break;
	}
	foundentry = 1;

	/* Get information about the volume from the server */
	if (verbose) {
	    fprintf(STDOUT, "Getting volume listing from the server %s .. ",
		    hostutil_GetNameByINet(aserver));
	    fflush(STDOUT);
	}
	if (wantExtendedInfo)
	    code = UV_XListOneVolume(aserver, apart, volid, &xInfoP);
	else
	    code = UV_ListOneVolume(aserver, apart, volid, &pntr);
	if (verbose)
	    fprintf(STDOUT, "done\n");

	if (code) {
	    error = code;
	    if (code == ENODEV) {
		if ((voltype == BACKVOL) && !(entry.flags & BACK_EXISTS)) {
		    /* The VLDB says there is no backup volume and its not on disk */
		    fprintf(STDERR, "Volume %s does not exist\n",
			    as->parms[0].items->data);
		    error = ENOENT;
		} else {
		    fprintf(STDERR,
			    "Volume does not exist on server %s as indicated by the VLDB\n",
			    hostutil_GetNameByINet(aserver));
		}
	    } else {
		PrintDiagnostics("examine", code);
	    }
	    fprintf(STDOUT, "\n");
	} else {
	    foundserv = 1;
	    if (wantExtendedInfo)
		XVolumeStats(xInfoP, &entry, aserver, apart, voltype);
	    else
#ifdef FULL_LISTVOL_SWITCH
	    if (as->parms[2].items) {
		DisplayFormat2(aserver, apart, pntr);
		EnumerateEntry(&entry);
	    } else
#endif /* FULL_LISTVOL_SWITCH */
		VolumeStats(pntr, &entry, aserver, apart, voltype);

	    if ((voltype == BACKVOL) && !(entry.flags & BACK_EXISTS)) {
		/* The VLDB says there is no backup volume yet we found one on disk */
		fprintf(STDERR, "Volume %s does not exist in VLDB\n",
			as->parms[0].items->data);
		error = ENOENT;
	    }
	}

	if (pntr)
	    free(pntr);
	if (xInfoP)
	    free(xInfoP);
    } while (voltype == ROVOL);

    if (!foundserv) {
	fprintf(STDERR, "Dump only information from VLDB\n\n");
	fprintf(STDOUT, "%s \n", entry.name);	/* PostVolumeStats doesn't print name */
    }
    PostVolumeStats(&entry);

    return (error);
}

/*------------------------------------------------------------------------
 * PRIVATE SetFields
 *
 * Description:
 *	Routine used to change the status of a single volume.
 *
 * Arguments:
 *	as : Ptr to parsed command line arguments.
 *
 * Returns:
 *	0 for a successful operation,
 *	Otherwise, one of the ubik or VolServer error values.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------
 */
static int
SetFields(register struct cmd_syndesc *as, void *arock)
{
    struct nvldbentry entry;
    afs_int32 vcode = 0;
    volintInfo info;
    afs_int32 volid;
    afs_int32 code, err;
    afs_int32 aserver, apart;
    int previdx = -1;

    volid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);	/* -id */
    if (volid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "Unknown volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return -1;
    }

    code = VLDB_GetEntryByID(volid, RWVOL, &entry);
    if (code) {
	fprintf(STDERR,
		"Could not fetch the entry for volume number %lu from VLDB \n",
		(unsigned long)volid);
	return (code);
    }
    MapHostToNetwork(&entry);

    GetServerAndPart(&entry, RWVOL, &aserver, &apart, &previdx);
    if (previdx == -1) {
	fprintf(STDERR, "Volume %s does not exist in VLDB\n\n",
		as->parms[0].items->data);
	return (ENOENT);
    }

    init_volintInfo(&info);
    info.volid = volid;
    info.type = RWVOL;

    if (as->parms[1].items) {
	/* -max <quota> */
	code = util_GetInt32(as->parms[1].items->data, &info.maxquota);
	if (code) {
	    fprintf(STDERR, "invalid quota value\n");
	    return code;
	}
    }
    if (as->parms[2].items) {
	/* -clearuse */
	info.dayUse = 0;
    }
    if (as->parms[3].items) {
	/* -clearVolUpCounter */
	info.spare2 = 0;
    }
    code = UV_SetVolumeInfo(aserver, apart, volid, &info);
    if (code)
	fprintf(STDERR,
		"Could not update volume info fields for volume number %lu\n",
		(unsigned long)volid);
    return (code);
}

/*------------------------------------------------------------------------
 * PRIVATE volOnline
 *
 * Description:
 *      Brings a volume online.
 *
 * Arguments:
 *	as : Ptr to parsed command line arguments.
 *
 * Returns:
 *	0 for a successful operation,
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------
 */
static int
volOnline(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 server, partition, volid;
    afs_int32 code, err = 0;

    server = GetServer(as->parms[0].items->data);
    if (server == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	return -1;
    }

    partition = volutil_GetPartitionID(as->parms[1].items->data);
    if (partition < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	return ENOENT;
    }

    volid = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &err);	/* -id */
    if (!volid) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "Unknown volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return -1;
    }

    code = UV_SetVolume(server, partition, volid, ITOffline, 0 /*online */ ,
			0 /*sleep */ );
    if (code) {
	fprintf(STDERR, "Failed to set volume. Code = %d\n", code);
	return -1;
    }

    return 0;
}

/*------------------------------------------------------------------------
 * PRIVATE volOffline
 *
 * Description:
 *      Brings a volume offline.
 *
 * Arguments:
 *	as : Ptr to parsed command line arguments.
 *
 * Returns:
 *	0 for a successful operation,
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------
 */
static int
volOffline(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 server, partition, volid;
    afs_int32 code, err = 0;
    afs_int32 transflag, sleeptime, transdone;

    server = GetServer(as->parms[0].items->data);
    if (server == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	return -1;
    }

    partition = volutil_GetPartitionID(as->parms[1].items->data);
    if (partition < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	return ENOENT;
    }

    volid = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &err);	/* -id */
    if (!volid) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "Unknown volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return -1;
    }

    transflag = (as->parms[4].items ? ITBusy : ITOffline);
    sleeptime = (as->parms[3].items ? atol(as->parms[3].items->data) : 0);
    transdone = (sleeptime ? 0 /*online */ : VTOutOfService);
    if (as->parms[4].items && !as->parms[3].items) {
	fprintf(STDERR, "-sleep option must be used with -busy flag\n");
	return -1;
    }

    code =
	UV_SetVolume(server, partition, volid, transflag, transdone,
		     sleeptime);
    if (code) {
	fprintf(STDERR, "Failed to set volume. Code = %d\n", code);
	return -1;
    }

    return 0;
}

static int
CreateVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 pnum;
    char part[10];
    afs_int32 volid, code;
    struct nvldbentry entry;
    afs_int32 vcode;
    afs_int32 quota;

    quota = 5000;
    tserver = GetServer(as->parms[0].items->data);
    if (!tserver) {
	fprintf(STDERR, "vos: host '%s' not found in host table\n",
		as->parms[0].items->data);
	return ENOENT;
    }
    pnum = volutil_GetPartitionID(as->parms[1].items->data);
    if (pnum < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	return ENOENT;
    }
    if (!IsPartValid(pnum, tserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[1].items->data);
	return ENOENT;
    }
    if (!ISNAMEVALID(as->parms[2].items->data)) {
	fprintf(STDERR,
		"vos: the name of the root volume %s exceeds the size limit of %d\n",
		as->parms[2].items->data, VOLSER_OLDMAXVOLNAME - 10);
	return E2BIG;
    }
    if (!VolNameOK(as->parms[2].items->data)) {
	fprintf(STDERR,
		"Illegal volume name %s, should not end in .readonly or .backup\n",
		as->parms[2].items->data);
	return EINVAL;
    }
    if (IsNumeric(as->parms[2].items->data)) {
	fprintf(STDERR, "Illegal volume name %s, should not be a number\n",
		as->parms[2].items->data);
	return EINVAL;
    }
    vcode = VLDB_GetEntryByName(as->parms[2].items->data, &entry);
    if (!vcode) {
	fprintf(STDERR, "Volume %s already exists\n",
		as->parms[2].items->data);
	PrintDiagnostics("create", code);
	return EEXIST;
    }

    if (as->parms[3].items) {
	if (!IsNumeric(as->parms[3].items->data)) {
	    fprintf(STDERR, "Initial quota %s should be numeric.\n",
		    as->parms[3].items->data);
	    return EINVAL;
	}

	code = util_GetInt32(as->parms[3].items->data, &quota);
	if (code) {
	    fprintf(STDERR, "vos: bad integer specified for quota.\n");
	    return code;
	}
    }

    code =
	UV_CreateVolume2(tserver, pnum, as->parms[2].items->data, quota, 0,
			 0, 0, 0, &volid);
    if (code) {
	PrintDiagnostics("create", code);
	return code;
    }
    MapPartIdIntoName(pnum, part);
    fprintf(STDOUT, "Volume %lu created on partition %s of %s\n",
	    (unsigned long)volid, part, as->parms[0].items->data);

    return 0;
}

static afs_int32
DeleteAll(entry)
     struct nvldbentry *entry;
{
    int i;
    afs_int32 error, code, curserver, curpart, volid;

    MapHostToNetwork(entry);
    error = 0;
    for (i = 0; i < entry->nServers; i++) {
	curserver = entry->serverNumber[i];
	curpart = entry->serverPartition[i];
	if (entry->serverFlags[i] & ITSROVOL) {
	    volid = entry->volumeId[ROVOL];
	} else {
	    volid = entry->volumeId[RWVOL];
	}
	code = UV_DeleteVolume(curserver, curpart, volid);
	if (code && !error)
	    error = code;
    }
    return error;
}

static int
DeleteVolume(struct cmd_syndesc *as, void *arock)
{
    afs_int32 err, code = 0;
    afs_int32 server = 0, partition = -1, volid;
    char pname[10];
    afs_int32 idx, j;

    if (as->parms[0].items) {
	server = GetServer(as->parms[0].items->data);
	if (!server) {
	    fprintf(STDERR, "vos: server '%s' not found in host table\n",
		    as->parms[0].items->data);
	    return ENOENT;
	}
    }

    if (as->parms[1].items) {
	partition = volutil_GetPartitionID(as->parms[1].items->data);
	if (partition < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[1].items->data);
	    return EINVAL;
	}

	/* Check for validity of the partition */
	if (!IsPartValid(partition, server, &code)) {
	    if (code) {
		PrintError("", code);
	    } else {
		fprintf(STDERR,
			"vos : partition %s does not exist on the server\n",
			as->parms[1].items->data);
	    }
	    return ENOENT;
	}
    }

    volid = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &err);
    if (volid == 0) {
	fprintf(STDERR, "Can't find volume name '%s' in VLDB\n",
		as->parms[2].items->data);
	if (err)
	    PrintError("", err);
	return ENOENT;
    }

    /* If the server or partition option are not complete, try to fill
     * them in from the VLDB entry.
     */
    if ((partition == -1) || !server) {
	struct nvldbentry entry;

	code = VLDB_GetEntryByID(volid, -1, &entry);
	if (code) {
	    fprintf(STDERR,
		    "Could not fetch the entry for volume %lu from VLDB\n",
		    (unsigned long)volid);
	    PrintError("", code);
	    return (code);
	}

	if (((volid == entry.volumeId[RWVOL]) && (entry.flags & RW_EXISTS))
	    || ((volid == entry.volumeId[BACKVOL])
		&& (entry.flags & BACK_EXISTS))) {
	    idx = Lp_GetRwIndex(&entry);
	    if ((idx == -1) || (server && (server != entry.serverNumber[idx]))
		|| ((partition != -1)
		    && (partition != entry.serverPartition[idx]))) {
		fprintf(STDERR, "VLDB: Volume '%s' no match\n",
			as->parms[2].items->data);
		return ENOENT;
	    }
	} else if ((volid == entry.volumeId[ROVOL])
		   && (entry.flags & RO_EXISTS)) {
	    for (idx = -1, j = 0; j < entry.nServers; j++) {
		if (entry.serverFlags[j] != ITSROVOL)
		    continue;

		if (((server == 0) || (server == entry.serverNumber[j]))
		    && ((partition == -1)
			|| (partition == entry.serverPartition[j]))) {
		    if (idx != -1) {
			fprintf(STDERR,
				"VLDB: Volume '%s' matches more than one RO\n",
				as->parms[2].items->data);
			return ENOENT;
		    }
		    idx = j;
		}
	    }
	    if (idx == -1) {
		fprintf(STDERR, "VLDB: Volume '%s' no match\n",
			as->parms[2].items->data);
		return ENOENT;
	    }
	} else {
	    fprintf(STDERR, "VLDB: Volume '%s' no match\n",
		    as->parms[2].items->data);
	    return ENOENT;
	}

	server = htonl(entry.serverNumber[idx]);
	partition = entry.serverPartition[idx];
    }


    code = UV_DeleteVolume(server, partition, volid);
    if (code) {
	PrintDiagnostics("remove", code);
	return code;
    }

    MapPartIdIntoName(partition, pname);
    fprintf(STDOUT, "Volume %lu on partition %s server %s deleted\n",
	    (unsigned long)volid, pname, hostutil_GetNameByINet(server));
    return 0;
}

#define TESTM	0		/* set for move space tests, clear for production */
static
MoveVolume(register struct cmd_syndesc *as, void *arock)
{

    afs_int32 volid, fromserver, toserver, frompart, topart;
    afs_int32 flags, code, err;
    char fromPartName[10], toPartName[10];

    struct diskPartition partition;	/* for space check */
    volintInfo *p;

    volid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (volid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }
    fromserver = GetServer(as->parms[1].items->data);
    if (fromserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[1].items->data);
	return ENOENT;
    }
    toserver = GetServer(as->parms[3].items->data);
    if (toserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[3].items->data);
	return ENOENT;
    }
    frompart = volutil_GetPartitionID(as->parms[2].items->data);
    if (frompart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[2].items->data);
	return EINVAL;
    }
    if (!IsPartValid(frompart, fromserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[2].items->data);
	return ENOENT;
    }
    topart = volutil_GetPartitionID(as->parms[4].items->data);
    if (topart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[4].items->data);
	return EINVAL;
    }
    if (!IsPartValid(topart, toserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[4].items->data);
	return ENOENT;
    }

    flags = 0;
    if (as->parms[5].items) flags |= RV_NOCLONE;

    /*
     * check source partition for space to clone volume
     */

    MapPartIdIntoName(topart, toPartName);
    MapPartIdIntoName(frompart, fromPartName);

    /*
     * check target partition for space to move volume
     */

    code = UV_PartitionInfo(toserver, toPartName, &partition);
    if (code) {
	fprintf(STDERR, "vos: cannot access partition %s\n", toPartName);
	exit(1);
    }
    if (TESTM)
	fprintf(STDOUT, "target partition %s free space %d\n", toPartName,
		partition.free);

    p = (volintInfo *) 0;
    code = UV_ListOneVolume(fromserver, frompart, volid, &p);
    if (code) {
	fprintf(STDERR, "vos:cannot access volume %lu\n",
		(unsigned long)volid);
	exit(1);
    }
    if (TESTM)
	fprintf(STDOUT, "volume %lu size %d\n", (unsigned long)volid,
		p->size);
    if (partition.free <= p->size) {
	fprintf(STDERR,
		"vos: no space on target partition %s to move volume %lu\n",
		toPartName, (unsigned long)volid);
	free(p);
	exit(1);
    }
    free(p);

    if (TESTM) {
	fprintf(STDOUT, "size test - don't do move\n");
	exit(0);
    }

    /* successful move still not guaranteed but shoot for it */

    code =
	UV_MoveVolume2(volid, fromserver, frompart, toserver, topart, flags);
    if (code) {
	PrintDiagnostics("move", code);
	return code;
    }
    MapPartIdIntoName(topart, toPartName);
    MapPartIdIntoName(frompart, fromPartName);
    fprintf(STDOUT, "Volume %lu moved from %s %s to %s %s \n",
	    (unsigned long)volid, as->parms[1].items->data, fromPartName,
	    as->parms[3].items->data, toPartName);

    return 0;
}

static int
CopyVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 volid, fromserver, toserver, frompart, topart, code, err, flags;
    char fromPartName[10], toPartName[10], *tovolume;
    struct nvldbentry entry;
    struct diskPartition partition;	/* for space check */
    volintInfo *p;

    volid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (volid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }
    fromserver = GetServer(as->parms[1].items->data);
    if (fromserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[1].items->data);
	return ENOENT;
    }

    toserver = GetServer(as->parms[4].items->data);
    if (toserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[4].items->data);
	return ENOENT;
    }

    tovolume = as->parms[3].items->data;
    if (!ISNAMEVALID(tovolume)) {
	fprintf(STDERR,
		"vos: the name of the root volume %s exceeds the size limit of %d\n",
		tovolume, VOLSER_OLDMAXVOLNAME - 10);
	return E2BIG;
    }
    if (!VolNameOK(tovolume)) {
	fprintf(STDERR,
		"Illegal volume name %s, should not end in .readonly or .backup\n",
		tovolume);
	return EINVAL;
    }
    if (IsNumeric(tovolume)) {
	fprintf(STDERR, "Illegal volume name %s, should not be a number\n",
		tovolume);
	return EINVAL;
    }
    code = VLDB_GetEntryByName(tovolume, &entry);
    if (!code) {
	fprintf(STDERR, "Volume %s already exists\n", tovolume);
	PrintDiagnostics("copy", code);
	return EEXIST;
    }

    frompart = volutil_GetPartitionID(as->parms[2].items->data);
    if (frompart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[2].items->data);
	return EINVAL;
    }
    if (!IsPartValid(frompart, fromserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[2].items->data);
	return ENOENT;
    }

    topart = volutil_GetPartitionID(as->parms[5].items->data);
    if (topart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[5].items->data);
	return EINVAL;
    }
    if (!IsPartValid(topart, toserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[5].items->data);
	return ENOENT;
    }

    flags = 0;
    if (as->parms[6].items) flags |= RV_OFFLINE;
    if (as->parms[7].items) flags |= RV_RDONLY;
    if (as->parms[8].items) flags |= RV_NOCLONE;

    MapPartIdIntoName(topart, toPartName);
    MapPartIdIntoName(frompart, fromPartName);

    /*
     * check target partition for space to move volume
     */

    code = UV_PartitionInfo(toserver, toPartName, &partition);
    if (code) {
	fprintf(STDERR, "vos: cannot access partition %s\n", toPartName);
	exit(1);
    }
    if (TESTM)
	fprintf(STDOUT, "target partition %s free space %d\n", toPartName,
		partition.free);

    p = (volintInfo *) 0;
    code = UV_ListOneVolume(fromserver, frompart, volid, &p);
    if (code) {
	fprintf(STDERR, "vos:cannot access volume %lu\n",
		(unsigned long)volid);
	exit(1);
    }

    if (partition.free <= p->size) {
	fprintf(STDERR,
		"vos: no space on target partition %s to copy volume %lu\n",
		toPartName, (unsigned long)volid);
	free(p);
	exit(1);
    }
    free(p);

    /* successful copy still not guaranteed but shoot for it */

    code =
	UV_CopyVolume2(volid, fromserver, frompart, tovolume, toserver,
		       topart, 0, flags);
    if (code) {
	PrintDiagnostics("copy", code);
	return code;
    }
    MapPartIdIntoName(topart, toPartName);
    MapPartIdIntoName(frompart, fromPartName);
    fprintf(STDOUT, "Volume %lu copied from %s %s to %s on %s %s \n",
	    (unsigned long)volid, as->parms[1].items->data, fromPartName,
	    tovolume, as->parms[4].items->data, toPartName);

    return 0;
}


static int
ShadowVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 volid, fromserver, toserver, frompart, topart, tovolid;
    afs_int32 code, err, flags;
    char fromPartName[10], toPartName[10], toVolName[32], *tovolume;
    struct nvldbentry entry;
    struct diskPartition partition;	/* for space check */
    volintInfo *p, *q;

    p = (volintInfo *) 0;
    q = (volintInfo *) 0;

    volid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (volid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }
    fromserver = GetServer(as->parms[1].items->data);
    if (fromserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[1].items->data);
	return ENOENT;
    }

    toserver = GetServer(as->parms[3].items->data);
    if (toserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[3].items->data);
	return ENOENT;
    }

    frompart = volutil_GetPartitionID(as->parms[2].items->data);
    if (frompart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[2].items->data);
	return EINVAL;
    }
    if (!IsPartValid(frompart, fromserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[2].items->data);
	return ENOENT;
    }

    topart = volutil_GetPartitionID(as->parms[4].items->data);
    if (topart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[4].items->data);
	return EINVAL;
    }
    if (!IsPartValid(topart, toserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[4].items->data);
	return ENOENT;
    }

    if (as->parms[5].items) {
	tovolume = as->parms[5].items->data;
	if (!ISNAMEVALID(tovolume)) {
	    fprintf(STDERR,
		"vos: the name of the root volume %s exceeds the size limit of %d\n",
		tovolume, VOLSER_OLDMAXVOLNAME - 10);
	    return E2BIG;
	}
	if (!VolNameOK(tovolume)) {
	    fprintf(STDERR,
		"Illegal volume name %s, should not end in .readonly or .backup\n",
		tovolume);
	    return EINVAL;
	}
	if (IsNumeric(tovolume)) {
	    fprintf(STDERR,
		"Illegal volume name %s, should not be a number\n",
		tovolume);
	    return EINVAL;
	}
    } else {
	/* use actual name of source volume */
	code = UV_ListOneVolume(fromserver, frompart, volid, &p);
	if (code) {
	    fprintf(STDERR, "vos:cannot access volume %lu\n",
		(unsigned long)volid);
	    exit(1);
	}
	strcpy(toVolName, p->name);
	tovolume = toVolName;
	/* save p for size checks later */
    }

    if (as->parms[6].items) {
	tovolid = vsu_GetVolumeID(as->parms[6].items->data, cstruct, &err);
	if (tovolid == 0) {
	    if (err)
		PrintError("", err);
	    else
		fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
			as->parms[6].items->data);
	    if (p)
		free(p);
	    return ENOENT;
	}
    } else {
	tovolid = vsu_GetVolumeID(tovolume, cstruct, &err);
	if (tovolid == 0) {
	    if (err)
		PrintError("", err);
	    else
		fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
			tovolume);
	    if (p)
		free(p);
	    return ENOENT;
	}
    }

    flags = RV_NOVLDB;
    if (as->parms[7].items) flags |= RV_OFFLINE;
    if (as->parms[8].items) flags |= RV_RDONLY;
    if (as->parms[9].items) flags |= RV_NOCLONE;
    if (as->parms[10].items) flags |= RV_CPINCR;

    MapPartIdIntoName(topart, toPartName);
    MapPartIdIntoName(frompart, fromPartName);

    /*
     * check target partition for space to move volume
     */

    code = UV_PartitionInfo(toserver, toPartName, &partition);
    if (code) {
	fprintf(STDERR, "vos: cannot access partition %s\n", toPartName);
	exit(1);
    }
    if (TESTM)
	fprintf(STDOUT, "target partition %s free space %d\n", toPartName,
		partition.free);

    /* Don't do this again if we did it above */
    if (!p) {
	code = UV_ListOneVolume(fromserver, frompart, volid, &p);
	if (code) {
	    fprintf(STDERR, "vos:cannot access volume %lu\n",
		(unsigned long)volid);
	    exit(1);
	}
    }

    /* OK if this fails */
    code = UV_ListOneVolume(toserver, topart, tovolid, &q);

    /* Treat existing volume size as "free" */
    if (q)
	p->size = (q->size < p->size) ? p->size - q->size : 0;

    if (partition.free <= p->size) {
	fprintf(STDERR,
		"vos: no space on target partition %s to copy volume %lu\n",
		toPartName, (unsigned long)volid);
	free(p);
	if (q) free(q);
	exit(1);
    }
    free(p);
    if (q) free(q);

    /* successful copy still not guaranteed but shoot for it */

    code =
	UV_CopyVolume2(volid, fromserver, frompart, tovolume, toserver,
		       topart, tovolid, flags);
    if (code) {
	PrintDiagnostics("shadow", code);
	return code;
    }
    MapPartIdIntoName(topart, toPartName);
    MapPartIdIntoName(frompart, fromPartName);
    fprintf(STDOUT, "Volume %lu shadowed from %s %s to %s %s \n",
	    (unsigned long)volid, as->parms[1].items->data, fromPartName,
	    as->parms[3].items->data, toPartName);

    return 0;
}


static int
CloneVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 server, part, volid, cloneid, voltype;
    char partName[10], *volname;
    afs_int32 code, err, flags;
    struct nvldbentry entry;

    volid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (volid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }

    if (as->parms[1].items || as->parms[2].items) {
	if (!as->parms[1].items || !as->parms[2].items) {
	    fprintf(STDERR,
		    "Must specify both -server and -partition options\n");
	    return -1;
	}
	server = GetServer(as->parms[1].items->data);
	if (server == 0) {
	    fprintf(STDERR, "vos: server '%s' not found in host table\n",
		    as->parms[1].items->data);
	    return ENOENT;
	}
	part = volutil_GetPartitionID(as->parms[2].items->data);
	if (part < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[2].items->data);
	    return EINVAL;
	}
	if (!IsPartValid(part, server, &code)) {	/*check for validity of the partition */
	    if (code)
		PrintError("", code);
	    else
		fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[2].items->data);
	    return ENOENT;
        }
    } else {
	code = GetVolumeInfo(volid, &server, &part, &voltype, &entry);
	if (code)
	    return code;
    }

    volname = 0;
    if (as->parms[3].items) {
	volname = as->parms[3].items->data;
	if (strlen(volname) > VOLSER_OLDMAXVOLNAME - 1) {
	    fprintf(STDERR,
		"vos: the name of the root volume %s exceeds the size limit of %d\n",
		volname, VOLSER_OLDMAXVOLNAME - 1);
	    return E2BIG;
	}
#if 0
	/* 
	 * In order that you be able to make clones of RO or BK, this
	 * check must be omitted.
	 */
	if (!VolNameOK(volname)) {
	    fprintf(STDERR,
		"Illegal volume name %s, should not end in .readonly or .backup\n",
		volname);
	    return EINVAL;
	}
#endif
	if (IsNumeric(volname)) {
	    fprintf(STDERR,
		"Illegal volume name %s, should not be a number\n",
		volname);
	    return EINVAL;
	}
    }

    cloneid = 0;
    if (as->parms[4].items) {
	cloneid = vsu_GetVolumeID(as->parms[4].items->data, cstruct, &err);
	if (cloneid == 0) {
	    if (err)
		PrintError("", err);
	    else
		fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
			as->parms[4].items->data);
	    return ENOENT;
	}
    }

    flags = 0;
    if (as->parms[5].items) flags |= RV_OFFLINE;
    if (as->parms[6].items) flags |= RV_RDONLY;


    code = 
	UV_CloneVolume(server, part, volid, cloneid, volname, flags);

    if (code) {
	PrintDiagnostics("clone", code);
	return code;
    }
    MapPartIdIntoName(part, partName);
    fprintf(STDOUT, "Created clone for volume %s\n",
	    as->parms[0].items->data);

    return 0;
}


static int
BackupVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, aserver, apart, vtype, code, err;
    struct nvldbentry entry;

    afs_int32 buvolid, buserver, bupart, butype;
    struct nvldbentry buentry;

    avolid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }
    code = GetVolumeInfo(avolid, &aserver, &apart, &vtype, &entry);
    if (code)
	exit(1);

    /* verify this is a readwrite volume */

    if (vtype != RWVOL) {
	fprintf(STDERR, "%s not RW volume\n", as->parms[0].items->data);
	exit(1);
    }

    /* is there a backup volume already? */

    if (entry.flags & BACK_EXISTS) {
	/* yep, where is it? */

	buvolid = entry.volumeId[BACKVOL];
	code = GetVolumeInfo(buvolid, &buserver, &bupart, &butype, &buentry);
	if (code)
	    exit(1);

	/* is it local? */
	code = VLDB_IsSameAddrs(buserver, aserver, &err);
	if (err) {
	    fprintf(STDERR,
		    "Failed to get info about server's %d address(es) from vlserver; aborting call!\n",
		    buserver);
	    exit(1);
	}
	if (!code) {
	    fprintf(STDERR,
		    "FATAL ERROR: backup volume %lu exists on server %lu\n",
		    (unsigned long)buvolid, (unsigned long)buserver);
	    exit(1);
	}
    }

    /* nope, carry on */

    code = UV_BackupVolume(aserver, apart, avolid);

    if (code) {
	PrintDiagnostics("backup", code);
	return code;
    }
    fprintf(STDOUT, "Created backup volume for %s \n",
	    as->parms[0].items->data);
    return 0;
}

static int
ReleaseVolume(register struct cmd_syndesc *as, void *arock)
{

    struct nvldbentry entry;
    afs_int32 avolid, aserver, apart, vtype, code, err;
    int force = 0;

    if (as->parms[1].items)
	force = 1;
    avolid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }
    code = GetVolumeInfo(avolid, &aserver, &apart, &vtype, &entry);
    if (code)
	return code;

    if (vtype != RWVOL) {
	fprintf(STDERR, "%s not a RW volume\n", as->parms[0].items->data);
	return (ENOENT);
    }

    if (!ISNAMEVALID(entry.name)) {
	fprintf(STDERR,
		"Volume name %s is too long, rename before releasing\n",
		entry.name);
	return E2BIG;
    }

    code = UV_ReleaseVolume(avolid, aserver, apart, force);
    if (code) {
	PrintDiagnostics("release", code);
	return code;
    }
    fprintf(STDOUT, "Released volume %s successfully\n",
	    as->parms[0].items->data);
    return 0;
}

static
DumpVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, aserver, apart, voltype, fromdate = 0, code, err, i, flags;
    char filename[MAXPATHLEN];
    struct nvldbentry entry;

    rx_SetRxDeadTime(60 * 10);
    for (i = 0; i < MAXSERVERS; i++) {
	struct rx_connection *rxConn = ubik_GetRPCConn(cstruct, i);
	if (rxConn == 0)
	    break;
	rx_SetConnDeadTime(rxConn, rx_connDeadTime);
	if (rxConn->service)
	    rxConn->service->connDeadTime = rx_connDeadTime;
    }

    avolid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }

    if (as->parms[3].items || as->parms[4].items) {
	if (!as->parms[3].items || !as->parms[4].items) {
	    fprintf(STDERR,
		    "Must specify both -server and -partition options\n");
	    return -1;
	}
	aserver = GetServer(as->parms[3].items->data);
	if (aserver == 0) {
	    fprintf(STDERR, "Invalid server name\n");
	    return -1;
	}
	apart = volutil_GetPartitionID(as->parms[4].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "Invalid partition name\n");
	    return -1;
	}
    } else {
	code = GetVolumeInfo(avolid, &aserver, &apart, &voltype, &entry);
	if (code)
	    return code;
    }

    if (as->parms[1].items && strcmp(as->parms[1].items->data, "0")) {
	code = ktime_DateToInt32(as->parms[1].items->data, &fromdate);
	if (code) {
	    fprintf(STDERR, "vos: failed to parse date '%s' (error=%d))\n",
		    as->parms[1].items->data, code);
	    return code;
	}
    }
    if (as->parms[2].items) {
	strcpy(filename, as->parms[2].items->data);
    } else {
	strcpy(filename, "");
    }

    flags = as->parms[6].items ? VOLDUMPV2_OMITDIRS : 0;
retry_dump:
    if (as->parms[5].items) {
	code =
	    UV_DumpClonedVolume(avolid, aserver, apart, fromdate,
				DumpFunction, filename, flags);
    } else {
	code =
	    UV_DumpVolume(avolid, aserver, apart, fromdate, DumpFunction,
			  filename, flags);
    }
    if ((code == RXGEN_OPCODE) && (as->parms[6].items)) {
	flags &= ~VOLDUMPV2_OMITDIRS;
	goto retry_dump;
    }
    if (code) {
	PrintDiagnostics("dump", code);
	return code;
    }
    if (strcmp(filename, ""))
	fprintf(STDERR, "Dumped volume %s in file %s\n",
		as->parms[0].items->data, filename);
    else
	fprintf(STDERR, "Dumped volume %s in stdout \n",
		as->parms[0].items->data);
    return 0;
}

#define ASK   0
#define ABORT 1
#define FULL  2
#define INC   3

#define TS_DUMP	1
#define TS_KEEP	2
#define TS_NEW	3

static int
RestoreVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, aparentid, aserver, apart, code, vcode, err;
    afs_int32 aoverwrite = ASK;
    afs_int32 acreation = 0, alastupdate = 0;
    int restoreflags, readonly = 0, offline = 0, voltype = RWVOL;
    char prompt;
    char afilename[MAXPATHLEN], avolname[VOLSER_MAXVOLNAME + 1], apartName[10];
    char volname[VOLSER_MAXVOLNAME + 1];
    struct nvldbentry entry;

    prompt = 'n';

    aparentid = 0;
    if (as->parms[4].items) {
	avolid = vsu_GetVolumeID(as->parms[4].items->data, cstruct, &err);
	if (avolid == 0) {
	    if (err)
		PrintError("", err);
	    else
		fprintf(STDERR, "vos: can't find volume '%s'\n",
			as->parms[4].items->data);
	    exit(1);
	}
    } else
	avolid = 0;

    if (as->parms[5].items) {
	if ((strcmp(as->parms[5].items->data, "a") == 0)
	    || (strcmp(as->parms[5].items->data, "abort") == 0)) {
	    aoverwrite = ABORT;
	} else if ((strcmp(as->parms[5].items->data, "f") == 0)
		   || (strcmp(as->parms[5].items->data, "full") == 0)) {
	    aoverwrite = FULL;
	} else if ((strcmp(as->parms[5].items->data, "i") == 0)
		   || (strcmp(as->parms[5].items->data, "inc") == 0)
		   || (strcmp(as->parms[5].items->data, "increment") == 0)
		   || (strcmp(as->parms[5].items->data, "incremental") == 0)) {
	    aoverwrite = INC;
	} else {
	    fprintf(STDERR, "vos: %s is not a valid argument to -overwrite\n",
		    as->parms[5].items->data);
	    exit(1);
	}
    }
    if (as->parms[6].items)
	offline = 1;
    if (as->parms[7].items) {
	readonly = 1;
	voltype = ROVOL;
    }

    if (as->parms[8].items) {
	if ((strcmp(as->parms[8].items->data, "d") == 0)
	    || (strcmp(as->parms[8].items->data, "dump") == 0)) {
	    acreation = TS_DUMP;
	} else if ((strcmp(as->parms[8].items->data, "k") == 0)
	    || (strcmp(as->parms[8].items->data, "keep") == 0)) {
	    acreation = TS_KEEP;
	} else if ((strcmp(as->parms[8].items->data, "n") == 0)
	    || (strcmp(as->parms[8].items->data, "new") == 0)) {
	    acreation = TS_NEW;
	} else {
	    fprintf(STDERR, "vos: %s is not a valid argument to -creation\n",
		    as->parms[8].items->data);
	    exit(1);
	}
    }

    if (as->parms[9].items) {
	if ((strcmp(as->parms[9].items->data, "d") == 0)
	    || (strcmp(as->parms[9].items->data, "dump") == 0)) {
	    alastupdate = TS_DUMP;
	} else if ((strcmp(as->parms[9].items->data, "k") == 0)
	    || (strcmp(as->parms[9].items->data, "keep") == 0)) {
	    alastupdate = TS_KEEP;
	} else if ((strcmp(as->parms[9].items->data, "n") == 0)
	    || (strcmp(as->parms[9].items->data, "new") == 0)) {
	    alastupdate = TS_NEW;
	} else {
	    fprintf(STDERR, "vos: %s is not a valid argument to -lastupdate\n",
		    as->parms[9].items->data);
	    exit(1);
	}
    }

    aserver = GetServer(as->parms[0].items->data);
    if (aserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    apart = volutil_GetPartitionID(as->parms[1].items->data);
    if (apart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	exit(1);
    }
    if (!IsPartValid(apart, aserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[1].items->data);
	exit(1);
    }
    strcpy(avolname, as->parms[2].items->data);
    if (!ISNAMEVALID(avolname)) {
	fprintf(STDERR,
		"vos: the name of the volume %s exceeds the size limit\n",
		avolname);
	exit(1);
    }
    if (!VolNameOK(avolname)) {
	fprintf(STDERR,
		"Illegal volume name %s, should not end in .readonly or .backup\n",
		avolname);
	exit(1);
    }
    if (as->parms[3].items) {
	strcpy(afilename, as->parms[3].items->data);
	if (!FileExists(afilename)) {
	    fprintf(STDERR, "Can't access file %s\n", afilename);
	    exit(1);
	}
    } else {
	strcpy(afilename, "");
    }

    /* Check if volume exists or not */

    vsu_ExtractName(volname, avolname);
    vcode = VLDB_GetEntryByName(volname, &entry);
    if (vcode) {		/* no volume - do a full restore */
	restoreflags = RV_FULLRST;
	if ((aoverwrite == INC) || (aoverwrite == ABORT))
	    fprintf(STDERR,
		    "Volume does not exist; Will perform a full restore\n");
    }

    else if ((!readonly && Lp_GetRwIndex(&entry) == -1)	/* RW volume does not exist - do a full */
	     ||(readonly && !Lp_ROMatch(0, 0, &entry))) {	/* RO volume does not exist - do a full */
	restoreflags = RV_FULLRST;
	if ((aoverwrite == INC) || (aoverwrite == ABORT))
	    fprintf(STDERR,
		    "%s Volume does not exist; Will perform a full restore\n",
		    readonly ? "RO" : "RW");

	if (avolid == 0) {
	    avolid = entry.volumeId[voltype];
	} else if (entry.volumeId[voltype] != 0
		   && entry.volumeId[voltype] != avolid) {
	    avolid = entry.volumeId[voltype];
	}
        aparentid = entry.volumeId[RWVOL];
    }

    else {			/* volume exists - do we do a full incremental or abort */
	int Oserver, Opart, Otype, vol_elsewhere = 0;
	struct nvldbentry Oentry;
	int c, dc;

	if (avolid == 0) {
	    avolid = entry.volumeId[voltype];
	} else if (entry.volumeId[voltype] != 0
		   && entry.volumeId[voltype] != avolid) {
	    avolid = entry.volumeId[voltype];
	}
        aparentid = entry.volumeId[RWVOL];

	/* A file name was specified  - check if volume is on another partition */
	vcode = GetVolumeInfo(avolid, &Oserver, &Opart, &Otype, &Oentry);
	if (vcode)
	    exit(1);

	vcode = VLDB_IsSameAddrs(Oserver, aserver, &err);
	if (err) {
	    fprintf(STDERR,
		    "Failed to get info about server's %d address(es) from vlserver (err=%d); aborting call!\n",
		    Oserver, err);
	    exit(1);
	}
	if (!vcode || (Opart != apart))
	    vol_elsewhere = 1;

	if (aoverwrite == ASK) {
	    if (strcmp(afilename, "") == 0) {	/* The file is from standard in */
		fprintf(STDERR,
			"Volume exists and no -overwrite option specified; Aborting restore command\n");
		exit(1);
	    }

	    /* Ask what to do */
	    if (vol_elsewhere) {
		fprintf(STDERR,
			"The volume %s %u already exists on a different server/part\n",
			volname, entry.volumeId[voltype]);
		fprintf(STDERR,
			"Do you want to do a full restore or abort? [fa](a): ");
	    } else {
		fprintf(STDERR,
			"The volume %s %u already exists in the VLDB\n",
			volname, entry.volumeId[voltype]);
		fprintf(STDERR,
			"Do you want to do a full/incremental restore or abort? [fia](a): ");
	    }
	    dc = c = getchar();
	    while (!(dc == EOF || dc == '\n'))
		dc = getchar();	/* goto end of line */
	    if ((c == 'f') || (c == 'F'))
		aoverwrite = FULL;
	    else if ((c == 'i') || (c == 'I'))
		aoverwrite = INC;
	    else
		aoverwrite = ABORT;
	}

	if (aoverwrite == ABORT) {
	    fprintf(STDERR, "Volume exists; Aborting restore command\n");
	    exit(1);
	} else if (aoverwrite == FULL) {
	    restoreflags = RV_FULLRST;
	    fprintf(STDERR,
		    "Volume exists; Will delete and perform full restore\n");
	} else if (aoverwrite == INC) {
	    restoreflags = 0;
	    if (vol_elsewhere) {
		fprintf(STDERR,
			"%s volume %lu already exists on a different server/part; not allowed\n",
			readonly ? "RO" : "RW", (unsigned long)avolid);
		exit(1);
	    }
	}
    }
    if (offline)
	restoreflags |= RV_OFFLINE;
    if (readonly)
	restoreflags |= RV_RDONLY;

    switch (acreation) {
	case TS_DUMP:
	    restoreflags |= RV_CRDUMP;
	    break;
	case TS_KEEP:
	    restoreflags |= RV_CRKEEP;
	    break;
	case TS_NEW:
	    restoreflags |= RV_CRNEW;
	    break;
	default:
	    if (aoverwrite == FULL)
		restoreflags |= RV_CRNEW;
	    else
		restoreflags |= RV_CRKEEP;
    }

    switch (alastupdate) {
	case TS_DUMP:
	    restoreflags |= RV_LUDUMP;
	    break;
	case TS_KEEP:
	    restoreflags |= RV_LUKEEP;
	    break;
	case TS_NEW:
	    restoreflags |= RV_LUNEW;
	    break;
	default:
	    restoreflags |= RV_LUDUMP;
    }
    if (as->parms[10].items) {
	restoreflags |= RV_NODEL;
    }
    

    code =
	UV_RestoreVolume2(aserver, apart, avolid, aparentid,
                          avolname, restoreflags, WriteData, afilename);
    if (code) {
	PrintDiagnostics("restore", code);
	exit(1);
    }
    MapPartIdIntoName(apart, apartName);

    /*
     * patch typo here - originally "parms[1]", should be "parms[0]"
     */

    fprintf(STDOUT, "Restored volume %s on %s %s\n", avolname,
	    as->parms[0].items->data, apartName);
    return 0;
}

static int
LockReleaseCmd(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, code, err;

    avolid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[0].items->data);
	exit(1);
    }

    code = UV_LockRelease(avolid);
    if (code) {
	PrintDiagnostics("unlock", code);
	exit(1);
    }
    fprintf(STDOUT, "Released lock on vldb entry for volume %s\n",
	    as->parms[0].items->data);
    return 0;
}

static int
AddSite(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, aserver, apart, code, err, valid = 0;
    char apartName[10], avolname[VOLSER_MAXVOLNAME + 1];

    vsu_ExtractName(avolname, as->parms[2].items->data);;
    avolid = vsu_GetVolumeID(avolname, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[2].items->data);
	exit(1);
    }
    aserver = GetServer(as->parms[0].items->data);
    if (aserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    apart = volutil_GetPartitionID(as->parms[1].items->data);
    if (apart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	exit(1);
    }
    if (!IsPartValid(apart, aserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[1].items->data);
	exit(1);
    }
    if (as->parms[3].items) {
	valid = 1;
    }
    code = UV_AddSite(aserver, apart, avolid, valid);
    if (code) {
	PrintDiagnostics("addsite", code);
	exit(1);
    }
    MapPartIdIntoName(apart, apartName);
    fprintf(STDOUT, "Added replication site %s %s for volume %s\n",
	    as->parms[0].items->data, apartName, as->parms[2].items->data);
    return 0;
}

static int
RemoveSite(register struct cmd_syndesc *as, void *arock)
{

    afs_int32 avolid, aserver, apart, code, err;
    char apartName[10], avolname[VOLSER_MAXVOLNAME + 1];

    vsu_ExtractName(avolname, as->parms[2].items->data);
    avolid = vsu_GetVolumeID(avolname, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[2].items->data);
	exit(1);
    }
    aserver = GetServer(as->parms[0].items->data);
    if (aserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    apart = volutil_GetPartitionID(as->parms[1].items->data);
    if (apart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	exit(1);
    }
/*
 *skip the partition validity check, since it is possible that the partition
 *has since been decomissioned.
 */
/*
	if (!IsPartValid(apart,aserver,&code)){
	    if(code) PrintError("",code);
	    else fprintf(STDERR,"vos : partition %s does not exist on the server\n",as->parms[1].items->data);
	    exit(1);
	}
*/
    code = UV_RemoveSite(aserver, apart, avolid);
    if (code) {
	PrintDiagnostics("remsite", code);
	exit(1);
    }
    MapPartIdIntoName(apart, apartName);
    fprintf(STDOUT, "Removed replication site %s %s for volume %s\n",
	    as->parms[0].items->data, apartName, as->parms[2].items->data);
    return 0;
}

static int
ChangeLocation(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, aserver, apart, code, err;
    char apartName[10];

    avolid = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[2].items->data);
	exit(1);
    }
    aserver = GetServer(as->parms[0].items->data);
    if (aserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    apart = volutil_GetPartitionID(as->parms[1].items->data);
    if (apart < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	exit(1);
    }
    if (!IsPartValid(apart, aserver, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[1].items->data);
	exit(1);
    }
    code = UV_ChangeLocation(aserver, apart, avolid);
    if (code) {
	PrintDiagnostics("addsite", code);
	exit(1);
    }
    MapPartIdIntoName(apart, apartName);
    fprintf(STDOUT, "Changed location to %s %s for volume %s\n",
	    as->parms[0].items->data, apartName, as->parms[2].items->data);
    return 0;
}

static int
ListPartitions(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 aserver, code;
    struct partList dummyPartList;
    int i;
    char pname[10];
    int total, cnt;

    aserver = GetServer(as->parms[0].items->data);
    if (aserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }


    code = UV_ListPartitions(aserver, &dummyPartList, &cnt);
    if (code) {
	PrintDiagnostics("listpart", code);
	exit(1);
    }
    total = 0;
    fprintf(STDOUT, "The partitions on the server are:\n");
    for (i = 0; i < cnt; i++) {
	if (dummyPartList.partFlags[i] & PARTVALID) {
	    memset(pname, 0, sizeof(pname));
	    MapPartIdIntoName(dummyPartList.partId[i], pname);
	    fprintf(STDOUT, " %10s ", pname);
	    total++;
	    if ((i % 5) == 0 && (i != 0))
		fprintf(STDOUT, "\n");
	}
    }
    fprintf(STDOUT, "\n");
    fprintf(STDOUT, "Total: %d\n", total);
    return 0;

}

static int
CompareVolName(p1, p2)
     char *p1, *p2;
{
    volintInfo *arg1, *arg2;

    arg1 = (volintInfo *) p1;
    arg2 = (volintInfo *) p2;
    return (strcmp(arg1->name, arg2->name));

}

/*------------------------------------------------------------------------
 * PRIVATE XCompareVolName
 *
 * Description:
 *	Comparison routine for volume names coming from an extended
 *	volume listing.
 *
 * Arguments:
 *	a_obj1P : Char ptr to first extended vol info object
 *	a_obj1P : Char ptr to second extended vol info object
 *
 * Returns:
 *	The value of strcmp() on the volume names within the passed
 *	objects (i,e., -1, 0, or 1).
 *
 * Environment:
 *	Passed to qsort() as the designated comparison routine.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
XCompareVolName(a_obj1P, a_obj2P)
     char *a_obj1P, *a_obj2P;

{				/*XCompareVolName */

    return (strcmp
	    (((struct volintXInfo *)(a_obj1P))->name,
	     ((struct volintXInfo *)(a_obj2P))->name));

}				/*XCompareVolName */

static int
CompareVolID(p1, p2)
     char *p1, *p2;
{
    volintInfo *arg1, *arg2;

    arg1 = (volintInfo *) p1;
    arg2 = (volintInfo *) p2;
    if (arg1->volid == arg2->volid)
	return 0;
    if (arg1->volid > arg2->volid)
	return 1;
    else
	return -1;

}

/*------------------------------------------------------------------------
 * PRIVATE XCompareVolID
 *
 * Description:
 *	Comparison routine for volume IDs coming from an extended
 *	volume listing.
 *
 * Arguments:
 *	a_obj1P : Char ptr to first extended vol info object
 *	a_obj1P : Char ptr to second extended vol info object
 *
 * Returns:
 *	The value of strcmp() on the volume names within the passed
 *	objects (i,e., -1, 0, or 1).
 *
 * Environment:
 *	Passed to qsort() as the designated comparison routine.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
XCompareVolID(a_obj1P, a_obj2P)
     char *a_obj1P, *a_obj2P;

{				/*XCompareVolID */

    afs_int32 id1, id2;		/*Volume IDs we're comparing */

    id1 = ((struct volintXInfo *)(a_obj1P))->volid;
    id2 = ((struct volintXInfo *)(a_obj2P))->volid;
    if (id1 == id2)
	return (0);
    else if (id1 > id2)
	return (1);
    else
	return (-1);

}				/*XCompareVolID */

/*------------------------------------------------------------------------
 * PRIVATE ListVolumes
 *
 * Description:
 *	Routine used to list volumes, contacting the Volume Server
 *	directly, bypassing the VLDB.
 *
 * Arguments:
 *	as : Ptr to parsed command line arguments.
 *
 * Returns:
 *	0			Successful operation
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static
ListVolumes(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 apart, int32list, fast;
    afs_int32 aserver, code;
    volintInfo *pntr, *oldpntr;
    afs_int32 count;
    int i;
    char *base;
    volintXInfo *xInfoP, *origxInfoP;	/*Ptr to current/orig extended vol info */
    int wantExtendedInfo;	/*Do we want extended vol info? */

    char pname[10];
    struct partList dummyPartList;
    int all;
    int quiet, cnt;

    apart = -1;
    fast = 0;
    int32list = 0;

    if (as->parms[3].items)
	int32list = 1;
    if (as->parms[4].items)
	quiet = 1;
    else
	quiet = 0;
    if (as->parms[2].items)
	fast = 1;
    if (fast)
	all = 0;
    else
	all = 1;
    if (as->parms[5].items) {
	/*
	 * We can't coexist with the fast flag.
	 */
	if (fast) {
	    fprintf(STDERR,
		    "vos: Can't use the -fast and -extended flags together\n");
	    exit(1);
	}

	/*
	 * We need to turn on ``long'' listings to get the full effect.
	 */
	wantExtendedInfo = 1;
	int32list = 1;
    } else
	wantExtendedInfo = 0;
    if (as->parms[1].items) {
	apart = volutil_GetPartitionID(as->parms[1].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[1].items->data);
	    exit(1);
	}
	dummyPartList.partId[0] = apart;
	dummyPartList.partFlags[0] = PARTVALID;
	cnt = 1;
    }
    aserver = GetServer(as->parms[0].items->data);
    if (aserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }

    if (apart != -1) {
	if (!IsPartValid(apart, aserver, &code)) {	/*check for validity of the partition */
	    if (code)
		PrintError("", code);
	    else
		fprintf(STDERR,
			"vos : partition %s does not exist on the server\n",
			as->parms[1].items->data);
	    exit(1);
	}
    } else {
	code = UV_ListPartitions(aserver, &dummyPartList, &cnt);
	if (code) {
	    PrintDiagnostics("listvol", code);
	    exit(1);
	}
    }
    for (i = 0; i < cnt; i++) {
	if (dummyPartList.partFlags[i] & PARTVALID) {
	    if (wantExtendedInfo)
		code =
		    UV_XListVolumes(aserver, dummyPartList.partId[i], all,
				    &xInfoP, &count);
	    else
		code =
		    UV_ListVolumes(aserver, dummyPartList.partId[i], all,
				   &pntr, &count);
	    if (code) {
		PrintDiagnostics("listvol", code);
		if (pntr)
		    free(pntr);
		exit(1);
	    }
	    if (wantExtendedInfo) {
		origxInfoP = xInfoP;
		base = (char *)xInfoP;
	    } else {
		oldpntr = pntr;
		base = (char *)pntr;
	    }

	    if (!fast) {
		if (wantExtendedInfo)
		    qsort(base, count, sizeof(volintXInfo), XCompareVolName);
		else
		    qsort(base, count, sizeof(volintInfo), CompareVolName);
	    } else {
		if (wantExtendedInfo)
		    qsort(base, count, sizeof(volintXInfo), XCompareVolID);
		else
		    qsort(base, count, sizeof(volintInfo), CompareVolID);
	    }
	    MapPartIdIntoName(dummyPartList.partId[i], pname);
	    if (!quiet)
		fprintf(STDOUT,
			"Total number of volumes on server %s partition %s: %lu \n",
			as->parms[0].items->data, pname,
			(unsigned long)count);
	    if (wantExtendedInfo) {
#ifdef FULL_LISTVOL_SWITCH
		if (as->parms[6].items)
		    XDisplayVolumes2(aserver, dummyPartList.partId[i], origxInfoP,
				count, int32list, fast, quiet);
		else
#endif /* FULL_LISTVOL_SWITCH */
		XDisplayVolumes(aserver, dummyPartList.partId[i], origxInfoP,
				count, int32list, fast, quiet);
		if (xInfoP)
		    free(xInfoP);
		xInfoP = (volintXInfo *) 0;
	    } else {
#ifdef FULL_LISTVOL_SWITCH
		if (as->parms[6].items)
		    DisplayVolumes2(aserver, dummyPartList.partId[i], oldpntr,
				    count);
		else
#endif /* FULL_LISTVOL_SWITCH */
		    DisplayVolumes(aserver, dummyPartList.partId[i], oldpntr,
				   count, int32list, fast, quiet);
		if (pntr)
		    free(pntr);
		pntr = (volintInfo *) 0;
	    }
	}
    }
    return 0;
}

static int
SyncVldb(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 pnum = 0, code;	/* part name */
    char part[10];
    int flags = 0;
    char *volname = 0;

    tserver = 0;
    if (as->parms[0].items) {
	tserver = GetServer(as->parms[0].items->data);
	if (!tserver) {
	    fprintf(STDERR, "vos: host '%s' not found in host table\n",
		    as->parms[0].items->data);
	    exit(1);
	}
    }

    if (as->parms[1].items) {
	pnum = volutil_GetPartitionID(as->parms[1].items->data);
	if (pnum < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[1].items->data);
	    exit(1);
	}
	if (!IsPartValid(pnum, tserver, &code)) {	/*check for validity of the partition */
	    if (code)
		PrintError("", code);
	    else
		fprintf(STDERR,
			"vos: partition %s does not exist on the server\n",
			as->parms[1].items->data);
	    exit(1);
	}
	flags = 1;

	if (!tserver) {
	    fprintf(STDERR,
		    "The -partition option requires a -server option\n");
	    exit(1);
	}
    }

    if (as->parms[2].items) {
	/* Synchronize an individual volume */
	volname = as->parms[2].items->data;
	code = UV_SyncVolume(tserver, pnum, volname, flags);
    } else {
	if (!tserver) {
	    fprintf(STDERR,
		    "Without a -volume option, the -server option is required\n");
	    exit(1);
	}
	code = UV_SyncVldb(tserver, pnum, flags, 0 /*unused */ );
    }

    if (code) {
	PrintDiagnostics("syncvldb", code);
	exit(1);
    }

    /* Print a summary of what we did */
    if (volname)
	fprintf(STDOUT, "VLDB volume %s synchronized", volname);
    else
	fprintf(STDOUT, "VLDB synchronized");
    if (tserver) {
	fprintf(STDOUT, " with state of server %s", as->parms[0].items->data);
    }
    if (flags) {
	MapPartIdIntoName(pnum, part);
	fprintf(STDOUT, " partition %s\n", part);
    }
    fprintf(STDOUT, "\n");

    return 0;
}

static int
SyncServer(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 pnum, code;	/* part name */
    char part[10];

    int flags = 0;

    tserver = GetServer(as->parms[0].items->data);
    if (!tserver) {
	fprintf(STDERR, "vos: host '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    if (as->parms[1].items) {
	pnum = volutil_GetPartitionID(as->parms[1].items->data);
	if (pnum < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[1].items->data);
	    exit(1);
	}
	if (!IsPartValid(pnum, tserver, &code)) {	/*check for validity of the partition */
	    if (code)
		PrintError("", code);
	    else
		fprintf(STDERR,
			"vos : partition %s does not exist on the server\n",
			as->parms[1].items->data);
	    exit(1);
	}
	flags = 1;
    } else {
        pnum = -1;
    }

    code = UV_SyncServer(tserver, pnum, flags, 0 /*unused */ );
    if (code) {
	PrintDiagnostics("syncserv", code);
	exit(1);
    }
    if (flags) {
	MapPartIdIntoName(pnum, part);
	fprintf(STDOUT, "Server %s partition %s synchronized with VLDB\n",
		as->parms[0].items->data, part);
    } else
	fprintf(STDOUT, "Server %s synchronized with VLDB\n",
		as->parms[0].items->data);
    return 0;

}

static
VolumeInfoCmd(name)
     char *name;
{
    struct nvldbentry entry;
    afs_int32 vcode;

    /* The vlserver will handle names with the .readonly
     * and .backup extension as well as volume ids.
     */
    vcode = VLDB_GetEntryByName(name, &entry);
    if (vcode) {
	PrintError("", vcode);
	exit(1);
    }
    MapHostToNetwork(&entry);
    EnumerateEntry(&entry);

    /* Defect #3027: grubby check to handle locked volume.
     * If VLOP_ALLOPERS is set, the entry is locked.
     * Leave this routine as is, but put in correct check.
     */
    if (entry.flags & VLOP_ALLOPERS)
	fprintf(STDOUT, "    Volume is currently LOCKED  \n");

    return 0;
}

static int
VolumeZap(register struct cmd_syndesc *as, void *arock)
{
    struct nvldbentry entry;
    afs_int32 volid, code, server, part, zapbackupid = 0, backupid = 0, err;

    if (as->parms[3].items) {
	/* force flag is on, use the other version */
	return NukeVolume(as);
    }

    if (as->parms[4].items) {
	zapbackupid = 1;
    }

    volid = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &err);
    if (volid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[2].items->data);
	exit(1);
    }
    part = volutil_GetPartitionID(as->parms[1].items->data);
    if (part < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	exit(1);
    }
    server = GetServer(as->parms[0].items->data);
    if (!server) {
	fprintf(STDERR, "vos: host '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    if (!IsPartValid(part, server, &code)) {	/*check for validity of the partition */
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[1].items->data);
	exit(1);
    }
    code = VLDB_GetEntryByID(volid, -1, &entry);
    if (!code) {
	if (volid == entry.volumeId[RWVOL])
	    backupid = entry.volumeId[BACKVOL];
	fprintf(STDERR,
		"Warning: Entry for volume number %lu exists in VLDB (but we're zapping it anyway!)\n",
		(unsigned long)volid);
    }
    if (zapbackupid) {
	volintInfo *pntr = (volintInfo *) 0;

	if (!backupid) {
	    code = UV_ListOneVolume(server, part, volid, &pntr);
	    if (!code) {
		if (volid == pntr->parentID)
		    backupid = pntr->backupID;
		if (pntr)
		    free(pntr);
	    }
	}
	if (backupid) {
	    code = UV_VolumeZap(server, part, backupid);
	    if (code) {
		PrintDiagnostics("zap", code);
		exit(1);
	    }
	    fprintf(STDOUT, "Backup Volume %lu deleted\n",
		    (unsigned long)backupid);
	}
    }
    code = UV_VolumeZap(server, part, volid);
    if (code) {
	PrintDiagnostics("zap", code);
	exit(1);
    }
    fprintf(STDOUT, "Volume %lu deleted\n", (unsigned long)volid);

    return 0;
}

static int
VolserStatus(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 server, code;
    transDebugInfo *pntr, *oldpntr;
    afs_int32 count;
    int i;
    char pname[10];

    server = GetServer(as->parms[0].items->data);
    if (!server) {
	fprintf(STDERR, "vos: host '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    code = UV_VolserStatus(server, &pntr, &count);
    if (code) {
	PrintDiagnostics("status", code);
	exit(1);
    }
    oldpntr = pntr;
    if (count == 0)
	fprintf(STDOUT, "No active transactions on %s\n",
		as->parms[0].items->data);
    else {
	fprintf(STDOUT, "Total transactions: %d\n", count);
    }
    for (i = 0; i < count; i++) {
	/*print out the relevant info */
	fprintf(STDOUT, "--------------------------------------\n");
	fprintf(STDOUT, "transaction: %lu  created: %s",
		(unsigned long)pntr->tid, vos_ctime( & pntr->time));
	if (pntr->returnCode) {
	    fprintf(STDOUT, "returnCode: %lu\n",
		    (unsigned long)pntr->returnCode);
	}
	if (pntr->iflags) {
	    fprintf(STDOUT, "attachFlags:  ");
	    switch (pntr->iflags) {
	    case ITOffline:
		fprintf(STDOUT, "offline ");
		break;
	    case ITBusy:
		fprintf(STDOUT, "busy ");
		break;
	    case ITReadOnly:
		fprintf(STDOUT, "readonly ");
		break;
	    case ITCreate:
		fprintf(STDOUT, "create ");
		break;
	    case ITCreateVolID:
		fprintf(STDOUT, "create volid ");
		break;
	    }
	    fprintf(STDOUT, "\n");
	}
	if (pntr->vflags) {
	    fprintf(STDOUT, "volumeStatus: ");
	    switch (pntr->vflags) {
	    case VTDeleteOnSalvage:
		fprintf(STDOUT, "deleteOnSalvage ");
	    case VTOutOfService:
		fprintf(STDOUT, "outOfService ");
	    case VTDeleted:
		fprintf(STDOUT, "deleted ");
	    }
	    fprintf(STDOUT, "\n");
	}
	if (pntr->tflags) {
	    fprintf(STDOUT, "transactionFlags: ");
	    fprintf(STDOUT, "delete\n");
	}
	MapPartIdIntoName(pntr->partition, pname);
	fprintf(STDOUT, "volume: %lu  partition: %s  procedure: %s\n",
		(unsigned long)pntr->volid, pname, pntr->lastProcName);
	if (pntr->callValid) {
	    fprintf(STDOUT,
		    "packetRead: %lu  lastReceiveTime: %d  packetSend: %lu  lastSendTime: %d\n",
		    (unsigned long)pntr->readNext, pntr->lastReceiveTime,
		    (unsigned long)pntr->transmitNext, pntr->lastSendTime);
	}
	pntr++;
	fprintf(STDOUT, "--------------------------------------\n");
	fprintf(STDOUT, "\n");
    }
    if (oldpntr)
	free(oldpntr);
    return 0;
}

static int
RenameVolume(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 code1, code2, code;
    struct nvldbentry entry;

    code1 = VLDB_GetEntryByName(as->parms[0].items->data, &entry);
    if (code1) {
	fprintf(STDERR, "vos: Could not find entry for volume %s\n",
		as->parms[0].items->data);
	exit(1);
    }
    code2 = VLDB_GetEntryByName(as->parms[1].items->data, &entry);
    if ((!code1) && (!code2)) {	/*the newname already exists */
	fprintf(STDERR, "vos: volume %s already exists\n",
		as->parms[1].items->data);
	exit(1);
    }

    if (code1 && code2) {
	fprintf(STDERR, "vos: Could not find entry for volume %s or %s\n",
		as->parms[0].items->data, as->parms[1].items->data);
	exit(1);
    }
    if (!VolNameOK(as->parms[0].items->data)) {
	fprintf(STDERR,
		"Illegal volume name %s, should not end in .readonly or .backup\n",
		as->parms[0].items->data);
	exit(1);
    }
    if (!ISNAMEVALID(as->parms[1].items->data)) {
	fprintf(STDERR,
		"vos: the new volume name %s exceeds the size limit of %d\n",
		as->parms[1].items->data, VOLSER_OLDMAXVOLNAME - 10);
	exit(1);
    }
    if (!VolNameOK(as->parms[1].items->data)) {
	fprintf(STDERR,
		"Illegal volume name %s, should not end in .readonly or .backup\n",
		as->parms[1].items->data);
	exit(1);
    }
    if (IsNumeric(as->parms[1].items->data)) {
	fprintf(STDERR, "Illegal volume name %s, should not be a number\n",
		as->parms[1].items->data);
	exit(1);
    }
    MapHostToNetwork(&entry);
    code =
	UV_RenameVolume(&entry, as->parms[0].items->data,
			as->parms[1].items->data);
    if (code) {
	PrintDiagnostics("rename", code);
	exit(1);
    }
    fprintf(STDOUT, "Renamed volume %s to %s\n", as->parms[0].items->data,
	    as->parms[1].items->data);
    return 0;
}

GetVolumeInfo(volid, server, part, voltype, rentry)
     afs_int32 *server, volid, *part, *voltype;
     register struct nvldbentry *rentry;
{
    afs_int32 vcode;
    int i, index = -1;

    vcode = VLDB_GetEntryByID(volid, -1, rentry);
    if (vcode) {
	fprintf(STDERR,
		"Could not fetch the entry for volume %lu from VLDB \n",
		(unsigned long)volid);
	PrintError("", vcode);
	return (vcode);
    }
    MapHostToNetwork(rentry);
    if (volid == rentry->volumeId[ROVOL]) {
	*voltype = ROVOL;
	for (i = 0; i < rentry->nServers; i++) {
	    if ((index == -1) && (rentry->serverFlags[i] & ITSROVOL)
		&& !(rentry->serverFlags[i] & RO_DONTUSE))
		index = i;
	}
	if (index == -1) {
	    fprintf(STDERR,
		    "RO volume is not found in VLDB entry for volume %lu\n",
		    (unsigned long)volid);
	    return -1;
	}

	*server = rentry->serverNumber[index];
	*part = rentry->serverPartition[index];
	return 0;
    }

    index = Lp_GetRwIndex(rentry);
    if (index == -1) {
	fprintf(STDERR,
		"RW Volume is not found in VLDB entry for volume %lu\n",
		(unsigned long)volid);
	return -1;
    }
    if (volid == rentry->volumeId[RWVOL]) {
	*voltype = RWVOL;
	*server = rentry->serverNumber[index];
	*part = rentry->serverPartition[index];
	return 0;
    }
    if (volid == rentry->volumeId[BACKVOL]) {
	*voltype = BACKVOL;
	*server = rentry->serverNumber[index];
	*part = rentry->serverPartition[index];
	return 0;
    }
    fprintf(STDERR,
            "unexpected volume type for volume %lu\n",
            (unsigned long)volid);
    return -1;
}

static int
DeleteEntry(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 apart;
    afs_int32 avolid;
    afs_int32 vcode;
    struct VldbListByAttributes attributes;
    nbulkentries arrayEntries;
    register struct nvldbentry *vllist;
    struct cmd_item *itp;
    afs_int32 nentries;
    int j;
    char prefix[VOLSER_MAXVOLNAME + 1];
    int seenprefix = 0;
    afs_int32 totalBack = 0, totalFail = 0, err;

    if (as->parms[0].items) {	/* -id */
	if (as->parms[1].items || as->parms[2].items || as->parms[3].items) {
	    fprintf(STDERR,
		    "You cannot use -server, -partition, or -prefix with the -id argument\n");
	    exit(-2);
	}
	for (itp = as->parms[0].items; itp; itp = itp->next) {
	    avolid = vsu_GetVolumeID(itp->data, cstruct, &err);
	    if (avolid == 0) {
		if (err)
		    PrintError("", err);
		else
		    fprintf(STDERR, "vos: can't find volume '%s'\n",
			    itp->data);
		continue;
	    }
	    if (as->parms[4].items) {	/* -noexecute */
		fprintf(STDOUT, "Would have deleted VLDB entry for %s \n",
			itp->data);
		fflush(STDOUT);
		continue;
	    }
	    vcode = ubik_VL_DeleteEntry(cstruct, 0, avolid, RWVOL);
	    if (vcode) {
		fprintf(STDERR, "Could not delete entry for volume %s\n",
			itp->data);
		fprintf(STDERR,
			"You must specify a RW volume name or ID "
			"(the entire VLDB entry will be deleted)\n");
		PrintError("", vcode);
		totalFail++;
		continue;
	    }
	    totalBack++;
	}
	fprintf(STDOUT, "Deleted %d VLDB entries\n", totalBack);
	return (totalFail);
    }

    if (!as->parms[1].items && !as->parms[2].items && !as->parms[3].items) {
	fprintf(STDERR, "You must specify an option\n");
	exit(-2);
    }

    /* Zero out search attributes */
    memset(&attributes, 0, sizeof(struct VldbListByAttributes));

    if (as->parms[1].items) {	/* -prefix */
	strncpy(prefix, as->parms[1].items->data, VOLSER_MAXVOLNAME);
	seenprefix = 1;
	if (!as->parms[2].items && !as->parms[3].items) {	/* a single entry only */
	    fprintf(STDERR,
		    "You must provide -server with the -prefix argument\n");
	    exit(-2);
	}
    }

    if (as->parms[2].items) {	/* -server */
	afs_int32 aserver;
	aserver = GetServer(as->parms[2].items->data);
	if (aserver == 0) {
	    fprintf(STDERR, "vos: server '%s' not found in host table\n",
		    as->parms[2].items->data);
	    exit(-1);
	}
	attributes.server = ntohl(aserver);
	attributes.Mask |= VLLIST_SERVER;
    }

    if (as->parms[3].items) {	/* -partition */
	if (!as->parms[2].items) {
	    fprintf(STDERR,
		    "You must provide -server with the -partition argument\n");
	    exit(-2);
	}
	apart = volutil_GetPartitionID(as->parms[3].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[3].items->data);
	    exit(-1);
	}
	attributes.partition = apart;
	attributes.Mask |= VLLIST_PARTITION;
    }

    /* Print status line of what we are doing */
    fprintf(STDOUT, "Deleting VLDB entries for ");
    if (as->parms[2].items) {
	fprintf(STDOUT, "server %s ", as->parms[2].items->data);
    }
    if (as->parms[3].items) {
	char pname[10];
	MapPartIdIntoName(apart, pname);
	fprintf(STDOUT, "partition %s ", pname);
    }
    if (seenprefix) {
	fprintf(STDOUT, "which are prefixed with %s ", prefix);
    }
    fprintf(STDOUT, "\n");
    fflush(STDOUT);

    /* Get all the VLDB entries on a server and/or partition */
    memset(&arrayEntries, 0, sizeof(arrayEntries));
    vcode = VLDB_ListAttributes(&attributes, &nentries, &arrayEntries);
    if (vcode) {
	fprintf(STDERR, "Could not access the VLDB for attributes\n");
	PrintError("", vcode);
	exit(-1);
    }

    /* Process each entry */
    for (j = 0; j < nentries; j++) {
	vllist = &arrayEntries.nbulkentries_val[j];
	if (seenprefix) {
	    /* It only deletes the RW volumes */
	    if (strncmp(vllist->name, prefix, strlen(prefix))) {
		if (verbose) {
		    fprintf(STDOUT,
			    "Omitting to delete %s due to prefix %s mismatch\n",
			    vllist->name, prefix);
		}
		fflush(STDOUT);
		continue;
	    }
	}

	if (as->parms[4].items) {	/* -noexecute */
	    fprintf(STDOUT, "Would have deleted VLDB entry for %s \n",
		    vllist->name);
	    fflush(STDOUT);
	    continue;
	}

	/* Only matches the RW volume name */
	avolid = vllist->volumeId[RWVOL];
	vcode = ubik_VL_DeleteEntry(cstruct, 0, avolid, RWVOL);
	if (vcode) {
	    fprintf(STDOUT, "Could not delete VDLB entry for  %s\n",
		    vllist->name);
	    totalFail++;
	    PrintError("", vcode);
	    continue;
	} else {
	    totalBack++;
	    if (verbose)
		fprintf(STDOUT, "Deleted VLDB entry for %s \n", vllist->name);
	}
	fflush(STDOUT);
    }				/*for */

    fprintf(STDOUT, "----------------------\n");
    fprintf(STDOUT,
	    "Total VLDB entries deleted: %lu; failed to delete: %lu\n",
	    (unsigned long)totalBack, (unsigned long)totalFail);
    if (arrayEntries.nbulkentries_val)
	free(arrayEntries.nbulkentries_val);
    return 0;
}


static int
CompareVldbEntryByName(p1, p2)
     char *p1, *p2;
{
    struct nvldbentry *arg1, *arg2;

    arg1 = (struct nvldbentry *)p1;
    arg2 = (struct nvldbentry *)p2;
    return (strcmp(arg1->name, arg2->name));
}

/*
static int CompareVldbEntry(p1,p2)
char *p1,*p2;
{
    struct nvldbentry *arg1,*arg2;
    int i;
    int pos1, pos2;
    char comp1[100],comp2[100];
    char temp1[20],temp2[20];

    arg1 = (struct nvldbentry *)p1;
    arg2 = (struct nvldbentry *)p2;
    pos1 = -1;
    pos2 = -1;

    for(i = 0; i < arg1->nServers; i++)
	if(arg1->serverFlags[i] & ITSRWVOL) pos1 = i;
    for(i = 0; i < arg2->nServers; i++)
	if(arg2->serverFlags[i] & ITSRWVOL) pos2 = i;
    if(pos1 == -1 || pos2 == -1){
	pos1 = 0;
	pos2 = 0;
    }
    sprintf(comp1,"%10u",arg1->serverNumber[pos1]);
    sprintf(comp2,"%10u",arg2->serverNumber[pos2]);
    sprintf(temp1,"%10u",arg1->serverPartition[pos1]);
    sprintf(temp2,"%10u",arg2->serverPartition[pos2]);
    strcat(comp1,temp1);
    strcat(comp2,temp2);
    strcat(comp1,arg1->name);
    strcat(comp1,arg2->name);
    return(strcmp(comp1,comp2));

}

*/
static int
ListVLDB(struct cmd_syndesc *as, void *arock)
{
    afs_int32 apart;
    afs_int32 aserver, code;
    afs_int32 vcode;
    struct VldbListByAttributes attributes;
    nbulkentries arrayEntries;
    struct nvldbentry *vllist, *tarray = 0, *ttarray;
    afs_int32 centries, nentries = 0, tarraysize, parraysize;
    int j;
    char pname[10];
    int quiet, sort, lock;
    afs_int32 thisindex, nextindex;

    aserver = 0;
    apart = 0;

    attributes.Mask = 0;
    lock = (as->parms[3].items ? 1 : 0);	/* -lock   flag */
    quiet = (as->parms[4].items ? 1 : 0);	/* -quit   flag */
    sort = (as->parms[5].items ? 0 : 1);	/* -nosort flag */

    /* If the volume name is given, Use VolumeInfoCmd to look it up
     * and not ListAttributes.
     */
    if (as->parms[0].items) {
	if (lock) {
	    fprintf(STDERR,
		    "vos: illegal use of '-locked' switch, need to specify server and/or partition\n");
	    exit(1);
	}
	code = VolumeInfoCmd(as->parms[0].items->data);
	if (code) {
	    PrintError("", code);
	    exit(1);
	}
	return 0;
    }

    /* Server specified */
    if (as->parms[1].items) {
	aserver = GetServer(as->parms[1].items->data);
	if (aserver == 0) {
	    fprintf(STDERR, "vos: server '%s' not found in host table\n",
		    as->parms[1].items->data);
	    exit(1);
	}
	attributes.server = ntohl(aserver);
	attributes.Mask |= VLLIST_SERVER;
    }

    /* Partition specified */
    if (as->parms[2].items) {
	apart = volutil_GetPartitionID(as->parms[2].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[2].items->data);
	    exit(1);
	}
	attributes.partition = apart;
	attributes.Mask |= VLLIST_PARTITION;
    }

    if (lock) {
	attributes.Mask |= VLLIST_FLAG;
	attributes.flag = VLOP_ALLOPERS;
    }

    /* Print header information */
    if (!quiet) {
	MapPartIdIntoName(apart, pname);
	fprintf(STDOUT, "VLDB entries for %s %s%s%s %s\n",
		(as->parms[1].items ? "server" : "all"),
		(as->parms[1].items ? as->parms[1].items->data : "servers"),
		(as->parms[2].items ? " partition " : ""),
		(as->parms[2].items ? pname : ""),
		(lock ? "which are locked:" : ""));
    }

    for (thisindex = 0; (thisindex != -1); thisindex = nextindex) {
	memset(&arrayEntries, 0, sizeof(arrayEntries));
	centries = 0;
	nextindex = -1;

	vcode =
	    VLDB_ListAttributesN2(&attributes, 0, thisindex, &centries,
				  &arrayEntries, &nextindex);
	if (vcode == RXGEN_OPCODE) {
	    /* Vlserver not running with ListAttributesN2. Fall back */
	    vcode =
		VLDB_ListAttributes(&attributes, &centries, &arrayEntries);
	    nextindex = -1;
	}
	if (vcode) {
	    fprintf(STDERR, "Could not access the VLDB for attributes\n");
	    PrintError("", vcode);
	    exit(1);
	}
	nentries += centries;

	/* We don't sort, so just print the entries now */
	if (!sort) {
	    for (j = 0; j < centries; j++) {	/* process each entry */
		vllist = &arrayEntries.nbulkentries_val[j];
		MapHostToNetwork(vllist);
		EnumerateEntry(vllist);

		if (vllist->flags & VLOP_ALLOPERS)
		    fprintf(STDOUT, "    Volume is currently LOCKED  \n");
	    }
	}

	/* So we sort. First we must collect all the entries and keep
	 * them in memory.
	 */
	else if (centries > 0) {
	    if (!tarray) {
		/* steal away the first bulk entries array */
		tarray = (struct nvldbentry *)arrayEntries.nbulkentries_val;
		tarraysize = centries * sizeof(struct nvldbentry);
		arrayEntries.nbulkentries_val = 0;
	    } else {
		/* Grow the tarray to keep the extra entries */
		parraysize = (centries * sizeof(struct nvldbentry));
		ttarray =
		    (struct nvldbentry *)realloc(tarray,
						 tarraysize + parraysize);
		if (!ttarray) {
		    fprintf(STDERR,
			    "Could not allocate enough space for  the VLDB entries\n");
		    goto bypass;
		}
		tarray = ttarray;

		/* Copy them in */
		memcpy(((char *)tarray) + tarraysize,
		       (char *)arrayEntries.nbulkentries_val, parraysize);
		tarraysize += parraysize;
	    }
	}

	/* Free the bulk array */
	if (arrayEntries.nbulkentries_val) {
	    free(arrayEntries.nbulkentries_val);
	    arrayEntries.nbulkentries_val = 0;
	}
    }

    /* Here is where we now sort all the entries and print them */
    if (sort && (nentries > 0)) {
	qsort((char *)tarray, nentries, sizeof(struct nvldbentry),
	      CompareVldbEntryByName);
	for (vllist = tarray, j = 0; j < nentries; j++, vllist++) {
	    MapHostToNetwork(vllist);
	    EnumerateEntry(vllist);

	    if (vllist->flags & VLOP_ALLOPERS)
		fprintf(STDOUT, "    Volume is currently LOCKED  \n");
	}
    }

  bypass:
    if (!quiet)
	fprintf(STDOUT, "\nTotal entries: %lu\n", (unsigned long)nentries);
    if (tarray)
	free(tarray);
    return 0;
}

static int
BackSys(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 apart = 0, avolid;
    afs_int32 aserver = 0, code, aserver1, apart1;
    afs_int32 vcode;
    struct VldbListByAttributes attributes;
    nbulkentries arrayEntries;
    register struct nvldbentry *vllist;
    afs_int32 nentries;
    int j;
    char pname[10];
    int seenprefix, seenxprefix, exclude, ex, exp, noaction;
    afs_int32 totalBack = 0;
    afs_int32 totalFail = 0;
    int previdx = -1, error, same;
    int comp = 0;
    struct cmd_item *ti;
    char *ccode;
    int match;

    memset(&attributes, 0, sizeof(struct VldbListByAttributes));
    attributes.Mask = 0;

    seenprefix = (as->parms[0].items ? 1 : 0);
    exclude = (as->parms[3].items ? 1 : 0);
    seenxprefix = (as->parms[4].items ? 1 : 0);
    noaction = (as->parms[5].items ? 1 : 0);

    if (as->parms[1].items) {	/* -server */
	aserver = GetServer(as->parms[1].items->data);
	if (aserver == 0) {
	    fprintf(STDERR, "vos: server '%s' not found in host table\n",
		    as->parms[1].items->data);
	    exit(1);
	}
	attributes.server = ntohl(aserver);
	attributes.Mask |= VLLIST_SERVER;
    }

    if (as->parms[2].items) {	/* -partition */
	apart = volutil_GetPartitionID(as->parms[2].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[2].items->data);
	    exit(1);
	}
	attributes.partition = apart;
	attributes.Mask |= VLLIST_PARTITION;
    }

    /* Check to make sure the prefix and xprefix expressions compile ok */
    if (seenprefix) {
	for (ti = as->parms[0].items; ti; ti = ti->next) {
	    if (strncmp(ti->data, "^", 1) == 0) {
#ifdef HAVE_POSIX_REGEX
		regex_t re;
		char errbuf[256];

		code = regcomp(&re, ti->data, REG_NOSUB);
		if (code != 0) {
		    regerror(code, &re, errbuf, sizeof errbuf);
		    fprintf(STDERR,
			    "Unrecognizable -prefix regular expression: '%s': %s\n",
			    ti->data, errbuf);
		    exit(1);
		}
		regfree(&re);
#else
		ccode = (char *)re_comp(ti->data);
		if (ccode) {
		    fprintf(STDERR,
			    "Unrecognizable -prefix regular expression: '%s': %s\n",
			    ti->data, ccode);
		    exit(1);
		}
#endif
	    }
	}
    }
    if (seenxprefix) {
	for (ti = as->parms[4].items; ti; ti = ti->next) {
	    if (strncmp(ti->data, "^", 1) == 0) {
#ifdef HAVE_POSIX_REGEX
		regex_t re;
		char errbuf[256];

		code = regcomp(&re, ti->data, REG_NOSUB);
		if (code != 0) {
		    regerror(code, &re, errbuf, sizeof errbuf);
		    fprintf(STDERR,
			    "Unrecognizable -xprefix regular expression: '%s': %s\n",
			    ti->data, errbuf);
		    exit(1);
		}
		regfree(&re);
#else
		ccode = (char *)re_comp(ti->data);
		if (ccode) {
		    fprintf(STDERR,
			    "Unrecognizable -xprefix regular expression: '%s': %s\n",
			    ti->data, ccode);
		    exit(1);
		}
#endif
	    }
	}
    }

    memset(&arrayEntries, 0, sizeof(arrayEntries));	/* initialize to hint the stub to alloc space */
    vcode = VLDB_ListAttributes(&attributes, &nentries, &arrayEntries);
    if (vcode) {
	fprintf(STDERR, "Could not access the VLDB for attributes\n");
	PrintError("", vcode);
	exit(1);
    }

    if (as->parms[1].items || as->parms[2].items || verbose) {
	fprintf(STDOUT, "%s up volumes",
		(noaction ? "Would have backed" : "Backing"));

	if (as->parms[1].items) {
	    fprintf(STDOUT, " on server %s", as->parms[1].items->data);
	} else if (as->parms[2].items) {
	    fprintf(STDOUT, " for all servers");
	}

	if (as->parms[2].items) {
	    MapPartIdIntoName(apart, pname);
	    fprintf(STDOUT, " partition %s", pname);
	}

	if (seenprefix || (!seenprefix && seenxprefix)) {
	    ti = (seenprefix ? as->parms[0].items : as->parms[4].items);
	    ex = (seenprefix ? exclude : !exclude);
	    exp = (strncmp(ti->data, "^", 1) == 0);
	    fprintf(STDOUT, " which %smatch %s '%s'", (ex ? "do not " : ""),
		    (exp ? "expression" : "prefix"), ti->data);
	    for (ti = ti->next; ti; ti = ti->next) {
		exp = (strncmp(ti->data, "^", 1) == 0);
		printf(" %sor %s '%s'", (ex ? "n" : ""),
		       (exp ? "expression" : "prefix"), ti->data);
	    }
	}

	if (seenprefix && seenxprefix) {
	    ti = as->parms[4].items;
	    exp = (strncmp(ti->data, "^", 1) == 0);
	    fprintf(STDOUT, " %swhich match %s '%s'",
		    (exclude ? "adding those " : "removing those "),
		    (exp ? "expression" : "prefix"), ti->data);
	    for (ti = ti->next; ti; ti = ti->next) {
		exp = (strncmp(ti->data, "^", 1) == 0);
		printf(" or %s '%s'", (exp ? "expression" : "prefix"),
		       ti->data);
	    }
	}
	fprintf(STDOUT, " .. ");
	if (verbose)
	    fprintf(STDOUT, "\n");
	fflush(STDOUT);
    }

    for (j = 0; j < nentries; j++) {	/* process each vldb entry */
	vllist = &arrayEntries.nbulkentries_val[j];

	if (seenprefix) {
	    for (ti = as->parms[0].items; ti; ti = ti->next) {
		if (strncmp(ti->data, "^", 1) == 0) {
#ifdef HAVE_POSIX_REGEX
		    regex_t re;
		    char errbuf[256];

		    /* XXX -- should just do the compile once! */
		    code = regcomp(&re, ti->data, REG_NOSUB);
		    if (code != 0) {
			regerror(code, &re, errbuf, sizeof errbuf);
			fprintf(STDERR,
				"Error in -prefix regular expression: '%s': %s\n",
				ti->data, errbuf);
			exit(1);
		    }
		    match = (regexec(&re, vllist->name, 0, NULL, 0) == 0);
		    regfree(&re);
#else
		    ccode = (char *)re_comp(ti->data);
		    if (ccode) {
			fprintf(STDERR,
				"Error in -prefix regular expression: '%s': %s\n",
				ti->data, ccode);
			exit(1);
		    }
		    match = (re_exec(vllist->name) == 1);
#endif
		} else {
		    match =
			(strncmp(vllist->name, ti->data, strlen(ti->data)) ==
			 0);
		}
		if (match)
		    break;
	    }
	} else {
	    match = 1;
	}

	/* Without the -exclude flag: If it matches the prefix, then
	 *    check if we want to exclude any from xprefix.
	 * With the -exclude flag: If it matches the prefix, then
	 *    check if we want to add any from xprefix.
	 */
	if (match && seenxprefix) {
	    for (ti = as->parms[4].items; ti; ti = ti->next) {
		if (strncmp(ti->data, "^", 1) == 0) {
#ifdef HAVE_POSIX_REGEX
		    regex_t re;
		    char errbuf[256];

		    /* XXX -- should just do the compile once! */
		    code = regcomp(&re, ti->data, REG_NOSUB);
		    if (code != 0) {
			regerror(code, &re, errbuf, sizeof errbuf);
			fprintf(STDERR,
				"Error in -xprefix regular expression: '%s': %s\n",
				ti->data, errbuf);
			exit(1);
		    }
		    if (regexec(&re, vllist->name, 0, NULL, 0) == 0)
			    match = 0;
		    regfree(&re);
#else
		    ccode = (char *)re_comp(ti->data);
		    if (ccode) {
			fprintf(STDERR,
				"Error in -xprefix regular expression: '%s': %s\n",
				ti->data, ccode);
			exit(1);
		    }
		    if (re_exec(vllist->name) == 1) {
			match = 0;
			break;
		    }
#endif
		} else {
		    if (strncmp(vllist->name, ti->data, strlen(ti->data)) ==
			0) {
			match = 0;
			break;
		    }
		}
	    }
	}

	if (exclude)
	    match = !match;	/* -exclude will reverse the match */
	if (!match)
	    continue;		/* Skip if no match */

	/* Print list of volumes to backup */
	if (noaction) {
	    fprintf(STDOUT, "     %s\n", vllist->name);
	    continue;
	}

	if (!(vllist->flags & RW_EXISTS)) {
	    if (verbose) {
		fprintf(STDOUT,
			"Omitting to backup %s since RW volume does not exist \n",
			vllist->name);
		fprintf(STDOUT, "\n");
	    }
	    fflush(STDOUT);
	    continue;
	}

	avolid = vllist->volumeId[RWVOL];
	MapHostToNetwork(vllist);
	GetServerAndPart(vllist, RWVOL, &aserver1, &apart1, &previdx);
	if (aserver1 == -1 || apart1 == -1) {
	    fprintf(STDOUT, "could not backup %s, invalid VLDB entry\n",
		    vllist->name);
	    totalFail++;
	    continue;
	}
	if (aserver) {
	    same = VLDB_IsSameAddrs(aserver, aserver1, &error);
	    if (error) {
		fprintf(STDERR,
			"Failed to get info about server's %d address(es) from vlserver (err=%d); aborting call!\n",
			aserver, error);
		totalFail++;
		continue;
	    }
	}
	if ((aserver && !same) || (apart && (apart != apart1))) {
	    if (verbose) {
		fprintf(STDOUT,
			"Omitting to backup %s since the RW is in a different location\n",
			vllist->name);
	    }
	    continue;
	}
	if (verbose) {
	    time_t now = time(0);
	    fprintf(STDOUT, "Creating backup volume for %s on %s",
		    vllist->name, ctime(&now));
	    fflush(STDOUT);
	}

	code = UV_BackupVolume(aserver1, apart1, avolid);
	if (code) {
	    fprintf(STDOUT, "Could not backup %s\n", vllist->name);
	    totalFail++;
	} else {
	    totalBack++;
	}
	if (verbose)
	    fprintf(STDOUT, "\n");
	fflush(STDOUT);
    }				/* process each vldb entry */
    fprintf(STDOUT, "done\n");
    fprintf(STDOUT, "Total volumes backed up: %lu; failed to backup: %lu\n",
	    (unsigned long)totalBack, (unsigned long)totalFail);
    fflush(STDOUT);
    if (arrayEntries.nbulkentries_val)
	free(arrayEntries.nbulkentries_val);
    return 0;
}

static int
UnlockVLDB(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 apart;
    afs_int32 aserver, code;
    afs_int32 vcode;
    struct VldbListByAttributes attributes;
    nbulkentries arrayEntries;
    register struct nvldbentry *vllist;
    afs_int32 nentries;
    int j;
    afs_int32 volid;
    afs_int32 totalE;
    char pname[10];

    apart = -1;
    totalE = 0;
    attributes.Mask = 0;

    if (as->parms[0].items) {	/* server specified */
	aserver = GetServer(as->parms[0].items->data);
	if (aserver == 0) {
	    fprintf(STDERR, "vos: server '%s' not found in host table\n",
		    as->parms[0].items->data);
	    exit(1);
	}
	attributes.server = ntohl(aserver);
	attributes.Mask |= VLLIST_SERVER;
    }
    if (as->parms[1].items) {	/* partition specified */
	apart = volutil_GetPartitionID(as->parms[1].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[1].items->data);
	    exit(1);
	}
	if (!IsPartValid(apart, aserver, &code)) {	/*check for validity of the partition */
	    if (code)
		PrintError("", code);
	    else
		fprintf(STDERR,
			"vos : partition %s does not exist on the server\n",
			as->parms[1].items->data);
	    exit(1);
	}
	attributes.partition = apart;
	attributes.Mask |= VLLIST_PARTITION;
    }
    attributes.flag = VLOP_ALLOPERS;
    attributes.Mask |= VLLIST_FLAG;
    memset(&arrayEntries, 0, sizeof(arrayEntries));	/*initialize to hint the stub  to alloc space */
    vcode = VLDB_ListAttributes(&attributes, &nentries, &arrayEntries);
    if (vcode) {
	fprintf(STDERR, "Could not access the VLDB for attributes\n");
	PrintError("", vcode);
	exit(1);
    }
    for (j = 0; j < nentries; j++) {	/* process each entry */
	vllist = &arrayEntries.nbulkentries_val[j];
	volid = vllist->volumeId[RWVOL];
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, volid, -1,
				LOCKREL_OPCODE | LOCKREL_AFSID | 
				LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR, "Could not unlock entry for volume %s\n",
		    vllist->name);
	    PrintError("", vcode);
	    totalE++;
	}

    }
    MapPartIdIntoName(apart, pname);
    if (totalE)
	fprintf(STDOUT,
		"Could not lock %lu VLDB entries of %lu locked entries\n",
		(unsigned long)totalE, (unsigned long)nentries);
    else {
	if (as->parms[0].items) {
	    fprintf(STDOUT,
		    "Unlocked all the VLDB entries for volumes on server %s ",
		    as->parms[0].items->data);
	    if (as->parms[1].items) {
		MapPartIdIntoName(apart, pname);
		fprintf(STDOUT, "partition %s\n", pname);
	    } else
		fprintf(STDOUT, "\n");

	} else if (as->parms[1].items) {
	    MapPartIdIntoName(apart, pname);
	    fprintf(STDOUT,
		    "Unlocked all the VLDB entries for volumes on partition %s on all servers\n",
		    pname);
	}
    }

    if (arrayEntries.nbulkentries_val)
	free(arrayEntries.nbulkentries_val);
    return 0;
}

static char *
PrintInt64Size(afs_uint64 in)
{
    register afs_uint32 hi, lo;
    register char * units;
    static char output[16];

    SplitInt64(in,hi,lo);

    if (hi == 0) {
        units = "KB";
    } else if (!(hi & 0xFFFFFC00)) {
        units = "MB";
        lo = (hi << 22) | (lo >> 10);
    } else if (!(hi & 0xFFF00000)) {
        units = "GB";
        lo = (hi << 12) | (lo >> 20);
    } else if (!(hi & 0xC0000000)) {
        units = "TB";
        lo = (hi << 2) | (lo >> 30);
    } else {
        units = "PB";
        lo = (hi >> 8);
    }
    sprintf(output,"%u %s", lo, units);
    return output;
}

static int
PartitionInfo(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 apart;
    afs_int32 aserver, code;
    char pname[10];
    struct diskPartition partition;
    struct partList dummyPartList;
    int i, cnt;
    int printSummary=0, sumPartitions=0;
    afs_uint64 sumFree, sumStorage, tmp;

    ZeroInt64(sumFree);
    ZeroInt64(sumStorage);
    apart = -1;
    aserver = GetServer(as->parms[0].items->data);
    if (aserver == 0) {
	fprintf(STDERR, "vos: server '%s' not found in host table\n",
		as->parms[0].items->data);
	exit(1);
    }
    if (as->parms[1].items) {
	apart = volutil_GetPartitionID(as->parms[1].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		    as->parms[1].items->data);
	    exit(1);
	}
	dummyPartList.partId[0] = apart;
	dummyPartList.partFlags[0] = PARTVALID;
	cnt = 1;
    }
    if (as->parms[2].items) {
        printSummary = 1;
    }
    if (apart != -1) {
	if (!IsPartValid(apart, aserver, &code)) {	/*check for validity of the partition */
	    if (code)
		PrintError("", code);
	    else
		fprintf(STDERR,
			"vos : partition %s does not exist on the server\n",
			as->parms[1].items->data);
	    exit(1);
	}
    } else {
	code = UV_ListPartitions(aserver, &dummyPartList, &cnt);
	if (code) {
	    PrintDiagnostics("listpart", code);
	    exit(1);
	}
    }
    for (i = 0; i < cnt; i++) {
	if (dummyPartList.partFlags[i] & PARTVALID) {
	    MapPartIdIntoName(dummyPartList.partId[i], pname);
	    code = UV_PartitionInfo(aserver, pname, &partition);
	    if (code) {
		fprintf(STDERR, "Could not get information on partition %s\n",
			pname);
		PrintError("", code);
		exit(1);
	    }
	    fprintf(STDOUT,
		    "Free space on partition %s: %d K blocks out of total %d\n",
		    pname, partition.free, partition.minFree);
	    sumPartitions++;
            FillInt64(tmp,0,partition.free);
            AddUInt64(sumFree,tmp,&sumFree);
            FillInt64(tmp,0,partition.minFree);
            AddUInt64(sumStorage,tmp,&sumStorage);
	}
    }
    if (printSummary) {
        fprintf(STDOUT,
		"Summary: %s free out of ",
		PrintInt64Size(sumFree));
        fprintf(STDOUT,
                "%s on %d partitions\n",
                PrintInt64Size(sumStorage), 
                sumPartitions);
    }
    return 0;
}

static int
ChangeAddr(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 ip1, ip2, vcode;
    int remove = 0;

    ip1 = GetServer(as->parms[0].items->data);
    if (!ip1) {
	fprintf(STDERR, "vos: invalid host address\n");
	return (EINVAL);
    }

    if ((as->parms[1].items && as->parms[2].items)
	|| (!as->parms[1].items && !as->parms[2].items)) {
	fprintf(STDERR,
		"vos: Must specify either '-newaddr <addr>' or '-remove' flag\n");
	return (EINVAL);
    }

    if (as->parms[1].items) {
	ip2 = GetServer(as->parms[1].items->data);
	if (!ip2) {
	    fprintf(STDERR, "vos: invalid host address\n");
	    return (EINVAL);
	}
    } else {
	/* Play a trick here. If we are removing an address, ip1 will be -1
	 * and ip2 will be the original address. This switch prevents an 
	 * older revision vlserver from removing the IP address.
	 */
	remove = 1;
	ip2 = ip1;
	ip1 = 0xffffffff;
    }

    vcode = ubik_Call_New(VL_ChangeAddr, cstruct, 0, ntohl(ip1), ntohl(ip2));
    if (vcode) {
	if (remove) {
	    fprintf(STDERR, "Could not remove server %s from the VLDB\n",
		    as->parms[0].items->data);
	    if (vcode == VL_NOENT) {
		fprintf(STDERR,
			"vlserver does not support the remove flag or ");
	    }
	} else {
	    fprintf(STDERR, "Could not change server %s to server %s\n",
		    as->parms[0].items->data, as->parms[1].items->data);
	}
	PrintError("", vcode);
	return (vcode);
    }

    if (remove) {
	fprintf(STDOUT, "Removed server %s from the VLDB\n",
		as->parms[0].items->data);
    } else {
	fprintf(STDOUT, "Changed server %s to server %s\n",
		as->parms[0].items->data, as->parms[1].items->data);
    }
    return 0;
}

static void
print_addrs(const bulkaddrs * addrs, const afsUUID * m_uuid, int nentries,
	    int print, int noresolve)
{
    afs_int32 vcode;
    afs_int32 i, j;
    struct VLCallBack vlcb;
    afs_int32 *addrp;
    bulkaddrs m_addrs;
    ListAddrByAttributes m_attrs;
    afs_int32 m_nentries, *m_addrp;
    afs_int32 base, index;
    char buf[1024];

    if (print) {
	afsUUID_to_string(m_uuid, buf, sizeof(buf));
	printf("UUID: %s\n", buf);
    }

    /* print out the list of all the server */
    addrp = (afs_int32 *) addrs->bulkaddrs_val;
    for (i = 0; i < nentries; i++, addrp++) {
	/* If it is a multihomed address, then we will need to 
	 * get the addresses for this multihomed server from
	 * the vlserver and print them.
	 */
	if (((*addrp & 0xff000000) == 0xff000000) && ((*addrp) & 0xffff)) {
	    /* Get the list of multihomed fileservers */
	    base = (*addrp >> 16) & 0xff;
	    index = (*addrp) & 0xffff;

	    if ((base >= 0) && (base <= VL_MAX_ADDREXTBLKS) && (index >= 1)
		&& (index <= VL_MHSRV_PERBLK)) {
		m_attrs.Mask = VLADDR_INDEX;
		m_attrs.index = (base * VL_MHSRV_PERBLK) + index;
		m_nentries = 0;
		m_addrs.bulkaddrs_val = 0;
		m_addrs.bulkaddrs_len = 0;
		vcode =
		    ubik_VL_GetAddrsU(cstruct, 0, &m_attrs, &m_uuid,
				      &vlcb, &m_nentries, &m_addrs);
		if (vcode) {
		    fprintf(STDERR,
			    "vos: could not list the multi-homed server addresses\n");
		    PrintError("", vcode);
		}

		/* Print the list */
		m_addrp = (afs_int32 *) m_addrs.bulkaddrs_val;
		for (j = 0; j < m_nentries; j++, m_addrp++) {
		    *m_addrp = htonl(*m_addrp);
		    if (noresolve) {
			char hoststr[16];
			printf("%s ", afs_inet_ntoa_r(*m_addrp, hoststr));
		    } else {
			printf("%s ", hostutil_GetNameByINet(*m_addrp));
		    }
		}
		if (j == 0) {
		    printf("<unknown>\n");
		} else {
		    printf("\n");
		}

		continue;
	    }
	}

	/* Otherwise, it is a non-multihomed entry and contains
	 * the IP address of the server - print it.
	 */
	*addrp = htonl(*addrp);
	if (noresolve) {
	    char hoststr[16];
	    printf("%s\n", afs_inet_ntoa_r(*addrp, hoststr));
	} else {
	    printf("%s\n", hostutil_GetNameByINet(*addrp));
	}
    }

    if (print) {
	printf("\n");
    }
    return;
}

static int
ListAddrs(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 vcode;
    afs_int32 i, noresolve = 0, printuuid = 0;
    struct VLCallBack vlcb;
    afs_int32 nentries;
    bulkaddrs m_addrs;
    ListAddrByAttributes m_attrs;
    afsUUID m_uuid, askuuid;
    afs_int32 m_nentries;

    memset(&m_attrs, 0, sizeof(struct ListAddrByAttributes));
    m_attrs.Mask = VLADDR_INDEX;

    memset(&m_addrs, 0, sizeof(bulkaddrs));
    memset(&askuuid, 0, sizeof(afsUUID));
    if (as->parms[0].items) {
	/* -uuid */
        if (afsUUID_from_string(as->parms[0].items->data, &askuuid) < 0) {
	    fprintf(STDERR, "vos: invalid UUID '%s'\n", 
		    as->parms[0].items->data);
	    exit(-1);
	}
	m_attrs.Mask = VLADDR_UUID;
	m_attrs.uuid = askuuid;
    }
    if (as->parms[1].items) {
	/* -host */
	struct hostent *he;
	afs_int32 saddr;
	he = hostutil_GetHostByName((char *)as->parms[1].items->data);
	if (he == NULL) {
	    fprintf(STDERR, "vos: Can't get host info for '%s'\n",
		    as->parms[1].items->data);
	    exit(-1);
	}
	memcpy(&saddr, he->h_addr, 4);
	m_attrs.Mask = VLADDR_IPADDR;
	m_attrs.ipaddr = ntohl(saddr);
    }
    if (as->parms[2].items) {
	noresolve = 1;
    }
    if (as->parms[3].items) {
	printuuid = 1;
    }

    m_addrs.bulkaddrs_val = 0;
    m_addrs.bulkaddrs_len = 0;

    vcode =
	ubik_Call_New(VL_GetAddrs, cstruct, 0, 0, 0, &vlcb, &nentries,
		      &m_addrs);
    if (vcode) {
	fprintf(STDERR, "vos: could not list the server addresses\n");
	PrintError("", vcode);
	return (vcode);
    }

    m_nentries = 0;
    m_addrs.bulkaddrs_val = 0;
    m_addrs.bulkaddrs_len = 0;
    i = 1;
    while (1) {
	m_attrs.index = i;

	vcode =
	    ubik_Call_New(VL_GetAddrsU, cstruct, 0, &m_attrs, &m_uuid,
			  &vlcb, &m_nentries, &m_addrs);

	if (vcode == VL_NOENT) {
  	    if (m_attrs.Mask == VLADDR_UUID) {
	        fprintf(STDERR, "vos: no entry for UUID '%s' found in VLDB\n",
			as->parms[0].items->data);
		exit(-1);
	    } else if (m_attrs.Mask == VLADDR_IPADDR) {
	        fprintf(STDERR, "vos: no entry for host '%s' [0x%08x] found in VLDB\n",
			as->parms[1].items->data, m_attrs.ipaddr);
		exit(-1);
	    } else {
	        i++;
		nentries++;
		continue;
	    }
	}

	if (vcode == VL_INDEXERANGE) {
	    break;
	}

	if (vcode) {
	    fprintf(STDERR, "vos: could not list the server addresses\n");
	    PrintError("", vcode);
	    return (vcode);
	}

	print_addrs(&m_addrs, &m_uuid, m_nentries, printuuid, noresolve);
	i++;

	if ((as->parms[1].items) || (as->parms[0].items) || (i > nentries))
	    break;
    }

    return 0;
}

static int
LockEntry(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, vcode, err;

    avolid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[0].items->data);
	exit(1);
    }
    vcode = ubik_VL_SetLock(cstruct, 0, avolid, -1, VLOP_DELETE);
    if (vcode) {
	fprintf(STDERR, "Could not lock VLDB entry for volume %s\n",
		as->parms[0].items->data);
	PrintError("", vcode);
	exit(1);
    }
    fprintf(STDOUT, "Locked VLDB entry for volume %s\n",
	    as->parms[0].items->data);
    return 0;
}

static int
ConvertRO(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 partition = -1;
    afs_int32 server, volid, code, i, same;
    struct nvldbentry entry, storeEntry;
    afs_int32 vcode;
    afs_int32 rwindex;
    afs_int32 rwserver = 0;
    afs_int32 rwpartition;
    afs_int32 roindex;
    afs_int32 roserver = 0;
    afs_int32 ropartition;
    int force = 0;
    struct rx_connection *aconn;
    char c, dc;

    server = GetServer(as->parms[0].items->data);
    if (!server) {
	fprintf(STDERR, "vos: host '%s' not found in host table\n",
		as->parms[0].items->data);
	return ENOENT;
    }
    partition = volutil_GetPartitionID(as->parms[1].items->data);
    if (partition < 0) {
	fprintf(STDERR, "vos: could not interpret partition name '%s'\n",
		as->parms[1].items->data);
	return ENOENT;
    }
    if (!IsPartValid(partition, server, &code)) {
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR,
		    "vos : partition %s does not exist on the server\n",
		    as->parms[1].items->data);
	return ENOENT;
    }
    volid = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &code);
    if (volid == 0) {
	if (code)
	    PrintError("", code);
	else
	    fprintf(STDERR, "Unknown volume ID or name '%s'\n",
		    as->parms[0].items->data);
	return -1;
    }
    if (as->parms[3].items)
	force = 1;

    vcode = VLDB_GetEntryByID(volid, -1, &entry);
    if (vcode) {
	fprintf(STDERR,
		"Could not fetch the entry for volume %lu from VLDB\n",
		(unsigned long)volid);
	PrintError("convertROtoRW", code);
	return vcode;
    }

    /* use RO volid even if user specified RW or BK volid */

    if (volid != entry.volumeId[ROVOL])
	volid = entry.volumeId[ROVOL];

    MapHostToNetwork(&entry);
    for (i = 0; i < entry.nServers; i++) {
	if (entry.serverFlags[i] & ITSRWVOL) {
	    rwindex = i;
	    rwserver = entry.serverNumber[i];
	    rwpartition = entry.serverPartition[i];
	}
	if (entry.serverFlags[i] & ITSROVOL) {
	    same = VLDB_IsSameAddrs(server, entry.serverNumber[i], &code);
	    if (code) {
		fprintf(STDERR,
			"Failed to get info about server's %d address(es) from vlserver (err=%d); aborting call!\n",
			server, code);
		return ENOENT;
	    }
	    if (same) {
		roindex = i;
		roserver = entry.serverNumber[i];
		ropartition = entry.serverPartition[i];
		break;
	    }
	}
    }
    if (!roserver) {
	fprintf(STDERR, "Warning: RO volume didn't exist in vldb!\n");
    }
    if (ropartition != partition) {
	fprintf(STDERR,
		"Warning: RO volume should be in partition %d instead of %d (vldb)\n",
		ropartition, partition);
    }

    if (rwserver) {
	fprintf(STDERR,
		"VLDB indicates that a RW volume exists already on %s in partition %s.\n",
		hostutil_GetNameByINet(rwserver),
		volutil_PartitionName(rwpartition));
	if (!force) {
	    fprintf(STDERR, "Overwrite this VLDB entry? [y|n] (n)\n");
	    dc = c = getchar();
	    while (!(dc == EOF || dc == '\n'))
		dc = getchar();	/* goto end of line */
	    if ((c != 'y') && (c != 'Y')) {
		fprintf(STDERR, "aborted.\n");
		return -1;
	    }
	}
    }

    vcode =
	ubik_VL_SetLock(cstruct, 0, entry.volumeId[RWVOL], RWVOL,
		  VLOP_MOVE);
    aconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    code = AFSVolConvertROtoRWvolume(aconn, partition, volid);
    if (code) {
	fprintf(STDERR,
		"Converting RO volume %lu to RW volume failed with code %d\n",
		(unsigned long)volid, code);
	PrintError("convertROtoRW ", code);
	return -1;
    }
    entry.serverFlags[roindex] = ITSRWVOL;
    entry.flags |= RW_EXISTS;
    entry.flags &= ~BACK_EXISTS;
    if (rwserver) {
	(entry.nServers)--;
	if (rwindex != entry.nServers) {
	    entry.serverNumber[rwindex] = entry.serverNumber[entry.nServers];
	    entry.serverPartition[rwindex] =
		entry.serverPartition[entry.nServers];
	    entry.serverFlags[rwindex] = entry.serverFlags[entry.nServers];
	    entry.serverNumber[entry.nServers] = 0;
	    entry.serverPartition[entry.nServers] = 0;
	    entry.serverFlags[entry.nServers] = 0;
	}
    }
    entry.flags &= ~RO_EXISTS;
    for (i = 0; i < entry.nServers; i++) {
	if (entry.serverFlags[i] & ITSROVOL) {
	    if (!(entry.serverFlags[i] & (RO_DONTUSE | NEW_REPSITE)))
		entry.flags |= RO_EXISTS;
	}
    }
    MapNetworkToHost(&entry, &storeEntry);
    code =
	VLDB_ReplaceEntry(entry.volumeId[RWVOL], RWVOL, &storeEntry,
			  (LOCKREL_OPCODE | LOCKREL_AFSID |
			   LOCKREL_TIMESTAMP));
    if (code) {
	fprintf(STDERR,
		"Warning: volume converted, but vldb update failed with code %d!\n",
		code);
    }
    vcode = UV_LockRelease(entry.volumeId[RWVOL]);
    if (vcode) {
	PrintDiagnostics("unlock", vcode);
    }
    return code;
}

static int
Sizes(register struct cmd_syndesc *as, void *arock)
{
    afs_int32 avolid, aserver, apart, voltype, fromdate = 0, code, err, i;
    struct nvldbentry entry;
    volintSize vol_size;

    rx_SetRxDeadTime(60 * 10);
    for (i = 0; i < MAXSERVERS; i++) {
	struct rx_connection *rxConn = ubik_GetRPCConn(cstruct, i);
	if (rxConn == 0)
	    break;
	rx_SetConnDeadTime(rxConn, rx_connDeadTime);
	if (rxConn->service)
	    rxConn->service->connDeadTime = rx_connDeadTime;
    }

    avolid = vsu_GetVolumeID(as->parms[0].items->data, cstruct, &err);
    if (avolid == 0) {
	if (err)
	    PrintError("", err);
	else
	    fprintf(STDERR, "vos: can't find volume '%s'\n",
		    as->parms[0].items->data);
	return ENOENT;
    }

    if (as->parms[1].items || as->parms[2].items) {
	if (!as->parms[1].items || !as->parms[2].items) {
	    fprintf(STDERR,
		    "Must specify both -server and -partition options\n");
	    return -1;
	}
	aserver = GetServer(as->parms[2].items->data);
	if (aserver == 0) {
	    fprintf(STDERR, "Invalid server name\n");
	    return -1;
	}
	apart = volutil_GetPartitionID(as->parms[1].items->data);
	if (apart < 0) {
	    fprintf(STDERR, "Invalid partition name\n");
	    return -1;
	}
    } else {
	code = GetVolumeInfo(avolid, &aserver, &apart, &voltype, &entry);
	if (code)
	    return code;
    }

    fromdate = 0;

    if (as->parms[4].items && strcmp(as->parms[4].items->data, "0")) {
	code = ktime_DateToInt32(as->parms[4].items->data, &fromdate);
	if (code) {
	    fprintf(STDERR, "vos: failed to parse date '%s' (error=%d))\n",
		    as->parms[4].items->data, code);
	    return code;
	}
    }

    fprintf(STDOUT, "Volume: %s\n", as->parms[0].items->data);

    if (as->parms[3].items) {	/* do the dump estimate */
#ifdef AFS_64BIT_ENV
	vol_size.dump_size = 0;
#else
   FillInt64(vol_size.dump_size,0, 1);
#endif
	code = UV_GetSize(avolid, aserver, apart, fromdate, &vol_size);
	if (code) {
	    PrintDiagnostics("size", code);
	    return code;
	}
	/* presumably the size info is now gathered in pntr */
	/* now we display it */

	fprintf(STDOUT, "dump_size: %llu\n", vol_size.dump_size);
    }

    /* Display info */

    return 0;
}

PrintDiagnostics(astring, acode)
     char *astring;
     afs_int32 acode;
{
    if (acode == EACCES) {
	fprintf(STDERR,
		"You are not authorized to perform the 'vos %s' command (%d)\n",
		astring, acode);
    } else {
	fprintf(STDERR, "Error in vos %s command.\n", astring);
	PrintError("", acode);
    }
    return 0;
}


static int
MyBeforeProc(struct cmd_syndesc *as, void *arock)
{
    register char *tcell;
    register afs_int32 code;
    register afs_int32 sauth;

    /* Initialize the ubik_client connection */
    rx_SetRxDeadTime(90);
    cstruct = (struct ubik_client *)0;

    sauth = 0;
    tcell = NULL;
    if (as->parms[12].items)	/* if -cell specified */
	tcell = as->parms[12].items->data;
    if (as->parms[14].items)	/* -serverauth specified */
	sauth = 1;
    if (as->parms[16].items)	/* -crypt specified */
	vsu_SetCrypt(1);
    if ((code =
	 vsu_ClientInit((as->parms[13].items != 0), confdir, tcell, sauth,
			&cstruct, UV_SetSecurity))) {
	fprintf(STDERR, "could not initialize VLDB library (code=%lu) \n",
		(unsigned long)code);
	exit(1);
    }
    rxInitDone = 1;
    if (as->parms[15].items)	/* -verbose flag set */
	verbose = 1;
    else
	verbose = 0;
    return 0;
}

int
osi_audit()
{
/* this sucks but it works for now.
*/
    return 0;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
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

    confdir = AFSDIR_CLIENT_ETC_DIRPATH;

    cmd_SetBeforeProc(MyBeforeProc, NULL);

    ts = cmd_CreateSyntax("create", CreateVolume, NULL, "create a new volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "partition name");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "volume name");
    cmd_AddParm(ts, "-maxquota", CMD_SINGLE, CMD_OPTIONAL,
		"initial quota (KB)");
#ifdef notdef
    cmd_AddParm(ts, "-minquota", CMD_SINGLE, CMD_OPTIONAL, "");
#endif
    COMMONPARMS;

    ts = cmd_CreateSyntax("remove", DeleteVolume, NULL, "delete a volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");

    COMMONPARMS;

    ts = cmd_CreateSyntax("move", MoveVolume, NULL, "move a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-fromserver", CMD_SINGLE, 0, "machine name on source");
    cmd_AddParm(ts, "-frompartition", CMD_SINGLE, 0,
		"partition name on source");
    cmd_AddParm(ts, "-toserver", CMD_SINGLE, 0,
		"machine name on destination");
    cmd_AddParm(ts, "-topartition", CMD_SINGLE, 0,
		"partition name on destination");
    cmd_AddParm(ts, "-live", CMD_FLAG, CMD_OPTIONAL,
		"copy live volume without cloning");
    COMMONPARMS;

    ts = cmd_CreateSyntax("copy", CopyVolume, NULL, "copy a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID on source");
    cmd_AddParm(ts, "-fromserver", CMD_SINGLE, 0, "machine name on source");
    cmd_AddParm(ts, "-frompartition", CMD_SINGLE, 0,
		"partition name on source");
    cmd_AddParm(ts, "-toname", CMD_SINGLE, 0, "volume name on destination");
    cmd_AddParm(ts, "-toserver", CMD_SINGLE, 0,
		"machine name on destination");
    cmd_AddParm(ts, "-topartition", CMD_SINGLE, 0,
		"partition name on destination");
    cmd_AddParm(ts, "-offline", CMD_FLAG, CMD_OPTIONAL,
		"leave new volume offline");
    cmd_AddParm(ts, "-readonly", CMD_FLAG, CMD_OPTIONAL,
		"make new volume read-only");
    cmd_AddParm(ts, "-live", CMD_FLAG, CMD_OPTIONAL,
		"copy live volume without cloning");
    COMMONPARMS;

    ts = cmd_CreateSyntax("shadow", ShadowVolume, NULL,
			  "make or update a shadow volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID on source");
    cmd_AddParm(ts, "-fromserver", CMD_SINGLE, 0, "machine name on source");
    cmd_AddParm(ts, "-frompartition", CMD_SINGLE, 0,
		"partition name on source");
    cmd_AddParm(ts, "-toserver", CMD_SINGLE, 0,
		"machine name on destination");
    cmd_AddParm(ts, "-topartition", CMD_SINGLE, 0,
		"partition name on destination");
    cmd_AddParm(ts, "-toname", CMD_SINGLE, CMD_OPTIONAL,
		"volume name on destination");
    cmd_AddParm(ts, "-toid", CMD_SINGLE, CMD_OPTIONAL,
		"volume ID on destination");
    cmd_AddParm(ts, "-offline", CMD_FLAG, CMD_OPTIONAL,
		"leave shadow volume offline");
    cmd_AddParm(ts, "-readonly", CMD_FLAG, CMD_OPTIONAL,
		"make shadow volume read-only");
    cmd_AddParm(ts, "-live", CMD_FLAG, CMD_OPTIONAL,
		"copy live volume without cloning");
    cmd_AddParm(ts, "-incremental", CMD_FLAG, CMD_OPTIONAL,
		"do incremental update if target exists");
    COMMONPARMS;

    ts = cmd_CreateSyntax("backup", BackupVolume, NULL,
			  "make backup of a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    COMMONPARMS;

    ts = cmd_CreateSyntax("clone", CloneVolume, NULL,
			  "make clone of a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "server");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition");
    cmd_AddParm(ts, "-toname", CMD_SINGLE, CMD_OPTIONAL,
		"volume name on destination");
    cmd_AddParm(ts, "-toid", CMD_SINGLE, CMD_OPTIONAL,
		"volume ID on destination");
    cmd_AddParm(ts, "-offline", CMD_FLAG, CMD_OPTIONAL,
		"leave clone volume offline");
    cmd_AddParm(ts, "-readonly", CMD_FLAG, CMD_OPTIONAL,
		"make clone volume read-only, not readwrite");
    COMMONPARMS;

    ts = cmd_CreateSyntax("release", ReleaseVolume, NULL, "release a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL,
		"force a complete release");
    COMMONPARMS;

    ts = cmd_CreateSyntax("dump", DumpVolume, NULL, "dump a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-time", CMD_SINGLE, CMD_OPTIONAL, "dump from time");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "dump file");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "server");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition");
    cmd_AddParm(ts, "-clone", CMD_FLAG, CMD_OPTIONAL,
		"dump a clone of the volume");
    cmd_AddParm(ts, "-omitdirs", CMD_FLAG, CMD_OPTIONAL,
		"omit unchanged directories from an incremental dump");
    COMMONPARMS;

    ts = cmd_CreateSyntax("restore", RestoreVolume, NULL, "restore a volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "partition name");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name of volume to be restored");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "dump file");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_OPTIONAL, "volume ID");
    cmd_AddParm(ts, "-overwrite", CMD_SINGLE, CMD_OPTIONAL,
		"abort | full | incremental");
    cmd_AddParm(ts, "-offline", CMD_FLAG, CMD_OPTIONAL,
		"leave restored volume offline");
    cmd_AddParm(ts, "-readonly", CMD_FLAG, CMD_OPTIONAL,
		"make restored volume read-only");
    cmd_AddParm(ts, "-creation", CMD_SINGLE, CMD_OPTIONAL,
		"dump | keep | new");
    cmd_AddParm(ts, "-lastupdate", CMD_SINGLE, CMD_OPTIONAL,
		"dump | keep | new");
    cmd_AddParm(ts, "-nodelete", CMD_FLAG, CMD_OPTIONAL,
		"do not delete old site when restoring to a new site");
    COMMONPARMS;

    ts = cmd_CreateSyntax("unlock", LockReleaseCmd, NULL,
			  "release lock on VLDB entry for a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    COMMONPARMS;

    ts = cmd_CreateSyntax("changeloc", ChangeLocation, NULL,
			  "change an RW volume's location in the VLDB");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0,
		"machine name for new location");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0,
		"partition name for new location");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    COMMONPARMS;

    ts = cmd_CreateSyntax("addsite", AddSite, NULL, "add a replication site");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name for new site");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0,
		"partition name for new site");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-valid", CMD_FLAG, CMD_OPTIONAL | CMD_HIDE, "publish as an up-to-date site in VLDB");
    COMMONPARMS;

    ts = cmd_CreateSyntax("remsite", RemoveSite, NULL,
			  "remove a replication site");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "partition name");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    COMMONPARMS;

    ts = cmd_CreateSyntax("listpart", ListPartitions, NULL, "list partitions");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    COMMONPARMS;

    ts = cmd_CreateSyntax("listvol", ListVolumes, NULL,
			  "list volumes on server (bypass VLDB)");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-fast", CMD_FLAG, CMD_OPTIONAL, "minimal listing");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL,
		"list all normal volume fields");
    cmd_AddParm(ts, "-quiet", CMD_FLAG, CMD_OPTIONAL,
		"generate minimal information");
    cmd_AddParm(ts, "-extended", CMD_FLAG, CMD_OPTIONAL,
		"list extended volume fields");
#ifdef FULL_LISTVOL_SWITCH
    cmd_AddParm(ts, "-format", CMD_FLAG, CMD_OPTIONAL,
		"machine readable format");
#endif /* FULL_LISTVOL_SWITCH */
    COMMONPARMS;

    ts = cmd_CreateSyntax("syncvldb", SyncVldb, NULL,
			  "synchronize VLDB with server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_OPTIONAL, "volume name or ID");
    COMMONPARMS;

    ts = cmd_CreateSyntax("syncserv", SyncServer, NULL,
			  "synchronize server with VLDB");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    COMMONPARMS;

    ts = cmd_CreateSyntax("examine", ExamineVolume, NULL,
			  "everything about the volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-extended", CMD_FLAG, CMD_OPTIONAL,
		"list extended volume fields");
#ifdef FULL_LISTVOL_SWITCH
    cmd_AddParm(ts, "-format", CMD_FLAG, CMD_OPTIONAL,
		"machine readable format");
#endif /* FULL_LISTVOL_SWITCH */
    COMMONPARMS;
    cmd_CreateAlias(ts, "volinfo");

    ts = cmd_CreateSyntax("setfields", SetFields, NULL,
			  "change volume info fields");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-maxquota", CMD_SINGLE, CMD_OPTIONAL, "quota (KB)");
    cmd_AddParm(ts, "-clearuse", CMD_FLAG, CMD_OPTIONAL, "clear dayUse");
    cmd_AddParm(ts, "-clearVolUpCounter", CMD_FLAG, CMD_OPTIONAL, "clear volUpdateCounter");
    COMMONPARMS;

    ts = cmd_CreateSyntax("offline", volOffline, NULL, (char *)CMD_HIDDEN);
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "server name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "partition name");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-sleep", CMD_SINGLE, CMD_OPTIONAL, "seconds to sleep");
    cmd_AddParm(ts, "-busy", CMD_FLAG, CMD_OPTIONAL, "busy volume");
    COMMONPARMS;

    ts = cmd_CreateSyntax("online", volOnline, NULL, (char *)CMD_HIDDEN);
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "server name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "partition name");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    COMMONPARMS;

    ts = cmd_CreateSyntax("zap", VolumeZap, NULL,
			  "delete the volume, don't bother with VLDB");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "partition name");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume ID");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL,
		"force deletion of bad volumes");
    cmd_AddParm(ts, "-backup", CMD_FLAG, CMD_OPTIONAL,
		"also delete backup volume if one is found");
    COMMONPARMS;

    ts = cmd_CreateSyntax("status", VolserStatus, NULL,
			  "report on volser status");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    COMMONPARMS;

    ts = cmd_CreateSyntax("rename", RenameVolume, NULL, "rename a volume");
    cmd_AddParm(ts, "-oldname", CMD_SINGLE, 0, "old volume name ");
    cmd_AddParm(ts, "-newname", CMD_SINGLE, 0, "new volume name ");
    COMMONPARMS;

    ts = cmd_CreateSyntax("listvldb", ListVLDB, NULL,
			  "list volumes in the VLDB");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_OPTIONAL, "volume name or ID");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-locked", CMD_FLAG, CMD_OPTIONAL, "locked volumes only");
    cmd_AddParm(ts, "-quiet", CMD_FLAG, CMD_OPTIONAL,
		"generate minimal information");
    cmd_AddParm(ts, "-nosort", CMD_FLAG, CMD_OPTIONAL,
		"do not alphabetically sort the volume names");
    COMMONPARMS;

    ts = cmd_CreateSyntax("backupsys", BackSys, NULL, "en masse backups");
    cmd_AddParm(ts, "-prefix", CMD_LIST, CMD_OPTIONAL,
		"common prefix on volume(s)");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-exclude", CMD_FLAG, CMD_OPTIONAL,
		"exclude common prefix volumes");
    cmd_AddParm(ts, "-xprefix", CMD_LIST, CMD_OPTIONAL,
		"negative prefix on volume(s)");
    cmd_AddParm(ts, "-dryrun", CMD_FLAG, CMD_OPTIONAL, "no action");
    COMMONPARMS;

    ts = cmd_CreateSyntax("delentry", DeleteEntry, NULL,
			  "delete VLDB entry for a volume");
    cmd_AddParm(ts, "-id", CMD_LIST, CMD_OPTIONAL, "volume name or ID");
    cmd_AddParm(ts, "-prefix", CMD_SINGLE, CMD_OPTIONAL,
		"prefix of the volume whose VLDB entry is to be deleted");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-noexecute", CMD_FLAG, CMD_OPTIONAL | CMD_HIDE,
		"no execute");
    COMMONPARMS;

    ts = cmd_CreateSyntax("partinfo", PartitionInfo, NULL,
			  "list partition information");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-summary", CMD_FLAG, CMD_OPTIONAL,
		"print storage summary");
    COMMONPARMS;

    ts = cmd_CreateSyntax("unlockvldb", UnlockVLDB, NULL,
			  "unlock all the locked entries in the VLDB");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    COMMONPARMS;

    ts = cmd_CreateSyntax("lock", LockEntry, NULL,
			  "lock VLDB entry for a volume");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    COMMONPARMS;

    ts = cmd_CreateSyntax("changeaddr", ChangeAddr, NULL,
			  "change the IP address of a file server");
    cmd_AddParm(ts, "-oldaddr", CMD_SINGLE, 0, "original IP address");
    cmd_AddParm(ts, "-newaddr", CMD_SINGLE, CMD_OPTIONAL, "new IP address");
    cmd_AddParm(ts, "-remove", CMD_FLAG, CMD_OPTIONAL,
		"remove the IP address from the VLDB");
    COMMONPARMS;

    ts = cmd_CreateSyntax("listaddrs", ListAddrs, NULL,
			  "list the IP address of all file servers registered in the VLDB");
    cmd_AddParm(ts, "-uuid", CMD_SINGLE, CMD_OPTIONAL, "uuid of server");
    cmd_AddParm(ts, "-host", CMD_SINGLE, CMD_OPTIONAL, "address of host");
    cmd_AddParm(ts, "-noresolve", CMD_FLAG, CMD_OPTIONAL,
		"don't resolve addresses");
    cmd_AddParm(ts, "-printuuid", CMD_FLAG, CMD_OPTIONAL,
		"print uuid of hosts");
    COMMONPARMS;

    ts = cmd_CreateSyntax("convertROtoRW", ConvertRO, NULL,
			  "convert a RO volume into a RW volume (after loss of old RW volume)");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "partition name");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL, "don't ask");
    COMMONPARMS;

    ts = cmd_CreateSyntax("size", Sizes, NULL,
			  "obtain various sizes of the volume.");
    cmd_AddParm(ts, "-id", CMD_SINGLE, 0, "volume name or ID");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "machine name");
    cmd_AddParm(ts, "-dump", CMD_FLAG, CMD_OPTIONAL,
		"Obtain the size of the dump");
    cmd_AddParm(ts, "-time", CMD_SINGLE, CMD_OPTIONAL, "dump from time");
    COMMONPARMS;

    code = cmd_Dispatch(argc, argv);
    if (rxInitDone) {
	/* Shut down the ubik_client and rx connections */
	if (cstruct) {
	    (void)ubik_ClientDestroy(cstruct);
	    cstruct = 0;
	}
	rx_Finalize();
    }

    exit((code ? -1 : 0));
}
