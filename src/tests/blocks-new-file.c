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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <err.h>

static void
doit(const char *filename)
{
    int fd;
    struct stat sb;

    fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, 0666);
    if (fd < 0)
	err(1, "open %s", filename);
    if (lseek(fd, (off_t) (1024 * 1024), SEEK_SET) != 1024 * 1024)
	err(1, "lseek %s", filename);
    if (write(fd, "hej", 3) != 3)
	err(1, "write to %s", filename);
    if (close(fd) < 0)
	err(1, "close %s", filename);
    if (stat(filename, &sb) < 0)
	err(1, "stat %s", filename);
    if (unlink(filename) < 0)
	err(1, "unlink %s", filename);
    if (sb.st_blocks == 0)
	errx(1, "st_blocks == 0");
}

int
main(int argc, char **argv)
{

    if (argc == 1)
	doit("foo");
    else if (argc == 2)
	doit(argv[1]);
    else
	errx(1, "usage: %s [filename]", argv[0]);
    return 0;
}
