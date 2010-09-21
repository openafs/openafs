/* Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BUTC_PROTOTYPES_H
#define _BUTC_PROTOTYPES_H

/* dbentries.c */

extern void *dbWatcher(void *);

/* dump.c */

extern void *Dumper(void *);
extern void *DeleteDump(void *);

/* lwps.c */
extern void *Restorer(void *);
extern void *Labeller(void *);

/* recoverdDb.c */

extern void *ScanDumps(void *);

/* tcudbprocs.c */

extern void *saveDbToTape(void *);
extern void *restoreDbFromTape(void *);
extern void *KeepAlive(void *);

#endif

