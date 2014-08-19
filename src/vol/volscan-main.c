/*
 * Copyright 2013-2014, Sine Nomine Associates
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <ctype.h>
#include <afs/cmd.h>
#include <afs/afsint.h>
#include <rx/rx_queue.h>
#include "nfs.h"
#include "ihandle.h"
#include "lock.h"
#include "vnode.h"  /* for vSmall, vLarge */
#include "vol-info.h"

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

static const char *progname = "volscan";

/* All columns names as a single string. */
#define c(x) #x " "
static char *ColumnNames = VOLSCAN_COLUMNS;
#undef c
#undef VOLSCAN_COLUMNS

/* Command line options */
typedef enum {
    P_CHECKOUT,
    P_PART,
    P_VOLUMEID,
    P_TYPE,
    P_FIND,
    P_MASK,
    P_OUTPUT,
    P_DELIM,
    P_NOHEADING,
    P_NOMAGIC,
} volscan_parm_t;

/**
 * Process command line options and start scanning
 *
 * @param[in] as     command syntax object
 * @param[in] arock  opaque object; not used
 *
 * @return error code
 *    @retval 0 success
 *    @retvol 1 failure
 */
static int
VolScan(struct cmd_syndesc *as, void *arock)
{
    int code;
    struct cmd_item *ti;
    VolumeId volumeId = 0;
    char *partNameOrId = NULL;
    int i;
    struct VolInfoOpt *opt;

    code = volinfo_Init(progname);
    if (code) {
	return code;
    }
    code = volinfo_Options(&opt);
    if (code) {
	return code;
    }

    opt->dumpInfo = 0;		/* volscan mode */
    if (as->parms[P_CHECKOUT].items) {
	opt->checkout = 1;
    }
    if ((ti = as->parms[P_PART].items)) {
	partNameOrId = ti->data;
    }
    if ((ti = as->parms[P_VOLUMEID].items)) {
	volumeId = strtoul(ti->data, NULL, 10);
    }
    if (as->parms[P_NOHEADING].items) {
	opt->printHeading = 0;
    } else {
	opt->printHeading = 1;
    }
    if (as->parms[P_NOMAGIC].items) {
        opt->checkMagic = 0;
    }
    if ((ti = as->parms[P_DELIM].items)) {
	strncpy(opt->columnDelim, ti->data, 15);
	opt->columnDelim[15] = '\0';
    }

    /* Limit types of volumes to scan. */
    if (!as->parms[P_TYPE].items) {
	opt->scanVolType = (SCAN_RW | SCAN_RO | SCAN_BK);
    } else {
	opt->scanVolType = 0;
	for (ti = as->parms[P_TYPE].items; ti; ti = ti->next) {
	    if (strcmp(ti->data, "rw") == 0) {
		opt->scanVolType |= SCAN_RW;
	    } else if (strcmp(ti->data, "ro") == 0) {
		opt->scanVolType |= SCAN_RO;
	    } else if (strcmp(ti->data, "bk") == 0) {
		opt->scanVolType |= SCAN_BK;
	    } else {
		fprintf(stderr, "%s: Unknown -type value: %s\n", progname,
			ti->data);
		return 1;
	    }
	}
    }

    /* Limit types of vnodes to find (plus access entries) */
    if (!as->parms[P_FIND].items) {
	opt->findVnType = (FIND_FILE | FIND_DIR | FIND_MOUNT | FIND_SYMLINK);
    } else {
	opt->findVnType = 0;
	for (ti = as->parms[P_FIND].items; ti; ti = ti->next) {
	    if (strcmp(ti->data, "file") == 0) {
		opt->findVnType |= FIND_FILE;
	    } else if (strcmp(ti->data, "dir") == 0) {
		opt->findVnType |= FIND_DIR;
	    } else if (strcmp(ti->data, "mount") == 0) {
		opt->findVnType |= FIND_MOUNT;
	    } else if (strcmp(ti->data, "symlink") == 0) {
		opt->findVnType |= FIND_SYMLINK;
	    } else if (strcmp(ti->data, "acl") == 0) {
		opt->findVnType |= FIND_ACL;
	    } else {
		fprintf(stderr, "%s: Unknown -find value: %s\n", progname,
			ti->data);
		return 1;
	    }
	}
    }

    /* Show vnodes matching one of the mode masks */
    for (i = 0, ti = as->parms[P_MASK].items; ti; i++, ti = ti->next) {
	if (i >= (sizeof(opt->modeMask) / sizeof(*opt->modeMask))) {
	    fprintf(stderr, "Too many -mask values\n");
	    return -1;
	}
	opt->modeMask[i] = strtol(ti->data, NULL, 8);
	if (!opt->modeMask[i]) {
	    fprintf(stderr, "Invalid -mask value: %s\n", ti->data);
	    return 1;
	}
    }

    /* Set which columns to be printed. */
    if (!as->parms[P_OUTPUT].items) {
	volinfo_AddOutputColumn("host");
	volinfo_AddOutputColumn("desc");
	volinfo_AddOutputColumn("fid");
	volinfo_AddOutputColumn("dv");
	if (opt->findVnType & FIND_ACL) {
	    volinfo_AddOutputColumn("aid");
	    volinfo_AddOutputColumn("arights");
	}
	volinfo_AddOutputColumn("path");
    } else {
	for (ti = as->parms[P_OUTPUT].items; ti; ti = ti->next) {
	    if (volinfo_AddOutputColumn(ti->data) != 0) {;
		fprintf(stderr, "%s: Unknown -output value: %s\n", progname,
			ti->data);
		return 1;
	    }
	}
    }

    if (opt->findVnType & FIND_DIR) {
	volinfo_AddVnodeHandler(vLarge, volinfo_PrintVnodeDetails, NULL);
    }
    if (opt->findVnType & (FIND_FILE | FIND_MOUNT | FIND_SYMLINK)) {
	volinfo_AddVnodeHandler(vSmall, volinfo_PrintVnodeDetails, NULL);
    }
    if (opt->findVnType & FIND_ACL) {
	volinfo_AddVnodeHandler(vLarge, volinfo_ScanAcl, NULL);
    }

    code = volinfo_ScanPartitions(opt, partNameOrId, volumeId);
    if (opt) {
	free(opt);
    }
    return code;
}
/**
 * volscan program entry
 */
