/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * setgroups (syscall)
 * setpag
 *
 */
#include <afsconfig.h>
#include "afs/param.h"
#ifdef LINUX_KEYRING_SUPPORT
#include <linux/seq_file.h>
#endif


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */
#include "afs/nfsclient.h"
#include "osi_compat.h"

#ifdef AFS_LINUX26_ONEGROUP_ENV
# define NUMPAGGROUPS 1

static afs_uint32
afs_linux_pag_from_groups(struct group_info *group_info) {
    afs_uint32 g0 = 0;
    afs_uint32 i;

    if (group_info->ngroups < NUMPAGGROUPS)
	return NOPAG;

    for (i = 0; i < group_info->ngroups; i++) {
	g0 = afs_from_kgid(GROUP_AT(group_info, i));
	if (((g0 >> 24) & 0xff) == 'A')
	    return g0;
    }
    return NOPAG;
}

static inline void
afs_linux_pag_to_groups(afs_uint32 newpag,
			struct group_info *old, struct group_info **new) {
    int need_space = 0;
    int i;
    int j;
    afs_kgid_t newkgid = afs_make_kgid(newpag);

    if (afs_linux_pag_from_groups(old) == NOPAG)
	need_space = NUMPAGGROUPS;

    *new = groups_alloc(old->ngroups + need_space);

    for (i = 0, j = 0; i < old->ngroups; ++i) {
	afs_kgid_t ths = GROUP_AT(old, i);
	if ((afs_from_kgid(ths) >> 24) == 'A')
	    continue;
	if ((i == 0 || !gid_lt(newkgid, GROUP_AT(old, i-1))) &&
	    gid_lt(newkgid, ths)) {
	   GROUP_AT(*new, j) = newkgid;
	   j++;
	}
	GROUP_AT(*new, j) = ths;
	j++;
    }
    if (j != i + need_space)
        GROUP_AT(*new, j) = newkgid;
}

#else
# define NUMPAGGROUPS 2

static inline afs_uint32
afs_linux_pag_from_groups(struct group_info *group_info) {

    if (group_info->ngroups < NUMPAGGROUPS)
	return NOPAG;

    return afs_get_pag_from_groups(GROUP_AT(group_info, 0), GROUP_AT(group_info, 1));
}

static inline void
afs_linux_pag_to_groups(afs_uint32 newpag,
			struct group_info *old, struct group_info **new) {
    int need_space = 0;
    int i;
    gid_t g0;
    gid_t g1;

    if (afs_linux_pag_from_groups(old) == NOPAG)
	need_space = NUMPAGGGROUPS;

    *new = groups_alloc(old->ngroups + need_space);

    for (i = 0; i < old->ngroups; ++i)
          GROUP_AT(new, i + need_space) = GROUP_AT(old, i);

    afs_get_groups_from_pag(newpag, &g0, g1);
    GROUP_AT(new, 0) = g0;
    GROUP_AT(new, 1) = g1;
}
#endif

afs_int32
osi_get_group_pag(afs_ucred_t *cred) {
    return afs_linux_pag_from_groups(afs_cr_group_info(cred));
}


static int
afs_setgroups(cred_t **cr, struct group_info *group_info, int change_parent)
{
    struct group_info *old_info;

    AFS_STATCNT(afs_setgroups);

    old_info = afs_cr_group_info(*cr);
    get_group_info(group_info);
    afs_set_cr_group_info(*cr, group_info);
    put_group_info(old_info);

    crset(*cr);

#if defined(STRUCT_TASK_STRUCT_HAS_PARENT) && !defined(STRUCT_TASK_STRUCT_HAS_CRED)
    if (change_parent) {
	old_info = current->parent->group_info;
	get_group_info(group_info);
	current->parent->group_info = group_info;
	put_group_info(old_info);
    }
#endif

    return (0);
}

int
__setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
         int change_parent, struct group_info **old_groups)
{
    struct group_info *group_info;
    struct group_info *tmp;

    get_group_info(afs_cr_group_info(*cr));
    group_info = afs_cr_group_info(*cr);

    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_linux_pag_to_groups(*newpag, group_info, &tmp);

    if (old_groups) {
	*old_groups = group_info;
    } else {
	put_group_info(group_info);
	group_info = NULL;
    }

    afs_setgroups(cr, tmp, change_parent);

    put_group_info(tmp);

    return 0;
}

