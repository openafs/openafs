/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * test readdir routines.
* Need to ensure that opendir succeeds if the directory is empty. To that
 * end report the NT error for any failure.
 * Basic tests to do:
 * 1) Read non-existent directory.
 * 2) Read empty <drive:>
 * 3) Try to read a file.
 */
#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#include <errno.h>
#include <dirent.h>

main(int ac, char **av)
{
    int i;
    DIR *dirp;
    struct dirent *direntp;
    int first;


    if (ac < 2) {
	printf("Usage: treaddir dir [dir ....]\n");
	return -1;
    }

    for (i = 1; i < ac; i++) {
	dirp = opendir(av[i]);
	if (!dirp) {
#ifdef AFS_NT40_ENV
	    printf("Can't open directory \"%s\", errno=%d, NT error=%d\n",
		   av[i], errno, GetLastError());
#else
	    printf("Can't open directory \"%s\", errno=%d\n", av[i], errno);
#endif
	    continue;
	}

	first = 1;
	errno = 0;
	while (direntp = readdir(dirp)) {
	    if (first) {
		first = 0;
		if (i > 1)
		    printf("\n");
	    }
	    printf("%s\n", direntp->d_name);
	}
#ifdef AFS_NT40_ENV
	if (errno) {
	    printf
		("readdir failed in directory %s with errno=%d, NT error=%d\n",
		 av[i], errno, GetLastError());
	}
#endif
	(void)closedir(dirp);
    }

    return 0;
}
