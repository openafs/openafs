/*
 * Copyright 2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* exec-related helper routines */

#include <afsconfig.h>
#include <afs/param.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

/* helper for afs_exec_alt; just constructs the string for the 'alternate'
 * program */
static char *
construct_alt(const char *argv0, const char *prefix, const char *suffix)
{
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t replace_len;
    char *alt, *replace;

    alt = malloc(strlen(argv0) + prefix_len + suffix_len + 1);
    if (!alt) {
	return NULL;
    }
    strcpy(alt, argv0);

    replace = strrchr(alt, '/');
    if (replace) {
	replace++;
    } else {
	replace = alt;
    }

    replace_len = strlen(replace);

    memmove(replace + prefix_len, replace, replace_len);
    memcpy(replace, prefix, prefix_len);
    /* basically strcat(replace, suffix), but a touch faster */
    memcpy(replace + prefix_len + replace_len, suffix, suffix_len + 1);

    return alt;
}

/**
 * execute an alternate version of the running program.
 *
 * This takes the current argc/argv, and executes an "anternate" version
 * of the current program ("alternate" meaning e.g. DAFS/non-DAFS,
 * 32-bit/64-bit, etc). For example, if the current running program is
 * fssync-debug, running afs_exec_alt(argc, argc, "da", NULL) will try
 * to execute a "dafssync-debug" program, if it can be run.
 *
 * @param[in] argc  argc for the current program
 * @param[in] argv  argv for the current program
 * @param[in] prefix  prefix for the alternate program (or NULL)
 * @param[in] suffix  suffix for the alternate program (or NULL)
 *
 * @return the program that we failed to run, or NULL if we failed
 *         before constructing it. This string must be freed by the caller
 *
 * @pre at least one of 'prefix' or 'suffix' is non-NULL and non-empty
 *
 * @note if successful, never returns
 */
char *
afs_exec_alt(int argc, char **argv, const char *prefix, const char *suffix)
{
    char **targv = NULL;
    char *ret = NULL;
    int errno_save;
    int i = 0;

    if (!prefix) {
	prefix = "";
    }
    if (!suffix) {
	suffix = "";
    }

    if (prefix[0] == '\0' && suffix[0] == '\0') {
	/* we'd be exec'ing ourselves; that's no good */
	errno = EINVAL;
	return NULL;
    }

    targv = malloc(sizeof(char*) * (argc+1));
    if (!targv) {
	goto error;
    }

    targv[0] = construct_alt(argv[0], prefix, suffix);
    if (!targv[0]) {
	goto error;
    }

    for (i = 1; i < argc; i++) {
	targv[i] = strdup(argv[i]);
	if (!targv[i]) {
	    goto error;
	}
    }
    targv[i] = NULL;

    execvp(targv[0], targv);

 error:
    /* we only call free() beyond this point, which I don't think would ever
     * muck with errno, but I don't trust everyone's libc... */
    errno_save = errno;
    if (targv) {
	i--;
	for (; i >= 1; i--) {
	    free(targv[i]);
	}
	ret = targv[0];
	free(targv);
    }
    errno = errno_save;
    return ret;
}
