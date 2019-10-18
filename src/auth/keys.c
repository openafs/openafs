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

#include <opr/queue.h>

/* Need rx/rx.h to get working assert(), used by LOCK_GLOBAL_MUTEX */
#include <rx/rx.h>
#include <rx/rx_atomic.h>

#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <afs/afsutil.h>

#include "cellconfig.h"
#include "keys.h"
#include "internal.h"

struct afsconf_typedKey {
    rx_atomic_t refcnt;
    afsconf_keyType type;
    int kvno;
    int subType;
    struct rx_opaque key;
};

static struct afsconf_typedKey *afsconf_typedKey_blank(void);

/* Memory storage for keyfile contents. */

struct keyTypeList {
    struct opr_queue link;
    afsconf_keyType type;
    struct opr_queue kvnoList;
};

struct kvnoList {
    struct opr_queue link;
    int kvno;
    struct opr_queue subTypeList;
};

struct subTypeList {
    struct opr_queue link;
    int subType;
    struct afsconf_typedKey *key;
};

static int
listToArray(struct kvnoList *kvnoEntry, struct afsconf_typedKeyList **keys)
{
    struct afsconf_typedKeyList *retval;
    struct opr_queue *cursor;
    int i;

    /* Allocate space for the keys we've got stored */
    retval = malloc(sizeof(struct afsconf_typedKeyList));
    retval->nkeys = opr_queue_Count(&kvnoEntry->subTypeList);

    if (retval->nkeys > 0) {
        retval->keys  = calloc(retval->nkeys, sizeof(struct afsconf_typedKey *));

	i = 0;
        for(opr_queue_Scan(&kvnoEntry->subTypeList, cursor)) {
	    struct subTypeList *entry;

	    entry = opr_queue_Entry(cursor, struct subTypeList, link);
	    retval->keys[i] = afsconf_typedKey_get(entry->key);
	    i++;
        }
    } else {
	retval->keys = NULL;
    }

    *keys = retval;
    return 0;
}

static struct keyTypeList *
findByType(struct afsconf_dir *dir, afsconf_keyType type)
{
    struct opr_queue *cursor;
    struct keyTypeList *entry = NULL;

    for (opr_queue_Scan(&dir->keyList, cursor)) {
	entry = opr_queue_Entry(cursor, struct keyTypeList, link);
	if (entry->type >= type)
	    break;
    }
    if (entry == NULL || entry->type != type)
	return NULL;

    return entry;
}

static struct kvnoList *
findInTypeList(struct keyTypeList *parent, int kvno)
{
    struct opr_queue *cursor;
    struct kvnoList *entry = NULL;

    for (opr_queue_Scan(&parent->kvnoList, cursor)) {
	entry = opr_queue_Entry(cursor, struct kvnoList, link);
	if (entry->kvno >= kvno)
	    break;
    }
    if (entry == NULL || entry->kvno != kvno)
	return NULL;

    return entry;
}

static struct kvnoList *
findByKvno(struct afsconf_dir *dir, afsconf_keyType type, int kvno)
{
    struct keyTypeList *entry;
    entry = findByType(dir, type);

    if (entry == NULL)
	return NULL;

    return findInTypeList(entry, kvno);
}

static struct subTypeList *
findInKvnoList(struct kvnoList *parent, int subType)
{
    struct opr_queue *cursor;
    struct subTypeList *entry = NULL;

    for (opr_queue_Scan(&parent->subTypeList, cursor)) {
	entry = opr_queue_Entry(cursor, struct subTypeList, link);
	if (entry->subType >= subType)
	     break;
    }
    if (entry == NULL || entry->subType != subType)
	return NULL;

    return entry;
}

static struct subTypeList *
findBySubType(struct afsconf_dir *dir, afsconf_keyType type, int kvno,
	      int subType)
{
   struct kvnoList *entry;

   entry = findByKvno(dir, type, kvno);
   if (entry == NULL)
	return NULL;

   return findInKvnoList(entry, subType);
}


