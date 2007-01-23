/*
 * COPYRIGHT NOTICE
 * Copyright (c) 1997 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/* UB - Unified Backups */
/* methods/afs/dumpscan/afsdump_scan.c - General-purpose dump scanner */

#include "dumpscan.h"
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

extern int opterr, optind;
extern char *optarg;

extern XFILE repair_output;
extern afs_uint32 repair_dumphdr_cb(afs_dump_header *, XFILE *, void *);
extern afs_uint32 repair_volhdr_cb(afs_vol_header *, XFILE *, void *);
extern afs_uint32 repair_vnode_cb(afs_vnode *, XFILE *, void *);

char *argv0;
static char *input_path, *gendump_path;
static afs_uint32 printflags, repairflags, add_admin;
static int quiet, verbose, error_count;

static path_hashinfo phi;
static dump_parser dp;


/* Print a usage message and exit */
static void
usage(int status, char *msg)
{
    if (msg)
	fprintf(stderr, "%s: %s\n", argv0, msg);
    fprintf(stderr, "Usage: %s [options] [file]\n", argv0);
    fprintf(stderr, "  -Pxxx  Set print options:\n");
    fprintf(stderr, "          B = Print backup system header (if any)\n");
    fprintf(stderr, "          H = Print AFS dump header\n");
    fprintf(stderr, "          V = Print AFS volume header\n");
    fprintf(stderr, "          v = List vnodes\n");
    fprintf(stderr, "          p = Include path to each vnode\n");
    fprintf(stderr, "          i = Include info for each vnode\n");
    fprintf(stderr, "          d = List directory contents\n");
    fprintf(stderr, "          a = List access control lists\n");
    fprintf(stderr, "          g = Print debugging info\n");
    fprintf(stderr, "  -Rxxx  Set repair options:\n");
    fprintf(stderr, "          0 = Skip null tags\n");
    fprintf(stderr, "          b = Seek backward to find skipped tags\n");
    fprintf(stderr, "          d = Resync after vnode data\n");
    fprintf(stderr, "          v = Resync after corrupted vnodes\n");
    fprintf(stderr,
	    "  -Annn  Add all rights for ID nnn to every directory\n");
    fprintf(stderr, "  -h     Print this help message\n");
    fprintf(stderr, "  -gxxx  Generate a new dump in file xxx\n");
    fprintf(stderr, "  -q     Quiet mode (don't print errors)\n");
    fprintf(stderr, "  -v     Verbose mode\n");
    exit(status);
}


/* Parse the argument given to the -P option.
 * Returns the resulting * dumpscan print flags (DSPRINT_*).
 * If an unrecognized flag is used, prints an error message and exits.
 */
static afs_uint32
parse_printflags(char *flags)
{
    afs_uint32 result = 0;
    char *x;

    for (x = flags; *x; x++)
	switch (*x) {
	case 'B':
	    result |= DSPRINT_BCKHDR;
	    continue;
	case 'H':
	    result |= DSPRINT_DUMPHDR;
	    continue;
	case 'V':
	    result |= DSPRINT_VOLHDR;
	    continue;
	case 'v':
	    result |= DSPRINT_ITEM;
	    continue;
	case 'p':
	    result |= DSPRINT_PATH;
	    continue;
	case 'i':
	    result |= DSPRINT_VNODE;
	    continue;
	case 'd':
	    result |= DSPRINT_DIR;
	    continue;
	case 'a':
	    result |= DSPRINT_ACL;
	    continue;
	case 'g':
	    result |= DSPRINT_DEBUG;
	    continue;
	default:
	    usage(1, "Invalid print options!");
	}
    return result;
}


/* Parse the argument given to the -R option.
 * Returns the resulting * dumpscan repair flags (DSFIX_*).
 * If an unrecognized flag is used, prints an error message and exits.
 */
static afs_uint32
parse_repairflags(char *flags)
{
    afs_uint32 result = 0;
    char *x;

    for (x = flags; *x; x++)
	switch (*x) {
	case '0':
	    result |= DSFIX_SKIP;
	    continue;
	case 'b':
	    result |= DSFIX_RSKIP;
	    continue;
	case 'd':
	    result |= DSFIX_VDSYNC;
	    continue;
	case 'v':
	    result |= DSFIX_VFSYNC;
	    continue;
	default:
	    usage(1, "Invalid repair options!");
	}
    return result;
}


/* Parse the command-line options */
static void
parse_options(int argc, char **argv)
{
    int c;

    /* Set the program name */
    if (argv0 = strrchr(argv[0], '/'))
	argv0++;
    else
	argv0 = argv[0];

    /* Initialize options */
    input_path = gendump_path = 0;
    printflags = repairflags = add_admin = 0;
    quiet = verbose = 0;

    /* Initialize other stuff */
    error_count = 0;

    /* Parse the options */
    while ((c = getopt(argc, argv, "A:P:R:g:hv")) != EOF) {
	switch (c) {
	case 'A':
	    add_admin = atoi(optarg);
	    continue;
	case 'P':
	    printflags = parse_printflags(optarg);
	    continue;
	case 'R':
	    repairflags = parse_repairflags(optarg);
	    continue;
	case 'g':
	    gendump_path = optarg;
	    continue;
	case 'q':
	    quiet = 1;
	    continue;
	case 'v':
	    verbose = 1;
	    continue;
	case 'h':
	    usage(0, 0);
	default:
	    usage(1, "Invalid option!");
	}
    }

    if (quiet && verbose)
	usage(1, "Can't specify both -q and -v");

    /* Parse non-option arguments */
    if (argc - optind > 1)
	usage(1, "Too many arguments!");
    input_path = (argc == optind) ? "-" : argv[optind];
    if (add_admin && !gendump_path)
	add_admin = 0;
}


