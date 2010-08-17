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
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>

static char *
write_random_file(int fd, size_t sz)
{
    char *buf;
    int i, j;

    j = sz;
    if (j > 2048) {
	j = 2048;
    }
    buf = malloc(j);
    if (buf == NULL)
	err(1, "malloc %u", (unsigned)sz);

    for (i = 0; i < j; ++i) {
	buf[i] = rand();
    }
    while (sz > 0) {
	if (write(fd, buf, j) != j)
	    err(1, "write");

	sz -= j;
	j = sz;
	if (j > 2048)
	    j = 2048;
    }

    return 0;
}

int
main(int argc, char **argv)
{
    const char *file;
    size_t sz;
    int fd;

    if (argc != 3)
	errx(1, "usage: %s file size", argv[0]);

    file = argv[1];
    sz = atoi(argv[2]);

    srand(time(NULL));

    fd = open(file, O_RDWR | O_CREAT, 0755);
    if (fd < 0)
	err(1, "open %s", file);

    if (lseek(fd, 0, SEEK_SET) < 0)
	err(1, "lseek");
    write_random_file(fd, sz);

    close(fd);
    return 0;
}
