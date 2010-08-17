/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BUDB_PROTOTYPES_H
#define _BUDB_PROTOTYPES_H

/* struct_ops.c */

extern void structDumpHeader_ntoh(struct structDumpHeader *,
				  struct structDumpHeader *);
extern void DbHeader_ntoh(struct DbHeader *, struct DbHeader *);
extern void dumpEntry_ntoh(struct budb_dumpEntry *, struct budb_dumpEntry *);
extern void tapeEntry_ntoh(struct budb_tapeEntry *, struct budb_tapeEntry *);
extern void volumeEntry_ntoh(struct budb_volumeEntry *,
			     struct budb_volumeEntry *);
extern int default_tapeset(struct budb_tapeSet *, char *);
extern void printDumpEntry(struct budb_dumpEntry *deptr);
extern void printTapeEntry(struct budb_tapeEntry *teptr);
extern void printVolumeEntry(struct budb_volumeEntry *veptr);
#endif
