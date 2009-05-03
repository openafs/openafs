/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 2.1  90/08/07  19:33:16
 * Start with clean version to sync test and dev trees.
 *
 */

#ifndef OPENAFS_RXKAD_FCRYPT_H
#define OPENAFS_RXKAD_FCRYPT_H

#ifdef ENCRYPTIONBLOCKSIZE
#undef ENCRYPTIONBLOCKSIZE
#endif
#define ENCRYPTIONBLOCKSIZE 8

typedef struct {
    afs_uint32 d[ENCRYPTIONBLOCKSIZE / 4];
} fc_InitializationVector;

#ifdef MAXROUNDS
#undef MAXROUNDS
#endif
#define MAXROUNDS 16
typedef struct {
    afs_uint32 d[MAXROUNDS];
} fc_KeySchedule;

#ifndef ENCRYPT
#define ENCRYPT 1
#define DECRYPT 0
#endif

#endif
