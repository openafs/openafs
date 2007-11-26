/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Afsmon-win: Curses interface for the Afsmonitor using the gtx library.
 *
 *-------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <cmd.h>
#include <string.h>
#undef IN
#include <time.h>

#include <gtxwindows.h>		/*Generic window package */
#include <gtxobjects.h>		/*Object definitions */
#if 0
#include <gtxtextcb.h>		/*Text object circular buffer interface */
#include <gtxtextobj.h>		/*Text object interface */
#endif
#include <gtxlightobj.h>	/*Light object interface */
#include <gtxcurseswin.h>	/*Curses window package */
#include <gtxdumbwin.h>		/*Dumb terminal window package */
#include <gtxX11win.h>		/*X11 window package */
#include <gtxframe.h>		/*Frame package */

#include <afs/xstat_fs.h>
#include <afs/xstat_cm.h>


#include "afsmonitor.h"
#include "afsmon-labels.h"


/* afsmonitor version number */
static char afsmon_version[] = "1.0";

/* EXTERNAL VARIABLES (from afsmonitor.c) */

extern int afsmon_debug;	/* debug info to file ? */
extern FILE *debugFD;		/* debugging file descriptor */
extern char errMsg[256];	/* buffers used to print error messages after */
extern char errMsg1[256];	/* gtx is initialized (stderr/stdout gone !) */

/* number of fileservers and cache managers to monitor */
extern int numFS;
extern int numCM;

/* number of FS alerts and number of hosts on FS alerts */
extern int num_fs_alerts;
extern int numHosts_onfs_alerts;

/* number of CM alerts and number of hosts on FS alerts */
extern int num_cm_alerts;
extern int numHosts_oncm_alerts;

/* ptr to array holding the results of FS probes in ascii format */
extern struct fs_Display_Data *prev_fsData;

/* ptr to array holding the results of CM probes in ascii format */
extern struct cm_Display_Data *prev_cmData;

extern int afsmon_fs_curr_probeNum;	/* current fs probe number */
extern int afsmon_fs_prev_probeNum;	/* previous fs probe number */
extern int afsmon_cm_curr_probeNum;	/* current cm probe number */
extern int afsmon_cm_prev_probeNum;	/* previous cm probe number */

extern int afsmon_probefreq;	/* probe frequency */

/* map of fs results items we must display. This array contains indices to
the fs_varNames[] array corresponding to the stats we want to display. It is
initialized while processing the config file */

extern short fs_Display_map[XSTAT_FS_FULLPERF_RESULTS_LEN];
extern int fs_DisplayItems_count;	/* number of items to display */

extern short cm_Display_map[XSTAT_FS_FULLPERF_RESULTS_LEN];
extern int cm_DisplayItems_count;	/* number of items to display */


/* GTX VARIABLES */

/* minimum window size */
#define MINX 80
#define MINY 12

/* justifications */
#define RIGHT_JUSTIFY 0
#define LEFT_JUSTIFY 1
#define CENTER 2

/* width of overview frame objects */
/* field widths include the terminating null character */

#define PROGNAME_O_WIDTH (maxX/2)	/* program name object */
#define OVW_PAGENUM_O_WIDTH	29	/* page number object */
#define OVW_PROBENUM_O_WIDTH  39	/* probe number object */
#define OVW_CMD_O_WIDTH  (maxX/2)	/* cmd line object */
#define OVW_NUMFS_O_WIDTH	40	/* num FSs monitored */
#define OVW_NUMCM_O_WIDTH	40	/* num CMs monitored */
#define OVW_FSALERTS_O_WIDTH 40	/* num FS alerts */
#define OVW_CMALERTS_O_WIDTH 40	/* num CM alerts */
#define OVW_HOSTNAME_O_WIDTH (maxX / 2)	/* FS & CM host names */
#define OVW_HOSTNAME_O_WIDTH_HGL 30	/* cosmetic, atleast this many chars
					 * will be highlightned */

/* widths of FS and CM frame objects */
#define FC_NUMHOSTS_O_WIDTH  (maxX - 8)	/* number of fs monitored. leave 4
					 * chars on either side for '<<','>>' */
#define FC_PAGENUM_O_WIDTH 43
#define FC_HOSTNAME_O_WIDTH 11	/* width of FS hostnames column */
#define FC_CMD_O_WIDTH    55	/* width of cmd line */
#define FC_PROBENUM_O_WIDTH 30	/* width of probe number object */
#define FC_ARROWS_O_WIDTH    4	/* width of arrow indicators */
#define FC_COLUMN_WIDTH    11	/* width of data columns */

/* bit definitions for use in resolving command line */
/* these bits are set in the xx_pageType variables to indicate what commands
are acceptable */

#define CMD_NEXT  1		/* next page ? */
#define CMD_PREV  2		/* previous page ? */
#define CMD_LEFT  4		/* left scroll ? */
#define CMD_RIGHT 8		/* right scroll ? */
#define CMD_FS    16		/* file servers frame exists ? */
#define CMD_CM    32		/* cache managers frame exists ? */


#define FC_NUM_FIXED_LINES 10	/* number of fixed lines */
#define FC_FIRST_HOST_ROW 8	/* first host entry row number */
#define FC_FIRST_LABEL_ROW 4	/* first label row number */

/* number of fixed lines (that dont change) on the overview screen */
#define OVW_NUM_FIXED_LINES 7
#define OVW_FIRST_HOST_ROW 5	/* row index of first host entry in ovw frame */

#define HIGHLIGHT 1		/* highlight object? */

static char blankline[256];	/* blank line */

/* maximum X & Y coordinates of the frames */
int maxX;
int maxY;

struct gwin *afsmon_win;	/* afsmonitor window */
int gtx_initialized = 0;

/* Overview screen related definitions */

struct gtx_frame *ovwFrame;	/* overview screen frame */
struct gwin_sizeparams frameDims;	/* frame dimensions. all frames have
					 * same dimensions */

/* overview frame object names */
struct onode *ovw_progName_o;	/* program name object */
struct onode *ovw_pageNum_o;	/* page number onject */
struct onode *ovw_cmd_o;	/* command line object */
struct onode *ovw_probeNum_o;	/* probe number object */
struct onode *ovw_numFS_o;	/* num FS monitored */
struct onode *ovw_numCM_o;	/* num CM monitored */
struct onode *ovw_FSalerts_o;	/* nunber of FS alerts */
struct onode *ovw_CMalerts_o;	/* nunber of CM alerts */
struct onode *initMsg_o;	/* initialization message */

/* number of pages of data for the overview frame */
int ovw_numPages = 0;
int ovw_currPage = 1;		/* current page number */

static int ovw_pageType = 0;	/* one of the above types */

/* number of rows of server names that can be displayed on one overview page*/
int ovw_numHosts_perPage;

/* ptr to a block of ovw_numHosts_perPage number of objects for file servers */
struct onode **ovw_fsNames_o;
/*ptr to a block of ovw_numHosts_perPage number of objects for cache managers */
struct onode **ovw_cmNames_o;

/* When the ovw_refresh routine is called by the keyboard handlers the
following variable is used to determine if fs/cm/fs&cm info must be updated */
int ovw_update_info = 0;

/* Variables needed to display an intialization message on startup */
static char *initMsg = "AFSMonitor Collecting Statistics ...";
static int initMsg_on = 0;	/* message on ? */

/* FILE SERVER Screen related definitions */

struct gtx_frame *fsFrame;	/* File Server screen frame */

struct onode *fs_pageNum_o;	/* fs page number object */
struct onode *fs_cmd_o;		/* fs command line object */
struct onode *fs_probeNum_o;	/* fs probe number object */
struct onode *fs_numFS_o;	/* fs number of FSs object */
struct onode *fs_leftArrows_o;	/* fs cols on left signal object */
struct onode *fs_rightArrows_o;	/* fs cols on right signal object */
struct onode **fs_hostNames_o;	/* ptr to host names objects */

/* bit-map to characterize page type and contents of command prompt */
static int fs_pageType = 0;

/* coordinates for the File Servers screen */

/* we use page numbers to navigate vertically (ie, across hosts) and column
numbers to navigate horizontally */

int fs_numHosts_perPage;	/* number of file servers per page */
int fs_cols_perPage;		/* number of data columns per file server page */
int fs_currPage;		/* current FS page number */
int fs_numPages;		/* number of FS pages */

/* column numbers are index to the mapping structure fs_Display_map. this
map contains the indices of datums that should be displayed */

int fs_numCols;			/* number of columns of FS data (excluding hostname) */
			/* this is the same as fs_DisplayItems_count */
/* following column indices run from 1 to (fs_numCols -1) */
int fs_curr_LCol = 0;		/* column number of leftmost column on display */
int fs_curr_RCol = 0;		/* column number of rightmost column on display */
int fs_Data_Available = 0;	/* atleast one fs probe cycle completed ? */


/* structure that defines a line of data in the fs/cm frames */

/* we store each datum value in two objects, one below the other. The reason
for doing this is to conserve screen area. most of the datums are just longs
and will fit into one object. some of them are timing values and require 13
characters to be displayed - such fields may overflow to the second object
placed below the first one. */

struct ServerInfo_line {
    struct onode *host_o;	/* hostname object */
    struct onode **data_o[2];	/* ptrs to two arrays of data objects. */

};

struct ServerInfo_line *fs_lines;	/* ptr to the file server data objects */

/* file server label onodes - three rows of them */
struct onode **fsLabels_o[3];

/* CACHE MANAGER Screen related definitions */

struct gtx_frame *cmFrame;	/* Cache Manager screen frame */

struct onode *cm_pageNum_o;	/* cm page number object */
struct onode *cm_cmd_o;		/* cm command line object */
struct onode *cm_probeNum_o;	/* cm probe number object */
struct onode *cm_numCM_o;	/* cm number of FSs object */
struct onode *cm_leftArrows_o;	/* fs cols on left signal object */
struct onode *cm_rightArrows_o;	/* fs cols on right signal object */

struct onode **cm_hostNames_o;	/* ptr to host names objects */

/* bit-map to characterize page type and contents of command prompt */
static int cm_pageType = 0;

/* coordinates for the Cache Managers screen */

/* we use page numbers to navigate vertically (ie, across hosts) and column
numbers to navigate horizontally */

int cm_numHosts_perPage;	/* number of cache managers per page */
int cm_cols_perPage;		/* number of data columns per file server page */
int cm_currPage;		/* current CM page number */
int cm_numPages;		/* number of CM pages */

/* column numbers are index to the mapping structure cm_Display_map. this
map contains the indices of datums that should be displayed */

int cm_numCols;			/* number of columns of FS data (excluding hostname) */
			/* this is the same as cm_DisplayItems_count */
/* following column indices run from 1 to (cm_numCols -1) */
int cm_curr_LCol = 0;		/* column number of leftmost column on display */
int cm_curr_RCol = 0;		/* column number of rightmost column on display */
int cm_Data_Available = 0;	/* atleast one cm probe cycle completed ? */


/* structure that defines a line of data in the fs/cm frames */
struct ServerInfo_line *cm_lines;	/* ptr to the file server data objects */

/* file server label onodes - three rows of them */
struct onode **cmLabels_o[3];



/*------------------------------------------------------------------------
 * initLightObject
 *
 * Description:
 *	Create and initialize a light onode according to the given
 *	parameters.
 *	( Borrowed from scout.c )
 *
 * Arguments:
 *	char *a_name	   : Ptr to the light's string name.
 *	int a_x		   : X offset.
 *	int a_y		   : Y offset.
 *	int a_width	   : Width in chars.
 *	struct gwin *a_win : Ptr to window structure.
 *
 * Returns:
 *	Ptr to new light onode on success,
 *	A null pointer otherwise.
 *
 * Environment:
 *	See above.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static struct onode *
initLightObject(a_name, a_x, a_y, a_width, a_win)
     char *a_name;
     int a_x;
     int a_y;
     int a_width;
     struct gwin *a_win;

{				/*initLightObject */

    static char rn[] = "initLightObject";	/*Routine name */
    struct onode *newlightp;	/*Ptr to new light onode */
    struct gator_light_crparams light_crparams;	/*Light creation params */
    char *truncname;		/*Truncated name, if needed */
    int name_len;		/*True length of name */

/* the following debug statement floods the debug file */
#ifdef DEBUG_DETAILED
    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, a_name= %s, a_x= %d, a_y= %d, a_width= %d, a_win= %d\n",
		rn, a_name, a_x, a_y, a_width, a_win);
	fflush(debugFD);
    }
