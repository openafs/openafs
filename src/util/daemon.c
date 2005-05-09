#include <afsconfig.h>
#include <afs/param.h>

#ifndef HAVE_DAEMON

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

int daemon(int nochdir, int noclose)
{
	int err = -1;
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		goto out;
	} else if (pid) {
		exit(0);
	}

	err = setsid();
	if (err == -1) {
		goto out;
	}

	if (!nochdir) {
		err = chdir("/");
		if (err == -1) {
			goto out;
		}
	}

	err = -1;
	if (!noclose) {
		if (!freopen("/dev/null", "r", stdin)) {
			goto out;
		}

		if (!freopen("/dev/null", "w", stdout)) {
			goto out;
		}

		if (!freopen("/dev/null", "w", stderr)) {
			goto out;
		}
	}

	err = 0;

out:
	return(err);
}
#endif
