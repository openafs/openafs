/*
 * This file contians functions that relate to performance statistics
 * for disconnected operation.
 */


#ifdef DISCONN

#ifndef MACH30
#include "../afs/param.h"    /* Should be always first */
#include "../afs/sysincludes.h"      /* Standard vendor system headers */
#include "../afs/afsincludes.h"      /* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */
#include "../afs/osi.h"
#include "../afs/discon_stats.h"
#else
#include <afs/param.h>    /* Should be always first */
#include <afs/sysincludes.h>      /* Standard vendor system headers */
#include <afs/afsincludes.h>      /* Afs-based standard headers */
#include <afs/afs_stats.h>   /* afs statistics */
#include <afs/osi.h>

#endif


#include "discon.h"
#include "discon_stats.h"

#ifdef LHUSTON
#include "discon_log.h"
#include "proto.h"
#endif

/*
 * These values need to correspond to the index values 
 * defined in discon_stats.h 
 */
char    *stat_names[] = {
                "recon write",			/* 0 */
                "recon create",			/* 1 */
                "recon mkdir",			/* 2 */
                "recon remove",			/* 3 */
                "recon rmdir",			/* 4 */
                "recon rename",			/* 5 */
                "recon link",			/* 6 */
                "recon symlink",		/* 7 */
                "recon setattr",		/* 8 */
                "recon non_mutating",		/* 9 */
                "recon errors"}; 		/* 10 */


/* global structure to store replay stats in */

struct  replay_times	replay_stats[TOTAL_STATS];

/*
 * compute the time difference between first and second, and 
 * return the value in difference 
 */

void
time_difference(first, second, difference)
	struct timeval	first;
	struct timeval	second;
	struct timeval	*difference;
{
	long	usec;
	long	sec;

	sec = first.tv_sec - second.tv_sec;
	usec = first.tv_usec - second.tv_usec;

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
	struct timeval	first;
	struct timeval	second;
	struct timeval	*result;
{
	long	usec;
	long	sec;

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
/*
 * This function takes two timeval struct and compares them.  The return value
 * is:
 *  	-1	if first < second
 * 	 0 	if first = second
 *	 1 	if first > second
 */

int
time_cmp(first, second)
	struct timeval first;
	struct timeval second;
{
	if (first.tv_sec < second.tv_sec) 
		return (-1);
	else if (first.tv_sec == second.tv_sec) {
		if (first.tv_usec < second.tv_usec) 
			return (-1);
		else if (first.tv_usec == second.tv_usec) 
			return (0);
		else
			return (1);
	} else {
		return (1);
	}	

}

void
update_stats(start, end, index)
	struct timeval	start;
	struct timeval	end;
	int	index;
{
	struct  replay_times	*current;
	struct timeval	difference;
	struct timeval  new_total;

	/* XXX lhuston debug */
	if ((index >= TOTAL_STATS) || (index < 0)) {
		printf("update_stats: bad index %d \n", index);
		panic("update_stats: bad index ");
	}

	current = &replay_stats[index];
	
	time_difference(end, start, &difference);
	time_add(difference, current->total_time, &new_total);
	
	/* set the new values */
	current->number++;
	current->total_time = new_total;

	if (time_cmp(difference, current->mintime) < 0) 
		current->mintime = difference;

	if (time_cmp(difference, current->maxtime) > 0)
		current->maxtime = difference;
	
}


/* Initialize the statistics structure */

void
initilize_replay_stats()
{
	int i;
	
	for (i=0; i< TOTAL_STATS; i++) {
		replay_stats[i].number = 0;
		replay_stats[i].total_time.tv_sec = 0;
		replay_stats[i].total_time.tv_usec = 0;
		replay_stats[i].maxtime.tv_sec = 0;
		replay_stats[i].maxtime.tv_usec = 0;
		replay_stats[i].mintime.tv_sec = 0xBEAF;
		replay_stats[i].mintime.tv_usec = 0xBEAF;

	}
}

void
dump_replay_stats()
{
	int i;

	discon_log(DISCON_INFO, "replay stats: \n");
	
	for (i=0; i< TOTAL_STATS; i++) {
		discon_log(DISCON_INFO, "%s \n", stat_names[i]);
		discon_log(DISCON_INFO, "\t number %d  total_time %d.%d \n",
		    replay_stats[i].number, replay_stats[i].total_time.tv_sec,
		    replay_stats[i].total_time.tv_usec);
		discon_log(DISCON_INFO, "\t min %d.%d  max %d.%d \n",
		    replay_stats[i].mintime.tv_sec, 
		    replay_stats[i].mintime.tv_usec,
		    replay_stats[i].maxtime.tv_sec,
		    replay_stats[i].maxtime.tv_usec);
	}
}

/*
This copyright pertains to modifications set off by the compile
option DISCONN
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



#endif /* DISCONN */
