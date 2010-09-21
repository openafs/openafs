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

typedef afs_int32 fc_InitializationVector[ENCRYPTIONBLOCKSIZE / 4];

#ifdef MAXROUNDS
#undef MAXROUNDS
#endif
#define MAXROUNDS 16
typedef afs_int32 fc_KeySchedule[MAXROUNDS];

#ifndef ENCRYPT
#define ENCRYPT 1
#define DECRYPT 0
#endif

#endif
