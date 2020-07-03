#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <rx/rx.h>

#include <afs/authcon.h>
#include <afs/cellconfig.h>

#include <tests/tap/basic.h>

#include "common.h"

/* Start up the VLserver, using the configuration in dirname, and putting our
 * logs there too.
 */

int
afstest_StartVLServer(char *dirname, pid_t *serverPid)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == -1) {
	exit(1);
	/* Argggggghhhhh */
    } else if (pid == 0) {
	char *binPath, *logPath, *dbPath;

	/* Child */
	binPath = afstest_obj_path("src/tvlserver/vlserver");
	logPath = afstest_asprintf("%s/VLLog", dirname);
	dbPath = afstest_asprintf("%s/vldb", dirname);

	execl(binPath, "vlserver",
	      "-logfile", logPath, "-database", dbPath, "-config", dirname, NULL);
	fprintf(stderr, "Running %s failed\n", binPath);
	exit(1);
    }

    if (waitpid(pid, &status, WNOHANG) != 0) {
	fprintf(stderr, "Error starting vlserver\n");
	return -1;
    }

    diag("Sleeping for a few seconds to let the vlserver startup");
    sleep(5);

    if (waitpid(pid, &status, WNOHANG) != 0) {
	fprintf(stderr, "vlserver died during startup\n");
	return -1;
    }

    *serverPid = pid;

    return 0;
}

int
afstest_StopServer(pid_t serverPid)
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

int
afstest_StartTestRPCService(const char *configPath,
			    pid_t signal_pid,
			    u_short port,
			    u_short serviceId,
			    afs_int32 (*proc) (struct rx_call *))
{
    struct afsconf_dir *dir;
    struct rx_securityClass **classes;
    afs_int32 numClasses;
    int code;
    struct rx_service *service;
    struct afsconf_bsso_info bsso;

    memset(&bsso, 0, sizeof(bsso));

    dir = afsconf_Open(configPath);
    if (dir == NULL) {
        fprintf(stderr, "Server: Unable to open config directory\n");
        return -1;
    }

    code = rx_Init(htons(port));
    if (code != 0) {
	fprintf(stderr, "Server: Unable to initialise RX\n");
	return -1;
    }

    if (signal_pid != 0) {
	kill(signal_pid, SIGUSR1);
    }

    bsso.dir = dir;
    afsconf_BuildServerSecurityObjects_int(&bsso, &classes, &numClasses);
    service = rx_NewService(0, serviceId, "test", classes, numClasses,
                            proc);
    if (service == NULL) {
        fprintf(stderr, "Server: Unable to start to test service\n");
        return -1;
    }

    rx_StartServer(1);

    return 0; /* Not reached, we donated ourselves to StartServer */
}
