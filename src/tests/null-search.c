#include <sys/fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "dumpscan.h"

char *argv0;
static char *input_path = 0;
static int quiet = 0, showpaths = 0, searchcount = 1;
static int error_count = 0, bad_count = 0;
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
    fprintf(stderr, "  -p     Print paths of bad vnodes\n");
    fprintf(stderr, "  -q     Quiet mode (don't print errors)\n");
    exit(status);
}


/* Parse the command-line options */
static void
parse_options(int argc, char **argv)
{
    int c;

    if (argv0 = strrchr(argv[0], '/'))
	argv0++;
    else
	argv0 = argv[0];

    /* Parse the options */
    while ((c = getopt(argc, argv, "n:hpq")) != EOF) {
	switch (c) {
	case 'n':
	    searchcount = atoi(optarg);
	    continue;
	case 'p':
	    showpaths = 1;
	    continue;
	case 'q':
	    quiet = 1;
	    continue;
	case 'h':
	    usage(0, 0);
	default:
	    usage(1, "Invalid option!");
	}
    }

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


/* A callback to process file vnodes */
static afs_uint32
my_file_cb(afs_vnode * v, XFILE * X, void *refcon)
{
    static char buf[1024];
    afs_uint32 size, nulls, cnulls, maxcnulls, n, r;
    char *name = 0;
    int i;

    nulls = cnulls = maxcnulls = 0;
    size = v->size;
    if ((r = xfseek(X, &v->d_offset)))
	return r;
    while (size) {
	n = (size > 1024) ? 1024 : size;
	if (r = xfread(X, buf, n))
	    return r;
	for (i = 0; i < n; i++) {
	    if (buf[i]) {
		if (cnulls > maxcnulls)
		    maxcnulls = cnulls;
		cnulls = 0;
	    } else {
		nulls++;
		cnulls++;
	    }
	}
	size -= n;
    }
    if (maxcnulls >= searchcount) {
	bad_count++;
	if (showpaths)
	    Path_Build(X, &phi, v->vnode, &name, 0);
	if (name) {
	    printf("*** BAD %d (%s) - %d nulls, %d consecutive\n", v->vnode,
		   name, nulls, maxcnulls);
	    free(name);
	} else {
	    printf("*** BAD %d - %d nulls, %d consecutive\n", v->vnode, nulls,
		   maxcnulls);
	}
    }
    return r;
}


int
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
    initialize_rx_error_table();
    r = xfopen(&input_file, O_RDONLY, input_path);
    if (r) {
	afs_com_err(argv0, r, "opening %s", input_path);
	exit(2);
    }

    memset(&dp, 0, sizeof(dp));
    dp.cb_error = my_error_cb;
    if (input_file.is_seekable)
	dp.flags |= DSFLAG_SEEK;
    if (showpaths) {
	u_int64 where;

	memset(&phi, 0, sizeof(phi));
	phi.p = &dp;

	if ((r = xftell(&input_file, &where))
	    || (r = Path_PreScan(&input_file, &phi, 0))
	    || (r = xfseek(&input_file, &where))) {
	    afs_com_err(argv0, r, "- path initialization failed");
	    xfclose(&input_file);
	    exit(2);
	}
    }

    dp.cb_vnode_file = my_file_cb;
    r = ParseDumpFile(&input_file, &dp);
    xfclose(&input_file);

    if (error_count)
	printf("*** %d errors\n", error_count);
    if (bad_count)
	printf("*** %d bad files\n", bad_count);
    if (r && !quiet)
	printf("*** FAILED: %s\n", afs_error_message(r));
}
