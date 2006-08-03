/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This is a PAM module that calls setpag when creating a new session or
 * establishing credentials and then forks an external aklog program.
 * Eventually, it should call aklog inline without forking it.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <kopenafs.h>

/* This should really be set by the Makefile. */
#ifndef AKLOG_PATH
# define AKLOG_PATH "/usr/bin/aklog"
#endif

/* Warn about a problem. */
#define WARN(A, B) \
    syslog(LOG_WARNING, "pam_afs_session: %s: %s", (A), (B));

/* Log a debug message. */
#define DLOG(A, B) \
    if (debug)     \
        syslog(LOG_DEBUG, "pam_afs_session:%d: %s: %s", __LINE__, (A), (B))

/*
 * We store in the PAM environment whether aklog has already been run by
 * setting afs_aklog.  The value doesn't matter, but we need a pointer to use
 * as the data value.
 */
static int afs_aklog_flag;


/*
 * Given a cache template and a struct passwd, generate the cache environment
 * string and return it in newly allocated space.
 *
 * This is unfortunately necessary on HP-UX, since HP-UX's PAM implementation
 * apparently lacks any way to recover the KRB5CCNAME that's set in the PAM
 * environment; hence, this module has to be told how to reconstruct the
 * path.
 */
static char *
build_cache_env(const char *cache_template, struct passwd *pwd)
{
    char *cache, *out;
    const char *p;
    char scratch[BUFSIZ];
    size_t length;

    length = 0;
    for (p = cache_template; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            if (*p == 'u') {
                sprintf(scratch, "%ld", (long) pwd->pw_uid);
                length += strlen(scratch);
            } else if (*p == 'p') {
                sprintf(scratch, "%ld", (long) getpid());
                length += strlen(scratch);
            } else {
                length++;
            }
        } else {
            length++;
        }
    }
    cache = malloc(length + strlen("KRB5CCNAME=") + 1);
    if (cache == NULL)
        return NULL;
    strcpy(cache, "KRB5CCNAME=");
    out = cache + strlen(cache);
    for (p = cache_template, out = cache; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            if (*p == 'u') {
                sprintf(out, "%ld", (long) pwd->pw_uid);
                out += strlen(out);
            } else if (*p == 'p') {
                sprintf(out, "%ld", (long) getpid());
                out += strlen(out);
            } else {
                *out = *p;
                out++;
            }
        } else {
            *out = *p;
            out++;
        }
    }
    return cache;
}


/*
 * Call aklog with the appropriate environment.  Takes the PAM handle (so that
 * we can get the environment), the path to aklog, and the path to the ticket
 * cache (possibly a template).  Returns a PAM status value.
 */
static int
run_aklog(pam_handle_t *pamh, const char *aklog, const char *cache_template)
{
    int status, result;
    char *cache, scratch[BUFSIZ];
    char *username = NULL;
    char **env;
    struct passwd *pwd;
    pid_t child;

    DEBUG("run_aklog" "called");

    if (pam_get_item(pamh, PAM_USER, (void *) &username)) {
        status = PAM_SERVICE_ERR;
        goto done;
    }
    pwd = getpwnam(username);
    if (pwd == NULL) {
        status = PAM_USER_UNKNOWN;
        DEBUG("user unknown", username);
        goto done;
    }

    /*
     * HP-UX doesn't have pam_getenvlist, so for it we'll create a special
     * environment that contains only PATH and KRB5CCNAME.
     */
#ifdef __hpux
    env = malloc(sizeof(char *) * 3);
    if (env == NULL) {
        status = PAM_SERVICE_ERR;
        goto done;
    }
    if (cache_template == NULL)
        cache_template = "FILE:/tmp/krb5cc_%u";
    cache = build_cache_path(cache_template, pwd);
    if (cache == NULL) {
        status = PAM_SERVICE_ERR;
        goto done;
    }
    env[0] = cache;
    env[1] = getenv("PATH");
    env[2] = NULL;
#else
    env = pam_getenvlist(pamh);
#endif

    /* Fork off a copy of aklog. */
    child = fork();
    if (child < 0) {
        status = PAM_SERVICE_ERR;
        goto done;
    } else if (child == 0) {
        if (setuid(pwd->pw_uid) < 0) {
            WARN("cannot setuid to user", strerror(errno));
            exit(1);
        }
        execle(aklog, aklog, NULL, env);
        WARN("cannot exec aklog", strerror(errno));
        exit(1);
    }
    if (waitpid(child, &result, 0) && WIFEXITED(result))
        status = PAM_SUCCESS;
    else
        status = PAM_SESSION_ERR;

done:
    return status;
}


