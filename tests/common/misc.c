/*
 * Copyright (c) 2019 Sine Nomine Associates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!
 * Common misc functions for testing programs
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>
#include <tests/tap/basic.h>

#include "common.h"

char *
afstest_GetProgname(char **argv)
{
    char *argv0;

    /* For invocations like ./foo/bar/prog, strip out everything but the
     * trailing 'prog'. */
    argv0 = strrchr(argv[0], '/');
    if (argv0 != NULL) {
        argv0++;
        return argv0;
    }
    return argv[0];
}

char *
afstest_vasprintf(const char *fmt, va_list ap)
{
    char *str;
    if (vasprintf(&str, fmt, ap) < 0) {
	sysbail("vasprintf");
    }
    return str;
}

char *
afstest_asprintf(const char *fmt, ...)
{
    char *str;
    va_list ap;
    va_start(ap, fmt);
    str = afstest_vasprintf(fmt, ap);
    va_end(ap);
    return str;
}
