/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSIFD_H_ENV_
#define _OSIFD_H_ENV_ 1

#ifndef DJGPP
#include "dbrpc.h"
#endif /* !DJGPP */
#include "osiqueue.h"

struct osi_fd;
struct osi_fdType;

/* operations on a file descriptor */
typedef struct osi_fdOps {
	/* create an object, passed in the type info and returns
	 * the newly created fd.  This is the only function not passed
	 * in the object, since it creates it.
	 */
	long (*Create)(struct osi_fdType *, struct osi_fd **);

#ifndef DJGPP
	/* gets info about the object; fields are type specific, and eventually
	 * self-labelling
	 */
	long (*GetInfo)(struct osi_fd *, osi_remGetInfoParms_t *);
#endif

	/* close an object; frees the storage associated with it */
	long (*Close)(struct osi_fd *);
} osi_fdOps_t;

/* header structure that must be at the start of all FDs so that we can find
 * them generically from the network layer by fd number.
 */
typedef struct osi_fd {
	osi_queue_t q;
	osi_fdOps_t *opsp;
	long fd;
} osi_fd_t;

/* represents a specific file descriptor for iterating over all file
 * descriptor types.
 */
typedef struct osi_typeFD {
	osi_fd_t fd;			/* header */
	struct osi_fdType *curp;	/* type scan is about to return next */
} osi_typeFD_t;

typedef struct osi_fdTypeFormat {
	struct osi_fdTypeFormat *nextp;
	char *labelp;
	long region;
	long index;
	long format;
} osi_fdTypeFormat_t;

/* describes type of file descriptor; anyone can register a new one */
typedef struct osi_fdType {
	osi_queue_t q;		/* for threading together so we can find by name */
	char *namep;		/* name */
	void *rockp;		/* opaque for type owner */
	osi_fdOps_t *opsp;	/* operations */
	osi_fdTypeFormat_t *formatListp;	/* list of formatting info */
} osi_fdType_t;

extern osi_fdType_t *osi_FindFDType(char *);

extern osi_fdType_t *osi_RegisterFDType(char *, osi_fdOps_t *, void *);

extern long osi_UnregisterFDType(char *);

extern int osi_AddFDFormatInfo(osi_fdType_t *typep, long region, long index,
	char *labelp, long format);

extern int osi_InitFD(void);

extern osi_fd_t *osi_AllocFD(char *);

extern osi_fd_t *osi_FindFD(long);

extern int osi_CloseFD(osi_fd_t *);

extern long osi_FDTypeCreate(osi_fdType_t *, osi_fd_t **);

#ifndef DJGPP
extern long osi_FDTypeGetInfo(osi_fd_t *, osi_remGetInfoParms_t *);
#endif

extern long osi_FDTypeClose(osi_fd_t *);

#endif /* _OSIFD_H_ENV_ */