/*
 * Open a new session.  Create a new PAG with k_setpag and then fork the aklog
 * binary as the user.  A Kerberos v5 PAM module should have previously run to
 * obtain Kerberos tickets (or ticket forwarding should have already
 * happened).
 */
int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc,
                    const char *argv[])
{
    int i, status;
    int debug = 0, no_unlog = 0;
    int do_setpag = 1;
    const char *cache_template = NULL;
    const char *path = AKLOG_PATH;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "debug") == 0)
            debug++;
        else if (strncmp(argv[i], "aklog=", strlen("aklog=")) == 0)
            path = &argv[i][strlen("aklog=")];
        else if (strcmp(argv[i], "nopag") == 0)
            do_setpag = 0;
        else if (strcmp(argv[i], "no_unlog") == 0)
            no_unlog = 1;
#ifdef __hpux
        else if (strncmp(argv[i], "ccache=", strlen("ccache=")) == 0)
            cache_template = &argv[i][strlen("ccache=")];
#endif
        else
            WARN("unknown option", argv[i]);
    }

    if (!k_hasafs()) {
        WARN("skipping AFS session initialization", "AFS not running");
        return PAM_SUCCESS;
    }
    if (do_setpag && k_setpag() != 0) {
        WARN("setpag failed", strerror(errno));
        return PAM_SESSION_ERR;
    }
    status = run_aklog(pamh, path, cache_template);
    if (status != PAM_SUCCESS)
        status = PAM_SESSION_ERR;
    return status;
}


/*
 * Don't do anything for authenticate.  We're only an auth module so that we
 * can supply a pam_setcred implementation.
 */
int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
                    const char *argv[])
{
    return PAM_SUCCESS;
}


/*
 * Calling pam_setcred with PAM_ESTABLISH_CRED is equivalent to opening a new
 * session for our purposes.  With PAM_REFRESH_CRED, we don't call setpag,
 * just run aklog again.  PAM_DELETE_CRED calls unlog.
 */
int 
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
               const char *argv[])
{
    int debug = 0, no_unlog = 0;
    int do_setpag = 1;
    int i, status;
    const char *cache_template = NULL;
    const char *path = AKLOG_PATH;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "debug") == 0)
            debug++;
        else if (strncmp(argv[i], "aklog=", strlen("aklog=")) == 0)
            path = &argv[i][strlen("aklog=")];
        else if (strcmp(argv[i], "nopag")  == 0)
            do_setpag = 0;
        else if (strcmp(argv[i], "no_unlog") == 0)
            no_unlog = 1;
#ifdef __hpux
        else if (strncmp(argv[i], "ccache=", strlen("ccache=")) == 0)
            cache_template = &argv[i][strlen("ccache=")];
#endif
        else
            WARN("unknown option", argv[i]);
    }

    if (!k_hasafs()) {
        WARN("skipping AFS session initialization", "AFS not running");
        return PAM_SUCCESS;
    }
    if (flags & PAM_DELETE_CRED) {
        if (k_unlog() != 0) {
            WARN("unable to delete credentials", strerror(errno));
            return PAM_CRED_ERR;
        }
        return PAM_SUCCESS;
    }

    if (flags & (PAM_REINITIALIZE_CRED | PAM_REFRESH_CRED))
        do_setpag = 0;
    if (do_setpag && k_setpag() != 0) {
        WARN("setpag failed", strerror(errno));
        return PAM_CRED_ERR;
    }
    status = run_aklog(pamh, path, cache_template);
    if (status != PAM_SUCCESS)
        status = PAM_CRED_ERR;
    return status;
}


/*
 * Close a session.  Normally, what we do here is call unlog, but we can be
 * configured not to do so.
 */
int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc,
                     const char *argv[])
{
    int debug = 0, no_unlog = 0;
    int do_setpag = 1;
    int i;
    const char *cache_template = NULL;
    const char *path = AKLOG_PATH;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "debug") == 0)
            debug++;
        else if (strncmp(argv[i], "aklog=", strlen("aklog=")) == 0)
            path = &argv[i][strlen("aklog=")];
        else if (strcmp(argv[i], "nopag")  == 0)
            do_setpag = 0;
        else if (strcmp(argv[i], "no_unlog") == 0)
            no_unlog = 1;
#ifdef __hpux
        else if (strncmp(argv[i], "ccache=", strlen("ccache=")) == 0)
            cache_template = &argv[i][strlen("ccache=")];
#endif
        else
            WARN("unknown option", argv[i]);
    }

    if (!k_hasafs()) {
        WARN("skipping AFS session initialization", "AFS not running");
        return PAM_SUCCESS;
    }
    if (k_unlog() != 0) {
        WARN("unable to delete credentials", strerror(errno));
        return PAM_CRED_ERR;
    }
    return PAM_SUCCESS;
}
