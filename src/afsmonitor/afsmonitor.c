/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.
 */

/*
 * Afsmonitor: An AFS Performance Monitoring Tool
 *
 *-------------------------------------------------------------------------*/


#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <cmd.h>
#include <signal.h>
#undef IN
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>

#include <gtxwindows.h>		/*Generic window package */
#include <gtxobjects.h>		/*Object definitions */
#include <gtxlightobj.h>	/*Light object interface */
#include <gtxcurseswin.h>	/*Curses window package */
#include <gtxdumbwin.h>		/*Dumb terminal window package */
#include <gtxX11win.h>		/*X11 window package */
#include <gtxframe.h>		/*Frame package */



#include <afs/xstat_fs.h>
#include <afs/xstat_cm.h>


#include "afsmonitor.h"


/* command line parameter indices */

#define P_CONFIG 	0
#define P_FREQUENCY	1
#define P_OUTPUT	2
#define P_DETAILED	3
/* #define P_PACKAGE	X */
#define P_DEBUG		4
#define P_FSHOSTS	5
#define P_CMHOSTS	6
#define P_BUFFERS	7


int afsmon_debug = 0;		/* debug info to file ? */
FILE *debugFD;			/* debugging file descriptor */
static int afsmon_output = 0;	/* output to file ? */
static int afsmon_detOutput = 0;	/* detailed output ? */
static int afsmon_onceOnly = 0;	/* probe once only ? (not implemented) */
int afsmon_probefreq;		/* probe frequency */
static int wpkg_to_use;		/* graphics package to use */
static char output_filename[80];	/* output filename */
char errMsg[256];		/* buffers used to print error messages after */
char errMsg1[256];		/* gtx is initialized (stderr/stdout gone !) */
int num_bufSlots = 0;		/* number of slots in fs & cm circular buffers */

/* Flags used to process "show" directives in config file */
short fs_showFlags[NUM_FS_STAT_ENTRIES];
short cm_showFlags[NUM_CM_STAT_ENTRIES];


/* afsmonitor misc definitions */

#define DEFAULT_FREQUENCY 60	/* default proble frequency in seconds */
#define DEFAULT_BUFSLOTS  0	/* default number of buffer slots */
#define CFG_STR_LEN	80	/* max length of config file fields */
#define FS 1			/* for misc. use */
#define CM 2			/* for misc. use */


#define	NUM_XSTAT_FS_AFS_PERFSTATS_LONGS 66	/* number of fields (longs) in struct afs_PerfStats that we display */
#define NUM_AFS_STATS_CMPERF_LONGS 40	/* number of longs in struct afs_stats_CMPerf excluding up/down stats and fields we dont display */


/* variables used for exec'ing user provided threshold handlers */
char *fsHandler_argv[20];	/* *argv[] for the handler */
char fsHandler_args[20][256];	/* buffer space for arguments */
int exec_fsThreshHandler = 0;	/* execute fs threshold handler ? */


/* THRESHOLD STRUCTURE DEFINITIONS */

/* flag to indicate that threshold entries apply to all hosts. these will
   be turned off when the first fs or cm host entry is processed */
static int global_ThreshFlag = 1;
static int global_fsThreshCount = 0;	/* number of global fs thresholds */
static int global_cmThreshCount = 0;	/* number of global cm thresholds */



/* Linked lists of file server and cache manager host names are made from
the entries in the config file. Head pointers to FS and CM server name lists. */
static struct afsmon_hostEntry *FSnameList;
static struct afsmon_hostEntry *CMnameList;

/* number of fileservers and cache managers to monitor */
int numFS = 0;
int numCM = 0;

/* variables used for processing config file */
/* ptr to the hostEntry structure of the last "fs" or "cm" entry processed
in the config file */
static struct afsmon_hostEntry *last_hostEntry;
/* names of the last host processed in the config file */
static char last_fsHost[HOST_NAME_LEN];
static char last_cmHost[HOST_NAME_LEN];
static lastHostType = 0;	/* 0 = no host entries processed
				 * 1 = last host was file server
				 * 2 = last host was cache manager. */


/* FILE SERVER CIRCULAR BUFFER VARIABLES  */

struct afsmon_fs_Results_list {
    struct xstat_fs_ProbeResults *fsResults;	/* ptr to results struct */
    int empty;			/* fsResults empty ? */
    struct afsmon_fs_Results_list *next;
};

struct afsmon_fs_Results_CBuffer {
    int probeNum;		/* probe number of entries in this slot */
    struct afsmon_fs_Results_list *list;	/* ptr to list of results */
};

/* buffer for FS probe results */
struct afsmon_fs_Results_CBuffer *afsmon_fs_ResultsCB;

int afsmon_fs_curr_CBindex = 0;	/* current fs CB slot */

/* Probe number variables. The current probe number is incremented 
when the first probe from a new probe cycle is received. The prev probe
number is incremented when the last probe of the current cycle is
received. This difference is because of the purpose for which these
counters are used */

int afsmon_fs_curr_probeNum = 1;	/* current fs probe number */
int afsmon_fs_prev_probeNum = 0;	/* previous fs probe number */


/* CACHE MANAGER CIRCULAR BUFFER VARIABLES  */

struct afsmon_cm_Results_list {
    struct xstat_cm_ProbeResults *cmResults;	/* ptr to results struct */
    int empty;			/* cmResults empty ? */
    struct afsmon_cm_Results_list *next;
};

struct afsmon_cm_Results_CBuffer {
    int probeNum;		/* probe number of entries in this slot */
    struct afsmon_cm_Results_list *list;	/* ptr to list of results */
};

/* buffer for CM probe results */
struct afsmon_cm_Results_CBuffer *afsmon_cm_ResultsCB;

int afsmon_cm_curr_CBindex = 0;	/* current cm CB slot */


/* Probe number variables. The current probe number is incremented 
when the first probe from a new probe cycle is received. The prev probe
number is incremented when the last probe of the current cycle is
received. This difference is because of the purpose for which these
counters are used */

int afsmon_cm_curr_probeNum = 1;	/* current cm probe number */
int afsmon_cm_prev_probeNum = 0;	/* previous cm probe number */


/* Structures to hold FS & CM results in string format(suitable for display ) */

/* ptr to array holding the results of FS probes in ascii format */
	/* for current probe cycle */
struct fs_Display_Data *curr_fsData = (struct fs_Display_Data *)0;
	/* for previous probe cycle */
struct fs_Display_Data *prev_fsData = (struct fs_Display_Data *)0;


/* ptr to array holding the results of CM probes in ascii format */
	/* for current probe cycle */
struct cm_Display_Data *curr_cmData = (struct cm_Display_Data *)0;
	/* for previous probe cycle */
struct cm_Display_Data *prev_cmData = (struct cm_Display_Data *)0;


/* EXTERN DEFINITIONS */

extern struct hostent *hostutil_GetHostByName();



/* routines from afsmon-output.c */
extern int afsmon_fsOutput();
extern int afsmon_cmOutput();

/* file server and cache manager variable names (from afsmon_labels.h) */
extern char *fs_varNames[];
extern char *cm_varNames[];

/* GTX & MISC VARIABLES */

/* afsmonitor window */
extern struct gwin *afsmon_win;

/* current page number in the overview frame */
extern int ovw_currPage;

/* number of FS alerts and number of hosts on FS alerts */
int num_fs_alerts;
int numHosts_onfs_alerts;

/* number of CM alerts and number of hosts on FS alerts */
int num_cm_alerts;
int numHosts_oncm_alerts;

/* flag to indicate that atleast one probe cycle has completed and 
data is available for updating the display */
extern fs_Data_Available;
extern cm_Data_Available;

extern int gtx_initialized;	/* gtx initialized ? */

/* This array contains the indices of the file server data items that
are to be displayed on the File Servers screen. For example, suppose the
user wishes to display only the vcache statistics then the following array
will contain indices 2 to 14 corresponding to the position of the
vcache data items in the fs_varNames[] array. If the config file contains
no "show fs .." directives, it will contain the indices of all the 
items in the fs_varNames[] array */

short fs_Display_map[XSTAT_FS_FULLPERF_RESULTS_LEN];
int fs_DisplayItems_count = 0;	/* number of items to display */
int fs_showDefault = 1;		/* show all of FS data ? */


/* same use as above for Cache Managers  */
short cm_Display_map[XSTAT_CM_FULLPERF_RESULTS_LEN];
int cm_DisplayItems_count = 0;	/* number of items to display */
int cm_showDefault = 1;		/* show all of CM data ? */

extern int fs_currPage;		/* current page number in the File Servers frame */
extern int fs_curr_LCol;	/* current leftmost column on display on FS frame */

extern int cm_currPage;		/* current page number in the Cache Managers frame */
extern int cm_curr_LCol;	/* current leftmost column on display on CM frame */

/* File server and Cache manager data is classified into sections & 
groups to help the user choose what he wants displayed */
extern char *fs_categories[];	/* file server data category names */
extern char *cm_categories[];	/* cache manager data category names */



#ifndef HAVE_STRCASESTR
/*	
        strcasestr(): Return first occurence of pattern s2 in s1, case 
	insensitive. 

	This routine is required since I made pattern matching of the
	config file to be case insensitive. 
*/

char *
strcasestr(s1, s2)
     char *s1;
     char *s2;
{
    char *ptr;
    int len1, len2;

    len1 = strlen(s1);
    len2 = strlen(s2);

    if (len1 < len2)
	return ((char *)NULL);

    ptr = s1;

    while (len1 >= len2 && len1 > 0) {
	if ((strncasecmp(ptr, s2, len2)) == 0)
	    return (ptr);
	ptr++;
	len1--;
    }
    return ((char *)NULL);
}
#endif

struct hostent *
GetHostByName(name)
     char *name;
{
    struct hostent *he;
#ifdef AFS_SUN5_ENV
    char ip_addr[32];
#endif

    he = gethostbyname(name);
#ifdef AFS_SUN5_ENV
    /* On solaris the above does not resolve hostnames to full names */
    if (he != NULL) {
	memcpy(ip_addr, he->h_addr, he->h_length);
	he = gethostbyaddr(ip_addr, he->h_length, he->h_addrtype);
    }
#endif
    return (he);
}


/*-----------------------------------------------------------------------
 * afsmon_Exit()
 *
 * Description
 * 	Exit gracefully from the afsmonitor. Frees memory where appropriate,
 *	cleans up after gtx and closes all open file descriptors. If a user 
 *	provided threshold handler is to be exec'ed then gtx cleanup is
 *	not performed and an exec() is made instead of an exit(). 
 *
 * Returns
 *	Nothing.
 *
 * Comments 
 *	This function is called to execute a user handler only 
 *	by a child process.
 * 
 *----------------------------------------------------------------------*/

int
afsmon_Exit(a_exitVal)
     int a_exitVal;		/* exit code */
{				/* afsmon_Exit */
    static char rn[] = "afsmon_Exit";
    struct afsmon_fs_Results_list *tmp_fslist;
    struct afsmon_fs_Results_list *next_fslist;
    struct xstat_fs_ProbeResults *tmp_xstat_fsPR;
    struct afsmon_cm_Results_list *tmp_cmlist;
    struct afsmon_cm_Results_list *next_cmlist;
    struct xstat_cm_ProbeResults *tmp_xstat_cmPR;
    struct afsmon_hostEntry *curr_hostEntry;
    struct afsmon_hostEntry *prev_hostEntry;
    int i;
    int j;
    int bufslot;
    int code;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called with exit code %d\n", rn, a_exitVal);
	fflush(debugFD);
    }

    /* get out of curses first, but not if we are here to exec a threshold
     * handler. If we do, the screen gets messed up  */
    if (gtx_initialized && !exec_fsThreshHandler)
	gator_cursesgwin_cleanup(afsmon_win);

    /* print the error message buffer */
    if (errMsg[0] != '\0')
	fprintf(stderr, "%s", errMsg);
    if (errMsg1[0] != '\0')
	fprintf(stderr, "%s", errMsg1);

    /* deallocate file server circular buffers */
    if (numFS && num_bufSlots) {
	if (afsmon_debug) {
	    fprintf(debugFD, "freeing FS circular buffers ");
	    fflush(debugFD);
	}

	for (bufslot = 0; bufslot < num_bufSlots; bufslot++) {
	    if (afsmon_debug)
		fprintf(debugFD, " %d) ", bufslot);
	    if (afsmon_fs_ResultsCB[bufslot].list !=
		(struct afsmon_fs_Results_list *)0) {
		tmp_fslist = afsmon_fs_ResultsCB[bufslot].list;
		j = numFS;
		while (tmp_fslist) {
		    /* make sure we do not go astray */
		    if (--j < 0) {
			if (afsmon_debug)
			    fprintf(debugFD,
				    "[ %s ] error in deallocating fs CB\n",
				    rn);
			break;
		    }
		    next_fslist = tmp_fslist->next;
		    tmp_xstat_fsPR = tmp_fslist->fsResults;

		    if (afsmon_debug)
			fprintf(debugFD, "%d ", numFS - j);

		    /* free xstat_fs_Results data */
		    free(tmp_xstat_fsPR->data.AFS_CollData_val);
		    free(tmp_xstat_fsPR->connP);
		    free(tmp_xstat_fsPR);

		    /* free the fs list item */
		    free(tmp_fslist);
		    tmp_fslist = next_fslist;

		}		/* while fs list items in this slot */
	    }			/* if entries in this buffer slot */
	}			/* for each fs buffer slot */
	if (afsmon_debug)
	    fprintf(debugFD, "\n");
    }

    if (afsmon_debug)
	fflush(debugFD);
    /* deallocate cache manager curcular buffers */
    if (numCM && num_bufSlots) {
	if (afsmon_debug)
	    fprintf(debugFD, "freeing CM curcular buffers ");
	for (bufslot = 0; bufslot < num_bufSlots; bufslot++) {
	    if (afsmon_debug)
		fprintf(debugFD, " %d) ", bufslot);
	    if (afsmon_cm_ResultsCB[bufslot].list !=
		(struct afsmon_cm_Results_list *)0) {
		tmp_cmlist = afsmon_cm_ResultsCB[bufslot].list;
		j = numCM;
		while (tmp_cmlist) {
		    /* make sure we do not go astray */
		    if (--j < 0) {
			if (afsmon_debug)
			    fprintf(debugFD,
				    "[ %s ] error in deallocating cm CB\n",
				    rn);
			break;
		    }
		    next_cmlist = tmp_cmlist->next;
		    tmp_xstat_cmPR = tmp_cmlist->cmResults;

		    if (afsmon_debug)
			fprintf(debugFD, "%d ", numCM - j);
		    /* make sure data is ok */
		    /* Print_cm_FullPerfInfo(tmp_xstat_cmPR); */

		    /* free xstat_cm_Results data */
		    free(tmp_xstat_cmPR->data.AFSCB_CollData_val);
		    free(tmp_xstat_cmPR->connP);
		    free(tmp_xstat_cmPR);

		    /* free the cm list item */
		    free(tmp_cmlist);
		    tmp_cmlist = next_cmlist;

		}		/* while cm list items in this slot */
	    }			/* if entries in this buffer slot */
	}			/* for each cm buffer slot */
	if (afsmon_debug)
	    fprintf(debugFD, "\n");
    }


    /* deallocate FS & CM Print buffers */
    if (curr_fsData != (struct fs_Display_Data *)0) {
	if (afsmon_debug)
	    fprintf(debugFD, "Deallocating FS Print Buffers .... curr");
	free(curr_fsData);
    }
    if (prev_fsData != (struct fs_Display_Data *)0) {
	if (afsmon_debug)
	    fprintf(debugFD, ", prev \n");
	free(prev_fsData);
    }
    if (prev_cmData != (struct cm_Display_Data *)0) {
	if (afsmon_debug)
	    fprintf(debugFD, "Deallocating CM Print Buffers .... curr");
	free(curr_cmData);
    }
    if (prev_cmData != (struct cm_Display_Data *)0) {
	if (afsmon_debug)
	    fprintf(debugFD, ", prev \n");
	free(prev_cmData);
    }

    /* deallocate hostEntry lists */
    if (numFS) {
	if (afsmon_debug)
	    fprintf(debugFD, "Deallocating FS hostEntries ..");
	curr_hostEntry = FSnameList;
	for (i = 0; i < numFS; i++) {
	    prev_hostEntry = curr_hostEntry;
	    if (curr_hostEntry->thresh != NULL)
		free(curr_hostEntry->thresh);
	    free(curr_hostEntry);
	    if (afsmon_debug)
		fprintf(debugFD, " %d", i);
	    curr_hostEntry = prev_hostEntry->next;
	}
	if (afsmon_debug)
	    fprintf(debugFD, "\n");
    }
    if (numCM) {
	if (afsmon_debug)
	    fprintf(debugFD, "Deallocating CM hostEntries ..");
	curr_hostEntry = CMnameList;
	for (i = 0; i < numCM; i++) {
	    prev_hostEntry = curr_hostEntry;
	    if (curr_hostEntry->thresh != NULL)
		free(curr_hostEntry->thresh);
	    free(curr_hostEntry);
	    if (afsmon_debug)
		fprintf(debugFD, " %d", i);
	    curr_hostEntry = prev_hostEntry->next;
	}
	if (afsmon_debug)
	    fprintf(debugFD, "\n");
    }

    /* close debug file */
    if (afsmon_debug) {
	fflush(debugFD);
	fclose(debugFD);
    }

    if (exec_fsThreshHandler) {
	code = execvp(fsHandler_argv[0], fsHandler_argv);
	if (code == -1) {
	    fprintf(stderr, "execvp() of %s returned %d, errno %d\n",
		    fsHandler_argv[0], code, errno);
	    exit(-1);
	}
    }

    exit(a_exitVal);
}				/* afsmon_Exit */

