#ifndef __AKIMPERSONATE_V5GEN_H__
#define __AKIMPERSONATE_V5GEN_H__
extern int akv5gen_encode_krb5_ticket(int kvno,
                                      char *realm,
                                      int name_type,
                                      int name_len,
                                      char **name_parts,
                                      int enctype,
                                      size_t cipher_len,
                                      char *cipher_data,
                                      size_t *a_out_len,
                                      char **a_out_data);

extern int akv5gen_encode_krb5_enc_tkt_part(int enctype,
                                            size_t key_len,
                                            unsigned char *key_data,
                                            char *realm,
                                            int name_type,
                                            int name_len,
                                            char **name_parts,
                                            int transited_type,
                                            int transited_len,
                                            char *transited_data,
                                            time_t authtime,
                                            time_t starttime,
                                            time_t endtime,
                                            time_t renew_till,
                                            size_t *a_out_len,
                                            char **a_out_data);
#endif
