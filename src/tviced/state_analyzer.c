/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * demand attach fs
 * fileserver state serialization
 *
 * state analyzer
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <errno.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/stds.h>
#include <rx/xdr.h>
#include <afs/assert.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afsint.h>
#include <afs/rxgen_consts.h>
#include <afs/nfs.h>
#include <afs/errors.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#ifdef AFS_ATHENA_STDENV
#include <krb.h>
#endif
#include <afs/acl.h>
#include <afs/ptclient.h>
#include <afs/prs_fs.h>
#include <afs/auth.h>
#include <afs/afsutil.h>
#include <rx/rx.h>
#include <afs/cellconfig.h>
#include <stdlib.h>
#include "../util/afsutil_prototypes.h"
#include "../viced/viced.h"
#include "../viced/host.h"
#include "../viced/callback.h"
#include "serialize_state.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

/*@+fcnmacros +macrofcndecl@*/
#ifdef O_LARGEFILE
#ifdef S_SPLINT_S
extern off64_t afs_lseek(int FD, off64_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)	lseek64(FD, (off64_t)(O), F)
#define afs_stat		stat64
#define afs_fstat		fstat64
#define afs_open		open64
#define afs_fopen		fopen64
#define afs_mmap                mmap64
#ifdef AFS_AIX_ENV
extern void * mmap64();  /* ugly hack since aix build env appears to be somewhat broken */
#endif
#else /* !O_LARGEFILE */
#ifdef S_SPLINT_S
extern off_t afs_lseek(int FD, off_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)	lseek(FD, (off_t)(O), F)
#define afs_stat		stat
#define afs_fstat		fstat
#define afs_open		open
#define afs_fopen		fopen
#define afs_mmap                mmap
#endif /* !O_LARGEFILE */
/*@=fcnmacros =macrofcndecl@*/


#ifndef AFS_DEMAND_ATTACH_FS
int
main (int argc, char ** argv)
{
    fprintf(stderr, "%s is only supported for demand attach fileservers\n",
	    argv[0] ? argv[0] : "state analyzer");
    return 1;
}
#else /* AFS_DEMAND_ATTACH_FS */

static void usage(char * prog);
static int openFile(char * path);
static void initState(void);

static void banner(void);
static void prompt(void);

static void print_help(void);
static void print_global_help(void);
static void print_h_help(void);
static void print_fe_help(void);
static void print_cb_help(void);

static void dump_hdr(void);
static void dump_h_hdr(void);
static void dump_cb_hdr(void);

static void dump_cb_timeout(void);
static void dump_cb_fehash(void);

static void dump_all_hes(void);
static void dump_all_fes(void);
static void dump_all_cbs(void);

static void dump_he(afs_uint32 idx);
static void dump_fe(afs_uint32 idx);
static void dump_cb(afs_uint32 idx);
static void dump_this_he(void);
static void dump_this_fe(void);
static void dump_this_cb(void);
static void dump_next_he(void);
static void dump_next_fe(void);
static void dump_next_cb(void);
static void dump_prev_he(void);
static void dump_prev_fe(void);
static void dump_prev_cb(void);
static void dump_first_he(void);
static void dump_first_fe(void);
static void dump_first_cb(void);
static void dump_last_he(void);
static void dump_last_fe(void);
static void dump_last_cb(void);
static void dump_he_hdr(void);
static void dump_he_entry(void);
static void dump_he_interfaces(void);
static void dump_he_hcps(void);
static void dump_fe_hdr(void);
static void dump_fe_entry(void);
static void dump_cb_entry(void);

static void hexdump_map(afs_uint32 offset, afs_uint32 len);

static int get_hdr(void);
static int get_h_hdr(void);
static int get_cb_hdr(void);
static int get_cb_timeout_hdr(void);
static int get_cb_timeout(void);
static int get_cb_fehash_hdr(void);
static int get_cb_fehash(void);
static int get_he(afs_uint32 idx);
static int get_he_hdr(void);
static int get_he_entry(void);
static int get_fe(afs_uint32 idx);
static int get_fe_hdr(void);
static int get_fe_entry(void);
static int get_cb(afs_uint32 idx);
static int get_cb_entry(void);

static int find_fe_by_index(afs_uint32 idx);
static int find_cb_by_index(afs_uint32 idx);
static int find_fe_by_fid(afs_uint32 vol, afs_uint32 vn, afs_uint32 uniq);


static int dump_fd = -1;
static void * map = NULL;
static size_t map_len;

static struct {
    struct fs_state_header hdr;
    struct host_state_header h_hdr;
    struct callback_state_header cb_hdr;
    struct callback_state_timeout_header timeout_hdr;
    struct callback_state_fehash_header fehash_hdr;
    afs_uint32 * timeout;
    afs_uint32 * fehash;

    /* pointers into the memory map */
    void * hdr_p;
    void * h_hdr_p;
    void * cb_hdr_p;
    void * timeout_hdr_p;
    void * timeout_p;
    void * fehash_hdr_p;
    void * fehash_p;

    byte hdr_valid;
    byte h_hdr_valid;
    byte cb_hdr_valid;
    byte timeout_hdr_valid;
    byte fehash_hdr_valid;
} hdrs;

static struct {
    void * fh;
    void * cursor;
    void * ifp;
    void * hcps;
    struct host_state_entry_header hdr;
    struct hostDiskEntry he;
    afs_uint32 idx;
    byte hdr_valid;
    byte he_valid;
} he_cursor;

static struct {
    void ** cursor;
} he_cache;

static struct {
    void * ffe;
    void * cursor;
    void * fcb;
    struct callback_state_entry_header hdr;
    struct FEDiskEntry fe;
    afs_uint32 idx;
    byte hdr_valid;
    byte fe_valid;
} fe_cursor;

static struct {
    void ** cursor;
} fe_cache;

static struct {
    void * cursor;
    struct CBDiskEntry cb;
    afs_uint32 idx;
    byte cb_valid;
} cb_cursor;

static struct {
    void ** cursor;
} cb_cache;

static void
usage(char * prog)
{
    fprintf(stderr, "usage: %s [<state dump file>]\n");
}

int
main(int argc, char ** argv)
{
    banner();

    if (argc > 2 || (argc == 2 && !strcmp(argv[1], "-h"))) {
	usage(argv[0]);
	return 1;
    }

    initState();

    if (argc > 1) {
	if (openFile(argv[1]))
	    return 1;
    } else {
	if (openFile(AFSDIR_SERVER_FSSTATE_FILEPATH))
	    return 1;
    }

    prompt();
    return 0;
}


