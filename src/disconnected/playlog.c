#ifdef __OpenBSD__
#define __NetBSD__
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
#ifndef __NetBSD__
#include <sys/inode.h>
#endif
#include <netinet/in.h>
/*
*/
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/stds.h>
#undef VIRTUE
#undef VICE
#include "afs/prs_fs.h"
#include <afs/afsint.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <rx/rxkad.h>
#include <afs/vldbint.h>
#include <afs/volser.h>
#include <afs/volser.h>
#include <afs/vlserver.h>
#include <afs/cmd.h>
#include <strings.h>
#include <pwd.h>
#include <signal.h>

#ifdef __NetBSD__
#include "../libafs/afs/lock.h"
#else
#include "lock.h"
#endif
#include "afsint.h"

#define KERNEL
#include "afs.h"
#undef KERNEL
#include "discon.h"
#include "discon_log.h"
#include "playlog.h"

#define MAX_NAME  255
#define CELL_DIRTY   0x01

#define	DEFAULT_INTERVAL 5
#define REALLY_BIG 1024
#define MAX_OPTSTRING  512

extern char    *optarg, *tmpnam();

/* forward function declarations */
void init_stats(char *);
void close_backup(void);
void usage();
void log_stats(log_ent_t *, int, struct timeval);
FILE *open_log_file(void);
int *open_backup_file(void);
void replay_operations(char *, int, int, int, int);
int save_good_op(log_ent_t *, int);
int save_failed_op(log_ent_t *, int);
void clean_up(char *, char *, int);
void do_replay(int, int, int, int);
void copy_operations(char *, char *, int);
void optimize_log(char *, char *, int);
void init_optnames(char *, int);

extern char *mode_to_string(discon_modes_t mode);

void sig_handle();


/* global variable declarations */	
int   stat_fd;

/* strings for the optimizer */

#define DEFAULT_PATH 	"/usr/vice/etc"
#define	PREPROCESS 	"preprocess"
#define	COPT 		"copt"
#define	POST 		"post"
#define	SORT 		"sortents"
#define	PASS1 		"pass1"
#define	PASS2 		"pass2"

/* storage for the file names */

char	preprocess[MAX_NAME];
char	copt[MAX_NAME];
char	sort[MAX_NAME];
char	post[MAX_NAME];
char	pass1[MAX_NAME];
char	pass2[MAX_NAME];


/* extensions for the temporary files */

#define	OPT_EXT ".opt"
#define	NEW_EXT ".new"

/* storage for the temporary file names */
char	newfile_name[MAX_NAME];
char	optfile_name[MAX_NAME];


int min_delay = 30;
int once_only = 0;
int rflag = 0;



main(argc, argv)
int argc;
char **argv;
{
    int fd, cc;
    int c;
    int verbose = 0;
    char *new_path = 0;
    int lflag = 0;
    int detailed = 0;
    log_ent_t *cur_log;
    int current_rec = 0;
    int code;
    int step = 0;
    int interval = DEFAULT_INTERVAL;
    discon_modes_t current_mode;

    char *bp;
    int len;
    log_ops_t op;

    signal(SIGKILL, sig_handle);
    signal(SIGINT, sig_handle);

    while ((c = getopt(argc, argv, "?i:hdvsl:p:m:or")) != EOF) {
	switch (c) {

	    /* use verbose mode */
	case 'v':
	    verbose++;
	    break;

	    /* give detailed log information */
	case 'd':
	    detailed++;
	    break;

	    /* change the condition probe interval */
	case 'i':
	    interval = atoi(optarg);
	    break;

	    /* set the miniumum time delay */
	case 'm':
	    min_delay = atoi(optarg);
	    break;

	    /* should we log replay stats */
	case 'l':
	    init_stats(optarg);
	    lflag = 1;
	    break;

	    /* step through the log replay ? */
	case 's':
	    step = 1;
	    break;

	case 'r':
	    rflag = 1;
	case 'o':
	    once_only = 1;
	    break;

	    /* use a non-default path for the optimizer files */
	case 'p':
	    new_path = optarg;
	    break;
	

	case 'h': case '?':
	    usage();
	    exit(0);	

	default:
	    printf("bad opt %c \n", c);
	    usage();
	    exit(1);
	}
    }

    init_optnames((new_path ? new_path : DEFAULT_PATH), detailed);

    if (once_only) {
	current_mode = get_current_mode();
	if (current_mode == CONNECTED) {
	    fprintf(stderr, "Already connected\n");
	    exit(15);
	}
	if (!tokencheck(1))
	    exit(15);
	if (are_operations(0)) {
	    if (current_mode == DISCONNECTED)
		change_mode(AFS_DIS_FETCHONLY, FETCH_ONLY);
	    if (!network_up(0)) {
		fprintf(stderr, "Network is down");
		if (!askuser())
		    exit(15);
	    }
	    do_replay(verbose, detailed, step, lflag);
	}
	if (rflag)
	    change_mode(AFS_DIS_RECON, CONNECTED);
	exit(0);
    }

    while (1)
	if (should_replay(interval, verbose))
	    do_replay(verbose, detailed, step, lflag);
}