/* Add key. */
static int
addMemoryKey(struct afsconf_dir *dir, struct afsconf_typedKey *key,
	     int overwrite)
{
    struct opr_queue   *cursor;
    struct keyTypeList *typeEntry = NULL;
    struct kvnoList    *kvnoEntry = NULL;
    struct subTypeList *subType   = NULL;

    /* Find the place in the keyType list to insert the key into */
    for (opr_queue_Scan(&dir->keyList, cursor)) {
	typeEntry = opr_queue_Entry(cursor, struct keyTypeList, link);
	if (typeEntry->type >= key->type)
	    break;
    }

    if (typeEntry == NULL || typeEntry->type != key->type) {
	struct keyTypeList *list;

	list = malloc(sizeof(struct keyTypeList));
	opr_queue_Init(&list->kvnoList);
	list->type = key->type;
	opr_queue_InsertBefore(cursor, &list->link);
	typeEntry = list;
    }

    /* And the place in the kvno list */
    for (opr_queue_Scan(&typeEntry->kvnoList, cursor)) {
	kvnoEntry = opr_queue_Entry(cursor, struct kvnoList, link);
	if (kvnoEntry->kvno >= key->kvno)
	    break;
    }

    if (kvnoEntry == NULL || kvnoEntry->kvno != key->kvno) {
	struct kvnoList *list;

	/* In the legacy rxkad key case, we need to check to see if we've
	 * gone over the maximum of 8 keys */
	if (key->type == afsconf_rxkad &&
	    opr_queue_Count(&typeEntry->kvnoList)>=8)
	    return AFSCONF_FULL;

	list = malloc(sizeof(struct kvnoList));
	opr_queue_Init(&list->subTypeList);
	list->kvno = key->kvno;
	opr_queue_InsertBefore(cursor, &list->link);
	kvnoEntry = list;
    }

    /* And the place in the subtype list */
    for (opr_queue_Scan(&kvnoEntry->subTypeList, cursor)) {
	subType = opr_queue_Entry(cursor, struct subTypeList, link);
	if (subType->subType >= key->subType)
	    break;
    }

    if (subType == NULL || subType->subType != key->subType) {
	struct subTypeList *list;

	list = malloc(sizeof(struct subTypeList));
	list->subType = key->subType;
	list->key = afsconf_typedKey_get(key);
	opr_queue_InsertBefore(cursor, &list->link);
    } else {
	if (overwrite) {
	    /* Give up our reference to the existing key */
	    afsconf_typedKey_put(&subType->key);
	    subType->key = afsconf_typedKey_get(key);
	} else {
	    return AFSCONF_KEYINUSE;
	}
    }
    return 0;
}

static void
deleteKvnoEntry(struct kvnoList *entry)
{
    struct subTypeList *subTypeEntry;

    while (!opr_queue_IsEmpty(&entry->subTypeList)) {
	subTypeEntry = opr_queue_First(&entry->subTypeList,
				       struct subTypeList, link);
	afsconf_typedKey_put(&subTypeEntry->key);
	opr_queue_Remove(&subTypeEntry->link);
	free(subTypeEntry);
    }
    opr_queue_Remove(&entry->link);
    free(entry);
}

void
_afsconf_FreeAllKeys(struct afsconf_dir *dir)
{
    struct keyTypeList *typeEntry;
    struct kvnoList *kvnoEntry;

    while (!opr_queue_IsEmpty(&dir->keyList)) {
	typeEntry = opr_queue_First(&dir->keyList, struct keyTypeList, link);

	while (!opr_queue_IsEmpty(&typeEntry->kvnoList)) {
	    kvnoEntry = opr_queue_First(&typeEntry->kvnoList,
					struct kvnoList, link);

	    deleteKvnoEntry(kvnoEntry);
	}
	opr_queue_Remove(&typeEntry->link);
	free(typeEntry);
    }
}
void
_afsconf_InitKeys(struct afsconf_dir *dir)
{
    opr_queue_Init(&dir->keyList);
}