/*-----------------------------------------------------------------------
 * insert_FS()
 *
 * Description:
 * 	Insert a hostname in the file server names list.
 *
 * Returns:
 *	Success: 0
 *	Failure: -1
 *----------------------------------------------------------------------*/

int
insert_FS(a_hostName)
     char *a_hostName;		/* name of cache manager to be inserted in list */
{				/* insert_FS() */
    static char rn[] = "insert_FS";	/* routine name */
    static struct afsmon_hostEntry *curr_item;
    static struct afsmon_hostEntry *prev_item;

    if (*a_hostName == '\0')
	return (-1);
    curr_item = (struct afsmon_hostEntry *)
	malloc(sizeof(struct afsmon_hostEntry));
    if (curr_item == (struct afsmon_hostEntry *)0) {
	fprintf(stderr, "Failed to allocate space for FS nameList\n");
	return (-1);
    }

    strncpy(curr_item->hostName, a_hostName, CFG_STR_LEN);
    curr_item->next = (struct afsmon_hostEntry *)0;
    curr_item->numThresh = 0;
    curr_item->thresh = NULL;

    if (FSnameList == (struct afsmon_hostEntry *)0)
	FSnameList = curr_item;
    else
	prev_item->next = curr_item;

    prev_item = curr_item;
    /*  record the address of this entry so that its threshold
     * count can be incremented during  the first pass of the config file */
    last_hostEntry = curr_item;

    return (0);
}

/*-----------------------------------------------------------------------
 * print_FS()
 *
 * Description:
 *	Debug routine.
 *	Prints the file server names linked list.
 *
 * Returns:
 *	Nothing.
 *----------------------------------------------------------------------*/
void
print_FS()
{				/* print_FS() */
    static char rn[] = "print_FS";
    struct afsmon_hostEntry *tempFS;
    struct Threshold *threshP;
    int i;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    if (afsmon_debug) {
	tempFS = FSnameList;
	fprintf(debugFD, "No of File Servers: %d\n", numFS);
	if (numFS) {
	    do {
		fprintf(debugFD, "\t %s threshCount = %d\n", tempFS->hostName,
			tempFS->numThresh);
		threshP = tempFS->thresh;
		for (i = 0; i < tempFS->numThresh; i++, threshP++)
		    fprintf(debugFD, "\t thresh (%2d) %s %s %s\n",
			    threshP->index, threshP->itemName,
			    threshP->threshVal, threshP->handler);
	    } while ((tempFS = tempFS->next) != (struct afsmon_hostEntry *)0);
	}
	fprintf(debugFD, "\t\t-----End of List-----\n");
	fflush(debugFD);
    }

}

/*-----------------------------------------------------------------------
 * insert_CM()
 *
 * Description:
 * 	Insert a hostname in the cache manager names list.
 *
 * Returns:
 *	Success: 0
 *	Failure: -1
 *----------------------------------------------------------------------*/

int
insert_CM(a_hostName)
     char *a_hostName;		/* name of cache manager to be inserted in list */
{				/* insert_CM */
    static char rn[] = "insert_CM";	/* routine name */
    static struct afsmon_hostEntry *curr_item;
    static struct afsmon_hostEntry *prev_item;

    if (*a_hostName == '\0')
	return (-1);
    curr_item = (struct afsmon_hostEntry *)
	malloc(sizeof(struct afsmon_hostEntry));
    if (curr_item == (struct afsmon_hostEntry *)0) {
	fprintf(stderr, "Failed to allocate space for CM nameList\n");
	return (-1);
    }

    strncpy(curr_item->hostName, a_hostName, CFG_STR_LEN);
    curr_item->next = (struct afsmon_hostEntry *)0;
    curr_item->numThresh = 0;
    curr_item->thresh = NULL;

    if (CMnameList == (struct afsmon_hostEntry *)0)
	CMnameList = curr_item;
    else
	prev_item->next = curr_item;

    prev_item = curr_item;
    /* side effect. note the address of this entry so that its threshold
     * count can be incremented during  the first pass of the config file */
    last_hostEntry = curr_item;

    return (0);
}


/*-----------------------------------------------------------------------
 * print_CM()
 *
 * Description:
 *	Debug routine.
 *	Prints the cache manager names linked list.
 *
 * Returns:
 *	Nothing.
 *----------------------------------------------------------------------*/
int
print_CM()
{				/* print_CM() */
    static char rn[] = "print_CM";
    struct afsmon_hostEntry *tempCM;
    struct Threshold *threshP;
    int i;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    if (afsmon_debug) {
	tempCM = CMnameList;
	fprintf(debugFD, "No of Cache Managers: %d\n", numCM);
	if (numCM) {
	    do {
		fprintf(debugFD, "\t %s threshCount = %d\n", tempCM->hostName,
			tempCM->numThresh);
		threshP = tempCM->thresh;
		for (i = 0; i < tempCM->numThresh; i++, threshP++)
		    fprintf(debugFD, "\t thresh (%2d) %s %s %s\n",
			    threshP->index, threshP->itemName,
			    threshP->threshVal, threshP->handler);
	    } while ((tempCM = tempCM->next) != (struct afsmon_hostEntry *)0);
	}
	fprintf(debugFD, "\t\t-----End of List-----\n");
    }
    return (0);
}				/* print_CM() */



/*-----------------------------------------------------------------------
 * parse_hostEntry()
 *
 * Description:
 *	Parse the host entry line in the config file. Check the syntax,
 *	and inserts the host name in the FS ot CM linked list. Also
 *	remember if this entry was an fs or cm & the ptr to its hostEntry
 *	structure. The threshold entries in the config file are dependent
 *	on their position relative to the hostname entries. Hence it is
 *	required to remember the names of the last file server and cache
 *	manager entries that were processed.
 *
 * Returns:
 *	Success: 0
 *	Failure: -1
 *
 *----------------------------------------------------------------------*/

int
parse_hostEntry(a_line)
     char *a_line;
{				/* parse_hostEntry */

    static char rn[] = "parse_hostEntry";	/* routine name */
    char opcode[CFG_STR_LEN];	/* specifies type of config entry */
    char arg1[CFG_STR_LEN];	/* hostname or qualifier (fs/cm?)  */
    char arg2[CFG_STR_LEN];	/* threshold variable */
    char arg3[CFG_STR_LEN];	/* threshold value */
    char arg4[CFG_STR_LEN];	/* user's handler  */
    struct hostent *he;		/* host entry */

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_line = %s\n", rn, a_line);
	fflush(debugFD);
    }

    /* break it up */
    opcode[0] = 0;
    arg1[0] = 0;
    arg2[0] = 0;
    arg3[0] = 0;
    arg4[0] = 0;
    sscanf(a_line, "%s %s %s %s %s", opcode, arg1, arg2, arg3, arg4);
    /* syntax is "opcode hostname" */
    if ((strlen(arg2)) != 0) {
	fprintf(stderr, "[ %s ] Extraneous characters at end of line\n", rn);
	return (-1);
    }

    /* good host ? */
    he = GetHostByName(arg1);
    if (he == NULL) {
	fprintf(stderr, "[ %s ] Unable to resolve hostname %s\n", rn, arg1);
	return (-1);
    }

    if ((strcasecmp(opcode, "fs")) == 0) {
	/* use the complete host name to insert in the file server names list */
	insert_FS(he->h_name);
	/* note that last host entry in the config file was fs */
	lastHostType = 1;
	numFS++;
	/* threholds are not global anymore */
	if (global_ThreshFlag)
	    global_ThreshFlag = 0;
    } else if ((strcasecmp(opcode, "cm")) == 0) {
	/* use the complete host name to insert in the CM names list */
	insert_CM(he->h_name);
	/* last host entry in the config file was cm */
	lastHostType = 2;
	numCM++;
	/* threholds are not global anymore */
	if (global_ThreshFlag)
	    global_ThreshFlag = 0;
    } else
	return (-1);

    return (0);
}

/*-----------------------------------------------------------------------
 * parse_threshEntry()
 *
 * Description
 *	Parse the threshold entry line in the config file. This function is
 *	called in the the first pass of the config file. It checks the syntax 
 *	of the config lines and verifies their positional validity - eg.,
 *	a cm threshold cannot appear after a fs hostname entry, etc.
 *	It also counts the thresholds applicable to each host.
 *
 * Returns
 *	Success: 0
 *	Failure: -1
 *
 *----------------------------------------------------------------------*/

int
parse_threshEntry(a_line)
     char *a_line;
{				/* parse_threshEntry */
    static char rn[] = "parse_threshEntry";	/* routine name */
    char opcode[CFG_STR_LEN];	/* specifies type of config entry */
    char arg1[CFG_STR_LEN];	/* hostname or qualifier (fs/cm?)  */
    char arg2[CFG_STR_LEN];	/* threshold variable */
    char arg3[CFG_STR_LEN];	/* threshold value */
    char arg4[CFG_STR_LEN];	/* user's handler  */
    char arg5[CFG_STR_LEN];	/* junk characters */

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_line = %s\n", rn, a_line);
	fflush(debugFD);
    }

    /* break it up */
    opcode[0] = 0;
    arg1[0] = 0;
    arg2[0] = 0;
    arg3[0] = 0;
    arg4[0] = 0;
    arg5[0] = 0;
    sscanf(a_line, "%s %s %s %s %s %s", opcode, arg1, arg2, arg3, arg4, arg5);

    /* syntax is "thresh fs/cm variable_name threshold_value [handler] " */
    if (((strlen(arg1)) == 0) || ((strlen(arg2)) == 0)
	|| ((strlen(arg3)) == 0)) {
	fprintf(stderr, "[ %s ] Incomplete line\n", rn);
	return (-1);
    }
    if (strlen(arg3) > THRESH_VAR_LEN - 2) {
	fprintf(stderr, "[%s ] threshold value too long\n", rn);
	return (-1);
    }

    if ((strcasecmp(arg1, "fs")) == 0) {
	switch (lastHostType) {
	case 0:		/* its a global threshold */
	    global_fsThreshCount++;
	    break;
	case 1:		/* inc thresh count of last file server */
	    last_hostEntry->numThresh++;
	    break;
	case 2:
	    fprintf(stderr,
		    "[ %s ] A threshold for a File Server cannot be placed after a Cache Manager host entry in the config file \n",
		    rn);
	    return (-1);
	default:
	    fprintf(stderr, "[ %s ] Programming error 1\n", rn);
	    return (-1);
	}
    } else if ((strcasecmp(arg1, "cm")) == 0) {
	switch (lastHostType) {
	case 0:		/* its a global threshold */
	    global_cmThreshCount++;
	    break;
	case 2:		/* inc thresh count of last cache manager */
	    last_hostEntry->numThresh++;
	    break;
	case 1:
	    fprintf(stderr,
		    "[ %s ] A threshold for a Cache Manager cannot be placed after a File Server host entry in the config file \n",
		    rn);
	    return (-1);
	default:
	    fprintf(stderr, "[ %s ] Programming error 2\n", rn);
	    return (-1);
	}
    } else {
	fprintf(stderr,
		"[ %s ] Syntax error. Second argument should be \"fs\" or \"cm\" \n",
		rn);
	return (-1);
    }

    return (0);
}				/* parse_threshEntry */


/*-----------------------------------------------------------------------
 * store_threshold()
 *
 * Description
 *	The thresholds applicable to each host machine are stored in the
 *	FSnameList and CMnameList. Threshold entries in the config file are
 *	context sensitive. The host to which this threshold is applicable
 *	is pointed to by last_fsHost (for file servers) and last_cmHost
 *	for cache managers. For global thresholds the info is recorded for
 *	all the hosts. This function is called in the second pass of the
 *	config file. In the first pass a count of the number of global
 *	thresholds is determined and this information is used in this 
 *	routine. If threshold entries are duplicated the first entry is
 *	overwritten.
 *	Each threshold entry also has an index field. This is a positional
 *	index to the corresponding variable in the prev_[fs/cm]Data arrays.
 *	This makes it easy to check the threshold for overflow.
 *
 * Returns:
 *	Success: 0
 *	Failure: -1
 *----------------------------------------------------------------------*/

int
store_threshold(a_type, a_varName, a_value, a_handler)
     int a_type;		/* 1 = fs , 2 = cm */
     char *a_varName;		/* threshold name */
     char *a_value;		/* threshold value */
     char *a_handler;		/* threshold overflow handler */

{				/* store_thresholds */

    static char rn[] = "store_thresholds";	/* routine name */
    struct afsmon_hostEntry *tmp_host;	/* tmp ptr to hostEntry */
    struct afsmon_hostEntry *Header;	/* tmp ptr to hostEntry list header */
    struct Threshold *threshP;	/* tmp ptr to threshold list */
    char *hostname;
    int index;			/* index to fs_varNames or cm_varNames */
    int found;
    int done;
    int srvCount;		/* tmp count of host names */
    int *global_TC;		/* ptr to global_xxThreshCount */
    int i, j;

    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, a_type= %d, a_varName= %s, a_value= %s, a_handler=%s\n",
		rn, a_type, a_varName, a_value, a_handler);
	fflush(debugFD);
    }

    /* resolve the threshold variable name */
    found = 0;
    if (a_type == 1) {		/* fs threshold */
	for (index = 0; index < NUM_FS_STAT_ENTRIES; index++) {
	    if (strcasecmp(a_varName, fs_varNames[index]) == 0) {
		found = 1;
		break;
	    }
	}
	if (!found) {
	    fprintf(stderr, "[ %s ] Unknown FS threshold variable name %s\n",
		    rn, a_varName);
	    return (-1);
	}
	Header = FSnameList;
	srvCount = numFS;
	hostname = last_fsHost;
	global_TC = &global_fsThreshCount;
    } else if (a_type == 2) {	/* cm threshold */
	for (index = 0; index < NUM_CM_STAT_ENTRIES; index++) {
	    if (strcasecmp(a_varName, cm_varNames[index]) == 0) {
		found = 1;
		break;
	    }
	}
	if (!found) {
	    fprintf(stderr, "[ %s ] Unknown CM threshold variable name %s\n",
		    rn, a_varName);
	    return (-1);
	}
	Header = CMnameList;
	srvCount = numCM;
	hostname = last_cmHost;
	global_TC = &global_cmThreshCount;
    } else
	return (-1);



    /* if the global thresh count is not zero, place this threshold on
     * all the host entries  */

    if (*global_TC) {
	tmp_host = Header;
	for (i = 0; i < srvCount; i++) {
	    threshP = tmp_host->thresh;
	    done = 0;
	    for (j = 0; j < tmp_host->numThresh; j++) {
		if ((threshP->itemName[0] == '\0')
		    || (strcasecmp(threshP->itemName, a_varName) == 0)) {
		    strncpy(threshP->itemName, a_varName,
			    THRESH_VAR_NAME_LEN);
		    strncpy(threshP->threshVal, a_value, THRESH_VAR_LEN);
		    strcpy(threshP->handler, a_handler);
		    threshP->index = index;
		    done = 1;
		    break;
		}
		threshP++;
	    }
	    if (!done) {
		fprintf(stderr, "[ %s ] Could not insert threshold entry",
			rn);
		fprintf(stderr, "for %s in thresh list of host %s \n",
			a_varName, tmp_host->hostName);
		return (-1);
	    }
	    tmp_host = tmp_host->next;
	}
	(*global_TC)--;
	return (0);
    }

    /* it is not a global threshold, insert it in the thresh list of this
     * host only. We overwrite the global threshold if it was alread set */

    if (*hostname == '\0') {
	fprintf(stderr, "[ %s ] Programming error 3\n", rn);
	return (-1);
    }

    /* get the hostEntry that this threshold belongs to */
    tmp_host = Header;
    found = 0;
    for (i = 0; i < srvCount; i++) {
	if (strcasecmp(tmp_host->hostName, hostname) == 0) {
	    found = 1;
	    break;
	}
	tmp_host = tmp_host->next;
    }
    if (!found) {
	fprintf(stderr, "[ %s ] Unable to find host %s in %s hostEntry list",
		rn, hostname, (a_type - 1) ? "CM" : "FS");
	return (-1);
    }

    /* put this entry on the thresh list of this host, overwrite global value
     * if needed */

    threshP = tmp_host->thresh;
    done = 0;
    for (i = 0; i < tmp_host->numThresh; i++) {
	if ((threshP->itemName[0] == '\0')
	    || (strcasecmp(threshP->itemName, a_varName) == 0)) {
	    strncpy(threshP->itemName, a_varName, THRESH_VAR_NAME_LEN);
	    strncpy(threshP->threshVal, a_value, THRESH_VAR_LEN);
	    strcpy(threshP->handler, a_handler);
	    threshP->index = index;
	    done = 1;
	    break;
	}
	threshP++;
    }

    if (!done) {
	fprintf(stderr,
		"[ %s ] Unable to insert threshold %s for %s host %s\n", rn,
		a_varName, (a_type - 1) ? "CM" : "FS", tmp_host->hostName);
	return (-1);
    }

    return (0);

}				/* store_thresholds */


