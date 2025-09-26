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

#include <roken.h>
#include <ctype.h>
#include <afs/cmd.h>
#include <afs/afsint.h>
#include <rx/rx_queue.h>
#include "nfs.h"
#include "ihandle.h"
#include <afs/afs_lock.h>
#include "vnode.h"  /* for vSmall, vLarge */
#include "vol-info.h"

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

static const char *progname = "volinfo";

/* Command line options */
typedef enum {
    P_CHECKOUT,
    P_VNODE,
    P_DATE,
    P_INODE,
    P_ITIME,
    P_PART,
    P_VOLUMEID,
    P_HEADER,
    P_SIZEONLY,
    P_FIXHEADER,
    P_SAVEINODES,
    P_ORPHANED,
    P_FILENAMES,
} volinfo_parm_t;

/**
 * Process command line options and start scanning
 *
 * @param[in] as     command syntax object
 * @param[in] arock  opaque object; not used
 *
 * @return error code
 */
static int
VolInfo(struct cmd_syndesc *as, void *arock)
{
    int code;
    struct cmd_item *ti;
    VolumeId volumeId = 0;
    char *partNameOrId = NULL;
    struct VolInfoOpt *opt;

    code = volinfo_Init(progname);
    if (code) {
	return code;
    }
    code = volinfo_Options(&opt);
    if (code) {
	return code;
    }

    if (as->parms[P_CHECKOUT].items) {
	opt->checkout = 1;
    }
    if (as->parms[P_VNODE].items) {
	opt->dumpVnodes = 1;
    }
    if (as->parms[P_DATE].items) {
	opt->dumpDate = 1;
    }
    if (as->parms[P_INODE].items) {
	opt->dumpInodeNumber = 1;
    }
    if (as->parms[P_ITIME].items) {
	opt->dumpInodeTimes = 1;
    }
    if ((ti = as->parms[P_PART].items)) {
	partNameOrId = ti->data;
    }
    if ((ti = as->parms[P_VOLUMEID].items)) {
	volumeId = strtoul(ti->data, NULL, 10);
	if (volumeId == 0) {
	    fprintf(stderr, "%s: Invalid -volumeid value: %s\n", progname,
		    ti->data);
	    return 1;
	}
    }
    if (as->parms[P_HEADER].items) {
	opt->dumpHeader = 1;
    }
    if (as->parms[P_SIZEONLY].items) {
	opt->showSizes = 1;
    }
    if (as->parms[P_FIXHEADER].items) {
	opt->fixHeader = 1;
    }
    if (as->parms[P_SAVEINODES].items) {
	opt->saveInodes = 1;
    }
    if (as->parms[P_ORPHANED].items) {
	opt->showOrphaned = 1;
    }
    if (as->parms[P_FILENAMES].items) {
	opt->dumpFileNames = 1;
    }

    /* -saveinodes and -sizeOnly override the default mode.
     * For compatibility with old versions of volinfo, -orphaned
     * and -filename options imply -vnodes */
    if (opt->saveInodes || opt->showSizes) {
	opt->dumpInfo = 0;
	opt->dumpHeader = 0;
	opt->dumpVnodes = 0;
	opt->dumpInodeTimes = 0;
	opt->showOrphaned = 0;
    } else if (opt->showOrphaned) {
	opt->dumpVnodes = 1;		/* implied */
    } else if (opt->dumpFileNames) {
	opt->dumpVnodes = 1;		/* implied */
    }

    if (opt->saveInodes) {
	volinfo_AddVnodeHandler(vSmall, volinfo_SaveInode,
			"Saving all volume files to current directory ...\n");
    }
    if (opt->showSizes) {
	volinfo_AddVnodeHandler(vLarge, volinfo_AddVnodeToSizeTotals, NULL);
	volinfo_AddVnodeHandler(vSmall, volinfo_AddVnodeToSizeTotals, NULL);
    }
    if (opt->dumpVnodes) {
	volinfo_AddVnodeHandler(vLarge, volinfo_PrintVnode, "\nLarge vnodes (directories)\n");
	volinfo_AddVnodeHandler(vSmall, volinfo_PrintVnode,
			"\nSmall vnodes(files, symbolic links)\n");
    }
    code = volinfo_ScanPartitions(opt, partNameOrId, volumeId);
    if (opt) {
        free(opt);
    }
    return code;
}

/**
 * volinfo program entry
 */
int
main(int argc, char **argv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, VolInfo, NULL, 0,
			  "Dump volume's internal state");
    cmd_AddParmAtOffset(ts, P_CHECKOUT, "-checkout", CMD_FLAG, CMD_OPTIONAL,
			"Checkout volumes from running fileserver");
    cmd_AddParmAtOffset(ts, P_VNODE, "-vnode", CMD_FLAG, CMD_OPTIONAL,
			"Dump vnode info");
    cmd_AddParmAtOffset(ts, P_DATE, "-date", CMD_FLAG, CMD_OPTIONAL,
			"Also dump vnode's mod date");
    cmd_AddParmAtOffset(ts, P_INODE, "-inode", CMD_FLAG, CMD_OPTIONAL,
			"Also dump vnode's inode number");
    cmd_AddParmAtOffset(ts, P_ITIME, "-itime", CMD_FLAG, CMD_OPTIONAL,
			"Dump special inode's mod times");
    cmd_AddParmAtOffset(ts, P_PART, "-part", CMD_LIST, CMD_OPTIONAL,
			"AFS partition name or id (default current partition)");
    cmd_AddParmAtOffset(ts, P_VOLUMEID, "-volumeid", CMD_LIST, CMD_OPTIONAL,
			"Volume id");
    cmd_AddParmAtOffset(ts, P_HEADER, "-header", CMD_FLAG, CMD_OPTIONAL,
			"Dump volume's header info");
    cmd_AddParmAtOffset(ts, P_SIZEONLY, "-sizeonly", CMD_FLAG, CMD_OPTIONAL,
			"Dump volume's size");
    cmd_AddParmAtOffset(ts, P_FIXHEADER, "-fixheader", CMD_FLAG,
			CMD_OPTIONAL, "Try to fix header");
    cmd_AddParmAtOffset(ts, P_SAVEINODES, "-saveinodes", CMD_FLAG,
			CMD_OPTIONAL, "Try to save all inodes");
    cmd_AddParmAtOffset(ts, P_ORPHANED, "-orphaned", CMD_FLAG, CMD_OPTIONAL,
			"List all dir/files without a parent");
#if defined(AFS_NAMEI_ENV)
    cmd_AddParmAtOffset(ts, P_FILENAMES, "-filenames", CMD_FLAG,
			CMD_OPTIONAL, "Also dump vnode's namei filename");
#endif

    /* For compatibility with older versions. */
    cmd_AddParmAlias(ts, P_SIZEONLY, "-sizeOnly");

    code = cmd_Dispatch(argc, argv);
    return code;
}