/* Disk based key storage. This is made somewhat complicated because we
 * store keys in more than one place - keys of type 'rxkad' (0) are stored
 * in the original KeyFile, so that we can continue to be compatible with
 * utilities that directly modify that file.
 *
 * All other keys are stored in the file KeyFileExt, which has the following
 * format:
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                        number of keys                           |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Key data ...
 *   +-+-+-+-+-+-+-+
 *
 * If the format ever needs to chanage incompatibly, a new file name
 * will be used.
 *
 * Key data is a sequence of the following records (note that these are
 * not word aligned - the next record begins where the previous one ends)
 *
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         meta-data length                        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                           key type                              |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                       key version number                        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         key sub type                            |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                      length of key material                     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Key material
 *   +-+-+-+-+-+-+-+
 *
 * All values are expressed in network byte order
 *
 * Meta data length is the length of the initial portion of the record
 * (itself, key type, key version number, and key sub type). In this
 * version of the specification it would be 16. It is there to allow
 * additional fields to be added to this specification at a later date
 * without breaking backwards compatibility.
 */

/* XXX - We need to be careful with failure here, because failure due to
 * a missing file is fine, but failure due to read errors needs to be trapped,
 * before it results in a corrupted file being written out.
 */

static int
_parseOriginalKeyFile(struct afsconf_dir *dir, char *fileName)
{
    int fd, code, nkeys, i;
    struct afsconf_typedKey *key;

    fd = open(fileName, O_RDONLY);
    if (fd < 0)
	return 0;

    code = read(fd, &nkeys, sizeof(afs_int32));
    if (code!= sizeof(afs_int32))
	goto fail;

    nkeys=ntohl(nkeys);
    for(i=0; i<nkeys; i++) {

	key = afsconf_typedKey_blank();
	if (key == NULL)
	    goto fail;

	key->type = afsconf_rxkad;
	key->subType = 0;

	code = read(fd, &key->kvno, sizeof(afs_int32));
	if (code != sizeof(afs_int32)) {
	    free(key);
	    goto fail;
	}
	key->kvno = ntohl(key->kvno);

	rx_opaque_alloc(&key->key, 8);
	code = read(fd, key->key.val, 8);
	if (code != 8) {
	    rx_opaque_freeContents(&key->key);
	    free(key);
	    goto fail;
	}
	code = addMemoryKey(dir, key, 1);
	afsconf_typedKey_put(&key); /* Done with key */
	if (code)
	    goto fail;
    }
    close(fd);
    return 0;

fail:
    close(fd);
    return EIO;
}

static_inline int
writeWord(int fd, afs_int32 data)
{

     data = htonl(data);

     if (write(fd, &data, sizeof(afs_int32)) != sizeof(afs_int32))
	return EIO;

     return 0;
}

static int
_writeOriginalKeyFile(struct afsconf_dir *dir, char *fileName)
{
    int nkeys, fd;
    struct opr_queue *cursor;
    struct keyTypeList *typeEntry;

    fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
	return AFSCONF_FAILURE;

    typeEntry = findByType(dir, afsconf_rxkad);
    if (typeEntry)
	nkeys = opr_queue_Count(&typeEntry->kvnoList);
    else
	nkeys = 0;

    if (writeWord(fd, nkeys))
	goto fail;

    if (typeEntry == NULL)
	goto out;

    for (opr_queue_Scan(&typeEntry->kvnoList, cursor)) {
	struct kvnoList *kvnoEntry;
        struct subTypeList *subEntry;

	kvnoEntry = opr_queue_Entry(cursor, struct kvnoList, link);
        subEntry = opr_queue_First(&kvnoEntry->subTypeList,
				   struct subTypeList, link);
	if (writeWord(fd, subEntry->key->kvno))
	    goto fail;
	if (write(fd, subEntry->key->key.val, 8) != 8)
	    goto fail;
    }

out:
    close(fd);
    return 0;

fail:
    close(fd);
    return AFSCONF_FAILURE;
}