static int
openFile(char * path)
{
    int ret = 0;
    struct afs_stat status;
    
    dump_fd = afs_open(path, O_RDWR);
    if (dump_fd == -1) {
	fprintf(stderr, "dump file '%s' failed to open\n", path);
	ret = 1;
	goto done;
    }

    printf("opened dump file '%s'\n", path);

    if (afs_fstat(dump_fd, &status) == -1) {
	fprintf(stderr, "failed to stat file\n");
	ret = 1;
	goto done;
    }

    map_len = status.st_size;

    map = afs_mmap(NULL, map_len, PROT_READ, MAP_SHARED, dump_fd, 0);
    if (map == MAP_FAILED) {
	fprintf(stderr, "failed to mmap file\n");
	ret = 1;
	goto done;
    }

    printf("mapped %d bytes at 0x%x\n", map_len, map);

 done:
    if (ret) {
	if (map) {
	    munmap(map, map_len);
	    map = NULL;
	}
	if (dump_fd != -1) {
	    close(dump_fd);
	    dump_fd = -1;
	}
    }
    return ret;
}

static void
initState(void)
{
    hdrs.hdr_valid = hdrs.h_hdr_valid = hdrs.cb_hdr_valid = 0;
    he_cursor.cursor = fe_cursor.cursor = cb_cursor.cursor = NULL;
    he_cursor.fh = fe_cursor.ffe = fe_cursor.fcb = NULL;
    he_cache.cursor = fe_cache.cursor = NULL;
}

static void
banner(void)
{
    fprintf(stderr, "demand attach fs\n");
    fprintf(stderr, "fileserver state analyzer\n");
    fprintf(stderr, "version 0.1\n");
}

#define PROGNAME "fs state analyzer"

static void
prompt(void)
{
    char input[256];
    char prev_input[256];
    char * tok = NULL;
    afs_uint32 x, y, z;
    enum {
	PR_GLOBAL_MODE,
	PR_H_MODE,
	PR_FE_MODE,
	PR_CB_MODE
    } mode = PR_GLOBAL_MODE, next_mode;

    next_mode = mode;
    input[0] = prev_input[0] = '\0';

    while (1) {
	if (!tok) {
	    switch(mode) {
	    case PR_GLOBAL_MODE:
		printf(PROGNAME "> ");
		break;
	    case PR_H_MODE:
		printf(PROGNAME ": h(%d)> ", he_cursor.idx);
		break;
	    case PR_FE_MODE:
		printf(PROGNAME ": fe(%d)> ", fe_cursor.idx);
		break;
	    case PR_CB_MODE:
		printf(PROGNAME ": fe(%d):cb(%d)> ", fe_cursor.idx, cb_cursor.idx);
		break;
	    default:
		fprintf(stderr, "prompt state broken; aborting\n");
		return;
	    }
	    gets(input);

	    if (!strcmp(input, "")) {
		/* repeat last command */
		if (!strcmp(prev_input, "")) {
		    continue;
		}
		strlcpy(input, prev_input, sizeof(input));
	    } else {
		/* save command for repetition */
		strlcpy(prev_input, input, sizeof(prev_input));
	    }

	    tok = strtok(input, " \t");
	}
	while (tok && !strcmp(tok, ";")) {
	    tok = strtok(NULL, "; \t");
	}

	if (!tok) {
	    continue;
	}

	if (!strcasecmp(tok, "exit")) {
	    return;
	} else if (!strcasecmp(tok, "quit")) {
	    switch(mode) {
	    case PR_CB_MODE:
		next_mode = PR_FE_MODE;
		break;
	    case PR_FE_MODE:
	    case PR_H_MODE:
		next_mode = PR_GLOBAL_MODE;
		break;
	    default:
		return;
	    }
	} else if (!strcasecmp(tok, "h")) {
	    tok = strtok(NULL, " \t");
	    mode = PR_H_MODE;
	    if (!tok) {
		next_mode = mode;
	    }
	    continue;
	} else if (!strcasecmp(tok, "fe")) {
	    tok = strtok(NULL, " \t");
	    mode = PR_FE_MODE;
	    if (!tok) {
		next_mode = mode;
	    }
	    continue;
	} else if (!strcasecmp(tok, "fs")) {
	    tok = strtok(NULL, " \t");
	    mode = PR_GLOBAL_MODE;
	    if (!tok) {
		next_mode = mode;
	    }
	    continue;
	} else if (!strcasecmp(tok, "cb")) {
	    tok = strtok(NULL, " \t");
	    mode = PR_CB_MODE;
	    if (!tok) {
		next_mode = mode;
	    }
	    continue;
	} else if (!strcasecmp(tok, "help")) {
	    switch(mode) {
	    case PR_H_MODE:
		print_h_help();
		break;
	    case PR_FE_MODE:
		print_fe_help();
		break;
	    case PR_CB_MODE:
		print_cb_help();
		break;
	    default:
		print_global_help();
	    }
	    print_help();
	} else if (!strcasecmp(tok, "hexdump")) {
	    tok = strtok(NULL, " \t");
	    if (!tok) {
		hexdump_map(0, map_len);
		continue;
	    }
	    if (sscanf(tok, "%u", &x) != 1) {
		fprintf(stderr, "hexdump parse error 1\n");
		tok = NULL;
		continue;
	    }
	    tok = strtok(NULL, " \t");
	    if (!tok) {
		hexdump_map(x, map_len - x);
		continue;
	    }
	    if (sscanf(tok, "%u", &y) != 1) {
		fprintf(stderr, "hexdump parse error 2\n");
		continue;
	    }
	    hexdump_map(x,y);
	} else if (!strcasecmp(tok, "hdr")) {
	    switch(mode) {
	    case PR_H_MODE:
		dump_h_hdr();
		break;
	    case PR_FE_MODE:
		dump_cb_hdr();
		break;
	    case PR_CB_MODE:
		dump_this_fe();
		break;
	    default:
		dump_hdr();
	    }
	} else if (!strcasecmp(tok, "this")) {
	    switch(mode) {
	    case PR_H_MODE:
		dump_this_he();
		break;
	    case PR_FE_MODE:
		dump_this_fe();
		break;
	    case PR_CB_MODE:
		dump_this_cb();
		break;
	    default:
		fprintf(stderr, "command not valid for this mode\n");
	    }
	} else if (!strcasecmp(tok, "next")) {
	    switch(mode) {
	    case PR_H_MODE:
		dump_next_he();
		break;
	    case PR_FE_MODE:
		dump_next_fe();
		break;
	    case PR_CB_MODE:
		dump_next_cb();
		break;
	    default:
		fprintf(stderr, "command not valid for this mode\n");
	    }
	} else if (!strcasecmp(tok, "prev")) {
	    switch(mode) {
	    case PR_H_MODE:
		dump_prev_he();
		break;
	    case PR_FE_MODE:
		dump_prev_fe();
		break;
	    case PR_CB_MODE:
		dump_prev_cb();
		break;
	    default:
		fprintf(stderr, "command not valid for this mode\n");
	    }
	} else if (!strcasecmp(tok, "first")) {
	    switch(mode) {
	    case PR_H_MODE:
		dump_first_he();
		break;
	    case PR_FE_MODE:
		dump_first_fe();
		break;
	    case PR_CB_MODE:
		dump_first_cb();
		break;
	    default:
		fprintf(stderr, "command not valid for this mode\n");
	    }
	} else if (!strcasecmp(tok, "last")) {
	    switch(mode) {
	    case PR_H_MODE:
		dump_last_he();
		break;
	    case PR_FE_MODE:
		dump_last_fe();
		break;
	    case PR_CB_MODE:
		dump_last_cb();
		break;
	    default:
		fprintf(stderr, "command not valid for this mode\n");
	    }
	} else if (!strcasecmp(tok, "dump")) {
	    switch(mode) {
	    case PR_H_MODE:
		dump_all_hes();
		break;
	    case PR_FE_MODE:
		dump_all_fes();
		break;
	    case PR_CB_MODE:
		dump_all_cbs();
		break;
	    default:
		fprintf(stderr, "command not valid for this mode\n");
	    }
	} else if (!strcasecmp(tok, "find")) {
	    tok = strtok(NULL, " \t");
	    if (!tok || strcasecmp(tok, "by")) {
		tok = NULL;
		fprintf(stderr, "find syntax error 1 (%s)\n", 
			(tok) ? tok : "nil");
		continue;
	    }
	    tok = strtok(NULL, " \t");
	    if (!tok) {
		fprintf(stderr, "find syntax error 2\n");
		continue;
	    }
	    switch(mode) {
	    case PR_H_MODE:
		fprintf(stderr, "not implemented yet\n");
		break;
	    case PR_FE_MODE:
		if (!strcasecmp(tok, "index")) {
		    tok = strtok(NULL, " \t");
		    if (!tok || sscanf(tok, "%u", &x) != 1) {
			tok = NULL;
			fprintf(stderr, "find syntax error 3\n");
			continue;
		    }
		    if (find_fe_by_index(x)) {
			fprintf(stderr, "find returned no results\n");
		    }
		} else if (!strcasecmp(tok, "fid")) {
		    tok = strtok(NULL, "(), \t");
		    if (!tok || sscanf(tok, "%u", &x) != 1) {
			tok = NULL;
			fprintf(stderr, "find syntax error 4\n");
			continue;
		    }
		    tok = strtok(NULL, "(), \t");
		    if (!tok || sscanf(tok, "%u", &y) != 1) {
			tok = NULL;
			fprintf(stderr, "find syntax error 5\n");
			continue;
		    }
		    tok = strtok(NULL, "(), \t");
		    if (!tok || sscanf(tok, "%u", &z) != 1) {
			tok = NULL;
			fprintf(stderr, "find syntax error 6\n");
			continue;
		    }
		    if (find_fe_by_fid(x,y,z)) {
			fprintf(stderr, "find returned no results\n");
		    }
		} else {
		    fprintf(stderr, "unsupported filter type\n");
		}
		break;
	    case PR_CB_MODE:
		if (!strcasecmp(tok, "index")) {
		    tok = strtok(NULL, " \t");
		    if (!tok || sscanf(tok, "%u", &x) != 1) {
			tok = NULL;
			fprintf(stderr, "find syntax error 3\n");
			continue;
		    }
		    if (find_cb_by_index(x)) {
			fprintf(stderr, "find returned no results\n");
		    }
		} else {
		    fprintf(stderr, "unsupported filter type\n");
		}
		break;
	    default:
		fprintf(stderr, "find not supported for this menu\n");
	    }
	} else if (!strcspn(tok, "0123456789")) {
	    if (sscanf(tok, "%u", &x) == 1) {
		switch(mode) {
		case PR_H_MODE:
		    dump_he(x);
		    break;
		case PR_FE_MODE:
		    dump_fe(x);
		    break;
		case PR_CB_MODE:
		    dump_cb(x);
		    break;
		default:
		    fprintf(stderr, "command not available from this menu\n");
		}
	    } else {
		fprintf(stderr, "input parse error ('%s')\n", tok);
	    }
	} else if (mode == PR_FE_MODE) {
	    if (!strcmp(tok, "timeout")) {
		dump_cb_timeout();
	    } else if (!strcmp(tok, "hash")) {
		dump_cb_fehash();
	    }
	} else {
	    fprintf(stderr, "unknown command\n");
	}
	tok = strtok(NULL, " \t");
	mode = next_mode;
    }
}

