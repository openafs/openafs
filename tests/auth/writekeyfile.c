/* This is a simple program which originally produced the KeyFile used
 * by the test suite. The contents of that file shouldn't be regenerated,
 * though, as the purpose of the tests using that file is to ensure that we
 * can still read old KeyFiles.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/opr.h>

#include <roken.h>

#include "common.h"

int
main(int argc, char **argv)
{
    struct afsconf_dir *dir;
    char *dirname;
    char *block;
    char *keyfile = NULL;
    int in, out;
    size_t len;
    struct afstest_configinfo bct;

    memset(&bct, 0, sizeof(bct));

    bct.skipkeys = 1;

    dirname = afstest_BuildTestConfig(&bct);
    if (dirname == NULL) {
	fprintf(stderr, "Unable to create tmp config dir\n");
	exit(1);
    }

    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to open configuration directory\n");
	exit(1);
    }

    afsconf_AddKey(dir, 1, "\x01\x02\x04\x08\x10\x20\x40\x80", 1);
    afsconf_AddKey(dir, 2, "\x04\x04\x04\x04\x04\x04\x04\x04", 1);
    afsconf_AddKey(dir, 4, "\x19\x16\xfe\xe6\xba\x77\x2f\xfd", 1);

    afsconf_Close(dir);

    /* Copy out the resulting keyfile into our homedirectory */
    keyfile = afstest_asprintf("%s/KeyFile", dirname);
    in = open(keyfile, O_RDONLY);
    out = open("KeyFile", O_WRONLY | O_CREAT, 0644);

    block = malloc(1024);
    do {
	len = read(in, block, 1024);
	if (len > 0) {
	    if (write(out, block, len) != len) {
		len = -1;
	    }
	}
    } while (len > 0);

    if (len == -1) {
	fprintf(stderr, "I/O error whilst copying file\n");
        exit(1);
    }

    close(in);
    close(out);

    afstest_rmdtemp(dirname);

    return 0;
}