void
init_optnames(char *path, int detailed)
{

    sprintf(preprocess, "%s/%s", path, PREPROCESS);
    sprintf(copt, "%s/%s", path, COPT);
    sprintf(sort, "%s/%s", path, SORT);
    sprintf(post, "%s/%s", path, POST);
    if (detailed)
	strcat(post, " -d");
    sprintf(pass1, "%s/%s", path, PASS1);
    sprintf(pass2, "%s/%s", path, PASS2);

    return;
}


void
sig_handle()
{
	printf("caught signal, cleaning up \n");

	/* we keep these files name in a global for the sig handler. */
	clean_up(newfile_name, optfile_name, 0);
	exit(0);
}

void
copy_operations(origfile, newfile, verbose)
char *origfile;
char *newfile;
{
    FILE *orig_log;
    FILE *new_log;
    char *cp;	
    char *bp;
    log_ent_t *cur_log;
    int len, rval;

    if (verbose)
	printf("copying operations \n");

    /* get a name for a temp file for the log operations */

    cp = tmpnam(newfile);
    if (verbose)
	printf("new log file: %s\n", cp);

    if (cp == NULL) {
	perror("failed get a temp file name");
	exit(3);
    }
    strcat(newfile, NEW_EXT);

    /* open the new file */
    if ((new_log = fopen(newfile, "w")) == NULL) {
	perror("failed to open newlog ");
	exit(4);
    }

    /* open the log file */
    if ((orig_log = fopen(origfile, "r")) == 0) {
	perror(origfile);
	exit(5);
    }


    cur_log = (log_ent_t *) malloc(sizeof(log_ent_t));
	
    /* read each operation */
    while (fread(cur_log, sizeof (int), 1, orig_log)) {
	if (cur_log->log_len < sizeof(*cur_log) - sizeof(cur_log->log_data) ||
	    cur_log->log_len > sizeof(log_ent_t)) {
	    fprintf(stderr, "corrupt log entry, log_len %d\n", cur_log->log_len);
	    exit(6);
	}
	len = cur_log->log_len - sizeof(int);
	bp = (char *) cur_log + sizeof(int);

	if ((rval = fread(bp, len, 1, orig_log)) != 1) {
	    printf("parse log: bad read, rval %d != len %d\n");
	    break;		/* XXX should be fatal? */
	}
	/* check to see if this op is still valid */
	if ((cur_log->log_flags & LOG_INACTIVE) != 0)
	    continue;		/* inactive */
	/* write out this log ent */
		
	if ((rval = fwrite((char *) cur_log, cur_log->log_len, 1,
			   new_log)) != 1) {
	    printf("bad log write, ret %d\n", rval);
	    perror(newfile);
	    exit(7);
	}
    }	

    fclose(orig_log);
    fclose(new_log);
}


void	
optimize_log(newfile, optfile, verbose)
	char *newfile;
	char *optfile;
	int verbose;
{
	char *cp;
	int	error;
	char opt_string[MAX_OPTSTRING];

	if (verbose)
		printf("Optimizing log \n");

	cp = tmpnam(optfile);
	if (cp == NULL) {
		perror("failed get a temp file name");
		exit(8);
	}

	strcat(optfile, OPT_EXT);

	/* create the pipline string for the optimizer */
	sprintf(opt_string, "%s -f %s | %s %s | %s | %s %s | %s -l %s -n %s",
	    preprocess, newfile, copt, pass1, sort, copt, pass2, post, newfile,
	    optfile);

	if (strlen(opt_string) > MAX_OPTSTRING) {
		printf("string too long \n");
	}


	if (verbose)
		puts(opt_string);
	/* run the optimizer */
	error = system(opt_string);	
	if (error) {
		perror("optimizer failed");
		exit(9);
	}

	return;
}


/* remove the temporary log files that were created */

void	
clean_up(newfile, optfile, verbose)
	char *newfile;
	char *optfile;
	int verbose;
{
	int code;

	if (verbose)
		printf("cleaning up \n");
	
	if (newfile[0] != 0) {	
		code = unlink(newfile);
		if (code)
			perror("clean_up: removing the tmp log file");
		newfile[0] = 0;
	}

	if (optfile[0] != 0) {
		code = unlink(optfile);
		if (code)
			perror("clean_up: removing the optimized log file");
		optfile[0] = 0;
	}

	return;
}