/* A callback to count and print errors */
static afs_uint32
my_error_cb(afs_uint32 code, int fatal, void *ref, char *msg, ...)
{
    va_list alist;

    error_count++;
    if (!quiet) {
	va_start(alist, msg);
	com_err_va(argv0, code, msg, alist);
	va_end(alist);
    }
}


/* A callback to print the path of a vnode. */
static afs_uint32
print_vnode_path(afs_vnode * v, XFILE * X, void *refcon)
{
    afs_uint32 r;
    char *name = 0;

    /* Do repair, but only for known vnode types */
    if (gendump_path && (!(v->field_mask & F_VNODE_TYPE)
			 || v->type != vFile || v->type != vDirectory
			 || v->type != vSymlink)) {
	r = repair_vnode_cb(v, X, refcon);
	if (r)
	    return r;
    }
    r = Path_Build(X, &phi, v->vnode, &name, 0);
    if (!r && name)
	printf(" Path: %s\n", name);
    if (name)
	free(name);
    return r;
}


static afs_uint32
munge_admin_acl(afs_vnode * v, XFILE * X, void *refcon)
{
    struct acl_accessList *acl;
    int add_entry = 1, remove_entry = -1;
    int i, o, n;

    acl = (struct acl_accessList *)(v->acl);
    o = n = ntohl(acl->positive);
    for (i = 0; i < n; i++)
	if (ntohl(acl->entries[i].id) == add_admin)
	    add_entry = 0;
    n = ntohl(acl->negative);
    for (i = o; i < n + o; i++)
	if (ntohl(acl->entries[i].id) == add_admin)
	    remove_entry = i;

    if (add_entry) {
	for (i = (remove_entry < 0) ? o + n : remove_entry; i > o; i--) {
	    acl->entries[i].id = acl->entries[i - 1].id;
	    acl->entries[i].rights = acl->entries[i - 1].rights;
	}
	acl->entries[o].id = htonl(add_admin);
	acl->entries[o].rights =
	    htonl((PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
		   PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER));
	acl->positive = htonl(o + 1);
	if (remove_entry < 0)
	    acl->total = htonl(o + n + 1);
	else
	    acl->negative = htonl(n - 1);
    } else if (remove_entry >= 0) {
	for (i = remove_entry; i < o + n - 1; i++) {
	    acl->entries[i].id = acl->entries[i + 1].id;
	    acl->entries[i].rights = acl->entries[i + 1].rights;
	}
	acl->negative = htonl(n - 1);
	acl->total = htonl(o + n - 1);
    }
    return repair_vnode_cb(v, X, refcon);
}


/* Setup for generating a repaired dump */
static afs_uint32
setup_repair(void)
{
    afs_uint32 r;

    r = xfopen(&repair_output, gendump_path, O_RDWR, 0644);
    if (r)
	return r;

    dp.cb_dumphdr = repair_dumphdr_cb;
    dp.cb_volhdr = repair_volhdr_cb;
    dp.cb_vnode_dir = repair_vnode_cb;
    dp.cb_vnode_file = repair_vnode_cb;
    dp.cb_vnode_link = repair_vnode_cb;
    dp.cb_vnode_empty = repair_vnode_cb;
    return 0;
}


/* Main program */
void
main(int argc, char **argv)
{
    XFILE *X;
    afs_uint32 r;

    parse_options(argc, argv);
    initialize_UB_error_table();
    initialize_UBsp_error_table();
    initialize_AVds_error_table();
    initialize_rx_error_table();
    r = xfopen(&X, input_path, O_RDONLY, 0);
    if (r) {
	com_err(argv0, r, "opening %s", input_path);
	exit(2);
    }

    bzero(&dp, sizeof(dp));
    dp.cb_error = my_error_cb;
    dp.repair_flags = repairflags;
    if (X->is_seekable)
	dp.flags |= DSFLAG_SEEK;
    else {
	if (repairflags)
	    fprintf(stderr,
		    "Repair modes available only for seekable dumps\n");
	if (printflags & DSPRINT_PATH)
	    fprintf(stderr,
		    "Path-printing available only for seekable dumps\n");
	if (repairflags || (printflags & DSPRINT_PATH))
	    exit(1);
    }

    if (gendump_path && (r = setup_repair())) {
	com_err(argv0, r, "setting up repair output");
	xfclose(X);
	exit(2);
    }

    if (printflags & DSPRINT_PATH) {
	u_int64 where;

	dp.print_flags = printflags & DSPRINT_DEBUG;
	bzero(&phi, sizeof(phi));
	phi.p = &dp;

	if ((r = xftell(X, &where))
	    || (r = Path_PreScan(X, &phi, 0))
	    || (r = xfseek(X, &where))) {
	    com_err(argv0, r, "- path initialization failed");
	    xfclose(X);
	    exit(2);
	}

	dp.cb_vnode_dir = print_vnode_path;
	dp.cb_vnode_file = print_vnode_path;
	dp.cb_vnode_link = print_vnode_path;
	dp.cb_vnode_empty = print_vnode_path;
	dp.cb_vnode_wierd = print_vnode_path;
    }

    if (add_admin) {
	dp.cb_vnode_dir = munge_admin_acl;
    }

    dp.print_flags = printflags;
    r = ParseDumpFile(X, &dp);
    if (gendump_path) {
	if (!r)
	    r = DumpDumpEnd(&repair_output);
	if (!r)
	    r = xfclose(&repair_output);
	else
	    xfclose(&repair_output);
    }

    if (verbose && error_count)
	fprintf(stderr, "*** %d errors\n", error_count);
    if (r && !quiet)
	fprintf(stderr, "*** FAILED: %s\n", error_message(r));
    exit(0);
}