#ifdef LINUX_KEYRING_SUPPORT
extern struct key_type key_type_keyring __attribute__((weak));
static struct key_type *__key_type_keyring = &key_type_keyring;

/* install_session_keyring returns negative error values */
static int
install_session_keyring(struct key *keyring)
{
    struct key *old;
    char desc[20];
    int code = -EINVAL;
    unsigned long flags;

    if (!__key_type_keyring)
	return code;

    if (!keyring) {

	/* create an empty session keyring */
	sprintf(desc, "_ses.%u", current->tgid);

	/* if we're root, don't count the keyring against our quota. This
	 * avoids starvation issues when dealing with PAM modules that always
	 * setpag() as root */
	if (capable(CAP_SYS_ADMIN))
	    flags = KEY_ALLOC_NOT_IN_QUOTA;
	else
	    flags = KEY_ALLOC_IN_QUOTA;

	keyring = afs_linux_key_alloc(
			    __key_type_keyring, desc,
			    current_uid(), current_gid(),
			    (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_ALL,
			    flags);

	if (IS_ERR(keyring)) {
	    code = PTR_ERR(keyring);
	    goto out;
	}
    }

    code = key_instantiate_and_link(keyring, NULL, 0, NULL, NULL);
    if (code < 0) {
	key_put(keyring);
	goto out;
    }

    /* install the keyring */
    old = afs_set_session_keyring(keyring);
    if (old)
	key_put(old);

out:
    return code;
}
#endif /* LINUX_KEYRING_SUPPORT */

/* Error codes from setpag must be positive, otherwise they don't
 * make it back into userspace properly. Error codes from the
 * Linux keyring utilities, and from install_session_keyring()
 * are negative. So we need to be careful to convert them correctly
 * here
 */
int
setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
       int change_parent)
{
    int code;
    struct group_info *old_groups = NULL;

    AFS_STATCNT(setpag);

    code = __setpag(cr, pagvalue, newpag, change_parent, &old_groups);

#ifdef LINUX_KEYRING_SUPPORT
    if (code == 0 && afs_cr_rgid(*cr) != NFSXLATOR_CRED) {
	code = install_session_keyring(NULL);
	if (code == 0 && current_session_keyring()) {
	    struct key *key;
	    key_perm_t perm;

	    perm = KEY_POS_VIEW | KEY_POS_SEARCH;
	    perm |= KEY_USR_VIEW | KEY_USR_SEARCH;

	    key = afs_linux_key_alloc(&key_type_afs_pag, "_pag", GLOBAL_ROOT_UID, GLOBAL_ROOT_GID, perm, KEY_ALLOC_NOT_IN_QUOTA);

	    if (!IS_ERR(key)) {
		code = key_instantiate_and_link(key, (void *) newpag, sizeof(afs_uint32),
					 current_session_keyring(), NULL);
		key_put(key);
	    } else {
		code = PTR_ERR(key);
	    }
	}
	if (code)
	    code = -code;
    }
#endif /* LINUX_KEYRING_SUPPORT */

    if (code) {
	if (old_groups) {
	    afs_setgroups(cr, old_groups, change_parent);
	    put_group_info(old_groups);
	    old_groups = NULL;
	}
	if (*newpag > -1) {
	    afs_MarkUserExpired(*newpag);
	    *newpag = -1;
	}
    }

    return code;
}


/* Intercept the standard system call. */
extern asmlinkage long (*sys_setgroupsp) (int gidsetsize, gid_t * grouplist);
asmlinkage long
afs_xsetgroups(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    old_pag = PagInCred(cr);
    crfree(cr);

    code = (*sys_setgroupsp) (gidsetsize, grouplist);
    if (code) {
	return code;
    }

    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0, NULL);
    }
    crfree(cr);

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

/* Intercept the standard uid32 system call. */
extern asmlinkage int (*sys_setgroups32p) (int gidsetsize,
					   __kernel_gid32_t * grouplist);
asmlinkage long
afs_xsetgroups32(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    old_pag = PagInCred(cr);
    crfree(cr);

    code = (*sys_setgroups32p) (gidsetsize, grouplist);

    if (code) {
	return code;
    }

    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0, NULL);
    }
    crfree(cr);

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

