/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Get CM initialization parameters. */
#include <afsconfig.h>

RCSID
    ("$Header$");

#include <afs/param.h>
#include <stdio.h>
#include <netinet/in.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/cmd.h>
#include <fcntl.h>
#ifdef AFS_AIX41_ENV
#include <signal.h>
#endif

/*
 * if -file <path> is given, fetch initialization parameters via ioctl()
 * on that file
 *
 * otherwise, fetch initialization paramters via lpioctl()
 */



int
GetInitParamsCmd(struct cmd_syndesc *as, void *arock)
{
    struct cm_initparams cm_initParams;
    struct ViceIoctl blob;
    int code;
    int len;
    char *file = 0;
    int fd;

    if (as->parms[0].items) {
	file = as->parms[0].items->data;
    }

    if (file) {
	printf("ioctl test\n");
	fd = open(file, O_RDONLY, 0);
	if (fd < 0) {
	    perror("open");
	    exit(1);
	}
    } else {
	printf("lpioctl test\n");
    }

    blob.in = (char *)0;
    blob.in_size = 0;
    blob.out = (char *)&cm_initParams;
    blob.out_size = sizeof(struct cm_initparams);

    if (file) {
	code = ioctl(fd, VIOC_GETINITPARAMS, &blob);
	if (code < 0) {
	    perror("ioctl: Error getting CM initialization parameters");
	    exit(1);
	}
	close(fd);
    } else {
	code = lpioctl(NULL, VIOC_GETINITPARAMS, &blob, 0);
	if (code) {
	    perror("lpioctl: Error getting CM initialization parameters");
	    exit(1);
	}
    }

    printf("cm_initparams version: %d\n", cm_initParams.cmi_version);
    printf("Chunk Files: %d\n", cm_initParams.cmi_nChunkFiles);
    printf("Stat Caches: %d\n", cm_initParams.cmi_nStatCaches);
    printf("Data Caches: %d\n", cm_initParams.cmi_nDataCaches);
    printf("Volume Caches: %d\n", cm_initParams.cmi_nVolumeCaches);
    printf("First Chunk Size: %d\n", cm_initParams.cmi_firstChunkSize);
    printf("Other Chunk Size: %d\n", cm_initParams.cmi_otherChunkSize);
    printf("Initial Cache Size: %dK\n", cm_initParams.cmi_cacheSize);
    printf("CM Sets Time: %c\n", cm_initParams.cmi_setTime ? 'Y' : 'N');
    printf("Disk Based Cache: %c\n", cm_initParams.cmi_memCache ? 'N' : 'Y');

    exit(0);
}


main(ac, av)
     int ac;
     char **av;
{
    int code;
    struct cmd_syndesc *ts;

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

    ts = cmd_CreateSyntax(NULL, GetInitParamsCmd, NULL,
			  "Get CM initialization parameters");

    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "filename in AFS");
    code = cmd_Dispatch(ac, av);
    exit(code);
}
