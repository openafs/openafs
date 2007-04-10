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

/* afsdump_extract.c - Extract files from an AFS dump */

#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "dumpscan.h"
#include "dumpscan_errs.h"

#define COPYBUFSIZE (256*1024)

extern int optind;
extern char *optarg;

char *argv0;
static char **file_names;
static int *file_vnums, name_count, vnum_count;

static char *input_path, *target;
static int quiet, verbose, error_count, dirs_done, extract_all;
static int nomode, use_realpath, use_vnum;
static int do_acls, do_headers;

static path_hashinfo phi;
static dump_parser dp;

/* Print a usage message and exit */
static void
usage(int status, char *msg)
{
    if (msg)
	fprintf(stderr, "%s: %s\n", argv0, msg);
    fprintf(stderr, "Usage: %s [options] dumpfile [dest [files...]]\n",
	    argv0);
    fprintf(stderr, "  -A     Save ACL's\n");
    fprintf(stderr, "  -H     Save headers\n");
    fprintf(stderr, "  -h     Print this help message\n");
    fprintf(stderr, "  -i     Use vnode numbers\n");
    fprintf(stderr, "  -n     Don't actually create files\n");
    fprintf(stderr, "  -p     Use real pathnames internally\n");
    fprintf(stderr, "  -q     Quiet mode (don't print errors)\n");
    fprintf(stderr, "  -v     Verbose mode\n");
    fprintf(stderr, "The destination directory defaults to .\n");
    fprintf(stderr, "Files may be vnode numbers or volume-relative paths;\n");
    fprintf(stderr, "If vnode numbers are used, files will be extracted\n");
    fprintf(stderr,
	    "a name generated from the vnode number and uniqifier.\n");
    fprintf(stderr, "If paths are used, -p is implied and files will be\n");
    fprintf(stderr, "into correctly-named files.\n");
    exit(status);
}


