/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <err.h>

static int
creat_files(const char *dirname, int count)
{
    struct stat sb;
    int i;
    DIR *d;
    struct dirent *dp;

    if (mkdir(dirname, 0777) < 0)
	err(1, "mkdir %s", dirname);

    if (chdir(dirname) < 0)
	err(1, "chdir %s", dirname);
    if (stat(".", &sb) < 0)
	err(1, "stat .");
    for (i = 0; i < count; ++i) {
	char num[17];
	int fd;

	snprintf(num, sizeof(num), "%d", i);

	fd = open(num, O_CREAT | O_EXCL, 0777);
	if (fd < 0)
	    err(1, "open %s", num);
	if (close(fd) < 0)
	    err(1, "close %s", num);
    }
    if (stat(".", &sb) < 0)
	err(1, "stat .");

    d = opendir(".");
    if (d == NULL)
	err(1, "opendir .");
    while ((dp = readdir(d)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
	    continue;

	if (unlink(dp->d_name) < 0)
	    err(1, "unlink %s", dp->d_name);
    }
    closedir(d);
    if (chdir("..") < 0)
	err(1, "chdir ..");
    if (rmdir(dirname) < 0)
	err(1, "rmdir");
    return 0;
}

static void
usage(int ret)
{
    warnx("directory number-of-files");
    exit(ret);
}

int
main(int argc, char **argv)
{
    char *ptr;
    int count;


    if (argc != 3)
	usage(1);

    count = strtol(argv[2], &ptr, 0);
    if (count == 0 && ptr == argv[2])
	errx(1, "'%s' not a number", argv[2]);

    return creat_files(argv[1], count);
}
