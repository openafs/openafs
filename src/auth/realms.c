/*
 * Copyright 2012, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>
#include <opr/queue.h>
#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <afs/afsutil.h>
#include <ctype.h>
#include <search.h>
#include "cellconfig.h"
#include "internal.h"

#define MAXLINESIZE 2047

/* Can be set during initialization, overriding the krb.conf file. */
static struct opr_queue *lrealms = NULL;

/**
 * Realm and exclusion list entries.
 */
struct afsconf_realm_entry {
    struct opr_queue link;  /**< linked list header */
    char *value;	    /**< local realm or principal */
};

/**
 * Realm and exclusion lists.
 */
struct afsconf_realms {
    struct opr_queue list;  /**< list of afsconf_realm_entry */
    int time_read;	    /**< time when read from file */
    void *tree;		    /**< for lookup */
    int (*compare) (const void *, const void *); /**< compare entries */
};

static int
compare_realms(const void *a, const void *b)
{
    return strcasecmp((char *)a, (char *)b);
}

static int
compare_principals(const void *a, const void *b)
{
    return strcmp((char *)a, (char *)b);
}

/**
 * Format the k4-style principal string.
 *
 * @param[out] pvname output buffer, must be freed by caller
 * @param[in]  name   user name, required
 * @param[in]  inst   user instance, optional
 * @param[in]  cell   cell name, optional
 *
 * @return status
 *   @retval 0 success
 *   @retval EINVAL invalid arguments
 *   @retval E2BIG insufficient output buffer space
 *
 * @internal
 */
static int
create_name(char **pvname, const char *name,
	    const char *inst, const char *cell)
{
    int code = 0;

    if (!name || !*name) {
	return EINVAL;
    }
    if (cell && *cell) {
	if (inst && *inst) {
	    code = asprintf(pvname, "%s.%s@%s", name, inst, cell);
	} else {
	    code = asprintf(pvname, "%s@%s", name, cell);
	}
    } else {
	if (inst && *inst) {
	    code = asprintf(pvname, "%s.%s", name, inst);
	} else {
	    code = asprintf(pvname, "%s", name);
	}
    }
    return (code < 0 ? ENOMEM : 0);
}

/**
 * Parse whitespace delimited values
 *
 * @param[in]    buffer    input string
 * @param[out]   result    output string
 * @param[in]    size      size of result buffer
 *
 * @return pointer to the next value
 *
 * @internal
 */
static char *
parse_str(char *buffer, char *result, int size)
{
    int n = 0;

    if (!buffer)
	goto cleanup;

    while (*buffer && isspace(*buffer))
	buffer++;
    while (*buffer && !isspace(*buffer)) {
	if (n < size - 1) {
	    *result++ = *buffer++;
	    n++;
	} else {
	    buffer++;
	}
    }

  cleanup:
    *result = '\0';
    return buffer;
}

/**
 * Add a new list element.
 *
 * Add the name element if not already present in the list.
 * The names are case insensitive.
 *
 * @param[inout] list  list of name elements
 * @param[in]    name  name to add
 *
 * @return status
 *   @retval 0 success
 *   @retval ENOMEM unable to allocate new entry
 *
 * @internal
 */
static int
add_entry(struct opr_queue *list, const char *name)
{
    struct afsconf_realm_entry *entry;

    entry = malloc(sizeof(struct afsconf_realm_entry));
    if (!entry) {
	return ENOMEM;
    }
    entry->value = strdup(name);
    opr_queue_Append(list, (struct opr_queue *)entry);
    return 0;
}

/**
 * Free all entries in a list.
 *
 * @param[in] list  list of entries
 *
 * @return none
 *
 * @internal
 */
static void
free_realm_entries(struct opr_queue *list)
{
    struct afsconf_realm_entry *entry;

    while (!opr_queue_IsEmpty(list)) {
	entry = opr_queue_First(list, struct afsconf_realm_entry, link);
	opr_queue_Remove(&entry->link);
	if (entry->value) {
	    free(entry->value);
	}
	free(entry);
    }
}

#if HAVE_TDESTROY
/**
 * Placeholder for tdestroy.
 */
static void
free_tree_node(void *nodep)
{
    return;			/* empty */
}
#endif

/**
 * Delete all the entries from the search tree.
 *
 * @param[in] list  list of entries
 *
 * @return none
 *
 * @internal
 */
/*static*/ void
destroy_tree(struct afsconf_realms *entries)
{
    if (entries->tree) {
#if HAVE_TDESTROY
	tdestroy(entries->tree, free_tree_node);
#else
	struct opr_queue *cursor;
	struct afsconf_realm_entry *entry;

	for (opr_queue_Scan(&entries->list, cursor)) {
	    entry = opr_queue_Entry(cursor, struct afsconf_realm_entry, link);
	    tdelete(entry->value, &entries->tree, entries->compare);
	}
#endif
	entries->tree = NULL;
    }
}

/**
 * Build a search tree from the list of entries.
 *
 * @param[in] list  list of entries
 *
 * @return none
 *
 * @internal
 */
