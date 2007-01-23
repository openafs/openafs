/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works 
 * and redistribute this software and such derivative works 
 * for any purpose, so long as the name of the University of 
 * Michigan is not used in any advertising or publicity 
 * pertaining to the use or distribution of this software 
 * without specific, written prior authorization.  If the 
 * above copyright notice or any other identification of the 
 * University of Michigan is included in any copy of any 
 * portion of this software, then the disclaimer below must 
 * also be included.
 *
 * This software is provided as is, without representation 
 * from the University of Michigan as to its fitness for any 
 * purpose, and without warranty by the University of 
 * Michigan of any kind, either express or implied, including 
 * without limitation the implied warranties of 
 * merchantability and fitness for a particular purpose.  The 
 * regents of the University of Michigan shall not be liable 
 * for any damages, including special, indirect, incidental, or 
 * consequential damages, with respect to any claim arising 
 * out of or in connection with the use of the software, even 
 * if it has been or is hereafter advised of the possibility of 
 * such damages.
 */

#include "rxk5.h"

#ifdef USING_SHISHI
typedef struct _rxk5_keyblock { Shishi_key *sk;} krb5_keyblock;
typedef Shishi *krb5_context;
typedef char *krb5_principal;
#define ENCTYPE_NULL SHISHI_NULL
#define ENCTYPE_DES_CBC_CRC SHISHI_DES_CBC_CRC
#define ENCTYPE_DES_CBC_MD4 SHISHI_DES_CBC_MD4
#define ENCTYPE_DES_CBC_MD5 SHISHI_DES_CBC_MD5
/* #define ENCTYPE_DES3_CBC_MD5	??? */
#define ENCTYPE_DES_CBC_NONE SHISHI_DES_CBC_NONE
#define ENCTYPE_DES3_CBC_NONE SHISHI_DES3_CBC_NONE
#define ENCTYPE_DES3_CBC_SHA1 SHISHI_DES3_CBC_HMAC_SHA1_KD
#define ENCTYPE_AES128_CTS_HMAC_SHA1_96 SHISHI_AES128_CTS_HMAC_SHA1_96
#define ENCTYPE_AES256_CTS_HMAC_SHA1_96 SHISHI_AES256_CTS_HMAC_SHA1_96
#define ENCTYPE_ARCFOUR_HMAC_MD5 SHISHI_ARCFOUR_HMAC		/* X */
#define ENCTYPE_ARCFOUR_HMAC_MD5_EXP SHISHI_ARCFOUR_HMAC_EXP	/* X */
#define CKSUMTYPE_CRC32 SHISHI_CRC32
#define CKSUMTYPE_RSA_MD4 SHISHI_RSA_MD4
#define CKSUMTYPE_RSA_MD4_DES SHISHI_RSA_MD4_DES
#define CKSUMTYPE_DES_MAC SHISHI_DES_MAC
#define CKSUMTYPE_DES_MAC_K SHISHI_DES_MAC_K
#define CKSUMTYPE_RSA_MD4_DES_K SHISHI_RSA_MD4_DES_K
#define CKSUMTYPE_RSA_MD5 SHISHI_RSA_MD5
#define CKSUMTYPE_RSA_MD5_DES SHISHI_RSA_MD5_DES
#define CKSUMTYPE_RSA_MD5_DES_GSS SHISHI_RSA_MD5_DES_GSS
#define CKSUMTYPE_HMAC_SHA1_DES3 SHISHI_HMAC_SHA1_DES3_KD	/* X */
#define CKSUMTYPE_HMAC_SHA1_96_AES_128 SHISHI_HMAC_SHA1_96_AES128 /* X */
#define CKSUMTYPE_HMAC_SHA1_96_AES_256 SHISHI_HMAC_SHA1_96_AES256 /* X */
#define CKSUMTYPE_HMAC_MD5_ARCFOUR SHISHI_ARCFOUR_HMAC_MD5	/* X */
#define ERROR_TABLE_BASE_SHI5 (1283619328L)
typedef struct _rxk5_data { char *data; size_t length; } krb5_data;
#endif

struct rxk5_connstats {
	int bytesReceived, bytesSent, packetsReceived, packetsSent;
};

struct rxk5_cprivate {
    krb5_context k5_context;
    int level;
    int cktype;
    krb5_keyblock session[1];
    int ticketlen;
    char ticket[1];
};

struct rxk5_cconn {
    int pad[2];
    krb5_keyblock mykey[1], hiskey[1];
    krb5_keyblock thekey[1];
    struct rxk5_connstats stats;
};

struct rxk5_sprivate {
    krb5_context k5_context;
    int minlevel;
    char *get_key_arg;
    int (*get_key)();
    int (*user_ok)();
};

struct rxk5_sconn {
    int pad[2];
    krb5_keyblock mykey[1], hiskey[1];
    struct rxk5_connstats stats;
    int cktype;
    int level;
    int flags;
#define AUTHENTICATED 1
#define TRIED 2
    time_t start, expires;
    long challengeid;
    char *client, *server;
    int kvno;
};

#define USAGE_ENCRYPT 100
#define USAGE_CHECK 101
#define USAGE_CLIENT 102
#define USAGE_SERVER 103
#define USAGE_RESPONSE 104

krb5_context rxk5_get_context();

#ifndef KERNEL
#define osi_Time()	time(0)
#endif

int rxk5i_SetLevel();
int rxk5i_CheckEncrypt();
int rxk5i_CheckSum();
int rxk5i_GetStats();
int rxk5i_NewConnection();
int rxk5i_PrepareEncrypt();
int rxk5i_PrepareSum();
int rxk5i_derive_key();
int rxk5i_random_integer();
void rxk5_OnetimeInit();
#ifdef USING_SHISHI
void krb5_free_keyblock_contents();
#endif

char *rxk5i_nfold();

#if defined(AFS_PTHREAD_ENV)
extern pthread_mutex_t rxk5i_context_mutex[1];
#define LOCK_RXK5_CONTEXT assert(!pthread_mutex_lock(rxk5i_context_mutex))
#define UNLOCK_RXK5_CONTEXT assert(!pthread_mutex_unlock(rxk5i_context_mutex))
#else
#define LOCK_RXK5_CONTEXT
#define UNLOCK_RXK5_CONTEXT
#endif

#if 0 || defined(AFS_PTHREAD_ENV)
#include <pthread.h>
#include <assert.h>
extern pthread_mutex_t rxk5_stats_mutex[1];
#define LOCK_RXK5_STATS assert(!pthread_mutex_lock(rxk5_stats_mutex))
#define UNLOCK_RXK5_STATS assert(!pthread_mutex_unlock(rxk5_stats_mutex))
#else
#define LOCK_RXK5_STATS
#define UNLOCK_RXK5_STATS
#endif

#if 0
#define rxk5_client 0
#define rxk5_server 1
#define INCSTAT(x)	do{LOCK_RXK5_STATS; rxk5_stats.x++; UNLOCK_RXK5_STATS;}while(0)
#else
#define INCSTAT(x)	/* x++ */
#endif
