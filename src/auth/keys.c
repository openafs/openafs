/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

/* Need rx/rx.h to get working assert(), used by LOCK_GLOBAL_MUTEX */
#include <rx/rx.h>

#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <afs/afsutil.h>

#include "cellconfig.h"
#include "keys.h"
#include "internal.h"

/* called during opening of config file */
int
_afsconf_IntGetKeys(struct afsconf_dir *adir)
{
    char tbuffer[256];
    int fd;
    struct afsconf_keys *tstr;
    afs_int32 code;

#ifdef AFS_NT40_ENV
    /* NT client config dir has no KeyFile; don't risk attempting open
     * because there might be a random file of this name if dir is shared.
     */
    if (_afsconf_IsClientConfigDirectory(adir->name)) {
	adir->keystr = ((struct afsconf_keys *)
			malloc(sizeof(struct afsconf_keys)));
	adir->keystr->nkeys = 0;
	return 0;
    }
#endif /* AFS_NT40_ENV */

    LOCK_GLOBAL_MUTEX;
    /* compute the key name and other setup */
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_KEY_FILE, NULL);
    tstr = (struct afsconf_keys *)malloc(sizeof(struct afsconf_keys));
    adir->keystr = tstr;

    /* read key file */
    fd = open(tbuffer, O_RDONLY);
    if (fd < 0) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }
    code = read(fd, tstr, sizeof(struct afsconf_keys));
    close(fd);
    if (code < sizeof(afs_int32)) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }

    /* convert key structure to host order */
    tstr->nkeys = ntohl(tstr->nkeys);

    if (code < sizeof(afs_int32) + (tstr->nkeys*sizeof(struct afsconf_key))) {
	tstr->nkeys = 0;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }

    for (fd = 0; fd < tstr->nkeys; fd++)
	tstr->key[fd].kvno = ntohl(tstr->key[fd].kvno);

    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* get keys structure */
int
afsconf_GetKeys(struct afsconf_dir *adir, struct afsconf_keys *astr)
{
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code = _afsconf_Check(adir);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    memcpy(astr, adir->keystr, sizeof(struct afsconf_keys));
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

/* get latest key */
afs_int32
afsconf_GetLatestKey(struct afsconf_dir * adir, afs_int32 * avno,
		     struct ktc_encryptionKey *akey)
{
    int i;
    int maxa;
    struct afsconf_key *tk;
    afs_int32 best;
    struct afsconf_key *bestk;
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code = _afsconf_Check(adir);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    maxa = adir->keystr->nkeys;

    best = -1;			/* highest kvno we've seen yet */
    bestk = (struct afsconf_key *)0;	/* ptr to structure providing best */
    for (tk = adir->keystr->key, i = 0; i < maxa; i++, tk++) {
	if (tk->kvno == 999)
	    continue;		/* skip bcrypt keys */
	if (tk->kvno > best) {
	    best = tk->kvno;
	    bestk = tk;
	}
    }
    if (bestk) {		/* found any  */
	if (akey)
	    memcpy(akey, bestk->key, 8);	/* copy out latest key */
	if (avno)
	    *avno = bestk->kvno;	/* and kvno to caller */
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }
    UNLOCK_GLOBAL_MUTEX;
    return AFSCONF_NOTFOUND;	/* didn't find any keys */
}

/* get a particular key */
int
afsconf_GetKey(void *rock, int avno, struct ktc_encryptionKey *akey)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) rock;
    int i, maxa;
    struct afsconf_key *tk;
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code = _afsconf_Check(adir);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_FAILURE;
    }
    maxa = adir->keystr->nkeys;

    for (tk = adir->keystr->key, i = 0; i < maxa; i++, tk++) {
	if (tk->kvno == avno) {
	    memcpy(akey, tk->key, 8);
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;
	}
    }

    UNLOCK_GLOBAL_MUTEX;
    return AFSCONF_NOTFOUND;
}

/* save the key structure in the appropriate file */
static int
SaveKeys(struct afsconf_dir *adir)
{
    struct afsconf_keys tkeys;
    int fd;
    afs_int32 i;
    char tbuffer[256];

    memcpy(&tkeys, adir->keystr, sizeof(struct afsconf_keys));

    /* convert it to net byte order */
    for (i = 0; i < tkeys.nkeys; i++)
	tkeys.key[i].kvno = htonl(tkeys.key[i].kvno);
    tkeys.nkeys = htonl(tkeys.nkeys);

    /* rewrite keys file */
    strcompose(tbuffer, 256, adir->name, "/", AFSDIR_KEY_FILE, NULL);
    fd = open(tbuffer, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
	return AFSCONF_FAILURE;
    i = write(fd, &tkeys, sizeof(tkeys));
    if (i != sizeof(tkeys)) {
	close(fd);
	return AFSCONF_FAILURE;
    }
    if (close(fd) < 0)
	return AFSCONF_FAILURE;
    return 0;
}

int
afsconf_AddKey(struct afsconf_dir *adir, afs_int32 akvno, char akey[8],
	       afs_int32 overwrite)
{
    struct afsconf_keys *tk;
    struct afsconf_key *tkey;
    afs_int32 i;
    int foundSlot;

    LOCK_GLOBAL_MUTEX;
    tk = adir->keystr;

    if (akvno != 999) {
	if (akvno < 0 || akvno > 255) {
	    UNLOCK_GLOBAL_MUTEX;
	    return ERANGE;
	}
    }
    foundSlot = 0;
    for (i = 0, tkey = tk->key; i < tk->nkeys; i++, tkey++) {
	if (tkey->kvno == akvno) {
	    if (!overwrite) {
		UNLOCK_GLOBAL_MUTEX;
		return AFSCONF_KEYINUSE;
	    }
	    foundSlot = 1;
	    break;
	}
    }
    if (!foundSlot) {
	if (tk->nkeys >= AFSCONF_MAXKEYS) {
	    UNLOCK_GLOBAL_MUTEX;
	    return AFSCONF_FULL;
	}
	tkey = &tk->key[tk->nkeys++];
    }
    tkey->kvno = akvno;
    memcpy(tkey->key, akey, 8);
    i = SaveKeys(adir);
    _afsconf_Touch(adir);
    UNLOCK_GLOBAL_MUTEX;
    return i;
}

/* this proc works by sliding the other guys down, rather than using a funny
    kvno value, so that callers can count on getting a good key in key[0].
*/
int
afsconf_DeleteKey(struct afsconf_dir *adir, afs_int32 akvno)
{
    struct afsconf_keys *tk;
    struct afsconf_key *tkey;
    int i;
    int foundFlag = 0;

    LOCK_GLOBAL_MUTEX;
    tk = adir->keystr;

    for (i = 0, tkey = tk->key; i < tk->nkeys; i++, tkey++) {
	if (tkey->kvno == akvno) {
	    foundFlag = 1;
	    break;
	}
    }
    if (!foundFlag) {
	UNLOCK_GLOBAL_MUTEX;
	return AFSCONF_NOTFOUND;
    }

    /* otherwise slide the others down.  i and tkey point at the guy to delete */
    for (; i < tk->nkeys - 1; i++, tkey++) {
	tkey->kvno = (tkey + 1)->kvno;
	memcpy(tkey->key, (tkey + 1)->key, 8);
    }
    tk->nkeys--;
    i = SaveKeys(adir);
    _afsconf_Touch(adir);
    UNLOCK_GLOBAL_MUTEX;
    return i;
}
