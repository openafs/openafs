/* This is a simple program which originally produced the KeyFile used
 * by the test suite. The contents of that file shouldn't be regenerated,
 * though, as the purpose of the tests using that file is to ensure that we
 * can still read old KeyFiles.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>

#include <roken.h>

int
main(int argc, char **argv)
{
    struct afsconf_dir *dir;
    char buffer[1024];
    char *block;
    char *dirEnd;
    FILE *file;
    int in, out;
    size_t len;
    int code;

    snprintf(buffer, sizeof(buffer), "%s/afs_XXXXXX", gettmpdir());
    afstest_mkdtemp(buffer);
    dirEnd = buffer + strlen(buffer);

    /* Create a CellServDB file */
    strcpy(dirEnd, "/CellServDB");
    file = fopen(buffer, "w");
    fprintf(file, ">example.org # An example cell\n");
    fprintf(file, "127.0.0.1 #test.example.org\n");
    fclose(file);

    /* Create a ThisCell file */
    strcpy(dirEnd, "/ThisCell");
    file = fopen(buffer, "w");
    fprintf(file, "example.org\n");
    fclose(file);

    *dirEnd='\0';
    dir = afsconf_Open(strdup(buffer));
    if (dir == NULL) {
	fprintf(stderr, "Unable to open configuration directory\n");
	exit(1);
    }

    afsconf_AddKey(dir, 1, "\x01\x02\x04\x08\x10\x20\x40\x80", 1);
    afsconf_AddKey(dir, 2, "\x04\x04\x04\x04\x04\x04\x04\x04", 1);
    afsconf_AddKey(dir, 4, "\x19\x16\xfe\xe6\xba\x77\x2f\xfd", 1);

    afsconf_Close(dir);

    /* Copy out the resulting keyfile into our homedirectory */
    strcpy(dirEnd, "/KeyFile");
    in = open(buffer, O_RDONLY);
    out = open("KeyFile", O_WRONLY | O_CREAT, 0644);

    block = malloc(1024);
    do {
	len = read(in, block, 1024);
	if (len > 0)
	    write(out, block, len);
    } while (len > 0);

    if (len == -1) {
	fprintf(stderr, "I/O error whilst copying file\n");
        exit(1);
    }

    close(in);
    close(out);

    strcpy(dirEnd, "/KeyFile");
    unlink(buffer);
    strcpy(dirEnd, "/CellServDB");
    unlink(buffer);
    strcpy(dirEnd, "/ThisCell");
    unlink(buffer);
    strcpy(dirEnd, "/UserList");
    unlink(buffer);
    *dirEnd='\0';
    rmdir(buffer);
}
