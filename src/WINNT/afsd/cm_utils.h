/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef __CM_UTILS_H_ENV__
#define __CM_UTILS_H_ENV__ 1

#define CM_UTILS_SPACESIZE		8192	/* space to allocate */
typedef struct cm_space {
	char data[CM_UTILS_SPACESIZE];
        struct cm_space *nextp;
} cm_space_t;

/* error code hack */
#define ERROR_TABLE_BASE_vl	(363520L)
#define VL_NOENT		(363524L)

extern cm_space_t *cm_GetSpace(void);

extern void cm_FreeSpace(cm_space_t *);

extern long cm_MapRPCError(long error, cm_req_t *reqp);
extern long cm_MapRPCErrorRmdir(long error, cm_req_t *reqp);
extern long cm_MapVLRPCError(long error, cm_req_t *reqp);

#endif /*  __CM_UTILS_H_ENV__ */