static void
print_help(void)
{
    printf("\th <...>  -- host menu commands\n");
    printf("\tfe <...> -- FileEntry menu commands\n");
    printf("\tcb <...> -- CallBack menu commands\n");
    printf("\thexdump [<offset> [<len>]]\n\t\t -- hex dump the raw data\n");
    printf("\tquit     -- quit this menu\n");
    printf("\texit     -- exit the debugger\n");
    printf("\thelp     -- this help message\n");
}

static void
print_global_help(void)
{
    printf("\thdr      -- display the fs_state_header struct\n");
}

static void
print_h_help(void)
{
    printf("\thdr      -- display the host_state_header struct\n");
    printf("\tfirst    -- display the first host\n");
    printf("\tprev     -- display the previous host\n");
    printf("\tthis     -- display this host\n");
    printf("\tnext     -- display the next host\n");
    printf("\tlast     -- display the last host\n");
    printf("\tdump     -- display all hosts\n");
}

static void
print_fe_help(void)
{
    printf("\thdr      -- display the callback_state_header struct\n");
    printf("\tfirst    -- display the first FE\n");
    printf("\tprev     -- display the previous FE\n");
    printf("\tthis     -- display this FE\n");
    printf("\tnext     -- display the next FE\n");
    printf("\tlast     -- display the last FE\n");
    printf("\tdump     -- display all FEs\n");
    printf("\ttimeout  -- display the timeout queue heads\n");
    printf("\thash   -- display the file entry hash buckets\n");
    printf("\tfind by index <id>\n\t\t -- find an fe by its array index\n");
    printf("\tfind by fid <(vol,vnode,unique)>\n\t\t -- find an fe by its AFSFid\n");
}

static void
print_cb_help(void)
{
    printf("\thdr      -- display the callback_state_entry_header struct\n");
    printf("\tfirst    -- display the first CB\n");
    printf("\tprev     -- display the previous CB\n");
    printf("\tthis     -- display this CB\n");
    printf("\tnext     -- display the next CB\n");
    printf("\tlast     -- display the last CB\n");
    printf("\tdump     -- display all CBs\n");
}

#define DPFTB0 "\t"
#define DPFTB1 "\t\t"
#define DPFTB2 "\t\t\t"

#define DPFOFF(addr) \
    do { \
        char * _p = (char *)addr; \
        char * _m = (char *)map; \
        printf("loading structure from address 0x%x (offset %u)\n", \
               addr, _p-_m); \
    } while (0)

/* structs */
#define DPFSO(T, name) printf(T "%s = {\n", name)
#define DPFSO0(name) DPFSO(DPFTB0, name)
#define DPFSO1(name) DPFSO(DPFTB1, name)
#define DPFSC(T) printf(T "}\n")
#define DPFSC0 DPFSC(DPFTB0)
#define DPFSC1 DPFSC(DPFTB1)

/* arrays */
#define DPFAO(T1, T2, name) printf(T1 "%s =\n" T2 "{ ", name)
#define DPFAO0(name) DPFAO(DPFTB0, DPFTB1, name)
#define DPFAO1(name) DPFAO(DPFTB1, DPFTB2, name)
#define DPFAC0 printf(" }\n")
#define DPFAC1 DPFAC0
#define DPFA1 printf(DPFTB1 "  ")
#define DPFA2 printf(DPFTB2 "  ")
#define DPFAN printf("\n")
#define DPFALE(type, var) printf("%" type, var)
#define DPFAE(type, var) printf("%" type ",\t", var)