/*-----------------------------------------------------------------------
 * parse_showEntry()
 *
 * Description:
 *	This function process a "show" entry in the config file. A "show"
 *	entry specifies what statistics the user wants to see. File
 *	server and Cache Manager data is divided into sections. Each section
 *	is made up of one or more groups. If a group name is specified only
 *	those statistics under that group are shown. If a section name is
 *	specified all the groups under this section are shown.
 *	Data as obtained from the xstat probes is considered to be ordered.
 *	This data is mapped to the screen thru fs_Display_map[] and
 *	cm_Display_map[]. This routine parses the "show" entry against the
 * 	section/group names in the [fs/cm]_categories[] array. If there is
 *	no match it tries to match it against a variable name in 
 *	[fs/cm]_varNames[] array. In each case the corresponding indices to
 * 	the data is the [fs/cm]_displayInfo[] is recorded. 
 *
 * Returns:
 *	Success: 0
 *	Failure: -1 (invalid entry)
 *		 > -1 (programming error)
 *----------------------------------------------------------------------*/

int
parse_showEntry(a_line)
     char *a_line;
{				/* parse_showEntry */
    static char rn[] = "parse_showEntry";
    char opcode[CFG_STR_LEN];	/* specifies type of config entry */
    char arg1[CFG_STR_LEN];	/* show fs or cm entry ? */
    char arg2[CFG_STR_LEN];	/* what we gotta show  */
    char arg3[CFG_STR_LEN];	/* junk */
    char catName[CFG_STR_LEN];	/* for category names */
    int numGroups;		/* number of groups in a section */
    int fromIdx;
    int toIdx;
    int found;
    int idx = 0;		/* index to fs_categories[] */
    int i;
    int j;


    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_line= %s\n", rn, a_line);
	fflush(debugFD);
    }
    opcode[0] = 0;
    arg1[0] = 0;
    arg2[0] = 0;
    arg3[0] = 0;
    sscanf(a_line, "%s %s %s %s", opcode, arg1, arg2, arg3);

    if (arg3[0] != '\0') {
	fprintf(stderr, "[ %s ] Extraneous characters at end of line\n", rn);
	return (-1);
    }

    if ((strcasecmp(arg1, "fs") != 0) && (strcasecmp(arg1, "cm") != 0)) {
	fprintf(stderr,
		"[ %s ] Second argument of \"show\" directive should be \"fs\" or \"cm\" \n",
		rn);
	return (-1);
    }

    /* Each entry can either be a variable name or a section/group name. Variable
     * names are listed in xx_varNames[] and section/group names in xx_categories[].
     * The section/group names in xx_categiries[] also give the starting/ending
     * indices of the variables belonging to that section/group. These indices
     * are stored in order in xx_Display_map[] and displayed to the screen in that
     * order. */

    /* To handle duplicate "show" entries we keep track of what what we have
     * already marked to show in the xx_showFlags[] */

    if (strcasecmp(arg1, "fs") == 0) {	/* its a File Server entry */

	/* mark that we have to show only what the user wants */
	fs_showDefault = 0;

	/* if it is a section/group name, find it in the fs_categories[] array */

	found = 0;
	if (strcasestr(arg2, "_section") != (char *)NULL
	    || strcasestr(arg2, "_group") != (char *)NULL) {
	    idx = 0;
	    while (idx < FS_NUM_DATA_CATEGORIES) {
		sscanf(fs_categories[idx], "%s %d %d", catName, &fromIdx,
		       &toIdx);
		idx++;
		if (strcasecmp(arg2, catName) == 0) {
		    found = 1;
		    break;
		}
	    }

	    if (!found) {	/* typo in section/group name */
		fprintf(stderr,
			"[ %s ] Could not find section/group name %s\n", rn,
			arg2);
		return (-1);
	    }
	}

	/* if it is a group name, read its start/end indices and fill in the
	 * fs_Display_map[]. */

	if (strcasestr(arg2, "_group") != (char *)NULL) {

	    if (fromIdx < 0 || toIdx < 0 || fromIdx > NUM_FS_STAT_ENTRIES
		|| toIdx > NUM_FS_STAT_ENTRIES)
		return (-2);
	    for (j = fromIdx; j <= toIdx; j++) {
		if (!fs_showFlags[j]) {
		    fs_Display_map[fs_DisplayItems_count] = j;
		    fs_DisplayItems_count++;
		    fs_showFlags[j] = 1;
		}
		if (fs_DisplayItems_count > NUM_FS_STAT_ENTRIES) {
		    fprintf(stderr, "[ %s ] fs_DisplayItems_count ovf\n", rn);
		    return (-3);
		}
	    }
	} else
	    /* if it is a section name, get the count of number of groups in it and
	     * for each group fill in the start/end indices in the fs_Display_map[] */

	if (strcasestr(arg2, "_section") != (char *)NULL) {
	    /* fromIdx is actually the number of groups in thi section */
	    numGroups = fromIdx;
	    /* for each group in section */
	    while (idx < FS_NUM_DATA_CATEGORIES && numGroups) {
		sscanf(fs_categories[idx], "%s %d %d", catName, &fromIdx,
		       &toIdx);

		if (strcasestr(catName, "_group") != NULL) {
		    if (fromIdx < 0 || toIdx < 0
			|| fromIdx > NUM_FS_STAT_ENTRIES
			|| toIdx > NUM_FS_STAT_ENTRIES)
			return (-4);
		    for (j = fromIdx; j <= toIdx; j++) {
			if (!fs_showFlags[j]) {
			    fs_Display_map[fs_DisplayItems_count] = j;
			    fs_DisplayItems_count++;
			    fs_showFlags[j] = 1;
			}
			if (fs_DisplayItems_count > NUM_FS_STAT_ENTRIES) {
			    fprintf(stderr,
				    "[ %s ] fs_DisplayItems_count ovf\n", rn);
			    return (-5);
			}
		    }
		} else {
		    fprintf(stderr, "[ %s ] Error parsing groups for %s\n",
			    rn, arg2);
		    return (-6);
		}
		idx++;
		numGroups--;
	    }			/* for each group in section */


	} else {		/* it is a variable name */

	    for (i = 0; i < NUM_FS_STAT_ENTRIES; i++) {
		if (strcasecmp(arg2, fs_varNames[i]) == 0) {
		    if (!fs_showFlags[i]) {
			fs_Display_map[fs_DisplayItems_count] = i;
			fs_DisplayItems_count++;
			fs_showFlags[i] = 1;
		    }
		    if (fs_DisplayItems_count >= NUM_FS_STAT_ENTRIES) {
			fprintf(stderr, "[ %s ] fs_DisplayItems_count ovf\n",
				rn);
			return (-25);
		    }
		    found = 1;
		}
	    }
	    if (!found) {	/* typo in section/group name */
		fprintf(stderr, "[ %s ] Could not find variable name %s\n",
			rn, arg2);
		return (-1);
	    }
	}			/* its a variable name */

    }

    /* it is an fs entry */
    if (strcasecmp(arg1, "cm") == 0) {	/* its a Cache Manager entry */


	/* mark that we have to show only what the user wants */
	cm_showDefault = 0;

	/* if it is a section/group name, find it in the cm_categories[] array */

	found = 0;
	if (strcasestr(arg2, "_section") != (char *)NULL
	    || strcasestr(arg2, "_group") != (char *)NULL) {
	    idx = 0;
	    while (idx < CM_NUM_DATA_CATEGORIES) {
		sscanf(cm_categories[idx], "%s %d %d", catName, &fromIdx,
		       &toIdx);
		idx++;
		if (strcasecmp(arg2, catName) == 0) {
		    found = 1;
		    break;
		}
	    }

	    if (!found) {	/* typo in section/group name */
		fprintf(stderr,
			"[ %s ] Could not find section/group name %s\n", rn,
			arg2);
		return (-1);
	    }
	}

	/* if it is a group name, read its start/end indices and fill in the
	 * cm_Display_map[]. */

	if (strcasestr(arg2, "_group") != (char *)NULL) {

	    if (fromIdx < 0 || toIdx < 0 || fromIdx > NUM_CM_STAT_ENTRIES
		|| toIdx > NUM_CM_STAT_ENTRIES)
		return (-10);
	    for (j = fromIdx; j <= toIdx; j++) {
		if (!cm_showFlags[j]) {
		    cm_Display_map[cm_DisplayItems_count] = j;
		    cm_DisplayItems_count++;
		    cm_showFlags[j] = 1;
		}
		if (cm_DisplayItems_count > NUM_CM_STAT_ENTRIES) {
		    fprintf(stderr, "[ %s ] cm_DisplayItems_count ovf\n", rn);
		    return (-11);
		}
	    }
	} else
	    /* if it is a section name, get the count of number of groups in it and
	     * for each group fill in the start/end indices in the cm_Display_map[] */

	if (strcasestr(arg2, "_section") != (char *)NULL) {
	    /* fromIdx is actually the number of groups in thi section */
	    numGroups = fromIdx;
	    /* for each group in section */
	    while (idx < CM_NUM_DATA_CATEGORIES && numGroups) {
		sscanf(cm_categories[idx], "%s %d %d", catName, &fromIdx,
		       &toIdx);

		if (strcasestr(catName, "_group") != NULL) {
		    if (fromIdx < 0 || toIdx < 0
			|| fromIdx > NUM_CM_STAT_ENTRIES
			|| toIdx > NUM_CM_STAT_ENTRIES)
			return (-12);
		    for (j = fromIdx; j <= toIdx; j++) {
			if (!cm_showFlags[j]) {
			    cm_Display_map[cm_DisplayItems_count] = j;
			    cm_DisplayItems_count++;
			    cm_showFlags[j] = 1;
			}
			if (cm_DisplayItems_count > NUM_CM_STAT_ENTRIES) {
			    fprintf(stderr,
				    "[ %s ] cm_DisplayItems_count ovf\n", rn);
			    return (-13);
			}
		    }
		} else {
		    fprintf(stderr, "[ %s ] Error parsing groups for %s\n",
			    rn, arg2);
		    return (-15);
		}
		idx++;
		numGroups--;
	    }			/* for each group in section */






	} else {		/* it is a variable name */

	    for (i = 0; i < NUM_CM_STAT_ENTRIES; i++) {
		if (strcasecmp(arg2, cm_varNames[i]) == 0) {
		    if (!cm_showFlags[i]) {
			cm_Display_map[cm_DisplayItems_count] = i;
			cm_DisplayItems_count++;
			cm_showFlags[i] = 1;
		    }
		    if (cm_DisplayItems_count >= NUM_CM_STAT_ENTRIES) {
			fprintf(stderr, "[ %s ] cm_DisplayItems_count ovf\n",
				rn);
			return (-20);
		    }
		    found = 1;
		}
	    }
	    if (!found) {	/* typo in section/group name */
		fprintf(stderr, "[ %s ] Could not find variable name %s\n",
			rn, arg2);
		return (-1);
	    }
	}			/* its a variable name */

    }
    /* it is an cm entry */
    return (0);


}				/* parse_showEntry */


/*-----------------------------------------------------------------------
 * process_config_file()
 *
 * Description:
 *	Parse config file entries in two passes. In the first pass: 
 *		- the syntax of all the entries is checked
 *		- host names are noted and the FSnamesList and CMnamesList 
 *		  constructed. 
 *		- a count of the global thresholds and local thresholds of 
 *		  each host are counted. 
 *		- "show" entries are processed.
 *	In the second pass:
 *		- thresholds are stored 
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor showing error and line.
 *----------------------------------------------------------------------*/

int
process_config_file(a_config_filename)
     char *a_config_filename;
{				/* process_config_file() */
    static char rn[] = "process_config_file";	/* routine name */
    FILE *configFD;		/* config file descriptor */
    char line[4 * CFG_STR_LEN];	/* a line of config file */
    char opcode[CFG_STR_LEN];	/* specifies type of config entry */
    char arg1[CFG_STR_LEN];	/* hostname or qualifier (fs/cm?)  */
    char arg2[CFG_STR_LEN];	/* threshold variable */
    char arg3[CFG_STR_LEN];	/* threshold value */
    char arg4[CFG_STR_LEN];	/* user's handler  */
    struct afsmon_hostEntry *curr_host;
    struct hostent *he;		/* hostentry to resolve host name */
    char *handlerPtr;		/* ptr to pass theresh handler string */
    int code = 0;		/* error code */
    int linenum = 0;		/* config file line number */
    int threshCount;		/* count of thresholds for each server */
    int error_in_config;	/* syntax errors in config file  ?? */
    int i;
    int numBytes;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_config_filename= %s\n", rn,
		a_config_filename);
	fflush(debugFD);
    }

    /* open config file */

    configFD = fopen(a_config_filename, "r");
    if (configFD == (FILE *) 0) {
	fprintf(stderr, "Failed to open config file %s \n",
		a_config_filename);
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Failed to open config file %s \n", rn,
		    a_config_filename);
	}
	afsmon_Exit(5);
    }


    /* parse config file */

    /* We process the config file in two passes. In the first pass we check
     * for correct syntax and for valid entries and also keep count of the
     * number of servers and thresholds to monitor. This the data strctures
     * can be arrays instead of link lists since we would know their sizes. */

    /* First Pass */

    numFS = 0;
    numCM = 0;
    threshCount = 0;
    error_in_config = 0;	/* flag to note if config file has syntax errors */

    while ((fgets(line, CFG_STR_LEN, configFD)) != NULL) {
	opcode[0] = 0;
	arg1[0] = 0;
	arg2[0] = 0;
	arg3[0] = 0;
	arg4[0] = 0;
	sscanf(line, "%s %s %s %s %s", opcode, arg1, arg2, arg3, arg4);
	linenum++;
	/* skip blank lines and comment lines */
	if ((strlen(opcode) == 0) || line[0] == '#')
	    continue;

	if ((strcasecmp(opcode, "fs") == 0)
	    || (strcasecmp(opcode, "cm")) == 0) {
	    code = parse_hostEntry(line);
	} else if ((strcasecmp(opcode, "thresh")) == 0) {
	    code = parse_threshEntry(line);
	} else if ((strcasecmp(opcode, "show")) == 0) {
	    code = parse_showEntry(line);
	} else {
	    fprintf(stderr, "[ %s ] Unknown opcode %s\n", rn, opcode);
	    code = 1;
	}

	if (code) {
	    fprintf(stderr, "[ %s ] Error in line:\n %d: %s\n", rn, linenum,
		    line);
	    error_in_config = 1;
	}
    }

    if (error_in_config)
	afsmon_Exit(10);

    if (afsmon_debug) {
	fprintf(debugFD, "Global FS thresholds count = %d\n",
		global_fsThreshCount);
	fprintf(debugFD, "Global CM thresholds count = %d\n",
		global_cmThreshCount);
	fflush(debugFD);
    }

    /* the threshold count of all hosts in increased by 1 for each global
     * threshold. If one of the hosts has a local threshold for the same 
     * variable it would end up being counted twice. whats a few bytes of memory
     * wasted anyway ? */

    if (global_fsThreshCount) {
	curr_host = FSnameList;
	for (i = 0; i < numFS; i++) {
	    curr_host->numThresh += global_fsThreshCount;
	    curr_host = curr_host->next;
	}
    }
    if (global_cmThreshCount) {
	curr_host = CMnameList;
	for (i = 0; i < numCM; i++) {
	    curr_host->numThresh += global_cmThreshCount;
	    curr_host = curr_host->next;
	}
    }


    /* make sure we have something to monitor */
    if (numFS == 0 && numCM == 0) {
	fprintf(stderr,
		"\nConfig file must specify atleast one File Server or Cache Manager host to monitor.\n");
	fclose(configFD);
	afsmon_Exit(15);
    }

    /* Second Pass */

    fseek(configFD, 0, 0);	/* seek to the beginning */


    /* allocate memory for threshold lists */
    curr_host = FSnameList;
    for (i = 0; i < numFS; i++) {
	if (curr_host->hostName[0] == '\0') {
	    fprintf(stderr, "[ %s ] Programming error 4\n", rn);
	    afsmon_Exit(20);
	}
	if (curr_host->numThresh) {
	    numBytes = curr_host->numThresh * sizeof(struct Threshold);
	    curr_host->thresh = (struct Threshold *)malloc(numBytes);
	    if (curr_host->thresh == NULL) {
		fprintf(stderr, "[ %s ] Memory Allocation error 1", rn);
		afsmon_Exit(25);
	    }
	    memset(curr_host->thresh, 0, numBytes);
	}
	curr_host = curr_host->next;;
    }

    curr_host = CMnameList;
    for (i = 0; i < numCM; i++) {
	if (curr_host->hostName[0] == '\0') {
	    fprintf(stderr, "[ %s ] Programming error 5\n", rn);
	    afsmon_Exit(30);
	}
	if (curr_host->numThresh) {
	    numBytes = curr_host->numThresh * sizeof(struct Threshold);
	    curr_host->thresh = (struct Threshold *)malloc(numBytes);
	    if (curr_host->thresh == NULL) {
		fprintf(stderr, "[ %s ] Memory Allocation error 2", rn);
		afsmon_Exit(35);
	    }
	    memset(curr_host->thresh, 0, numBytes);
	}
	curr_host = curr_host->next;;
    }


    opcode[0] = 0;
    arg1[0] = 0;
    arg2[0] = 0;
    arg3[0] = 0;
    arg4[0] = 0;
    last_fsHost[0] = '\0';
    last_cmHost[0] = '\0';
    linenum = 0;
    while ((fgets(line, CFG_STR_LEN, configFD)) != NULL) {
	opcode[0] = 0;
	arg1[0] = 0;
	arg2[0] = 0;
	arg3[0] = 0;
	arg4[0] = 0;
	sscanf(line, "%s %s %s %s %s", opcode, arg1, arg2, arg3, arg4);
	linenum++;

	/* if we have a host entry, remember the host name */
	if (strcasecmp(opcode, "fs") == 0) {
	    he = GetHostByName(arg1);
	    strncpy(last_fsHost, he->h_name, HOST_NAME_LEN);
	} else if (strcasecmp(opcode, "cm") == 0) {
	    he = GetHostByName(arg1);
	    strncpy(last_cmHost, he->h_name, HOST_NAME_LEN);
	} else if (strcasecmp(opcode, "thresh") == 0) {
	    /* if we have a threshold handler it may have arguments
	     * and the sscanf() above would not get them, so do the 
	     * following */
	    if (strlen(arg4)) {
		handlerPtr = line;
		/* now skip over 4 words - this is done by first
		 * skipping leading blanks then skipping a word */
		for (i = 0; i < 4; i++) {
		    while (isspace(*handlerPtr))
			handlerPtr++;
		    while (!isspace(*handlerPtr))
			handlerPtr++;
		}
		while (isspace(*handlerPtr))
		    handlerPtr++;
		/* we how have a pointer to the start of the handler
		 * name & args */
	    } else
		handlerPtr = arg4;	/* empty string */


	    if (strcasecmp(arg1, "fs") == 0)
		code = store_threshold(1,	/* 1 = fs */
				       arg2, arg3, handlerPtr);

	    else if (strcasecmp(arg1, "cm") == 0)
		code = store_threshold(2,	/* 2 = fs */
				       arg2, arg3, handlerPtr);

	    else {
		fprintf(stderr, "[ %s ] Programming error 6\n", rn);
		afsmon_Exit(40);
	    }
	    if (code) {
		fprintf(stderr, "[ %s ] Failed to store threshold\n", rn);
		fprintf(stderr, "[ %s ] Error processing line:\n%d: %s", rn,
			linenum, line);
		afsmon_Exit(45);
	    }
	}
    }


    fclose(configFD);
    return (0);
}