static int
_parseExtendedKeyFile(struct afsconf_dir *dir, char *fileName)
{
    int fd, i, code;
    afs_int32 nkeys;
    struct afsconf_typedKey *key = NULL;

    fd = open(fileName, O_RDONLY);
    if (fd < 0)
	return 0;

    code = read(fd, &nkeys, sizeof(afs_int32));
    if (code!= sizeof(afs_int32))
	goto fail;

    nkeys=ntohl(nkeys);
    for(i=0; i<nkeys; i++) {
	afs_int32 reclen;

	key = afsconf_typedKey_blank();
	if (key == NULL)
	    goto fail;

	/* The only data version we currently parse has a reclen of 16.
	 * Anything smaller indicates a corrupt key file. Anything more,
	 * and we just skip the extra fields */
	code = read(fd, &reclen, sizeof(afs_int32));
	if (code != sizeof(afs_int32))
	    goto fail;
	reclen = ntohl(reclen);
	if (reclen < 16)
	    goto fail;
	reclen-=sizeof(afs_int32);

	code = read(fd, &key->type, sizeof(afs_int32));
	if (code != sizeof(afs_int32))
	    goto fail;
	key->type = ntohl(key->type);
	reclen-=sizeof(afs_int32);

	code = read(fd, &key->kvno, sizeof(afs_int32));
	if (code != sizeof(afs_int32))
	    goto fail;
	key->kvno = ntohl(key->kvno);
	reclen-=sizeof(afs_int32);

	code = read(fd, &key->subType, sizeof(afs_int32));
	if (code != sizeof(afs_int32))
	    goto fail;
	key->subType = ntohl(key->subType);
	reclen-=sizeof(afs_int32);

	if (reclen > 0) {
	    code = lseek(fd, reclen, SEEK_CUR);
	    if (code < 0)
		goto fail;
	}

	code = read(fd, &reclen, sizeof(afs_int32));
	if (code != sizeof(afs_int32))
	    goto fail;
	reclen = ntohl(reclen);

	rx_opaque_alloc(&key->key, reclen);
	code = read(fd, key->key.val, reclen);
	if (code != reclen) {
	    rx_opaque_freeContents(&key->key);
	    goto fail;
	}
	code = addMemoryKey(dir, key, 1);
	afsconf_typedKey_put(&key);
	if (code)
	    goto fail;
    }
    close(fd);
    return 0;

fail:
    if (key)
	afsconf_typedKey_put(&key);

    close(fd);
    return EIO;
}


static int
_writeExtendedKeyFile(struct afsconf_dir *dir, char *fileName)
{
    int nkeys;
    int fd;

    struct keyTypeList *typeEntry;
    struct kvnoList    *kvnoEntry;
    struct subTypeList *entry;
    struct opr_queue   *keyCursor;
    struct opr_queue   *kvnoCursor;
    struct opr_queue   *subCursor;

    fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
	return AFSCONF_FAILURE;

    /* Iterate over the whole in-memory key store, and write everything
     * except keys with type rxkad into the extended key file */

    /* Write a 0 key count - we'll fill it in later */
    nkeys = 0;
    if (writeWord(fd, 0))
	goto fail;

    for (opr_queue_Scan(&dir->keyList, keyCursor)) {
	typeEntry = opr_queue_Entry(keyCursor, struct keyTypeList, link);

	if (typeEntry->type != afsconf_rxkad) {
	    for (opr_queue_Scan(&typeEntry->kvnoList, kvnoCursor)) {
	        kvnoEntry = opr_queue_Entry(kvnoCursor,	struct kvnoList, link);
		for (opr_queue_Scan(&kvnoEntry->subTypeList, subCursor)) {
		    entry = opr_queue_Entry(subCursor, struct subTypeList, link);
		    if (writeWord(fd, 16)) /* record length */
			goto fail;
		    if (writeWord(fd, entry->key->type))
			goto fail;
		    if (writeWord(fd, entry->key->kvno))
			goto fail;
		    if (writeWord(fd, entry->key->subType))
			goto fail;
		    if (writeWord(fd, entry->key->key.len))
			goto fail;
		    if (write(fd, entry->key->key.val,
			      entry->key->key.len) !=
			entry->key->key.len)
			goto fail;
		    nkeys++;
		}
	    }
	}
    }

    if (lseek(fd, 0, SEEK_SET)<0)
	goto fail;

    if (writeWord(fd, nkeys))
	goto fail;

    close(fd);

    return 0;

fail:
    close(fd);
    return AFSCONF_FAILURE;
}