/* normal vars */
#define DPFV(T, name, type, var) printf(T "%s = %" type "\n", name, var)
#define DPFV1(name, type, var) DPFV(DPFTB1, name, type, var)
#define DPFV2(name, type, var) DPFV(DPFTB2, name, type, var)

/* hex */
#define DPFX(T, name, var) printf(T "%s = 0x%x\n", name, var)
#define DPFX1(name, var) DPFX(DPFTB1, name, var)
#define DPFX2(name, var) DPFX(DPFTB2, name, var)

/* strings */
#define DPFS(T, name, var) printf(T "%s = \"%s\"\n", name, var)
#define DPFS1(name, var) DPFS(DPFTB1, name, var)
#define DPFS2(name, var) DPFS(DPFTB2, name, var)

/* time */
#define DPFT(T, name, var) \
    do { \
        char * last; \
        printf(T "%s = \"%s\"\n", name, strtok_r(ctime(&(var)), "\r\n", &last)); \
    } while(0)
#define DPFT1(name, var) DPFT(DPFTB1, name, var)
#define DPFT2(name, var) DPFT(DPFTB2, name, var)

static void
dump_hdr(void)
{
    char uuid_str[40];
    afs_uint32 hi, lo;

    if (get_hdr())
	return;

    DPFOFF(map);
    DPFSO0("fs_state_header");
    DPFSO1("stamp");
    DPFX2("magic", hdrs.hdr.stamp.magic);
    DPFV2("version", "u", hdrs.hdr.stamp.version);
    DPFSC1;
    DPFT1("timestamp", hdrs.hdr.timestamp);
    DPFV1("sys_name", "u", hdrs.hdr.sys_name);

    afsUUID_to_string(&hdrs.hdr.server_uuid, uuid_str, sizeof(uuid_str));
    DPFS1("server_uuid", uuid_str);
    DPFV1("valid", "d", hdrs.hdr.valid);
    DPFV1("endianness", "d", hdrs.hdr.endianness);
    DPFV1("stats_detailed", "d", hdrs.hdr.stats_detailed);

    SplitInt64(hdrs.hdr.h_offset, hi, lo);
    DPFSO1("h_offset");
    DPFV2("hi", "u", hi);
    DPFV2("lo", "u", lo);
    DPFSC1;

    SplitInt64(hdrs.hdr.cb_offset, hi, lo);
    DPFSO1("cb_offset");
    DPFV2("hi", "u", hi);
    DPFV2("lo", "u", lo);
    DPFSC1;

    DPFS1("server_version_string", hdrs.hdr.server_version_string);
    DPFSC0;

    if (hdrs.hdr.stamp.magic != FS_STATE_MAGIC) {
	fprintf(stderr, "* magic check failed\n");
    }
    if (hdrs.hdr.stamp.version != FS_STATE_VERSION) {
	fprintf(stderr, "* version check failed\n");
    }
}

static void
dump_h_hdr(void)
{
    if (get_h_hdr())
	return;

    DPFOFF(hdrs.h_hdr_p);
    DPFSO0("host_state_header");
    DPFSO1("stamp");
    DPFX2("magic", hdrs.h_hdr.stamp.magic);
    DPFV2("version", "u", hdrs.h_hdr.stamp.version);
    DPFSC1;
    DPFV1("records", "u", hdrs.h_hdr.records);
    DPFV1("index_max", "u", hdrs.h_hdr.index_max);
    DPFSC0;

    if (hdrs.h_hdr.stamp.magic != HOST_STATE_MAGIC) {
	fprintf(stderr, "* magic check failed\n");
    }
    if (hdrs.h_hdr.stamp.version != HOST_STATE_VERSION) {
	fprintf(stderr, "* version check failed\n");
    }
}

static void
dump_cb_hdr(void)
{
    afs_uint32 hi, lo;

    if (get_cb_hdr())
	return;

    DPFOFF(hdrs.cb_hdr_p);
    DPFSO0("callback_state_header");
    DPFSO1("stamp");
    DPFX2("magic", hdrs.cb_hdr.stamp.magic);
    DPFV2("version", "u", hdrs.cb_hdr.stamp.version);
    DPFSC1;
    DPFV1("nFEs", "u", hdrs.cb_hdr.nFEs);
    DPFV1("nCBs", "u", hdrs.cb_hdr.nCBs);
    DPFV1("fe_max", "u", hdrs.cb_hdr.fe_max);
    DPFV1("cb_max", "u", hdrs.cb_hdr.cb_max);
    DPFV1("tfirst", "d", hdrs.cb_hdr.tfirst);

    SplitInt64(hdrs.cb_hdr.timeout_offset, hi, lo);
    DPFSO1("timeout_offset");
    DPFV2("hi", "u", hi);
    DPFV2("lo", "u", lo);
    DPFSC1;

    SplitInt64(hdrs.cb_hdr.fehash_offset, hi, lo);
    DPFSO1("fehash_offset");
    DPFV2("hi", "u", hi);
    DPFV2("lo", "u", lo);
    DPFSC1;

    SplitInt64(hdrs.cb_hdr.fe_offset, hi, lo);
    DPFSO1("fe_offset");
    DPFV2("hi", "u", hi);
    DPFV2("lo", "u", lo);
    DPFSC1;

    DPFSC0;

    if (hdrs.cb_hdr.stamp.magic != CALLBACK_STATE_MAGIC) {
	fprintf(stderr, "* magic check failed\n");
    }
    if (hdrs.cb_hdr.stamp.version != CALLBACK_STATE_VERSION) {
	fprintf(stderr, "* version check failed\n");
    }
}

static void
dump_cb_timeout(void)
{
    int i;

    if (get_cb_hdr())
	return;

    if (get_cb_timeout_hdr())
	return;

    if (get_cb_timeout())
	return;

    DPFOFF(hdrs.timeout_hdr_p);
    DPFSO0("callback_state_timeout_header");
    DPFX1("magic", hdrs.timeout_hdr.magic);
    DPFV1("len", "u", hdrs.timeout_hdr.len);
    DPFV1("records", "u", hdrs.timeout_hdr.records);
    DPFSC0;

    if (hdrs.timeout_hdr.magic != CALLBACK_STATE_TIMEOUT_MAGIC) {
	fprintf(stderr, "* magic check failed\n");
    }

    DPFOFF(hdrs.timeout_p);
    DPFAO0("timeout");
    for (i = 0; i < 127; i++) {
	DPFAE("u", hdrs.timeout[i]);
	if ((i % 8) == 7) {
	    DPFAN;
	    DPFA1;
	}
    }
    DPFALE("u", hdrs.timeout[127]);
    DPFAC0;
}

