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

#include <sys/types.h>

#include <stdio.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <err.h>

const char *fn = "foobar";

static void
check_size(const char *fn, size_t sz, int paranoia)
{
    struct stat sb;

    if (paranoia)
	return;

    if (stat(fn, &sb) < 0)
	err(1, "stat");
    if (sb.st_size != sz)
	errx(1, "st_size mismatch %d != %d", (int)sb.st_size, (int)sz);
}

static void
check_size_read(int fd, size_t sz)
{
    off_t old_off;
    size_t sz2;
    void *buf;

    if ((old_off = lseek(fd, 0, SEEK_CUR)) < 0)
	err(1, "lseek");

    if (lseek(fd, 0, SEEK_SET) < 0)
	err(1, "lseek");
    buf = malloc(sz);
    if (buf == NULL)
	errx(1, "malloc");
    if ((sz2 = read(fd, buf, sz)) < 0)
	errx(1, "read");
    if (sz2 != sz)
	errx(1, "end before end: sz2 (%u) != sz (%u)", (unsigned)sz2,
	     (unsigned)sz);
    if ((sz2 = lseek(fd, 0, SEEK_END)) < 0)
	errx(1, "lseek");
    if (sz2 != sz)
	errx(1, "end past end: sz2 (%u) != sz (%u)", (unsigned)sz2,
	     (unsigned)sz);
    free(buf);
}


int
main(int argc, char **argv)
{
    int fd;

    fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
	err(1, "open1");
    if (write(fd, "kaka", 4) != 4)
	errx(1, "write1");
    check_size(fn, 4, 1);
    if (close(fd) < 0)
	err(1, "close1");

    check_size(fn, 4, 0);

    fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 644);
    if (fd < 0)
	err(1, "open2");
    check_size(fn, 0, 1);
    if (write(fd, "kaka", 4) != 4)
	errx(1, "write2");
    check_size(fn, 4, 1);
    check_size_read(fd, 4);
    if (close(fd) < 0)
	err(1, "close2");
    check_size(fn, 4, 1);

    unlink(fn);

    return 0;
}
