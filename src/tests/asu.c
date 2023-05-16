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

#include <afsconfig.h>
#include <afs/param.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <grp.h>

#include <err.h>

static void
usage(int exit_val)
{
    fprintf(stderr, "asu user program [arguments ...]\n");
    exit(exit_val);
}

int
main(int argc, char **argv)
{
    const char *user, *prog;


    if (argc < 3)
	usage(1);

    user = argv[1];
    prog = argv[2];

    if (getuid() == 0) {
	struct passwd *pw;
	gid_t groups[1];
	uid_t uid;
	gid_t gid;

	pw = getpwnam(user);
	if (pw == NULL)
	    errx(1, "no such user %s", user);

	uid = pw->pw_uid;
	gid = pw->pw_gid;
	groups[0] = gid;

	if (setgroups(1, groups))
	    errx(1, "setgroups failed");

	if (setgid(gid) != 0) {
	    err(1, "setgid failed");
	}
	if (setuid(uid) != 0) {
	    err(1, "setuid failed");
	}
	if (setegid(gid) != 0) {
	    err(1, "setegid failed");
	}
	if (seteuid(uid) != 0) {
	    err(1, "seteuid failed");
	}
    }

    execvp(prog, &argv[2]);

    return 0;
}