static void
dump_cb_fehash(void)
{
    int i;

    if (get_cb_hdr())
	return;

    if (get_cb_fehash_hdr())
	return;

    if (get_cb_fehash())
	return;

    DPFOFF(hdrs.fehash_hdr_p);
    DPFSO0("callback_state_fehash_header");
    DPFX1("magic", hdrs.fehash_hdr.magic);
    DPFV1("len", "u", hdrs.fehash_hdr.len);
    DPFV1("records", "u", hdrs.fehash_hdr.records);
    DPFSC0;

    if (hdrs.fehash_hdr.magic != CALLBACK_STATE_FEHASH_MAGIC) {
	fprintf(stderr, "* magic check failed\n");
    }

    DPFOFF(hdrs.fehash_p);
    DPFAO0("fehash");
    for (i = 0; i < hdrs.fehash_hdr.records - 1; i++) {
	DPFAE("u", hdrs.fehash[i]);
	if ((i % 8) == 7) {
	    DPFAN;
	    DPFA1;
	}
    }
    DPFALE("u", hdrs.fehash[hdrs.fehash_hdr.records-1]);
    DPFAC0;
}

static void
dump_all_hes(void)
{
    int i;

    if (get_h_hdr()) {
	fprintf(stderr, "error getting host_state_header\n");
	return;
    }

    for (i = 0; i < hdrs.h_hdr.records; i++) {
	dump_he(i);
    }
}

static void
dump_all_fes(void)
{
    int i;

    if (get_cb_hdr()) {
	fprintf(stderr, "error getting callback_state_header\n");
	return;
    }

    for (i = 0; i < hdrs.cb_hdr.nFEs; i++) {
	dump_fe(i);
    }
}

static void
dump_all_cbs(void)
{
    int i;

    if (get_fe_hdr()) {
	fprintf(stderr, "error getting callback_state_entry_header\n");
	return;
    }

    for (i = 0; i < fe_cursor.hdr.nCBs; i++) {
	dump_cb(i);
    }
}

static void
dump_he(afs_uint32 idx)
{
    if (get_he(idx)) {
	fprintf(stderr, "error getting he %d\n", idx);
	return;
    }

    DPFOFF(he_cursor.cursor);
    dump_he_hdr();
    dump_he_entry();
    dump_he_interfaces();
    dump_he_hcps();
}

static void
dump_fe(afs_uint32 idx)
{
    if (get_fe(idx)) {
	fprintf(stderr, "error getting fe %d\n", idx);
	return;
    }

    DPFOFF(fe_cursor.cursor);
    dump_fe_hdr();
    dump_fe_entry();
}

static void
dump_cb(afs_uint32 idx)
{
    if (get_cb(idx)) {
	fprintf(stderr, "error getting cb %d\n", idx);
	return;
    }

    DPFOFF(cb_cursor.cursor);
    dump_cb_entry();
}

static void
dump_this_he(void)
{
    dump_he(he_cursor.idx);
}

static void
dump_this_fe(void)
{
    dump_fe(fe_cursor.idx);
}

static void
dump_this_cb(void)
{
    dump_cb(cb_cursor.idx);
}

static void
dump_next_he(void)
{
    if (get_h_hdr()) {
	fprintf(stderr, "error getting host_state_header\n");
	return;
    }

    if ((he_cursor.idx + 1) >= hdrs.h_hdr.records) {
	fprintf(stderr, "no more HEs\n");
	return;
    }
    
    dump_he(he_cursor.idx+1);
}

static void
dump_next_fe(void)
{
    if (get_cb_hdr()) {
	fprintf(stderr, "error getting callback_state_header\n");
	return;
    }

    if ((fe_cursor.idx + 1) >= hdrs.cb_hdr.nFEs) {
	fprintf(stderr, "no more FEs\n");
	return;
    }
    
    dump_fe(fe_cursor.idx+1);
}

static void
dump_next_cb(void)
{
    if (get_fe_hdr()) {
	fprintf(stderr, "error getting callback_state_entry_header\n");
	return;
    }

    if ((cb_cursor.idx + 1) >= fe_cursor.hdr.nCBs) {
	fprintf(stderr, "no more CBs\n");
	return;
    }
    
    dump_cb(cb_cursor.idx+1);
}

static void
dump_prev_he(void)
{
    if (!he_cursor.idx) {
	fprintf(stderr, "no more HEs\n");
	return;
    }
    
    dump_he(he_cursor.idx-1);
}

static void
dump_prev_fe(void)
{
    if (!fe_cursor.idx) {
	fprintf(stderr, "no more FEs\n");
	return;
    }
    
    dump_fe(fe_cursor.idx-1);
}

static void
dump_prev_cb(void)
{
    if (!cb_cursor.idx) {
	fprintf(stderr, "no more CBs\n");
	return;
    }
    
    dump_cb(cb_cursor.idx-1);
}

static void
dump_first_fe(void)
{
    if (get_cb_hdr()) {
	fprintf(stderr, "error getting callback_state_header\n");
	return;
    }

    if (!hdrs.cb_hdr.nFEs) {
	fprintf(stderr, "no FEs present\n");
	return;
    }
    
    dump_fe(0);
}

static void
dump_first_he(void)
{
    if (get_h_hdr()) {
	fprintf(stderr, "error getting host_state_header\n");
	return;
    }

    if (!hdrs.h_hdr.records) {
	fprintf(stderr, "no HEs present\n");
	return;
    }
    
    dump_he(0);
}

static void
dump_first_cb(void)
{
    if (get_fe_hdr()) {
	fprintf(stderr, "error getting callback_state_entry_header\n");
	return;
    }

    if (!fe_cursor.hdr.nCBs) {
	fprintf(stderr, "no CBs present\n");
	return;
    }
    
    dump_cb(0);
}

static void
dump_last_he(void)
{
    if (get_h_hdr()) {
	fprintf(stderr, "error getting host_state_header\n");
	return;
    }

    if (!hdrs.h_hdr.records) {
	fprintf(stderr, "no HEs present\n");
	return;
    }
    
    dump_he(hdrs.h_hdr.records-1);
}

static void
dump_last_fe(void)
{
    if (get_cb_hdr()) {
	fprintf(stderr, "error getting callback_state_header\n");
	return;
    }

    if (!hdrs.cb_hdr.nFEs) {
	fprintf(stderr, "no FEs present\n");
	return;
    }
    
    dump_fe(hdrs.cb_hdr.nFEs-1);
}

static void
dump_last_cb(void)
{
    if (get_fe_hdr()) {
	fprintf(stderr, "error getting callback_state_entry_header\n");
	return;
    }

    if (!fe_cursor.hdr.nCBs) {
	fprintf(stderr, "no CBs present\n");
	return;
    }

    dump_cb(fe_cursor.hdr.nCBs-1);
}

static void
dump_he_hdr(void)
{
    DPFSO0("host_state_entry_header");
    DPFX1("magic", he_cursor.hdr.magic);
    DPFV1("len", "u", he_cursor.hdr.len);
    DPFV1("interfaces", "u", he_cursor.hdr.interfaces);
    DPFV1("hcps", "u", he_cursor.hdr.hcps);
    DPFSC0;

    if (he_cursor.hdr.magic != HOST_STATE_ENTRY_MAGIC) {
	fprintf(stderr, "* magic check failed\n");
    }
}