#endif

    newlightp = NULL;

    /*
     * Set up the creation parameters according to the information we've
     * received.
     */
    light_crparams.onode_params.cr_type = GATOR_OBJ_LIGHT;
    name_len = strlen(a_name);

    if (name_len <= a_width)
	sprintf(light_crparams.onode_params.cr_name, "%s", a_name);
    else {
	/*
	 * We need to truncate the given name, leaving a `*' at the end to
	 * show us it's been truncated.
	 */
	truncname = light_crparams.onode_params.cr_name;
	strncpy(truncname, a_name, a_width - 1);
	truncname[a_width - 1] = '*';
	truncname[a_width] = 0;
    }
    light_crparams.onode_params.cr_x = a_x;
    light_crparams.onode_params.cr_y = a_y;
    light_crparams.onode_params.cr_width = a_width;
    light_crparams.onode_params.cr_height = 1;
    light_crparams.onode_params.cr_window = a_win;
    light_crparams.onode_params.cr_home_obj = NULL;
    light_crparams.onode_params.cr_prev_obj = NULL;
    light_crparams.onode_params.cr_parent_obj = NULL;
    light_crparams.onode_params.cr_helpstring = NULL;

    light_crparams.appearance = 0;
    light_crparams.flashfreq = 0;
    sprintf(light_crparams.label, "%s", a_name);
    light_crparams.label_x = 0;
    light_crparams.label_y = 0;

    newlightp =
	gator_objects_create((struct onode_createparams *)(&light_crparams));

    /*
     * Return the news, happy or not.
     */
    return (newlightp);

}				/*initLightObject */



/*------------------------------------------------------------------------
 * justify_light
 *
 * Description:
 *	Place the chars in the source buffer into the target buffer
 *	with the desired justification, either centered, left-justified
 *	or right-justified.  Also, support inidication of truncation
 *	with a star (*), either on the left or right of the string,
 *	and whether we're justifying a labeled disk quantity.
 *
 *	(derived from mini_justify() in scout.c)
 *
 * Arguments:
 *	char *a_srcbuff	    : Ptr to source char buffer.
 *	char *a_dstbuff	    : Ptr to dest char buffer.
 *	int a_dstwidth	    : Width of dest buffer in chars.
 *	int a_justification : Kind of justification.
 *	int a_rightTrunc    : If non-zero, place the truncation char
 *			      on the right of the string.  Otherwise,
 *			      place it on the left.
 *
 * Returns:
 *	0 on success,
 *	-1 otherwise.
 *
 *------------------------------------------------------------------------*/

int
justify_light(a_srcbuff, a_dstbuff, a_dstwidth, a_justification, a_rightTrunc)
     char *a_srcbuff;
     char *a_dstbuff;
     int a_dstwidth;
     int a_justification;
     int a_rightTrunc;

{				/*justify_light */

    static char rn[] = "justify_light";	/*Routine name */
    int leftpad_chars;		/*# of chars for left-padding */
    int num_src_chars;		/*# of chars in source */
    int true_num_src_chars;	/*# src chars before truncation */
    int trunc_needed;		/*Is truncation needed? */


/* the following debug statement floods the debug file */
#ifdef DEBUG_DETAILED
    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, a_srcbuff= %s, a_dstbuff= %d, a_dstwidth= %d, a_justification= %d, a_rightTrunc= %d\n",
		rn, a_srcbuff, a_dstbuff, a_dstwidth, a_justification,
		a_rightTrunc);
	fflush(debugFD);
    }
#endif


    /*
     * If the destination width will overrun the gtx string storage,
     * we automatically shorten up.
     */
    if (a_dstwidth > GATOR_LABEL_CHARS) {
	/* 
	 * if (afsmon_debug) {
	 * fprintf(debugFD,
	 * "[%s] Dest width (%d) > gtx buflen (%d), shrinking dest width\n",
	 * rn, a_dstwidth, GATOR_LABEL_CHARS);
	 * fflush(debugFD);
	 * }
	 */
	a_dstwidth = GATOR_LABEL_CHARS;
    }

    /*
     * If our source string is too long, prepare for truncation.
     */
    true_num_src_chars = strlen(a_srcbuff);
    if (true_num_src_chars >= a_dstwidth) {
	trunc_needed = 1;
	num_src_chars = a_dstwidth - 1;
	leftpad_chars = 0;
	if (!a_rightTrunc)
	    a_srcbuff += (true_num_src_chars - num_src_chars);
    } else {
	trunc_needed = 0;
	num_src_chars = true_num_src_chars;

	/*
	 * Compute the necessary left-padding.
	 */
	switch (a_justification) {

	case RIGHT_JUSTIFY:
	    leftpad_chars = (a_dstwidth - 1) - num_src_chars;
	    break;

	case LEFT_JUSTIFY:
	    /*
	     * This is the really easy one.
	     */
	    leftpad_chars = 0;
	    break;

	case CENTER:
	    leftpad_chars = ((a_dstwidth - 1) - num_src_chars) / 2;
	    break;

	default:
	    if (afsmon_debug) {
		fprintf(debugFD, "[%s] Illegal justification command: %d", rn,
			a_justification);
		fprintf(debugFD, "[%s] Called with '%s', dest width=%d\n", rn,
			a_srcbuff, a_dstwidth);
		fflush(debugFD);
	    }
	    return (-1);
	}			/*Switch on justification type */
    }

    /*
     * Clear out the dest buffer, then place the source string at the
     * appropriate padding location.  Remember to place a string
     * terminator at the end of the dest buffer, plus whatever truncation
     * may be needed.  If we're left-truncating, we've already shifted
     * the src buffer appropriately.
     */
    strncpy(a_dstbuff, blankline, a_dstwidth);
    strncpy(a_dstbuff + leftpad_chars, a_srcbuff, num_src_chars);
    *(a_dstbuff + a_dstwidth - 1) = '\0';
    if (trunc_needed) {
	if (a_rightTrunc)
	    *(a_dstbuff + a_dstwidth - 2) = '*';	/*Truncate on the right */
	else {
	    *a_dstbuff = '*';	/*Truncate on the left, non-disk */
	}
    }

    /*Handle truncations */
    /*
     * Return the good news.
     */
    return (0);

}				/*justify_light */



/*-----------------------------------------------------------------------
 * afsmonExit_gtx()
 *
 * Description:
 *	Call the exit routine. This function is mapped
 *	to the keys Q and  in all the frames and is called by the 
 * 	gtx input server.
 *----------------------------------------------------------------------*/

int
afsmonExit_gtx()
{
    static char rn[] = "afsmonExit_gtx";

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    afsmon_Exit(0);
    return 0; /* not reached */
}


/*-----------------------------------------------------------------------
 * ovw_refresh()
 *
 * Description:
 *	Refresh the overview screen with the contents of the specified page.
 *	There are two parts to the overview screen - the file server column
 *	and the cache manager column and each of them is independent of the
 *	other. Hence it takes as an argumnet the "type" of update to be
 * 	performed.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
ovw_refresh(a_pageNum, a_updateType)
     int a_pageNum;		/* page to refresh overview display */
     int a_updateType;		/* OVW_UPDATE_FS = update fs column only,
				 * OVW_UPDATE_CM = update cm column only,
				 * OVW_UPDATE_BOTH = update fs & cm columns. Note that 
				 * we do not want to update a column until the 
				 * corresponding probe cycle has completed */
{				/* ovw_refresh */

    static char rn[] = "ovw_refresh";	/* routine name */
    struct onode **tmp_fsNames_o;	/* ptr to fsNames onodes */
    struct gator_lightobj *tmp_lightobj;	/* ptr for object's display data */
    struct fs_Display_Data *fsDataP;	/* ptr to FS display data array */
    struct onode **tmp_cmNames_o;	/* ptr to fsNames onodes */
    struct cm_Display_Data *cmDataP;	/* ptr to CM display data array */
    int fsIdx;			/* for counting # of CM hosts */
    int cmIdx;			/* for counting # of CM hosts */
    int next_page = 0;		/* is there a next ovw page ? */
    int prev_page = 0;		/* is there a previous ovw page */
    char cmdLine[80];		/* buffer for command line */
    char printBuf[256];		/* buffer to print to screen */
    int i;
    int code;
    int len;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_pageNum= %d, a_updateType= %d\n",
		rn, a_pageNum, a_updateType);
	fflush(debugFD);
    }

    /* if the data is not yet available  ie., not one probe cycle has 
     * completed, do nothing */

    if ((a_updateType & OVW_UPDATE_FS) && !fs_Data_Available)
	return (0);
    if ((a_updateType & OVW_UPDATE_CM) && !cm_Data_Available)
	return (0);


    /* validate page number */
    if (a_pageNum < 1 || a_pageNum > ovw_numPages) {
	sprintf(errMsg, "[ %s ] called with incorrect page number %d\n", rn,
		a_pageNum);
	afsmon_Exit(235);
    }

    /* set the current page number */
    ovw_currPage = a_pageNum;

    /* turn off init message */
    if (initMsg_on) {
	initMsg_on = 0;
	gtxframe_RemoveFromList(ovwFrame, initMsg_o);
    }

    /* update the labels */

    /* version label */
    tmp_lightobj = (struct gator_lightobj *)ovw_progName_o->o_data;
    sprintf(printBuf, "AFSMonitor [Version %s]", afsmon_version);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);
    gator_light_set(ovw_progName_o, 1);

    /* page number label */
    tmp_lightobj = (struct gator_lightobj *)ovw_pageNum_o->o_data;
    sprintf(printBuf, "[System Overview, p. %d of %d]", ovw_currPage,
	    ovw_numPages);
    justify_light(printBuf, tmp_lightobj->label, OVW_PAGENUM_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(ovw_pageNum_o, 1);

    /* file servers monitored label */
    tmp_lightobj = (struct gator_lightobj *)ovw_numFS_o->o_data;
    sprintf(printBuf, "    %d File Servers monitored", numFS);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);

    /* cache managers monitored label */
    tmp_lightobj = (struct gator_lightobj *)ovw_numCM_o->o_data;
    sprintf(printBuf, "    %d Cache Managers monitored", numCM);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);


    /* no. of fs alerts label */
    tmp_lightobj = (struct gator_lightobj *)ovw_FSalerts_o->o_data;
    sprintf(printBuf, "    %d alerts on %d machines", num_fs_alerts,
	    numHosts_onfs_alerts);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);

    /* no. of cm alerts label */
    tmp_lightobj = (struct gator_lightobj *)ovw_CMalerts_o->o_data;
    sprintf(printBuf, "    %d alerts on %d machines", num_cm_alerts,
	    numHosts_oncm_alerts);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);

    /* command line */
    /* determine if we have fs/cm, more/previous pages of data to display */

    ovw_pageType = 0;
    if (numFS && fs_Data_Available)
	ovw_pageType |= CMD_FS;	/* we have an fs frame & data avail. */
    if (numCM && cm_Data_Available)
	ovw_pageType |= CMD_CM;	/* we have a cm frame & data avail. */
    if (ovw_currPage > 1)
	ovw_pageType |= CMD_PREV;	/* got a previous page */
    if (ovw_currPage < ovw_numPages)
	ovw_pageType |= CMD_NEXT;	/* got a next page */

    strcpy(cmdLine, "Command [");
    if ((ovw_pageType & CMD_FS) && (ovw_pageType & CMD_CM))
	strcat(cmdLine, "fs, cm");
    else if (ovw_pageType & CMD_FS)
	strcat(cmdLine, "fs");
    else if (ovw_pageType & CMD_CM)
	strcat(cmdLine, "cm");

    if (ovw_pageType & CMD_PREV)
	strcat(cmdLine, ", prev");
    if (ovw_pageType & CMD_NEXT)
	strcat(cmdLine, ", next");

    strcat(cmdLine, "]? ");




    /* display the command line */
    tmp_lightobj = (struct gator_lightobj *)ovw_cmd_o->o_data;
    sprintf(printBuf, "%s", cmdLine);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);
    gator_light_set(ovw_cmd_o, 1);

    /* display probe numbers line */
    tmp_lightobj = (struct gator_lightobj *)ovw_probeNum_o->o_data;
    sprintf(printBuf, "[probes %d(fs) %d(cm), freq=%d sec]",
	    afsmon_fs_prev_probeNum, afsmon_cm_prev_probeNum,
	    afsmon_probefreq);
    justify_light(printBuf, tmp_lightobj->label, OVW_PROBENUM_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(ovw_probeNum_o, 1);



    /* update the file server names column if we are asked to */

    if (numFS && (a_updateType & OVW_UPDATE_FS)) {

	/* move to the right spot in the FS display data array */
	fsDataP = prev_fsData;
	fsIdx = 0;
	for (i = 0; i < ((a_pageNum - 1) * ovw_numHosts_perPage); i++) {
	    fsDataP++;
	    fsIdx++;
	}

	/* get the address to the first FS name */
	tmp_fsNames_o = ovw_fsNames_o;

	for (i = 0; i < ovw_numHosts_perPage; i++) {
	    if (fsIdx < numFS) {	/* this could be the last & partial page */

		if (fsDataP->hostName[0] == '\0') {
		    sprintf(errMsg, "[ %s ] empty fs display entry \n", rn);
		    afsmon_Exit(240);
		}

		/* check if the probe succeeded. if it did check for thresholds
		 * overflow. A failed probe is indicated by "PF" */

		if (!fsDataP->probeOK) {
		    sprintf(printBuf, "[ PF] %s", fsDataP->hostName);
		} else if (fsDataP->ovfCount)	/* thresholds overflow */
		    sprintf(printBuf, "[%3d] %s", fsDataP->ovfCount,
			    fsDataP->hostName);
		else {
		    sprintf(printBuf, "      %s", fsDataP->hostName);

		}
		if (afsmon_debug)
		    fprintf(debugFD, "[ %s ] to display %s\n", rn, printBuf);

		tmp_lightobj =
		    (struct gator_lightobj *)(*tmp_fsNames_o)->o_data;

		if (strlen(printBuf) + 1 < OVW_HOSTNAME_O_WIDTH_HGL)
		    len = OVW_HOSTNAME_O_WIDTH_HGL;
		else
		    len = strlen(printBuf) + 1;

		code =
		    justify_light(printBuf, tmp_lightobj->label, len,
				  LEFT_JUSTIFY, 1);
		if (code) {
		    if (afsmon_debug) {
			fprintf(debugFD, "[ %s ] justify_code returned %d\n",
				rn, code);
		    }
		}

		/* highlighten if overflowed or probe failed */

		if (fsDataP->ovfCount || !fsDataP->probeOK)
		    code = gator_light_set(*tmp_fsNames_o, 1);
		else
		    code = gator_light_set(*tmp_fsNames_o, 0);


	    } else {		/* no more hosts, blank the rest of the entries */
		tmp_lightobj =
		    (struct gator_lightobj *)(*tmp_fsNames_o)->o_data;
		sprintf(tmp_lightobj->label, "%s", "");
	    }

	    tmp_fsNames_o++;	/* next onode */
	    fsDataP++;		/* next host's data */
	    fsIdx++;		/* host index */
	}
    }

    /* if numFS */
    /* if we have any cache managers, update them if we are asked to */
    if (numCM && (a_updateType & OVW_UPDATE_CM)) {

	/* move to the right spot in the CM display data array */
	cmDataP = prev_cmData;
	cmIdx = 0;
	for (i = 0; i < ((a_pageNum - 1) * ovw_numHosts_perPage); i++) {
	    cmDataP++;
	    cmIdx++;
	}

	/* get the address to the first CM name */
	tmp_cmNames_o = ovw_cmNames_o;

	for (i = 0; i < ovw_numHosts_perPage; i++) {
	    if (cmIdx < numCM) {	/* this could be the last & partial page */

		if (cmDataP->hostName[0] == '\0') {
		    sprintf(errMsg, "[ %s ] empty cm display entry \n", rn);
		    afsmon_Exit(245);
		}

		/* check if the probe succeeded. if it did check for thresholds
		 * overflow. A failed probe is indicated by "PF" */

		if (!cmDataP->probeOK) {
		    sprintf(printBuf, "[ PF] %s", cmDataP->hostName);
		} else if (cmDataP->ovfCount) {	/* thresholds overflow */
		    sprintf(printBuf, "[%3d] %s", cmDataP->ovfCount,
			    cmDataP->hostName);
		} else
		    sprintf(printBuf, "      %s", cmDataP->hostName);

		if (afsmon_debug)
		    fprintf(debugFD, "[ %s ] to display %s\n", rn, printBuf);

		tmp_lightobj =
		    (struct gator_lightobj *)(*tmp_cmNames_o)->o_data;

		if (strlen(printBuf) + 1 < OVW_HOSTNAME_O_WIDTH_HGL)
		    len = OVW_HOSTNAME_O_WIDTH_HGL;
		else
		    len = strlen(printBuf) + 1;

		code =
		    justify_light(printBuf, tmp_lightobj->label, len,
				  LEFT_JUSTIFY, 1);
		if (code) {
		    if (afsmon_debug) {
			fprintf(debugFD, "[ %s ] justify_code returned %d\n",
				rn, code);
		    }
		}

		/* highlighten if overflow or if probe failed */
		if (cmDataP->ovfCount || !cmDataP->probeOK)
		    code = gator_light_set(*tmp_cmNames_o, 1);
		else
		    code = gator_light_set(*tmp_cmNames_o, 0);


	    } else {		/* no more hosts, blank the rest of the entries */
		tmp_lightobj =
		    (struct gator_lightobj *)(*tmp_cmNames_o)->o_data;
		sprintf(tmp_lightobj->label, "%s", "");
	    }

	    tmp_cmNames_o++;	/* next onode */
	    cmDataP++;		/* next host's data */
	    cmIdx++;		/* host index */
	}
    }

    /* if numCM */
    /* redraw the display if the overview screen is currently displayed */
    if (afsmon_win->w_frame == ovwFrame)
	WOP_DISPLAY(afsmon_win);

    return (0);

}				/* ovw_refresh */