int
main(int argc, char **argv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, VolScan, NULL,
			  "Print volume vnode information");

    cmd_AddParm(ts, "-checkout", CMD_FLAG, CMD_OPTIONAL,
			"Checkout volumes from running fileserver");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
			"AFS partition name or id (default current partition)");
    cmd_AddParm(ts, "-volumeid", CMD_SINGLE, CMD_OPTIONAL,
			"Volume id (-partition required)");
    cmd_AddParm(ts, "-type", CMD_LIST, CMD_OPTIONAL,
			"Volume types: rw, ro, bk");
    cmd_AddParm(ts, "-find", CMD_LIST, CMD_OPTIONAL,
			"Objects to find: file, dir, mount, symlink, acl");
    cmd_AddParm(ts, "-mask", CMD_LIST, (CMD_OPTIONAL | CMD_HIDE),
                        "Unix mode mask (example: 06000)");
    cmd_AddParm(ts, "-output", CMD_LIST, CMD_OPTIONAL,
			ColumnNames);
    cmd_AddParm(ts, "-delim", CMD_SINGLE, CMD_OPTIONAL,
			"Output field delimiter");
    cmd_AddParm(ts, "-noheading", CMD_FLAG, CMD_OPTIONAL,
			"Do not print the heading line");
    cmd_AddParm(ts, "-ignore-magic", CMD_FLAG, CMD_OPTIONAL,
			"Skip directory vnode magic checks when looking up paths.");

    code = cmd_Dispatch(argc, argv);
    return code;
}
