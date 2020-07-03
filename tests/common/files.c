/*
 * Copyright (c) 2020 Sine Nomine Associates. All rights reserved.
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
 * Common file-related functions for test programs
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <afs/opr.h>
#include <afs/afsutil.h>

#include <tests/tap/basic.h>

#include "common.h"

static char *
path_from_tdir(char *env_var, char *filename)
{
    char *tdir;

    /* C_TAP_SOURCE/C_TAP_BUILD in the env points to 'tests/' in the
     * srcdir/objdir. */

    tdir = getenv(env_var);
    if (tdir == NULL) {
	/*
	 * If C_TAP_SOURCE/C_TAP_BUILD isn't set, we assume we're running from
	 * the same cwd as one of the test programs (e.g. 'tests/foo/'). So to
	 * get to 'tests/', just go up one level.
	 */
	tdir = "..";
    }

    /*
     * The given 'filename' is specified relative to the top srcdir/objdir.
     * Since 'tdir' points to 'tests/', go up one level before following
     * 'filename'.
     */
    return afstest_asprintf("%s/../%s", tdir, filename);
}

char *
afstest_src_path(char *path)
{
    return path_from_tdir("C_TAP_SOURCE", path);
}

char *
afstest_obj_path(char *path)
{
    return path_from_tdir("C_TAP_BUILD", path);
}
