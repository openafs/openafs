/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_UTILS_H
#define OPENAFS_WINNT_AFSD_CM_UTILS_H 1

#define CM_UTILS_SPACESIZE		8192	/* space to allocate */
typedef struct cm_space {
    union {
        clientchar_t wdata[CM_UTILS_SPACESIZE];
        char data[CM_UTILS_SPACESIZE];
    };
    struct cm_space *nextp;
} cm_space_t;

/* error code hack */
#define VL_IDEXIST                               (363520L)
#define VL_IO                                    (363521L)
#define VL_NAMEEXIST                             (363522L)
#define VL_CREATEFAIL                            (363523L)
#define VL_NOENT                                 (363524L)
#define VL_EMPTY                                 (363525L)
#define VL_ENTDELETED                            (363526L)
#define VL_BADNAME                               (363527L)
#define VL_BADINDEX                              (363528L)
#define VL_BADVOLTYPE                            (363529L)
#define VL_BADSERVER                             (363530L)
#define VL_BADPARTITION                          (363531L)
#define VL_REPSFULL                              (363532L)
#define VL_NOREPSERVER                           (363533L)
#define VL_DUPREPSERVER                          (363534L)
#define VL_RWNOTFOUND                            (363535L)
#define VL_BADREFCOUNT                           (363536L)
#define VL_SIZEEXCEEDED                          (363537L)
#define VL_BADENTRY                              (363538L)
#define VL_BADVOLIDBUMP                          (363539L)
#define VL_IDALREADYHASHED                       (363540L)
#define VL_ENTRYLOCKED                           (363541L)
#define VL_BADVOLOPER                            (363542L)
#define VL_BADRELLOCKTYPE                        (363543L)
#define VL_RERELEASE                             (363544L)
#define VL_BADSERVERFLAG                         (363545L)
#define VL_PERM                                  (363546L)
#define VL_NOMEM                                 (363547L)
#define VL_BADVERSION                            (363548L)
#define VL_INDEXERANGE                           (363549L)
#define VL_MULTIPADDR                            (363550L)
#define VL_BADMASK                               (363551L)

extern cm_space_t *cm_GetSpace(void);

extern void cm_FreeSpace(cm_space_t *);

extern long cm_MapRPCError(long error, cm_req_t *reqp);

extern long cm_MapRPCErrorRmdir(long error, cm_req_t *reqp);

extern long cm_MapVLRPCError(long error, cm_req_t *reqp);

extern void init_et_to_sys_error(void);

extern int cm_Is8Dot3(clientchar_t *namep);

extern void cm_Gen8Dot3Name(struct cm_dirEntry *dep, clientchar_t *shortName,
                            clientchar_t **shortNameEndp);

#define cm_Gen8Dot3Name(dep,shortName,shortNameEndp)                  \
cm_Gen8Dot3NameInt((dep)->name, &(dep)->fid, shortName, shortNameEndp)

extern void cm_Gen8Dot3NameInt(const fschar_t * longname, cm_dirFid_t * pfid,
                               clientchar_t *shortName, clientchar_t **shortNameEndp);

extern void cm_Gen8Dot3NameIntW(const clientchar_t* longname, cm_dirFid_t * pfid,
                                clientchar_t *shortName, clientchar_t **shortNameEndp);

extern int cm_MatchMask(clientchar_t *namep, clientchar_t *maskp, int flags);

extern BOOL cm_TargetPerceivedAsDirectory(const fschar_t *target);

extern HANDLE cm_LoadAfsdHookLib(void);

extern BOOL cm_GetOSFileVersion(char *filename, LARGE_INTEGER *liVer);

extern BOOL msftSMBRedirectorSupportsExtendedTimeouts(void);

/* thread priority */
extern void cm_UpdateServerPriority(void);

extern void cm_SetRequestStartTime(void);

extern void cm_ResetServerPriority(void);

/* time related functions */

extern void cm_LargeSearchTimeFromUnixTime(FILETIME *largeTimep, time_t unixTime);

extern void cm_UnixTimeFromLargeSearchTime(time_t *unixTimep, FILETIME *largeTimep);

extern void cm_SearchTimeFromUnixTime(afs_uint32 *searchTimep, time_t unixTime);

extern void cm_UnixTimeFromSearchTime(time_t *unixTimep, afs_uint32 searchTime);

extern void cm_utilsInit(void);

extern void cm_utilsCleanup(void);

__inline void
cm_InterlockedAnd(LONG * pdest, LONG value)
{
    LONG orig, current, new;

    current = *pdest;

    do
    {
        orig = current;
        new = orig & value;
        current = _InterlockedCompareExchange(pdest, new, orig);
    } while (orig != current);
}

__inline void
cm_InterlockedOr(LONG * pdest, LONG value)
{
    LONG orig, current, new;

    current = *pdest;

    do
    {
        orig = current;
        new = orig | value;
        current = _InterlockedCompareExchange(pdest, new, orig);
    } while (orig != current);
}

#ifdef DEBUG
#ifdef _M_IX86
#define _InterlockedOr   cm_InterlockedOr
#define _InterlockedAnd  cm_InterlockedAnd
#endif
#endif

#endif /*  OPENAFS_WINNT_AFSD_CM_UTILS_H */
