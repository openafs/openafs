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

#define MAX_NAME  255
#define CELL_DIRTY   0x01

#define REALLY_BIG 1024



extern char    *optarg;

/* forward function declarations */
void init_stats(void);
void close_backup(void);
void merge_logs(void);
void usage();
void log_stats(log_ent_t *, int);


/* global variable declarations */	
int   backup_fd;
int   stat_fd;

char    *stat_names[] = {
		"store",		/* 0 */
		"mkdir",		/* 1 */
		"create",		/* 2 */
		"remove",		/* 3 */
		"rmdir",		/* 4 */
		"rename",		/* 5 */
		"link",			/* 6 */
		"symlink",		/* 7 */
		"setattr",		/* 8 */
		"fsync",           	/* 9 */
		"access",		/* 10 */
		"readdir",		/* 11 */
		"readlink",		/* 12 */
		"info",			/* 13 */
		"totals"};              /* 14 */

/* XXX lhuston fix */
#define	MAX_OPS		15
#define	TOTALS		14

final_stats_t	stats[MAX_OPS];





main(argc, argv)
	int argc;
	char **argv;
{	int fd, cc;
	FILE *fp;
	/* XXX lhuston fname should not optional */
	char *fname = NULL;
	int c;
	int verbose = 0;
	int lflag = 0;
	int detailed = 0;
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

	char *bp;
	int len;
	log_ops_t op;

	while ((c = getopt(argc, argv, "h?f:")) != EOF) {
		switch (c) {

		case 'f':
			fname = optarg;
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

	
	init_stats();

	cur_stat = (repl_stats_t *) malloc(sizeof(repl_stats_t));

	
	while(fread(cur_stat, sizeof (repl_stats_t), 1, fp)) {
		update_stats(cur_stat);
	}

	free((char *) cur_stat);
	fclose(fp);

	dump_stats();

	exit(0);
}

void
usage()
{

	printf("Options are: \n");
	printf("\t -f use a non-default file name \n");
	printf("\t -l log replay stats \n");
	printf("\t -r recno to print a specific record \n");
	printf("\t -v include read-only ops\n");
	printf("\t -h this help information \n");
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

/* add the time values first and second, and return the result in result */

void
time_add(first, second, result)
        struct timeval  first;
        struct timeval  second;
        struct timeval  *result;
{
        long    usec;
        long    sec;

        sec = first.tv_sec + second.tv_sec;
        usec = first.tv_usec + second.tv_usec;

        /* Handle carry  */
        if (usec > 1000000) {
                sec += 1;
                usec -= 1000000;
        }

        result->tv_sec = sec;
        result->tv_usec = usec;
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
update_stats(cur_stat)
	repl_stats_t *cur_stat;
{
        struct timeval  difference;
	float	fdiff;


        time_difference(&cur_stat->finish_time, &cur_stat->start_time,
	     &difference);

	fdiff = time_to_float(&difference);
	/* update the stats for this operations	 */

        stats[cur_stat->op].num_ops++;
        stats[cur_stat->op].total_time += fdiff;

        if (stats[cur_stat->op].max_time < fdiff)
		stats[cur_stat->op].max_time = fdiff;

        if (stats[cur_stat->op].min_time > fdiff)
		stats[cur_stat->op].min_time = fdiff;

	/*  update the overall stats */
        stats[TOTALS].num_ops++;
        stats[TOTALS].total_time += fdiff;

        if (stats[TOTALS].max_time < fdiff)
		stats[TOTALS].max_time = fdiff;

        if (stats[TOTALS].min_time > fdiff)
		stats[TOTALS].min_time = fdiff;
}

/* Initialize the statistics structure */

void
init_stats()

{
        int i;

        for (i=0; i< MAX_OPS; i++) {
                stats[i].num_ops = 0;
                stats[i].min_time = 10000000;
                stats[i].max_time = 0;
                stats[i].total_time = 0;
        }
}

void
dump_stats()
{
        int i;

        for (i=0; i< MAX_OPS; i++) {
		if (stats[i].num_ops > 0) {
                	printf("%s \n", stat_names[i]);
                	printf("\t number %d  total_time %4.2f  \n",
                    	    stats[i].num_ops, stats[i].total_time);
                	printf("\t min time %4.2f max time %4.2f \n",
			    stats[i].min_time, stats[i].max_time);
		}
        }
	
	printf("total max %5.2f avg %5.2f \n", stats[TOTALS].max_time,
		(stats[TOTALS].total_time/stats[TOTALS].num_ops));
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
