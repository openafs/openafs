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

#include <roken.h>

#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/cellconfig.h>

#include <tests/tap/basic.h>
#include "common.h"

extern int _afsconf_Touch(struct afsconf_dir *adir);

int verbose = 0;

#define LOCAL 1
#define FOREIGN 0
struct testcase {
    char *name;
    char *inst;
    char *cell;
    int expectedLocal;
};

/*
 * test set 0
 * cell: example.org
 */
struct testcase testset0[] = {
    {"jdoe", NULL, NULL, LOCAL},
    {"jdoe", NULL, "example.org", LOCAL},
    {"jdoe", NULL, "EXAMPLE.ORG", LOCAL},
    {"jdoe", NULL, NULL, LOCAL},
    {"jdoe", NULL, "my.realm.org", FOREIGN},
    {"jdoe", NULL, "MY.REALM.ORG", FOREIGN},
    {"jdoe", NULL, "MY.OTHER.REALM.ORG", FOREIGN},
    {"jdoe", "admin", NULL, LOCAL},
    {"jdoe", "admin", "my.realm.org", FOREIGN},
    {"jdoe", "admin", "MY.REALM.ORG", FOREIGN},
    {"jdoe", "admin", "your.realm.org", FOREIGN},
    {"jdoe", "admin", "YOUR.REALM.ORG", FOREIGN},
    {"admin", NULL, "example.org", LOCAL},
    {"admin", NULL, "my.realm.org", FOREIGN},
    {"admin", NULL, "MY.REALM.ORG", FOREIGN},
    {"admin", NULL, "MY.OTHER.REALM.ORG", FOREIGN},
    {NULL},
};

/*
 * test set 1
 * cell: example.org
 * local realms: MY.REALM.ORG, MY.OTHER.REALM.ORG
 */
struct testcase testset1[] = {
    {"jdoe", NULL, NULL, LOCAL},
    {"jdoe", NULL, "example.org", LOCAL},
    {"jdoe", NULL, "EXAMPLE.ORG", LOCAL},
    {"jdoe", NULL, NULL, LOCAL},
    {"jdoe", NULL, "my.realm.org", LOCAL},
    {"jdoe", NULL, "MY.REALM.ORG", LOCAL},
    {"jdoe", NULL, "MY.OTHER.REALM.ORG", LOCAL},
    {"jdoe", NULL, "SOME.REALM.ORG", FOREIGN},
    {"jdoe", "admin", NULL, LOCAL},
    {"jdoe", "admin", "my.realm.org", LOCAL},
    {"jdoe", "admin", "MY.REALM.ORG", LOCAL},
    {"jdoe", "admin", "MY.OTHER.REALM.ORG", LOCAL},
    {"jdoe", "admin", "your.realm.org", FOREIGN},
    {"jdoe", "admin", "YOUR.REALM.ORG", FOREIGN},
    {"admin", NULL, "example.org", LOCAL},
    {"admin", NULL, "my.realm.org", LOCAL},
    {"admin", NULL, "MY.REALM.ORG", LOCAL},
    {"admin", NULL, "MY.OTHER.REALM.ORG", LOCAL},
    {NULL},
};

/*
 * test set 2
 * cell: example.org
 * local realms: MY.REALM.ORG, MY.OTHER.REALM.ORG
 * exclude: admin@MY.REALM.ORG
 */
struct testcase testset2[] = {
    {"jdoe", NULL, NULL, LOCAL},
    {"jdoe", NULL, "example.org", LOCAL},
    {"jdoe", NULL, "EXAMPLE.ORG", LOCAL},
    {"jdoe", NULL, NULL, LOCAL},
    {"jdoe", NULL, "my.realm.org", LOCAL},
    {"jdoe", NULL, "MY.REALM.ORG", LOCAL},
    {"jdoe", NULL, "MY.OTHER.REALM.ORG", LOCAL},
    {"jdoe", "admin", NULL, LOCAL},
    {"jdoe", "admin", "my.realm.org", LOCAL},
    {"jdoe", "admin", "MY.REALM.ORG", LOCAL},
    {"jdoe", "admin", "MY.OTHER.REALM.ORG", LOCAL},
    {"jdoe", "admin", "your.realm.org", FOREIGN},
    {"jdoe", "admin", "YOUR.REALM.ORG", FOREIGN},
    {"admin", NULL, "example.org", LOCAL},
    {"admin", NULL, "my.realm.org", LOCAL},
    {"admin", NULL, "MY.REALM.ORG", FOREIGN},
    {"admin", NULL, "MY.OTHER.REALM.ORG", LOCAL},
    {NULL},
};

