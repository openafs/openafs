/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * cfgafs -	load/configure the AFS kernel extension
 */
#include <afsconfig.h>
#include <afs/param.h>


#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/sysconfig.h>
#include <sys/uio.h>
#include <sys/ldr.h>
#include <setjmp.h>
#include <signal.h>

extern char *malloc(), *optarg;

extern int sysconfig(int cmd, void *arg, int len);

#include "AFS_component_version_number.c"

main(argc, argv)
     char **argv;
{
    int add, del;
    int c;
    int res;
    char *file;
    mid_t kmid;
    struct cfg_load cload;
    struct cfg_kmod cmod;
    FILE *fp;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    add = del = 0;

    while ((c = getopt(argc, argv, "a:d:")) != EOF) {
	switch (c) {
	case 'a':
	    add = 1;
	    file = optarg;
	    if (!file)
		usage();
	    break;

	case 'd':
	    del = 1;
	    file = optarg;
	    if (!file)
		usage();
	    break;

	default:
	    usage();
	    break;
	}
    }

    if (!add && !del)
	usage();

    if (add) {
	char *buf[1024];
	char PidFile[256];

	buf[0] = "execerror";
	buf[1] = "cfgafs";
	cload.path = file;
	res = sysconfig(SYS_KLOAD, &cload, sizeof(cload));
	if (res != 0) {
	    perror("SYS_KLOAD");
	    loadquery(L_GETMESSAGES, &buf[2], sizeof buf - 8);
	    execvp("/etc/execerror", buf);
	    exit(1);
	}

	cmod.kmid = cload.kmid;
	cmod.cmd = CFG_INIT;
	cmod.mdiptr = 0;
	cmod.mdilen = 0;

	res = sysconfig(SYS_CFGKMOD, &cmod, sizeof(cmod));
	if (res != 0) {
	    perror("SYS_CFGKMOD");
	    cload.kmid = cload.kmid;
	    sysconfig(SYS_KULOAD, &cload, sizeof(cload));
	    exit(1);
	}
#ifdef notdef
	printf("cfgafs -d 0x%x # to remove AFS\n", cload.kmid);
#endif
	strcpy(PidFile, file);
	strcat(PidFile, ".kmid");
	fp = fopen(PidFile, "w");
	if (fp) {
	    (void)fprintf(fp, "%d\n", cload.kmid);
	    (void)fclose(fp);
	} else {
	    printf("Can't open for write file %s (error=%d); ignored\n",
		   PidFile, errno);
	}
	exit(0);
    } else if (del) {
	char PidFile[256];

	strcpy(PidFile, file);
	strcat(PidFile, ".kmid");
	fp = fopen(PidFile, "r");
	if (!fp) {
	    printf("Can't read %s file (error=%d); aborting\n", PidFile,
		   errno);
	    exit(1);
	}
	(void)fscanf(fp, "%d\n", &kmid);
	(void)fclose(fp);
	unlink(PidFile);
	cmod.kmid = kmid;
	cmod.cmd = CFG_TERM;
	cmod.mdiptr = NULL;
	cmod.mdilen = 0;

	if (sysconfig(SYS_CFGKMOD, &cmod, sizeof(cmod)) == -1) {
	    perror("SYS_CFGKMOD");
	    exit(1);
	}

	cload.kmid = kmid;
	if (sysconfig(SYS_KULOAD, &cload, sizeof(cload)) == -1) {
	    perror("SYS_KULOAD");
	    exit(1);
	}
	exit(0);
    }
}

usage()
{

    fprintf(stderr, "usage: cfgafs [-a mod_file] [-d mod_file]\n");
    exit(1);
}