void
do_replay(verbose, detailed, step, lflag)
int verbose;
int detailed;
int step;
int lflag;
{
    char *fname = NULL;
    int code;

    /* get the name of the log file from the kernel */
    if (!fname)
	fname = (char *) malloc(MAX_NAME);
    code = get_log_file_name(fname);
    if (code) {
	perror("failed to get log name");
	exit (2);
    }

    /* copy the operations from the real log to a temp file */
    copy_operations(fname, newfile_name, verbose);

    /* optimize the temp log */
    optimize_log(newfile_name, optfile_name, verbose);

    /* replay the oeprations in the optimized file */
    replay_operations(optfile_name, verbose, detailed, step, lflag);

    /* delete the intermediate files created during replay */	
    clean_up(newfile_name, optfile_name, verbose);
}



/* This goes through the log of operations and replays them */

void
replay_operations(fname, verbose, detailed, step, do_log)
char *fname;
int verbose;
int detailed;
int step;
int do_log;
{
    log_ent_t *cur_log;
    char *bp;
    int len, code;
    FILE *log_file;
    int backup;
    struct timeval  start;
    struct timezone tz;
    int join_error = 0;

    /* open the log file */
    if ((log_file = fopen(fname, "r")) == 0) {
	perror(fname);
	exit(10);
    }

    cur_log = (log_ent_t *) malloc(sizeof(log_ent_t));
	
    /* read each operation */
    while (fread(cur_log, sizeof (int), 1, log_file)) {
	if (cur_log->log_len < sizeof(*cur_log) - sizeof(cur_log->log_data) ||
	    cur_log->log_len > sizeof(log_ent_t)) {
	    fprintf(stderr, "corrupt log entry, log_len %d\n", cur_log->log_len);
	    exit(11);
	}
	len = cur_log->log_len - sizeof(int);
	bp = (char *) cur_log + sizeof(int);

	if (!fread(bp, len, 1, log_file)) {
	    printf("parse log: bad read \n");
	    break;
	}

	/* check to see if this op is still active */
	if (cur_log->log_flags & LOG_INACTIVE) {
	    continue;
	}

	/* see if we should print out data */
	if (verbose)
	    display_op(cur_log, detailed);

	/*
	 * if this operations was optimized away, then
	 * we tell the kernel to remove it from the original
	 * log file.
	 */

	if (cur_log->log_flags & LOG_OPTIMIZED) {
	    /* if there was an error playing a joined
	     * operation, then we need to skip all
	     * subsequent optimized ops because their
	     * equivalent has not been replayed
	     */

	    if (!join_error) {
		set_log_flags(cur_log->log_offset,
			      (LOG_OPTIMIZED | LOG_INACTIVE));
	    }
	    continue;
	}

	join_error = 0;	

	/* get the current time for stats */
	code = gettimeofday(&start, &tz);
	if (code)
	    perror("repl_ops - failed to get current time:");

	/*
	 * the time on the log entry is greater than
	 * the minimum delay time.  If not, then we quit replaying
	 */
	if (!once_only && (start.tv_sec - cur_log->log_time.tv_sec) < min_delay)
	    break;
	
	/* if we are in interactive mode see if we should cont. */
	if (step) {
	    if (!askuser())
		break;
	}

	/* get the current time for stats */
	code = gettimeofday(&start, &tz);
	if (code)
	    perror("repl_ops - failed to get current time:");

	/* try to replay the operation */
	code = replay_op(cur_log);

	/* if we have an error, print out a message */
	if (code) {
	    printf("replay_ops: code %d \n", code);

	    if (cur_log->log_flags & LOG_GENERATED) {
		join_error = 1;
	    }
	}

	/*
	 * if we have replay logging turned on, then
	 * replay the operation.
	 */

	if (do_log)
	    log_stats(cur_log, code, start);
    }

    free((char *) cur_log);

    /* close the two log files */
    fclose(log_file);
}



void
usage()
{

	printf("Options are: \n");
	printf("\t -v be verbose \n");
	printf("\t -d detailed info about operations being replayed \n");
	printf("\t -i <n>  set the probe interval to n seconds \n");
	printf("\t -m <n>  set the miniumum delay to n seconds \n");
	printf("\t -l log replay stats \n");
	printf("\t -s query the user before replaying each operation \n");
	printf("\t -p <path> use non-default path for opimizer files \n");
	printf("\t -o once only mode.  Replay the current ops and exit \n");
	printf("\t -r replay once, reconnect, and exit\n");
	printf("\t -h this help information \n");
	printf("\t -? this help information \n");

}


/*
 * this function is the guts of the replay daemon because it
 * tries to determine if we should attempt to replay the
 * log.  It checks several conditions.  Is the kernel disconnected?,
 * is there a network, does the user have tokens ...
 */

