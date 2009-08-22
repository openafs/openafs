/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    char *dirName;
    char tempName[1024];
    struct stat tstat;
    struct timeval tvp[2];
    int fd1;
    int code;
#ifndef HAVE_GETCWD
#define getcwd(x,y) getwd(x)
#endif

    /* venus system tester */
    if (argc != 2)
	return printf("usage: fulltest <dir-to-screw-up>\n");
    dirName = argv[1];
    mkdir(dirName, 0777);
    if (chdir(dirName) < 0) {
	perror("chdir");
	return -1;
    }
    if (getcwd(tempName, 1024) == 0) {
	printf("Could not get working dir.\n");
	return -1;
    }
    /* now create some files */
    fd1 = open("hi", O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd1 < 0) {
	perror("open1");
	return -1;
    }
    if (close(fd1) < 0) {
	perror("close1");
	return -1;
    }
    if (access("hi", 2) < 0) {
	printf("New file can not be written (access)\n");
	return -1;
   }
    if (chmod("hi", 0741) < 0) {
	perror("chmod1");
	return -1;
    }
    if (stat("hi", &tstat) < 0) {
	perror("stat1");
	return -1;
    }
    if ((tstat.st_mode & 0777) != 0741) {
	printf("chmod failed to set mode properly\n");
	return -1;
    }
    fd1 = open("hi", O_RDWR);
    if (fd1 < 0) {
	perror("open2");
	return -1;
    }
    if (fchmod(fd1, 0654) < 0) {
	perror("fchmod");
	return -1;
    }
    if (fstat(fd1, &tstat) < 0) {
	perror("fstat1");
	return -1;
    }
    if ((tstat.st_mode & 0777) != 0654) {
	printf("fchmod failed to set mode properly\n");
	return -1;
    }
#if 0
    /* These appear to be defunct routines;
     * I don't know what, if anything, replaced them */
    if (osi_ExclusiveLockNoBlock(fd1) < 0)
	{perror("flock1");return -1;}
    if (osi_UnLock(fd1) < 0)
	{perror("flock/unlock");return -1;}
#endif

/* How about shared lock portability? */
    {
	struct flock fl;

	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(fd1, F_SETLK, &fl) == -1) {
	    perror("fcntl1: RDLCK");
	    return -1;
	}

	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(fd1, F_SETLK, &fl) == -1) {
	    perror("fcntl2: UNLCK");
	    return -1;
	}

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(fd1, F_SETLK, &fl) == -1) {
	    perror("fcntl3: WRLCK");
	    return -1;
	}

	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(fd1, F_SETLK, &fl) == -1) {
	    perror("fcntl4: UNLCK");
	    return -1;
	}
    }

    if (fsync(fd1) < 0) {
	perror("fsync");
	return -1;
    }
    if (write(fd1, "hi\n", 3) != 3) {
	perror("write");
	return -1;
    }
    if (ftruncate(fd1, 2) < 0) {
	perror("ftruncate");
	return -1;
    }
    if (close(fd1) < 0) {
	perror("close2");
	return -1;
    }

    fd1 = open("hi", O_RDONLY);
    if (fd1 < 0) {
	perror("open3");
	return -1;
    }
    if (read(fd1, tempName, 100) != 2) {
	perror("read2");
	return -1;
    }
    if (close(fd1) < 0) {
	perror("close3");
	return -1;
    }

    if (link("hi", "bye") < 0) {
	perror("link");
	return -1;
    }
    if (stat("bye", &tstat) < 0) {
	perror("link/stat");
	return -1;
    }

    if (unlink("bye") < 0) {
	perror("unlink");
	return -1;
    }

    if (symlink("hi", "bye") < 0) {
	perror("symlink");
	return -1;
    }
    if (readlink("bye", tempName, 100) != 2) {
	perror("readlink");
	return -1;
    }
    if (strncmp(tempName, "hi", 2) != 0) {
	printf("readlink contents");
        return -1;
    }
    if (mkdir("tdir", 0777) < 0) {
	perror("mkdir");
	return -1;
    }
    fd1 = open("tdir/fdsa", O_CREAT | O_TRUNC, 0777);
    close(fd1);
    if (rmdir("tdir") == 0) {
	printf("removed non-empty dir\n");
	return -1;
    }
    if (unlink("tdir/fdsa") < 0) {
	perror("unlink tdir contents");
	return -1;
    }
    if (rmdir("tdir") < 0) {
	perror("rmdir");
	return -1;
    }

    fd1 = open(".", O_RDONLY);
    if (fd1 < 0) {
	perror("open dot");
	return -1;
    }
    if (read(fd1, tempName, 20) < 20)
	perror("read dir");
    close(fd1);

    fd1 = open("rotest", O_RDWR | O_CREAT, 0444);
    if (fd1 < 0) {
	perror("open ronly");
	return -1;
    }
    fchown(fd1, 1, -1);		/* don't check error code, may fail on Ultrix */
    code = write(fd1, "test", 4);
    if (code != 4) {
	printf("rotest short read (%d)\n", code);
	exit(1);
    }
    code = close(fd1);
    if (code) {
	perror("close ronly");
	return -1;
    }
    code = stat("rotest", &tstat);
    if (code < 0) {
	perror("stat ronly");
	return -1;
    }
    if (tstat.st_size != 4) {
	printf("rotest short close\n");
	exit(1);
    }
    if (unlink("rotest") < 0) {
	perror("rotest unlink");
	return -1;
    }

    if (rename("hi", "bye") < 0) {
	perror("rename1");
	return -1;
    }
    if (stat("bye", &tstat) < 0) {
	perror("rename target invisible\n");
	return -1;
    }
    if (stat("hi", &tstat) == 0) {
	printf("rename source still there\n");
	return -1;
    }

#ifndef	AFS_AIX_ENV
/* No truncate(2) on aix so the following are excluded */
    if (truncate("bye", 1) < 0) {
	perror("truncate");
	return -1;
    }
    if (stat("bye", &tstat) < 0) {
	perror("truncate zapped");
	return -1;
    }
    if (tstat.st_size != 1) {
	printf("truncate failed\n");
	return -1;
    }
#endif
    if (utimes("bye", tvp) < 0) {
	perror("utimes");
	return -1;
    }
    if (unlink("bye") < 0) {
	perror("unlink bye");
	return -1;
    }

    /* now finish up */
    chdir("..");
    rmdir(dirName);
    printf("Test completed successfully.\n");
    return 0;
}
