#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
/*#include <sys/inode.h>*/
#include <netinet/in.h>
#include "../libafs/afs/lock.h"
#include "stds.h"
#include "afsint.h" 

#define KERNEL

#include "afs.h"
#undef KERNEL
#include "discon.h"
#include "discon_log.h"

#define MAX_NAME  255
#define CELL_DIRTY   0x01

#define DEFAULT_NAME  "/var/log/DisconnectedLog"

extern char    *optarg;

/* forward function declarations */
void display_store();
void display_create();
void display_mkdir();
void display_remove();
void display_rmdir();
void display_rename();
void display_setattr();
void display_link();
void display_symlink();
void display_fsync();
void display_access();
void display_readdir();
void display_readlink();



main(argc, argv)
int argc;
char **argv;
{
    int fd, cc;
    FILE *fp;
    char *fname = NULL;
    int c;
    int vflag = 0, lflag = 0;
    int detailed = 0;
    log_ent_t *in_log;
    int recflag = 0;
    int recno;
    int current_rec = 0;

    char *bp;
    int len;
    log_ops_t op;

    while ((c = getopt(argc, argv, "?hdvf:r:l")) != EOF) {
	switch (c) {
	case 'v':
	    vflag++;
	    break;

	case 'd':
	    detailed++;
	    break;

	case 'f':
	    fname = optarg;
	    break;

	case 'l':
	    lflag++;
	    break;

	case 'r':
	    recflag = 1;
	    recno = atoi(optarg);
	    break;	

	case 'h': case '?':
	    printf("Options are: \n");
	    printf("\t -d print out detailed information \n");
	    printf("\t -f use a non-default file name \n");
	    printf("\t -l show current log file name \n");
	    printf("\t -r recno to print a specific record \n");
	    printf("\t -v include read-only ops\n");
	    printf("\t -h this help information \n");
	    printf("\t -? this help information \n");

	default:
	    printf("bad opt %c \n", c);
	    exit(1);
	}
    }

    if (!fname) {
#ifdef DOPIOCTL
	static char logfname[MAX_NAME];

	if (get_log_file_name(logfname)) {
	    fprintf(stderr, "scanlog: get_log_file_name failed\n");
	    exit(1);
	}
	fname = logfname;
#else
	fname = DEFAULT_NAME;
#endif
    }

    if (lflag)
	puts(fname);

    if ((fp = fopen(fname, "r")) == 0) {
	perror("failed to open log ");
	exit(1);
    } else if (!detailed) {
	printf("%-16s%10s%10s%10s\n", "   Operation", "Volume", 
	       "Vnode", "Unique");
    }

    in_log = (log_ent_t *) malloc(sizeof(log_ent_t));

    while(fread(in_log, sizeof (int), 1, fp)) {
	if (in_log->log_len < sizeof(*in_log) - sizeof(in_log->log_data) ||
	    in_log->log_len > sizeof(log_ent_t)) {
	    fprintf(stderr, "corrupt log entry, log_len %d\n", in_log->log_len);
	    exit(2);
	}
	len = in_log->log_len - sizeof(int);
	bp = (char *) in_log + sizeof(int);
	if (fread(bp, len, 1, fp) != 1) {
	    fprintf(stderr, "parse log: bad read\n");
	    break;
	}

	current_rec ++;

	if (recflag && (recno > current_rec))
	    continue;
	else if (recflag && (recno < current_rec))
	    break;

	display_op(in_log, detailed, vflag);
    }

    fclose(fp);
    exit(0);
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
