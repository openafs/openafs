/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Test the kopenafs setpag support.
 */

#include <errno.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
    int status;

    if (k_hasafs()) {
        printf("Running k_setpag\n");
        status = k_setpag();
        printf("Status: %d, errno: %d\n", status, errno);
        if (argc > 1) {
            argv++;
            execvp(argv[0], argv);
        }
    } else {
        printf("AFS apparently not running\n");
    }
}
