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

/*!
 * Tests for the afsconf key handling functions
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/afsutil.h>
#include <rx/rxkad.h>

#include <tests/tap/basic.h>

#include "test.h"
#include "common.h"

static int
copy(char *inFile, char *outFile)
{
    int in, out;
    char *block, *block_out;
    ssize_t len;
    size_t len_out;

    in = open(inFile, O_RDONLY);
    if (in<0)
	return EIO;

    out = open(outFile, O_WRONLY | O_CREAT, 0600);
    if (out<0)
	return EIO;

    block = malloc(1024);
    do {
	len = read(in, block, 1024);
	if (len <= 0)
	    break;
	len_out = len;
	block_out = block;
	do {
	    len = write(out, block_out, len_out);
	    if (len <= 0)
		break;
	    block_out += len;
	    len_out -= len;
	} while (len_out > 0);
    } while (len > 0);
    free(block);

    close(in);
    close(out);

    if (len == -1)
	return EIO;

    return 0;
}

int
keyMatches(struct afsconf_typedKey *typedKey,
	   afsconf_keyType type, int kvno, int subType,
	   void *keyMaterial, size_t keyLen)
{
    afsconf_keyType keyType;
    int keyKvno;
    int keySubType;
    struct rx_opaque *buffer;

    afsconf_typedKey_values(typedKey, &keyType, &keyKvno, &keySubType,
			    &buffer);

    return (keyType == type && keyKvno == kvno && keySubType == subType &&
	    buffer->len == keyLen &&
	    memcmp(keyMaterial, buffer->val, buffer->len) == 0);
}

int main(int argc, char **argv)
{
    struct afsconf_dir *dir;
    struct afsconf_keys keys;
    struct ktc_encryptionKey key;
    struct rx_opaque *keyMaterial;
    struct afsconf_typedKey *typedKey;
    struct afsconf_typedKeyList *typedKeyList;
    struct afstest_configinfo bct;
    char *dirname;
    char *keyfile;
    char *keyfilesrc;
    afs_int32 kvno;
    int code;
    int i;

    memset(&bct, 0, sizeof(bct));

    afstest_SkipTestsIfBadHostname();

    plan(134);

    /* Create a temporary afs configuration directory */

    bct.skipkeys = 1;
    dirname = afstest_BuildTestConfig(&bct);

    keyfile = afstest_asprintf("%s/KeyFile", dirname);

    keyfilesrc = afstest_src_path("tests/auth/KeyFile");

    /* First, copy in a known keyfile */
    code = copy(keyfilesrc, keyfile);
    free(keyfilesrc);
    if (code)
	goto out;

    /* Start with a blank configuration directory */
    dir = afsconf_Open(dirname);
    ok(dir != NULL, "Sucessfully re-opened config directory");
    if (dir == NULL)
	goto out;

    /* Verify that GetKeys returns the entire set of keys correctly */
    code = afsconf_GetKeys(dir, &keys);
    is_int(0, code, "afsconf_GetKeys returns successfully");
    is_int(3, keys.nkeys, "... and returns the right number of keys");
    is_int(1, keys.key[0].kvno, " ... first key number is correct");
    is_int(2, keys.key[1].kvno, " ... second key number is correct");
    is_int(4, keys.key[2].kvno, " ... third key number is correct");
    ok(memcmp(keys.key[0].key, "\x01\x02\x04\x08\x10\x20\x40\x80", 8) == 0,
       " ... first key matches");
    ok(memcmp(keys.key[1].key, "\x04\x04\x04\x04\x04\x04\x04\x04", 8) == 0,
       " ... second key matches");
    ok(memcmp(keys.key[2].key, "\x19\x16\xfe\xe6\xba\x77\x2f\xfd", 8) == 0,
       " ... third key matches");

    /* Verify that GetLatestKey returns the newest key */
    code = afsconf_GetLatestKey(dir, &kvno, &key);
    is_int(0, code, "afsconf_GetLatestKey returns sucessfully");
    is_int(4, kvno, " ... with correct key number");
    ok(memcmp(&key, "\x19\x16\xfe\xe6\xba\x77\x2f\xfd", 8) == 0,
       " ... and correct key");

    /* Check that GetLatestKey works if called with NULL parameters */
    code = afsconf_GetLatestKey(dir, NULL, NULL);
    is_int(0, code, "afsconf_GetLatestKey works if parameters are NULL");

    /* Verify that random access using GetKey works properly */
    code = afsconf_GetKey(dir, 2, &key);
    is_int(0, code, "afsconf_GetKey returns successfully");
    ok(memcmp(&key, "\x04\x04\x04\x04\x04\x04\x04\x04", 8) == 0,
       " ... and with correct key");

    /* And that it fails if the key number doesn't exist */
    code = afsconf_GetKey(dir, 3, &key);
    is_int(code, AFSCONF_NOTFOUND,
	   "afsconf_GetKey returns not found for missing key");

    /* Check that AddKey can be used to add a new 'newest' key */
    code = afsconf_AddKey(dir, 5, "\x08\x08\x08\x08\x08\x08\x08\x08", 0);
    is_int(0, code, "afsconf_AddKey sucessfully adds a new key");

    /* And that we can get it back with GetKeys, GetLatestKey and GetKey */
    code = afsconf_GetKeys(dir, &keys);
    is_int(0, code, " ... and GetKeys still works");
    is_int(4, keys.nkeys, "... and has the correct number of keys");
    is_int(5, keys.key[3].kvno, " ... and the fourth key has the correct kvno");
    ok(memcmp(keys.key[3].key, "\x08\x08\x08\x08\x08\x08\x08\x08", 8) == 0,
       " ... and is the correct key");

    code = afsconf_GetLatestKey(dir, &kvno, &key);
    is_int(0, code, " ... and GetLatestKey returns successfully");
    is_int(5, kvno, " ... with the correct key number");
    ok(memcmp(&key, "\x08\x08\x08\x08\x08\x08\x08\x08", 8) == 0,
       " ... and the correct key");

    code = afsconf_GetKey(dir, 5, &key);
    is_int(0, code, " ... and GetKey still works");
    ok(memcmp(&key, "\x08\x08\x08\x08\x08\x08\x08\x08", 8) == 0,
       " ... and returns the correct key");

    /* Check that AddKey without the overwrite flag won't overwrite an existing
     * key */
    code = afsconf_AddKey(dir, 5, "\x10\x10\x10\x10\x10\x10\x10", 0);
    is_int(AFSCONF_KEYINUSE, code, "AddKey won't overwrite without being told to");

    /* Check with GetKey that it didn't */
    code = afsconf_GetKey(dir, 5, &key);
    is_int(0, code, " ... and GetKey still works");
    ok(memcmp(&key, "\x08\x08\x08\x08\x08\x08\x08\x08", 8) == 0,
       " ... and key hasn't been overwritten");

    /* Check that AddKey with the overwrite flag will overwrite an existing key */
    code = afsconf_AddKey(dir, 5, "\x10\x10\x10\x10\x10\x10\x10\x10", 1);
    is_int(0, code, "AddKey overwrites when asked");

    /* Use GetKey to check that it did so */
    code = afsconf_GetKey(dir, 5, &key);
    is_int(0, code, " ... and GetKey still works");
    ok(memcmp(&key, "\x10\x10\x10\x10\x10\x10\x10\x10", 8) == 0,
       " ... and key has been overwritten");

    /* Check that deleting a key that doesn't exist fails */
    code = afsconf_DeleteKey(dir, 6);
    is_int(AFSCONF_NOTFOUND, code,
           "afsconf_DeleteKey returns NOTFOUND if key doesn't exist");

    /* Check that we can delete a key using afsconf_DeleteKey */
    code = afsconf_DeleteKey(dir, 2);
    is_int(0, code, "afsconf_DeleteKey can delete a key");
    code = afsconf_GetKey(dir, 2, &key);
    is_int(AFSCONF_NOTFOUND, code, " ... and afsconf_GetKey can't find it");

    /* Check that deleting it doesn't leave a hole in what GetKeys returns */
    code = afsconf_GetKeys(dir, &keys);
    is_int(0, code, "... and afsconf_GetKeys returns it");
    is_int(3, keys.nkeys, "... and returns the right number of keys");
    is_int(1, keys.key[0].kvno, " ... first key number is correct");
    is_int(4, keys.key[1].kvno, " ... second key number is correct");
    is_int(5, keys.key[2].kvno, " ... third key number is correct");

    /* Make sure that if we drop the dir structure, and then rebuild it, we
     * still have the same KeyFile */
    afsconf_Close(dir);

    dir = afsconf_Open(dirname);
    ok(dir != NULL, "Sucessfully re-opened config directory");
    if (dir == NULL)
	goto out;

    code = afsconf_GetKeys(dir, &keys);
    is_int(0, code, "afsconf_GetKeys still works");
    is_int(3, keys.nkeys, "... and returns the right number of keys");
    is_int(1, keys.key[0].kvno, " ... first key number is correct");
    is_int(4, keys.key[1].kvno, " ... second key number is correct");
    is_int(5, keys.key[2].kvno, " ... third key number is correct");

    /* Now check that we're limited to 8 keys */
    for (i=0; i<5; i++) {
	code = afsconf_AddKey(dir, 10+i, "\x10\x10\x10\x10\x10\x10\x10\x10",
			      0);
	is_int(0, code, "Adding %dth key with AddKey works", i+4);
    }
    code = afsconf_AddKey(dir, 20, "\x10\x10\x10\x10\x10\x10\x10\x10",0);
    is_int(AFSCONF_FULL, code, "afsconf_AddKey fails once we've got 8 keys");

    /* Check that the new interface also fails when we've got too many
     * keys */
    keyMaterial = rx_opaque_new("\x10\x10\x10\x10\x10\x10\x10\x10", 8);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, 20, 0, keyMaterial);
    rx_opaque_free(&keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 0);
    afsconf_typedKey_put(&typedKey);
    is_int(AFSCONF_FULL, code,
	   "afsconf_AddTypedKey fails for rxkad once we've got 8 keys");

    /* Check the new accessors work for rxkad keys */
    code = afsconf_GetKeyByTypes(dir, afsconf_rxkad, 4, 0, &typedKey);
    is_int(0, code,
	   "afsconf_GetKeyByTypes works for rxkad");
    ok(keyMatches(typedKey, afsconf_rxkad, 4, 0,
		  "\x19\x16\xfe\xe6\xba\x77\x2f\xfd", 8),
       " ... and returned key matches");

    afsconf_typedKey_put(&typedKey);

    code = afsconf_GetKeysByType(dir, afsconf_rxkad, 4, &typedKeyList);
    is_int(0, code,
	   "afsconf_GetKeysByType works for rxkad");
    is_int(1, typedKeyList->nkeys,
	   " ... and returns 1 key, as expected");
    ok(keyMatches(typedKeyList->keys[0], afsconf_rxkad, 4, 0,
		  "\x19\x16\xfe\xe6\xba\x77\x2f\xfd", 8),
       " ... and returned key matches");

    afsconf_PutTypedKeyList(&typedKeyList);

    code = afsconf_GetLatestKeyByTypes(dir, afsconf_rxkad, 0, &typedKey);
    is_int(0, code,
	   "afsconf_GetLatestKeyByTypes works for rxkad");
    ok(keyMatches(typedKey, afsconf_rxkad, 14, 0,
		  "\x10\x10\x10\x10\x10\x10\x10\x10", 8),
       " ... and returned key matches");

    afsconf_typedKey_put(&typedKey);

    code = afsconf_GetLatestKeysByType(dir, afsconf_rxkad, &typedKeyList);
    is_int(0, code,
	   "afsconf_GetLatestKeysByType works for rxkad");
    is_int(1, typedKeyList->nkeys,
	   " ... and returns 1 key, as expected");
    ok(keyMatches(typedKeyList->keys[0], afsconf_rxkad, 14, 0,
		 "\x10\x10\x10\x10\x10\x10\x10\x10", 8),
       " ... and returned key matches");
    afsconf_PutTypedKeyList(&typedKeyList);

    /* Check that we can't delete a key that doesn't exist */
    code = afsconf_DeleteKeyByType(dir, afsconf_rxkad, 6);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_DeleteKeyByType returns NOTFOUND if key doesn't exist");
    code = afsconf_DeleteKeyBySubType(dir, afsconf_rxkad, 6, 0);
    is_int(AFSCONF_NOTFOUND, code,
           "afsconf_DeleteKeyBySubType returns NOTFOUND if key doesn't exist");
    code = afsconf_DeleteKeyBySubType(dir, afsconf_rxkad, 14, 1);
    is_int(AFSCONF_NOTFOUND, code,
           "afsconf_DeleteKeyBySubType doesn't delete with wrong subtype");
    code = afsconf_GetKeyByTypes(dir, afsconf_rxkad, 14, 0, &typedKey);
    is_int(0, code, " ... and key is still there!");
    afsconf_typedKey_put(&typedKey);

    /* Check that we can delete a key that does */
    code = afsconf_DeleteKeyByType(dir, afsconf_rxkad, 13);
    is_int(0, code, "afsconf_DeleteKeyByType works");
    code = afsconf_GetKeysByType(dir, afsconf_rxkad, 13, &typedKeyList);
    is_int(AFSCONF_NOTFOUND, code, " ... and is really gone");

    code = afsconf_DeleteKeyBySubType(dir, afsconf_rxkad, 14, 0);
    is_int(0, code, "afsconf_DeleteKeyBySubType works");
    code = afsconf_GetKeyByTypes(dir, afsconf_rxkad, 14, 0, &typedKey);
    is_int(AFSCONF_NOTFOUND, code, " ... and is really gone");

    /* Unlink the KeyFile */
    unlink(keyfile);

    /* Force a rebuild of the directory structure, just in case */
    afsconf_Close(dir);

    dir = afsconf_Open(dirname);
    ok(dir != NULL, "Sucessfully re-opened config directory");
    if (dir == NULL)
	goto out;

    /* Check that all of the various functions work properly if the file
     * isn't there */
    code = afsconf_GetKeys(dir, &keys);
    is_int(0, code, "afsconf_GetKeys works with an empty KeyFile");
    is_int(0, keys.nkeys, " ... and returns the right number of keys");
    code = afsconf_GetKey(dir, 1, &key);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_GetKey returns NOTFOUND with an empty KeyFile");
    code = afsconf_DeleteKey(dir, 1);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_DeleteKey returns NOTFOUND with an empty KeyFile");
    code = afsconf_GetLatestKey(dir, &kvno, &key);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_GetLatestKey returns NOTFOUND with an empty KeyFile");
    code = afsconf_GetKeysByType(dir, afsconf_rxkad, 1, &typedKeyList);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_GetKeysByType returns NOTFOUND with an empty KeyFile");
    code = afsconf_GetKeyByTypes(dir, afsconf_rxkad, 1, 0, &typedKey);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_GetKeyByTypes returns NOTFOUND with an empty KeyFile");
    code = afsconf_GetLatestKeysByType(dir, afsconf_rxkad, &typedKeyList);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_GetLatestKeysByType returns NOTFOUND with empty KeyFile");
    code = afsconf_GetLatestKeyByTypes(dir, afsconf_rxkad, 0, &typedKey);
    is_int(AFSCONF_NOTFOUND, code,
	   "afsconf_GetLatestKeyByTypes returns NOTFOUND with empty KeyFile");

    /* Now try adding a key to an empty file */
    code = afsconf_AddKey(dir, 1, "\x10\x10\x10\x10\x10\x10\x10\x10", 1);
    is_int(0, code, "afsconf_AddKey succeeds with an empty KeyFile");
    code = afsconf_GetLatestKey(dir, &kvno, &key);
    is_int(0, code, " ... and afsconf_GetLatestKey succeeds");
    is_int(1, kvno, " ... with correct kvno");
    ok(memcmp(&key, "\x10\x10\x10\x10\x10\x10\x10\x10", 8) == 0,
       " ... and key");

    /* And adding a key using the new interface */

    keyMaterial = rx_opaque_new("\x20\x20\x20\x20\x20\x20\x20\x20", 8);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, 2, 0, keyMaterial);
    rx_opaque_free(&keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 0);
    afsconf_typedKey_put(&typedKey);
    is_int(0, code, "afsconf_AddTypedKey works");
    code = afsconf_GetLatestKey(dir, &kvno, &key);
    is_int(0, code, " ... and afsconf_GetLatestKey succeeds");
    is_int(2, kvno, " ... with correct kvno");
    ok(memcmp(&key, "\x20\x20\x20\x20\x20\x20\x20\x20", 8) == 0,
       " ... and key");
    code = afsconf_GetLatestKeyByTypes(dir, afsconf_rxkad, 0, &typedKey);
    is_int(0, code, " ... and so does afsconf_GetLatestKeyByTypes");
    ok(keyMatches(typedKey, afsconf_rxkad, 2, 0,
		  "\x20\x20\x20\x20\x20\x20\x20\x20", 8),
       " ... with correct key");
    afsconf_typedKey_put(&typedKey);

    /* And that we can't add a key to an existing kvno and type */
    keyMaterial = rx_opaque_new("\x30\x30\x30\x30\x30\x30\x30\x30", 8);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, 2, 0, keyMaterial);
    rx_opaque_free(&keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 0);
    afsconf_typedKey_put(&typedKey);
    is_int(AFSCONF_KEYINUSE, code,
	   "afsconf_AddTypedKey won't overwrite without being told to");
    code = afsconf_GetKeyByTypes(dir, afsconf_rxkad, 2, 0, &typedKey);
    is_int(0, code, " ... and key still exists");
    ok(keyMatches(typedKey, afsconf_rxkad, 2, 0,
	          "\x20\x20\x20\x20\x20\x20\x20\x20", 8),
       " ... and hasn't changed");
    afsconf_typedKey_put(&typedKey);

    /* But we can if we force */
    keyMaterial = rx_opaque_new("\x30\x30\x30\x30\x30\x30\x30\x30", 8);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, 2, 0, keyMaterial);
    rx_opaque_free(&keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 1);
    afsconf_typedKey_put(&typedKey);
    is_int(0, code,  "afsconf_AddTypedKey overwrites when asked");
    code = afsconf_GetKeyByTypes(dir, afsconf_rxkad, 2, 0, &typedKey);
    is_int(0, code, " ... and GetKeyByTypes retrieves new key");
    ok(keyMatches(typedKey, afsconf_rxkad, 2, 0,
	          "\x30\x30\x30\x30\x30\x30\x30\x30", 8),
       " ... and it is the new key");

    /* Check that we can't add bad rxkad keys */
    keyMaterial = rx_opaque_new("\x30\x30\x30\x30\x30\x30\x30", 7);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, 3, 0, keyMaterial);
    rx_opaque_free(&keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 1);
    afsconf_typedKey_put(&typedKey);
    is_int(AFSCONF_BADKEY, code,
	   "afsconf_AddTypedKey won't add short rxkad keys");
    keyMaterial = rx_opaque_new("\x30\x30\x30\x30\x30\x30\x30\x30\x30", 9);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, 3, 0, keyMaterial);
    rx_opaque_free(&keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 1);
    afsconf_typedKey_put(&typedKey);
    is_int(AFSCONF_BADKEY, code,
	   "afsconf_AddTypedKey won't add long rxkad keys");
    keyMaterial = rx_opaque_new("\x30\x30\x30\x30\x30\x30\x30\x30", 8);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, 3, 1, keyMaterial);
    rx_opaque_free(&keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 1);
    afsconf_typedKey_put(&typedKey);
    is_int(AFSCONF_BADKEY, code,
	   "afsconf_AddTypedKey won't add rxkad keys with non-zero subtype");

    /* Now, test things with other key types. */

    /* Add a different key type, but with same kvno as rxkad */
    keyMaterial = rx_opaque_new("\x01", 1);
    typedKey = afsconf_typedKey_new(1, 2, 0, keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 0);
    afsconf_typedKey_put(&typedKey);
    is_int(0, code,
	   "afsconf_AddTypedKey can add keys with different key type");

    /* Add a different subtype, with same kvno */
    keyMaterial = rx_opaque_new("\x02\x03", 2);
    typedKey = afsconf_typedKey_new(1, 2, 1, keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 0);
    afsconf_typedKey_put(&typedKey);
    is_int(0, code,
	   "afsconf_AddTypedKey can add keys with different sub type");

    /* Check the GetKeyByTypes returns one of the keys */
    code = afsconf_GetKeyByTypes(dir, 1, 2, 1, &typedKey);
    is_int(0, code, "afsconf_GetKeyByTypes returns");
    ok(keyMatches(typedKey, 1, 2, 1, "\x02\x03", 2),
       " ... with the right key");

    /* Check that GetKeysByType returns both of the keys */
    code = afsconf_GetKeysByType(dir, 1, 2, &typedKeyList);
    is_int(0, code, "afsconf_GetKeysByType returns");
    is_int(2, typedKeyList->nkeys, " ... with correct number of keys");
    ok(keyMatches(typedKeyList->keys[0], 1, 2, 0, "\x01", 1),
       " ... with the right key in slot 0");
    ok(keyMatches(typedKeyList->keys[1], 1, 2, 1, "\x02\x03", 2),
       " ... with the right key in slot 1");
    afsconf_PutTypedKeyList(&typedKeyList);

    /* Add another key, before these ones, so we can check that
     * latest really works */
    keyMaterial = rx_opaque_new("\x03", 1);
    typedKey = afsconf_typedKey_new(1, 1, 0, keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 0);
    afsconf_typedKey_put(&typedKey);
    is_int(0, code, "afsconf_AddTypedKey worked again");

    /* Check that GetLatestKeyByTypes returns one */
    code = afsconf_GetLatestKeyByTypes(dir, 1, 1, &typedKey);
    is_int(0, code, "afsconf_GetLatestKeyByTypes returns");
    ok(keyMatches(typedKey, 1, 2, 1, "\x02\x03", 2),
       " ... with the right key");

    /* Check that GetLatestKeysByType returns both */
    code = afsconf_GetLatestKeysByType(dir, 1, &typedKeyList);
    is_int(0, code, "afsconf_GetLatestKeysByType returns");
        is_int(2, typedKeyList->nkeys, " ... with correct number of keys");
    ok(keyMatches(typedKeyList->keys[0], 1, 2, 0, "\x01", 1),
       " ... with the right key in slot 0");
    ok(keyMatches(typedKeyList->keys[1], 1, 2, 1, "\x02\x03", 2),
       " ... with the right key in slot 1");
    afsconf_PutTypedKeyList(&typedKeyList);

    /* Check that closing this instance, and reopening, still has all of
     * the required keys
     */
    afsconf_Close(dir);

    dir = afsconf_Open(dirname);
    ok(dir != NULL, "Sucessfully re-opened config directory");
    if (dir == NULL)
	goto out;

    /* Check that GetKeysByType returns all of the keys */
    code = afsconf_GetKeysByType(dir, 1, 1, &typedKeyList);
    is_int(0, code, "afsconf_GetKeysByType returns after reopening");
    is_int(1, typedKeyList->nkeys, " ... First kvno has correct number of keys");
    ok(keyMatches(typedKeyList->keys[0], 1, 1, 0, "\x03", 1),
       " ... and key material is correct");
    afsconf_PutTypedKeyList(&typedKeyList);

    code = afsconf_GetKeysByType(dir, 1, 2, &typedKeyList);
    is_int(0, code, "afsconf_GetKeysByType returns after reopening");
    is_int(2, typedKeyList->nkeys, " ... with correct number of keys");
    ok(keyMatches(typedKeyList->keys[0], 1, 2, 0, "\x01", 1),
       " ... with the right key in slot 0");
    ok(keyMatches(typedKeyList->keys[1], 1, 2, 1, "\x02\x03", 2),
       " ... with the right key in slot 1");
    afsconf_PutTypedKeyList(&typedKeyList);

    /* Check that GetAllKeys works as expected */
    code = afsconf_GetAllKeys(dir, &typedKeyList);
    is_int(0, code, "afsconf_GetAllKeys returns success");
    is_int(5, typedKeyList->nkeys, " ... with the correct number of keys");
    ok(keyMatches(typedKeyList->keys[0], afsconf_rxkad, 1, 0,
		  "\x10\x10\x10\x10\x10\x10\x10\x10", 8),
       " ... with right key in slot 0");
    ok(keyMatches(typedKeyList->keys[1], afsconf_rxkad, 2, 0,
		   "\x30\x30\x30\x30\x30\x30\x30\x30", 8),
       " ... with right key in slot 1");
    ok(keyMatches(typedKeyList->keys[2], 1, 1, 0, "\x03", 1),
       " ... with right key in slot 2");
    ok(keyMatches(typedKeyList->keys[3], 1, 2, 0, "\x01", 1),
       " ... with right key in slot 3");
    ok(keyMatches(typedKeyList->keys[4], 1, 2, 1, "\x02\03", 2),
       " ... with right key in slot 4");

    afsconf_Close(dir);

    afstest_rmdtemp(dirname);
    free(dirname);
    free(keyfile);

    /* Start a new test configuration */
    dirname = afstest_BuildTestConfig(&bct);
    dir = afsconf_Open(dirname);
    ok(dir != NULL, "Sucessfully opened brand new config directory");
    if (dir == NULL)
	goto out;

    /* Check that directories with just new style keys work */
    keyMaterial = rx_opaque_new("\x02\x03", 2);
    typedKey = afsconf_typedKey_new(1, 2, 1, keyMaterial);
    code = afsconf_AddTypedKey(dir, typedKey, 0);
    afsconf_typedKey_put(&typedKey);
    is_int(0, code,
	   "afsconf_AddTypedKey can add keys with different sub type");

    /* Check the GetKeyByTypes returns one of the keys */
    code = afsconf_GetKeyByTypes(dir, 1, 2, 1, &typedKey);
    is_int(0, code, "afsconf_GetKeyByTypes returns it");
    ok(keyMatches(typedKey, 1, 2, 1, "\x02\x03", 2),
       " ... with the right key");

out:
    afstest_rmdtemp(dirname);

    return 0;
}