/*-----------------------------------------------------------------------
 * Print_FS_CB
 *
 * Description:
 *	Debug routine.
 *	Print the File Server circular buffer.
 *
 * Returns:
 *	Nothing.
 *----------------------------------------------------------------------*/

void
Print_FS_CB()
{				/* Print_FS_CB() */

    struct afsmon_fs_Results_list *fslist;
    int i;
    int j;

    /* print valid info in the fs CB */

    if (afsmon_debug) {
	fprintf(debugFD,
		"==================== FS Buffer ========================\n");
	fprintf(debugFD, "afsmon_fs_curr_CBindex = %d\n",
		afsmon_fs_curr_CBindex);
	fprintf(debugFD, "afsmon_fs_curr_probeNum = %d\n\n",
		afsmon_fs_curr_probeNum);

	for (i = 0; i < num_bufSlots; i++) {
	    fprintf(debugFD, "\t--------- slot %d ----------\n", i);
	    fslist = afsmon_fs_ResultsCB[i].list;
	    j = 0;
	    while (j < numFS) {
		if (!fslist->empty) {
		    fprintf(debugFD, "\t %d) probeNum = %d host = %s", j,
			    fslist->fsResults->probeNum,
			    fslist->fsResults->connP->hostName);
		    if (fslist->fsResults->probeOK)
			fprintf(debugFD, " NOTOK\n");
		    else
			fprintf(debugFD, " OK\n");
		} else
		    fprintf(debugFD, "\t %d) -- empty --\n", j);
		fslist = fslist->next;
		j++;
	    }
	    if (fslist != (struct afsmon_fs_Results_list *)0)
		fprintf(debugFD, "dangling last next ptr fs CB\n");
	}
    }
}				/* Print_FS_CB() */

/*-----------------------------------------------------------------------
 * save_FS_results_inCB()
 *
 * Description:
 *	Saves the results of the latest FS probe in the fs circular
 * 	buffers. If the current probe cycle is in progress the contents
 * 	of xstat_fs_Results are copied to the end of the list of results
 *	in the current slot (pointed to by afsmon_fs_curr_CBindex). If
 * 	a new probe cycle has started the next slot in the circular buffer
 *	is initialized and the results copied. Note that the Rx related
 *	information available in xstat_fs_Results is not copied.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/
int
save_FS_results_inCB(a_newProbeCycle)
     int a_newProbeCycle;	/* start of a new probe cycle ? */

{				/* save_FS_results_inCB() */
    static char rn[] = "save_FS_results_inCB";	/* routine name */
    struct afsmon_fs_Results_list *tmp_fslist_item;	/* temp fs list item */
    struct xstat_fs_ProbeResults *tmp_fsPR;	/* temp ptr */
    int i;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_newProbeCycle= %d\n", rn,
		a_newProbeCycle);
	fflush(debugFD);
    }


    /* If a new probe cycle started, mark the list in the current buffer
     * slot empty for resuse. Note that afsmon_fs_curr_CBindex was appropriately
     * incremented in afsmon_FS_Handler() */

    if (a_newProbeCycle) {
	tmp_fslist_item = afsmon_fs_ResultsCB[afsmon_fs_curr_CBindex].list;
	for (i = 0; i < numFS; i++) {
	    tmp_fslist_item->empty = 1;
	    tmp_fslist_item = tmp_fslist_item->next;
	}
    }

    /* locate last unused item in list */
    tmp_fslist_item = afsmon_fs_ResultsCB[afsmon_fs_curr_CBindex].list;
    for (i = 0; i < numFS; i++) {
	if (tmp_fslist_item->empty)
	    break;
	tmp_fslist_item = tmp_fslist_item->next;
    }

    /* if we could not find one we have an inconsistent list */
    if (!tmp_fslist_item->empty) {
	fprintf(stderr,
		"[ %s ] list inconsistency 1. unable to find an empty slot to store results of probenum %d of %s\n",
		rn, xstat_fs_Results.probeNum,
		xstat_fs_Results.connP->hostName);
	afsmon_Exit(50);
    }

    tmp_fsPR = tmp_fslist_item->fsResults;

    /* copy hostname and probe number and probe time and probe status.
     * if the probe failed return now */

    memcpy(tmp_fsPR->connP->hostName, xstat_fs_Results.connP->hostName,
	   sizeof(xstat_fs_Results.connP->hostName));
    tmp_fsPR->probeNum = xstat_fs_Results.probeNum;
    tmp_fsPR->probeTime = xstat_fs_Results.probeTime;
    tmp_fsPR->probeOK = xstat_fs_Results.probeOK;
    if (xstat_fs_Results.probeOK) {	/* probeOK = 1 => notOK */
	/* we have a nonempty results structure so mark the list item used */
	tmp_fslist_item->empty = 0;
	return (0);
    }

    /* copy connection information */
    memcpy(&(tmp_fsPR->connP->skt), &(xstat_fs_Results.connP->skt),
	   sizeof(struct sockaddr_in));

    memcpy(tmp_fsPR->connP->hostName, xstat_fs_Results.connP->hostName,
	   sizeof(xstat_fs_Results.connP->hostName));
    tmp_fsPR->collectionNumber = xstat_fs_Results.collectionNumber;

    /* copy the probe data information */
    tmp_fsPR->data.AFS_CollData_len = xstat_fs_Results.data.AFS_CollData_len;
    memcpy(tmp_fsPR->data.AFS_CollData_val,
	   xstat_fs_Results.data.AFS_CollData_val,
	   xstat_fs_Results.data.AFS_CollData_len * sizeof(afs_int32));


    /* we have a valid results structure so mark the list item used */
    tmp_fslist_item->empty = 0;

    /* Print the fs circular buffer */
    Print_FS_CB();

    return (0);
}				/* save_FS_results_inCB() */


/*-----------------------------------------------------------------------
 * fs_Results_ltoa()
 *
 * Description:
 *	The results of xstat probes are stored in a string format in 
 *	the arrays curr_fsData and prev_fsData. The information stored in
 *	prev_fsData is copied to the screen. 
 *	This function converts xstat FS results from longs to strings and 
 *	place them in the given buffer (a pointer to an item in curr_fsData).
 *	When a probe cycle completes, curr_fsData is copied to prev_fsData
 *	in afsmon_FS_Hnadler().
 *
 * Returns:
 *	Always returns 0.
 *----------------------------------------------------------------------*/

int
fs_Results_ltoa(a_fsData, a_fsResults)
     struct fs_Display_Data *a_fsData;	/* target buffer */
     struct xstat_fs_ProbeResults *a_fsResults;	/* ptr to xstat fs Results */
{				/* fs_Results_ltoa */

    static char rn[] = "fs_Results_ltoa";	/* routine name */
    afs_int32 *srcbuf;
    struct fs_stats_FullPerfStats *fullPerfP;
    int idx;
    int i, j;
    afs_int32 *tmpbuf;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_fsData= %d, a_fsResults= %d\n", rn,
		a_fsData, a_fsResults);
	fflush(debugFD);
    }

    fullPerfP = (struct fs_stats_FullPerfStats *)
	(a_fsResults->data.AFS_CollData_val);

    /* there are two parts to the xstat FS statistics 
     * - fullPerfP->overall which give the overall performance statistics, and
     * - fullPerfP->det which gives detailed info about file server operation
     * execution times */

    /* copy overall performance statistics */
    srcbuf = (afs_int32 *) & (fullPerfP->overall);
    idx = 0;
    for (i = 0; i < NUM_XSTAT_FS_AFS_PERFSTATS_LONGS; i++) {
	sprintf(a_fsData->data[idx], "%d", *srcbuf);
	idx++;
	srcbuf++;
    }

    /* copy epoch */
    srcbuf = (afs_int32 *) & (fullPerfP->det.epoch);
    sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* epoch */
    idx++;

    /* copy fs operation timing */

    srcbuf = (afs_int32 *) (fullPerfP->det.rpcOpTimes);

    for (i = 0; i < FS_STATS_NUM_RPC_OPS; i++) {
	sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* numOps */
	idx++;
	srcbuf++;
	sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* numSuccesses */
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sum time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sqr time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* min time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* max time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
    }

    /* copy fs transfer timings */

    srcbuf = (afs_int32 *) (fullPerfP->det.xferOpTimes);
    for (i = 0; i < FS_STATS_NUM_XFER_OPS; i++) {
	sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* numOps */
	idx++;
	srcbuf++;
	sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* numSuccesses */
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sum time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sqr time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* min time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* max time */
	sprintf(a_fsData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* sum bytes */
	idx++;
	srcbuf++;
	sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* min bytes */
	idx++;
	srcbuf++;
	sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* max bytes */
	idx++;
	srcbuf++;
	for (j = 0; j < FS_STATS_NUM_XFER_BUCKETS; j++) {
	    sprintf(a_fsData->data[idx], "%d", *srcbuf);	/* bucket[j] */
	    idx++;
	    srcbuf++;
	}
    }

    return (0);
}				/* fs_Results_ltoa */



/*-----------------------------------------------------------------------
 * execute_thresh_handler()
 *
 * Description:
 *	Execute a threshold handler. An agrv[] array of pointers is 
 *	constructed from the given data. A child process is forked 
 *	which immediately calls afsmon_Exit() with indication that a
 *	threshold handler is to be exec'ed insted of exiting. 
 *
 * Returns:
 *	Success: 0
 *	Failure: Afsmonitor exits if threshold handler has more than 20 args.
 *----------------------------------------------------------------------*/

int
execute_thresh_handler(a_handler, a_hostName, a_hostType, a_threshName,
		       a_threshValue, a_actValue)
     char *a_handler;		/* ptr to handler function + args */
     char *a_hostName;		/* host name for which threshold crossed */
     int a_hostType;		/* fs or cm ? */
     char *a_threshName;	/* threshold variable name */
     char *a_threshValue;	/* threshold value */
     char *a_actValue;		/* actual value */

{				/* execute_thresh_handler */

    static char rn[] = "execute_thresh_handler";
    char fileName[256];		/* file name to execute */
    int i;
    char *ch;
    int code;
    int argNum;
    int anotherArg;		/* boolean used to flag if another arg is available */

    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, a_handler= %s, a_hostName= %s, a_hostType= %d, a_threshName= %s, a_threshValue= %s, a_actValue= %s\n",
		rn, a_handler, a_hostName, a_hostType, a_threshName,
		a_threshValue, a_actValue);
	fflush(debugFD);
    }


    /* get the filename to execute - the first argument */
    sscanf(a_handler, "%s", fileName);

    /* construct the contents of *argv[] */

    strncpy(fsHandler_args[0], fileName, 256);
    strncpy(fsHandler_args[1], a_hostName, HOST_NAME_LEN);
    if (a_hostType == FS)
	strcpy(fsHandler_args[2], "fs");
    else
	strcpy(fsHandler_args[2], "cm");
    strncpy(fsHandler_args[3], a_threshName, THRESH_VAR_NAME_LEN);
    strncpy(fsHandler_args[4], a_threshValue, THRESH_VAR_LEN);
    strncpy(fsHandler_args[5], a_actValue, THRESH_VAR_LEN);


    argNum = 6;
    anotherArg = 1;
    ch = a_handler;

    /* we have already extracted the file name so skip to the 1st arg */
    while (isspace(*ch))	/* leading blanks */
	ch++;
    while (!isspace(*ch) && *ch != '\0')	/* handler filename */
	ch++;

    while (*ch != '\0') {
	if (isspace(*ch)) {
	    anotherArg = 1;
	} else if (anotherArg) {
	    anotherArg = 0;
	    sscanf(ch, "%s", fsHandler_args[argNum]);
	    argNum++;
	}
	ch++;
	if (argNum >= 20) {
	    sprintf(errMsg,
		    "Threshold handlers cannot have more than 20 arguments\n");
	    afsmon_Exit(55);
	}

    }

    fsHandler_argv[argNum] = NULL;
    for (i = 0; i < argNum; i++)
	fsHandler_argv[i] = fsHandler_args[i];


    /* exec the threshold handler */

    if (fork() == 0) {
	exec_fsThreshHandler = 1;
	code = afsmon_Exit(60);
    }

    return (0);
}				/* execute_thresh_handler */



/*-----------------------------------------------------------------------
 * check_fs_thresholds()
 *
 * Description:
 *	Checks the thresholds and sets the overflow flag. Recall that the
 *	thresholds for each host are stored in the hostEntry lists
 *	[fs/cm]nameList arrays. The probe results are passed to this 
 *	function in the display-ready format - ie., as strings. Though
 *	this looks stupid the overhead incurred in converting the strings
 *	back to floats and comparing them is insignificant and 
 *	programming is easier this way. 
 *	The threshold flags are a part of the display structures
 *	curr_[fs/cm]Data.
 *
 * Returns:
 *	0
 *----------------------------------------------------------------------*/

int
check_fs_thresholds(a_hostEntry, a_Data)
     struct afsmon_hostEntry *a_hostEntry;	/* ptr to hostEntry */
     struct fs_Display_Data *a_Data;	/* ptr to fs data to be displayed */

{				/* check_fs_thresholds */

