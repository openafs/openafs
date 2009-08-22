/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This program is NOT intended ever to be run.  Its sole purpose is to
 * test whether a program can link with libuafs.a.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <afs/sysincludes.h>
#include <rx/rx.h>
#include <afs_usrops.h>

void uafs_Shutdown(void);

int
main(int argc, char **argv)
{
    int port = 0;
    char *afsMount = 0;
    char *confDir = 0;
    char *cacheBaseDir = 0;
    long cacheBlocks = 0;
    long cacheFiles = 0;
    long cacheStatEntries = 0;
    long dCacheSize = 0;
    long vCacheSize = 0;
    long chunkSize = 0;
    long closeSynch = 0;
    long debug = 0;
    long nDaemons = 0;
    long memCache = 0;
    char *logFile = 0;

    /*
     * Initialize the AFS client
     */
    uafs_SetRxPort(port);

    uafs_Init("linktest", afsMount, confDir, cacheBaseDir, cacheBlocks,
	      cacheFiles, cacheStatEntries, dCacheSize, vCacheSize, chunkSize,
	      closeSynch, debug, nDaemons, memCache, logFile);

    uafs_RxServerProc();

    uafs_Shutdown();

    return 0;
}
