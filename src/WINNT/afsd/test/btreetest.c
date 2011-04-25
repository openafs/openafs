#include<windows.h>
#include<stdio.h>
#include<wchar.h>
#include<strsafe.h>

#include "..\afsd.h"

#define paste(a,b) a ## b
#define _L(a) paste(L,a)

#define TRACE1 wprintf

/* Setup and Fakery ... */
osi_log_t * afsd_logp;

void cm_SetFid(cm_fid_t *fidp, afs_uint32 cell, afs_uint32 volume, afs_uint32 vnode, afs_uint32 unique)
{
    fidp->cell = cell;
    fidp->volume = volume;
    fidp->vnode = vnode;
    fidp->unique = unique;
    fidp->hash = ((cell & 0xF) << 28) | ((volume & 0x3F) << 22) | ((vnode & 0x7FF) << 11) | (unique & 0x7FF);
}

cm_scache_t *cm_FindSCache(cm_fid_t *fidp)
{
    return NULL;
}

void cm_ReleaseSCache(cm_scache_t *scp)
{
}


long cm_ApplyDir(cm_scache_t *scp, cm_DirFuncp_t funcp, void *parmp,
                 osi_hyper_t *startOffsetp, cm_user_t *userp, cm_req_t *reqp,
                 cm_scache_t **retscp)
{
    return 0;
}

void
afsi_log(char *pattern, ...)
{
    char s[256], t[100], d[100], u[512];
    va_list ap;
    va_start(ap, pattern);

    StringCbVPrintfA(s, sizeof(s), pattern, ap);
    GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, t, sizeof(t));
    GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, d, sizeof(d));
    StringCbPrintfA(u, sizeof(u), "%s %s: %s\r\n", d, t, s);
    printf("%s", u);
}


static int initialize_tests(void)
{
    osi_Init();
    cm_InitNormalization();

    if (!cm_InitBPlusDir()) {
        TRACE1(L"Can't initialize BPlusDir\n");
        return 1;
    }

    afsd_logp = osi_LogCreate("fakelog", 100);

    return 0;
}

int n_succeeded = 0;
int n_failed = 0;

int n_subSucceeded = 0;
int n_subFailed = 0;

static int runtest(wchar_t * testname, int (*f)(void))
{
    int rv = 0;

    n_subSucceeded = 0;
    n_subFailed = 0;

    TRACE1(L"Begin test %s\n", testname);
    f();
    TRACE1(L"End test %s\n", testname);
    TRACE1(L"Subtests Succeeded %d, Failed %d\n", n_subSucceeded, n_subFailed);

    if (n_subFailed)
        n_failed++;
    else
        n_succeeded++;

    return rv;
}

#define RUNTEST(f) runtest(_L(#f), f)

#define IS_NOT_NULL(v) (((v) != NULL)? n_subSucceeded++: (TRACE1(L"Failed %s\n", _L(#v)), n_subFailed++))
#define IS(e) ((e)? n_subSucceeded++ : (TRACE1(L"Failed %s\n", _L(#e)), n_subFailed++))
#define CHECK_IF(e) do { if (e) { n_subSucceeded++; } else { TRACE1(L"Failed %s\n", _L(#e)); n_subFailed++; return 1; }} while (0)

/**************************************************************/
/* Actual tests */

struct strings {
    const fschar_t * str;
    const clientchar_t * lookup;
    int rc;
};

struct strings simple[] = {
    {"abc", L"ABC", CM_ERROR_INEXACT_MATCH},
    {"A", L"A", 0},
    {".", L".", 0},
    {"567", L"567", 0},
    {"b", L"B", CM_ERROR_INEXACT_MATCH},
    {"d", L"D", CM_ERROR_INEXACT_MATCH},
    {"àáâ", L"\x00c0\x00c1\x00c2", CM_ERROR_INEXACT_MATCH},
    {"Ŷ", L"\x0177", CM_ERROR_INEXACT_MATCH},
    {"a\xef\xac\xb4",L"a\xfb34",0},
    {"b\xd7\x94\xd6\xbc",L"b\xfb34",0},
    {"c\xef\xac\xb4",L"c\x05d4\x05bc",0},
    {"d\xd7\x94\xd6\xbc",L"d\x05d4\x05bc",0},
};

