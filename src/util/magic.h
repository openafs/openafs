/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_MAGIC_H
#define AFS_MAGIC_H
typedef enum afs_structmagic_t {
	MAGIC_RXCONN = 0xFF010001,
	MAGIC_RXCALL = 0xFF010002,
	MAGIC_RXPEER = 0xFF010003,
	MAGIC_RXSVC = 0xFF010004,
	MAGIC_RXSECURITY = 0xFF010005,
	MAGIC_UBIK = 0xFF010101,
	MAGIC_VICED_HOST = 0xFF010201,
	MAGIC_VICED_CLIENT = 0xFF010202,
	MAGIC_RXCONN_FREE = 0xFF020001,
	MAGIC_RXCALL_FREE = 0xFF020002,
	MAGIC_RXPEER_FREE = 0xFF010003,
	/*MAGIC_RXSVC_FREE = 0xFF020004,*/
	MAGIC_RXSEC_FREE = 0xFF020005,
	MAGIC_UBIK_FREE = 0xFF020101,
	MAGIC_VICED_HOST_FREE = 0xFF020101,
	MAGIC_VICED_CLIENT_FREE = 0xFF020102
} afs_structmagic;
#endif
