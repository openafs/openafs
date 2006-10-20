/*
 * des.h
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Include file for the Data Encryption Standard library.
 */

/* only do the whole thing once	 */
#ifndef DES_DEFS
#define DES_DEFS

#include "mit_copy.h"
#include "conf.h"

#ifndef KRB_INT32
#define KRB_INT32 long
#endif
#ifndef KRB_UINT32
#define KRB_UINT32 unsigned KRB_INT32
#endif

/* There are some declarations in the system-specific header files which
   can't be done until KRB_INT32 is defined.  So they are in a macro,
   which we expand here if defined.  */

#ifdef	DECL_THAT_NEEDS_KRB_INT32
DECL_THAT_NEEDS_KRB_INT32
#endif

typedef unsigned char des_cblock[8];	/* crypto-block size */
/* Key schedule */
typedef struct des_ks_struct { union { KRB_INT32 pad; des_cblock _;} __; } des_key_schedule[16];

#define DES_KEY_SZ 	(sizeof(des_cblock))
#define DES_ENCRYPT	1
#define DES_DECRYPT	0

#ifndef NCOMPAT
#define C_Block des_cblock
#define Key_schedule des_key_schedule
#define ENCRYPT DES_ENCRYPT
#define DECRYPT DES_DECRYPT
#define KEY_SZ DES_KEY_SZ
#define string_to_key des_string_to_key
#define read_pw_string des_read_pw_string
#define random_key des_random_key
#define pcbc_encrypt des_pcbc_encrypt
#define key_sched des_key_sched
#define cbc_encrypt des_cbc_encrypt
#define cbc_cksum des_cbc_cksum
#define C_Block_print des_cblock_print
#define quad_cksum des_quad_cksum
typedef struct des_ks_struct bit_64;
#endif

#define des_cblock_print(x) des_cblock_print_file(x, stdout)

/* Function declarations */

#define DES_CALLCONV_C

extern unsigned long DES_CALLCONV_C
quad_cksum PROTOTYPE ((
			unsigned char *in,	/* input block */
			unsigned KRB_INT32 *out,/* optional longer output */
			long length,		/* original length in bytes */
			int out_count,		/* number of iterations */
			des_cblock *c_seed));	/* secret seed, 8 bytes */

int DES_CALLCONV_C
des_key_sched PROTOTYPE ((des_cblock FAR, des_key_schedule FAR));

int DES_CALLCONV_C
des_ecb_encrypt PROTOTYPE ((des_cblock FAR *, des_cblock FAR *,
			    des_key_schedule FAR, int));

int DES_CALLCONV_C
des_pcbc_encrypt PROTOTYPE ((des_cblock FAR *, des_cblock FAR *, long,
			     des_key_schedule FAR, des_cblock FAR *, int));

int DES_CALLCONV_C
des_is_weak_key PROTOTYPE ((des_cblock FAR));

void DES_CALLCONV_C
des_fixup_key_parity PROTOTYPE ((des_cblock FAR));

int DES_CALLCONV_C
des_check_key_parity PROTOTYPE ((des_cblock FAR));

/* 
   These random_key routines are made external here 
   for the Macintosh Driver which exports pointers to them 
   to applications via a driver level interface.
   Preserved for KClient compatability.
*/

int DES_CALLCONV_C
des_new_random_key PROTOTYPE (( des_cblock ));

void DES_CALLCONV_C
des_init_random_number_generator PROTOTYPE (( des_cblock ));
     
void DES_CALLCONV_C 
des_set_random_generator_seed PROTOTYPE (( des_cblock ));

unsigned long DES_CALLCONV_C
des_cbc_cksum PROTOTYPE ((des_cblock *,des_cblock *,long,des_key_schedule,des_cblock));
	
/* FIXME, put the rest of the function declarations here */

#endif /* DES_DEFS */
