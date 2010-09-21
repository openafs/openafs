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

int
main(int argc, char **argv)
{
    int port = 0;
    /*
     * Initialize the AFS client
     */
    uafs_SetRxPort(port);

    uafs_Setup(NULL);
    uafs_ParseArgs(0, NULL);
    uafs_Run();

    uafs_RxServerProc();

    uafs_Shutdown();

    return 0;
}
