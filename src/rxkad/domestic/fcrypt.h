
/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*
 * Revision 2.1  90/08/07  19:33:16
 * Start with clean version to sync test and dev trees.
 *
 */

#ifndef TRANSARC_RXKAD_FCRYPT_H
#define TRANSARC_RXKAD_FCRYPT_H

#define ENCRYPTIONBLOCKSIZE 8

typedef afs_int32 fc_InitializationVector[ENCRYPTIONBLOCKSIZE/4];

#define MAXROUNDS 16
typedef afs_int32 fc_KeySchedule[MAXROUNDS];

#ifndef ENCRYPT
#define ENCRYPT 1
#define DECRYPT 0
#endif

#endif