/*-----------------------------------------------------------------------
 * Switch_ovw_2_fs()
 *
 * Description:
 *		Switch from the overview screen to the FS screen
 *----------------------------------------------------------------------*/
int
Switch_ovw_2_fs()
{
    static char rn[] = "Switch_ovw_2_fs";

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    /* bind the File Server frame to the window */
    if (ovw_pageType & CMD_FS)
	gtxframe_SetFrame(afsmon_win, fsFrame);
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_ovw_2_cm()
 *
 * Description:
 *		Switch from the overview screen to the CM screen
 *----------------------------------------------------------------------*/
int
Switch_ovw_2_cm()
{
    static char rn[] = "Switch_ovw_2_cm";

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    /* bind the Cache Managers frame to the window */
    if (ovw_pageType & CMD_CM)
	gtxframe_SetFrame(afsmon_win, cmFrame);
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_ovw_next()
 *
 * Description:
 *		Switch to the next page in overview screen
 *----------------------------------------------------------------------*/
int
Switch_ovw_next()
{
    static char rn[] = "Switch_ovw_next";

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    if (ovw_pageType & CMD_NEXT) {
	/* call refresh with the next page number */
	ovw_refresh(ovw_currPage + 1, ovw_update_info);
    }

    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_ovw_last()
 *
 * Description:
 *		Switch to the last page in the overview screen
 *----------------------------------------------------------------------*/
int
Switch_ovw_last()
{
    static char rn[] = "Switch_ovw_last";

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    if (ovw_pageType & CMD_NEXT) {
	/* call refresh with the last page number */
	ovw_refresh(ovw_numPages, ovw_update_info);
    }

    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_ovw_prev()
 *
 * Description:
 *		Switch to the previous page in the overview screen
 *----------------------------------------------------------------------*/
int
Switch_ovw_prev()
{
    static char rn[] = "Switch_ovw_prev";

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    if (ovw_pageType & CMD_PREV) {
	/* call refresh with the previous page number */
	ovw_refresh(ovw_currPage - 1, ovw_update_info);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_ovw_first()
 *
 * Description:
 *		Switch to the first page in the overview screen
 *----------------------------------------------------------------------*/
int
Switch_ovw_first()
{
    static char rn[] = "Switch_ovw_first";

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    if (ovw_pageType & CMD_PREV) {
	/* refresh with the first page number */
	ovw_refresh(1, ovw_update_info);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * create_ovwFrame_objects()
 *
 * Description:
 *	Create the gtx objects (onodes) for the overview frame and setup
 *	the keyboard bindings.
 *	Only as many objects as can fit on the display are created. The
 *	positions and lengths of all these objects are fixed at creation.
 *	These objects are updated with new data at the end of each probe
 *	cycle.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
create_ovwFrame_objects()
{				/* create_ovwFrame_objects */

    static char rn[] = "create_ovwFrame_objects";
    int hostLines;		/* number of lines of host names to display */
    struct onode **ovw_fsNames_o_Ptr;	/* index to list of fs names onodes */
    struct onode **ovw_cmNames_o_Ptr;	/* index to list of cm names onodes */
    int code;
    int i;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    /* get frame dimensions, it must be atleast MINXxMINY for any sensible output */
    WOP_GETDIMENSIONS(ovwFrame->window, &frameDims);
    maxX = frameDims.maxx;
    maxY = frameDims.maxy;
    if (maxX + 1 < MINX || maxY + 1 < MINY) {
	sprintf(errMsg1, "[ %s ] Window size %dx%d; must be at least %dx%d\n",
		rn, maxX + 1, maxY + 1, MINX, MINY);
	return (-1);
    }
    if (afsmon_debug)
	fprintf(debugFD, "maxX = %d maxY = %d\n", maxX, maxY);


    /* Print an Intial message to the screen. The init message is 36 chars
     * long */
    initMsg_o =
	initLightObject(initMsg, maxX / 2 - 18, maxY / 3, sizeof(initMsg),
			afsmon_win);
    if (initMsg_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create initMsg_o onode\n", rn);
	afsmon_Exit(250);
    }
    code = gtxframe_AddToList(ovwFrame, initMsg_o);
    code = gator_light_set(initMsg_o, HIGHLIGHT);
    initMsg_on = 1;



    /* create the command line object */

    ovw_cmd_o = initLightObject("", 0, maxY - 1, OVW_CMD_O_WIDTH, afsmon_win);
    if (ovw_cmd_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create command onode\n", rn);
	afsmon_Exit(265);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_cmd_o);
    code = gator_light_set(ovw_cmd_o, HIGHLIGHT);

    /* create the program name object */

    ovw_progName_o = initLightObject("", 0, 0, PROGNAME_O_WIDTH, afsmon_win);
    if (ovw_progName_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create programName onode\n", rn);
	afsmon_Exit(255);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_progName_o);
    code = gator_light_set(ovw_progName_o, HIGHLIGHT);

    /* create the page number object */

    ovw_pageNum_o =
	initLightObject("", maxX - OVW_PAGENUM_O_WIDTH, 0,
			OVW_PAGENUM_O_WIDTH, afsmon_win);
    if (ovw_pageNum_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create pageNumber onode\n", rn);
	afsmon_Exit(260);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_pageNum_o);
    code = gator_light_set(ovw_pageNum_o, HIGHLIGHT);

    /* create the probe number object */
    ovw_probeNum_o =
	initLightObject("", maxX - OVW_PROBENUM_O_WIDTH, maxY - 1,
			OVW_PROBENUM_O_WIDTH, afsmon_win);
    if (ovw_probeNum_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create probe number onode\n", rn);
	afsmon_Exit(270);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_probeNum_o);
    code = gator_light_set(ovw_probeNum_o, HIGHLIGHT);

    /* create the numFS monitored object */
    ovw_numFS_o = initLightObject("", 0, 2, FC_NUMHOSTS_O_WIDTH, afsmon_win);
    if (ovw_numFS_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create numFS onode\n", rn);
	afsmon_Exit(275);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_numFS_o);

    /* create the numCM monitored object */
    ovw_numCM_o =
	initLightObject("", maxX / 2, 2, OVW_NUMCM_O_WIDTH, afsmon_win);
    if (ovw_numCM_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create numCM_o onode\n", rn);
	afsmon_Exit(280);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_numCM_o);

    /* create the number-of-FS-alerts object */
    ovw_FSalerts_o =
	initLightObject("", 0, 3, OVW_FSALERTS_O_WIDTH, afsmon_win);
    if (ovw_FSalerts_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create FSalerts_o onode\n", rn);
	afsmon_Exit(285);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_FSalerts_o);

    /* create the number-of-CM-alerts object */
    ovw_CMalerts_o =
	initLightObject("", maxX / 2, 3, OVW_CMALERTS_O_WIDTH, afsmon_win);
    if (ovw_CMalerts_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create CMalerts_o onode\n", rn);
	afsmon_Exit(290);
    }
    code = gtxframe_AddToList(ovwFrame, ovw_CMalerts_o);

    /* create file-server-name and cache-manager-names objects */
    ovw_numHosts_perPage = maxY - OVW_NUM_FIXED_LINES;

    /* allocate memory for a list of onode pointers for file server names */
    ovw_fsNames_o =
	(struct onode **)malloc(sizeof(struct onode *) *
				ovw_numHosts_perPage);
    if (ovw_fsNames_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to allocate memory for FS onodes\n",
		rn);
	afsmon_Exit(295);
    }

    /* create file server name objects */
    ovw_fsNames_o_Ptr = ovw_fsNames_o;
    for (i = 0; i < ovw_numHosts_perPage; i++) {
	*ovw_fsNames_o_Ptr =
	    initLightObject("", 0, OVW_FIRST_HOST_ROW + i,
			    OVW_HOSTNAME_O_WIDTH, afsmon_win);
	if (*ovw_fsNames_o_Ptr == NULL) {
	    sprintf(errMsg, "[ %s ] Failed to create an FS name onode\n", rn);
	    afsmon_Exit(300);
	}
	/*
	 * if (afsmon_debug) {
	 * fprintf(debugFD,"[ %s ] fsName_o  %d:  %d\n",
	 * rn,i,*ovw_fsNames_o_Ptr);
	 * fflush(debugFD);
	 * }
	 */
	code = gtxframe_AddToList(ovwFrame, *ovw_fsNames_o_Ptr);
	ovw_fsNames_o_Ptr++;

    }


    /* allocate memory for a list of onode pointers for cache manager names */
    ovw_cmNames_o =
	(struct onode **)malloc(sizeof(struct onode *) *
				ovw_numHosts_perPage);
    if (ovw_cmNames_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to allocate memory for CM onodes\n",
		rn);
	afsmon_Exit(305);
    }

    /* create cache manager name objects */
    ovw_cmNames_o_Ptr = ovw_cmNames_o;
    for (i = 0; i < ovw_numHosts_perPage; i++) {
	*ovw_cmNames_o_Ptr =
	    initLightObject("", maxX / 2, OVW_FIRST_HOST_ROW + i,
			    OVW_HOSTNAME_O_WIDTH, afsmon_win);
	if (*ovw_cmNames_o_Ptr == NULL) {
	    sprintf(errMsg, "[ %s ] Failed to create a CM name onode\n", rn);
	    afsmon_Exit(310);
	}
	code = gtxframe_AddToList(ovwFrame, *ovw_cmNames_o_Ptr);
	ovw_cmNames_o_Ptr++;
    }


    /* Calculate the number of pages of overview data to display */
    /* host information starts at the 6th line from top and stops at 3rd
     * line from bottom of screen */

    if (numFS > numCM)
	hostLines = numFS;
    else
	hostLines = numCM;

    ovw_numPages = hostLines / (maxY - OVW_NUM_FIXED_LINES);
    if (hostLines % (maxY - OVW_NUM_FIXED_LINES))
	ovw_numPages++;

    if (afsmon_debug)
	fprintf(debugFD, "[ %s ] number of ovw pages = %d\n", rn,
		ovw_numPages);

    /* When the ovw_refresh() routine is called by the keyboard handlers the
     * following variable is used to determine if fs/cm/fs&cm info must be
     * updated */
    ovw_update_info = 0;
    if (numFS)
	ovw_update_info |= OVW_UPDATE_FS;
    if (numCM)
	ovw_update_info |= OVW_UPDATE_CM;

    /* bind the overview frame to a keyboard input handler */

    /* bind Q and  to exit */
    keymap_BindToString(ovwFrame->keymap, "Q", afsmonExit_gtx, NULL, NULL);
    keymap_BindToString(ovwFrame->keymap, "", afsmonExit_gtx, NULL, NULL);

    /* f -> switch of fs frame */
    keymap_BindToString(ovwFrame->keymap, "f", Switch_ovw_2_fs, NULL, NULL);
    /* c -> switch of cm frame */
    keymap_BindToString(ovwFrame->keymap, "c", Switch_ovw_2_cm, NULL, NULL);
    /* n -> switch to next overview page */
    keymap_BindToString(ovwFrame->keymap, "n", Switch_ovw_next, NULL, NULL);
    /* N -> switch to last overview page */
    keymap_BindToString(ovwFrame->keymap, "N", Switch_ovw_last, NULL, NULL);
    /* p -> switch to previous overview page */
    keymap_BindToString(ovwFrame->keymap, "p", Switch_ovw_prev, NULL, NULL);
    /* P -> switch to first overview page */
    keymap_BindToString(ovwFrame->keymap, "P", Switch_ovw_first, NULL, NULL);


    return (0);
}				/* create_ovwFrame_objects */


/*-----------------------------------------------------------------------
 * resolve_CmdLine()
 *
 * Description:
 *	This function is called to determine the permissible keyboard
 *	operations on the FS and CM frames. This information is used
 *	to create an appropriate command line prompt. It also generates
 *	a bit map of the permissible operations on this page which is
 *	used by the keyboard-input handler routines.
 *
 * Returns:
 *	Success: page-type (bit map of permissible operations)
 *	Failure: -1
 *----------------------------------------------------------------------*/

int
resolve_CmdLine(a_buffer, a_currFrame, a_currPage, a_numPages, a_numCols,
		a_curr_LCol, a_cols_perPage, a_Data_Available)
     char *a_buffer;		/* buffer to copy command line */
     int a_currFrame;		/* current frame ovw, fs or cm? */
     int a_currPage;		/* current page number */
     int a_numPages;		/* number of pages of data */
     int a_numCols;		/* number of columns of data to display */
     int a_curr_LCol;		/* current number of leftmost column */
     int a_cols_perPage;	/* number of columns per page */

{				/* resolve_CmdLine */
    static char rn[] = "resolve_CmdLine";
    int pageType;

    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, a_buffer= %d, a_currFrame= %d, a_currPage= %d, a_numPages= %d, a_numCols= %d, a_curr_LCol= %d, a_cols_perPage= %d\n",
		rn, a_buffer, a_currFrame, a_currPage, a_numPages, a_numCols,
		a_curr_LCol, a_cols_perPage);
	fflush(debugFD);
    }

    pageType = 0;

    /* determine if we have fs/cm frames. If we do, note that we should not
     * let the user seen the initial junk we have there until the probe
     * results are available  */
    if (a_currFrame == 1) {	/* in the fs frame */
	if (numCM && cm_Data_Available)
	    pageType |= CMD_CM;
    } else if (a_currFrame == 2) {	/* in the cm frame */
	if (numFS && fs_Data_Available)
	    pageType |= CMD_FS;
    } else {
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Wrong frame type %d\n", rn, a_currFrame);
	    fflush(debugFD);
	}
	return (-1);
    }

    /* do we have next/previous pages */
    if (a_currPage < a_numPages)
	pageType |= CMD_NEXT;	/* have a next page */
    if (a_currPage > 1)
	pageType |= CMD_PREV;	/* have a previous page */

    if (a_numCols > a_cols_perPage) {
	if (a_curr_LCol > 0)
	    pageType |= CMD_LEFT;	/* have columns on left */
	if ((a_curr_LCol + a_cols_perPage) < a_numCols)
	    pageType |= CMD_RIGHT;	/* have columns on right */
    }

    /* now build the command line */

    strcpy(a_buffer, "Command [oview");
    if (pageType & CMD_FS)
	strcat(a_buffer, ", fs");
    if (pageType & CMD_CM)
	strcat(a_buffer, ", cm");
    if (pageType & CMD_PREV)
	strcat(a_buffer, ", prev");
    if (pageType & CMD_NEXT)
	strcat(a_buffer, ", next");
    if (pageType & CMD_LEFT)
	strcat(a_buffer, ", left");
    if (pageType & CMD_RIGHT)
	strcat(a_buffer, ", right");
    strcat(a_buffer, "]? ");

    return (pageType);

}				/* resolve_CmdLine */

/*-----------------------------------------------------------------------
 * display_Server_datum()
 *
 * Description:
 *	The data in the file server & cache manager frames are displayed
 *	in two objects, one below the other. If the data is too long to
 *	fit in the first object it will overflow into the next. This is
 *	to conserve real estate on the screen. This function copies the 
 *	contents of the source buffer adjusted to the two objects if the
 *	probe had succeded. Otherwise it enters "--" in the first object 
 *	blanks out the second. If the object needs to be highlightned
 *	(due to a threshold crossing) it is done.
 *
 * Returns:
 *	0 always
 *----------------------------------------------------------------------*/
int
display_Server_datum(a_srcBuf, a_firstObj_o, a_secondObj_o, a_probeOK, a_just,
		     a_highlight)

     char *a_srcBuf;		/* source buffer */
     struct onode *a_firstObj_o;	/* first object */
     struct onode *a_secondObj_o;	/* second object */
     int a_probeOK;		/* probe OK ? */
     int a_just;		/* justification */
     int a_highlight;		/* highlight object  ? */

{				/* display_Server_datum */

    static char rn[] = "display_Server_datum";
    struct gator_lightobj *tmp_lightobj1;
    struct gator_lightobj *tmp_lightobj2;
    char part1[FC_COLUMN_WIDTH + 2];
    char part2[FC_COLUMN_WIDTH + 2];
    int code;

    if (afsmon_debug) {
	if (a_highlight)
	    fprintf(debugFD,
		    "[ %s ] Called, a_srcBuf= %s, a_firstObj_o= %d, a_secondObj_o= %d, a_probeOK= %d, a_just= %d, a_highlight= %d\n",
		    rn, a_srcBuf, a_firstObj_o, a_secondObj_o, a_probeOK,
		    a_just, a_highlight);
	fflush(debugFD);
    }


    tmp_lightobj1 = (struct gator_lightobj *)a_firstObj_o->o_data;
    tmp_lightobj2 = (struct gator_lightobj *)a_secondObj_o->o_data;

    if (a_probeOK) {		/* probe is ok so fill in the data */

	/* check if it would fit in one object */
	if (strlen(a_srcBuf) < FC_COLUMN_WIDTH) {
	    strcpy(part1, a_srcBuf);
	    strcpy(part2, "");
	} else {
	    /* break up the src string into 2 parts */
	    /* note that column width includes terminator */
	    strncpy(part1, a_srcBuf, FC_COLUMN_WIDTH - 1);
	    part1[FC_COLUMN_WIDTH - 1] = '\0';
	    strncpy(part2, a_srcBuf + FC_COLUMN_WIDTH - 1,
		    FC_COLUMN_WIDTH - 1);

	}

    } else {			/* probe failed, enter "--"s */
	strcpy(part1, "--");
	strcpy(part2, "");
    }

    /* if (afsmon_debug) {
     * fprintf(debugFD,"[ %s ] %s split to %s & %s\n",rn,a_srcBuf,part1,part2);
     * fflush(debugFD);
     * } */

    /* initialize both the objects */

    code =
	justify_light(part1, tmp_lightobj1->label, FC_COLUMN_WIDTH, a_just,
		      1);
    if (code) {
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] justify_light failed 1 \n", rn);
	    fflush(debugFD);
	}
    }

    code =
	justify_light(part2, tmp_lightobj2->label, FC_COLUMN_WIDTH, a_just,
		      1);
    if (code) {
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] justify_light failed 1 \n", rn);
	    fflush(debugFD);
	}
    }

    /* highlight them */
    if (a_highlight && (part1[0] != '-'))
	gator_light_set(a_firstObj_o, 1);
    else
	gator_light_set(a_firstObj_o, 0);
    if (a_highlight && (part2[0] != '\0'))
	gator_light_set(a_secondObj_o, 1);
    else
	gator_light_set(a_secondObj_o, 0);



    return (0);

}				/* display_Server_datum */