    static char rn[] = "check_fs_thresholds";
    struct Threshold *threshP;
    double tValue;		/* threshold value */
    double pValue;		/* probe value */
    int i;
    int idx;
    int count;			/* number of thresholds exceeded */

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_hostEntry= %d, a_Data= %d\n", rn,
		a_hostEntry, a_Data);
	fflush(debugFD);
    }

    if (a_hostEntry->numThresh == 0) {
	/* store in ovf count ?? */
	return (0);
    }

    count = 0;
    threshP = a_hostEntry->thresh;
    for (i = 0; i < a_hostEntry->numThresh; i++) {
	if (threshP->itemName[0] == '\0') {
	    threshP++;
	    continue;
	}
	idx = threshP->index;	/* positional index to the data array */
	tValue = atof(threshP->threshVal);	/* threshold value */
	pValue = atof(a_Data->data[idx]);	/* probe value */
	if (pValue > tValue) {

	    if (afsmon_debug) {
		fprintf(debugFD,
			"[ %s ] fs = %s, thresh ovf for %s, threshold= %s, probevalue= %s\n",
			rn, a_hostEntry->hostName, threshP->itemName,
			threshP->threshVal, a_Data->data[idx]);
		fflush(debugFD);
	    }
	    /* if the threshold is crossed, call the handler function
	     * only if this was a transition -ie, if the threshold was
	     * crossed in the last probe too just count & keep quite! */

	    if (!a_Data->threshOvf[idx]) {
		a_Data->threshOvf[idx] = 1;
		/* call the threshold handler if provided */
		if (threshP->handler[0] != '\0') {
		    if (afsmon_debug) {
			fprintf(debugFD, "[ %s ] Calling ovf handler %s\n",
				rn, threshP->handler);
			fflush(debugFD);
		    }
		    execute_thresh_handler(threshP->handler, a_Data->hostName,
					   FS, threshP->itemName,
					   threshP->threshVal,
					   a_Data->data[idx]);
		}
	    }

	    count++;
	} else
	    /* in case threshold was previously crossed, blank it out */
	    a_Data->threshOvf[idx] = 0;
	threshP++;
    }
    /* store the overflow count */
    a_Data->ovfCount = count;

    return (0);
}				/* check_fs_thresholds */


/*-----------------------------------------------------------------------
 * save_FS_data_forDisplay()
 *
 * Description:
 *	Does the following:
 *	- if the probe number changed (ie, a cycle completed) curr_fsData
 *	is copied to prev_fsData, curr_fsData zeroed and refresh the 
 *	overview screen and file server screen with the new data.
 *	- store the results of the current probe from xstat_fs_Results into
 *	curr_fsData. ie., convert longs to strings.
 *	- check the thresholds 
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
save_FS_data_forDisplay(a_fsResults)
     struct xstat_fs_ProbeResults *a_fsResults;
{				/* save_FS_data_forDisplay */

    static char rn[] = "save_FS_data_forDisplay";	/* routine name */
    struct fs_Display_Data *curr_fsDataP;	/* tmp ptr to curr_fsData */
    struct fs_Display_Data *prev_fsDataP;	/* tmp ptr to prev_fsData */
    struct afsmon_hostEntry *curr_host;
    static int probes_Received = 0;	/* number of probes reveived in
					 * the current cycle. If this is equal to numFS we got all
					 * the data we want in this cycle and can now display it */
    int numBytes;
    int okay;
    int i;
    int code;
    int done;


    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_fsResults= %d\n", rn, a_fsResults);
	fflush(debugFD);
    }



    /* store results in the display array */

    okay = 0;
    curr_fsDataP = curr_fsData;
    for (i = 0; i < numFS; i++) {
	if ((strcasecmp(curr_fsDataP->hostName, a_fsResults->connP->hostName))
	    == 0) {
	    okay = 1;
	    break;
	}
	curr_fsDataP++;
    }

    if (!okay) {
	fprintf(stderr,
		"[ %s ] Could not insert FS probe results for host %s in fs display array\n",
		rn, a_fsResults->connP->hostName);
	afsmon_Exit(65);
    }

    /*  Check the status of the probe. If it succeeded, we store its
     * results in the display data structure. If it failed we only mark 
     * the failed status in the display data structure. */

    if (a_fsResults->probeOK) {	/* 1 => notOK the xstat results */
	curr_fsDataP->probeOK = 0;

	/* print the probe status */
	if (afsmon_debug) {
	    fprintf(debugFD, "\n\t\t ----- fs display data ------\n");
	    fprintf(debugFD, "HostName = %s  PROBE FAILED \n",
		    curr_fsDataP->hostName);
	    fflush(debugFD);
	}

    } else {			/* probe succeeded, update display data structures */
	curr_fsDataP->probeOK = 1;

	/* covert longs to strings and place them in curr_fsDataP */
	fs_Results_ltoa(curr_fsDataP, a_fsResults);

	/* compare with thresholds and set the overflow flags.
	 * note that the threshold information is in the hostEntry structure and 
	 * each threshold item has a positional index associated with it */

	/* locate the hostEntry for this host */
	done = 0;
	curr_host = FSnameList;
	for (i = 0; i < numFS; i++) {
	    if (strcasecmp(curr_host->hostName, a_fsResults->connP->hostName)
		== 0) {
		done = 1;
		break;
	    }
	    curr_host = curr_host->next;;
	}
	if (!done)
	    afsmon_Exit(70);

	code = check_fs_thresholds(curr_host, curr_fsDataP);
	if (code) {
	    fprintf(stderr, "[ %s ] Error in checking thresholds\n", rn);
	    afsmon_Exit(75);
	}




	/* print the info we just saved */

	if (afsmon_debug) {
	    fprintf(debugFD, "\n\t\t ----- fs display data ------\n");
	    fprintf(debugFD, "HostName = %s\n", curr_fsDataP->hostName);
	    for (i = 0; i < NUM_FS_STAT_ENTRIES; i++)
		fprintf(debugFD, "%20s  %30s  %s\n", curr_fsDataP->data[i],
			fs_varNames[i],
			curr_fsDataP->threshOvf[i] ? "(ovf)" : "");

	    fprintf(debugFD, "\t\t--------------------------------\n\n");
	    fflush(debugFD);
	}

    }				/* the probe succeeded, so we store the data in the display structure */


    /* if we have received a reply from all the hosts for this probe cycle,
     * it is time to display the data */

    probes_Received++;
    if (probes_Received == numFS) {
	probes_Received = 0;

	if (afsmon_fs_curr_probeNum != afsmon_fs_prev_probeNum + 1) {
	    sprintf(errMsg, "[ %s ] Probe number %d missed! \n", rn,
		    afsmon_fs_prev_probeNum + 1);
	    afsmon_Exit(80);
	} else
	    afsmon_fs_prev_probeNum++;

	/* backup the display data of the probe cycle that just completed -
	 * ie., store curr_fsData in prev_fsData */

	memcpy((char *)prev_fsData, (char *)curr_fsData,
	       (numFS * sizeof(struct fs_Display_Data)));


	/* initialize curr_fsData but retain the threshold flag information.
	 * The previous state of threshold flags is used in check_fs_thresholds() */

	numBytes = NUM_FS_STAT_ENTRIES * CM_STAT_STRING_LEN;
	curr_fsDataP = curr_fsData;
	for (i = 0; i < numFS; i++) {
	    curr_fsDataP->probeOK = 0;
	    curr_fsDataP->ovfCount = 0;
	    memset((char *)curr_fsDataP->data, 0, numBytes);
	    curr_fsDataP++;
	}


	/* prev_fsData now contains all the information for the probe cycle
	 * that just completed. Now count the number of threshold overflows for
	 * use in the overview screen */

	prev_fsDataP = prev_fsData;
	num_fs_alerts = 0;
	numHosts_onfs_alerts = 0;
	for (i = 0; i < numFS; i++) {
	    if (!prev_fsDataP->probeOK) {	/* if probe failed */
		num_fs_alerts++;
		numHosts_onfs_alerts++;
	    }
	    if (prev_fsDataP->ovfCount) {	/* overflows ?? */
		num_fs_alerts += prev_fsDataP->ovfCount;
		numHosts_onfs_alerts++;
	    }
	    prev_fsDataP++;
	}
	if (afsmon_debug)
	    fprintf(debugFD, "Number of FS alerts = %d (on %d hosts)\n",
		    num_fs_alerts, numHosts_onfs_alerts);

	/* flag that the data is now ready to be displayed */
	fs_Data_Available = 1;

	/* call the Overview frame update routine (update only FS info) */
	ovw_refresh(ovw_currPage, OVW_UPDATE_FS);

	/* call the File Servers frame update routine */
	fs_refresh(fs_currPage, fs_curr_LCol);

    }
    /* display data */
    return (0);
}				/* save_FS_data_forDisplay */




/*-----------------------------------------------------------------------
 * afsmon_FS_Handler()
 *
 * Description:
 *	This is the File Server probe Handler. It updates the afsmonitor
 *	probe counts, fs circular buffer indices and calls the functions
 *	to process the results of this probe.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
afsmon_FS_Handler()
{				/* afsmon_FS_Handler() */
    static char rn[] = "afsmon_FS_Handler";	/* routine name */
    int newProbeCycle;		/* start of new probe cycle ? */
    int code;			/* return status */


    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, hostName= %s, probeNum= %d, status=%s\n", rn,
		xstat_fs_Results.connP->hostName, xstat_fs_Results.probeNum,
		xstat_fs_Results.probeOK ? "FAILED" : "OK");
	fflush(debugFD);
    }


    /* print the probe results to output file */
    if (afsmon_output) {
	code = afsmon_fsOutput(output_filename, afsmon_detOutput);
	if (code) {
	    fprintf(stderr,
		    "[ %s ] output to file %s returned error code=%d\n", rn,
		    output_filename, code);
	}
    }

    /* Update current probe number and circular buffer index. if current 
     * probenum changed make sure it is only by 1 */

    newProbeCycle = 0;
    if (xstat_fs_Results.probeNum != afsmon_fs_curr_probeNum) {
	if (xstat_fs_Results.probeNum == afsmon_fs_curr_probeNum + 1) {
	    afsmon_fs_curr_probeNum++;
	    newProbeCycle = 1;
	    if (num_bufSlots)
		afsmon_fs_curr_CBindex =
		    (afsmon_fs_curr_probeNum - 1) % num_bufSlots;
	} else {
	    fprintf(stderr, "[ %s ] probe number %d-1 missed\n", rn,
		    xstat_fs_Results.probeNum);
	    afsmon_Exit(85);
	}
    }

    /* store the results of this probe in the FS circular buffer */
    if (num_bufSlots)
	save_FS_results_inCB(newProbeCycle);


    /* store the results of the current probe in the fs data display structure.
     * if the current probe number changed, swap the current and previous display
     * structures. note that the display screen is updated from these structures 
     * and should start showing the data of the just completed probe cycle */

    save_FS_data_forDisplay(&xstat_fs_Results);

    return (0);
}



/*----------------------------------------------------------------------- * 
 * Print_CM_CB()     
 *
 * Description:
 *	Debug routine.
 *	Prints the  Cache Manager circular buffer 
 *----------------------------------------------------------------------*/

void
Print_CM_CB()
{				/* Print_CM_CB() */

    struct afsmon_cm_Results_list *cmlist;
    int i;
    int j;

    /* print valid info in the cm CB */

    if (afsmon_debug) {
	fprintf(debugFD,
		"==================== CM Buffer ========================\n");
	fprintf(debugFD, "afsmon_cm_curr_CBindex = %d\n",
		afsmon_cm_curr_CBindex);
	fprintf(debugFD, "afsmon_cm_curr_probeNum = %d\n\n",
		afsmon_cm_curr_probeNum);

	for (i = 0; i < num_bufSlots; i++) {
	    fprintf(debugFD, "\t--------- slot %d ----------\n", i);
	    cmlist = afsmon_cm_ResultsCB[i].list;
	    j = 0;
	    while (j < numCM) {
		if (!cmlist->empty) {
		    fprintf(debugFD, "\t %d) probeNum = %d host = %s", j,
			    cmlist->cmResults->probeNum,
			    cmlist->cmResults->connP->hostName);
		    if (cmlist->cmResults->probeOK)
			fprintf(debugFD, " NOTOK\n");
		    else
			fprintf(debugFD, " OK\n");
		} else
		    fprintf(debugFD, "\t %d) -- empty --\n", j);
		cmlist = cmlist->next;
		j++;
	    }
	    if (cmlist != (struct afsmon_cm_Results_list *)0)
		fprintf(debugFD, "dangling last next ptr cm CB\n");
	}
    }
}


/*-----------------------------------------------------------------------
 * save_CM_results_inCB()
 *
 * Description:
 *	Saves the results of the latest CM probe in the cm circular
 * 	buffers. If the current probe cycle is in progress the contents
 * 	of xstat_cm_Results are copied to the end of the list of results
 *	in the current slot (pointed to by afsmon_cm_curr_CBindex). If
 * 	a new probe cycle has started the next slot in the circular buffer
 *	is initialized and the results copied. Note that the Rx related
 *	information available in xstat_cm_Results is not copied.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
save_CM_results_inCB(a_newProbeCycle)
     int a_newProbeCycle;	/* start of new probe cycle ? */

{				/* save_CM_results_inCB() */
    static char rn[] = "save_CM_results_inCB";	/* routine name */
    struct afsmon_cm_Results_list *tmp_cmlist_item;	/* temp cm list item */
    struct xstat_cm_ProbeResults *tmp_cmPR;	/* temp ptr */
    int i;


    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_newProbeCycle= %d\n", rn,
		a_newProbeCycle);
	fflush(debugFD);
    }

    /* If a new probe cycle started, mark the list in the current buffer
     * slot empty for resuse. Note that afsmon_cm_curr_CBindex was appropriately
     * incremented in afsmon_CM_Handler() */

    if (a_newProbeCycle) {
	tmp_cmlist_item = afsmon_cm_ResultsCB[afsmon_cm_curr_CBindex].list;
	for (i = 0; i < numCM; i++) {
	    tmp_cmlist_item->empty = 1;
	    tmp_cmlist_item = tmp_cmlist_item->next;
	}
    }

    /* locate last unused item in list */
    tmp_cmlist_item = afsmon_cm_ResultsCB[afsmon_cm_curr_CBindex].list;
    for (i = 0; i < numCM; i++) {
	if (tmp_cmlist_item->empty)
	    break;
	tmp_cmlist_item = tmp_cmlist_item->next;
    }

    /* if we could not find one we have an inconsistent list */
    if (!tmp_cmlist_item->empty) {
	fprintf(stderr,
		"[ %s ] list inconsistency 1. unable to find an empty slot to store results of probenum %d of %s\n",
		rn, xstat_cm_Results.probeNum,
		xstat_cm_Results.connP->hostName);
	afsmon_Exit(90);
    }

    tmp_cmPR = tmp_cmlist_item->cmResults;

    /* copy hostname and probe number and probe time and probe status.
     * if the probe failed return now */

    memcpy(tmp_cmPR->connP->hostName, xstat_cm_Results.connP->hostName,
	   sizeof(xstat_cm_Results.connP->hostName));
    tmp_cmPR->probeNum = xstat_cm_Results.probeNum;
    tmp_cmPR->probeTime = xstat_cm_Results.probeTime;
    tmp_cmPR->probeOK = xstat_cm_Results.probeOK;
    if (xstat_cm_Results.probeOK) {	/* probeOK = 1 => notOK */
	/* we have a nonempty results structure so mark the list item used */
	tmp_cmlist_item->empty = 0;
	return (0);
    }


    /* copy connection information */
    memcpy(&(tmp_cmPR->connP->skt), &(xstat_cm_Results.connP->skt),
	   sizeof(struct sockaddr_in));

   /**** NEED TO COPY rx_connection INFORMATION HERE ******/

    memcpy(tmp_cmPR->connP->hostName, xstat_cm_Results.connP->hostName,
	   sizeof(xstat_cm_Results.connP->hostName));
    tmp_cmPR->collectionNumber = xstat_cm_Results.collectionNumber;

    /* copy the probe data information */
    tmp_cmPR->data.AFSCB_CollData_len =
	xstat_cm_Results.data.AFSCB_CollData_len;
    memcpy(tmp_cmPR->data.AFSCB_CollData_val,
	   xstat_cm_Results.data.AFSCB_CollData_val,
	   xstat_cm_Results.data.AFSCB_CollData_len * sizeof(afs_int32));


    /* we have a valid results structure so mark the list item used */
    tmp_cmlist_item->empty = 0;

    /* print the stored info - to make sure we copied it right */
    /*   Print_cm_FullPerfInfo(tmp_cmPR);        */
    /* Print the cm circular buffer */
    Print_CM_CB();
    return (0);
}				/* save_CM_results_inCB */



/*-----------------------------------------------------------------------
 * cm_Results_ltoa()
 *
 * Description:
 *	The results of xstat probes are stored in a string format in 
 *	the arrays curr_cmData and prev_cmData. The information stored in
 *	prev_cmData is copied to the screen. 
 *	This function converts xstat FS results from longs to strings and 
 *	places them in the given buffer (a pointer to an item in curr_cmData).
 *	When a probe cycle completes, curr_cmData is copied to prev_cmData
 *	in afsmon_CM_Handler().
 *
 * Returns:
 *	Always returns 0.
 *----------------------------------------------------------------------*/

