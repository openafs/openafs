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

#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <rx/rx_identity.h>

#include <tap/basic.h>

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

int main(int argc, char **argv)
{
    struct afsconf_dir *dir;
    char buffer[1024];
    char ubuffer[256];
    char *dirEnd;
    FILE *file;
    struct rx_identity *testId, *anotherId, *extendedId, *dummy;

    plan(36);

    snprintf(buffer, sizeof(buffer), "%s/afs_XXXXXX", gettmpdir());
    mkdtemp(buffer);
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
    /* Start with a blank configuration directory */
    dir = afsconf_Open(strdup(buffer));
    ok(dir!=NULL,
       "Configuration directory opened sucessfully");

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

    strcpy(dirEnd, "/CellServDB");
    unlink(buffer);
    strcpy(dirEnd, "/ThisCell");
    unlink(buffer);
    strcpy(dirEnd, "/UserList");
    unlink(buffer);
    *dirEnd='\0';
    rmdir(buffer);

    return 0;
}
