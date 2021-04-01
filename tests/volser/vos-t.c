#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <sys/wait.h>

#include <rx/rx.h>
#include <ubik.h>

#include <afs/com_err.h>
#include <afs/vldbint.h>
#include <afs/cellconfig.h>

#include <tests/tap/basic.h>

#include "common.h"

/* This checks for a bug in vos where it would fail to allocate additional
 * space for the results of multi homed VL_GetAddrsU, and so would segfault
 * if a host with a small number of addresses was immediately followed by
 * a host with a large number of addresses.
 */
void
TestListAddrs(struct ubik_client *client, char *dirname)
{
    int code;
    bulkaddrs addrs;
    char *vos;
    char *cmd = NULL;
    struct afstest_cmdinfo cmdinfo;

    afs_uint32 addrsA[] = {0x0a000000};
    afs_uint32 addrsB[] = {0x0a000001, 0x0a000002, 0x0a000003, 0x0a000004,
			   0x0a000005, 0x0a000006, 0x0a000007, 0x0a000008,
			   0x0a000009, 0x0a00000a, 0x0a00000b, 0x0a00000c,
			   0x0a00000d, 0x0a00000e, 0x0a00000f};
    char uuidA[] = {0x4F, 0x44, 0x94, 0x47, 0x76, 0xBA, 0x47, 0x2C, 0x97, 0x1A,
		    0x86, 0x6B, 0xC0, 0x10, 0x1A, 0x4B};
    char uuidB[] = {0x5D, 0x2A, 0x39, 0x36, 0x94, 0xB2, 0x48, 0x90, 0xA8, 0xD2,
		    0x7F, 0xBC, 0x1B, 0x29, 0xDA, 0x9B};
    char expecting[] = "10.0.0.0\n10.0.0.1\n10.0.0.2\n10.0.0.3\n10.0.0.4\n"
		       "10.0.0.5\n10.0.0.6\n10.0.0.7\n10.0.0.8\n10.0.0.9\n"
		       "10.0.0.10\n10.0.0.11\n10.0.0.12\n10.0.0.13\n"
		       "10.0.0.14\n10.0.0.15\n";

    addrs.bulkaddrs_len = 1;
    addrs.bulkaddrs_val = addrsA;
    code = ubik_VL_RegisterAddrs(client, 0, (afsUUID *)uuidA, 0, &addrs);
    is_int(0, code, "First address registration succeeds");

    addrs.bulkaddrs_len = 15;
    addrs.bulkaddrs_val = addrsB;
    code = ubik_VL_RegisterAddrs(client, 0, (afsUUID *)uuidB, 0, &addrs);
    is_int(0, code, "Second address registration succeeds");

    /* Now we need to run vos ListAddrs and see what happens ... */
    vos = afstest_obj_path("src/volser/vos");

    cmd = afstest_asprintf("'%s' listaddrs -config '%s' "
			   "-noauth -noresolve",
			   vos, dirname);

    memset(&cmdinfo, 0, sizeof(cmdinfo));
    cmdinfo.exit_code = 0;
    cmdinfo.output = expecting;
    cmdinfo.fd = STDOUT_FILENO;
    cmdinfo.command = cmd;

    is_command(&cmdinfo, "vos output matches");

    free(vos);
    free(cmd);
}

int
main(int argc, char **argv)
{
    char *dirname;
    struct afsconf_dir *dir;
    int code, secIndex;
    pid_t serverPid = 0;
    struct rx_securityClass *secClass;
    struct ubik_client *ubikClient = NULL;
    int ret = 0;
    char *argv0 = afstest_GetProgname(argv);

    /* Skip all tests if the current hostname can't be resolved */
    afstest_SkipTestsIfBadHostname();
    /* Skip all tests if the current hostname is on the loopback network */
    afstest_SkipTestsIfLoopbackNetIsDefault();
    /* Skip all tests if a vlserver is already running on this system. */
    afstest_SkipTestsIfServerRunning("afs3-vlserver");

    plan(6);

    code = rx_Init(0);

    dirname = afstest_BuildTestConfig(NULL);

    dir = afsconf_Open(dirname);

    code = afstest_StartVLServer(dirname, &serverPid);
    if (code) {
	afs_com_err(argv0, code, "while starting the vlserver");
	ret = 1;
	goto out;
    }

    code = afsconf_ClientAuthSecure(dir, &secClass, &secIndex);
    is_int(code, 0, "Successfully got security class");
    if (code) {
	afs_com_err(argv0, code, "while getting anonymous secClass");
	ret = 1;
	goto out;
    }

    code = afstest_GetUbikClient(dir, AFSCONF_VLDBSERVICE, USER_SERVICE_ID,
				 secClass, secIndex, &ubikClient);
    is_int(code, 0, "Successfully built ubik client structure");
    if (code) {
	afs_com_err(argv0, code, "while building ubik client");
	ret = 1;
	goto out;
    }

    TestListAddrs(ubikClient, dirname);

out:
    if (serverPid != 0) {
	code = afstest_StopServer(serverPid);
	is_int(0, code, "Server exited cleanly");
    }

    afstest_rmdtemp(dirname);
    return ret;
}