struct testcase* testset[] = { testset0, testset1, testset2 };

char *
make_string(int len)
{
    char *s = malloc(len + 1);
    if (!s) {
	fprintf(stderr, "Failed to allocate string buffer.\n");
	exit(1);
    }
    memset(s, 'x', len);
    s[len] = '\0';
    return s;
}

void
run_tests(struct afsconf_dir *dir, int setnum, char *setname)
{
    struct testcase *t;
    int code;

    for (t = testset[setnum]; t->name; t++) {
        afs_int32 local = -1;

	code = afsconf_IsLocalRealmMatch(dir, &local, t->name, t->inst, t->cell);
	ok(code == 0, "%s: test case %s/%s/%s",
           setname,
	   t->name ? t->name : "(null)",
           t->inst ? t->inst : "(null)",
	   t->cell ? t->cell : "(null)");
        if (code==0) {
	   ok(local == t->expectedLocal, "... expected %d, got %d", t->expectedLocal, local);
        }
    }
}

void
run_edge_tests(struct afsconf_dir *dir)
{
    afs_int32 local = -1;
    int code = 0;
    char *name = "jdoe";
    char *inst = "";
    char *cell = "";

    /* null argument checks */
    code = afsconf_IsLocalRealmMatch(dir, &local, NULL, inst, cell);
    ok(code == EINVAL, "null name: code=%d", code);

    code = afsconf_IsLocalRealmMatch(dir, &local, name, NULL, cell);
    ok(code == 0, "null inst: code=%d", code);

    code = afsconf_IsLocalRealmMatch(dir, &local, name, inst, NULL);
    ok(code == 0, "null cell: code=%d", code);

    /* large ticket test */
    name = make_string(64);
    inst = make_string(64);
    cell = make_string(64);
    code = afsconf_IsLocalRealmMatch(dir, &local, name, inst, cell);
    ok(code == 0, "name size 64: code=%d", code);
    free(name);
    free(inst);
    free(cell);

    name = make_string(255);
    inst = NULL;
    cell = "my.realm.org";
    code = afsconf_IsLocalRealmMatch(dir, &local, name, inst, cell);
    ok(code == 0, "name size 255: code=%d", code);
    free(name);
}

void
write_krb_conf(char *dirname, char *data)
{
    char *filename = NULL;
    FILE *fp;

    asnprintf(&filename, 256, "%s/%s", dirname, "krb.conf");
    if ((fp = fopen(filename, "w")) == NULL) {
	fprintf(stderr, "Unable to create test file %s\n", filename);
	exit(1);
    }
    if (verbose) {
	diag("writing to %s: %s", filename, data);
    }
    fprintf(fp, "%s\n", data);
    fclose(fp);
    free(filename);
}

void
write_krb_excl(char *dirname)
{
    char *filename = NULL;
    FILE *fp;
    char *data;

    asnprintf(&filename, 256, "%s/%s", dirname, "krb.excl");
    if ((fp = fopen(filename, "w")) == NULL) {
	fprintf(stderr, "Unable to create test file %s\n", filename);
	exit(1);
    }
    data = "admin@MY.REALM.ORG";
    if (verbose) {
	diag("writing to %s: %s", filename, data);
    }
    fprintf(fp, "%s\n", data);
    data = "admin@EXAMPLE.ORG";
    if (verbose) {
	diag("writing to %s: %s", filename, data);
    }
    fprintf(fp, "%s\n", data);
    fclose(fp);
    free(filename);
}