int
cm_Results_ltoa(a_cmData, a_cmResults)
     struct cm_Display_Data *a_cmData;	/* target buffer */
     struct xstat_cm_ProbeResults *a_cmResults;	/* ptr to xstat cm Results */
{				/* cm_Results_ltoa */

    static char rn[] = "cm_Results_ltoa";	/* routine name */
    struct afs_stats_CMFullPerf *fullP;	/* ptr to complete CM stats */
    afs_int32 *srcbuf;
    afs_int32 *tmpbuf;
    int i, j;
    int idx;
    afs_int32 numLongs;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_cmData= %d, a_cmResults= %d\n", rn,
		a_cmData, a_cmResults);
	fflush(debugFD);
    }


    fullP = (struct afs_stats_CMFullPerf *)
	(a_cmResults->data.AFSCB_CollData_val);

    /* There are 4 parts to CM statistics
     * - Overall performance statistics (including up/down statistics)
     * - This CMs FS RPC operations info 
     * - This CMs FS RPC errors info
     * - This CMs FS transfers info
     * - Authentication info
     * - [Un]Replicated access info
     */

    /* copy overall performance statistics */
    srcbuf = (afs_int32 *) & (fullP->perf);
    idx = 0;
    /* we skip the 19 entry, ProtServAddr, so the index must account for this */
    for (i = 0; i < NUM_AFS_STATS_CMPERF_LONGS + 1; i++) {
	if (i == 19) {
	    srcbuf++;
	    continue;		/* skip ProtServerAddr */
	}
	sprintf(a_cmData->data[idx], "%d", *srcbuf);
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* server up/down statistics */
    /* copy file server up/down stats */
    srcbuf = (afs_int32 *) (fullP->perf.fs_UpDown);
    numLongs =
	2 * (sizeof(struct afs_stats_SrvUpDownInfo) / sizeof(afs_int32));
    for (i = 0; i < numLongs; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* copy volume location  server up/down stats */
    srcbuf = (afs_int32 *) (fullP->perf.vl_UpDown);
    numLongs =
	2 * (sizeof(struct afs_stats_SrvUpDownInfo) / sizeof(afs_int32));
    for (i = 0; i < numLongs; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* copy CMs individual FS RPC operations info */
    srcbuf = (afs_int32 *) (fullP->rpc.fsRPCTimes);
    for (i = 0; i < AFS_STATS_NUM_FS_RPC_OPS; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* numOps */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* numSuccesses */
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sum time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sqr time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* min time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* max time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* copy CMs individual FS RPC errors info */

    srcbuf = (afs_int32 *) (fullP->rpc.fsRPCErrors);
    for (i = 0; i < AFS_STATS_NUM_FS_RPC_OPS; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* server */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* network */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* prot */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* vol */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* busies */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* other */
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* copy CMs individual RPC transfers info */

    srcbuf = (afs_int32 *) (fullP->rpc.fsXferTimes);
    for (i = 0; i < AFS_STATS_NUM_FS_XFER_OPS; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* numOps */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* numSuccesses */
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sum time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sqr time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* min time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* max time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* sum bytes */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* min bytes */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* max bytes */
	idx++;
	srcbuf++;
	for (j = 0; j < AFS_STATS_NUM_XFER_BUCKETS; j++) {
	    sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* bucket[j] */
	    idx++;
	    srcbuf++;
	}
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* copy CM operations timings */

    srcbuf = (afs_int32 *) (fullP->rpc.cmRPCTimes);
    for (i = 0; i < AFS_STATS_NUM_CM_RPC_OPS; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* numOps */
	idx++;
	srcbuf++;
	sprintf(a_cmData->data[idx], "%d", *srcbuf);	/* numSuccesses */
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sum time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* sqr time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* min time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
	tmpbuf = srcbuf++;	/* max time */
	sprintf(a_cmData->data[idx], "%d.%06d", *tmpbuf, *srcbuf);
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* copy authentication info */

    srcbuf = (afs_int32 *) & (fullP->authent);
    numLongs = sizeof(struct afs_stats_AuthentInfo) / sizeof(afs_int32);
    for (i = 0; i < numLongs; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */

    /* copy CM [un]replicated access info */

    srcbuf = (afs_int32 *) & (fullP->accessinf);
    numLongs = sizeof(struct afs_stats_AccessInfo) / sizeof(afs_int32);
    for (i = 0; i < numLongs; i++) {
	sprintf(a_cmData->data[idx], "%d", *srcbuf);
	idx++;
	srcbuf++;
    }

    /*printf("Ending index value = %d\n",idx-1); */
    return (0);

}				/* cm_Results_ltoa */


/*-----------------------------------------------------------------------
 * Function:	check_cm_thresholds()
 *
 * Description:
 *	Checks the thresholds and sets the overflow flag. Recall that the
 *	thresholds for each host are stored in the hostEntry lists
 *	[fs/cm]nameList arrays. The probe results are passed to this 
 *	function in the display-ready format - ie., as strings. Though
 *	this looks stupid the overhead incurred in converting the strings
 *	back to floats and comparing them is insignificant and 
 *	programming is easier this way. 
 *	The threshold flags are a part of the display structures
 *	curr_[fs/cm]Data.
 *
 * Returns:
 *	0
 *----------------------------------------------------------------------*/

int
check_cm_thresholds(a_hostEntry, a_Data)
     struct afsmon_hostEntry *a_hostEntry;	/* ptr to hostEntry */
     struct cm_Display_Data *a_Data;	/* ptr to cm data to be displayed */

{				/* check_cm_thresholds */

    static char rn[] = "check_cm_thresholds";
    struct Threshold *threshP;
    double tValue;		/* threshold value */
    double pValue;		/* probe value */
    int i;
    int idx;
    int count;			/* number of thresholds exceeded */

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_hostEntry= %d, a_Data= %d\n", rn,
		a_hostEntry, a_Data);
	fflush(debugFD);
    }

    if (a_hostEntry->numThresh == 0) {
	/* store in ovf count ?? */
	return (0);
    }

    count = 0;
    threshP = a_hostEntry->thresh;
    for (i = 0; i < a_hostEntry->numThresh; i++) {
	if (threshP->itemName[0] == '\0') {
	    threshP++;
	    continue;
	}
	idx = threshP->index;	/* positional index to the data array */
	tValue = atof(threshP->threshVal);	/* threshold value */
	pValue = atof(a_Data->data[idx]);	/* probe value */
	if (pValue > tValue) {

	    if (afsmon_debug) {
		fprintf(debugFD,
			"[ %s ] cm = %s, thresh ovf for %s, threshold= %s, probevalue= %s\n",
			rn, a_hostEntry->hostName, threshP->itemName,
			threshP->threshVal, a_Data->data[idx]);
		fflush(debugFD);
	    }

	    /* if the threshold is crossed, call the handler function
	     * only if this was a transition -ie, if the threshold was
	     * crossed in the last probe too just count & keep quite! */

	    if (!a_Data->threshOvf[idx]) {
		a_Data->threshOvf[idx] = 1;
		/* call the threshold handler if provided */
		if (threshP->handler[0] != '\0') {
		    if (afsmon_debug) {
			fprintf(debugFD, "[ %s ] Calling ovf handler %s\n",
				rn, threshP->handler);
			fflush(debugFD);
		    }
		    execute_thresh_handler(threshP->handler, a_Data->hostName,
					   CM, threshP->itemName,
					   threshP->threshVal,
					   a_Data->data[idx]);
		}
	    }

	    count++;
	} else
	    /* in case threshold was previously crossed, blank it out */
	    a_Data->threshOvf[idx] = 0;
	threshP++;
    }
    /* store the overflow count */
    a_Data->ovfCount = count;

    return (0);
}				/* check_cm_thresholds */


/*-----------------------------------------------------------------------
 * save_CM_data_forDisplay()
 *
 * Description:
 *	Does the following:
 *	- if the probe number changed (ie, a cycle completed) curr_cmData
 *	is copied to prev_cmData, curr_cmData zeroed and refresh the 
 *	overview screen and file server screen with the new data.
 *	- store the results of the current probe from xstat_cm_Results into
 *	curr_cmData. ie., convert longs to strings.
 *	- check the thresholds 
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *
 *----------------------------------------------------------------------*/

int
save_CM_data_forDisplay(a_cmResults)
     struct xstat_cm_ProbeResults *a_cmResults;
{				/* save_CM_data_forDisplay */

    static char rn[] = "save_CM_data_forDisplay";	/* routine name */
    struct cm_Display_Data *curr_cmDataP;
    struct cm_Display_Data *prev_cmDataP;
    struct afsmon_hostEntry *curr_host;
    static int probes_Received = 0;	/* number of probes reveived in
					 * the current cycle. If this is equal to numFS we got all
					 * the data we want in this cycle and can now display it */
    int numBytes;
    int done;
    int code;
    int okay;
    int i;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_cmResults= %d\n", rn, a_cmResults);
	fflush(debugFD);
    }

    /* store results in the display array */

    okay = 0;
    curr_cmDataP = curr_cmData;
    for (i = 0; i < numCM; i++) {
	if ((strcasecmp(curr_cmDataP->hostName, a_cmResults->connP->hostName))
	    == 0) {
	    okay = 1;
	    break;
	}
	curr_cmDataP++;
    }

    if (!okay) {
	fprintf(stderr,
		"[ %s ] Could not insert CM probe results for host %s in cm display array\n",
		rn, a_cmResults->connP->hostName);
	afsmon_Exit(95);
    }

    /*  Check the status of the probe. If it succeeded, we store its
     * results in the display data structure. If it failed we only mark 
     * the failed status in the display data structure. */


    if (a_cmResults->probeOK) {	/* 1 => notOK the xstat results */
	curr_cmDataP->probeOK = 0;

	/* print the probe status */
	if (afsmon_debug) {
	    fprintf(debugFD, "\n\t\t ----- cm display data ------\n");
	    fprintf(debugFD, "HostName = %s  PROBE FAILED \n",
		    curr_cmDataP->hostName);
	    fflush(debugFD);
	}

    } else {			/* probe succeeded, update display data structures */
	curr_cmDataP->probeOK = 1;


	/* covert longs to strings and place them in curr_cmDataP */
	cm_Results_ltoa(curr_cmDataP, a_cmResults);

	/* compare with thresholds and set the overflow flags.
	 * note that the threshold information is in the hostEntry structure and 
	 * each threshold item has a positional index associated with it */

	/* locate the hostEntry for this host */
	done = 0;
	curr_host = CMnameList;
	for (i = 0; i < numCM; i++) {
	    if (strcasecmp(curr_host->hostName, a_cmResults->connP->hostName)
		== 0) {
		done = 1;
		break;
	    }
	    curr_host = curr_host->next;
	}
	if (!done)
	    afsmon_Exit(100);

	code = check_cm_thresholds(curr_host, curr_cmDataP);
	if (code) {
	    fprintf(stderr, "[ %s ] Error in checking thresholds\n", rn);
	    afsmon_Exit(105);
	}


	/* print the info we just saved */
	if (afsmon_debug) {
	    fprintf(debugFD, "\n\t\t ----- CM display data ------\n");
	    fprintf(debugFD, "HostName = %s\n", curr_cmDataP->hostName);
	    for (i = 0; i < NUM_CM_STAT_ENTRIES; i++) {
		switch (i) {
		case 0:
		    fprintf(debugFD, "\t -- Overall Perf Info --\n");
		    break;
		case 39:
		    fprintf(debugFD,
			    "\t -- File Server up/down stats - same cell --\n");
		    break;
		case 64:
		    fprintf(debugFD,
			    "\t -- File Server up/down stats - diff cell --\n");
		    break;
		case 89:
		    fprintf(debugFD,
			    "\t -- VL server up/down stats - same cell --\n");
		    break;
		case 114:
		    fprintf(debugFD,
			    "\t -- VL server up/down stats - diff cell --\n");
		    break;
		case 139:
		    fprintf(debugFD, "\t -- FS Operation Timings --\n");
		    break;
		case 279:
		    fprintf(debugFD, "\t -- FS Error Info --\n");
		    break;
		case 447:
		    fprintf(debugFD, "\t -- FS Transfer Timings --\n");
		    break;
		case 475:
		    fprintf(debugFD, "\t -- CM Operations Timings --\n");
		    break;
		case 510:
		    fprintf(debugFD, "\t -- Authentication Info --\n");
		    break;
		case 522:
		    fprintf(debugFD, "\t -- Access Info --\n");
		    break;
		default:
		    break;
		}

		fprintf(debugFD, "%20s  %30s %s\n", curr_cmDataP->data[i],
			cm_varNames[i],
			curr_cmDataP->threshOvf[i] ? "(ovf)" : "");
	    }
	    fprintf(debugFD, "\t\t--------------------------------\n\n");
	}

    }				/* if the probe succeeded, update the display data structures */

    /* if we have received a reply from all the hosts for this probe cycle,
     * it is time to display the data */

    probes_Received++;
    if (probes_Received == numCM) {
	probes_Received = 0;

	if (afsmon_cm_curr_probeNum != afsmon_cm_prev_probeNum + 1) {
	    sprintf(errMsg, "[ %s ] Probe number %d missed! \n", rn,
		    afsmon_cm_prev_probeNum + 1);
	    afsmon_Exit(110);
	} else
	    afsmon_cm_prev_probeNum++;


	/* backup the display data of the probe cycle that just completed -
	 * ie., store curr_cmData in prev_cmData */

	memcpy((char *)prev_cmData, (char *)curr_cmData,
	       (numCM * sizeof(struct cm_Display_Data)));


	/* initialize curr_cmData but retain the threshold flag information.
	 * The previous state of threshold flags is used in check_cm_thresholds() */

	curr_cmDataP = curr_cmData;
	numBytes = NUM_CM_STAT_ENTRIES * CM_STAT_STRING_LEN;
	for (i = 0; i < numCM; i++) {
	    curr_cmDataP->probeOK = 0;
	    curr_cmDataP->ovfCount = 0;
	    memset((char *)curr_cmDataP->data, 0, numBytes);
	    curr_cmDataP++;
	}

	/* prev_cmData now contains all the information for the probe cycle
	 * that just completed. Now count the number of threshold overflows for
	 * use in the overview screen */

	prev_cmDataP = prev_cmData;
	num_cm_alerts = 0;
	numHosts_oncm_alerts = 0;
	for (i = 0; i < numCM; i++) {
	    if (!prev_cmDataP->probeOK) {	/* if probe failed */
		num_cm_alerts++;
		numHosts_oncm_alerts++;
	    } else if (prev_cmDataP->ovfCount) {	/* overflows ?? */
		num_cm_alerts += prev_cmDataP->ovfCount;
		numHosts_oncm_alerts++;
	    }
	    prev_cmDataP++;
	}
	if (afsmon_debug)
	    fprintf(debugFD, "Number of CM alerts = %d (on %d hosts)\n",
		    num_cm_alerts, numHosts_oncm_alerts);


	/* flag that the data is now ready to be displayed */
	cm_Data_Available = 1;

	/* update the Overview frame (only CM info) */
	ovw_refresh(ovw_currPage, OVW_UPDATE_CM);

	/* update the Cache Managers frame */
	cm_refresh(cm_currPage, cm_curr_LCol);

    }


    return (0);
}				/* save_CM_data_forDisplay */



/*-----------------------------------------------------------------------
 * afsmon_CM_Handler()
 *
 * Description:
 *	This is the Cache Manager probe Handler. It updates the afsmonitor
 *	probe counts, cm circular buffer indices and calls the functions
 *	to process the results of this probe.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
afsmon_CM_Handler()
{				/* afsmon_CM_Handler() */
    static char rn[] = "afsmon_CM_Handler";	/* routine name */
    int code;			/* return status */
    int newProbeCycle;		/* start of new probe cycle ? */

    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, hostName= %s, probeNum= %d, status= %s\n", rn,
		xstat_cm_Results.connP->hostName, xstat_cm_Results.probeNum,
		xstat_cm_Results.probeOK ? "FAILED" : "OK");
	fflush(debugFD);
    }


    /* print the probe results to output file */
    if (afsmon_output) {
	code = afsmon_cmOutput(output_filename, afsmon_detOutput);
	if (code) {
	    fprintf(stderr,
		    "[ %s ] output to file %s returned error code=%d\n", rn,
		    output_filename, code);
	}
    }

    /* Update current probe number and circular buffer index. if current 
     * probenum changed make sure it is only by 1 */

    newProbeCycle = 0;
    if (xstat_cm_Results.probeNum != afsmon_cm_curr_probeNum) {
	if (xstat_cm_Results.probeNum == afsmon_cm_curr_probeNum + 1) {
	    afsmon_cm_curr_probeNum++;
	    newProbeCycle = 1;
	    if (num_bufSlots)
		afsmon_cm_curr_CBindex =
		    (afsmon_cm_curr_probeNum - 1) % num_bufSlots;
	} else {
	    fprintf(stderr, "[ %s ] probe number %d-1 missed\n", rn,
		    xstat_cm_Results.probeNum);
	    afsmon_Exit(115);
	}
    }

    /* save the results of this probe in the CM buffer */
    if (num_bufSlots)
	save_CM_results_inCB(newProbeCycle);

    /* store the results of the current probe in the cm data display structure.
     * if the current probe number changed, swap the current and previous display
     * structures. note that the display screen is updated from these structures 
     * and should start showing the data of the just completed probe cycle */

    save_CM_data_forDisplay(&xstat_cm_Results);

    return (0);
}

/*-----------------------------------------------------------------------
 * init_fs_buffers()
 *
 * Description:
 *	Allocate and Initialize circular buffers for file servers.
 *
 * Returns:
 *	Success: 0
 *	Failure to allocate memory: exits afsmonitor.
 *----------------------------------------------------------------------*/

int
init_fs_buffers()
{				/* init_fs_buffers() */
    static char rn[] = "init_fs_buffers";	/* routine name */
    struct afsmon_fs_Results_list *new_fslist_item;	/* ptr for new struct */
    struct afsmon_fs_Results_list *tmp_fslist_item;	/* temp ptr */
    struct xstat_fs_ProbeResults *new_fsPR;	/* ptr for new struct  */
    int i, j;
    int bufslot;
    int numfs;


    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    /* allocate memory for the circular buffer of pointers */

    afsmon_fs_ResultsCB = (struct afsmon_fs_Results_CBuffer *)
	malloc(sizeof(struct afsmon_fs_Results_CBuffer) * num_bufSlots);

    /* initialize the fs circular buffer */
    for (i = 0; i < num_bufSlots; i++) {
	afsmon_fs_ResultsCB[i].list = (struct afsmon_fs_Results_list *)0;
	afsmon_fs_ResultsCB[i].probeNum = 0;
    }

    /* create  a list of numFS items to store fs probe results for 
     * each slot in CB */

    if (numFS) {		/* if we have file servers to monitor */
	for (bufslot = 0; bufslot < num_bufSlots; bufslot++) {
	    numfs = numFS;	/* get the number of servers */
	    while (numfs--) {

		/* if any of these mallocs fail we only need to free the memory we
		 * have allocated in this iteration. the rest of it which is in a 
		 * proper linked list will be freed in afsmon_Exit */

		/* allocate memory for an fs list item */
		new_fslist_item = (struct afsmon_fs_Results_list *)
		    malloc(sizeof(struct afsmon_fs_Results_list));
		if (new_fslist_item == (struct afsmon_fs_Results_list *)0)
		    return (-1);

		/* allocate memory to store xstat_fs_Results */
		new_fsPR = (struct xstat_fs_ProbeResults *)
		    malloc(sizeof(struct xstat_fs_ProbeResults));
		if (new_fsPR == (struct xstat_fs_ProbeResults *)0) {
		    free(new_fslist_item);
		    return (-1);
		}
		new_fsPR->connP = (struct xstat_fs_ConnectionInfo *)
		    malloc(sizeof(struct xstat_fs_ConnectionInfo));
		if (new_fsPR->connP == (struct xstat_fs_ConnectionInfo *)0) {
		    free(new_fslist_item);
		    free(new_fsPR);
		    return (-1);
		}

		/* >>>  need to allocate rx connection info structure here <<< */

		new_fsPR->data.AFS_CollData_val =
		    (afs_int32 *) malloc(XSTAT_FS_FULLPERF_RESULTS_LEN *
					 sizeof(afs_int32));
		if (new_fsPR->data.AFS_CollData_val == NULL) {
		    free(new_fslist_item);
		    free(new_fsPR->connP);
		    free(new_fsPR);
		    return (-1);
		}

		/* initialize this list entry */
		new_fslist_item->fsResults = new_fsPR;
		new_fslist_item->empty = 1;
		new_fslist_item->next = (struct afsmon_fs_Results_list *)0;

		/* store it at the end of the fs list in the current CB slot */
		if (afsmon_fs_ResultsCB[bufslot].list ==
		    (struct afsmon_fs_Results_list *)0)
		    afsmon_fs_ResultsCB[bufslot].list = new_fslist_item;
		else {
		    tmp_fslist_item = afsmon_fs_ResultsCB[bufslot].list;
		    j = 0;
		    while (tmp_fslist_item !=
			   (struct afsmon_fs_Results_list *)0) {
			if (tmp_fslist_item->next ==
			    (struct afsmon_fs_Results_list *)0)
			    break;
			tmp_fslist_item = tmp_fslist_item->next;
			if (++j > numFS) {
			    /* something goofed. exit */
			    fprintf(stderr, "[ %s ] list creation error\n",
				    rn);
			    return (-1);
			}
		    }
		    tmp_fslist_item->next = new_fslist_item;
		}

	    }			/* while servers */
	}			/* for each buffer slot */
    }				/* if we have file servers to monitor */
    return (0);
}

/*-----------------------------------------------------------------------
 * init_cm_buffers()
 *
 * Description:
 *	Allocate and Initialize circular buffers for cache managers.
 *
 * Returns:
 *	Success: 0
 *	Failure to allocate memory: exits afsmonitor.
 *----------------------------------------------------------------------*/

int
init_cm_buffers()
{				/* init_cm_buffers() */
    static char rn[] = "init_cm_buffers";	/* routine name */
    struct afsmon_cm_Results_list *new_cmlist_item;	/* ptr for new struct */
    struct afsmon_cm_Results_list *tmp_cmlist_item;	/* temp ptr */
    struct xstat_cm_ProbeResults *new_cmPR;	/* ptr for new struct  */
    int i, j;
    int bufslot;
    int numcm;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    /* allocate memory for the circular buffer of pointers */
    afsmon_cm_ResultsCB = (struct afsmon_cm_Results_CBuffer *)
	malloc(sizeof(struct afsmon_cm_Results_CBuffer) * num_bufSlots);

    /* initialize the fs circular buffer */
    for (i = 0; i < num_bufSlots; i++) {
	afsmon_cm_ResultsCB[i].list = (struct afsmon_cm_Results_list *)0;
	afsmon_cm_ResultsCB[i].probeNum = 0;
    }

    /* create  a list of numCM items to store fs probe results for 
     * each slot in CB */

    if (numCM) {		/* if we have file servers to monitor */
	for (bufslot = 0; bufslot < num_bufSlots; bufslot++) {
	    numcm = numCM;	/* get the number of servers */
	    while (numcm--) {

		/* if any of these mallocs fail we only need to free the memory we
		 * have allocated in this iteration. the rest of it which is in a 
		 * proper linked list will be freed in afsmon_Exit */

		/* allocate memory for an fs list item */
		new_cmlist_item = (struct afsmon_cm_Results_list *)
		    malloc(sizeof(struct afsmon_cm_Results_list));
		if (new_cmlist_item == (struct afsmon_cm_Results_list *)0)
		    return (-1);

		/* allocate memory to store xstat_cm_Results */
		new_cmPR = (struct xstat_cm_ProbeResults *)
		    malloc(sizeof(struct xstat_cm_ProbeResults));
		if (new_cmPR == (struct xstat_cm_ProbeResults *)0) {
		    free(new_cmlist_item);
		    return (-1);
		}
		new_cmPR->connP = (struct xstat_cm_ConnectionInfo *)
		    malloc(sizeof(struct xstat_cm_ConnectionInfo));
		if (new_cmPR->connP == (struct xstat_cm_ConnectionInfo *)0) {
		    free(new_cmlist_item);
		    free(new_cmPR);
		    return (-1);
		}

		/* >>>  need to allocate rx connection info structure here <<< */

		new_cmPR->data.AFSCB_CollData_val =
		    (afs_int32 *) malloc(XSTAT_CM_FULLPERF_RESULTS_LEN *
					 sizeof(afs_int32));
		if (new_cmPR->data.AFSCB_CollData_val == NULL) {
		    free(new_cmlist_item);
		    free(new_cmPR->connP);
		    free(new_cmPR);
		    return (-1);
		}

		/* initialize this list entry */
		new_cmlist_item->cmResults = new_cmPR;
		new_cmlist_item->empty = 1;
		new_cmlist_item->next = (struct afsmon_cm_Results_list *)0;

		/* store it at the end of the cm list in the current CB slot */
		if (afsmon_cm_ResultsCB[bufslot].list ==
		    (struct afsmon_cm_Results_list *)0)
		    afsmon_cm_ResultsCB[bufslot].list = new_cmlist_item;
		else {
		    tmp_cmlist_item = afsmon_cm_ResultsCB[bufslot].list;
		    j = 0;
		    while (tmp_cmlist_item !=
			   (struct afsmon_cm_Results_list *)0) {
			if (tmp_cmlist_item->next ==
			    (struct afsmon_cm_Results_list *)0)
			    break;
			tmp_cmlist_item = tmp_cmlist_item->next;
			if (++j > numCM) {
			    /* something goofed. exit */
			    fprintf(stderr, "[ %s ] list creation error\n",
				    rn);
			    return (-1);
			}
		    }
		    tmp_cmlist_item->next = new_cmlist_item;
		}

	    }			/* while servers */
	}			/* for each buffer slot */
    }
    /* if we have file servers to monitor */
    /* print the CB to make sure it is right */
    Print_CM_CB();

    return (0);
}				/* init_cm_buffers() */


/*-------------------------------------------------------------------------
 * init_print_buffers()
 *
 * Description:
 *	Allocate and initialize the buffers used for printing results
 *	to the display screen. These buffers store the current and 
 *	previous probe results in ascii format. 
 *
 * Returns:
 *	Success: 0
 *	Failure: < 0
 *------------------------------------------------------------------------*/

int
init_print_buffers()
{				/* init_print_buffers */

    static char rn[] = "init_print_buffers";	/* routine name */
    struct fs_Display_Data *tmp_fsData1;	/* temp pointers */
    struct fs_Display_Data *tmp_fsData2;
    struct cm_Display_Data *tmp_cmData1;
    struct cm_Display_Data *tmp_cmData2;
    struct afsmon_hostEntry *tmp_fsNames;
    struct afsmon_hostEntry *tmp_cmNames;
    int i;
    int numBytes;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    /* allocate numFS blocks of the FS print structure. */

    /* we need two instances of this structure - one (curr_fsData) for storing
     * the results of the fs probes currently in progress and another (prev_fsData)
     * for the last completed probe. The display is updated from the contents of
     * prev_fsData. The pointers curr_fsData & prev_fsData are switched whenever 
     * the probe number changes */

    if (numFS) {
	numBytes = numFS * sizeof(struct fs_Display_Data);
	curr_fsData = (struct fs_Display_Data *)malloc(numBytes);
	if (curr_fsData == (struct fs_Display_Data *)0) {
	    fprintf(stderr, "[ %s ] Memory allocation failure\n", rn);
	    return (-1);
	}
	memset(curr_fsData, 0, numBytes);

	numBytes = numFS * sizeof(struct fs_Display_Data);
	prev_fsData = (struct fs_Display_Data *)malloc(numBytes);
	if (prev_fsData == (struct fs_Display_Data *)0) {
	    fprintf(stderr, "[ %s ] Memory allocation failure\n", rn);
	    return (-5);
	}
	memset(prev_fsData, 0, numBytes);

	/* fill in the host names */
	tmp_fsData1 = curr_fsData;
	tmp_fsData2 = curr_fsData;
	tmp_fsNames = FSnameList;
	for (i = 0; i < numFS; i++) {
	    strncpy(tmp_fsData1->hostName, tmp_fsNames->hostName,
		    HOST_NAME_LEN);
	    strncpy(tmp_fsData2->hostName, tmp_fsNames->hostName,
		    HOST_NAME_LEN);
	    tmp_fsData1++;
	    tmp_fsData2++;
	    tmp_fsNames = tmp_fsNames->next;;
	}

    }



    /* if file servers to monitor */
    /* allocate numCM blocks of the CM print structure */
    /* we need two instances of this structure for the same reasons as above */
    if (numCM) {
	numBytes = numCM * sizeof(struct cm_Display_Data);

	curr_cmData = (struct cm_Display_Data *)malloc(numBytes);
	if (curr_cmData == (struct cm_Display_Data *)0) {
	    fprintf(stderr, "[ %s ] Memory allocation failure\n", rn);
	    return (-10);
	}
	memset(curr_cmData, 0, numBytes);

	numBytes = numCM * sizeof(struct cm_Display_Data);
	prev_cmData = (struct cm_Display_Data *)malloc(numBytes);
	if (prev_cmData == (struct cm_Display_Data *)0) {
	    fprintf(stderr, "[ %s ] Memory allocation failure\n", rn);
	    return (-15);
	}
	memset(prev_cmData, 0, numBytes);

	/* fill in the host names */
	tmp_cmData1 = curr_cmData;
	tmp_cmData2 = curr_cmData;
	tmp_cmNames = CMnameList;
	for (i = 0; i < numCM; i++) {
	    strncpy(tmp_cmData1->hostName, tmp_cmNames->hostName,
		    HOST_NAME_LEN);
	    strncpy(tmp_cmData2->hostName, tmp_cmNames->hostName,
		    HOST_NAME_LEN);
	    tmp_cmData1++;
	    tmp_cmData2++;
	    tmp_cmNames = tmp_cmNames->next;;
	}

    }
    /* if cache managers to monitor */
    return (0);

}				/* init_print_buffers */

/*-----------------------------------------------------------------------
 * quit_signal()
 *
 * Description:
 *	Trap the interrupt signal. This function is useful only until
 *	gtx is initialized.
 *----------------------------------------------------------------------*/

void
quit_signal(sig)
     int sig;
{				/* quit_signal */
    static char *rn = "quit_signal";	/* routine name */

    fprintf(stderr, "Received signal %d \n", sig);
    afsmon_Exit(120);
}				/* quit_signal */



/*-----------------------------------------------------------------------
 * afsmon_execute()
 *
 * Description:
 *	This is where we start it all. Initialize an array of sockets for
 *	file servers and cache cache managers and call the xstat_[fs/cm]_Init
 *	routines. The last step is to call the gtx input server which 
 *	grabs control of the keyboard.
 *
 * Returns:
 *	Does not return. Control is periodically returned to the afsmonitor
 *	thru afsmon_[FS/CM]_Handler() routines and also through the gtx
 *	keyboard handler calls.
 *
 *----------------------------------------------------------------------*/

int
afsmon_execute()
{				/* afsmon_execute() */
    static char rn[] = "afsmon_execute";	/* routine name */
    static char fullhostname[128];	/* full host name */
    struct sockaddr_in *FSSktArray;	/* fs socket array */
    int FSsktbytes;		/* num bytes in above */
    struct sockaddr_in *CMSktArray;	/* cm socket array */
    int CMsktbytes;		/* num bytes in above */
    struct sockaddr_in *curr_skt;	/* ptr to current socket */
    struct afsmon_hostEntry *curr_FS;	/* ptr to FS name list */
    struct afsmon_hostEntry *curr_CM;	/* ptr to CM name list */
    struct hostent *he;		/* host entry */
    afs_int32 *collIDP;		/* ptr to collection ID */
    int numCollIDs;		/* number of collection IDs */
    int FSinitFlags = 0;	/* flags for xstat_fs_Init */
    int CMinitFlags = 0;	/* flags for xstat_cm_Init */
    int code;			/* function return code */
    struct timeval tv;		/* time structure */

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }


    /* process file server entries */
    if (numFS) {
	/* Allocate an array of sockets for each fileserver we monitor */

	FSsktbytes = numFS * sizeof(struct sockaddr_in);
	FSSktArray = (struct sockaddr_in *)malloc(FSsktbytes);
	if (FSSktArray == (struct sockaddr_in *)0) {
	    fprintf(stderr,
		    "[ %s ] cannot malloc %d sockaddr_ins for fileservers\n",
		    rn, numFS);
	    return (-1);
	}

	memset(FSSktArray, 0, FSsktbytes);

	/* Fill in the socket information for each fileserve */

	curr_skt = FSSktArray;
	curr_FS = FSnameList;	/* FS name list header */
	while (curr_FS) {
	    strncpy(fullhostname, curr_FS->hostName, sizeof(fullhostname));
	    he = GetHostByName(fullhostname);
	    if (he == NULL) {
		fprintf(stderr, "[ %s ] Cannot get host info for %s\n", rn,
			fullhostname);
		return (-1);
	    }
	    strncpy(curr_FS->hostName, he->h_name, HOST_NAME_LEN);	/* complete name */
	    memcpy(&(curr_skt->sin_addr.s_addr), he->h_addr, 4);
	    curr_skt->sin_family = AF_INET;		/*Internet family */
	    curr_skt->sin_port = htons(7000);	/*FileServer port */
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	    curr_skt->sin_len = sizeof(struct sockaddr_in);
#endif

	    /* get the next dude */
	    curr_skt++;
	    curr_FS = curr_FS->next;
	}

	/* initialize collection IDs. We need only one entry since we collect
	 * all the information from xstat */

	numCollIDs = 1;
	collIDP = (afs_int32 *) malloc(sizeof(afs_int32));
	if (collIDP == NULL) {
	    fprintf(stderr,
		    "[ %s ] failed to allocate a measely afs_int32 word.Argh!\n",
		    rn);
	    return (-1);
	}
	*collIDP = 2;		/* USE A macro for this */

	FSinitFlags = 0;
	if (afsmon_onceOnly)	/* option not provided at this time */
	    FSinitFlags |= XSTAT_FS_INITFLAG_ONE_SHOT;

	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Calling xstat_fs_Init \n", rn);
	    fflush(debugFD);
	}

	code = xstat_fs_Init(numFS,	/*Num servers */
			     FSSktArray,	/*File Server socket array */
			     afsmon_probefreq,	/*probe frequency */
			     afsmon_FS_Handler,	/*Handler routine */
			     FSinitFlags,	/*Initialization flags */
			     numCollIDs,	/*Number of collection IDs */
			     collIDP);	/*Ptr to collection ID */

	if (code) {
	    fprintf(stderr, "[ %s ] xstat_fs_init returned error\n", rn);
	    afsmon_Exit(125);
	}

    }


    /* end of process fileserver entries */
    /* process cache manager entries */
    if (numCM) {
	/* Allocate an array of sockets for each cache manager we monitor */

	CMsktbytes = numCM * sizeof(struct sockaddr_in);
	CMSktArray = (struct sockaddr_in *)malloc(CMsktbytes);
	if (CMSktArray == (struct sockaddr_in *)0) {
	    fprintf(stderr,
		    "[ %s ] cannot malloc %d sockaddr_ins for CM entries\n",
		    rn, numCM);
	    return (-1);
	}

	memset(CMSktArray, 0, CMsktbytes);

	/* Fill in the socket information for each CM        */

	curr_skt = CMSktArray;
	curr_CM = CMnameList;	/* CM name list header */
	while (curr_CM) {
	    strncpy(fullhostname, curr_CM->hostName, sizeof(fullhostname));
	    he = GetHostByName(fullhostname);
	    if (he == NULL) {
		fprintf(stderr, "[ %s ] Cannot get host info for %s\n", rn,
			fullhostname);
		return (-1);
	    }
	    strncpy(curr_CM->hostName, he->h_name, HOST_NAME_LEN);	/* complete name */
	    memcpy(&(curr_skt->sin_addr.s_addr), he->h_addr, 4);
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
	    curr_skt->sin_family = AF_INET;		/*Internet family */
#else
	    curr_skt->sin_family = htons(AF_INET);	/*Internet family */
#endif
	    curr_skt->sin_port = htons(7001);	/*Cache Manager port */
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	    curr_skt->sin_len = sizeof(struct sockaddr_in);
#endif

	    /* get the next dude */
	    curr_skt++;
	    curr_CM = curr_CM->next;
	}

	/* initialize collection IDs. We need only one entry since we collect
	 * all the information from xstat */

	numCollIDs = 1;
	collIDP = (afs_int32 *) malloc(sizeof(afs_int32));
	if (collIDP == NULL) {
	    fprintf(stderr,
		    "[ %s ] failed to allocate a measely long word.Argh!\n",
		    rn);
	    return (-1);
	}
	*collIDP = 2;		/* USE A macro for this */

	CMinitFlags = 0;
	if (afsmon_onceOnly)	/* once only ? */
	    CMinitFlags |= XSTAT_CM_INITFLAG_ONE_SHOT;

	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Calling xstat_cm_Init \n", rn);
	    fflush(debugFD);
	}

	code = xstat_cm_Init(numCM,	/*Num servers */
			     CMSktArray,	/*Cache Manager  socket array */
			     afsmon_probefreq,	/*probe frequency */
			     afsmon_CM_Handler,	/*Handler routine */
			     CMinitFlags,	/*Initialization flags */
			     numCollIDs,	/*Number of collection IDs */
			     collIDP);	/*Ptr to collection ID */

	if (code) {
	    fprintf(stderr, "[ %s ] xstat_cm_init returned error\n", rn);
	    afsmon_Exit(130);
	}
    }



    /* end of process cache manager entries */
    /* if only one probe was required setup a waiting process for the 
     * termination signal */
    if (afsmon_onceOnly) {
	code = LWP_WaitProcess(&terminationEvent);
	if (code) {
	    if (afsmon_debug) {
		fprintf(debugFD, "LWP_WaitProcess() returned error %d\n",
			code);
		fflush(debugFD);
	    }
	    afsmon_Exit(135);
	}
    }

    /* start the gtx input server */
    code = gtx_InputServer(afsmon_win);
    if (code) {
	fprintf(stderr, "[ %s ] Failed to start input server \n", rn);
	afsmon_Exit(140);
    }

    /* This part of the code is reached only if the input server is not started
     * for debugging purposes */

    /* sleep forever */
    tv.tv_sec = 24 * 60;
    tv.tv_usec = 0;
    fprintf(stderr, "[ %s ] going to sleep ...\n", rn);
    while (1) {
	code = IOMGR_Select(0,	/*Num fds */
			    0,	/*Descriptors ready for reading */
			    0,	/*Descriptors ready for writing */
			    0,	/*Descriptors with exceptional conditions */
			    &tv);	/*Timeout structure */
	if (code) {
	    fprintf(stderr,
		    "[ %s ] IOMGR_Select() returned non-zero value %d\n", rn,
		    code);
	    afsmon_Exit(145);
	}
    }				/* while sleep */
}


