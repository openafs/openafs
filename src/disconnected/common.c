#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <netinet/in.h>
#include "../libafs/afs/lock.h"
#include "afsint.h"
#include "venus.h"
#include "stds.h"
#define KERNEL
#include "afs.h"
#undef KERNEL

#include "discon.h"
#include "discon_log.h"
#include "playlog.h"

discon_modes_t get_current_mode();
char *mode_to_string(discon_modes_t mode);

#define BIG_STRING	255

change_mode(cmd, newmode)
long cmd;
discon_modes_t newmode;
{
    discon_modes_t mode;
    int code;
    struct ViceIoctl blob;

    blob.in_size = sizeof(long);
    blob.in = (char *) &cmd;
    blob.out_size = sizeof(discon_modes_t);
    blob.out = (char *) &mode;

    code = pioctl(0, VIOCDISCONNECT, &blob, 1);

    if (code < 0 || mode != newmode) {
	fprintf(stderr, "Failed to change to %s mode\n", mode_to_string(newmode));
	if (code < 0)
	    perror("pioctl");
	else
	    print_current_mode(mode, stderr);
    }
}

discon_modes_t
display_current_mode()
{
    discon_modes_t mode;

    mode = get_current_mode();
    if ((int) mode < 0) {
        if (errno == ENOSYS)
	    fprintf(stderr, "AFS is not running.\n");
	else
	    perror("pioctl");
    } else
	print_current_mode(mode,stdout);
    return mode;
}


print_current_mode(mode, stream)
discon_modes_t mode;
FILE *stream;
{
    fprintf(stream, "The cache manager is in %s mode\n", mode_to_string(mode));
}

char *mode_to_string(discon_modes_t mode)
{
    char *s;
    static char unknown[24];

    switch (mode) {
    case CONNECTED:
	s = "connected";
	break;
    case DISCONNECTED:
	s = "disconnected";
	break;
    case FETCH_ONLY:
	s = "fetch only";
	break;
    case PARTIALLY_CONNECTED:
	s = "partial";
	break;
    default:
	sprintf(unknown, "unknown (%d)", (int) mode);
	s = unknown;
	break;
    }
    return s;
}

discon_modes_t
get_current_mode() 
{
    discon_modes_t mode;
    long query;
    struct ViceIoctl blob;
    int code;

    query = AFS_DIS_QUERY;

    blob.in_size = sizeof(long);
    blob.in = (char *) &query;

    blob.out_size = sizeof(discon_modes_t);
    blob.out = (char *) &mode;

    code = pioctl(0, VIOCDISCONNECT, &blob, 1);

    if (code < 0)
	return (discon_modes_t) -1;
    return mode;
}

int
get_file_name(which, name)
dis_setopt_op_t which;
char *name;
{
    struct ViceIoctl blob;
    dis_setop_info_t info;

    info.op = which;

    blob.in_size = sizeof(dis_setop_info_t);
    blob.in = (char *) &info;
    blob.out_size = MAX_NAME;
    blob.out = (char *) name;

    return pioctl(0, VIOCSETDOPS, &blob, 1);
}

int
get_log_file_name(log_file_name)
char *log_file_name;
{
    return get_file_name(GET_LOG_NAME, log_file_name);
}

int
get_backup_file_name(backup_name)
char *backup_name;
{
    return get_file_name(GET_BACKUP_LOG_NAME, backup_name);
}

int
replay_op(log_ent)
        log_ent_t *log_ent;
{
        struct ViceIoctl blob;
        int code;
        int out_code;

        blob.in_size = log_ent->log_len;
        blob.in = (char *) log_ent;
        blob.out_size = sizeof(int);
        blob.out = (char *) &out_code;

        code = pioctl(0, VIOCREPLAYOP, &blob, 1);
        if (code) {
                perror("bad pioctl");
                return code;
        } else
                return (out_code);
}




void
set_log_flags(offset, flags)
	long offset;
	long flags;
{
        struct ViceIoctl blob;
        dis_setop_info_t info;
        int code;
        int out_code = 0;
	long *lptr;

        info.op = UPDATE_FLAGS;
	lptr = (long *) &info.data;
	lptr[0] = offset;
	lptr[1] = flags;
	
        blob.in_size = (short) sizeof(dis_setop_info_t);
        blob.in = (char *) &info;
        blob.out_size = (short) sizeof(int);
        blob.out = (char *) &out_code;

        code = pioctl(0, VIOCSETDOPS, &blob, 1);

        if (code) {
                perror("failed to merge log");
        }
}


askuser()
{
    int c;

    fprintf(stderr, ".  Continue? ");
    fflush(stderr);
    c = getc(stdin);
    if (c != '\n')
	while (!feof(stdin))
	    if (getc(stdin) == '\n')
		break;
    if (c == 'y' || c == 'Y')
	return 1;

    return 0;
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