#if defined(AFS_PPC64_LINUX20_ENV)
/* Intercept the uid16 system call as used by 32bit programs. */
extern asmlinkage long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);
asmlinkage long afs32_xsetgroups(int gidsetsize, gid_t *grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;
    
    old_pag = PagInCred(cr);
    crfree(cr);
    
    code = (*sys32_setgroupsp)(gidsetsize, grouplist);
    if (code) {
	return code;
    }
    
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0, NULL);
    }
    crfree(cr);
    
    /* Linux syscall ABI returns errno as negative */
    return (-code);
}
#endif

#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_AMD64_LINUX20_ENV)
/* Intercept the uid16 system call as used by 32bit programs. */
#ifdef AFS_AMD64_LINUX20_ENV
extern asmlinkage long (*sys32_setgroupsp) (int gidsetsize, u16 * grouplist);
#endif /* AFS_AMD64_LINUX20_ENV */
#ifdef AFS_SPARC64_LINUX26_ENV
extern asmlinkage int (*sys32_setgroupsp) (int gidsetsize,
					   __kernel_gid32_t * grouplist);
#endif /* AFS_SPARC64_LINUX26_ENV */
asmlinkage long
afs32_xsetgroups(int gidsetsize, u16 * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;
    
    old_pag = PagInCred(cr);
    crfree(cr);
    
    code = (*sys32_setgroupsp) (gidsetsize, grouplist);
    if (code) {
	return code;
    }
    
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0, NULL);
    }
    crfree(cr);
    
    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

/* Intercept the uid32 system call as used by 32bit programs. */
#ifdef AFS_AMD64_LINUX20_ENV
extern asmlinkage long (*sys32_setgroups32p) (int gidsetsize, gid_t * grouplist);
#endif /* AFS_AMD64_LINUX20_ENV */
#ifdef AFS_SPARC64_LINUX26_ENV
extern asmlinkage int (*sys32_setgroups32p) (int gidsetsize,
					     __kernel_gid32_t * grouplist);
#endif /* AFS_SPARC64_LINUX26_ENV */
asmlinkage long
afs32_xsetgroups32(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    old_pag = PagInCred(cr);
    crfree(cr);

    code = (*sys32_setgroups32p) (gidsetsize, grouplist);
    if (code) {
	return code;
    }

    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0, NULL);
    }
    crfree(cr);

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}
#endif


#ifdef LINUX_KEYRING_SUPPORT
static void afs_pag_describe(const struct key *key, struct seq_file *m)
{
    seq_puts(m, key->description);

    seq_printf(m, ": %u", key->datalen);
}

#if defined(STRUCT_KEY_TYPE_HAS_PREPARSE)
static int afs_pag_instantiate(struct key *key, struct key_preparsed_payload *prep)
#else
static int afs_pag_instantiate(struct key *key, const void *data, size_t datalen)
#endif
{
    int code;
    afs_uint32 *userpag, pag = NOPAG;

    if (!uid_eq(key->uid, GLOBAL_ROOT_UID) || !gid_eq(key->gid, GLOBAL_ROOT_GID))
	return -EPERM;

    code = -EINVAL;
    get_group_info(current_group_info());

#if defined(STRUCT_KEY_TYPE_HAS_PREPARSE)
    if (prep->datalen != sizeof(afs_uint32) || !prep->data)
#else
    if (datalen != sizeof(afs_uint32) || !data)
#endif
	goto error;

    /* ensure key being set matches current pag */
    pag = afs_linux_pag_from_groups(current_group_info());

    if (pag == NOPAG)
	goto error;

#if defined(STRUCT_KEY_TYPE_HAS_PREPARSE)
    userpag = (afs_uint32 *)prep->data;
#else
    userpag = (afs_uint32 *)data;
#endif
    if (*userpag != pag)
	goto error;

#if defined(STRUCT_KEY_HAS_PAYLOAD_VALUE)
    key->payload.value = (unsigned long) *userpag;
#else
    memcpy(&key->payload, userpag, sizeof(afs_uint32));
#endif
    key->datalen = sizeof(afs_uint32);
    code = 0;

error:
    put_group_info(current_group_info());
    return code;
}

#if !defined(STRUCT_KEY_TYPE_HAS_MATCH_PREPARSE)
/* Note that we only define a ->match function if struct
 * key_type.match_preparse does _not_ exist. If key_type.match_preparse does
 * exist, we would use that to specify an alternative comparison function; but
 * since we just rely on default behavior, we don't need to actually specify
 * one. But for kernels with no such match_preparse function, we need to
 * specify a 'match' function, since there is no default. */
