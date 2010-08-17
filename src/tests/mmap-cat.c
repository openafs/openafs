/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <err.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

static void
doit_mmap(int fd, struct stat *sb)
{
    void *mmap_buf;
    int ret;

    mmap_buf = mmap(NULL, sb->st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mmap_buf == (void *)MAP_FAILED)
	err(1, "mmap");
    ret = write(1, mmap_buf, sb->st_size);
    if (ret != sb->st_size)
	err(1, "write returned %d wanted to write %d", ret, (int)sb->st_size);
    munmap(mmap_buf, sb->st_size);
}


static void
doit_read(int fd, struct stat *sb)
{
    int ret;
    void *read_buf;

    read_buf = malloc(sb->st_size);
    if (read_buf == NULL)
	err(1, "malloc(%d)", (int)sb->st_size);
    ret = read(fd, read_buf, sb->st_size);
    if (ret != sb->st_size)
	err(1, "read returned %d wanted to write %d", ret, (int)sb->st_size);
    ret = write(1, read_buf, sb->st_size);
    if (ret != sb->st_size)
	err(1, "write returned %d wanted to write %d", ret, (int)sb->st_size);
    free(read_buf);
}

static void
doit(const char *filename, void (*func) (int, struct stat *))
{
    struct stat sb;
    int fd;
    int ret;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
	err(1, "open %s", filename);
    ret = fstat(fd, &sb);
    (*func) (fd, &sb);
    if (ret < 0)
	err(1, "stat %s", filename);
    close(fd);
}

static int read_flag;
static int mmap_flag;

static void
usage(int exit_val)
{
    fprintf(stderr, "mmap-cat [-m|-r] filename\n");
    exit(exit_val);
}

int
main(int argc, char **argv)
{
    if (argc != 2)
	usage(1);

    if (!strcmp(argv[1], "-m")) {
	mmap_flag++;
    } else if (!strcmp(argv[1], "-r")) {
	read_flag++;
    } else
	usage(1);

    if (read_flag && mmap_flag)
	errx(1, "can't do both mmap and read");

    if (read_flag)
	doit(argv[0], doit_read);
    if (mmap_flag)
	doit(argv[0], doit_mmap);

    return 0;
}