/*-----------------------------------------------------------------------
 * afsmonInit()
 *
 * Description:
 *	Afsmonitor initialization routine.
 *	- processes command line parameters
 *	- call functions to:
 *		- process config file
 *		- initialize circular buffers and display buffers
 *		- initialize gtx
 *		- execute afsmonitor
 *	- initialize the display maps [fs/cm]_Display_map[].
 *
 * Returns:
 *	Success: Does not return from the call to afsmon_execute().
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
afsmonInit(as)
     struct cmd_syndesc *as;
{				/* afsmonInit() */

    static char rn[] = "afsmonInit";	/* Routine name */
    char *debug_filename;	/* pointer to debug filename */
    FILE *outputFD;		/* output file descriptor */
    struct cmd_item *hostPtr;	/* ptr to parse command line args */
    char buf[256];		/* buffer for processing hostnames */
    int code;
    int i;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, as= %d\n", rn, as);
	fflush(debugFD);
    }

    /* Open  the debug file if -debug option is specified */
    if (as->parms[P_DEBUG].items != 0) {
	afsmon_debug = 1;
	debug_filename = as->parms[P_DEBUG].items->data;
	debugFD = fopen(debug_filename, "w");
	if (debugFD == (FILE *) 0) {
	    printf("[ %s ] Failed to open debugging file %s for writing\n",
		   rn, "log");
	    afsmon_debug = 0;
	    afsmon_Exit(150);
	}
    }

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
    }


    /* use curses always until we support other packages */
