
/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef UPDATE_GLOBAL_H
#define UPDATE_GLOBAL_H

#ifdef AFS_NT40_ENV
#include <windows.h>
#endif

#define TIMEOUT 	300	/*interval after which the files are resynch'ed */
#define MAXSIZE 	1024	/*max size of filenames */
#define	MAXENTRIES	20
#define UPDATEERR 	100

struct filestr {
    struct filestr *next;
    char *name;
};

#endif /* UPDATE_GLOBAL_H */