void
update_csdb(char *dirname)
{
    char *filename = NULL;
    FILE *fp;

    asnprintf(&filename, 256, "%s/%s", dirname, "CellServDB");
    if ((fp = fopen(filename, "a")) == NULL) {
	fprintf(stderr, "Unable to create test file %s\n", filename);
	exit(1);
    }
    fprintf(fp, "10.0.0.1 #bogus.example.org\n");
    fclose(fp);
    free(filename);
}

void
test_edges(void)
{
    struct afsconf_dir *dir;
    char *dirname;

    /* run edge case tests */
    dirname = afstest_BuildTestConfig(NULL);
    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to configure directory.\n");
	exit(1);
    }
    run_edge_tests(dir);
    afstest_rmdtemp(dirname);
}

void
test_no_config_files(void)
{
    struct afsconf_dir *dir;
    char *dirname;

    /* run tests without config files */
    dirname = afstest_BuildTestConfig(NULL);
    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to configure directory.\n");
	exit(1);
    }
    run_tests(dir, 0, "no config");
    afstest_rmdtemp(dirname);
}

void
test_with_config_files(void)
{
    struct afsconf_dir *dir;
    char *dirname;

    /* run tests with config files */
    dirname = afstest_BuildTestConfig(NULL);
    write_krb_conf(dirname, "MY.REALM.ORG MY.OTHER.REALM.ORG");
    write_krb_excl(dirname);
    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to configure directory.\n");
	exit(1);
    }
    run_tests(dir, 2, "config");
    afstest_rmdtemp(dirname);
}

void
test_set_local_realms(void)
{
    struct afsconf_dir *dir;
    char *dirname;

    /* Simulate command line -realm option; overrides config file, if one.
     * Multiple realms can be added. */
    ok(afsconf_SetLocalRealm("MY.REALM.ORG") == 0, "set local realm MY.REALM.ORG");
    ok(afsconf_SetLocalRealm("MY.OTHER.REALM.ORG") == 0, "set local realm MY.OTHER.REALM.ORG");

    /* run tests without config files */
    dirname = afstest_BuildTestConfig(NULL);
    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to configure directory.\n");
	exit(1);
    }
    write_krb_conf(dirname, "SOME.REALM.ORG");
    run_tests(dir, 1, "set realm test");
    afstest_rmdtemp(dirname);
}

void
test_update_config_files(void)
{
    int code;
    struct afsconf_dir *dir;
    char *dirname;
    afs_int32 local = -1;

    dirname = afstest_BuildTestConfig(NULL);
    write_krb_conf(dirname, "SOME.REALM.ORG");
    dir = afsconf_Open(dirname);
    if (dir == NULL) {
	fprintf(stderr, "Unable to configure directory.\n");
	exit(1);
    }

    code = afsconf_IsLocalRealmMatch(dir, &local, "jdoe", NULL, "SOME.REALM.ORG");
    ok(code == 0 && local == 1, "before update: jdoe@SOME.REALM.ORG");

    code = afsconf_IsLocalRealmMatch(dir, &local, "jdoe", NULL, "MY.REALM.ORG");
    ok(code == 0 && local == 0, "before update: admin@MY.REALM.ORG");

    write_krb_conf(dirname, "MY.REALM.ORG MY.OTHER.REALM.ORG");
    write_krb_excl(dirname);
    update_csdb(dirname);
    _afsconf_Touch(dir);	/* forces reopen */

    code = afsconf_IsLocalRealmMatch(dir, &local, "jdoe", NULL, "MY.REALM.ORG");
    ok(code == 0 && local == 1, "after update: jdoe@MY.REALM.ORG");

    code = afsconf_IsLocalRealmMatch(dir, &local, "admin", NULL, "MY.REALM.ORG");
    ok(code == 0 && local == 0, "after update: admin@MY.REALM.ORG");

    afstest_rmdtemp(dirname);
}

int
main(int argc, char **argv)
{
    afstest_SkipTestsIfBadHostname();

    plan(113);

    test_edges();
    test_no_config_files();
    test_with_config_files();
    test_update_config_files();
    test_set_local_realms(); /* must be the last test */

    return 0;
}