/* Parse the command-line options */
static void
parse_options(int argc, char **argv)
{
    int c, i, i_name, i_vnum;

    /* Set the program name */
    if (argv0 = strrchr(argv[0], '/'))
	argv0++;
    else
	argv0 = argv[0];

    /* Initialize options */
    input_path = 0;
    quiet = verbose = nomode = 0;
    use_realpath = use_vnum = do_acls = do_headers = extract_all = 0;

    /* Initialize other stuff */
    error_count = 0;

    /* Parse the options */
    while ((c = getopt(argc, argv, "AHhinpqv")) != EOF) {
	switch (c) {
	case 'A':
	    do_acls = 1;
	    continue;
	case 'H':
	    do_headers = 1;
	    continue;
	case 'i':
	    use_vnum = 1;
	    continue;
	case 'n':
	    nomode = 1;
	    continue;
	case 'p':
	    use_realpath = 1;
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
    if (argc - optind < 1)
	usage(1, "Dumpfile name required!");
    input_path = argv[optind];

    if (argc - optind < 2)
	target = ".";
    target = argv[optind + 1];

    vnum_count = name_count = 0;
    if (argc - optind < 3)
	extract_all = 1;
    else {
	argv += optind + 2;
	argc -= optind + 2;
	for (i = 0; i < argc; i++) {
	    if (argv[i][0] == '/')
		name_count++;
	    else
		vnum_count++;
	}
	file_names = (char **)malloc(name_count + sizeof(char *));
	file_vnums = (afs_uint32 *) malloc(vnum_count + sizeof(afs_uint32));
	if (name_count)
	    use_realpath = 1;

	i_name = i_vnum = 0;
	for (i = 0; i < argc; i++) {
	    if (argv[i][0] == '/')
		file_names[i_name++] = argv[i];
	    else
		file_vnums[i_vnum++] = strtol(argv[i], 0, 0);
	}
    }
}


static int
mkdirp(char *path)
{
    char *x = path, slash;
    struct stat statbuf;

    for (;;) {
	while (*x && *x != '/')
	    x++;
	slash = *x;
	*x = 0;

	if (stat(path, &statbuf)) {
	    if (errno == ENOENT) {
		if (verbose)
		    printf("> mkdir %s\n", path);
		if (!mkdir(path, 0755))
		    errno = 0;
	    }
	}
	if (!slash)
	    break;
	*x++ = '/';
	if (errno)
	    return errno;
    }

    return 0;
}


static char *
modestr(int mode)
{
    static char str[10];
    int i;

    strcpy(str, "rwxrwxrwx");
    for (i = 0; i < 9; i++) {
	if (!(mode & (1 << i)))
	    str[8 - i] = '-';
    }
    if (mode & 01000)
	str[8] = (str[8] == '-') ? 'T' : 't';
    if (mode & 02000)
	str[5] = (str[5] == '-') ? 'S' : 's';
    if (mode & 04000)
	str[2] = (str[2] == '-') ? 'S' : 's';
    return str;
}


static char *month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static char *
datestr(time_t date)
{
    static char str[13];
    time_t clock = time(0);
    struct tm *now, *then;
    int diff;

    now = localtime(&clock);
    then = localtime(&date);

    diff = now->tm_mon - then->tm_mon;
    if (then->tm_year == now->tm_year - 1)
	diff += 12;
    if (then->tm_year == now->tm_year + 1)
	diff -= 12;

    if (diff < 5 || diff > 5)
	sprintf(str, "%3s %2d  %4d", month[then->tm_mon], then->tm_mday,
		then->tm_year + 1900);
    else
	sprintf(str, "%3s %2d %2d:%2d", month[then->tm_mon], then->tm_mday,
		then->tm_hour, then->tm_min);
    return str;
}


/* Should we use this vnode?
 * Return 0 if no, non-0 if yes
 */
static int
usevnode(XFILE * X, afs_uint32 vnum, char *vnodepath)
{
    int vl, vpl, r, i;

    /* Special case */
    if (extract_all || !strcmp(vnodepath, "/"))
	return 1;

    for (i = 0; i < vnum_count; i++)
	if (vnum == file_vnums[i])
	    return 2;

    vl = strlen(vnodepath);
/*fprintf(stderr, "++ checking %s\n", vnodepath);*/
    for (i = 0; i < name_count; i++) {
	vpl = strlen(file_names[i]);
/*  fprintf(stderr, "   %s\n", file_names[i]);*/

	if (vl > vpl) {
	    r = !strncmp(file_names[i], vnodepath, vpl)
		&& vnodepath[vpl] == '/';
	} else if (vl < vpl) {
	    r = !strncmp(file_names[i], vnodepath, vl)
		&& file_names[i][vl] == '/';
	} else {
	    r = !strcmp(file_names[i], vnodepath);
	}
	if (r)
	    return 1;
    }
    return 0;
}


static int
copyfile(XFILE * in, XFILE * out, int size)
{
    static char buf[COPYBUFSIZE];
    int nr, nw, r;

    while (size) {
	nr = (size > COPYBUFSIZE) ? COPYBUFSIZE : size;
	if (r = xfread(in, buf, nr))
	    return r;
	if (r = xfwrite(out, buf, nr))
	    return r;
	size -= nr;
    }
    return 0;
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


static afs_uint32
dumphdr_cb(afs_dump_header * hdr, XFILE * X, void *refcon)
{
    return 0;
}


static afs_uint32
volhdr_cb(afs_vol_header * hdr, XFILE * X, void *refcon)
{
    return 0;
}


static afs_uint32
directory_cb(afs_vnode * v, XFILE * X, void *refcon)
{
    char *vnodepath;
    int r, use;

    /* Should we even use this? */
    if (!use_vnum) {
	if (r = Path_Build(X, &phi, v->vnode, &vnodepath, !use_realpath))
	    return r;
	if (!(use = usevnode(X, v->vnode, vnodepath))) {
	    free(vnodepath);
	    return 0;
	}
    }

    /* Print it out */
    if (verbose) {
	if (use_vnum)
	    printf("d%s %3d %-11d %11d %s #%d:%d\n", modestr(v->mode),
		   v->nlinks, v->owner, v->size, datestr(v->server_date),
		   v->vnode, v->vuniq);
	else
	    printf("d%s %3d %-11d %11d %s %s\n", modestr(v->mode), v->nlinks,
		   v->owner, v->size, datestr(v->server_date), vnodepath);
    } else if (!quiet && !use_vnum)
	printf("%s\n", vnodepath);

    /* Make the directory, if needed */
    if (!nomode && !use_vnum && use != 2) {
	if (strcmp(vnodepath, "/")
	    && (r = mkdirp(vnodepath + 1))) {
	    free(vnodepath);
	    return r;
	}
	if (do_acls) {
	    /* XXX do ACL's later */
	}
    }
    if (!use_vnum)
	free(vnodepath);
    return 0;
}


static afs_uint32
file_cb(afs_vnode * v, XFILE * X, void *refcon)
{
    char *vnodepath, vnpx[30];
    u_int64 where;
    XFILE OX;
    int r, use;

    if (!dirs_done) {
	dirs_done = 1;
	if (verbose)
	    printf("* Extracting files...\n");
    }

    /* Should we even use this? */
    if (!use_vnum) {
	if (r = Path_Build(X, &phi, v->vnode, &vnodepath, !use_realpath))
	    return r;
	if (!(use = usevnode(X, v->vnode, vnodepath))) {
	    free(vnodepath);
	    return 0;
	}
	if (use == 2) {
	    free(vnodepath);
	    sprintf(vnpx, "#%d:%d", v->vnode, v->vuniq);
	    vnodepath = vnpx;
	}
    } else {
	sprintf(vnpx, "#%d:%d", v->vnode, v->vuniq);
	vnodepath = vnpx;
    }

    /* Print it out */
    if (verbose) {
	printf("-%s %3d %-11d %11d %s %s\n", modestr(v->mode), v->nlinks,
	       v->owner, v->size, datestr(v->server_date), vnodepath);
    } else if (!quiet) {
	printf("%s\n", vnodepath);
    }

    if (!nomode) {
	if ((r = xftell(X, &where))
	    || (r = xfseek(X, &v->d_offset))
	    || (r =
		xfopen_path(&OX, O_RDWR | O_CREAT | O_TRUNC, vnodepath + 1,
			    0644))) {
	    if (!use_vnum)
		free(vnodepath);
	    return r;
	}
	r = copyfile(X, &OX, v->size);
	xfclose(&OX);
	xfseek(X, &where);
    } else
	r = 0;

    if (!use_vnum && use != 2)
	free(vnodepath);
    return r;
}


static afs_uint32
symlink_cb(afs_vnode * v, XFILE * X, void *refcon)
{
    char *vnodepath, *linktarget, vnpx[30];
    u_int64 where;
    int r, use;

    if (!dirs_done) {
	dirs_done = 1;
	if (verbose)
	    printf("* Extracting files...\n");
    }

    /* Should we even use this? */
    if (!use_vnum) {
	if (r = Path_Build(X, &phi, v->vnode, &vnodepath, !use_realpath))
	    return r;
	if (!(use = usevnode(X, v->vnode, vnodepath))) {
	    free(vnodepath);
	    return 0;
	}
	if (use == 2) {
	    free(vnodepath);
	    sprintf(vnpx, "#%d:%d", v->vnode, v->vuniq);
	    vnodepath = vnpx;
	}
    } else {
	sprintf(vnpx, "#%d:%d", v->vnode, v->vuniq);
	vnodepath = vnpx;
    }

    if (!(linktarget = (char *)malloc(v->size + 1))) {
	if (!use_vnum && use != 2)
	    free(vnodepath);
	return DSERR_MEM;
    }
    if ((r = xftell(X, &where))
	|| (r = xfseek(X, &v->d_offset))
	|| (r = xfread(X, linktarget, v->size))) {
	if (!use_vnum && use != 2)
	    free(vnodepath);
	free(linktarget);
	return r;
    }
    xfseek(X, &where);
    linktarget[v->size] = 0;

    /* Print it out */
    if (verbose)
	printf("l%s %3d %-11d %11d %s %s -> %s\n", modestr(v->mode),
	       v->nlinks, v->owner, v->size, datestr(v->server_date),
	       vnodepath, linktarget);
    else if (!quiet)
	printf("%s\n", vnodepath);

    r = 0;
    if (!nomode) {
	if (symlink(linktarget, vnodepath + 1))
	    r = errno;
    }

    free(linktarget);
    if (!use_vnum && use != 2)
	free(vnodepath);
    return r;
}


static afs_uint32
lose_cb(afs_vnode * v, XFILE * F, void *refcon)
{
    int r;

    if (!dirs_done) {
	dirs_done = 1;
	if (verbose)
	    printf("* Extracting files...\n");
    }

    return 0;
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
    if (input_file.is_seekable)
	dp.flags |= DSFLAG_SEEK;
    dirs_done = 0;

    if (!use_vnum) {
	u_int64 where;

	memset(&phi, 0, sizeof(phi));
	phi.p = &dp;

	if (verbose)
	    printf("* Building pathname info...\n");
	if ((r = xftell(&input_file, &where))
	    || (r = Path_PreScan(&input_file, &phi, 1))
	    || (r = xfseek(&input_file, &where))) {
	    afs_com_err(argv0, r, "- path initialization failed");
	    xfclose(&input_file);
	    exit(1);
	}
    }

    dp.cb_vnode_dir = directory_cb;
    dp.cb_vnode_file = file_cb;
    dp.cb_vnode_link = symlink_cb;
    dp.cb_vnode_empty = lose_cb;
    dp.cb_vnode_wierd = lose_cb;
    if (do_headers) {
	dp.cb_dumphdr = dumphdr_cb;
	dp.cb_volhdr = volhdr_cb;
    }

    if (!nomode) {
	mkdir(target, 0755);
	if (chdir(target)) {
	    fprintf(stderr, "chdir %s failed: %s\n", target, strerror(errno));
	    exit(1);
	}
    }
    r = ParseDumpFile(&input_file, &dp);

    if (verbose && error_count)
	fprintf(stderr, "*** %d errors\n", error_count);
    if (r && !quiet)
	fprintf(stderr, "*** FAILED: %s\n", afs_error_message(r));
    exit(0);
}