void init_scache(cm_scache_t * scp, cm_fid_t * fidp)
{
    memset(scp, 0, sizeof(cm_scache_t));
    scp->magic = CM_SCACHE_MAGIC;
    lock_InitializeRWLock(&scp->rw, "cm_scache_t rw");
    lock_InitializeRWLock(&scp->bufCreateLock, "cm_scache_t bufCreateLock");
    lock_InitializeRWLock(&scp->dirlock, "cm_scache_t dirlock");
    scp->serverLock = -1;
    scp->fid = *fidp;
    scp->refCount = 1;
}

int simple_test(void)
{
    Tree * t;
    int i;

    t = initBtree(64, MAX_FANOUT, cm_BPlusCompareNormalizedKeys);
    CHECK_IF(t != NULL);

    for (i=0; i < lengthof(simple); i++) {
        normchar_t * norm;
        keyT key;
        dataT data;

        norm = cm_FsStringToNormStringAlloc(simple[i].str, -1, NULL);
        CHECK_IF(norm != NULL);

        key.name = norm;

        data.cname = cm_FsStringToClientStringAlloc(simple[i].str, -1, NULL);
        data.fsname = cm_FsStrDup(simple[i].str);
        data.shortform = FALSE;
        cm_SetFid(&data.fid, 1, 2, i, 4);

        insert(t, key, data);

        if (!cm_Is8Dot3(data.cname)) {
            wchar_t wshortName[13];
            cm_dirFid_t dfid;

            dfid.vnode = i;
            dfid.unique = 0;

            cm_Gen8Dot3NameIntW(data.cname, &dfid, wshortName, NULL);

            key.name = wshortName;
            data.cname = cm_FsStringToClientStringAlloc(simple[i].str, -1, NULL);
            data.fsname = cm_FsStrDup(simple[i].str);
            data.shortform = TRUE;

            insert(t, key, data);
        }

        free(norm);
    }

    for (i=0; i < lengthof(simple); i++) {
        int rc = EINVAL;
        normchar_t * entry = NULL;
        keyT key = {NULL};
        Nptr leafNode = NONODE;
        cm_fid_t fid;
        cm_fid_t * cfid = &fid;

        TRACE1(L"Test row %d\n", i);

        entry = cm_ClientStringToNormStringAlloc(simple[i].lookup, -1, NULL);
        key.name = entry;

        leafNode = bplus_Lookup(t, key);
        if (leafNode != NONODE) {
            int         slot;
            Nptr        firstDataNode, dataNode, nextDataNode;
            int         exact = 0;
            int         count = 0;

            slot = getSlot(t, leafNode);
            if (slot <= BTERROR) {
                rc = (slot == BTERROR ? EINVAL : ENOENT);
                goto done;
            }
            firstDataNode = getnode(leafNode, slot);

            for ( dataNode = firstDataNode; dataNode; dataNode = nextDataNode) {
                count++;
                if (!comparekeys(t)(key, getdatakey(dataNode), EXACT_MATCH) ) {
                    exact = 1;
                    break;
                }
                nextDataNode = getdatanext(dataNode);
            }

            if (exact) {
                *cfid = getdatavalue(dataNode).fid;
                rc = 0;
            } else if (count == 1) {
                *cfid = getdatavalue(firstDataNode).fid;
                rc = CM_ERROR_INEXACT_MATCH;
            } else {
                rc = CM_ERROR_AMBIGUOUS_FILENAME;
            }
        } else {
            rc = ENOENT;
        }

    done:
        if (entry)
            free(entry);

        IS(rc == simple[i].rc);
        if (rc == simple[i].rc)
            IS(fid.vnode == i);
    }
}


int wmain(int argc, wchar_t ** argv)
{
    TRACE1(L"\n\nRunning BPLUS tests...\n\n");
    if (initialize_tests())
        return 1;

    RUNTEST(simple_test);

    TRACE1(L"\nDone\n");
    TRACE1(L"# of test Succeeded: %d\n", n_succeeded);
    TRACE1(L"# of test Failed: %d\n", n_failed);

    return 0;
}
