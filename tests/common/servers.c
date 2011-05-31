#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "common.h"

/* Start up the VLserver, using the configuration in dirname, and putting our
 * logs there too.
 */

int
afstest_StartVLServer(char *dirname, pid_t *serverPid)
{
    pid_t pid;

    pid = fork();
    if (pid == -1) {
	exit(1);
	/* Argggggghhhhh */
    } else if (pid == 0) {
	char *binPath, *logPath, *dbPath, *build;

	/* Child */
	build = getenv("BUILD");

	if (build == NULL)
	    build = "..";

	asprintf(&binPath, "%s/../src/tvlserver/vlserver", build);
	asprintf(&logPath, "%s/VLLog", dirname);
	asprintf(&dbPath, "%s/vldb", dirname);
	execl(binPath, "vlserver",
	      "-logfile", logPath, "-database", dbPath, "-config", dirname, NULL);
	fprintf(stderr, "Running %s failed\n", binPath);
	exit(1);
    }
    *serverPid = pid;

    return 0;
}

int
afstest_StopVLServer(pid_t serverPid)
{
    int status;

    kill(serverPid, SIGTERM);

    waitpid(serverPid, &status, 0);

    if (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM) {
	fprintf(stderr, "Server died exited on signal %d\n", WTERMSIG(status));
	return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
	fprintf(stderr, "Server exited with code %d\n", WEXITSTATUS(status));
	return -1;
    }
    return 0;
}