/*-----------------------------------------------------------------------
 * display_Server_label()
 *
 * Description:
 *	Display the given server label in three objects. The label is
 *	partitioned into three parts by '/'s and each part is copied
 *	into each label object.
 *
 * Returns:
 *	0 always.
 *----------------------------------------------------------------------*/

int
display_Server_label(a_srcBuf, a_firstObj_o, a_secondObj_o, a_thirdObj_o)
     char *a_srcBuf;
     struct onode *a_firstObj_o;	/* first object */
     struct onode *a_secondObj_o;	/* second object */
     struct onode *a_thirdObj_o;	/* third object */

{				/* display_Server_label */

    static char rn[] = "display_Server_label";
    char part[3][20];		/* buffer for three parts of label */
    char *strPtr;
    struct gator_lightobj *tmp_lightobj;
    struct onode *objPtr_o[3];
    int code;
    int strLen;
    int len;
    int i;
    int j;

/* the following debug statement floods the debug file */
#ifdef DEBUG_DETAILED
    if (afsmon_debug) {
	fprintf(debugFD,
		"[ %s ] Called, a_srcBuf= %s, a_firstObj_o= %d, a_secondObj_o= %d, a_thirdObj_o= %d\n",
		rn, a_srcBuf, a_firstObj_o, a_secondObj_o, a_thirdObj_o);
	fflush(debugFD);
    }
#endif


    /* break the label string into three parts */

    part[0][0] = '\0';
    part[1][0] = '\0';
    part[2][0] = '\0';
    /* now for a dumb precaution */

    strLen = strlen(a_srcBuf);
    len = 0;
    strPtr = a_srcBuf;
    for (i = 0; i < 3; i++) {
	j = 0;
	while (*strPtr != '\0' && (len++ <= strLen)) {
	    if (*strPtr == '/') {
		strPtr++;
		break;
	    } else
		part[i][j] = *strPtr;
	    strPtr++;
	    j++;
	}
	part[i][j] = '\0';
    }

    /* 
     * if (afsmon_debug) {
     * fprintf(debugFD,"[ %s ] LABELS %s -> %s %s %s\n",
     * rn, a_srcBuf, part[0], part[1], part[2]);
     * fflush(debugFD);
     * }
     */

    objPtr_o[0] = a_firstObj_o;
    objPtr_o[1] = a_secondObj_o;
    objPtr_o[2] = a_thirdObj_o;

    /* display each label justified CENTER */

    for (i = 0; i < 3; i++) {
	tmp_lightobj = (struct gator_lightobj *)objPtr_o[i]->o_data;
	code =
	    justify_light(part[i], tmp_lightobj->label, FC_COLUMN_WIDTH,
			  CENTER, 1);
	if (code) {
	    if (afsmon_debug) {
		fprintf(debugFD, "[ %s ] justify_light %d failed \n", rn, i);
		fflush(debugFD);
	    }
	}
    }
    return 0;
}				/* display_Server_label */





/*-----------------------------------------------------------------------
 * fs_refresh()
 *
 * Description:
 *	Refresh the File Servers screen with the given page number starting
 *	at the given left-column number. The appropriate contents of 
 *	prev_fsData are displayed. 
 *	First the status labels at the four corners of the screen are
 *	updated. Next the column labels are updated and then each row
 *	of statistics.
 *	
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmoitor on a severe error.
 *----------------------------------------------------------------------*/


int
fs_refresh(a_pageNum, a_LcolNum)
     int a_pageNum;		/* page to display */
     int a_LcolNum;		/* starting (leftmost) column number */