static void
dump_he_entry(void)
{
    DPFSO0("hostDiskEntry");
    DPFS1("host", afs_inet_ntoa(he_cursor.he.host));
    DPFV1("port", "u", he_cursor.he.port);
    DPFX1("hostFlags", he_cursor.he.hostFlags);
    DPFV1("Console", "u", he_cursor.he.Console);
    DPFV1("hcpsfailed", "u", he_cursor.he.hcpsfailed);
    DPFV1("hcps_valid", "u", he_cursor.he.hcps_valid);
    if (hdrs.hdr.stats_detailed) {
#ifdef FS_STATS_DETAILED
	DPFV1("InSameNetwork", "u", he_cursor.he.InSameNetwork);
#else
	DPFV1("InSameNetwork", "u", he_cursor.he.padding1[0]);
#endif
    }
    DPFV1("hcps_len", "u", he_cursor.he.hcps_len);
    DPFT1("LastCall", he_cursor.he.LastCall);
    DPFT1("ActiveCall", he_cursor.he.ActiveCall);
    DPFT1("cpsCall", he_cursor.he.cpsCall);
    DPFV1("cblist", "u", he_cursor.he.cblist);
    DPFV1("index", "u", he_cursor.he.index);
    DPFSC0;
}

static void
dump_he_interfaces(void)
{
    char temp_str[40];
    struct Interface * ifp;
    int len, i;

    if (!he_cursor.hdr.interfaces)
	return;

    len = sizeof(struct Interface) + ((he_cursor.hdr.interfaces-1)*sizeof(struct AddrPort));
    ifp = (struct Interface *) malloc(len);
    assert(ifp != NULL);

    memcpy(ifp, he_cursor.ifp, len);

    DPFSO0("Interface");
    DPFV1("numberOfInterfaces", "u", ifp->numberOfInterfaces);

    afsUUID_to_string(&ifp->uuid, temp_str, sizeof(temp_str));
    DPFS1("uuid", temp_str);
    for (i = 0; i < he_cursor.hdr.interfaces; i++) {
	snprintf(temp_str, sizeof(temp_str), "interface[%d]", i);
	DPFSO1(temp_str);
	DPFS2("addr", afs_inet_ntoa(ifp->interface[i].addr));
	DPFV2("port", "u", ifp->interface[i].port);
	DPFSC1;
    }

    DPFSC0;

    if (he_cursor.hdr.interfaces != ifp->numberOfInterfaces) {
	fprintf(stderr, "* interface count mismatch between header and Interface struct\n");
    }
    free(ifp);
}

static void
dump_he_hcps(void)
{
    char temp_str[40];
    afs_int32 * hcps;
    int len, i;

    if (!he_cursor.hdr.hcps)
	return;

    len = (he_cursor.hdr.hcps)*sizeof(afs_uint32);
    hcps = (afs_int32 *) malloc(len);
    assert(hcps != NULL);
    memcpy(hcps, he_cursor.hcps, len);

    DPFSO0("hcps");
    DPFAO1("prlist_val");
    for (i = 0; i < he_cursor.hdr.hcps - 1; i++) {
	DPFAE("d", hcps[i]);
	if ((i % 8) == 7) {
	    DPFAN;
	    DPFA2;
	}
    }
    DPFALE("d", hcps[he_cursor.hdr.hcps-1]);
    DPFAC1;
    DPFSC0;
    free(hcps);
}

static void
dump_fe_hdr(void)
{
    DPFSO0("callback_state_entry_header");
    DPFX1("magic", fe_cursor.hdr.magic);
    DPFV1("len", "u", fe_cursor.hdr.len);
    DPFV1("nCBs", "u", fe_cursor.hdr.nCBs);
    DPFSC0;

    if (fe_cursor.hdr.magic != CALLBACK_STATE_ENTRY_MAGIC) {
	fprintf(stderr, "* magic check failed\n");
    }
}

static void
dump_fe_entry(void)
{
    DPFSO0("FEDiskEntry");
    DPFSO1("fe");
    DPFV2("vnode", "u", fe_cursor.fe.fe.vnode);
    DPFV2("unique", "u", fe_cursor.fe.fe.unique);
    DPFV2("volid", "u", fe_cursor.fe.fe.volid);
    DPFV2("fnext", "u", fe_cursor.fe.fe.fnext);
    DPFV2("ncbs", "u", fe_cursor.fe.fe.ncbs);
    DPFV2("firstcb", "u", fe_cursor.fe.fe.firstcb);
    DPFV2("status", "u", fe_cursor.fe.fe.status);
    DPFSC1;
    DPFV1("index", "u", fe_cursor.fe.index);
    DPFSC0;
}

static void
dump_cb_entry(void)
{
    DPFSO0("CBDiskEntry");
    DPFSO1("cb");
    DPFV2("cnext", "u", cb_cursor.cb.cb.cnext);
    DPFV2("fhead", "u", cb_cursor.cb.cb.fhead);
    DPFV2("thead", "u", (afs_uint32)cb_cursor.cb.cb.thead);
    DPFV2("status", "u", (afs_uint32)cb_cursor.cb.cb.status);
    DPFV2("hhead", "u", cb_cursor.cb.cb.hhead);
    DPFV2("tprev", "u", cb_cursor.cb.cb.tprev);
    DPFV2("tnext", "u", cb_cursor.cb.cb.tnext);
    DPFV2("hprev", "u", cb_cursor.cb.cb.hprev);
    DPFV2("hnext", "u", cb_cursor.cb.cb.hnext);
    DPFSC1;
    DPFV1("index", "u", cb_cursor.cb.index);
    DPFSC0;
}

#define DPFHMS printf("  ")
#define DPFHS printf("    ")
#define DPFHN(offset) printf("\n%u\t", offset)
#define DPFHD(x) printf("%02X  ", x)
#define DPFHE printf("\n")

static void
hexdump_map(afs_uint32 offset, afs_uint32 len)
{
    int i;
    unsigned char * p = (unsigned char *)map;
    afs_uint32 c32;

    if (!len)
	return;

    if ((offset + len) > map_len) {
	fprintf(stderr, "offset + length exceeds memory map size (%u > %u)\n",
		offset+len, map_len);
	return;
    }

    p += offset;
    DPFOFF(p);
    DPFHN(offset);

    for (i = offset % 16; i > 0; i--) {
	DPFHS;
    }

    for (i=0; i < len; i++, p++, offset++) {
	if (!(offset % 16)) {
	    DPFHN(offset);
	} else if (!(offset % 8)) {
	    DPFHMS;
	}
	DPFHD(*p);
    }
    DPFHE;
}

static int
get_hdr(void)
{
    if (!hdrs.hdr_valid) {
	if (map_len < sizeof(struct fs_state_header)) {
	    fprintf(stderr, "corrupt state dump: fs_state_header larger than memory map\n");
	    return 1;
	}
	memcpy(&hdrs.hdr, map, sizeof(hdrs.hdr));
	hdrs.hdr_p = map;
	hdrs.hdr_valid = 1;
    }
    return 0;
}

