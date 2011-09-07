/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <limits.h>

#ifdef AFS_AIX51_ENV
#include <sys/cred.h>
#ifdef HAVE_SYS_PAG_H
#include <sys/pag.h>
#endif
#endif

#include <security/pam_appl.h>

#include <afs/auth.h>

#include "afs_util.h"


char *pam_afs_ident = "pam_afs";
char *pam_afs_lh = "OPENAFS_PAM_AFS_AUTH_login_handle";


void
lc_cleanup(pam_handle_t * pamh, void *data, int pam_end_status)
{
    if (data) {
	memset(data, 0, strlen(data));
	free(data);
    }
}


void
nil_cleanup(pam_handle_t * pamh, void *data, int pam_end_status)
{
    return;
}

/* converts string to integer */

char *
cv2string(char *ttp, unsigned long aval)
{
    char *tp = ttp;
    int i;
    int any = 0;

    *(--tp) = 0;
    while (aval != 0) {
	i = aval % 10;
	*(--tp) = '0' + i;
	aval /= 10;
	any = 1;
    }
    if (!any)
	*(--tp) = '0';
    return tp;
}

int
do_klog(const char *user, const char *password, const char *lifetime,
	const char *cell_name)
{
    pid_t pid;
    int pipedes[2];
    int status;
    char *argv[32];
    int argc = 0;
    char *klog_prog;
    int ret = 1;

#if defined(AFS_KERBEROS_ENV)
    klog_prog = KLOGKRB;
#else
    klog_prog = KLOG;
#endif
    if (access(klog_prog, X_OK) != 0) {
	syslog(LOG_ERR, "can not access klog program '%s'", KLOG);
	goto out;
    }
#if defined(AFS_KERBEROS_ENV)
    argv[argc++] = "klog.krb";

#else
    argv[argc++] = "klog";
#endif
    argv[argc++] = (char *)user;
    if (cell_name) {
	argv[argc++] = "-cell";
	argv[argc++] = (char *)cell_name;
    }
    argv[argc++] = "-silent";
    argv[argc++] = "-pipe";
    if (lifetime != NULL) {
	argv[argc++] = "-lifetime";
	argv[argc++] = (char *)lifetime;
    }
    argv[argc] = NULL;

    if (pipe(pipedes) != 0) {
	syslog(LOG_ERR, "can not open pipe: %s", strerror(errno));
	goto out;
    }
    pid = fork();
    switch (pid) {
    case (-1):			/* Error: fork failed */
	syslog(LOG_ERR, "fork failed: %s", strerror(errno));
	goto out;
    case (0):			/* child */
	close(0);
	dup(pipedes[0]);
	close(pipedes[0]);
	close(1);
	dup(pipedes[1]);
	close(pipedes[1]);
	execv(klog_prog, argv);
	/* notreached */
	syslog(LOG_ERR, "execv failed: %s", strerror(errno));
	close(0);
	close(1);
	goto out;
    default:
	write(pipedes[1], password, strlen(password));
	write(pipedes[1], "\n", 1);
	close(pipedes[0]);
	close(pipedes[1]);
	if (pid != wait(&status))
	    return (0);
	if (WIFEXITED(status)) {
	    ret = WEXITSTATUS(status);
	    goto out;
	}
	syslog(LOG_NOTICE, "%s for %s failed", klog_prog, user);
    }
  out:
    /*   syslog(LOG_DEBUG, "do_klog returns %d", ret); */
    return (ret);
}

/* Returns the AFS pag number, if any, otherwise return -1 */
afs_int32
getPAG(void)
{
    afs_int32 pag;

    pag = ktc_curpag();
    if (pag == 0 || pag == -1)
	return -1;

    /* high order byte is always 'A'; actual pag value is low 24 bits */
    return (pag & 0xFFFFFF);
}
