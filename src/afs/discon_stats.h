

/*
** These are defines for the different type of disconnected
** operations stored in the log.
*/

#ifndef  _DISCONN_STATS_H
#define _DISCONN_STATS_H

struct	replay_times {
	int		number;
	struct	timeval	mintime;
	struct	timeval	maxtime;
	struct	timeval	total_time;
};

/* This define needs to be the maximum of the following defines */

#define	TOTAL_STATS	11

#define	TIME_RECON_STORE	0
#define	TIME_RECON_CREATE	1
#define	TIME_RECON_MKDIR	2
#define	TIME_RECON_REMOVE	3
#define	TIME_RECON_RMDIR	4
#define	TIME_RECON_RENAME	5
#define	TIME_RECON_LINK		6
#define	TIME_RECON_SYMLINK	7
#define	TIME_RECON_SETATTR	8
#define	TIME_RECON_NON_MUTE	9
#define	TIME_RECON_ERROR	10

#endif  _DISCONN_STATS_H


#ifdef DISCONN
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
#endif

