#ifndef DES_PROTOTYPES_H
#define DES_PROTOTYPES_H

/* misc.c */
extern afs_uint32 long_swap_bits(afs_uint32 x);
extern afs_uint32 swap_six_bits_to_ansi(afs_uint32 old);
extern afs_uint32 swap_four_bits_to_ansi(afs_uint32 old);
extern afs_uint32 swap_bit_pos_1(afs_uint32 x);
extern afs_uint32 swap_bit_pos_0(afs_uint32 x);
extern afs_uint32 swap_bit_pos_0_to_ansi(afs_uint32 x);
extern afs_uint32 rev_swap_bit_pos_0(afs_uint32 x);
extern afs_uint32 swap_byte_bits(afs_uint32 x);
extern int swap_long_bytes_bit_number(afs_uint32 x);
#if !defined(KERNEL)
extern void test_set(FILE * stream, const char *src, int testbit,
		     const char *dest, int setbit);
#endif
extern int des_debug;

static_inline unsigned char *
cblockptr_to_cblock(des_cblock *key) {
    return (unsigned char *)key;
}

static_inline unsigned char *
charptr_to_cblock(char *key) {
    return (unsigned char *)key;
}

static_inline des_cblock *
charptr_to_cblockptr(char *key) {
    return (unsigned char (*)[])key;
}

/* cbc_encrypt.c */
extern afs_int32 des_cbc_encrypt(void * in, void * out,
				 afs_int32 length,
				 des_key_schedule key, des_cblock * iv,
				 int encrypt);

/* pcbc_encrypt.c */
extern afs_int32 des_pcbc_encrypt(void * in, void * out,
				  afs_int32 length,
				  des_key_schedule key, des_cblock * iv,
				  int encrypt);

/* des.c */
extern afs_int32 des_ecb_encrypt(void * clear, void * cipher,
				 des_key_schedule schedule,
				 int encrypt);

/* util.c */
#if !defined(KERNEL)
extern int des_cblock_print_file(des_cblock * x, FILE * fp);
#endif

/* make_*.c */
#if !defined(KERNEL)
extern void gen(FILE * stream);
#endif

/* weak_key.c */
extern int des_is_weak_key(des_cblock key);

/* key_parity.c */
extern void des_fixup_key_parity(des_cblock key);
extern int des_check_key_parity(des_cblock key);

/* cksum.c */
extern afs_uint32 des_cbc_cksum(des_cblock * in, des_cblock * out,
				afs_int32 length,
				des_key_schedule key, des_cblock * iv);

/* quad_cksum.c */
extern afs_uint32 des_quad_cksum(unsigned char *in, afs_uint32 * out,
				 afs_int32 length, int out_count,
				 des_cblock * c_seed);

/* key_sched.c */
extern int afs_des_key_sched(des_cblock k,
			     des_key_schedule schedule);
extern int des_key_sched(des_cblock k, des_key_schedule schedule);


/* strng_to_key.c */
extern void des_string_to_key(char *str, des_cblock * key);

/* new_rnd_key.c */
extern int des_random_key(des_cblock key);
extern void des_init_random_number_generator(des_cblock key);
extern void des_set_random_generator_seed(des_cblock key);

/* read_pssword.c */
extern int des_read_password(des_cblock * k, char *prompt, int verify);
extern int des_read_pw_string(char *, int, char *, int);

#endif
