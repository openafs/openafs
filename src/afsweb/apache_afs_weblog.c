/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* the wrapper nanny process around weblog that would restart it in case it 
 * failed and hopefully log the error somewhere! The need for this nanny 
 * process arises because the pipe descriptors need to be maintained. The 
 * process catches SIGTERM and kills the weblog process and exits. 
 * The following line needs to precede the kill -TERM line for httpd.pid
 * in the stopd script to stop the server
          kill -TERM `<cat /local/stronghold/apache/logs/httpd.pid>.afs`
 * or whatever the pid file 
 */

#include <afs/stds.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "weblog_errors.h"
#include "AFS_component_version_number.c"

#define WEBLOG_BIN "weblog"

/* TODO - set restart_attempts to 0 if not within certain time period */
#define MAX_RESTARTS        3	/* after this many restarts just die */
static int restart_attempts = 0;

int error_fd;			/* error file descriptor */

void
sig_term()
{
#ifdef AIX
    pid_t pgrp = getpgrp();
#else
    pid_t pgrp = getpgrp(0);
#endif
    int n;

#ifdef DEBUG
    write(2, "weblog_starter: caught SIGTERM, shutting down", 43);
#endif

    log_error("weblog_starter: caught SIGTERM, shutting down");

    if (pgrp == (pid_t) - 1) {
	log_error("getpgrp failed - kill weblog manually");
	write(2, "getpgrp failed - kill weblog manually", 38);
	exit(1);
    }
    kill(-pgrp, SIGKILL);
    exit(1);
}

/* strip out this binary and replace it with the weblog binary's name */
getpath(char *this, char *path)
{
    char *temp = &this[0];
    int len = strlen(this);
    int pos = 0, i = 0, j = 0;
    char bin_name[] = WEBLOG_BIN;
    int len1 = strlen(bin_name);

    strcpy(path, this);
    temp += (len);
    pos = len;
    while (*temp != '/') {
	temp--;
	pos--;
    }
    for (i = (pos + 1); i < len; i++, j++) {
	path[i] = bin_name[j];
	if (j >= len1)
	    break;
    }
}

char *
get_time()
{
    time_t t;
    char *time_string;

    t = time(NULL);
    time_string = (char *)ctime(&t);
    time_string[strlen(time_string) - 1] = '\0';
    return (time_string);
}

log_error(char *msg)
{
    char err_msg[1024];

    sprintf(err_msg, "[%s] weblog:%s\n", get_time(), msg);
    write(error_fd, (void *)err_msg, strlen(err_msg));
}


int
main(int argc, char **argv)
{
    pid_t weblog_pid;
    int stat = -1;
    int exitstatus = 0;
    char rn[] = "weblog_starter";
    char path[1024];
    char error_fname[1024];

    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = (void (*)())sig_term;
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
	perror("sigaction(SIGTERM)");
    }

    getpath(argv[1], path);
    strcpy(error_fname, argv[2]);

    error_fd = open(error_fname, O_WRONLY | O_APPEND | O_CREAT);
    if (error_fd < 0) {
	fprintf(stderr, "%s:Error opening log file:%s\nExiting\n", rn,
		error_fname);
	perror("open");
	exit(-1);
    }

    log_error("weblog_starter resuming normal operation");

    while (restart_attempts < MAX_RESTARTS) {
	/* fork the weblog process and wait till it exits */
	if ((weblog_pid = fork()) < 0) {
	    log_error("Could not fork process");
#ifdef DEBUG
	    perror("apache_afs_weblog:Could not fork process");
#endif
	    exit(-1);
	}
	switch (weblog_pid) {
	case 0:
	    /* the child process - in this case weblog */
	    execlp(path, "weblog", argv[3], argv[4], argv[5], argv[6], NULL);
#ifdef DEBUG
	    perror("apache_afs_weblog:Could not execute weblog");
#endif
	    exit(RESTARTERROR);

	default:
	    /* parent: just wait for the child */
	    weblog_pid = waitpid((pid_t) - 1, &stat, 0);

	    if (weblog_pid == -1) {
#ifdef DEBUG
		perror("apache_Afs_weblog: wait error");
#endif
		log_error("wait error");
		kill(getpid(), SIGTERM);
	    }

	    if (WIFEXITED(stat)) {
		exitstatus = WEXITSTATUS(stat);
		switch (exitstatus) {
		case 0:
#ifdef DEBUG
		    fprintf(stderr, "%s:No error ... restarting\n", rn);
#endif
		    break;

		case RESTART:
#ifdef DEBUG
		    fprintf(stderr, "%s:%s...Exiting\n", rn, RESTARTMSG);
#endif
		    log_error(RESTARTMSG);
		    exit(-1);

		case NULLARGS:
#ifdef DEBUG
		    fprintf(stderr, "%s:%s...Exiting\n", rn, NULLARGSMSG);
		    log_error(NULLARGSMSG);
#endif
		    exit(-1);

		case PIPESEND:
#ifdef DEBUG
		    fprintf(stderr, "%s:%s...Restarting\n", rn, PIPESENDMSG);
#endif
		    log_error(PIPESENDMSG);
		    break;

		case PIPEREAD:
#ifdef DEBUG
		    fprintf(stderr, "%s:%s...Exiting\n", rn, PIPEREADMSG);
#endif
		    log_error(PIPEREADMSG);
		    exit(-1);

		case PARSE:
#ifdef DEBUG
		    fprintf(stderr, "%s:%s...Exiting\n", rn, PARSEMSG);
#endif
		    log_error(PARSEMSG);
		    exit(-1);

		case KA:
#ifdef DEBUG
		    fprintf(stderr, "%s:%s...Exiting\n", rn, KAMSG);
#endif
		    log_error(KAMSG);
		    exit(-1);

		default:
#ifdef DEBUG
		    fprintf(stderr, "%s:Unknown error...Exiting\n", rn);
#endif
		    log_error("Unknown error");
		    exit(-1);
		}		/* switch (exitstatus) */
	    }
	    /* if weblog exited */
	    else {		/* weblog terminated abnormally */
		if (WIFSIGNALED(stat)) {
#ifdef DEBUG
		    fprintf(stderr,
			    "%s:The signal that terminated weblog:%d\n"
			    "Restarting weblog ...\n", rn, WTERMSIG(stat));
#endif
		}
#ifndef AIX
		else if (WCOREDUMP(stat)) {
#ifdef DEBUG
		    fprintf(stderr, "%s: weblog dumped core" "Exiting ...\n",
			    rn);
#endif
		    log_error("Core dump");
		    exit(-1);
		}
#endif
		else {
#ifdef DEBUG
		    fprintf(stderr,
			    "%s: weblog died under STRANGE circumstances..."
			    "restarting weblog\n", rn);
#endif
		}
	    }
	    break;
	}			/* switch(weblog_pid) */
	restart_attempts++;
    }				/* while */
}
