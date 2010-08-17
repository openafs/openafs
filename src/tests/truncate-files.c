/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <err.h>

static void do_dir(const char *);
static void repeat_dir(const char *);

static void
do_dir(const char *dirname)
{
    int ret;

    ret = chdir(dirname);
    if (ret < 0)
	err(1, "chdir %s", dirname);
    repeat_dir(dirname);
    ret = chdir("..");
    if (ret < 0)
	err(1, "chdir ..");
}

static void
read_and_truncate(const char *filename, const struct stat *sb)
{
    int fd;
    char *buf;
    int ret;
    struct stat sb2;

    buf = malloc(sb->st_size);
    if (buf == NULL)
	err(1, "malloc %lu", (unsigned long)sb->st_size);
    fd = open(filename, O_RDONLY);
    if (fd < 0)
	err(1, "open readonly %s", filename);
    ret = read(fd, buf, sb->st_size);
    if (ret < 0)
	err(1, "read %s", filename);
    if (ret != sb->st_size)
	errx(1, "short read from %s", filename);
    if (close(fd))
	err(1, "close reading %s", filename);
    fd = open(filename, O_WRONLY | O_TRUNC, 0);
    if (fd < 0)
	err(1, "open wronly-trunc %s", filename);
    ret = write(fd, buf, sb->st_size);
    if (ret < 0)
	err(1, "write %s", filename);
    if (ret != sb->st_size)
	errx(1, "short write %s", filename);
    if (close(fd))
	err(1, "close writing %s", filename);
    ret = lstat(filename, &sb2);
    if (ret < 0)
	err(1, "stat %s", filename);
    if (sb2.st_size != sb->st_size)
	errx(1, "wrong size after re-writing %s: %lu != %lu", filename,
	     (unsigned long)sb->st_size, (unsigned long)sb2.st_size);
    free(buf);
}

static void
repeat_dir(const char *dirname)
{
    DIR *dir;
    struct dirent *dp;

    dir = opendir(".");
    if (dir == NULL)
	err(1, "opendir %s", dirname);
    while ((dp = readdir(dir)) != NULL) {
	struct stat sb;
	int ret;

	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
	    continue;

	ret = lstat(dp->d_name, &sb);
	if (ret < 0)
	    err(1, "lstat %s", dp->d_name);
	if (S_ISDIR(sb.st_mode))
	    do_dir(dp->d_name);
	else if (S_ISREG(sb.st_mode))
	    read_and_truncate(dp->d_name, &sb);
    }
    closedir(dir);
}


int
main(int argc, char **argv)
{

    if (argc != 2)
	errx(1, "usage: %s directory", argv[0]);
    do_dir(argv[1]);
    return 0;
}
