/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
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

/**
 * Test case iterator
 *
 * Iterate over an array of structs in a for loop.
 *
 * Example usage:
 *
 *     for (afstest_Scan(test_cases, tc, tc_i)) {
 *        ...
 *     }
 *
 * @param  test_cases  array of structs
 * @param  tc          current test case struct pointer
 * @param  tc_i        test case counter
 */
#define afstest_Scan(test_cases, tc, tc_i) \
    (tc_i) = 0, (tc) = (test_cases); \
    (tc_i) < sizeof(test_cases)/sizeof(*(test_cases)); \
    (tc_i)++, (tc)++

/* config.c */

struct afstest_configinfo {
    /* Skip adding keys to the created conf dir. */
    int skipkeys;

    /* Only add old single-DES keys to the conf dir. */
    int onlydeskeys;
};
extern char *afstest_BuildTestConfig(struct afstest_configinfo *info);

struct afsconf_dir;

/* exec.c */

struct afstest_cmdinfo {
    char *command;	/**< command to run (as given to e.g. system()) */
    int exit_code;	/**< expected exit code */
    const char *output; /**< expected command output */
    int fd;		/**< fd to read output from */

    /* The following fields are not to be set by the caller; they are set when
     * running the given command. */
    char *fd_descr;	/**< string description of 'fd' (e.g. "stdout") */
    pid_t child;	/**< child pid (after command has started) */
    FILE *pipe_fh;	/**< pipe from child (after started) */
};

extern int is_command(struct afstest_cmdinfo *cmdinfo,
		      const char *format, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 2, 3);
extern int afstest_systemlp(char *command, ...);

/* files.c */

extern char *afstest_mkdtemp(void);
extern void afstest_rmdtemp(char *path);
extern char *afstest_src_path(char *path);
extern char *afstest_obj_path(char *path);
extern int afstest_file_contains(char *path, char *target);

/* rxkad.c */

extern struct rx_securityClass
	*afstest_FakeRxkadClass(struct afsconf_dir *dir,
				char *name, char *instance, char *realm,
				afs_uint32 startTime, afs_uint32 endTime);
/* servers.c */

struct rx_call;
extern int afstest_StartVLServer(char *dirname, pid_t *serverPid);
extern int afstest_StopServer(pid_t serverPid);
extern int afstest_StartTestRPCService(const char *, pid_t, u_short, u_short,
				       afs_int32 (*proc)(struct rx_call *));

/* ubik.c */
struct ubik_client;
extern int afstest_GetUbikClient(struct afsconf_dir *dir, char *service,
				 int serviceId,
				 struct rx_securityClass *secClass,
				 int secIndex,
				 struct ubik_client **ubikClient);

/* network.c */
extern int afstest_IsLoopbackNetworkDefault(void);
extern int afstest_SkipTestsIfLoopbackNetIsDefault(void);
extern void afstest_SkipTestsIfBadHostname(void);
extern void afstest_SkipTestsIfServerRunning(char *name);
extern afs_uint32 afstest_MyHostAddr(void);

/* misc.c */
extern char *afstest_GetProgname(char **argv);
extern char *afstest_vasprintf(const char *fmt, va_list ap);
extern char *afstest_asprintf(const char *fmt, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);
extern int is_int64(afs_int64 left, afs_int64 right, const char *fmt, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 3, 4);
extern int is_uint64(afs_uint64 left, afs_uint64 right, const char *fmt, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 3, 4);
extern int is_int64_v(afs_int64 left, afs_int64 right, const char *fmt,
		      va_list ap);
extern int is_uint64_v(afs_uint64 left, afs_uint64 right, const char *fmt,
		       va_list ap);