static void
build_tree(struct afsconf_realms *entries)
{
    struct opr_queue *cursor;
    struct afsconf_realm_entry *entry;

    for (opr_queue_Scan(&entries->list, cursor)) {
	entry = opr_queue_Entry(cursor, struct afsconf_realm_entry, link);
	tsearch(entry->value, &entries->tree, entries->compare);
    }
}

/**
 * Read the list of local realms from a config file.
 *
 * @param[inout]  dir   config dir object
 *
 * @return status
 *
 * @internal
 */
static int
read_local_realms(struct afsconf_realms *entries, const char *path)
{
    int code = 0;
    char realm[AFS_REALM_SZ];
    struct opr_queue temp;
    char *filename = NULL;
    struct stat tstat;
    FILE *cnffile = NULL;
    char *linebuf = NULL;
    char *p;

    opr_queue_Init(&temp);
    code = asprintf(&filename, "%s/%s", path, AFSDIR_KCONF_FILE);
    if (code < 0) {
	code = ENOMEM;
	goto done;
    }
    code = stat(filename, &tstat);
    if (code < 0) {
	code = (errno == ENOENT ? 0 : errno);	/* this file is optional */
	goto done;
    }
    if (tstat.st_mtime == entries->time_read) {
	code = 0;
	goto done;
    }
    entries->time_read = tstat.st_mtime;
    if ((cnffile = fopen(filename, "r")) == NULL) {
	code = (errno == ENOENT ? 0 : errno);	/* this file is optional */
	goto done;
    }
    linebuf = malloc(sizeof(char) * (MAXLINESIZE + 1));
    if (!linebuf) {
	code = ENOMEM;
	goto done;
    }
    if (fgets(linebuf, MAXLINESIZE, cnffile) == NULL) {
	code = errno;
	goto done;
    }
    linebuf[MAXLINESIZE] = '\0';
    for (p = linebuf; *p;) {
	p = parse_str(p, realm, AFS_REALM_SZ);
	if (*realm) {
	    code = add_entry(&temp, realm);
	    if (code) {
		goto done;
	    }
	}
    }
    destroy_tree(entries);
    opr_queue_Swap(&temp, &entries->list);
    build_tree(entries);

  done:
    free_realm_entries(&temp);
    if (filename) {
	free(filename);
    }
    if (linebuf) {
	free(linebuf);
    }
    if (cnffile) {
	fclose(cnffile);
    }
    return code;
}

/**
 * Read the list of local exclusions from a config file.
 *
 * @param[inout]  dir   config dir object
 *
 * @return status
 *
 * @internal
 */
static int
read_local_exclusions(struct afsconf_realms *entries, const char *path)
{
    int code = 0;
    char *linebuf = NULL;
    char *filename = NULL;
    char name[256];
    FILE *cnffile = NULL;
    struct opr_queue temp;
    struct stat tstat;

    opr_queue_Init(&temp);
    code = asprintf(&filename, "%s/%s", path, AFSDIR_KRB_EXCL_FILE);
    if (code < 0) {
	code = ENOMEM;
	goto done;
    }
    code = stat(filename, &tstat);
    if (code < 0) {
	code = (errno == ENOENT ? 0 : errno);	/* this file is optional */
	goto done;
    }
    if (tstat.st_mtime == entries->time_read) {
	code = 0;
	goto done;
    }
    if ((cnffile = fopen(filename, "r")) == NULL) {
	code = (errno != ENOENT ? errno : 0);	/* this file is optional */
	goto done;
    }
    linebuf = malloc(sizeof(char) * (MAXLINESIZE + 1));
    if (!linebuf) {
	code = ENOMEM;
	goto done;
    }
    for (;;) {
	if (fgets(linebuf, MAXLINESIZE, cnffile) == NULL) {
	    break;
	}
	linebuf[MAXLINESIZE] = '\0';
	parse_str(linebuf, name, sizeof(name));
	if (*name) {
	    code = add_entry(&temp, name);
	    if (code) {
		goto done;
	    }
	}
    }
    destroy_tree(entries);
    opr_queue_Swap(&temp, &entries->list);
    build_tree(entries);
  done:
    free_realm_entries(&temp);
    if (filename) {
	free(filename);
    }
    if (linebuf) {
	free(linebuf);
    }
    if (cnffile) {
	fclose(cnffile);
    }
    return code;
}


/**
 * Free the local realms and exclusions lists.
 *
 * @param[in] dir afsconf dir object
 *
 * @return none
 *
 * @internal
 */
void
_afsconf_FreeRealms(struct afsconf_dir *dir)
{
    if (dir) {
	if (dir->local_realms) {
	    destroy_tree(dir->local_realms);
	    free_realm_entries(&dir->local_realms->list);
	    free(dir->local_realms);
	    dir->local_realms = NULL;
	}
	if (dir->exclusions) {
	    destroy_tree(dir->exclusions);
	    free_realm_entries(&dir->exclusions->list);
	    free(dir->exclusions);
	    dir->exclusions = NULL;
	}
    }
}

