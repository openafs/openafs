/*
 * Copyright 2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <afs/afsutil.h>
#include <tap/basic.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FAILSTR "exec test failure\n"
#define ARGSTRING "teststring"

static struct exec_test {
    const char *prefix; /* program prefix to run */
    const char *suffix; /* program suffix to run */
    const char *result; /* expected output from stdout from the child program,
                           or NULL if we should fail to run anything */
} tests[] = {
    { "exec-test1.",      NULL,          "exec test 1\n" },
    { NULL,               ".exec-test2", "exec test 2\n" },
    { "exec-test3.",      ".suffix",     "exec test 3\n" },
    { NULL,               NULL,          NULL },
    { "nonexistent",      NULL,          NULL },
};

static char*
create_child_script(const char *argv0, struct exec_test *test)
{
    char *script;
    char *dirc, *basec, *bname, *dname;
    const char *prefix, *suffix;
    int fd;
    FILE *fh;

    if (!test->result) {
	return NULL;
    }

    dirc = strdup(argv0);
    basec = strdup(argv0);
    script = malloc(PATH_MAX);

    if (!dirc || !basec || !script) {
	sysbail("malloc");
    }

    dname = dirname(dirc);
    bname = basename(basec);

    prefix = test->prefix;
    if (!prefix) {
	prefix = "";
    }
    suffix = test->suffix;
    if (!suffix) {
	suffix = "";
    }

    sprintf(script, "%s/%s%s%s", dname, prefix, bname, suffix);

    free(dirc);
    free(basec);

    fd = open(script, O_WRONLY | O_CREAT | O_EXCL, 0770);
    if (fd < 0) {
	sysbail("open(%s)", script);
    }

    fh = fdopen(fd, "w");
    if (!fh) {
	sysbail("fdopen");
    }

    fprintf(fh, "#!/bin/sh\n"
"if test x\"$1\" = x%s ; then\n"
"    echo %s\n"
"else\n"
"    exit 1\n"
"fi\n", ARGSTRING, test->result);

    fclose(fh);

    return script;
}

int
main(int argc, char **argv)
{
    int fds[2];
    pid_t pid;
    char *child_argv[3];
    int child_argc = 2;
    char buf[1024];
    int i;
    unsigned long nTests = sizeof(tests) / sizeof(tests[0]);

    if (argc != 1) {
	/* in case afs_exec_alt tries to run us again */
	exit(1);
    }

    child_argv[0] = argv[0];
    child_argv[1] = ARGSTRING;
    child_argv[2] = NULL;

    plan(nTests * 4);

    /*
     * Testing afs_exec_alt is a little interesting, since a successful run
     * means that we exec() someone else. Our general strategy is to fork and
     * run some generated shell scripts that just output a (known) string and
     * exit successfully. We have each of those scripts output something
     * different so we can make sure we're executing the right one.
     *
     * This isn't 100% foolproof, since afs_exec_alt could bug out and exec
     * some entirely different program, which happens to have the exact same
     * behavior as our shell scripts. But we've tried to make that unlikely.
     *
     * The shell scripts are passed exactly one argument: 'teststring'. All
     * they should do is see if that were given that argument, and if so,
     * they should print out the expected 'result' string to stdout, and
     * should exit with successful status.
     */

    for (i = 0; i < nTests; i++) {
	char *child_script;
	struct exec_test *t;

	t = &tests[i];

	if (pipe(fds)) {
	    sysbail("pipe");
	}

	child_script = create_child_script(argv[0], t);

	pid = fork();
	if (pid < 0) {
	    sysbail("fork");
	}

	if (pid == 0) {
	    char *prog;

	    if (close(fds[0])) {
		printf("child: close(%d) failed: %s", fds[0], strerror(errno));
	    }

	    /* child */
	    if (dup2(fds[1], 1) < 0) {
		printf("dup2: %s", strerror(errno));
		exit(1);
	    }

	    prog = afs_exec_alt(child_argc, child_argv, t->prefix, t->suffix);
	    if (t->result == NULL) {
		printf("%s", FAILSTR);
		exit(0);
	    }
	    printf("afs_exec_alt failed to exec %s: %s", prog, strerror(errno));
	    exit(1);

	} else {
	    /* parent */

	    ssize_t result_len, nBytes;
	    const char *result;
	    int status;

	    if (close(fds[1])) {
		sysdiag("parent: close(%d) failed", fds[1]);
	    }

	    result = t->result;
	    if (!result) {
		result = FAILSTR;
	    }

	    result_len = strlen(result);

	    nBytes = read(fds[0], buf, sizeof(buf)-1);
	    is_int(result_len, nBytes,
	           "child output size for prefix=%s, suffix=%s",
	           t->prefix, t->suffix);

	    if (nBytes < 0) {
		skip("read() failed; cannot test read buffer");
	    } else {
		/* just so is_string doesn't go running off into memory... */
		buf[nBytes] = '\0';

		is_string(result, buf,
		          "child output for prefix=%s, suffix=%s",
		          t->prefix, t->suffix);
	    }

	    if (close(fds[0])) {
		sysdiag("parent: close(%d) failed", fds[0]);
	    }

	    if (waitpid(pid, &status, 0) <= 0) {
		sysbail("waitpid");
	    }

	    if (child_script) {
		if (unlink(child_script)) {
		    sysdiag("unlink(%s)", child_script);
		}
		free(child_script);
	    }

	    ok(WIFEXITED(status), "child exited for prefix=%s, suffix=%s",
	                          t->prefix, t->suffix);

	    if (WIFEXITED(status)) {
		is_int(0, WEXITSTATUS(status),
		       "child exit code for prefix=%s, suffix=%s",
		       t->prefix, t->suffix);
	    } else {
		skip("!WIFEXITED(status) (status=%d), cannot check exit code",
		     status);
		if (WIFSIGNALED(status)) {
		    diag("terminated with signal %d", WTERMSIG(status));
		}
	    }
	}
    }
    return 0;
}