static int
get_h_hdr(void)
{
    char * buf;
    afs_uint32 hi, lo;

    if (hdrs.h_hdr_valid)
	return 0;

    if (get_hdr())
	return 1;

    SplitInt64(hdrs.hdr.h_offset, hi, lo);

    if (hi) {
	fprintf(stderr, "hi offset bits set in h_offset; can't get host_state_header\n");
	return 1;
    }
    if ((lo >= map_len) || 
	((lo + sizeof(struct host_state_header)) > map_len) ||
	(lo + sizeof(struct host_state_header) < lo)) {
	fprintf(stderr, "h_offset puts host_state_header beyond end of memory map\n");
	return 1;
    }

    buf = (char *) map;
    buf += lo;
    memcpy(&hdrs.h_hdr, buf, sizeof(struct host_state_header));
    hdrs.h_hdr_p = buf;
    buf += sizeof(struct host_state_header);
    he_cursor.fh = (void *)buf;
    return 0;
}

static int
get_cb_hdr(void)
{
    char * buf;
    afs_uint32 hi, lo;

    if (hdrs.cb_hdr_valid)
	return 0;

    if (get_hdr())
	return 1;

    SplitInt64(hdrs.hdr.cb_offset, hi, lo);

    if (hi) {
	fprintf(stderr, "hi offset bits set in cb_offset; can't get callback_state_header\n");
	return 1;
    }
    if ((lo >= map_len) || 
	((lo + sizeof(struct callback_state_header)) > map_len) ||
	(lo + sizeof(struct callback_state_header) < lo)) {
	fprintf(stderr, "cb_offset puts callback_state_header beyond end of memory map\n");
	return 1;
    }

    buf = (char *) map;
    buf += lo;
    memcpy(&hdrs.cb_hdr, buf, sizeof(struct callback_state_header));
    hdrs.cb_hdr_p = buf;
    hdrs.cb_hdr_valid = 1;

    SplitInt64(hdrs.cb_hdr.fe_offset, hi, lo);

    if (hi) {
	fprintf(stderr, "hi offset bits set in fe_offset; can't get callback_state_entry_header\n");
	return 1;
    }
    hi = lo + (hdrs.cb_hdr.nFEs * (sizeof(struct callback_state_entry_header) +
				  sizeof(struct FEDiskEntry)) +
	       hdrs.cb_hdr.nCBs * sizeof(struct CBDiskEntry));
    if ((hi > map_len) ||
	(lo > hi)) {
	fprintf(stderr, "fe_offset puts callback_state_entry_header beyond end of memory map\n");
	return 1;
    }

    buf = (char *) map;
    buf += lo;
    fe_cursor.ffe = (void *)buf;

    return 0;
}

static int
get_cb_timeout_hdr(void)
{
    char * buf;
    afs_uint32 hi, lo;

    if (hdrs.timeout_hdr_valid)
	return 0;

    if (get_cb_hdr())
	return 1;

    SplitInt64(hdrs.cb_hdr.timeout_offset, hi, lo);

    if (hi) {
	fprintf(stderr, "hi offset bits set in timeout_offset; can't get callback_state_timeout_header\n");
	return 1;
    }
    if ((lo >= map_len) || 
	((lo + sizeof(struct callback_state_timeout_header)) > map_len) ||
	(lo + sizeof(struct callback_state_timeout_header) < lo)) {
	fprintf(stderr, "timeout_offset puts callback_state_timeout_header beyond end of memory map\n");
	return 1;
    }

    buf = (char *) map;
    buf += lo;
    memcpy(&hdrs.timeout_hdr, buf, sizeof(struct callback_state_timeout_header));
    hdrs.timeout_hdr_p = buf;
    hdrs.timeout_hdr_valid = 1;
    buf += sizeof(struct callback_state_timeout_header);
    hdrs.timeout_p = buf;

    return 0;
}

static int
get_cb_timeout(void)
{
    char * buf;

    if (hdrs.timeout)
	return 0;

    if (get_cb_timeout_hdr())
	return 1;

    hdrs.timeout = (afs_uint32 *) calloc(hdrs.timeout_hdr.records, sizeof(afs_uint32));
    assert(hdrs.timeout != NULL);
    memcpy(hdrs.timeout, hdrs.timeout_p, hdrs.timeout_hdr.records * sizeof(afs_uint32));
    return 0;
}

static int
get_cb_fehash_hdr(void)
{
    char * buf;
    afs_uint32 hi, lo;

    if (hdrs.fehash_hdr_valid)
	return 0;

    if (get_cb_hdr())
	return 1;

    SplitInt64(hdrs.cb_hdr.fehash_offset, hi, lo);

    if (hi) {
	fprintf(stderr, "hi offset bits set in fehash_offset; can't get callback_state_fehash_header\n");
	return 1;
    }
    if ((lo >= map_len) || 
	((lo + sizeof(struct callback_state_fehash_header)) > map_len) ||
	(lo + sizeof(struct callback_state_fehash_header) < lo)) {
	fprintf(stderr, "timeout_offset puts callback_state_fehash_header beyond end of memory map\n");
	return 1;
    }

    buf = (char *) map;
    buf += lo;
    memcpy(&hdrs.fehash_hdr, buf, sizeof(struct callback_state_fehash_header));
    hdrs.fehash_hdr_p = buf;
    hdrs.fehash_hdr_valid = 1;
    buf += sizeof(struct callback_state_fehash_header);
    hdrs.fehash_p = buf;

    return 0;
}

static int
get_cb_fehash(void)
{
    char * buf;

    if (hdrs.fehash)
	return 0;

    if (get_cb_fehash_hdr())
	return 1;

    hdrs.fehash = (afs_uint32 *) calloc(hdrs.fehash_hdr.records, sizeof(afs_uint32));
    assert(hdrs.fehash != NULL);
    memcpy(hdrs.fehash, hdrs.fehash_p, hdrs.fehash_hdr.records * sizeof(afs_uint32));
    return 0;
}

static int
get_he(afs_uint32 idx)
{
    int i;
    char * p;

    if (get_h_hdr())
	return 1;

    if (idx >= hdrs.h_hdr.records)
	return 1;

    if (he_cursor.idx == idx && he_cursor.hdr_valid && he_cursor.he_valid)
	return 0;

    he_cursor.hdr_valid = he_cursor.he_valid = 0;

    if (he_cache.cursor == NULL) {
	he_cache.cursor = (void **) calloc(hdrs.h_hdr.records, sizeof(void *));
	assert(he_cache.cursor != NULL);
    }

    if (idx && he_cache.cursor[idx-1] == NULL) {
	for (i = 0; i < idx; i++) {
	    if (he_cache.cursor[i] == NULL) {
		get_he(i);
	    }
	}
    }

    if (!idx) {
	he_cursor.cursor = he_cursor.fh;
    } else if (he_cursor.cursor == he_cache.cursor[idx-1]) {
	p = (char *)he_cursor.cursor;
	p += he_cursor.hdr.len;
	he_cursor.cursor = (void *)p;
    } else {
	he_cursor.cursor = he_cache.cursor[idx-1];
	if (get_he_hdr())
	    return 1;
	p = (char *)he_cursor.cursor;
	p += he_cursor.hdr.len;
	he_cursor.cursor = (void *)p;
    }

    he_cursor.idx = idx;
    he_cache.cursor[idx] = he_cursor.cursor;

    if (get_he_hdr())
	return 1;
    if (get_he_entry())
	return 1;

    return 0;
}

