#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/inode.h>
#include <netinet/in.h>
/*
*/
#include "lock.h"
#include "afsint.h" 

#include "venus.h"
#define KERNEL
#include "afs.h"
#include "discon.h"
#include "discon_log.h"
#include "playlog.h"


#define MAX_ELEMENTS 	10000

/* defines for the type of elements */
#define	ADD	1
#define	REMOVE	2;

struct	elem {
	float	time;
	int	type;
};

struct	elem data_points[MAX_ELEMENTS];
int	current_ent;

extern char    *optarg;

/* forward function declarations */
void init_stats(void);
void close_backup(void);
void merge_logs(void);
void usage();
void log_stats(log_ent_t *, int);
void sort_ents(void);
void add_entry(repl_stats_t *); 
void dump_data(void); 

char    *stat_names[] = {
                "store",                /* 0 */
                "mkdir",                /* 1 */
                "create",               /* 2 */
                "remove",               /* 3 */
                "rmdir",                /* 4 */
                "rename",               /* 5 */
                "link",                 /* 6 */
                "symlink",              /* 7 */
                "setattr",              /* 8 */
                "fsync",                /* 9 */
                "access",               /* 10 */
                "readdir",              /* 11 */
                "readlink",             /* 12 */
                "info",                 /* 13 */
                "totals"};              /* 14 */





main(argc, argv)
	int argc;
	char **argv;
{	int fd, cc;
	FILE *fp;
	char *fname = NULL;
	int c;
	repl_stats_t *cur_stat;
	int recflag = 0;
	int recno;
	int current_rec = 0;
	int code;
	int step = 0;
	int aggressive = 0;
	int keep_looping = 1;
	long last_op;	
	int no_replay = 0;

	int sflag = 0;
	int fflag = 0;

	char *bp;
	int len;
	log_ops_t op;

	while ((c = getopt(argc, argv, "aslh?f:")) != EOF) {
		switch (c) {

		case 'f':
			fname = optarg;
			break;
		
		case 's':
			sflag = 1;
			break;

		case 'l':
			fflag = 1;
			break;

		case '?':
		case 'h':
			usage();
			exit(0);	
			
		default:
			printf("bad opt %c \n", c);
			usage();
			exit(1);
		}
	}

	if (fname == NULL) {
		usage();
		exit(1);
	}

	if ((fp = fopen(fname, "r")) == 0) {
		perror("failed to open log ");
		exit(-1);
	}

	
	cur_stat = (repl_stats_t *) malloc(sizeof(repl_stats_t));
	
	while(fread(cur_stat, sizeof (repl_stats_t), 1, fp)) {
		print_ent(cur_stat, sflag, fflag);
	}
	free((char *) cur_stat);
	fclose(fp);

	exit(0);
}

void
usage()
{

	printf("Options are: \n");
	printf("\t -f log file \n");
	printf("\t -? this help information \n");
}

/*
 * compute the time difference between first and second, and
 * return the value in difference
 */

void
time_difference(first, second, difference)
        struct timeval  *first;
        struct timeval  *second;
        struct timeval  *difference;
{
        long    usec;
        long    sec;

        sec = first->tv_sec - second->tv_sec;
        usec = first->tv_usec - second->tv_usec;

        /* Handle the carry operation */
        if (usec < 0) {
                sec -= 1;
                usec = 1000000 + usec;
        }

        difference->tv_sec = sec;
        difference->tv_usec = usec;
}


float
time_to_float(time)
        struct timeval  *time;
{
	float tot = 0;

	tot = (float) time->tv_usec / (float) 1000000;
	tot += (float) time->tv_sec;
	return tot;
}

void
print_ent(cur_stat, p_start, p_finish)
	repl_stats_t *cur_stat;
	int	p_start;
	int	p_finish;
	
{
	static struct timeval  start_time;
        struct timeval  difference;
	static do_init = 1;

	float	stime;
	float 	ftime;

	/* do this on the first time through */	
	if (do_init) {
		current_ent = 0;
		/* this is our 0 point */
		start_time = cur_stat->start_time;
		do_init = 0;
	}

	/* enter the start time */	
        time_difference(&cur_stat->start_time, &start_time,
	     &difference);
	stime = time_to_float(&difference);


	/* enter the finish time */	
        time_difference(&cur_stat->finish_time, &start_time,
	     &difference);

	ftime = time_to_float(&difference);

	printf("%9s ", stat_names[cur_stat->op]);
	if (p_start)
		printf("%5.2f ", stime);
	if (p_finish)
		printf("%5.2f ", ftime);
	printf("\n");
	
	
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