int
_afsconf_LoadKeys(struct afsconf_dir *dir)
{
    int code;
    char *fileName;

    /* If we're running on Windows, and we are a client, we don't have a
     * KeyFile, so don't try and open one */

#ifdef AFS_NT40_ENV
    if (_afsconf_IsClientConfigDirectory(dir->name))
	return 0;
#endif /* AFS_NT40_ENV */

    LOCK_GLOBAL_MUTEX;

    /* Delete all of our existing keys */
    _afsconf_FreeAllKeys(dir);

    /* Start by opening the original KeyFile */
    asnprintf(&fileName, 256, "%s/%s", dir->name, AFSDIR_KEY_FILE);
    code = _parseOriginalKeyFile(dir, fileName);
    free(fileName);
    if (code)
	goto out;

    /* Now open the new style KeyFile */
    asnprintf(&fileName, 256, "%s/%s", dir->name, AFSDIR_EXT_KEY_FILE);
    code = _parseExtendedKeyFile(dir, fileName);
    free(fileName);
    if (code)
	goto out;

out:
    if (code)
	_afsconf_FreeAllKeys(dir);

    UNLOCK_GLOBAL_MUTEX;

    return code;
}

static int
_afsconf_SaveKeys(struct afsconf_dir *dir)
{
    char *fileName;
    int code;

    /* If we're running on Windows, and we are a client, we don't have a
     * KeyFile, so don't try and open one */

#ifdef AFS_NT40_ENV
    if (_afsconf_IsClientConfigDirectory(dir->name))
	return 0;
#endif /* AFS_NT40_ENV */

    LOCK_GLOBAL_MUTEX;

    /* Start by opening the original KeyFile */
    asnprintf(&fileName, 256, "%s/%s", dir->name, AFSDIR_KEY_FILE);
    code = _writeOriginalKeyFile(dir, fileName);
    free(fileName);
    if (code)
	goto out;

    /* Now open the new style KeyFile */
    asnprintf(&fileName, 256, "%s/%s", dir->name, AFSDIR_EXT_KEY_FILE);
    code = _writeExtendedKeyFile(dir, fileName);
    free(fileName);
    if (code)
	goto out;

out:
    UNLOCK_GLOBAL_MUTEX;

    return code;
}



