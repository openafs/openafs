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
#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <signal.h>
#include <time.h>

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/com_err.h>

#include <rx/rxkad.h>
#include <rx/rx_identity.h>

#include <hcrypto/des.h>
#include <tests/tap/basic.h>

#include "test.h"
#include "common.h"

#define TEST_PORT 1234

static void
testOriginalIterator(struct afsconf_dir *dir, int num, char *user) {
    char buffer[256];

    ok((afsconf_GetNthUser(dir, num, buffer, sizeof buffer) == 0),
       "User %d successfully returned as %s", num, buffer);

    ok(strcmp(user, buffer) == 0,
       "User %d matches", num);
}

static void
testNewIterator(struct afsconf_dir *dir, int num, struct rx_identity *id) {
    struct rx_identity *fileId;

    ok((afsconf_GetNthIdentity(dir, num, &fileId) == 0),
       "Identity %d successfully returned", num);

    ok(rx_identity_match(fileId, id), "Identity %d matches", num);

    rx_identity_free(&fileId);
}


void
startClient(char *configPath)
{
    struct afsconf_dir *dir;
    struct rx_identity *testId, *anotherId, *extendedId, *dummy;
    struct rx_securityClass *class;
    struct rx_connection *conn;
    afs_uint32 startTime;
    char ubuffer[256];
    afs_int32 classIndex;
    int code;
    struct hostent *he;
    afs_uint32 addr;
    afs_int32 result;
    char *string = NULL;

    plan(63);

    dir = afsconf_Open(configPath);
    ok(dir!=NULL,
       "Configuration directory opened sucessfully by client");

    /* Add a normal user to the super user file */
    ok(afsconf_AddUser(dir, "test") == 0,
       "Adding a simple user works");

    testId = rx_identity_new(RX_ID_KRB4, "test", "test", strlen("test"));

    /* Check that they are a super user */
    ok(afsconf_IsSuperIdentity(dir, testId),
       "User added with old i/face is identitifed as super user");

    /* Check that nobody else is */
    ok(!afsconf_IsSuperIdentity(dir,
			       rx_identity_new(RX_ID_KRB4, "testy",
					       "testy", strlen("testy"))),
       "Additional users are not super users");

    ok(afsconf_AddUser(dir, "test") == EEXIST,
       "Adding a user that already exists fails");

    ok(afsconf_AddIdentity(dir, testId) == EEXIST,
       "Adding an identity that already exists fails");

    anotherId = rx_identity_new(RX_ID_KRB4, "another",
					    "another", strlen("another"));

    /* Add another normal user, but using the extended interface */
    ok(afsconf_AddIdentity(dir, anotherId) == 0,
       "Adding a KRB4 identity works");

    /* Check that they are a super user */
    ok(afsconf_IsSuperIdentity(dir, anotherId),
       "User added with new i/face is identitifed as super user");

    ok(afsconf_AddIdentity(dir, anotherId) == EEXIST,
       "Adding a KRB4 identity that already exists fails");

    /* Add an extended user to the super user file */
    extendedId = rx_identity_new(RX_ID_GSS, "sxw@INF.ED.AC.UK",
				 "\x04\x01\x00\x0B\x06\x09\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x00\x00\x00\x10sxw@INF.ED.AC.UK", 35);

    ok(afsconf_AddIdentity(dir, extendedId) == 0,
       "Adding a GSSAPI identity works");

    /* Check that they are now special */
    ok(afsconf_IsSuperIdentity(dir, extendedId),
       "Added GSSAPI identity is a super user");

    /* Check that display name isn't used for matches */
    ok(!afsconf_IsSuperIdentity(dir,
				rx_identity_new(RX_ID_GSS, "sxw@INF.ED.AC.UK",
						"abcdefghijklmnopqrstuvwxyz123456789", 35)),
       "Display name is not used for extended matches");

    ok(afsconf_AddIdentity(dir, extendedId) == EEXIST,
       "Adding GSSAPI identity twice fails");

    /* Add a final normal user, so we can check that iteration works */
    /* Add a normal user to the super user file */
    ok(afsconf_AddUser(dir, "test2") == 0,
       "Adding another simple user works");

    testOriginalIterator(dir, 0, "test");
    testOriginalIterator(dir, 1, "another");
    testOriginalIterator(dir, 2, "test2");
    ok(afsconf_GetNthUser(dir, 3, ubuffer, sizeof ubuffer) != 0,
       "Reading past the end of the superuser list fails");

    testNewIterator(dir, 0, testId);
    testNewIterator(dir, 1, anotherId);
    testNewIterator(dir, 2, extendedId);
    testNewIterator(dir, 3, rx_identity_new(RX_ID_KRB4, "test2",
					    "test2", strlen("test2")));
    ok(afsconf_GetNthIdentity(dir, 4, &dummy) != 0,
       "Reading past the end of the superuser list fails");

    ok(afsconf_DeleteUser(dir, "notthere") != 0,
       "Deleting a user that doesn't exist fails");

    /* Delete the normal user */
    ok(afsconf_DeleteUser(dir, "another") == 0,
       "Deleting normal user works");

    ok(!afsconf_IsSuperIdentity(dir, anotherId),
       "Deleted user is no longer super user");

    ok(afsconf_IsSuperIdentity(dir, testId) &&
       afsconf_IsSuperIdentity(dir, extendedId),
       "Other identities still are");

    ok(afsconf_DeleteIdentity(dir, extendedId) == 0,
       "Deleting identity works");

    ok(!afsconf_IsSuperIdentity(dir, extendedId),
       "Deleted identity is no longer special");

    /* Now, what happens if we're doing something over the network instead */

    code = rx_Init(0);
    is_int(code, 0, "Initialised RX");

    /* Fake up an rx ticket. Note that this will be for the magic 'superuser' */
    code = afsconf_ClientAuth(dir, &class, &classIndex);
    is_int(code, 0, "Can successfully create superuser token");

    /* Start a connection to our test service with it */
    he = gethostbyname("localhost");
    if (!he) {
        printf("Couldn't look up server hostname");
        exit(1);
    }

    memcpy(&addr, he->h_addr, sizeof(afs_uint32));

    conn = rx_NewConnection(addr, htons(TEST_PORT), TEST_SERVICE_ID,
			    class, classIndex);

    /* There's nothing in the list, so this just succeeds because we can */
    code = TEST_CanI(conn, &result);
    is_int(0, code, "Can run a simple RPC");

    code = TEST_WhoAmI(conn, &string);
    is_int(0, code, "Can get identity back");
    is_string("<LocalAuth>", string, "Forged token is super user");

    xdr_free((xdrproc_t)xdr_string, &string);

    /* Throw away this connection and security class */
    rx_DestroyConnection(conn);
    rxs_Release(class);

    /* Now fake an rx ticket for a normal user. We have to do more work by hand
     * here, sadly */

    startTime = time(NULL);
    class = afstest_FakeRxkadClass(dir, "rpctest", "", "", startTime,
				   startTime + 60* 60);

    conn = rx_NewConnection(addr, htons(TEST_PORT), TEST_SERVICE_ID, class,
			    RX_SECIDX_KAD);

    code = TEST_CanI(conn, &result);
    is_int(EPERM, code,
	   "Running RPC as non-super user fails as expected");
    code = TEST_NewCanI(conn, &result);
    is_int(EPERM, code,
	   "Running new interface RPC as non-super user fails as expected");
    code = TEST_WhoAmI(conn, &string);
    xdr_free((xdrproc_t)xdr_string, &string);
    is_int(EPERM, code,
	   "Running RPC returning string fails as expected");
    code = TEST_NewWhoAmI(conn, &string);
    xdr_free((xdrproc_t)xdr_string, &string);
    is_int(EPERM, code,
	   "Running new interface RPC returning string fails as expected");
    ok(afsconf_AddUser(dir, "rpctest") == 0,
       "Adding %s user works", "rpctest");
    code = TEST_CanI(conn, &result);
    is_int(0, code, "Running RPC as rpctest works");
    code = TEST_NewCanI(conn, &result);
    is_int(0, code, "Running new interface RPC as rpctest works");
    code = TEST_WhoAmI(conn, &string);
    is_int(0, code, "Running RPC returning string as %s works", "rpctest");
    is_string("rpctest", string, "Returned user string matches");
    xdr_free((xdrproc_t)xdr_string, &string);
    code = TEST_NewWhoAmI(conn, &string);
    is_int(0, code, "Running new RPC returning string as %s works", "rpctest");
    is_string("rpctest", string, "Returned user string for new interface matches");
    xdr_free((xdrproc_t)xdr_string, &string);
    rx_DestroyConnection(conn);
    rxs_Release(class);

    /* Now try with an admin principal */
    startTime = time(NULL);
    class = afstest_FakeRxkadClass(dir, "rpctest", "admin", "", startTime,
				   startTime + 60* 60);

    conn = rx_NewConnection(addr, htons(TEST_PORT), TEST_SERVICE_ID, class,
			    RX_SECIDX_KAD);

    code = TEST_CanI(conn, &result);
    is_int(EPERM, code,
	   "Running RPC as non-super user fails as expected");
    code = TEST_NewCanI(conn, &result);
    is_int(EPERM, code,
	   "Running new interface RPC as non-super user fails as expected");
    code = TEST_WhoAmI(conn, &string);
    xdr_free((xdrproc_t)xdr_string, &string);
    is_int(EPERM, code,
	   "Running RPC returning string fails as expected");
    code = TEST_NewWhoAmI(conn, &string);
    xdr_free((xdrproc_t)xdr_string, &string);
    is_int(EPERM, code,
	   "Running new interface RPC returning string fails as expected");

    ok(afsconf_AddUser(dir, "rpctest.admin") == 0,
       "Adding %s user works", "rpctest.admin");

    code = TEST_CanI(conn, &result);
    is_int(0, code, "Running RPC as %s works", "rpctest/admin");
    code = TEST_NewCanI(conn, &result);
    is_int(0, code, "Running new interface RPC as %s works", "rpctest/admin");
    code = TEST_WhoAmI(conn, &string);
    is_int(0, code, "Running RPC returning string as %s works", "rpctest/admin");
    is_string("rpctest.admin", string, "Returned user string matches");
    xdr_free((xdrproc_t)xdr_string, &string);
    code = TEST_NewWhoAmI(conn, &string);
    is_int(0, code, "Running new interface RPC returning string as %s works",
	   "rpctest/admin");
    is_string("rpctest.admin", string,
	      "Returned user string from new interface matches");
    xdr_free((xdrproc_t)xdr_string, &string);

    rx_DestroyConnection(conn);
    rxs_Release(class);
}