{				/* fs_refresh */

    static char rn[] = "fs_refresh";	/* routine name */
    struct gator_lightobj *tmp_lightobj;	/* ptr for object's display data */
    struct fs_Display_Data *fsDataP;	/* ptr to FS display data array */
    struct ServerInfo_line *tmp_fs_lines_P;	/* tmp ptr to fs_lines */
    struct onode **firstSlot_o_Ptr;	/* ptr to first data slot of a datum */
    struct onode **secondSlot_o_Ptr;	/* ptr to second data slot of a datum */
    struct onode **fsLabels_o_Ptr1;	/* ptr to label row 0 */
    struct onode **fsLabels_o_Ptr2;	/* ptr to label row 1 */
    struct onode **fsLabels_o_Ptr3;	/* ptr to label row 2 */
    char cmdLine[80];		/* buffer for command line */
    char printBuf[256];		/* buffer to print to screen */
    int i;
    int j;
    int k;
    int code;
    int fsIdx;
    int labelIdx;
    int dataIndex;		/* index to the data[] field of 
				 * struct fs_Display_Data */

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called with row %d col %d \n", rn, a_pageNum,
		a_LcolNum);
	fflush(debugFD);
    }


    /* if the data is not yet available, ie., not one probe cycle has 
     * completed, do nothing */

    if (!fs_Data_Available)
	return (0);


    /* validate the page number & column number */
    if (a_pageNum < 1 || a_pageNum > fs_numPages) {
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Called with wrong page # %d \n", rn,
		    a_pageNum);
	    fflush(debugFD);
	}
	afsmon_Exit(315);
    }
    if (a_LcolNum < 0 || a_LcolNum > fs_numCols) {
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Called with wrong column #%d\n", rn,
		    a_LcolNum);
	    fflush(debugFD);
	}
	afsmon_Exit(320);
    }



    /* update the fixed labels */

    /* we reuse the ovw version lable and hence do not have to do anything
     * for it here */

    /* page number label */
    tmp_lightobj = (struct gator_lightobj *)fs_pageNum_o->o_data;
    sprintf(printBuf, "[File Servers, p. %d of %d, c. %d of %d]", a_pageNum,
	    fs_numPages, a_LcolNum + 1, fs_numCols);
    justify_light(printBuf, tmp_lightobj->label, FC_PAGENUM_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(fs_pageNum_o, 1);

    /* file servers monitored label */
    tmp_lightobj = (struct gator_lightobj *)fs_numFS_o->o_data;
    sprintf(printBuf, "%d File Servers monitored, %d alerts on %d machines",
	    numFS, num_fs_alerts, numHosts_onfs_alerts);
    justify_light(printBuf, tmp_lightobj->label, FC_NUMHOSTS_O_WIDTH, CENTER,
		  1);


    /* command line */

    /* figure out what we need to show in the prompt & set the page type */
    /* the fs_pageType variable is in turn used by the keyboard handler 
     * routines to call fs_refresh() with the correct parameters */

    fs_pageType = resolve_CmdLine(cmdLine, 1 /* fs frame */ , a_pageNum,
				  fs_numPages, fs_numCols, a_LcolNum,
				  fs_cols_perPage);

    /* display the command line */
    tmp_lightobj = (struct gator_lightobj *)fs_cmd_o->o_data;
    sprintf(printBuf, "%s", cmdLine);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);
    gator_light_set(fs_cmd_o, 1);

    /* update the probe number label */
    tmp_lightobj = (struct gator_lightobj *)fs_probeNum_o->o_data;
    sprintf(printBuf, "[FS probes %d, freq=%d sec]", afsmon_fs_prev_probeNum,
	    afsmon_probefreq);
    justify_light(printBuf, tmp_lightobj->label, FC_PROBENUM_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(fs_probeNum_o, 1);

    /* update "columns on left" signal */
    tmp_lightobj = (struct gator_lightobj *)fs_leftArrows_o->o_data;
    if (fs_pageType & CMD_LEFT)
	strcpy(printBuf, "<<<");
    else
	strcpy(printBuf, "");
    justify_light(printBuf, tmp_lightobj->label, FC_ARROWS_O_WIDTH,
		  LEFT_JUSTIFY, 1);
    gator_light_set(fs_leftArrows_o, 0);

    /* update "columns on right" signal */
    tmp_lightobj = (struct gator_lightobj *)fs_rightArrows_o->o_data;
    if (fs_pageType & CMD_RIGHT)
	strcpy(printBuf, ">>>");
    else
	strcpy(printBuf, "");
    justify_light(printBuf, tmp_lightobj->label, FC_ARROWS_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(fs_rightArrows_o, 0);



    /* UPDATE THE COLUMN LABELS */

    /* the column index is also used to index the label arrays */
    labelIdx = a_LcolNum;

    /* get the pointers to the three arrays of label onodes */
    fsLabels_o_Ptr1 = fsLabels_o[0];
    fsLabels_o_Ptr2 = fsLabels_o[1];
    fsLabels_o_Ptr3 = fsLabels_o[2];

    for (k = 0; k < fs_cols_perPage; k++) {

	if (labelIdx < fs_numCols) {
	    dataIndex = fs_Display_map[labelIdx];
	    code =
		display_Server_label(fs_labels[dataIndex], *fsLabels_o_Ptr1,
				     *fsLabels_o_Ptr2, *fsLabels_o_Ptr3);

	    labelIdx++;		/* next label */
	} else {
	    code =
		display_Server_label("//", *fsLabels_o_Ptr1, *fsLabels_o_Ptr2,
				     *fsLabels_o_Ptr3);
	}

	fsLabels_o_Ptr1++;	/* next onode in label row 1 */
	fsLabels_o_Ptr2++;	/* next onode in label row 2 */
	fsLabels_o_Ptr3++;	/* next onode in label row 3 */

    }				/* labels for each column */


    /* UPDATE THE FILE SERVER STATISTICS */

    /* move to the right spot in the FS display data array */
    fsDataP = prev_fsData;
    fsIdx = 0;
    for (i = 0; i < ((a_pageNum - 1) * fs_numHosts_perPage); i++) {
	fsDataP++;
	fsIdx++;
    }

    if (fsIdx >= numFS) {	/* whoops! screwed up */
	sprintf(errMsg, "[ %s ] Programming error 1\n", rn);
	afsmon_Exit(325);
    }

    /* get the pointer to the first line of onodes of the file server frame */
    tmp_fs_lines_P = fs_lines;

    for (i = 0; i < fs_numHosts_perPage; i++) {


	/* if this is the last page we may not have file servers to fill up
	 * the page, so check the index */
	if (fsIdx < numFS) {

	    if (fsDataP->hostName[0] == '\0') {
		sprintf(errMsg, "[ %s ] empty fs display entry \n", rn);
		afsmon_Exit(330);
	    }

	    /* display the hostname , first names only please! */

	    sprintf(printBuf, fsDataP->hostName);
	    for (j = 0; j < strlen(printBuf); j++) {
		if (printBuf[j] == '.') {
		    printBuf[j] = '\0';
		    break;
		}
	    }

	    tmp_lightobj =
		(struct gator_lightobj *)tmp_fs_lines_P->host_o->o_data;
	    code =
		justify_light(printBuf, tmp_lightobj->label,
			      FC_HOSTNAME_O_WIDTH, LEFT_JUSTIFY, 1);
	    if (code) {
		fprintf(debugFD, "[ %s ] justify_code returned %d\n", rn,
			code);
		fflush(debugFD);
	    }

	    /* use the current column value to index into the fs_Display_map
	     * array to obtain the index of the item to display. check if its
	     * overflow flag is set and highlight if so. if the probe had failed
	     * enter "--" is all columns */

	    /* each host has two rows of slots for datums. get the pointers to 
	     * both the arrays */

	    firstSlot_o_Ptr = tmp_fs_lines_P->data_o[0];
	    secondSlot_o_Ptr = tmp_fs_lines_P->data_o[1];
	    fs_curr_RCol = a_LcolNum;	/* starting column number from which
					 * we are asked to display data */

	    for (j = 0; j < fs_cols_perPage; j++) {	/* for each column */

		/* if there is another column of data */
		if (fs_curr_RCol < fs_numCols) {

		    dataIndex = fs_Display_map[fs_curr_RCol];

		    code =
			display_Server_datum(fsDataP->data[dataIndex],
					     *firstSlot_o_Ptr,
					     *secondSlot_o_Ptr,
					     fsDataP->probeOK, RIGHT_JUSTIFY,
					     fsDataP->threshOvf[dataIndex]);

		    fs_curr_RCol++;
		} else {	/* no more data, blank out columns */
		    code = display_Server_datum("", *firstSlot_o_Ptr, *secondSlot_o_Ptr, 1,	/* probe ok */
						RIGHT_JUSTIFY, 0);	/* no overflow */
		}


		firstSlot_o_Ptr++;	/* onode of next column */
		secondSlot_o_Ptr++;	/* onode of next column */

	    }			/* for each column */

	    /* the loop could have taken the right-column-index one over,
	     * adjust it now */
	    if (fs_curr_RCol == fs_numCols)
		fs_curr_RCol--;


	}



	/* if fdIdx < numFS */
	/* if fsIdx >= numFS , blank out all succeding rows */
	if (fsIdx >= numFS) {

	    /* blank out host name object */
	    tmp_lightobj =
		(struct gator_lightobj *)tmp_fs_lines_P->host_o->o_data;
	    code =
		justify_light("", tmp_lightobj->label, FC_HOSTNAME_O_WIDTH,
			      LEFT_JUSTIFY, 1);
	    if (code) {
		fprintf(debugFD, "[ %s ] justify_code returned %d\n", rn,
			code);
		fflush(debugFD);
	    }

	    firstSlot_o_Ptr = tmp_fs_lines_P->data_o[0];
	    secondSlot_o_Ptr = tmp_fs_lines_P->data_o[1];

	    for (k = 0; k < fs_cols_perPage; k++) {
		code = display_Server_datum("", *firstSlot_o_Ptr, *secondSlot_o_Ptr, 1,	/* probe OK */
					    RIGHT_JUSTIFY, 0);	/* dont highlight */

		firstSlot_o_Ptr++;
		secondSlot_o_Ptr++;
	    }

	}

	/* fsIDx >= numFS */
	tmp_fs_lines_P++;	/* pointer to next line in the frame */
	fsDataP++;		/* next host's data */
	fsIdx++;		/* host index */


    }				/* for each row in the File Servers frame */

    /* redraw the display if the File Servers screen is currently displayed */
    if (afsmon_win->w_frame == fsFrame)
	WOP_DISPLAY(afsmon_win);

    /* update the global page & column numbers to reflect the changes */
    fs_currPage = a_pageNum;
    fs_curr_LCol = a_LcolNum;;

    return (0);

}				/* fs_refresh */




/*-----------------------------------------------------------------------
 * Switch_fs_2_ovw()
 *
 * Description:
 *	Switch from the File Server screen to the Overview Screen
 *----------------------------------------------------------------------*/
int
Switch_fs_2_ovw()
{
    /* bind the overview frame to the window */
    gtxframe_SetFrame(afsmon_win, ovwFrame);
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_2_cm()
 *
 * Description:
 *	Switch from the File Server screen to the Cache Managers screen. 
 *----------------------------------------------------------------------*/
int
Switch_fs_2_cm()
{
    if (fs_pageType & CMD_CM) {
	/* bind the overview Cache Managers to the window */
	gtxframe_SetFrame(afsmon_win, cmFrame);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_next()
 *
 * Description:
 *	Switch to next page of file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_next()
{
    static char rn[] = "Switch_fs_next";

    if (fs_pageType & CMD_NEXT) {
	/* we have a next page, refresh with next page number */
	fs_refresh(fs_currPage + 1, fs_curr_LCol);
    }

    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_last()
 *
 * Description:
 *	Switch to last page of file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_last()
{
    static char rn[] = "Switch_fs_last";


    if (fs_pageType & CMD_NEXT) {
	/* we have a next page, refresh with the last page number */
	fs_refresh(fs_numPages, fs_curr_LCol);
    }

    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_prev()
 *
 * Description:
 *	Switch to previous page of file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_prev()
{
    static char rn[] = "Switch_fs_prev";

    if (fs_pageType & CMD_PREV) {
	/* we have a previous page, refresh with the rpevious page number */
	fs_refresh(fs_currPage - 1, fs_curr_LCol);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_first()
 *
 * Description:
 *	Switch to first page of file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_first()
{
    static char rn[] = "Switch_fs_first";

    if (fs_pageType & CMD_PREV) {
	/* we have a previous page, got to first page */
	fs_refresh(1, fs_curr_LCol);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_left()
 *
 * Description:
 *	Scroll left on the file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_left()
{
    static char rn[] = "Switch_fs_left";

    if (fs_pageType & CMD_LEFT) {
	/* we have columns on left, refresh with new column number */
	fs_refresh(fs_currPage, fs_curr_LCol - fs_cols_perPage);
    }
    return (0);
}


/*-----------------------------------------------------------------------
 * Switch_fs_leftmost()
 *
 * Description:
 *	Scroll to first column on  the file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_leftmost()
{
    static char rn[] = "Switch_fs_leftmost";

    if (fs_pageType & CMD_LEFT) {
	/* we have columns on left, go to the first */
	fs_refresh(fs_currPage, 0);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_right()
 *
 * Description:
 *	Scroll right on the file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_right()
{
    static char rn[] = "Switch_fs_right";

    if (fs_pageType & CMD_RIGHT) {
	/* we have columns on right, refresh with new column number */
	fs_refresh(fs_currPage, fs_curr_LCol + fs_cols_perPage);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_fs_rightmost()
 *
 * Description:
 *	Scroll to last column on the file server screen 
 *----------------------------------------------------------------------*/
int
Switch_fs_rightmost()
{
    static char rn[] = "Switch_fs_rightmost";
    int curr_LCol;


    if (fs_pageType & CMD_RIGHT) {
	/* we have columns on right, go to the last column */
	if (fs_numCols % fs_cols_perPage)
	    curr_LCol = (fs_numCols / fs_cols_perPage) * fs_cols_perPage;
	else
	    curr_LCol =
		((fs_numCols / fs_cols_perPage) - 1) * fs_cols_perPage;

	fs_refresh(fs_currPage, curr_LCol);
    }
    return (0);
}


/*-----------------------------------------------------------------------
 * create_FSframe_objects()
 *
 * Description:
 *	Create the gtx objects (onodes) for the Fileservers frame and setup
 *	the keyboard bindings.
 *	Only as many objects as can fit on the display are created. The
 *	positions and lengths of all these objects are fixed at creation.
 *	These objects are updated with new data at the end of each probe
 *	cycle.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
create_FSframe_objects()
{				/* create_FSframe_objects */
    static char rn[] = "create_FSframe_objects";
    struct ServerInfo_line *fs_lines_Ptr;
    struct onode **fs_data_o_Ptr;
    struct onode **fsLabels_o_Ptr;
    int x_pos;
    int y_pos;
    int code;
    int i;
    int j;
    int numBytes;
    int arrIdx;


    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    /* create the command line object */
    fs_cmd_o =
	initLightObject("Command [oview, cm, prev, next, left, right] ? ", 0,
			maxY - 1, FC_CMD_O_WIDTH, afsmon_win);
    if (fs_cmd_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create fs command onode\n", rn);
	afsmon_Exit(340);
    }
    code = gtxframe_AddToList(fsFrame, fs_cmd_o);
    code = gator_light_set(fs_cmd_o, HIGHLIGHT);

    /* we already have the dimensions for the frame - same as the ovw frame */
    /* use the ovw program name object for the fs screen too */

    code = gtxframe_AddToList(fsFrame, ovw_progName_o);


    /* create the page number object */
    fs_pageNum_o =
	initLightObject("[File Servers, p. X of X, c. Y of Y]",
			maxX - FC_PAGENUM_O_WIDTH, 0, FC_PAGENUM_O_WIDTH,
			afsmon_win);
    if (fs_pageNum_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create pageNumber onode\n", rn);
	afsmon_Exit(335);
    }
    code = gtxframe_AddToList(fsFrame, fs_pageNum_o);
    code = gator_light_set(fs_pageNum_o, HIGHLIGHT);

    /* create the probe number object */
    fs_probeNum_o =
	initLightObject("[FS probes 1, freq=30 sec]",
			maxX - FC_PROBENUM_O_WIDTH, maxY - 1,
			FC_PROBENUM_O_WIDTH, afsmon_win);
    if (fs_probeNum_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create fs probeNum onode\n", rn);
	afsmon_Exit(345);
    }
    code = gtxframe_AddToList(fsFrame, fs_probeNum_o);
    code = gator_light_set(fs_probeNum_o, HIGHLIGHT);


    /* create the numFS monitored object */
    fs_numFS_o =
	initLightObject
	("  	0 File Servers monitored, 0 alerts on 0 machines", 4, 2,
	 FC_NUMHOSTS_O_WIDTH, afsmon_win);
    if (fs_numFS_o == NULL) {
	sprintf(errMsg,
		"[ %s ] Failed to create numFS onode for the fs frame\n", rn);
	afsmon_Exit(350);
    }
    code = gtxframe_AddToList(fsFrame, fs_numFS_o);

    /* create the "more columns to left" indicator */
    fs_leftArrows_o =
	initLightObject("<<<", 0, 2, FC_ARROWS_O_WIDTH, afsmon_win);
    if (fs_leftArrows_o == NULL) {
	sprintf(errMsg,
		"[ %s ] Failed to create leftArrows onode for the fs frame\n",
		rn);
	afsmon_Exit(355);
    }
    code = gtxframe_AddToList(fsFrame, fs_leftArrows_o);

    /* create the "more columns to right" indicator */
    fs_rightArrows_o =
	initLightObject(">>>", maxX - FC_ARROWS_O_WIDTH, 2, FC_ARROWS_O_WIDTH,
			afsmon_win);
    if (fs_rightArrows_o == NULL) {
	sprintf(errMsg,
		"[ %s ] Failed to create rightArrows onode for the fs frame\n",
		rn);
	afsmon_Exit(360);
    }
    code = gtxframe_AddToList(fsFrame, fs_rightArrows_o);




    /* calculate the maximum number of hosts per page (2 rows per host) */
    fs_numHosts_perPage = (maxY - FC_NUM_FIXED_LINES) / 2;

    /* determine the number of data columns that can fit in a page */
    fs_cols_perPage = (maxX - FC_HOSTNAME_O_WIDTH) / (FC_COLUMN_WIDTH);

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] fs_numHosts_perPage=%d fs_cols_perPage=%d\n",
		rn, fs_numHosts_perPage, fs_cols_perPage);
	fflush(debugFD);
    }

    /* the above two variables give us the information needed to create
     * the objects for displaying the file server information */

    /* allocate memory for all the onode pointers required to display
     * the file server statistics */

    numBytes = fs_numHosts_perPage * sizeof(struct ServerInfo_line);
    fs_lines = (struct ServerInfo_line *)malloc(numBytes);
    if (fs_lines == (struct ServerInfo_line *)0) {
	sprintf(errMsg,
		"[ %s ] Failed to allocate %d bytes for FS data lines\n", rn,
		numBytes);
	afsmon_Exit(365);
    }

    /* for each line of server statistics allocate memory to store two arrays 
     * of data onodes */

    fs_lines_Ptr = fs_lines;
    for (i = 0; i < fs_numHosts_perPage; i++) {
	for (arrIdx = 0; arrIdx < 2; arrIdx++) {
	    numBytes = fs_cols_perPage * sizeof(struct onode *);
	    fs_lines_Ptr->data_o[arrIdx] = (struct onode **)malloc(numBytes);
	    if (fs_lines_Ptr->data_o[arrIdx] == NULL) {
		sprintf(errMsg,
			"[ %s ] Failed to allocate %d bytes for FS data onodes\n",
			rn, numBytes);
		afsmon_Exit(370);
	    }
	}
	fs_lines_Ptr++;
    }

    /* now allocate the onodes itself */

    fs_lines_Ptr = fs_lines;
    for (i = 0; i < fs_numHosts_perPage; i++) {

	/* initialize host name onode */
	fs_lines_Ptr->host_o =
	    initLightObject("FSHostName", 0, FC_FIRST_HOST_ROW + 2 * i,
			    FC_HOSTNAME_O_WIDTH, afsmon_win);
	if (fs_lines_Ptr->host_o == NULL) {
	    sprintf(errMsg, "[ %s ] Failed to create an FS name onode\n", rn);
	    afsmon_Exit(375);
	}
	code = gtxframe_AddToList(fsFrame, fs_lines_Ptr->host_o);

	/* if (afsmon_debug) {
	 * fprintf(debugFD,"[ %s ] Addr of host_o = %d for line %d\n",
	 * rn,fs_lines_Ptr->host_o,i);
	 * fflush(debugFD);
	 * } */

	/* initialize data onodes for this host */

	for (arrIdx = 0; arrIdx < 2; arrIdx++) {	/* for each array index */

	    fs_data_o_Ptr = fs_lines_Ptr->data_o[arrIdx];
	    for (j = 0; j < fs_cols_perPage; j++) {	/* for each column */

		char tmpBuf[20];

		/* determine x & y coordinate for this data object */
		/* the 1's are for leaving a blank after each column */
		x_pos = FC_HOSTNAME_O_WIDTH + (j * (FC_COLUMN_WIDTH));
		y_pos = FC_FIRST_HOST_ROW + 2 * i + arrIdx;

		sprintf(tmpBuf, "-FSData %d-", arrIdx);
		*fs_data_o_Ptr =
		    initLightObject(tmpBuf, x_pos, y_pos, FC_COLUMN_WIDTH,
				    afsmon_win);
		if (*fs_data_o_Ptr == NULL) {
		    sprintf(errMsg,
			    "[ %s ] Failed to create an FS data onode\n", rn);
		    afsmon_Exit(380);
		}
		code = gtxframe_AddToList(fsFrame, *fs_data_o_Ptr);

		fs_data_o_Ptr++;
	    }			/* for each column */
	}			/* for each onode array index */

	fs_lines_Ptr++;
    }				/* for each host slot */


    /* INITIALIZE COLUMN LABELS */


    /* allocate memory for two arrays of onode pointers for file server column
     * labels */
    for (arrIdx = 0; arrIdx < 3; arrIdx++) {

	fsLabels_o[arrIdx] =
	    (struct onode **)malloc(sizeof(struct onode *) * fs_cols_perPage);
	if (fsLabels_o[arrIdx] == NULL) {
	    sprintf(errMsg,
		    "[ %s ] Failed to allocate memory for FS label onodes\n",
		    rn);
	    afsmon_Exit(385);
	}

	/* create cache manager name objects */
	fsLabels_o_Ptr = fsLabels_o[arrIdx];
	for (i = 0; i < fs_cols_perPage; i++) {
	    *fsLabels_o_Ptr =
		initLightObject("", FC_HOSTNAME_O_WIDTH + i * FC_COLUMN_WIDTH,
				FC_FIRST_LABEL_ROW + arrIdx, FC_COLUMN_WIDTH,
				afsmon_win);

	    if (*fsLabels_o_Ptr == NULL) {
		sprintf(errMsg, "[ %s ] Failed to create a FS label onode\n",
			rn);
		afsmon_Exit(390);
	    }
	    code = gtxframe_AddToList(fsFrame, *fsLabels_o_Ptr);
	    fsLabels_o_Ptr++;
	}

    }


    /* initialize the column & page counters */

    fs_currPage = 1;
    fs_numCols = fs_DisplayItems_count;
    fs_numPages = numFS / fs_numHosts_perPage;
    if (numFS % fs_numHosts_perPage)
	fs_numPages++;
    fs_curr_LCol = 0;		/* leftmost col */
    fs_curr_RCol = 0;		/* rightmost col */

    /* create keyboard bindings */
    /* bind Q and  to exit */
    keymap_BindToString(fsFrame->keymap, "Q", afsmonExit_gtx, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "", afsmonExit_gtx, NULL, NULL);

    /* o = overview, c = cm, n = next, p = prev, l = left, r = right 
     * N = last page, P = first page, L = leftmost col, R = rightmost col */

    keymap_BindToString(fsFrame->keymap, "o", Switch_fs_2_ovw, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "c", Switch_fs_2_cm, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "n", Switch_fs_next, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "N", Switch_fs_last, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "p", Switch_fs_prev, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "P", Switch_fs_first, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "l", Switch_fs_left, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "L", Switch_fs_leftmost, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "r", Switch_fs_right, NULL, NULL);
    keymap_BindToString(fsFrame->keymap, "R", Switch_fs_rightmost, NULL,
			NULL);

    return (0);
}				/* create_FSframe_objects */


/*-----------------------------------------------------------------------
 * Function:	cm_refresh()
 *
 * Description:
 *	Refresh the Cache Managers screen with the given page number starting
 *	at the given left-column number. The appropriate contents of 
 *	prev_cmData are displayed. 
 *	First the status labels at the four corners of the screen are
 *	updated. Next the column labels are updated and then each row
 *	of statistics.
 *	
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmoitor on a severe error.
 *----------------------------------------------------------------------*/

int
cm_refresh(a_pageNum, a_LcolNum)
     int a_pageNum;		/* page to display */
     int a_LcolNum;		/* starting (leftmost) column number */

{				/* cm_refresh */

    static char rn[] = "cm_refresh";	/* routine name */
    struct gator_lightobj *tmp_lightobj;	/* ptr for object's display data */
    struct cm_Display_Data *cmDataP;	/* ptr to CM display data array */
    struct ServerInfo_line *tmp_cm_lines_P;	/* tmp ptr to cm_lines */
    struct onode **firstSlot_o_Ptr;	/* ptr to first data slot of a datum */
    struct onode **secondSlot_o_Ptr;	/* ptr to second data slot of a datum */
    struct onode **cmLabels_o_Ptr1;	/* ptr to label row 0 */
    struct onode **cmLabels_o_Ptr2;	/* ptr to label row 1 */
    struct onode **cmLabels_o_Ptr3;	/* ptr to label row 2 */
    char cmdLine[80];		/* buffer for command line */
    char printBuf[256];		/* buffer to print to screen */
    int i;
    int j;
    int k;
    int code;
    int cmIdx;
    int labelIdx;
    int dataIndex;		/* index to the data[] field of 
				 * struct cm_Display_Data */

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_pageNum= %d, a_LcolNum= %d \n", rn,
		a_pageNum, a_LcolNum);
	fflush(debugFD);
    }


    /* if the data is not yet available, ie., not one probe cycle has 
     * completed, do nothing */

    if (!cm_Data_Available)
	return (0);


    /* validate the page number & column number */
    if (a_pageNum < 1 || a_pageNum > cm_numPages) {
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Called with wrong page # %d \n", rn,
		    a_pageNum);
	    fflush(debugFD);
	}
	afsmon_Exit(395);
    }
    if (a_LcolNum < 0 || a_LcolNum > cm_numCols) {
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] Called with wrong column #%d\n", rn,
		    a_LcolNum);
	    fflush(debugFD);
	}
	afsmon_Exit(400);
    }



    /* update the fixed labels */

    /* we reuse the ovw version lable and hence do not have to do anything
     * for it here */

    /* page number label */
    tmp_lightobj = (struct gator_lightobj *)cm_pageNum_o->o_data;
    sprintf(printBuf, "[Cache Managers, p.%d of %d, c.%d of %d]", a_pageNum,
	    cm_numPages, a_LcolNum + 1, cm_numCols);
    justify_light(printBuf, tmp_lightobj->label, FC_PAGENUM_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(cm_pageNum_o, 1);

    /* file servers monitored label */
    tmp_lightobj = (struct gator_lightobj *)cm_numCM_o->o_data;
    sprintf(printBuf, "%d Cache Managers monitored, %d alerts on %d machines",
	    numCM, num_cm_alerts, numHosts_oncm_alerts);
    justify_light(printBuf, tmp_lightobj->label, FC_NUMHOSTS_O_WIDTH, CENTER,
		  1);


    /* command line */

    /* figure out what we need to show in the prompt & set the page type */
    /* the cm_pageType variable is in turn used by the keyboard handler 
     * routines to call cm_refresh() with the correct parameters */

    cm_pageType = resolve_CmdLine(cmdLine, 2 /* cm frame */ , a_pageNum,
				  cm_numPages, cm_numCols, a_LcolNum,
				  cm_cols_perPage);

    /* display the command line */
    tmp_lightobj = (struct gator_lightobj *)cm_cmd_o->o_data;
    sprintf(printBuf, "%s", cmdLine);
    justify_light(printBuf, tmp_lightobj->label, strlen(printBuf) + 1,
		  LEFT_JUSTIFY, 1);
    gator_light_set(cm_cmd_o, 1);

    /* update the probe number label */
    tmp_lightobj = (struct gator_lightobj *)cm_probeNum_o->o_data;
    sprintf(printBuf, "[CM probes %d, freq=%d sec]", afsmon_cm_prev_probeNum,
	    afsmon_probefreq);
    justify_light(printBuf, tmp_lightobj->label, FC_PROBENUM_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(cm_cmd_o, 1);

    /* update "columns on left" signal */
    tmp_lightobj = (struct gator_lightobj *)cm_leftArrows_o->o_data;
    if (cm_pageType & CMD_LEFT)
	strcpy(printBuf, "<<<");
    else
	strcpy(printBuf, "");
    justify_light(printBuf, tmp_lightobj->label, FC_ARROWS_O_WIDTH,
		  LEFT_JUSTIFY, 1);
    gator_light_set(cm_leftArrows_o, 0);

    /* update "columns on right" signal */
    tmp_lightobj = (struct gator_lightobj *)cm_rightArrows_o->o_data;
    if (cm_pageType & CMD_RIGHT)
	strcpy(printBuf, ">>>");
    else
	strcpy(printBuf, "");
    justify_light(printBuf, tmp_lightobj->label, FC_ARROWS_O_WIDTH,
		  RIGHT_JUSTIFY, 1);
    gator_light_set(cm_rightArrows_o, 0);



    /* UPDATE THE COLUMN LABELS */

    /* the column index is also used to index the label arrays */
    labelIdx = a_LcolNum;

    /* get the pointers to the three arrays of label onodes */
    cmLabels_o_Ptr1 = cmLabels_o[0];
    cmLabels_o_Ptr2 = cmLabels_o[1];
    cmLabels_o_Ptr3 = cmLabels_o[2];

    for (k = 0; k < cm_cols_perPage; k++) {

	if (labelIdx < cm_numCols) {
	    dataIndex = cm_Display_map[labelIdx];
	    code =
		display_Server_label(cm_labels[dataIndex], *cmLabels_o_Ptr1,
				     *cmLabels_o_Ptr2, *cmLabels_o_Ptr3);

	    labelIdx++;		/* next label */
	} else {
	    code =
		display_Server_label("//", *cmLabels_o_Ptr1, *cmLabels_o_Ptr2,
				     *cmLabels_o_Ptr3);
	}

	cmLabels_o_Ptr1++;	/* next onode in label row 1 */
	cmLabels_o_Ptr2++;	/* next onode in label row 2 */
	cmLabels_o_Ptr3++;	/* next onode in label row 3 */

    }				/* labels for each column */


    /* UPDATE THE FILE SERVER STATISTICS */

    /* move to the right spot in the CM display data array */
    cmDataP = prev_cmData;
    cmIdx = 0;
    for (i = 0; i < ((a_pageNum - 1) * cm_numHosts_perPage); i++) {
	cmDataP++;
	cmIdx++;
    }

    if (cmIdx >= numCM) {	/* whoops! screwed up */
	sprintf(errMsg, "[ %s ] Programming error 1\n", rn);
	afsmon_Exit(405);
    }

    /* get the pointer to the first line of onodes of the file server frame */
    tmp_cm_lines_P = cm_lines;

    for (i = 0; i < cm_numHosts_perPage; i++) {


	/* if this is the last page we may not have file servers to fill up
	 * the page, so check the index */
	if (cmIdx < numCM) {

	    if (cmDataP->hostName[0] == '\0') {
		sprintf(errMsg, "[ %s ] empty cm display entry \n", rn);
		afsmon_Exit(410);
	    }

	    /* display the hostname , first names only please! */

	    sprintf(printBuf, cmDataP->hostName);
	    for (j = 0; j < strlen(printBuf); j++) {
		if (printBuf[j] == '.') {
		    printBuf[j] = '\0';
		    break;
		}
	    }

	    tmp_lightobj =
		(struct gator_lightobj *)tmp_cm_lines_P->host_o->o_data;
	    code =
		justify_light(printBuf, tmp_lightobj->label,
			      FC_HOSTNAME_O_WIDTH, LEFT_JUSTIFY, 1);
	    if (code) {
		fprintf(debugFD, "[ %s ] justify_code returned %d\n", rn,
			code);
		fflush(debugFD);
	    }

	    /* use the current column value to index into the cm_Display_map
	     * array to obtain the index of the item to display. check if its
	     * overflow flag is set and highlight if so. if the probe had failed
	     * enter "--" is all columns */

	    /* each host has two rows of slots for datums. get the pointers to 
	     * both the arrays */

	    firstSlot_o_Ptr = tmp_cm_lines_P->data_o[0];
	    secondSlot_o_Ptr = tmp_cm_lines_P->data_o[1];
	    cm_curr_RCol = a_LcolNum;	/* starting column number from which
					 * we are asked to display data */

	    for (j = 0; j < cm_cols_perPage; j++) {	/* for each column */

		/* if there is another column of data */
		if (cm_curr_RCol < cm_numCols) {

		    dataIndex = cm_Display_map[cm_curr_RCol];

		    code =
			display_Server_datum(cmDataP->data[dataIndex],
					     *firstSlot_o_Ptr,
					     *secondSlot_o_Ptr,
					     cmDataP->probeOK, RIGHT_JUSTIFY,
					     cmDataP->threshOvf[dataIndex]);

		    cm_curr_RCol++;
		} else {	/* no more data, blank out columns */
		    code = display_Server_datum("", *firstSlot_o_Ptr, *secondSlot_o_Ptr, 1,	/* probe ok */
						RIGHT_JUSTIFY, 0);	/* no overflow */
		}


		firstSlot_o_Ptr++;	/* onode of next column */
		secondSlot_o_Ptr++;	/* onode of next column */

	    }			/* for each column */

	    /* the loop could have taken the right-column-index one over,
	     * adjust it now */
	    if (cm_curr_RCol == cm_numCols)
		cm_curr_RCol--;


	}



	/* if fdIdx < numCM */
	/* if cmIdx >= numCM , blank out all succeding rows */
	if (cmIdx >= numCM) {

	    /* blank out host name object */
	    tmp_lightobj =
		(struct gator_lightobj *)tmp_cm_lines_P->host_o->o_data;
	    code =
		justify_light("", tmp_lightobj->label, FC_HOSTNAME_O_WIDTH,
			      LEFT_JUSTIFY, 1);
	    if (code) {
		fprintf(debugFD, "[ %s ] justify_code returned %d\n", rn,
			code);
		fflush(debugFD);
	    }

	    firstSlot_o_Ptr = tmp_cm_lines_P->data_o[0];
	    secondSlot_o_Ptr = tmp_cm_lines_P->data_o[1];

	    for (k = 0; k < cm_cols_perPage; k++) {
		code = display_Server_datum("", *firstSlot_o_Ptr, *secondSlot_o_Ptr, 1,	/* probe OK */
					    RIGHT_JUSTIFY, 0);	/* dont highlight */

		firstSlot_o_Ptr++;
		secondSlot_o_Ptr++;
	    }

	}

	/* cmIDx >= numCM */
	tmp_cm_lines_P++;	/* pointer to next line in the frame */
	cmDataP++;		/* next host's data */
	cmIdx++;		/* host index */


    }				/* for each row in the Cache Manager frame */

    /* redraw the display if the Cache Managers screen is currently displayed */
    if (afsmon_win->w_frame == cmFrame)
	WOP_DISPLAY(afsmon_win);

    /* update the global page & column numbers to reflect the changes */
    cm_currPage = a_pageNum;
    cm_curr_LCol = a_LcolNum;;

    return (0);

}				/* cm_refresh */



/*-----------------------------------------------------------------------
 * Switch_cm_2_ovw()
 *
 * Description:
 *	Switch from the Cache Manager screen to the Overview Screen
 *----------------------------------------------------------------------*/
int
Switch_cm_2_ovw()
{
    /* bind the overview frame to the window */
    gtxframe_SetFrame(afsmon_win, ovwFrame);
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_2_fs()
 *
 * Description:
 *	Switch from the Cache Manager screen to the File Servers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_2_fs()
{
    if (cm_pageType & CMD_FS) {
	/* bind the file servers frame to the window */
	gtxframe_SetFrame(afsmon_win, fsFrame);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_next()
 *
 * Description:
 *	Switch to next page of cache managers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_next()
{
    static char rn[] = "Switch_cm_next";

    if (cm_pageType & CMD_NEXT) {
	/* we have a next page, refresh with next page number */
	cm_refresh(cm_currPage + 1, cm_curr_LCol);
    }

    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_last()
 *
 * Description:
 *	Switch to last page of file server screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_last()
{
    static char rn[] = "Switch_cm_last";


    if (cm_pageType & CMD_NEXT) {
	/* we have a next page, refresh with last page number */
	cm_refresh(cm_numPages, cm_curr_LCol);
    }

    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_prev()
 *
 * Description:
 *	Switch to previous page of cache managers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_prev()
{
    static char rn[] = "Switch_cm_prev";

    if (cm_pageType & CMD_PREV) {
	/* we have a previous page, refresh to previous page */
	cm_refresh(cm_currPage - 1, cm_curr_LCol);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_first()
 *
 * Description:
 *	Switch to first page of cache managers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_first()
{
    static char rn[] = "Switch_cm_first";

    if (cm_pageType & CMD_PREV) {
	/* we have a previous page, refresh to first page */
	cm_refresh(1, cm_curr_LCol);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_left()
 *
 * Description:
 *	Scroll left on the cache managers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_left()
{
    static char rn[] = "Switch_cm_left";

    if (cm_pageType & CMD_LEFT) {
	/* we have columns on left, refresh with new column number */
	cm_refresh(cm_currPage, cm_curr_LCol - cm_cols_perPage);
    }
    return (0);
}


/*-----------------------------------------------------------------------
 * Switch_cm_leftmost()
 *
 * Description:
 *	Scroll to first column on  the cache managers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_leftmost()
{
    static char rn[] = "Switch_cm_leftmost";

    if (cm_pageType & CMD_LEFT) {
	/* we have columns on left, go to the first column */
	cm_refresh(cm_currPage, 0);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_right()
 *
 * Description:
 *	Scroll right on the cache managers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_right()
{
    static char rn[] = "Switch_cm_right";

    if (cm_pageType & CMD_RIGHT) {
	/* we have columns on right, refresh with new column number */
	cm_refresh(cm_currPage, cm_curr_LCol + cm_cols_perPage);
    }
    return (0);
}

/*-----------------------------------------------------------------------
 * Switch_cm_rightmost()
 *
 * Description:
 *	Scroll to last column on the cache managers screen 
 *----------------------------------------------------------------------*/
int
Switch_cm_rightmost()
{
    static char rn[] = "Switch_cm_rightmost";
    int curr_LCol;


    if (cm_pageType & CMD_RIGHT) {
	/* we have columns on right, go to the last column */
	if (cm_numCols % cm_cols_perPage)
	    curr_LCol = (cm_numCols / cm_cols_perPage) * cm_cols_perPage;
	else
	    curr_LCol =
		((cm_numCols / cm_cols_perPage) - 1) * cm_cols_perPage;
	cm_refresh(cm_currPage, curr_LCol);
    }
    return (0);
}


/*-----------------------------------------------------------------------
 * create_CMframe_objects()
 *
 * Description:
 *	Create the gtx objects (onodes) for the Cache Managers frame and setup
 *	the keyboard bindings.
 *	Only as many objects as can fit on the display are created. The
 *	positions and lengths of all these objects are fixed at creation.
 *	These objects are updated with new data at the end of each probe
 *	cycle.
 *
 * Returns:
 *	Success: 0
 *	Failure: Exits afsmonitor.
 *----------------------------------------------------------------------*/

int
create_CMframe_objects()
{				/* create_CMframe_objects */
    static char rn[] = "create_CMframe_objects";
    struct ServerInfo_line *cm_lines_Ptr;
    struct onode **cm_data_o_Ptr;
    struct onode **cmLabels_o_Ptr;
    int x_pos;
    int y_pos;
    int code;
    int i;
    int j;
    int numBytes;
    int arrIdx;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }



    /* create the command line object */
    cm_cmd_o =
	initLightObject("Command [oview, fs, prev, next, left, right] ? ", 0,
			maxY - 1, FC_CMD_O_WIDTH, afsmon_win);
    if (cm_cmd_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create cm command onode\n", rn);
	afsmon_Exit(420);
    }
    code = gtxframe_AddToList(cmFrame, cm_cmd_o);
    code = gator_light_set(cm_cmd_o, HIGHLIGHT);


    /* we already have the dimensions for the frame - same as the ovw frame */
    /* use the ovw program name object for the cm screen too */

    code = gtxframe_AddToList(cmFrame, ovw_progName_o);


    /* create the page number object */
    cm_pageNum_o =
	initLightObject("[Cache Managers, p. X of X, c. Y of Y]",
			maxX - FC_PAGENUM_O_WIDTH, 0, FC_PAGENUM_O_WIDTH,
			afsmon_win);
    if (cm_pageNum_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create pageNumber onode\n", rn);
	afsmon_Exit(415);
    }
    code = gtxframe_AddToList(cmFrame, cm_pageNum_o);
    code = gator_light_set(cm_pageNum_o, HIGHLIGHT);

    /* create the probe number object */
    cm_probeNum_o =
	initLightObject("[CM probes 1, freq=30 sec]",
			maxX - FC_PROBENUM_O_WIDTH, maxY - 1,
			FC_PROBENUM_O_WIDTH, afsmon_win);
    if (cm_probeNum_o == NULL) {
	sprintf(errMsg, "[ %s ] Failed to create cm probeNum onode\n", rn);
	afsmon_Exit(425);
    }
    code = gtxframe_AddToList(cmFrame, cm_probeNum_o);
    code = gator_light_set(cm_probeNum_o, HIGHLIGHT);


    /* create the numCM monitored object */
    cm_numCM_o =
	initLightObject
	("  	0 Cache Mangers monitored, 0 alerts on 0 machines", 4, 2,
	 FC_NUMHOSTS_O_WIDTH, afsmon_win);
    if (cm_numCM_o == NULL) {
	sprintf(errMsg,
		"[ %s ] Failed to create numCM onode for the cm frame\n", rn);
	afsmon_Exit(430);
    }
    code = gtxframe_AddToList(cmFrame, cm_numCM_o);

    /* create the "more columns to left" indicator */
    cm_leftArrows_o =
	initLightObject("<<<", 0, 2, FC_ARROWS_O_WIDTH, afsmon_win);
    if (cm_leftArrows_o == NULL) {
	sprintf(errMsg,
		"[ %s ] Failed to create leftArrows onode for the cm frame\n",
		rn);
	afsmon_Exit(435);
    }
    code = gtxframe_AddToList(cmFrame, cm_leftArrows_o);

    /* create the "more columns to right" indicator */
    cm_rightArrows_o =
	initLightObject(">>>", maxX - FC_ARROWS_O_WIDTH, 2, FC_ARROWS_O_WIDTH,
			afsmon_win);
    if (cm_rightArrows_o == NULL) {
	sprintf(errMsg,
		"[ %s ] Failed to create rightArrows onode for the cm frame\n",
		rn);
	afsmon_Exit(440);
    }
    code = gtxframe_AddToList(cmFrame, cm_rightArrows_o);




    /* calculate the maximum number of hosts per page (2 rows per host) */
    cm_numHosts_perPage = (maxY - FC_NUM_FIXED_LINES) / 2;

    /* determine the number of data columns that can fit in a page */
    cm_cols_perPage = (maxX - FC_HOSTNAME_O_WIDTH) / (FC_COLUMN_WIDTH);

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] cm_numHosts_perPage=%d cm_cols_perPage=%d\n",
		rn, cm_numHosts_perPage, cm_cols_perPage);
	fflush(debugFD);
    }

    /* the above two variables give us the information needed to create
     * the objects for displaying the file server information */

    /* allocate memory for all the onode pointers required to display
     * the file server statistics */

    numBytes = cm_numHosts_perPage * sizeof(struct ServerInfo_line);
    cm_lines = (struct ServerInfo_line *)malloc(numBytes);
    if (cm_lines == (struct ServerInfo_line *)0) {
	sprintf(errMsg,
		"[ %s ] Failed to allocate %d bytes for CM data lines\n", rn,
		numBytes);
	afsmon_Exit(445);
    }

    /* for each line of server statistics allocate memory to store two arrays 
     * of data onodes */

    cm_lines_Ptr = cm_lines;
    for (i = 0; i < cm_numHosts_perPage; i++) {
	for (arrIdx = 0; arrIdx < 2; arrIdx++) {
	    numBytes = cm_cols_perPage * sizeof(struct onode *);
	    cm_lines_Ptr->data_o[arrIdx] = (struct onode **)malloc(numBytes);
	    if (cm_lines_Ptr->data_o[arrIdx] == NULL) {
		sprintf(errMsg,
			"[ %s ] Failed to allocate %d bytes for CM data onodes\n",
			rn, numBytes);
		afsmon_Exit(450);
	    }
	}
	cm_lines_Ptr++;
    }

    /* now allocate the onodes itself */

    cm_lines_Ptr = cm_lines;
    for (i = 0; i < cm_numHosts_perPage; i++) {

	/* initialize host name onode */
	cm_lines_Ptr->host_o =
	    initLightObject("CMHostName", 0, FC_FIRST_HOST_ROW + 2 * i,
			    FC_HOSTNAME_O_WIDTH, afsmon_win);
	if (cm_lines_Ptr->host_o == NULL) {
	    sprintf(errMsg, "[ %s ] Failed to create an CM name onode\n", rn);
	    afsmon_Exit(455);
	}
	code = gtxframe_AddToList(cmFrame, cm_lines_Ptr->host_o);

	/* if (afsmon_debug) {
	 * fprintf(debugFD,"[ %s ] Addr of host_o = %d for line %d\n",
	 * rn,cm_lines_Ptr->host_o,i);
	 * fflush(debugFD);
	 * } */

	/* initialize data onodes for this host */

	for (arrIdx = 0; arrIdx < 2; arrIdx++) {	/* for each array index */

	    cm_data_o_Ptr = cm_lines_Ptr->data_o[arrIdx];
	    for (j = 0; j < cm_cols_perPage; j++) {	/* for each column */

		char tmpBuf[20];

		/* determine x & y coordinate for this data object */
		/* the 1's are for leaving a blank after each column */
		x_pos = FC_HOSTNAME_O_WIDTH + (j * (FC_COLUMN_WIDTH));
		y_pos = FC_FIRST_HOST_ROW + 2 * i + arrIdx;

		sprintf(tmpBuf, "-CMData %d-", arrIdx);
		*cm_data_o_Ptr =
		    initLightObject(tmpBuf, x_pos, y_pos, FC_COLUMN_WIDTH,
				    afsmon_win);
		if (*cm_data_o_Ptr == NULL) {
		    sprintf(errMsg,
			    "[ %s ] Failed to create an CM data onode\n", rn);
		    afsmon_Exit(460);
		}
		code = gtxframe_AddToList(cmFrame, *cm_data_o_Ptr);

		cm_data_o_Ptr++;
	    }			/* for each column */
	}			/* for each onode array index */

	cm_lines_Ptr++;
    }				/* for each host slot */


    /* INITIALIZE COLUMN LABELS */


    /* allocate memory for two arrays of onode pointers for file server column
     * labels */
    for (arrIdx = 0; arrIdx < 3; arrIdx++) {

	cmLabels_o[arrIdx] =
	    (struct onode **)malloc(sizeof(struct onode *) * cm_cols_perPage);
	if (cmLabels_o[arrIdx] == NULL) {
	    sprintf(errMsg,
		    "[ %s ] Failed to allocate memory for CM label onodes\n",
		    rn);
	    afsmon_Exit(465);
	}

	/* create cache manager name objects */
	cmLabels_o_Ptr = cmLabels_o[arrIdx];
	for (i = 0; i < cm_cols_perPage; i++) {
	    *cmLabels_o_Ptr =
		initLightObject("", FC_HOSTNAME_O_WIDTH + i * FC_COLUMN_WIDTH,
				FC_FIRST_LABEL_ROW + arrIdx, FC_COLUMN_WIDTH,
				afsmon_win);

	    if (*cmLabels_o_Ptr == NULL) {
		sprintf(errMsg, "[ %s ] Failed to create a CM label onode\n",
			rn);
		afsmon_Exit(470);
	    }
	    code = gtxframe_AddToList(cmFrame, *cmLabels_o_Ptr);
	    cmLabels_o_Ptr++;
	}

    }






    /* initialize the column & page counters */

    cm_currPage = 1;
    cm_numCols = cm_DisplayItems_count;
    cm_numPages = numCM / cm_numHosts_perPage;
    if (numCM % cm_numHosts_perPage)
	cm_numPages++;
    cm_curr_LCol = 0;		/* leftmost col */
    cm_curr_RCol = 0;		/* rightmost col */

    /* create keyboard bindings */
    /* bind Q and  to exit */
    keymap_BindToString(cmFrame->keymap, "Q", afsmonExit_gtx, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "", afsmonExit_gtx, NULL, NULL);

    /* o = overview, c = cm, n = next, p = prev, l = left, r = right 
     * N = last page, P = first page, L = leftmost col, R = rightmost col */

    keymap_BindToString(cmFrame->keymap, "o", Switch_cm_2_ovw, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "f", Switch_cm_2_fs, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "n", Switch_cm_next, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "N", Switch_cm_last, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "p", Switch_cm_prev, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "P", Switch_cm_first, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "l", Switch_cm_left, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "L", Switch_cm_leftmost, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "r", Switch_cm_right, NULL, NULL);
    keymap_BindToString(cmFrame->keymap, "R", Switch_cm_rightmost, NULL,
			NULL);

    return (0);
}				/* create_CMframe_objects */



/*-----------------------------------------------------------------------
 * gtx_initialize()
 *
 * Description:
 *	Initialize the gtx package and call routines to create the objects
 *	for the overview, File Servers & Cache Managers screens.
 *----------------------------------------------------------------------*/
int
gtx_initialize()
{				/* gtx_initialize */
    static char rn[] = "gtx_initialize";	/* routine name */
    int code;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called\n", rn);
	fflush(debugFD);
    }

    afsmon_win = gtx_Init(0, -1);	/* 0 => dont start input server,
					 * 1 => use curses */
    if (afsmon_win == NULL) {
	sprintf(errMsg, "[ %s ] gtx initialization failed\n", rn);
	afsmon_Exit(475);
    }
    gtx_initialized = 1;

    /* Create the Overview frame */

    ovwFrame = gtxframe_Create();
    if (ovwFrame == (struct gtx_frame *)0) {
	sprintf(errMsg, "[ %s ] Failed to create overview frame\n", rn);
	afsmon_Exit(480);
    }

    /* bind the overview frame to the window */
    gtxframe_SetFrame(afsmon_win, ovwFrame);

    /* create overview frame objects */
    code = create_ovwFrame_objects();
    if (code) {
	sprintf(errMsg, "[ %s ] Error in creating ovw frame objects\n", rn);
	afsmon_Exit(485);
    }


    /* Create the File Server frame */
    fsFrame = gtxframe_Create();
    if (fsFrame == (struct gtx_frame *)0) {
	sprintf(errMsg, "[ %s ] Failed to create file server frame\n", rn);
	afsmon_Exit(490);
    }


    /* Create File Server frame objects */
    code = create_FSframe_objects();
    if (code) {
	sprintf(errMsg, "[ %s ] Error in creating FS frame objects\n", rn);
	afsmon_Exit(495);
    }

    /* Create the Cache Managers frame */
    cmFrame = gtxframe_Create();
    if (cmFrame == (struct gtx_frame *)0) {
	sprintf(errMsg, "[ %s ] Failed to create Cache Managers frame\n", rn);
	afsmon_Exit(500);
    }

    /* Create Cache Managers frame objects */
    code = create_CMframe_objects();
    if (code) {
	sprintf(errMsg, "[ %s ] Error in creating CM frame objects\n", rn);
	afsmon_Exit(505);
    }

    /* misc initializations */
    sprintf(blankline, "%255s", " ");

    return (0);
}				/* gtx_initialize */