/**
 * Load the local realms and exclusions lists.
 *
 * @param[in] dir afsconf dir object
 *
 * @return none
 *
 * @internal
 */
int
_afsconf_LoadRealms(struct afsconf_dir *dir)
{
    int code = 0;
    struct afsconf_realms *local_realms = NULL;
    struct afsconf_realms *exclusions = NULL;

    /* Create and load the list of local realms. */
    local_realms = calloc(1, sizeof(struct afsconf_realms));
    if (!local_realms) {
	code = ENOMEM;
	goto cleanup;
    }
    opr_queue_Init(&local_realms->list);
    local_realms->compare = compare_realms;

    if (!lrealms) {
	code = read_local_realms(local_realms, dir->name);
	if (code) {
	    goto cleanup;
	}
    } else {
	struct opr_queue *cursor;
	struct afsconf_realm_entry *entry;
	for (opr_queue_Scan(lrealms, cursor)) {
	    entry = opr_queue_Entry(cursor, struct afsconf_realm_entry, link);
	    code = add_entry(&local_realms->list, entry->value);
	    if (code) {
		goto cleanup;
	    }
	}
	build_tree(local_realms);
    }

    /* Create and load the list of excluded principals. */
    exclusions = calloc(1, sizeof(struct afsconf_realms));
    if (!exclusions) {
	code = ENOMEM;
	goto cleanup;
    }
    opr_queue_Init(&exclusions->list);
    exclusions->compare = compare_principals;
    code = read_local_exclusions(exclusions, dir->name);
    if (code) {
	goto cleanup;
    }

    dir->local_realms = local_realms;
    dir->exclusions = exclusions;
    return 0;

  cleanup:
    if (local_realms) {
	destroy_tree(local_realms);
	free_realm_entries(&local_realms->list);
	free(local_realms);
    }
    if (exclusions) {
	destroy_tree(dir->exclusions);
	free_realm_entries(&exclusions->list);
	free(exclusions);
    }
    return code;
}

/**
 * Set a local realm, instead of retrieving the local realms from the
 * configuration file krb.conf (if it exists).  Maybe called multiple
 * times during application initialization to set one or more local
 * realms.
 *
 * @return status
 *   @retval 0 success
 *   @retval ENOMEM unable to allocate new entry
 */
int
afsconf_SetLocalRealm(const char *realm)
{
    int code = 0;

    LOCK_GLOBAL_MUTEX;
    if (!lrealms) {
	lrealms = malloc(sizeof(struct opr_queue));
	if (!lrealms) {
	    code = ENOMEM;
	    goto done;
	}
	opr_queue_Init(lrealms);
    }
    code = add_entry(lrealms, realm);
  done:
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

/**
 * Determine if a principal is local to this cell.
 *
 * @param[in]  dir     afsconf dir object
 * @param[out] plocal  set to 1 if user is local, 0 if foreign
 * @param[in]  name    user name
 * @param[in]  inst    user instance
 * @param[in]  cell    user cell name
 *
 * @returns status
 *   @retval 0 success
 *   @retval ENOMEM unable to allocate memory
 *   @retval EINVAL invalid argument
 */
int
afsconf_IsLocalRealmMatch(struct afsconf_dir *dir, afs_int32 * plocal,
			  const char *name, const char *inst,
			  const char *cell)
{
    int code = 0;
    char *localcell = NULL;
    char *tvname = NULL;
    struct afsconf_realms *local_realms = NULL;
    struct afsconf_realms *exclusions = NULL;

    if (!name)
	return EINVAL;

    if (!cell || !*cell) {
	*plocal = 1;
	return code;
    }

    LOCK_GLOBAL_MUTEX;
    code = _afsconf_GetLocalCell(dir, &localcell, 1);
    if (code)
	goto done;

    /* Does the cell match the local cell name? */
    if (strcasecmp(localcell, cell) == 0) {
	*plocal = 1;		/* cell matches the local cell name. */
	goto done;
    }

    /* Does the cell match one of the local_realms? */
    local_realms = dir->local_realms;
    if (!tfind(cell, &local_realms->tree, local_realms->compare)) {
	*plocal = 0;		/* Cell name not found in local realms. */
	goto done;
    }

    /* Local realm matches, make sure the principal is not in the
     * exclusion list, if one. */
    exclusions = dir->exclusions;
    if (!exclusions->tree) {
	*plocal = 1;		/* Matches one of the local realms; no exclusions */
	goto done;
    }

    /* Create a full principal name for the exclusion check. */
    code = create_name(&tvname, name, inst, cell);
    if (!code) {
	if (tfind(tvname, &exclusions->tree, exclusions->compare)) {
	    *plocal = 0;	/* name found in the exclusion list */
	} else {
	    *plocal = 1;	/* not in the exclusion list */
	}
    }
    if (tvname)
	free(tvname);

  done:
    UNLOCK_GLOBAL_MUTEX;

    return code;
}