/* get keys structure */
int
afsconf_GetKeys(struct afsconf_dir *dir, struct afsconf_keys *astr)
{
    afs_int32 code;
    struct keyTypeList *typeEntry;
    struct opr_queue *cursor;

    memset(astr, 0, sizeof(struct afsconf_keys));

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    typeEntry = findByType(dir, afsconf_rxkad);
    if (typeEntry == NULL)
	goto out;

    for (opr_queue_Scan(&typeEntry->kvnoList, cursor)) {
	struct kvnoList *kvnoEntry;
        struct subTypeList *subEntry;

	kvnoEntry = opr_queue_Entry(cursor, struct kvnoList, link);
        subEntry = opr_queue_First(&kvnoEntry->subTypeList,
				   struct subTypeList, link);
	/* XXX - If there is more than one key in this list, it's an error */
	astr->key[astr->nkeys].kvno = subEntry->key->kvno;
	/* XXX - If the opaque contains a number of bytes other than 8, it's
	 * an error */
	memcpy(&astr->key[astr->nkeys].key, subEntry->key->key.val, 8);
	astr->nkeys++;
    }

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

afs_int32
afsconf_GetLatestKey(struct afsconf_dir *dir, afs_int32 *kvno,
		     struct ktc_encryptionKey *key)
{
    struct afsconf_typedKey *typedKey;
    int code;

    code = afsconf_GetLatestKeyByTypes(dir, afsconf_rxkad, 0, &typedKey);
    if (code)
	return code;

    /* XXX - Should check that the key is of the correct length */

    /* Copy out the relevant details */
    if (kvno != NULL)
	*kvno = typedKey->kvno;

    if (key != NULL)
	memcpy(key, typedKey->key.val, 8);

    afsconf_typedKey_put(&typedKey);

    return 0;
}

int
afsconf_GetKey(void *rock, int kvno, struct ktc_encryptionKey *key)
{
    struct afsconf_typedKey *typedKey;
    int code;

    code = afsconf_GetKeyByTypes(rock, afsconf_rxkad, kvno, 0, &typedKey);
    if (code)
	return code;

    memcpy(key, typedKey->key.val, 8);

    afsconf_typedKey_put(&typedKey);

    return 0;
}

int
afsconf_AddKey(struct afsconf_dir *dir, afs_int32 kvno, char key[8],
	       afs_int32 overwrite)
{
    struct rx_opaque buffer;
    struct afsconf_typedKey *typedKey;
    int code;

    rx_opaque_alloc(&buffer, 8);
    memcpy(buffer.val, key, 8);
    typedKey = afsconf_typedKey_new(afsconf_rxkad, kvno, 0, &buffer);
    if (typedKey == NULL)
	return AFSCONF_FAILURE;

    rx_opaque_freeContents(&buffer);

    code = afsconf_AddTypedKey(dir, typedKey, overwrite);
    afsconf_typedKey_put(&typedKey);
    return code;
}

int
afsconf_DeleteKey(struct afsconf_dir *dir, afs_int32 kvno)
{
    return afsconf_DeleteKeyByType(dir, afsconf_rxkad, kvno);
}

int
afsconf_GetKeysByType(struct afsconf_dir *dir, afsconf_keyType type,
		      int kvno, struct afsconf_typedKeyList **keys)
{
    struct kvnoList *kvnoEntry;
    int code;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    kvnoEntry = findByKvno(dir, type, kvno);
    if (kvnoEntry == NULL) {
	code = AFSCONF_NOTFOUND;
        goto out;
    }

    code = listToArray(kvnoEntry, keys);

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

int
_afsconf_CountKeys(struct afsconf_dir *dir)
{
    int count = 0;
    struct opr_queue *typeCursor;
    struct keyTypeList *typeEntry;
    struct opr_queue *kvnoCursor;
    struct kvnoList *kvnoEntry;
    struct opr_queue *subCursor;

    for (opr_queue_Scan(&dir->keyList, typeCursor)) {
	typeEntry = opr_queue_Entry(typeCursor, struct keyTypeList, link);
	for (opr_queue_Scan(&typeEntry->kvnoList, kvnoCursor)) {
	    kvnoEntry = opr_queue_Entry(kvnoCursor, struct kvnoList, link);
	    for (opr_queue_Scan(&kvnoEntry->subTypeList, subCursor))
		count++;
	}
    }
    return count;
}

int
afsconf_CountKeys(struct afsconf_dir *dir)
{
    int count = 0;

    LOCK_GLOBAL_MUTEX;
    count = _afsconf_CountKeys(dir);
    UNLOCK_GLOBAL_MUTEX;

    return count;
}

int
afsconf_GetAllKeys(struct afsconf_dir *dir, struct afsconf_typedKeyList **keys)
{
    int code;
    struct afsconf_typedKeyList *retval;
    struct opr_queue *typeCursor;
    struct keyTypeList *typeEntry;
    struct opr_queue *kvnoCursor;
    struct kvnoList *kvnoEntry;
    struct opr_queue *subCursor;
    struct subTypeList *subEntry;
    int count;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    /* First, work out how many keys we have in total */
    count = _afsconf_CountKeys(dir);

    /* Allocate space for all of these */
    retval = malloc(sizeof(struct afsconf_typedKeyList));
    retval->nkeys = count;

    if (count > 0) {
	retval->keys = calloc(retval->nkeys,
			      sizeof(struct afsconf_typedKey *));

	/* Populate the key list */
        count = 0;
	for (opr_queue_Scan(&dir->keyList, typeCursor)) {
	    typeEntry = opr_queue_Entry(typeCursor,
					struct keyTypeList, link);
	    for (opr_queue_Scan(&typeEntry->kvnoList, kvnoCursor)) {
	        kvnoEntry = opr_queue_Entry(kvnoCursor,
					    struct kvnoList, link);
	        for (opr_queue_Scan(&kvnoEntry->subTypeList, subCursor)) {
		    subEntry = opr_queue_Entry(subCursor,
					       struct subTypeList, link);
		    retval->keys[count] = afsconf_typedKey_get(subEntry->key);
		    count++;
		}
	    }
	}
    } else {
	retval->keys = NULL;
    }

    *keys = retval;

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

int
afsconf_GetKeyByTypes(struct afsconf_dir *dir, afsconf_keyType type,
		      int kvno, int subType, struct afsconf_typedKey **key)
{
    int code = 0;
    struct subTypeList *subTypeEntry;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    subTypeEntry = findBySubType(dir, type, kvno, subType);
    if (subTypeEntry == NULL) {
	code = AFSCONF_NOTFOUND;
	goto out;
    }

    *key = afsconf_typedKey_get(subTypeEntry->key);

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

static struct kvnoList *
pickBestKvno(struct afsconf_dir *dir, afsconf_keyType type)
{
    struct keyTypeList *typeEntry;
    struct kvnoList    *kvnoEntry;

    typeEntry = findByType(dir, type);
    if (typeEntry == NULL)
	return NULL;

    /* We store all of the key lists ordered, so the last entry in the
     * kvno list must be the highest kvno. */

    kvnoEntry = opr_queue_Last(&typeEntry->kvnoList, struct kvnoList, link);

    /* Except, if we're in the rxkad list, we might have a bcrypt entry that
     * has a kvno of 999. So we need to skip that one
     */
    while (type == afsconf_rxkad && kvnoEntry->kvno == 999) {
	kvnoEntry = opr_queue_Prev(&typeEntry->kvnoList, struct kvnoList,
				   link);
	if (opr_queue_IsEnd(&typeEntry->kvnoList, &kvnoEntry->link))
	   return NULL;
    }

    return kvnoEntry;
}


int
afsconf_GetLatestKeysByType(struct afsconf_dir *dir, afsconf_keyType type,
			    struct afsconf_typedKeyList **keys)
{
    int code;
    struct kvnoList *kvnoEntry;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;


    kvnoEntry = pickBestKvno(dir, type);
    if (kvnoEntry == NULL) {
	code = AFSCONF_NOTFOUND;
	goto out;
    }

    code = listToArray(kvnoEntry, keys);

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

int
afsconf_GetLatestKeyByTypes(struct afsconf_dir *dir, afsconf_keyType type,
			    int subType, struct afsconf_typedKey **key)
{
    int code;
    struct kvnoList *kvnoEntry;
    struct subTypeList *subTypeEntry;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    kvnoEntry = pickBestKvno(dir, type);
    if (kvnoEntry == NULL) {
	code = AFSCONF_NOTFOUND;
	goto out;
    }

    subTypeEntry = findInKvnoList(kvnoEntry, subType);
    if (subTypeEntry == NULL) {
	code = AFSCONF_NOTFOUND;
	goto out;
    }

    *key = afsconf_typedKey_get(subTypeEntry->key);

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

void
afsconf_PutTypedKeyList(struct afsconf_typedKeyList **keys)
{
     int i;

     for (i=0;i<(*keys)->nkeys;i++)
	afsconf_typedKey_put(&((*keys)->keys[i]));

     if ((*keys)->keys != NULL)
	free((*keys)->keys);

     free(*keys);
     *keys = NULL;
}

static struct afsconf_typedKey *
afsconf_typedKey_blank(void)
{
    struct afsconf_typedKey *key;

    key = calloc(1, sizeof(struct afsconf_typedKey));
    if (key == NULL)
	return NULL;

    rx_atomic_set(&key->refcnt, 1);

    return key;
}

struct afsconf_typedKey *
afsconf_typedKey_new(afsconf_keyType type, int kvno, int subType,
		     struct rx_opaque *keyMaterial)
{
    struct afsconf_typedKey *key;
    int code;

    key = afsconf_typedKey_blank();
    if (key == NULL)
	return key;

    key->type = type;
    key->kvno = kvno;
    key->subType = subType;

    code = rx_opaque_copy(&key->key, keyMaterial);
    if (code != 0) {
	free(key);
	return NULL;
    }

    return key;
}

void
afsconf_typedKey_free(struct afsconf_typedKey **key)
{
    if (*key == NULL)
	return;
    rx_opaque_freeContents(&(*key)->key);
    free(*key);
    *key = NULL;
}

struct afsconf_typedKey *
afsconf_typedKey_get(struct afsconf_typedKey *key)
{
     rx_atomic_inc(&key->refcnt);
     return key;
}

void
afsconf_typedKey_put(struct afsconf_typedKey **key)
{
    if (rx_atomic_dec_and_read(&(*key)->refcnt) == 0)
	afsconf_typedKey_free(key);
    else
	*key = NULL;
}

void
afsconf_typedKey_values(struct afsconf_typedKey *key, afsconf_keyType *type,
			int *kvno, int *subType, struct rx_opaque **material)
{
    if (type != NULL)
	*type = key->type;
    if (kvno != NULL)
	*kvno = key->kvno;
    if (subType != NULL)
	*subType = key->subType;
    if (material != NULL)
	*material = &key->key;
}

int
afsconf_AddTypedKey(struct afsconf_dir *dir,
		    struct afsconf_typedKey *key,
		    int overwrite)
{
    int code;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    if (key->type == afsconf_rxkad) {
	/* There are restrictions on rxkad keys so that we can still
	 * return them using the old interface. We only enforce the
	 * same restrictions as that interface does - that is, we don't
	 * check that the key we're passed is a valid DES key */
	if (key->key.len != 8 || key->subType != 0) {
	    code = AFSCONF_BADKEY;
	    goto out;
	}
    }

    code = addMemoryKey(dir, key, overwrite);
    if (code)
	goto out;

    code = _afsconf_SaveKeys(dir);
    _afsconf_Touch(dir);

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

int
afsconf_DeleteKeyByType(struct afsconf_dir *dir,
		        afsconf_keyType type, int kvno)
{
    struct keyTypeList *typeEntry;
    struct kvnoList *kvnoEntry;
    int code;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    typeEntry = findByType(dir, type);
    if (typeEntry == NULL) {
	code = AFSCONF_NOTFOUND;
        goto out;
    }

    kvnoEntry = findInTypeList(typeEntry, kvno);
    if (kvnoEntry == NULL) {
	code = AFSCONF_NOTFOUND;
	goto out;
    }

    deleteKvnoEntry(kvnoEntry);

    /* Remove the typeEntry, if it has no sub elements */
    if (opr_queue_IsEmpty(&typeEntry->kvnoList)) {
	opr_queue_Remove(&typeEntry->link);
	free(typeEntry);
    }

    code = _afsconf_SaveKeys(dir);
    _afsconf_Touch(dir);

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

int
afsconf_DeleteKeyBySubType(struct afsconf_dir *dir,
			   afsconf_keyType type, int kvno, int subType)
{
    struct keyTypeList *typeEntry;
    struct kvnoList *kvnoEntry;
    struct subTypeList *subTypeEntry;
    int code;

    LOCK_GLOBAL_MUTEX;

    code = _afsconf_Check(dir);
    if (code)
	goto out;

    typeEntry = findByType(dir, type);
    if (typeEntry == NULL)
	return AFSCONF_NOTFOUND;

    kvnoEntry = findInTypeList(typeEntry, kvno);
    if (kvnoEntry == NULL)
	return AFSCONF_NOTFOUND;

    subTypeEntry = findInKvnoList(kvnoEntry, subType);
    if (subTypeEntry == NULL)
	return AFSCONF_NOTFOUND;

    /* Remove the subTypeEntry */
    afsconf_typedKey_put(&subTypeEntry->key);
    opr_queue_Remove(&subTypeEntry->link);
    free(subTypeEntry);

    /* Remove the kvnoEntry, if it has no sub elements */
    if (opr_queue_IsEmpty(&kvnoEntry->subTypeList)) {
	opr_queue_Remove(&kvnoEntry->link);
	free(kvnoEntry);
    }

    /* Remove the typeEntry, if it has no sub elements */
    if (opr_queue_IsEmpty(&typeEntry->kvnoList)) {
	opr_queue_Remove(&typeEntry->link);
	free(typeEntry);
    }

    code = _afsconf_SaveKeys(dir);
    _afsconf_Touch(dir);

out:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

int
afsconf_DeleteTypedKey(struct afsconf_dir *dir, struct afsconf_typedKey *key)
{
    return afsconf_DeleteKeyBySubType(dir, key->type, key->kvno, key->subType);
}
