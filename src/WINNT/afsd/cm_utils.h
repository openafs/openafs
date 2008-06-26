/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CM_UTILS_H_ENV__
#define __CM_UTILS_H_ENV__ 1

#define CM_UTILS_SPACESIZE		8192	/* space to allocate */
typedef struct cm_space {
    union {
        clientchar_t wdata[CM_UTILS_SPACESIZE];
        char data[CM_UTILS_SPACESIZE];
    };
    struct cm_space *nextp;
} cm_space_t;

/* error code hack */
#define ERROR_TABLE_BASE_VL	(363520L)
#define VL_NOENT		(363524L)

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

#endif /*  __CM_UTILS_H_ENV__ */