#ifdef notdef
    wpkg_to_use = atoi(as->parms[P_PACKAGE].items->data);

    switch (wpkg_to_use) {
    case GATOR_WIN_CURSES:
	fprintf(stderr, "curses\n");
	break;
    case GATOR_WIN_DUMB:
	fprintf(stderr, "dumb terminal\n");
	break;
    case GATOR_WIN_X11:
	fprintf(stderr, "X11\n");
	break;
    default:
	fprintf(stderr, "Illegal graphics package: %d\n", wpkg_to_use);
	afsmon_Exit(155);
    }				/*end switch (wpkg_to_use) */
#endif

    wpkg_to_use = GATOR_WIN_CURSES;

    /* get probe frequency . We check for meaningful bounds on the frequency
     * and reset to the default value if needed. The upper bound of 24
     * hours looks ridiculous though! */

    afsmon_probefreq = 0;
    if (as->parms[P_FREQUENCY].items != 0)
	afsmon_probefreq = atoi(as->parms[P_FREQUENCY].items->data);
    else
	afsmon_probefreq = DEFAULT_FREQUENCY;

    if (afsmon_probefreq <= 0 || afsmon_probefreq > 24 * 60 * 60) {
	afsmon_probefreq = DEFAULT_FREQUENCY;
	if (afsmon_debug) {
	    fprintf(debugFD,
		    "[ %s ] Invalid probe frequency %s specified, resetting to default value %d seconds\n",
		    rn, as->parms[P_FREQUENCY].items->data, afsmon_probefreq);
	    fflush(debugFD);
	}
	fprintf(stderr,
		"Invalid probe frequency %s specified, resetting to default value %d seconds\n",
		as->parms[P_FREQUENCY].items->data, afsmon_probefreq);
	sleep(3);
    }


    /* make sure output file is writable, else complain now */
    /* we will open and close it as needed after probes */

    if (as->parms[P_OUTPUT].items != 0) {
	afsmon_output = 1;	/* output flag */
	strncpy(output_filename, as->parms[P_OUTPUT].items->data, 80);
	outputFD = fopen(output_filename, "a");
	if (outputFD == (FILE *) 0) {
	    fprintf(stderr, "Failed to open output file %s \n",
		    output_filename);
	    if (afsmon_debug) {
		fprintf(debugFD, "[ %s ] Failed to open output file %s \n",
			rn, output_filename);
		afsmon_Exit(160);
	    }
	}
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] output file is %s\n", rn,
		    output_filename);
	}
	fclose(outputFD);
    }

    /* detailed statistics to storage file */
    if (as->parms[P_DETAILED].items != 0) {
	if (as->parms[P_OUTPUT].items == 0) {
	    fprintf(stderr,
		    "-detailed switch can be used only with -output\n");
	    afsmon_Exit(165);
	}
	afsmon_detOutput = 1;
    }

    /* Initialize host list headers */
    FSnameList = (struct afsmon_hostEntry *)0;
    CMnameList = (struct afsmon_hostEntry *)0;

    /* The -config option is mutually exclusive with the -fshosts,-cmhosts 
     * options */

    if (as->parms[P_CONFIG].items) {
	if (as->parms[P_FSHOSTS].items || as->parms[P_CMHOSTS].items) {
	    fprintf(stderr,
		    "Cannot use -config option with -fshosts or -cmhosts\n");
	    afsmon_Exit(170);
	}
    } else {
	if (!as->parms[P_FSHOSTS].items && !as->parms[P_CMHOSTS].items) {
	    fprintf(stderr,
		    "Must specify either -config or (-fshosts and/or -cmhosts) options \n");
	    afsmon_Exit(175);
	}
    }


    /* If a file server host is specified on the command line we reuse
     * parse_hostEntry() function . Just the pass the info as if it were 
     * read off the config file */

    if (as->parms[P_FSHOSTS].items) {
	hostPtr = as->parms[P_FSHOSTS].items;
	while (hostPtr != (struct cmd_item *)0) {
	    sprintf(buf, "fs %s", hostPtr->data);
	    code = parse_hostEntry(buf);
	    if (code) {
		fprintf(stderr, "Could not parse %s\n", hostPtr->data);
		afsmon_Exit(180);
	    }

	    hostPtr = hostPtr->next;
	}
    }

    /* same as above for -cmhosts */
    if (as->parms[P_CMHOSTS].items) {
	hostPtr = as->parms[P_CMHOSTS].items;
	while (hostPtr != (struct cmd_item *)0) {
	    sprintf(buf, "cm %s", hostPtr->data);
	    code = parse_hostEntry(buf);
	    if (code) {
		fprintf(stderr, "Could not parse %s\n", hostPtr->data);
		afsmon_Exit(185);
	    }

	    hostPtr = hostPtr->next;
	}
    }

    /* number of slots in circular buffers */
    if (as->parms[P_BUFFERS].items)
	num_bufSlots = atoi(as->parms[P_BUFFERS].items->data);
    else
	num_bufSlots = DEFAULT_BUFSLOTS;

    /* Initialize xx_showFlags[]. This array is used solely for processing the
     * "show" directives in the config file in parse_showEntries()  */
    for (i = 0; i < NUM_FS_STAT_ENTRIES; i++)
	fs_showFlags[i] = 0;
    for (i = 0; i < NUM_CM_STAT_ENTRIES; i++)
	cm_showFlags[i] = 0;


    /* Process the configuration file if given. This initializes among other
     * things, the list of FS & CM names in FSnameList and CMnameList */

    if (as->parms[P_CONFIG].items)
	process_config_file(as->parms[P_CONFIG].items->data);

    /* print out the FS and CM lists */
    print_FS();
    print_CM();

    /* Initialize the FS results-to-screen map array if there were no "show fs"
     * directives in the config file */
    if (fs_showDefault) {
	for (i = 0; i < NUM_FS_STAT_ENTRIES; i++)
	    fs_Display_map[i] = i;
	fs_DisplayItems_count = NUM_FS_STAT_ENTRIES;
    }

    /* Initialize the CM results-to-screen map array if there were no "show cm"
     * directives in the config file */
    if (cm_showDefault) {
	for (i = 0; i < NUM_CM_STAT_ENTRIES; i++)
	    cm_Display_map[i] = i;
	cm_DisplayItems_count = NUM_CM_STAT_ENTRIES;
    }



    /* setup an interrupt signal handler; we ain't wanna leak core  */
    /* this binding is useful only until gtx is initialized after which the
     * keyboard input server takes over. */
    if ((signal(SIGINT, quit_signal)) == SIG_ERR) {
	perror("signal() failed.");
	afsmon_Exit(190);
    }


    /* init error message buffers. these will be used to print error messages
     * once gtx is initialized and there is no access to stderr/stdout */
    errMsg[0] = '\0';
    errMsg1[0] = '\0';

    if (num_bufSlots) {

	/* initialize fs and cm circular buffers before initiating probes */
	if (numFS) {
	    code = init_fs_buffers();
	    if (code) {
		fprintf(stderr, "[ %s ] init_fs_buffers returned %d\n", rn,
			code);
		afsmon_Exit(195);
	    }
	}

	if (numCM) {
	    code = init_cm_buffers();
	    if (code) {
		fprintf(stderr, "[ %s ] init_cm_buffers returned %d\n", rn,
			code);
		afsmon_Exit(200);
	    }
	}
    }

    /* allocate and initialize buffers for holding fs & cm results in ascii
     * format suitable for updating the screen */
    code = init_print_buffers();
    if (code) {
	fprintf(stderr, "[ %s ] init_print_buffers returned %d\n", rn, code);
	afsmon_Exit(205);
    }

    /* perform gtx initializations */
    code = gtx_initialize();
    if (code) {
	fprintf(stderr, "[ %s ] gtx_initialize returned %d\n", rn, code);
	afsmon_Exit(210);
    }

    /* start xstat probes */
    afsmon_execute();

    return (0);			/* will not return from the call to afsmon_execute() */

}				/* afsmonInit() */


/*-----------------------------------------------------------------------
 * Function:	main()
 ------------------------------------------------------------------------*/

#include "AFS_component_version_number.c"

int
main(argc, argv)
     int argc;
     char **argv;
{				/* main() */

    static char rn[] = "main";	/* routine name */
    afs_int32 code;		/*Return code */
    struct cmd_syndesc *ts;	/*Ptr to cmd line syntax descriptor */

#ifdef  AFS_AIX32_ENV
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

    /*
     * Set up the commands we understand.
     */
    ts = cmd_CreateSyntax("initcmd", afsmonInit, 0, "initialize the program");
    cmd_AddParm(ts, "-config", CMD_SINGLE, CMD_OPTIONAL,
		"configuration file");
    cmd_AddParm(ts, "-frequency", CMD_SINGLE, CMD_OPTIONAL,
		"poll frequency, in seconds");
    cmd_AddParm(ts, "-output", CMD_SINGLE, CMD_OPTIONAL, "storage file name");
    cmd_AddParm(ts, "-detailed", CMD_FLAG, CMD_OPTIONAL,
		"output detailed statistics to storage file");
#ifdef notdef
    /* we hope to use this .... eventually! */
    cmd_AddParm(ts, "-package", CMD_SINGLE, CMD_REQUIRED,
		"Graphics Package to use");
#endif
    cmd_AddParm(ts, "-debug", CMD_SINGLE, CMD_OPTIONAL,
		"turn debugging output on to the named file");
    cmd_AddParm(ts, "-fshosts", CMD_LIST, CMD_OPTIONAL,
		"list of file servers to monitor");
    cmd_AddParm(ts, "-cmhosts", CMD_LIST, CMD_OPTIONAL,
		"list of cache managers to monitor");
    cmd_AddParm(ts, "-buffers", CMD_SINGLE, CMD_OPTIONAL,
		"number of buffer slots");

    /*
     * Parse command-line switches & execute afsmonitor 
     */

    code = cmd_Dispatch(argc, argv);
    if (code)
	afsmon_Exit(1);
    else
	afsmon_Exit(2);

    exit(0);			/* redundant, but gets rid of warning */
}				/*main */