int
should_replay(interval, verbose)
int interval;
int verbose;
{
    while (!good_mode(verbose) || !are_operations(verbose) ||
	   !has_tokens(verbose) || !network_up(verbose)) {

	if (verbose)
	    printf("sleep %d\n", interval);

	/* sleep for a while and then try again */
	sleep(interval);
    }
    return 1;
}



int
has_tokens(verbose)
{
    int code = 1;

    code = tokencheck(0);

    if (verbose)
	printf("tokens: %s; ", code ? "yes" : " no");

    return code;
}


/*
 * This functions checks to see if we are in a disconnected mode.  If
 * we are, then it returns 1, otherwise it will return a 0.
 */

int
good_mode(verbose)
{
    discon_modes_t current_mode;

    /* get the current mode */
    current_mode = get_current_mode();

    if (verbose)
	printf("mode: %s; ", mode_to_string(current_mode));

    /* if we are in a partial mood set code to 1 */
    return (current_mode == PARTIALLY_CONNECTED);
}

/*
 * This operations looks to see if there are operations in the log
 * to replay.  If there are,then it will return a 1, otherwise
 * it returns 0.
 */

int
are_operations(verbose)
{
    int are_ops = 0;
    char *fname;
    FILE *log_file;
    struct timeval  start;
    struct timezone tz;
    log_ent_t *cur_log;
    int code;
    int len;
    char *bp;

    /* get the name of the log file */

    fname = (char *) malloc(MAX_NAME);
    code = get_log_file_name(fname);
    if (code) {
	perror("are_ops: failed to get log name ");
	exit (12);
    }

    /* open the log file */
    if ((log_file = fopen(fname, "r")) == 0) {
	perror("are_ops: failed to open log");
	exit(13);
    }

    /* scan through the looking for ops to replay */
    cur_log = (log_ent_t *) malloc(sizeof(log_ent_t));
	
    /* read each operation */
    while (fread(cur_log, sizeof (int), 1, log_file)) {
	len = cur_log->log_len - sizeof(int);
	bp = (char *) cur_log + sizeof(int);

	if (!fread(bp, len, 1, log_file)) {
	    printf("parse log: bad read \n");
	    break;
	}

	/* check to see if this op is still active */
	if (cur_log->log_flags & LOG_INACTIVE) {
	    continue;
	}

	/*
	 * we have a good log entry; if it's over the maximum age
	 * set are_ops to 1.  Then we break.
	 */

	if (once_only) {
	    are_ops = 1;
	    break;
	}

	code = gettimeofday(&start, &tz);
	if (code)
	    perror("gettimeofday");

	/*
	 * the time on the log entry is greater than
	 * the minimum delay time.  If not, then we quit replaying
	 */
	if ((start.tv_sec - cur_log->log_time.tv_sec) >= min_delay)
	    are_ops = 1;

	break;
    }

    fclose(log_file);
    free(fname);
    free(cur_log);

    if (verbose)
	printf("ops: %s; ", are_ops ? "yes" : " no");

    return(are_ops);
}



/* open the file to dump the replay stats into */
void
init_stats(fname)
	char *fname;
{
	if (!(stat_fd = open(fname, O_WRONLY|O_CREAT, 0666))) {
		perror("init_backup: failed to backup file ");
		exit(14);
	}
}


void
log_stats(cur_log, op_status, start)
	log_ent_t *cur_log;
	int op_status;
	struct timeval  start;
{
	repl_stats_t repl_stats;
	struct timezone tz;
	struct timeval  cur_time;
	int code;

	repl_stats.op = cur_log->log_op;
	repl_stats.opno = cur_log->log_opno;
	repl_stats.start_time = cur_log->log_time;

	/* get the current time */
	code = gettimeofday(&cur_time, &tz);
	if (code)
		perror("log_stats - failed to get current time:");

	repl_stats.finish_time = cur_time;
	repl_stats.status = op_status;

	repl_stats.replstart_time = start;

	if (write(stat_fd, (char *) &repl_stats, sizeof(repl_stats_t)) !=
	    sizeof(repl_stats_t)) {
		perror("log_stats: writing stats file ");
	}

	return;
}

/*
****************************************************************************
*Copyright 1993 Regents of The University of Michigan - All Rights Reserved*
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appears in all copies and that  *
* both that copyright notice and this permission notice appear in          *
* supporting documentation, and that the name of The University of Michigan*
* not be used in advertising or publicity pertaining to distribution of    *
* the software without specific, written prior permission. This software   *
* is supplied as is without expressed or implied warranties of any kind.   *
*                                                                          *
*            Center for Information Technology Integration                 *
*                  Information Technology Division                         *
*                     The University of Michigan                           *
*                       535 W. William Street                              *
*                        Ann Arbor, Michigan                               *
*                            313-763-4403                                  *
*                        info@citi.umich.edu                               *
*                                                                          *
****************************************************************************
*/