static int
get_he_hdr(void)
{
    memcpy(&he_cursor.hdr, he_cursor.cursor, sizeof(struct host_state_entry_header));
    he_cursor.hdr_valid = 1;
    return 0;
}

static int
get_he_entry(void)
{
    char * p;

    if (!he_cursor.hdr_valid) {
	if (get_he_hdr()) {
	    return 1;
	}
    }

    p = (char *) he_cursor.cursor;
    p += sizeof(struct host_state_entry_header);

    memcpy(&he_cursor.he, p, sizeof(struct hostDiskEntry));

    he_cursor.he_valid = 1;
    p += sizeof(struct hostDiskEntry);
    he_cursor.ifp = (void *)p;
    if (he_cursor.hdr.interfaces) {
	p += sizeof(struct Interface) + ((he_cursor.hdr.interfaces-1)*sizeof(struct AddrPort));
	he_cursor.hcps = (void *)p;
    } else {
	he_cursor.hcps = he_cursor.ifp;
    }
    return 0;
}

static int
get_fe(afs_uint32 idx)
{
    int i;
    char * p;

    cb_cursor.cb_valid = 0;

    if (get_cb_hdr())
	return 1;

    if (idx >= hdrs.cb_hdr.nFEs)
	return 1;

    if (fe_cursor.idx == idx && fe_cursor.hdr_valid && fe_cursor.fe_valid)
	return 0;

    fe_cursor.hdr_valid = fe_cursor.fe_valid = 0;

    if (fe_cache.cursor == NULL) {
	fe_cache.cursor = (void **) calloc(hdrs.cb_hdr.nFEs, sizeof(void *));
	assert(fe_cache.cursor != NULL);
    }

    if (idx && fe_cache.cursor[idx-1] == NULL) {
	for (i = 0; i < idx; i++) {
	    if (fe_cache.cursor[i] == NULL) {
		get_fe(i);
	    }
	}
    }

    if (!idx) {
	fe_cursor.cursor = fe_cursor.ffe;
    } else if (fe_cursor.cursor == fe_cache.cursor[idx-1]) {
	p = (char *)fe_cursor.cursor;
	p += fe_cursor.hdr.len;
	fe_cursor.cursor = (void *)p;
    } else {
	fe_cursor.cursor = fe_cache.cursor[idx-1];
	if (get_fe_hdr())
	    return 1;
	p = (char *)fe_cursor.cursor;
	p += fe_cursor.hdr.len;
	fe_cursor.cursor = (void *)p;
    }

    fe_cursor.idx = idx;
    fe_cache.cursor[idx] = fe_cursor.cursor;

    if (get_fe_hdr())
	return 1;
    if (get_fe_entry())
	return 1;

    return 0;
}

static int
get_fe_hdr(void)
{
    memcpy(&fe_cursor.hdr, fe_cursor.cursor, sizeof(struct callback_state_entry_header));
    fe_cursor.hdr_valid = 1;
    return 0;
}

static int
get_fe_entry(void)
{
    char * p;

    if (!fe_cursor.hdr_valid) {
	if (get_fe_hdr()) {
	    return 1;
	}
    }

    p = (char *) fe_cursor.cursor;
    p += sizeof(struct callback_state_entry_header);

    memcpy(&fe_cursor.fe, p, sizeof(struct FEDiskEntry));

    fe_cursor.fe_valid = 1;
    p += sizeof(struct FEDiskEntry);
    fe_cursor.fcb = (void *)p;
    return 0;
}

static int
get_cb(afs_uint32 idx)
{
    int i;
    char * p;

    if (get_fe(fe_cursor.idx))
	return 1;

    if (idx >= fe_cursor.hdr.nCBs)
	return 1;

    if (idx == cb_cursor.idx && cb_cursor.cb_valid)
	return 0;

    cb_cursor.cb_valid = 0;

    p = (char *)fe_cursor.fcb;
    p += idx * sizeof(struct CBDiskEntry);
    cb_cursor.cursor = (void *)p;

    cb_cursor.idx = idx;

    if (get_cb_entry())
	return 1;

    return 0;
}

static int
get_cb_entry(void)
{
    memcpy(&cb_cursor.cb, cb_cursor.cursor, sizeof(struct CBDiskEntry));
    cb_cursor.cb_valid = 1;
    return 0;
}

static int
find_he_by_index(afs_uint32 idx)
{
    int i;

    if (get_h_hdr()) {
	return 1;
    }

    for (i = 0; i < hdrs.h_hdr.records; i++) {
	if (get_he(i)) {
	    fprintf(stderr, "error getting he %d\n", i);
	    return 1;
	}
	if (he_cursor.he.index == idx)
	    break;
    }

    if (i < hdrs.h_hdr.records) {
	dump_this_he();
	return 0;
    }
    return 1;
}

static int
find_fe_by_index(afs_uint32 idx)
{
    int i;

    if (get_cb_hdr()) {
	return 1;
    }

    for (i = 0; i < hdrs.cb_hdr.nFEs; i++) {
	if (get_fe(i)) {
	    fprintf(stderr, "error getting fe %d\n", i);
	    return 1;
	}
	if (fe_cursor.fe.index == idx)
	    break;
    }

    if (i < hdrs.cb_hdr.nFEs) {
	dump_this_fe();
	return 0;
    }
    return 1;
}

static int
find_fe_by_fid(afs_uint32 volid, afs_uint32 vnode, afs_uint32 unique)
{
    int i;

    if (get_cb_hdr()) {
	return 1;
    }

    for (i = 0; i < hdrs.cb_hdr.nFEs; i++) {
	if (get_fe(i)) {
	    fprintf(stderr, "error getting fe %d\n", i);
	    return 1;
	}
	if ((fe_cursor.fe.fe.unique == unique) &&
	    (fe_cursor.fe.fe.volid == volid) &&
	    (fe_cursor.fe.fe.vnode == vnode))
	    break;
    }

    if (i < hdrs.cb_hdr.nFEs) {
	dump_this_fe();
	return 0;
    }
    return 1;
}

static int
find_cb_by_index(afs_uint32 idx)
{
    int i;

    if (get_fe_hdr()) {
	return 1;
    }

    for (i = 0; i < fe_cursor.hdr.nCBs; i++) {
	if (get_cb(i)) {
	    fprintf(stderr, "error getting cb %d\n", i);
	    return 1;
	}
	if (cb_cursor.cb.index == idx)
	    break;
    }

    if (i < fe_cursor.hdr.nCBs) {
	dump_this_cb();
	return 0;
    }
    return 1;
}

#endif /* AFS_DEMAND_ATTACH_FS */