static int afs_pag_match(const struct key *key, const void *description)
{
	return strcmp(key->description, description) == 0;
}
#endif

static void afs_pag_destroy(struct key *key)
{
    afs_uint32 pag;
    int locked = ISAFS_GLOCK();

#if defined(STRUCT_KEY_HAS_PAYLOAD_VALUE)
    pag = key->payload.value;
#else
    memcpy(&pag, &key->payload, sizeof(afs_uint32));
#endif

    if (!locked)
	AFS_GLOCK();

    afs_MarkUserExpired(pag);

    if (!locked)
	AFS_GUNLOCK();
}

struct key_type key_type_afs_pag =
{
    .name        = "afs_pag",
    .describe    = afs_pag_describe,
#if defined(STRUCT_KEY_TYPE_HAS_INSTANTIATE_PREP)
    .instantiate_prep = afs_pag_instantiate,
    .instantiate = NULL,
#else
    .instantiate = afs_pag_instantiate,
#endif
#if !defined(STRUCT_KEY_TYPE_HAS_MATCH_PREPARSE)
    .match       = afs_pag_match,
#endif
    .destroy     = afs_pag_destroy,
};

#ifdef EXPORTED_TASKLIST_LOCK
extern rwlock_t tasklist_lock __attribute__((weak));
#endif

void osi_keyring_init(void)
{
#if !defined(EXPORTED_KEY_TYPE_KEYRING)
    struct task_struct *p;

    /* If we can't lock the tasklist, either with its explicit lock,
     * or by using the RCU lock, then we can't safely work out the 
     * type of a keyring. So, we have to rely on the weak reference. 
     * If that's not available, then keyring based PAGs won't work.
     */
    
#if defined(EXPORTED_TASKLIST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && defined(HAVE_LINUX_RCU_READ_LOCK))
    if (__key_type_keyring == NULL) {
# ifdef EXPORTED_TASKLIST_LOCK
	if (&tasklist_lock)
	    read_lock(&tasklist_lock);
# endif
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && defined(HAVE_LINUX_RCU_READ_LOCK))
#  ifdef EXPORTED_TASKLIST_LOCK
 	else
#  endif
	    rcu_read_lock();
# endif
#if defined(HAVE_LINUX_FIND_TASK_BY_PID)
	p = find_task_by_pid(1);
#else
	p = find_task_by_vpid(1);
#endif
	if (p && task_user(p)->session_keyring)
	    __key_type_keyring = task_user(p)->session_keyring->type;
# ifdef EXPORTED_TASKLIST_LOCK
	if (&tasklist_lock)
	    read_unlock(&tasklist_lock);
# endif
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && defined(HAVE_LINUX_RCU_READ_LOCK))
#  ifdef EXPORTED_TASKLIST_LOCK
	else
#  endif
	    rcu_read_unlock();
# endif
    }
#endif
#endif

    register_key_type(&key_type_afs_pag);
}

void osi_keyring_shutdown(void)
{
    unregister_key_type(&key_type_afs_pag);
}

afs_int32
osi_get_keyring_pag(afs_ucred_t *cred)
{
    struct key *key;
    afs_uint32 newpag;
    afs_int32 keyring_pag = NOPAG;

    if (afs_cr_rgid(cred) != NFSXLATOR_CRED) {
	key = afs_linux_search_keyring(cred, &key_type_afs_pag);

	if (!IS_ERR(key)) {
	    if (key_validate(key) == 0 && uid_eq(key->uid, GLOBAL_ROOT_UID)) {      /* also verify in the session keyring? */
#if defined(STRUCT_KEY_HAS_PAYLOAD_VALUE)
		keyring_pag = key->payload.value;
#else
		memcpy(&keyring_pag, &key->payload, sizeof(afs_int32));
#endif
		/* Only set PAG in groups if needed,
		 * and the creds are from the current process */
		if (afs_linux_cred_is_current(cred) &&
                    ((keyring_pag >> 24) & 0xff) == 'A' &&
		    keyring_pag != afs_linux_pag_from_groups(current_group_info())) {
			__setpag(&cred, keyring_pag, &newpag, 0, NULL);
		}
	    }
	    key_put(key);
	}
    }
    return keyring_pag;
}

#else
void osi_keyring_init(void)
{
	return;
}

void osi_keyring_shutdown(void)
{
	return;
}
#endif
