/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */


#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#include <rpc.h>
#include "dbrpc.h"
#endif /* !DJGPP */
#include <malloc.h>
#include "osi.h"
#include <assert.h>

static Crit_Sec osi_fdCS;
osi_fd_t *osi_allFDs;
osi_fdType_t *osi_allFDTypes;
long osi_nextFD = 0;

osi_fdOps_t osi_TypeFDOps = {
	osi_FDTypeCreate,
#ifndef DJGPP
	osi_FDTypeGetInfo,
#endif
	osi_FDTypeClose
};

/* called while holding osi_fdCS, returns named type or null pointer */
osi_fdType_t *osi_FindFDType(char *namep)
{
	osi_fdType_t *ftp;

	for(ftp = osi_allFDTypes; ftp; ftp = (osi_fdType_t *) osi_QNext(&ftp->q)) {
		if (strcmp(ftp->namep, namep) == 0) break;
	}

	return ftp;
}

long osi_UnregisterFDType(char *namep)
{
	osi_fdType_t *ftp;
        osi_fdTypeFormat_t *ffp;
        osi_fdTypeFormat_t *nffp;

	/* check for dup name */
	thrd_EnterCrit(&osi_fdCS);
	ftp = osi_FindFDType(namep);
        if (!ftp) return -1;
        
	/* free subsidiary storage, and remove from list */
        free(ftp->namep);
	osi_QRemove((osi_queue_t **) &osi_allFDTypes, &ftp->q);
        
	/* free format structs */
        for(ffp = ftp->formatListp; ffp; ffp=nffp) {
		nffp = ffp->nextp;
                free(ffp->labelp);
                free(ffp);
        }

	/* free main storage */
        free(ftp);

	/* cleanup and go */
	thrd_LeaveCrit(&osi_fdCS);
	return 0;
}

osi_fdType_t *osi_RegisterFDType(char *namep, osi_fdOps_t *opsp, void *datap)
{
	osi_fdType_t *ftp;

	/* check for dup name */
	thrd_EnterCrit(&osi_fdCS);
	osi_assertx(osi_FindFDType(namep) == NULL, "registering duplicate iteration type");

	ftp = (osi_fdType_t *) malloc(sizeof(*ftp));
	
	ftp->namep = (char *) malloc(strlen(namep)+1);
	strcpy(ftp->namep, namep);

	ftp->rockp = datap;

	ftp->opsp = opsp;

	ftp->formatListp = NULL;

	osi_QAdd((osi_queue_t **) &osi_allFDTypes, &ftp->q);
	thrd_LeaveCrit(&osi_fdCS);

	return ftp;
}

osi_AddFDFormatInfo(osi_fdType_t *typep, long region, long index,
	char *labelp, long format)
{
	osi_fdTypeFormat_t *formatp;

	formatp = (osi_fdTypeFormat_t *) malloc(sizeof(*formatp));

	formatp->labelp = (char *) malloc(strlen(labelp) + 1);
	strcpy(formatp->labelp, labelp);

	formatp->format = format;

	/* now copy index info */
	formatp->region = region;
	formatp->index = index;

	/* thread on the list when done */
	thrd_EnterCrit(&osi_fdCS);
	formatp->nextp = typep->formatListp;
	typep->formatListp = formatp;
	thrd_LeaveCrit(&osi_fdCS);

	/* all done */
	return 0;
}

osi_InitFD(void) {
	static osi_once_t once;

	if (!osi_Once(&once)) return 0;

	osi_allFDs = NULL;
	osi_allFDTypes = NULL;
	thrd_InitCrit(&osi_fdCS);

	/* now, initialize the types system by adding a type
	 * iteration operator
	 */
	osi_RegisterFDType("type", &osi_TypeFDOps, NULL);

	osi_EndOnce(&once);

	return 0;
}

osi_fd_t *osi_AllocFD(char *namep)
{
	osi_fd_t *fdp;
	osi_fdType_t *fdTypep;
	long code;

	/* initialize for failure */
	fdp = NULL;
	
	thrd_EnterCrit(&osi_fdCS);
	fdTypep = osi_FindFDType(namep);
	if (fdTypep) {
		code = (fdTypep->opsp->Create)(fdTypep, &fdp);
		if (code == 0) {
			fdp->fd = ++osi_nextFD;
			fdp->opsp = fdTypep->opsp;
			osi_QAdd((osi_queue_t **) &osi_allFDs, &fdp->q);
		}
		else fdp = NULL;
	}
	thrd_LeaveCrit(&osi_fdCS);

	return fdp;
}

osi_fd_t *osi_FindFD(long fd)
{
	osi_fd_t *fdp;

	thrd_EnterCrit(&osi_fdCS);
	for(fdp = osi_allFDs; fdp; fdp = (osi_fd_t *) osi_QNext(&fdp->q)) {
		if (fdp->fd == fd) break;
	}
	thrd_LeaveCrit(&osi_fdCS);

	return fdp;
}

osi_CloseFD(osi_fd_t *fdp)
{
	long code;

	thrd_EnterCrit(&osi_fdCS);
	osi_QRemove((osi_queue_t **) &osi_allFDs, &fdp->q);
	thrd_LeaveCrit(&osi_fdCS);

	/* this call frees the FD's storage, so make sure everything is unthreaded
	 * before here.
	 */
	code = (fdp->opsp->Close)(fdp);

	return code;
}


/* now we have the fd type operations */
long osi_FDTypeCreate(osi_fdType_t *fdTypep, osi_fd_t **outpp)
{
	osi_typeFD_t *fdp;

	fdp = (osi_typeFD_t *) malloc(sizeof(*fdp));

	fdp->curp = osi_allFDTypes;

	*outpp = &fdp->fd;
	return 0;
}


#ifndef DJGPP
long osi_FDTypeGetInfo(osi_fd_t *ifdp, osi_remGetInfoParms_t *outp)
{
	osi_typeFD_t *fdp;
	osi_fdType_t *typep;

	fdp = (osi_typeFD_t *) ifdp;

	if (typep = fdp->curp) {
		/* still more stuff left, copy out name move to the next */
		outp->icount = 0;
		outp->scount = 1;
		strcpy(outp->sdata[0], typep->namep);
		thrd_EnterCrit(&osi_fdCS);
		fdp->curp = (osi_fdType_t *) osi_QNext(&typep->q);
		thrd_LeaveCrit(&osi_fdCS);
		return 0;
	}
	else {
		/* otherwise we've hit EOF */
		return OSI_DBRPC_EOF;
	}
}
#endif /* !DJGPP */

long osi_FDTypeClose(osi_fd_t *ifdp)
{
	osi_typeFD_t *fdp;

	fdp = (osi_typeFD_t *) ifdp;

	free((void *)fdp);

	return 0;
}