/**********************************************************************
 * Server
 **********************************************************************/

struct afsconf_dir *globalDir;

int
STEST_CanI(struct rx_call *call, afs_int32 *result)
{
    *result = 0;
    if (!afsconf_SuperUser(globalDir, call, NULL)) {
	return EPERM;
    }
    return 0;
}

int
STEST_NewCanI(struct rx_call *call, afs_int32 *result)
{
    *result = 0;
    if (!afsconf_SuperIdentity(globalDir, call, NULL)) {
	return EPERM;
    }
    return 0;
}

int
STEST_WhoAmI(struct rx_call *call, char **result)
{
   char string[MAXKTCNAMELEN];

   if (!afsconf_SuperUser(globalDir, call, string)) {
	*result = strdup("");
	return EPERM;
   }
   *result = strdup(string);

   return 0;
}

int
STEST_NewWhoAmI(struct rx_call *call, char **result)
{
   struct rx_identity *id;

   if (!afsconf_SuperIdentity(globalDir, call, &id)) {
	*result = strdup("");
	return EPERM;
   }
   *result = strdup(id->displayName);

   return 0;
}

/*
 * Primitive replacement for using sigtimedwait(). Just see if 'signo' is
 * pending, and if it's not, wait 100ms and try again. Try this for
 * approximately as many times as it takes to wait for 'nsecs' seconds.
 */
