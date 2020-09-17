#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <rx/rx.h>

#include <afs/authcon.h>
#include <afs/cellconfig.h>
#define UBIK_INTERNALS
#include <ubik.h>

#include <tests/tap/basic.h>

#include "common.h"

static void
check_startup(pid_t pid, char *log, int *a_started, int *a_stopped)
{
    int status;
    struct rx_connection *conn;
    struct ubik_debug udebug;
    afs_int32 isclone;
    pid_t exited;
    int code;

    memset(&udebug, 0, sizeof(udebug));

    opr_Assert(pid > 0);

    exited = waitpid(pid, &status, WNOHANG);
    if (exited < 0) {
	opr_Assert(errno == ECHILD);
    }

    if (exited != 0) {
	/* pid is no longer running; vlserver must have died during startup */
	*a_stopped = 1;
	return;
    }

    if (!afstest_file_contains(log, "Starting AFS vlserver")) {
	/* vlserver hasn't logged the "Starting AFS vlserver" line yet, so it's
	 * presumably still starting up. */
	return;
    }

    /*
     * If we're going to write to the db, we also need to wait until recovery
     * has the UBIK_RECHAVEDB state bit (without that, we won't be able to
     * start write transactions). If we're just reading from the db, we
     * wouldn't need to wait for this, but we don't know whether our caller
     * will be writing to the db or not. It shouldn't take long for
     * UBIK_RECHAVEDB to get set anyway, so just always check if it's been set
     * (via VOTE_XDebug).
     */

    conn = rx_NewConnection(afstest_MyHostAddr(), htons(7003), VOTE_SERVICE_ID,
			    rxnull_NewClientSecurityObject(), 0);
    code = VOTE_XDebug(conn, &udebug, &isclone);
    rx_DestroyConnection(conn);
    if (code != 0) {
	diag("VOTE_XDebug returned %d while waiting for vlserver startup",
	     code);
	return;
    }

    if (udebug.amSyncSite && (udebug.recoveryState & UBIK_RECHAVEDB) != 0) {
	/* Okay, it's set! We have finished startup. */
	*a_started = 1;
    }
}

/* Start up the VLserver, using the configuration in dirname, and putting our
 * logs there too.
 */

int
afstest_StartVLServer(char *dirname, pid_t *serverPid)
{
    pid_t pid;
    char *logPath;
    int started = 0;
    int stopped = 0;
    int try;
    FILE *fh;
    int code = 0;

    logPath = afstest_asprintf("%s/VLLog", dirname);

    /* Create/truncate the log in advance (since we look at it to detect when
     * the vlserver has started). */
    fh = fopen(logPath, "w");
    opr_Assert(fh != NULL);
    fclose(fh);

    pid = fork();
    if (pid == -1) {
	exit(1);
	/* Argggggghhhhh */
    } else if (pid == 0) {
	char *binPath, *dbPath;

	/* Child */
	binPath = afstest_obj_path("src/tvlserver/vlserver");
	dbPath = afstest_asprintf("%s/vldb", dirname);

	execl(binPath, "vlserver",
	      "-logfile", logPath, "-database", dbPath, "-config", dirname, NULL);
	fprintf(stderr, "Running %s failed\n", binPath);
	exit(1);
    }

    /*
     * Wait for the vlserver to startup. Try to check if the vlserver is ready
     * by checking the log file and the urecovery_state (check_startup()), but
     * if it's taking too long, just return success anyway. If the vlserver
     * isn't ready yet, then the caller's tests may fail, but we can't wait
     * forever.
     */

    diag("waiting for vlserver to startup");

    usleep(5000); /* 5ms */
    check_startup(pid, logPath, &started, &stopped);
    for (try = 0; !started && !stopped; try++) {
	if (try > 100 * 5) {
	    diag("waited too long for vlserver to finish starting up; "
		 "proceeding anyway");
	    goto done;
	}

	usleep(1000 * 10); /* 10ms */
	check_startup(pid, logPath, &started, &stopped);
    }

    if (stopped) {
	fprintf(stderr, "vlserver died during startup\n");
	code = -1;
	goto done;
    }

    diag("vlserver started after try %d", try);

 done:
    *serverPid = pid;

    free(logPath);

    return code;
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
