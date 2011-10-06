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


#include <afs/stds.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <afs/afsutil.h>
#include <fcntl.h>

/*
 * XXX CHANGE the following depedent stuff XXX
 */
#define	SENDMAIL	"/afs/cellname/fs/dev/localtools/dest/bin/rcs-sendmail"
/*
 * Replace it with  a bboard (i.e. transarc.bosserver.auto-reports)
 */
#define	RECIPIENT	"foo@cellname"

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    struct stat tstat;
    FILE *fin = stdin;
    char buf[BUFSIZ], *bufp, *bufp1, *typep, *cmd, *bp;
    afs_int32 code, c, fd, pflags = -1, len, core = 0;
    char comLine[60], coreName[40], name[40];
    afs_int32 pid = -1, rsCount = -1;
    afs_int32 procStarts = -1;
    afs_int32 errorCode = -1, errorSignal = -1, goal = -1;
    time_t procStartTime = -1, rsTime = -1, lastAnyExit = -1, lastErrorExit = -1;
    char *timeStamp;

    typep = (char *)malloc(50);
    cmd = (char *)malloc(50);
    bufp = bufp1 = (char *)malloc(1000);
    while (fgets(buf, sizeof(buf), fin)) {
	code = sscanf(buf, "%s %s\n", typep, cmd);
	if (code < 2) {
	    continue;
	}
	if (!strcmp(typep, "BEGIN") && !strcmp(cmd, "bnode_proc")) {
	    while (fgets(buf, sizeof(buf), fin)) {
		code = sscanf(buf, "%s %s\n", typep, cmd);
		if (code < 2) {
		    printf("**bnode_proc**: typed=%s, cmd=%s\n", typep, cmd);
		    break;
		}
		if (!strcmp(typep, "comLine:"))
		    strcpy(comLine, cmd);
		else if (!strcmp(typep, "coreName:"))
		    strcpy(coreName, cmd);
		else if (!strcmp(typep, "pid:"))
		    pid = atoi(cmd);
		else if (!strcmp(typep, "flags:"))
		    pflags = atoi(cmd);
		else if (!strcmp(typep, "END")) {
		    break;
		} else {
		    printf
			("Unexpected token %s in the bnode_proc (should be END)\n",
			 typep);
		    exit(1);
		}
	    }
	} else if (!strcmp(typep, "BEGIN") && !strcmp(cmd, "bnode")) {
	    while (fgets(buf, sizeof(buf), fin)) {
		code = sscanf(buf, "%s %s\n", typep, cmd);
		if (code < 2) {
		    printf("**bnode**: typed=%s, cmd=%s\n", typep, cmd);
		    break;
		}
		if (!strcmp(typep, "name:"))
		    strcpy(name, cmd);
		else if (!strcmp(typep, "rsTime:"))
		    rsTime = atoi(cmd);
		else if (!strcmp(typep, "rsCount:"))
		    rsCount = atoi(cmd);
		else if (!strcmp(typep, "procStartTime:"))
		    procStartTime = atoi(cmd);
		else if (!strcmp(typep, "procStarts:"))
		    procStarts = atoi(cmd);
		else if (!strcmp(typep, "lastAnyExit:"))
		    lastAnyExit = atoi(cmd);
		else if (!strcmp(typep, "lastErrorExit:"))
		    lastErrorExit = atoi(cmd);
		else if (!strcmp(typep, "errorCode:"))
		    errorCode = atoi(cmd);
		else if (!strcmp(typep, "errorSignal:"))
		    errorSignal = atoi(cmd);
/*
		else if (!strcmp(typep, "lastErrorName:"))
		    strcpy(lastErrorName, cmd);
*/
		else if (!strcmp(typep, "goal:"))
		    goal = atoi(cmd);
		else if (!strcmp(typep, "END")) {
		    break;
		} else {
		    printf
			("Unexpected token %s in the bnode (should be END)\n",
			 typep);
		    exit(1);
		}
	    }
	} else {
	    printf("Unexpected token %s (should be BEGIN)\n", typep);
	    exit(1);
	}
    }
    /*
     * Now make up the text for the post
     */
    sprintf(buf, "/tmp/snote.%d", getpid());
    fd = open(buf, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) {
	perror(buf);
	printf("Unable to create temp file, %s\n", buf);
	exit(1);
    }
    (void)sprintf(bufp, "Subject: Bosserver's automatic notification\n\n");
    bufp += strlen(bufp);
    (void)sprintf(bufp,
		  "AUTOMATIC NOTIFICATION EVENT FOR AFS SERVER INSTANCE %s\n\n",
		  name);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "Server Process id was: %d\n", pid);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "Server command line: %s\n", comLine);
    bufp += strlen(bufp);
    if (strcmp(coreName, "(null)"))
	core = 1;
    bp = comLine;
    strcpy(bp, AFSDIR_SERVER_CORELOG_FILEPATH);
    if (core) {
	strcat(bp, coreName);
	strcat(bp, ".");
    }
    strcat(bp, name);
    if ((code = stat(bp, &tstat)) == 0) {
	c = 1;
	if ((lastAnyExit - tstat.st_ctime) > 300)	/* > 5 mins old */
	    c = 0;
	core = 1;
    } else
	core = 0;
    strcat(bp, " ");
    (void)sprintf(bufp, "There is %score dump left %sfor this server\n",
		  (core ? (c ? "a recent " : "an 'old' ") : "no "),
		  (core ? bp : ""));
    bufp += strlen(bufp);
    if (pflags == 1)
	strcpy(bp, "PROCESS STARTED");
    else if (pflags == 2)
	strcpy(bp, "PROCESS EXITED");
    else
	strcpy(bp, "UNKNOWN");
    (void)sprintf(bufp, "Process state %d (%s)\n", pflags, bp);
    bufp += strlen(bufp);
    timeStamp = ctime(&rsTime);
    timeStamp[24] = 0;
    (void)sprintf(bufp, "\nNumber of restarts since %s is %d\n", timeStamp,
		  rsCount);
    bufp += strlen(bufp);
    if (procStartTime) {
	timeStamp = ctime(&procStartTime);
	timeStamp[24] = 0;
	(void)sprintf(bufp,
		      "Number of process restarts since the process started %s is %d\n",
		      timeStamp, procStarts);
    }
    bufp += strlen(bufp);
    if (lastAnyExit) {
	timeStamp = ctime(&lastAnyExit);
	timeStamp[24] = 0;
	(void)sprintf(bufp, "Last time process exited for any reason: %s\n",
		      timeStamp);
    }
    bufp += strlen(bufp);
    if (lastErrorExit) {
	timeStamp = ctime(&lastErrorExit);
	timeStamp[24] = 0;
	(void)sprintf(bufp, "Last time process exited unexpectedly: %s\n",
		      timeStamp);
    }
    bufp += strlen(bufp);
    (void)sprintf(bufp, "Last exit return code %d\n", errorCode);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "Last process terminating signal %d\n", errorSignal);
    bufp += strlen(bufp);
    (void)sprintf(bufp, "The server is now %srunning\n",
		  (goal ? "" : "not "));
    bufp += strlen(bufp);
    len = (int)(bufp - bufp1);
    if (write(fd, bufp1, len) < 0) {
	perror("Write");
	exit(1);
    }
    close(fd);
    /*
     * Send the mail out
     */
    sprintf(bufp1, "%s %s -s TESTING < %s", SENDMAIL, RECIPIENT, buf);
    code = system(bufp1);
    unlink(buf);
    exit(0);
}
