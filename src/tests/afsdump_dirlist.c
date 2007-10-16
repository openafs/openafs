/*
 * CMUCS AFStools
 * dumpscan - routines for scanning and manipulating AFS volume dumps
 *
 * Copyright (c) 1998 Carnegie Mellon University
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

/* afsdump_dirlist.c - List an AFS directory file */

#include <sys/fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "dumpscan.h"

extern int optind;
extern char *optarg;

char *argv0;
static char *input_path;
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
    fprintf(stderr, "  -h     Print this help message\n");
    fprintf(stderr, "  -q     Quiet mode (don't print errors)\n");
    fprintf(stderr, "  -v     Verbose mode\n");
    exit(status);
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
    input_path = 0;
    quiet = verbose = 0;

    /* Initialize other stuff */
    error_count = 0;

    /* Parse the options */
    while ((c = getopt(argc, argv, "hqv")) != EOF) {
	switch (c) {
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
}


/* A callback to count and print errors */
static afs_uint32
my_error_cb(afs_uint32 code, int fatal, void *ref, char *msg, ...)
{
    va_list alist;

    error_count++;
    if (!quiet) {
	va_start(alist, msg);
	afs_com_err_va(argv0, code, msg, alist);
	va_end(alist);
    }
}


/* Main program */
void
main(int argc, char **argv)
{
    XFILE input_file;
    afs_uint32 r;

    parse_options(argc, argv);
    initialize_acfg_error_table();
    initialize_AVds_error_table();
    initialize_rxk_error_table();
    initialize_u_error_table();
    initialize_vl_error_table();
    initialize_vols_error_table();
    initialize_xFil_error_table();
    r = xfopen(&input_file, O_RDONLY, input_path);
    if (r) {
	afs_com_err(argv0, r, "opening %s", input_path);
	exit(2);
    }

    memset(&dp, 0, sizeof(dp));
    dp.cb_error = my_error_cb;
    dp.print_flags = DSPRINT_DIR;
    if (input_file.is_seekable)
	dp.flags |= DSFLAG_SEEK;

    r = ParseDirectory(&input_file, &dp, 0, 1);
    xfclose(&input_file);

    if (verbose && error_count)
	fprintf(stderr, "*** %d errors\n", error_count);
    if (r && !quiet)
	fprintf(stderr, "*** FAILED: %s\n", afs_error_message(r));
    exit(0);
}