static int
waitforsig(int signo, int nsecs)
{
    int nsleeps;

    for (nsleeps = 0; nsleeps < nsecs * 10; nsleeps++) {
	sigset_t set;
	int code;

	opr_Verify(sigpending(&set) == 0);
	if (sigismember(&set, signo)) {
	    return 0;
	}

	/* Sleep for 100ms */
	code = usleep(100000);
	opr_Assert(code == 0 || errno == EINTR);
    }

    return -1;
}

int main(int argc, char **argv)
{
    struct afsconf_dir *dir;
    char *dirname;
    int serverPid, clientPid, waited, stat;
    int ret = 0;
    sigset_t set;
    char *argv0 = afstest_GetProgname(argv);

    afstest_SkipTestsIfBadHostname();

    /* Start the client and the server if requested */

    if (argc == 3 ) {
        if (strcmp(argv[1], "-server") == 0) {
	    globalDir = afsconf_Open(argv[2]);
	    afstest_StartTestRPCService(argv[2], getppid(), TEST_PORT,
					TEST_SERVICE_ID, TEST_ExecuteRequest);
            exit(0);
        } else if (strcmp(argv[1], "-client") == 0) {
            startClient(argv[2]);
            exit(0);
        } else {
            printf("Bad option %s\n", argv[1]);
            exit(1);
        }
    }

    /* Otherwise, do the basic configuration, then start the client and
     * server */

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    opr_Verify(sigprocmask(SIG_BLOCK, &set, NULL) == 0);

    dirname = afstest_BuildTestConfig(NULL);

    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to configure directory.\n");
	ret = 1;
	goto out;
    }

    printf("Config directory is %s\n", dirname);
    serverPid = fork();
    if (serverPid == -1) {
        /* Bang */
    } else if (serverPid == 0) {
        execl(argv[0], argv[0], "-server", dirname, NULL);
	ret = 1;
	goto out;
    }

    /* Our server child pid will send us a SIGUSR1 when it's started listening
     * on its port. Wait for up to 5 seconds to get the USR1. */
    if (waitforsig(SIGUSR1, 5) != 0) {
	fprintf(stderr, "%s: Timed out waiting for SIGUSR1 from server child\n",
		argv0);
	kill(serverPid, SIGTERM);
	ret = 1;
	goto out;
    }

    clientPid = fork();
    if (clientPid == -1) {
        kill(serverPid, SIGTERM);
        waitpid(serverPid, &stat, 0);
	ret = 1;
	goto out;
    } else if (clientPid == 0) {
        execl(argv[0], argv[0], "-client", dirname, NULL);
    }

    do {
        waited = waitpid(0, &stat, 0);
    } while(waited == -1 && errno == EINTR);

    if (waited == serverPid) {
        kill(clientPid, SIGTERM);
    } else if (waited == clientPid) {
        kill(serverPid, SIGTERM);
    }
    waitpid(0, &stat, 0);

out:
    /* Client and server are both done, so cleanup after everything */
    afstest_rmdtemp(dirname);

    return ret;
}
