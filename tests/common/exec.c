/*
 * Copyright (c) 2020 Sine Nomine Associates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!
 * Common exec-related functions for testing programs
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <afs/opr.h>

#include <afs/opr.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <tests/tap/basic.h>

#include "common.h"

static void
afstest_command_start(struct afstest_cmdinfo *cmdinfo)
{
    int code;
    int pipefd[2];

    if (cmdinfo->fd == STDOUT_FILENO) {
	cmdinfo->fd_descr = "stdout";
    } else if (cmdinfo->fd == STDERR_FILENO) {
	cmdinfo->fd_descr = "stderr";
    } else {
	bail("unknown exec fd %d", cmdinfo->fd);
    }

    code = pipe(pipefd);
    if (code < 0) {
	sysbail("pipe");
    }

    cmdinfo->child = fork();
    if (cmdinfo->child < 0) {
	sysbail("fork");
    }

    if (cmdinfo->child == 0) {
	/* Child pid */
	char *cmd = "/bin/sh";
	char *argv[] = { "sh", "-c", cmdinfo->command, NULL };

	/* Redirect stdout/stderr to our pipe */
	code = dup2(pipefd[1], cmdinfo->fd);
	if (code < 0) {
	    sysdiag("dup2");
	    exit(1);
	}

	close(pipefd[0]);
	close(pipefd[1]);

	execv(cmd, argv);
	sysdiag("execv");
	exit(1);
    }

    /* Parent pid */

    close(pipefd[1]);

    cmdinfo->pipe_fh = fdopen(pipefd[0], "r");
    if (cmdinfo->pipe_fh == NULL) {
	sysbail("fdopen");
    }
}

static int
command_end_v(struct afstest_cmdinfo *cmdinfo, const char *format, va_list args)
{
    int code;
    int success;
    size_t buflen;
    char *buf = NULL;
    int nbytes;
    int status = -1;

    opr_Assert(cmdinfo->child != 0);

    /*
     * Read 1 byte more than the 'cmdinfo->output' string. If that's less than
     * 4095 bytes, read 4095 bytes (just so we read a decent amount of data, in
     * case we unexpectedly get a lot of data back).
     */
    buflen = strlen(cmdinfo->output)+1;
    if (buflen < 4095) {
	buflen = 4095;
    }
    buf = calloc(1, buflen+1);
    if (buf == NULL) {
	sysbail("calloc");
    }

    /*
     * Note that we must read the return value of fread() into something,
     * because it's declared with warn_unused_result in some glibc. We don't
     * actually care about the result, but pretend to care so we don't trigger
     * a warning.
     */
    nbytes = fread(buf, 1, buflen, cmdinfo->pipe_fh);
    if (ferror(cmdinfo->pipe_fh) != 0) {
	bail("error reading from %s pipe (read %d bytes)",
	     cmdinfo->fd_descr, nbytes);
    }
    fclose(cmdinfo->pipe_fh);
    cmdinfo->pipe_fh = NULL;

    code = waitpid(cmdinfo->child, &status, 0);
    if (code <= 0) {
	sysbail("waitpid");
    }

    success = 0;

    if (WIFEXITED(status) && WEXITSTATUS(status) != cmdinfo->exit_code) {
	int code = WEXITSTATUS(status);
	diag("command %s:", cmdinfo->command);
	diag("exit code wanted: %d", cmdinfo->exit_code);
	diag("  exit code seen: %d", code);
	diag(" ");
	diag("%s: %s", cmdinfo->fd_descr, buf);

    } else if (WIFSIGNALED(status)) {
	diag("command %s was killed by signal %d", cmdinfo->command,
	     WTERMSIG(status));

    } else if (!WIFEXITED(status)) {
	/* handle non-exited, non-signalled, just in case */
	diag("command %s died with weird status 0x%x", cmdinfo->command,
	     status);

    } else if (strcmp(cmdinfo->output, buf) != 0) {
	diag("command %s:", cmdinfo->command);
	diag("wanted %s: %s", cmdinfo->fd_descr, cmdinfo->output);
	diag("  seen %s: %s", cmdinfo->fd_descr, buf);

    } else {
	success = 1;
    }

    okv(success, format, args);

    cmdinfo->child = 0;
    free(buf);
    buf = NULL;

    return success;
}

/**
 * Test if running the given command matches the given output and exit code.
 *
 * @param[in] cmdinfo	Command to run, and expected output and exit code
 * @param[in] format	Message to print for the test (a la 'is_int' etc)
 * @return whether the test succeeded
 */
int
is_command(struct afstest_cmdinfo *cmdinfo, const char *format, ...)
{
    va_list args;
    int ret;

    afstest_command_start(cmdinfo);

    va_start(args, format);
    ret = command_end_v(cmdinfo, format, args);
    va_end(args);

    return ret;
}

static int
do_systemvp(char *command, char **argv)
{
    pid_t child;

    child = fork();
    if (child < 0) {
	sysbail("fork");
    }

    if (child > 0) {
	int status = -1;
	/* parent */
	if (waitpid(child, &status, 0) <= 0) {
	    sysbail("waitpid");
	}
	return status;
    }

    execvp(command, argv);
    sysbail("execvp");
    exit(1);
}

/*
 * This function is mostly the same as system(), in that the given command is
 * run with the same stdout, stderr, etc, and we return the exit status when it
 * finishes running. But instead of giving the command as a single string,
 * interpreted by the shell, the command is given as a list of arguments, like
 * execlp().
 *
 * For example, the following two are equivalent:
 *
 * code = afstest_systemlp("echo", "foo", "bar", (char*)NULL);
 * code = system("echo foo bar");
 *
 * Except that with afstest_systemlp, the arguments don't go through the shell,
 * so you don't need to worry about quoting arguments or escaping shell
 * metacharacters.
 */
int
afstest_systemlp(char *command, ...)
{
    va_list ap;
    int n_args = 1;
    int arg_i;
    int status;
    char **argv;

    va_start(ap, command);
    while (va_arg(ap, char*) != NULL) {
	n_args++;
    }
    va_end(ap);

    argv = calloc(n_args + 1, sizeof(argv[0]));
    opr_Assert(argv != NULL);

    argv[0] = command;

    va_start(ap, command);
    for (arg_i = 1; arg_i < n_args; arg_i++) {
	argv[arg_i] = va_arg(ap, char*);
	opr_Assert(argv[arg_i] != NULL);
    }
    va_end(ap);

    /* Note that argv[n_args] is NULL, since we allocated an extra arg in
     * calloc(), above. */

    status = do_systemvp(command, argv);
    free(argv);

    return status;
}
