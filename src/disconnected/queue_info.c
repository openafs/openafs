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

	
	cur_stat = (repl_stats_t *) malloc(sizeof(repl_stats_t));
	
	while(fread(cur_stat, sizeof (repl_stats_t), 1, fp)) {
		add_entry(cur_stat);
	}
	free((char *) cur_stat);
	fclose(fp);

	sort_ents();
	dump_data();

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
add_entry(cur_stat)
	repl_stats_t *cur_stat;
{
	static struct timeval  start_time;
        struct timeval  difference;
	static do_init = 1;

	float	fdiff;

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
	fdiff = time_to_float(&difference);

	data_points[current_ent].time = fdiff;
	data_points[current_ent].type = ADD;
	current_ent++;
	

	/* enter the finish time */	
        time_difference(&cur_stat->finish_time, &start_time,
	     &difference);

	fdiff = time_to_float(&difference);

	data_points[current_ent].time = fdiff;
	data_points[current_ent].type = REMOVE;
	current_ent++;
	
}

int
comp_ents(ent1, ent2)
	struct elem *ent1;
	struct elem *ent2;
{
	if (ent1->time < ent2->time)
		return (-1);
	else
		return (1);
}

void
sort_ents()
{
	qsort((char *) data_points, current_ent, sizeof(struct elem), 
	    comp_ents);

}

void
dump_data()
{
        int i;
	int len = 0;
		
        for (i=0; i< current_ent; i++) {
		if (data_points[i].type == ADD) { 
			len++;
		} else {
			len--;
		}
#ifdef notdef
		if (data_points[i].type == REMOVE) {
			len--;
		} else {
			printf("bad data type \n");
			exit (-1);
		}
#endif
		printf("%5.2f  %4d \n",data_points[i].time, len);
        }
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
